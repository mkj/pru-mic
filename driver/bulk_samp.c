/*
 * Based on rpmsg_pru.c:
 * PRU Remote Processor Messaging Driver
 *
 * Copyright (C) 2015 Texas Instruments, Inc.
 *
 * Jason Reeder <jreeder@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Modified/renamed April 2016 by Matt Johnston <matt@ucc.asn.au>
 * to allocate large sample buffers.
 */

#include <linux/kernel.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/remoteproc.h>
#include <linux/string.h>

#include "../bulk_samp_common.h"

#define PRU_MAX_DEVICES				(8)
/* Matches the definition in virtio_rpmsg_bus.c */
#define RPMSG_BUF_SIZE				(512)
#define MAX_FIFO_MSG				(32)
#define FIFO_MSG_SIZE				RPMSG_BUF_SIZE

/**
 * struct bulk_samp_dev - Structure that contains the per-device data
 * @rpdev: rpmsg channel device that is associated with this bulk_samp device
 * @dev: device
 * @cdev: character device
 * @locked: boolean used to determine whether or not the device file is in use
 * @devt: dev_t structure for the bulk_samp device
 * @wait_list: wait queue used to implement the poll operation of the character
 *             device
 *
 * Each bulk_samp device provides an interface, using an rpmsg channel (rpdev),
 * between a user space character device (cdev) and a PRU core. A kernel fifo
 * (msg_fifo) is used to buffer the messages in the kernel that are
 * being passed between the character device and the PRU.
 */
struct bulk_samp_dev {
	struct rpmsg_channel *rpdev;
	struct device *dev;
	struct cdev cdev;
	bool locked;
	dev_t devt;
	wait_queue_head_t wait_list;

	void* sample_buffers[BULK_SAMP_NUM_BUFFERS];
	dma_addr_t sample_buffers_phys[BULK_SAMP_NUM_BUFFERS];
    // XXX - do we need locking? Probably! Using smp_wmb() memory barrier.
    int read_idx;
    size_t read_off; // index into current sample_buffers[read_idx]
    int write_idx;

    bool warned_full;
};

static struct class *bulk_samp_class;
static dev_t bulk_samp_devt;
static DEFINE_MUTEX(bulk_samp_lock);
static DEFINE_IDR(bulk_samp_minors);

static int bulk_samp_is_empty(struct bulk_samp_dev *d) {
    return d->write_idx == d->read_idx;
}

static int bulk_samp_is_full(struct bulk_samp_dev *d) {
    return (d->write_idx+1)%BULK_SAMP_NUM_BUFFERS == d->read_idx;
}

static int bulk_samp_open(struct inode *inode, struct file *filp)
{
    struct bulk_samp_msg_start msg = { .type = BULK_SAMP_MSG_START };
	struct bulk_samp_dev *prudev;
	int ret = -EACCES;

	prudev = container_of(inode->i_cdev, struct bulk_samp_dev, cdev);

	mutex_lock(&bulk_samp_lock);
	if (!prudev->locked) {
		prudev->locked = true;
		filp->private_data = prudev;
		ret = 0;
	}
	mutex_unlock(&bulk_samp_lock);

	if (ret)
		dev_err(prudev->dev, "Device already open\n");

    prudev->warned_full = false;

	ret = rpmsg_send(prudev->rpdev, &msg, sizeof(msg));
	if (ret) {
		dev_err(prudev->dev, "rpmsg_send start failed: %d\n", ret);
    }

	return ret;
}

static int bulk_samp_release(struct inode *inode, struct file *filp)
{
    struct bulk_samp_msg_stop msg = { .type = BULK_SAMP_MSG_STOP };
	struct bulk_samp_dev *prudev;
    int ret;

	prudev = container_of(inode->i_cdev, struct bulk_samp_dev, cdev);

	ret = rpmsg_send(prudev->rpdev, &msg, sizeof(msg));
	if (ret) {
		dev_err(prudev->dev, "rpmsg_send stop failed: %d\n", ret);
    }

	mutex_lock(&bulk_samp_lock);
	prudev->locked = false;
	mutex_unlock(&bulk_samp_lock);
	return 0;
}

static ssize_t bulk_samp_read(struct file *filp, char __user *buf,
			      size_t count, loff_t *f_pos)
{
	int ret;
	size_t length;
	struct bulk_samp_dev *prudev;

	prudev = filp->private_data;

	if (bulk_samp_is_empty(prudev) && (filp->f_flags & O_NONBLOCK))
		return -EAGAIN;

	ret = wait_event_interruptible(prudev->wait_list,
				       !bulk_samp_is_empty(prudev));
	if (ret)
		return -EINTR;

    length = min(BULK_SAMP_BUFFER_SIZE - prudev->read_off, count);
    ret = copy_to_user(buf, prudev->sample_buffers[prudev->read_idx]+prudev->read_off, length);

    prudev->read_off += (length - ret);
    if (prudev->read_off == BULK_SAMP_BUFFER_SIZE) {
        prudev->read_idx++;
        prudev->read_idx %= BULK_SAMP_NUM_BUFFERS;
        prudev->read_off = 0;
        smp_wmb();
    }

	return ret ? ret : length;
}

static unsigned int bulk_samp_poll(struct file *filp,
				   struct poll_table_struct *wait)
{
	int mask;
	struct bulk_samp_dev *prudev;

	prudev = filp->private_data;

	poll_wait(filp, &prudev->wait_list, wait);

	mask = POLLOUT | POLLWRNORM;

	if (!bulk_samp_is_empty(prudev))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations bulk_samp_fops = {
	.owner = THIS_MODULE,
	.open = bulk_samp_open,
	.release = bulk_samp_release,
	.read = bulk_samp_read,
	.poll = bulk_samp_poll,
};

static void handle_msg_ready(struct rpmsg_channel *rpdev, void *data, int len) 
{
	struct bulk_samp_dev *prudev;
    struct bulk_samp_msg_ready *msg = data;
	prudev = dev_get_drvdata(&rpdev->dev);

    /* XXX handle 'msg' here, validate index etc */

	if (bulk_samp_is_full(prudev)) {
		if (!prudev->warned_full) {
			dev_err(&rpdev->dev, "Can't keep up with data from PRU!\n");
			prudev->warned_full = false;
		}
		return;
	}

    prudev->write_idx = (prudev->write_idx+1) % BULK_SAMP_NUM_BUFFERS;
    smp_wmb();

	wake_up_interruptible(&prudev->wait_list);
}

static void bulk_samp_cb(struct rpmsg_channel *rpdev, void *data, int len,
			 void *priv, u32 src)
{
    char type;

    if (len < 1) {
		dev_err(&rpdev->dev, "Short message from PRU!\n");
		return;
    }
    
    type = *(char*)data;

    if (type == BULK_SAMP_MSG_READY) {
        handle_msg_ready(rpdev, data, len);
    } else {
		dev_err(&rpdev->dev, "Unknown message type %d from PRU, length %d\n", type, len);
		return;
    }

}

/* copypaste from rpmsg_rpc.c */
static struct rproc *rpdev_to_rproc(struct rpmsg_channel *rpdev)
{
    struct virtio_device *vdev;

    vdev = rpmsg_get_virtio_dev(rpdev);
    if (!vdev)
        return NULL;

    return rproc_vdev_to_rproc_safe(vdev);
}

static int bulk_samp_probe(struct rpmsg_channel *rpdev)
{
	int ret;
	struct bulk_samp_dev *prudev;
	int minor_got;
    int i;
    struct bulk_samp_msg_buffers buf_msg = { .type = BULK_SAMP_MSG_BUFFERS };
    struct rproc *rp;
    struct virtio_device *vdev;

    rp = rpdev_to_rproc(rpdev);

	prudev = devm_kzalloc(&rpdev->dev, sizeof(*prudev), GFP_KERNEL);
	if (!prudev)
		return -ENOMEM;

	mutex_lock(&bulk_samp_lock);
	minor_got = idr_alloc(&bulk_samp_minors, prudev, 0, PRU_MAX_DEVICES,
			      GFP_KERNEL);
	mutex_unlock(&bulk_samp_lock);
	if (minor_got < 0) {
		ret = minor_got;
		dev_err(&rpdev->dev, "Failed to get a minor number for the bulk_samp device: %d\n",
			ret);
		goto fail_alloc_minor;
	}

	prudev->devt = MKDEV(MAJOR(bulk_samp_devt), minor_got);

	cdev_init(&prudev->cdev, &bulk_samp_fops);
	prudev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&prudev->cdev, prudev->devt, 1);
	if (ret) {
		dev_err(&rpdev->dev, "Unable to add cdev for the bulk_samp device\n");
		goto fail_add_cdev;
	}

	prudev->dev = device_create(bulk_samp_class, &rpdev->dev, prudev->devt,
				    NULL, "bulk_samp%d", rpdev->dst);
	if (IS_ERR(prudev->dev)) {
		dev_err(&rpdev->dev, "Unable to create the bulk_samp device\n");
		ret = PTR_ERR(prudev->dev);
		goto fail_create_device;
	}

	prudev->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	prudev->rpdev = rpdev;

    vdev = rpmsg_get_virtio_dev(rpdev);

    /* Allocate large buffers for data transfer. */
	for (i = 0; i < BULK_SAMP_NUM_BUFFERS; i++) {
        u64 da;
        /* TODO: for better performance this could use streaming DMA but
         * for now we use the simpler solution of coherent buffers */
		prudev->sample_buffers[i] = dma_alloc_coherent(prudev->dev, 
				BULK_SAMP_BUFFER_SIZE, &prudev->sample_buffers_phys[i], GFP_KERNEL);
		if (!prudev->sample_buffers[i]) { 
			dev_err(&rpdev->dev, "Unable to allocate sample buffers for the bulk_samp device\n");
			goto fail_alloc_buffers;
		}
        /* clear it to avoid bugs exposing kernel memory */
        memset(prudev->sample_buffers[i], 0x88, BULK_SAMP_BUFFER_SIZE);

        da = cpu_to_virtio64(vdev, prudev->sample_buffers_phys[i]);
        if (da) {
            buf_msg.buffers[i] = (uint32_t)da;
        } else {
			dev_err(&rpdev->dev, "Unable to map physical address %llx to device.\n", (u64)prudev->sample_buffers_phys[i]);
			goto fail_alloc_buffers;
        }
	}

	init_waitqueue_head(&prudev->wait_list);

	dev_set_drvdata(&rpdev->dev, prudev);

	dev_info(&rpdev->dev, "new bulk_samp device: /dev/bulk_samp%d",
		 rpdev->dst);

	ret = rpmsg_send(prudev->rpdev, &buf_msg, sizeof(buf_msg));
	if (ret) {
		dev_err(prudev->dev, "rpmsg_send buf_msg failed: %d\n", ret);
    }

	return 0;

fail_alloc_buffers:
	for (i = 0; i < BULK_SAMP_NUM_BUFFERS; i++) {
		if (prudev->sample_buffers[i]) {
			dma_free_coherent(prudev->dev, BULK_SAMP_BUFFER_SIZE, 
                    prudev->sample_buffers[i], prudev->sample_buffers_phys[i]);
		}
	}
	device_destroy(bulk_samp_class, prudev->devt);
fail_create_device:
	cdev_del(&prudev->cdev);
fail_add_cdev:
	mutex_lock(&bulk_samp_lock);
	idr_remove(&bulk_samp_minors, minor_got);
	mutex_unlock(&bulk_samp_lock);
fail_alloc_minor:
	return ret;
}

static void bulk_samp_remove(struct rpmsg_channel *rpdev)
{
	struct bulk_samp_dev *prudev;
    int i;

	prudev = dev_get_drvdata(&rpdev->dev);

	for (i = 0; i < BULK_SAMP_NUM_BUFFERS; i++) {
        dma_free_coherent(prudev->dev, BULK_SAMP_BUFFER_SIZE, 
                prudev->sample_buffers[i], prudev->sample_buffers_phys[i]);
	}
	device_destroy(bulk_samp_class, prudev->devt);
	cdev_del(&prudev->cdev);
	mutex_lock(&bulk_samp_lock);
	idr_remove(&bulk_samp_minors, MINOR(prudev->devt));
	mutex_unlock(&bulk_samp_lock);
}

/* .name matches on RPMsg Channels and causes a probe */
static const struct rpmsg_device_id rpmsg_driver_pru_id_table[] = {
	{ .name	= "bulksamp-pru" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_pru_id_table);

static struct rpmsg_driver bulk_samp_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_driver_pru_id_table,
	.probe		= bulk_samp_probe,
	.callback	= bulk_samp_cb,
	.remove		= bulk_samp_remove,
};

static int __init bulk_samp_init(void)
{
	int ret;

	bulk_samp_class = class_create(THIS_MODULE, "bulk_samp");
	if (IS_ERR(bulk_samp_class)) {
		pr_err("Unable to create class\n");
		ret = PTR_ERR(bulk_samp_class);
		goto fail_create_class;
	}

	ret = alloc_chrdev_region(&bulk_samp_devt, 0, PRU_MAX_DEVICES,
				  "bulk_samp");
	if (ret) {
		pr_err("Unable to allocate chrdev region\n");
		goto fail_alloc_region;
	}

	ret = register_rpmsg_driver(&bulk_samp_driver);
	if (ret) {
		pr_err("Unable to register rpmsg driver");
		goto fail_register_rpmsg_driver;
	}

	return 0;

fail_register_rpmsg_driver:
	unregister_chrdev_region(bulk_samp_devt, PRU_MAX_DEVICES);
fail_alloc_region:
	class_destroy(bulk_samp_class);
fail_create_class:
	return ret;
}

static void __exit bulk_samp_exit(void)
{
	unregister_rpmsg_driver(&bulk_samp_driver);
	idr_destroy(&bulk_samp_minors);
	mutex_destroy(&bulk_samp_lock);
	class_destroy(bulk_samp_class);
	unregister_chrdev_region(bulk_samp_devt, PRU_MAX_DEVICES);
}

module_init(bulk_samp_init);
module_exit(bulk_samp_exit);

// TODO
MODULE_AUTHOR("Jason Reeder <jreeder@ti.com>");
MODULE_DESCRIPTION("PRU Remote Processor Messaging Driver");
MODULE_LICENSE("GPL v2");
