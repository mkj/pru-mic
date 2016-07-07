/*
 * Copyright (C) 2015 Texas Instruments Incorporated - http://www.ti.com/ 
 *  
 *  
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *  * Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 * 
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 * 
 *  * Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
// Matt Johnston <matt@ucc.asn.au> 2016

#include <stdint.h>
#include <stdio.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_virtqueue.h>
#include <pru_rpmsg.h>
#include <pru_ctrl.h>
#include <sys_mailbox.h>
#include "resource_table_1.h"

#include "../bulk_samp_common.h"
#include "bulk_samp_pru.h"

/* PRU1 is mailbox module user 2 */
#define MB_USER 2
/* Mbox0 - mail_u2_irq (mailbox interrupt for PRU1) is Int Number 59 */
#define MB_INT_NUMBER 59

/* Host-1 Interrupt sets bit 31 in register R31, host int 1 */
#define ARM_INT ((uint32_t)(1 << 31))

/* The mailboxes used for RPMsg are defined in the Linux device tree
 * PRU0 uses mailboxes 2 (From ARM) and 3 (To ARM)
 * PRU1 uses mailboxes 4 (From ARM) and 5 (To ARM)
 */
#define MB_TO_ARM_HOST              5
#define MB_FROM_ARM_HOST            4

/*
 * Using the name 'rpmsg-client-sample' will probe the RPMsg sample driver
 * found at linux-x.y.z/samples/rpmsg/rpmsg_client_sample.c
 *
 * Using the name 'rpmsg-pru' will probe the rpmsg_pru driver found
 * at linux-x.y.z/drivers/rpmsg/rpmsg_pru.c
 */

/* XXX give a nice name */
#define RPMSG_CHAN_DESC                 "bulksamp PRU input interface"
#define RPMSG_CHAN_PORT                 31

/* 
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK   4

#define PAYLOAD_SIZE (RPMSG_BUF_SIZE-16)
uint8_t payload[PAYLOAD_SIZE];

static uint8_t *out_buffers[BULK_SAMP_NUM_BUFFERS];
static uint8_t out_index;
static int out_pos;

struct pru_rpmsg_transport transport;
uint16_t src, dst;

char going = 0;

static void reply_confirm(uint8_t type);

/* Receive samples from the other PRU, fill buffer and send to the 
ARM host if ready */
void grab_samples()
{
    struct _regbuf {
        // 40 bytes
        uint32_t x0;
        uint32_t x1;
        uint32_t x2;
        uint32_t x3;
        uint32_t x4;
    } regbuf; 
    // base address is r18
#ifdef TEST_CLOCK_OUT
    regbuf.x0 = 'c';
    regbuf.x1 = 'h';
    regbuf.x2 = 'a';
    regbuf.x3 = 'i';
    regbuf.x4 = 'r';
#else
    __xin(10, 18, 0, regbuf);
#endif

    if (!out_buffers[out_index])
    {
        // safety
        return;
    }
    reply_confirm(101);

    /* Copy into payload */
    *((struct _regbuf*)&out_buffers[out_index][out_pos]) = regbuf;
    out_pos += XFER_SIZE;

    /* Buffer is full, send it */
    if (out_pos >= BULK_SAMP_BUFFER_SIZE - XFER_SIZE) {
        struct bulk_samp_msg_ready *m = (void*)payload;
        m->type = BULK_SAMP_MSG_READY;
        m->buffer_index = out_index;
        m->size = out_pos;
        m->checksum = 0;
        m->magic = 12349876;
        pru_rpmsg_send(&transport, dst, src, payload, sizeof(struct bulk_samp_msg_ready));
        // reset buffers
        out_index = (out_index+1) % BULK_SAMP_NUM_BUFFERS;
        out_pos = 0;
#ifdef TEST_CLOCK_OUT
        going = 0;
#endif
    }
}

void poke_pru0(char msg, int value)
{
    uint8_t one = 1;
    pru_sharedmem->pru0_msg = msg;
    pru_sharedmem->pru0_val = value;

    // trigger
    __xout(10, 27, 0, one);
}

void reply_confirm(uint8_t type)
{
    struct bulk_samp_msg_confirm msg = 
    { 
        .type = BULK_SAMP_MSG_CONFIRM,
        .confirm_type = type,
    };

    pru_rpmsg_send(&transport, dst, src, &msg, sizeof(msg));
}

void send_message_debug(const char* s1, const char* s2,
    uint32_t n1, uint32_t n2, uint32_t n3)
{
    uint8_t n;
    struct bulk_samp_msg_debug msg =
    {
        .type = BULK_SAMP_MSG_DEBUG,
        .num1 = n1,
        .num2 = n2,
        .num3 = n3,
    };
    // strcpy
    for (n = 0; s1[n] && n < sizeof(msg.str1)-1; n++)
    {
        msg.str1[n] = s1[n];
    }
    msg.str1[n] = '\0';
    for (n = 0; s2[n] && n < sizeof(msg.str2)-1; n++)
    {
        msg.str2[n] = s2[n];
    }
    msg.str2[n] = '\0';
    pru_rpmsg_send(&transport, dst, src, &msg, sizeof(msg));
}

/* Handle control messages from the arm host */
void handle_rpmsg() 
{
    uint16_t len;
    /* Clear the mailbox interrupt */
    CT_MBX.IRQ[MB_USER].STATUS_CLR |= 1 << (MB_FROM_ARM_HOST * 2);
    /* Clear the event status, event MB_INT_NUMBER corresponds to the mailbox interrupt */
    CT_INTC.SICR_bit.STS_CLR_IDX = MB_INT_NUMBER;
    /* Use a while loop to read all of the current messages in the mailbox */
    while(CT_MBX.MSGSTATUS_bit[MB_FROM_ARM_HOST].NBOFMSG > 0)
    {
        /* Check to see if the message corresponds to a receive event for the PRU */
        if (CT_MBX.MESSAGE[MB_FROM_ARM_HOST] == 1)
        {
            /* Receive the message */
            if (pru_rpmsg_receive(&transport, &src, &dst, payload, &len) == PRU_RPMSG_SUCCESS)
            {
                if (len > 0)
                {
                    switch (payload[0])
                    {
                        case BULK_SAMP_MSG_START:
                        {
                            going = 1;
                            /* Turn on the other pru */
                            poke_pru0(BULK_SAMP_MSG_START, 1);
                            out_index = 0;
                            out_pos = 0;
                        }
                        break;
                        case BULK_SAMP_MSG_STOP:
                        {
                            going = 0;
                            poke_pru0(BULK_SAMP_MSG_STOP, 1);
                            /* Discard output */
                        }
                        break;
                        case BULK_SAMP_MSG_BUFFERS:
                        {
                            struct bulk_samp_msg_buffers *m = (void*)payload;
                            for (int i = 0; i < BULK_SAMP_NUM_BUFFERS; i++)
                            {
                                out_buffers[i] = (void*)m->buffers[i];
                            }
                            out_index = 0;
                        }
                        break;
                    }
                }
                reply_confirm(payload[0]);
            }
        }
    }
}

static void clear_pru1_msg_flag()
{
    uint32_t zero = 0;
    __xout(10, 28, 0, zero);
}

static void setup_pru()
{
    clear_pru1_msg_flag();
    PRU0_CTRL.CTRL_bit.CTR_EN = 1;
#if 0
    /* Set up PRU0->PRU1 interrupts */
    /* Map event 16 (PRU0_PRU1_EVT) to channel 1 */
    CT_INTC.CMR4_bit.CH_MAP_16 = 1;
    /* Map channel 1 to host 1 */
    CT_INTC.HMR0_bit.HINT_MAP_1 = 1;
    /* Ensure event 16 is cleared */
    CT_INTC.SICR_bit.STS_CLR_IDX = PRU0_PRU1_EVT;
    /* Enable event 16 */
    CT_INTC.EISR_bit.EN_SET_IDX = PRU0_PRU1_EVT;
    /* Enable Host interrupt 1 */
    CT_INTC.HIEISR |= (1 << 0);
    /* Globally enable host interrupts */
    CT_INTC.GER = 1;
#endif
}

void setup_rpmsg()
{
    volatile uint8_t *status;
    /* allow OCP master port access by the PRU so the PRU can read external memories */
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    /* clear the status of event MB_INT_NUMBER (the mailbox event) and enable the mailbox event */
    CT_INTC.SICR_bit.STS_CLR_IDX = MB_INT_NUMBER;
    CT_MBX.IRQ[MB_USER].ENABLE_SET |= 1 << (MB_FROM_ARM_HOST * 2);

    /* Make sure the Linux drivers are ready for RPMsg communication */
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

    /* Initialize pru_virtqueue corresponding to vring0 (PRU to ARM Host direction) */
    pru_virtqueue_init(&transport.virtqueue0, &resourceTable.rpmsg_vring0, &CT_MBX.MESSAGE[MB_TO_ARM_HOST], &CT_MBX.MESSAGE[MB_FROM_ARM_HOST]);

    /* Initialize pru_virtqueue corresponding to vring1 (ARM Host to PRU direction) */
    pru_virtqueue_init(&transport.virtqueue1, &resourceTable.rpmsg_vring1, &CT_MBX.MESSAGE[MB_TO_ARM_HOST], &CT_MBX.MESSAGE[MB_FROM_ARM_HOST]);

    /* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
    while(pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, RPMSG_CHAN_NAME, RPMSG_CHAN_DESC, RPMSG_CHAN_PORT) != PRU_RPMSG_SUCCESS)
    {
        /* Try again */
    }

    for (int i = 0; i < BULK_SAMP_NUM_BUFFERS; i++)
    {
        out_buffers[i] = 0;
    }
}

// how many cycles between calls to GRAB_SAMPLE_INTERVAL, for testing.
#define GRAB_SAMPLE_INTERVAL (50 * XFER_SIZE)

void main() {
    uint32_t val;

    setup_rpmsg();
    setup_pru();

    uint32_t last_sample = PRU0_CTRL.CYCLE;

    while(1)
    {
#ifdef TEST_CLOCK_OUT
        if (going)
        {
            __delay_cycles(GRAB_SAMPLE_INTERVAL);
            grab_samples();
        }
#else
        __xin(10, 28, 0, val);
        /* Check for message from the other pru */
        if(val)
        {
            /* Clear interrupt */
            clear_pru1_msg_flag();
            if (going) {
                grab_samples();
            }
        }
#endif

        /* Check bit 31 of register R31 to see if the mailbox interrupt has occurred */
        if(__R31 & ARM_INT) 
        {
            handle_rpmsg();
        }
    }
}
