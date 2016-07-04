/*
 * Copyright (C) 2015 Texas Instruments Incorporated - http://www.ti.com/ 
 *  
 *  
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 * 	* Redistributions of source code must retain the above copyright 
 * 	  notice, this list of conditions and the following disclaimer.
 * 
 * 	* Redistributions in binary form must reproduce the above copyright
 * 	  notice, this list of conditions and the following disclaimer in the 
 * 	  documentation and/or other materials provided with the   
 * 	  distribution.
 * 
 * 	* Neither the name of Texas Instruments Incorporated nor the names of
 * 	  its contributors may be used to endorse or promote products derived
 * 	  from this software without specific prior written permission.
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
#include <sys_mailbox.h>
#include "resource_table_1.h"

#include "../bulk_samp_common.h"
#include "bulk_samp_pru.h"

/* PRU1 is mailbox module user 2 */
#define MB_USER	2
/* Mbox0 - mail_u2_irq (mailbox interrupt for PRU1) is Int Number 59 */
#define MB_INT_NUMBER 59

/* Host-1 Interrupt sets bit 31 in register R31, host int 1 */
#define ARM_INT ((uint32_t)(1 << 31))

/* The mailboxes used for RPMsg are defined in the Linux device tree
 * PRU0 uses mailboxes 2 (From ARM) and 3 (To ARM)
 * PRU1 uses mailboxes 4 (From ARM) and 5 (To ARM)
 */
#define MB_TO_ARM_HOST				5
#define MB_FROM_ARM_HOST			4

/*
 * Using the name 'rpmsg-client-sample' will probe the RPMsg sample driver
 * found at linux-x.y.z/samples/rpmsg/rpmsg_client_sample.c
 *
 * Using the name 'rpmsg-pru' will probe the rpmsg_pru driver found
 * at linux-x.y.z/drivers/rpmsg/rpmsg_pru.c
 */
//#define CHAN_NAME					"rpmsg-client-sample"
#define RPMSG_CHAN_NAME					"bulksamp-pru"

/* XXX give a nice name */
#define RPMSG_CHAN_DESC					"bulksamp PRU input interface"
#define RPMSG_CHAN_PORT					31

/* 
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4

#define PAYLOAD_SIZE (RPMSG_BUF_SIZE-16)
uint8_t payload[PAYLOAD_SIZE];

/* Sample payload and position */
static uint8_t out_payload[PAYLOAD_SIZE];


static uint8_t *out_buffers[BULK_SAMP_NUM_BUFFERS];
static uint8_t out_index;
static int out_pos;

struct pru_rpmsg_transport transport;
uint16_t src, dst;

char going;

/* Receive samples from the other PRU, fill buffer and send to the 
ARM host if ready */
void grab_samples()
{
	bufferData regbuf;
	/* XFR registers R5-R10 from PRU0 to PRU1 */
	/* 14 is the device_id that signifies a PRU to PRU transfer */
	__xin(14, XFER_SIZE, 0, regbuf);

	if (!out_buffers[out_index])
	{
		// safety
		return;
	}

	/* Copy into payload */
	*((bufferData*)&out_buffers[out_index][out_pos]) = regbuf;
	out_pos += sizeof(bufferData);

	/* Buffer is full, send it */
	if (BULK_SAMP_BUFFER_SIZE - out_pos < XFER_SIZE) {
		struct bulk_samp_msg_ready *m = (void*)out_payload;
		m->type = BULK_SAMP_MSG_READY;
		m->buffer_index = out_index;
		m->size = out_pos;
		m->checksum = 0;
		m->magic = 12349876;
		pru_rpmsg_send(&transport, dst, src, out_payload, sizeof(struct bulk_samp_msg_ready));
		// reset buffers
		out_index = (out_index+1) % BULK_SAMP_NUM_BUFFERS;
		out_pos = 0;
	}
}

void poke_pru0(char msg, int value)
{
	pru_sharedmem->pru0_msg = msg;
	pru_sharedmem->pru0_val = value;

	PRU1_PRU0_TRIGGER;
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
			}
		}
	}
}

void setup_pru_comm()
{
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

/*
 * main.c
 */
void main() {
	going = 0; /* whether to be sending samples to the arm host */

	setup_rpmsg();
	setup_pru_comm();

	while(1)
	{
		/* Check for message from the other pru */
		if(__R31 & PRU_INT) 
		{
			/* Clear interrupt */
			CT_INTC.SICR_bit.STS_CLR_IDX = PRU0_PRU1_EVT;
			if (going) {
				grab_samples();
			}
		}

		/* Check bit 31 of register R31 to see if the mailbox interrupt has occurred */
		if(__R31 & ARM_INT) 
		{
			handle_rpmsg();
		}
	}
}
