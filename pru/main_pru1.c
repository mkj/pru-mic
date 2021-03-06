/*
 * Matt Johnston <matt@ucc.asn.au> 2016
 *
 * Based on sample code:
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

#include <stdint.h>
#include <stdio.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>
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

/* Host-1 Interrupt sets bit 31 in register R31 */
#define HOST_INT            ((uint32_t) 1 << 31)

/* The PRU-ICSS system events used for RPMsg are defined in the Linux device tree
 * PRU0 uses system event 16 (To ARM) and 17 (From ARM)
 * PRU1 uses system event 18 (To ARM) and 19 (From ARM)
 */
#define TO_ARM_HOST         18
#define FROM_ARM_HOST           19

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

static uint32_t buffer_count;
static uint32_t buffer_size;
static uint8_t *out_buffers[BULK_SAMP_MAX_NUM_BUFFERS];
static uint8_t out_index;
static int out_pos;

struct pru_rpmsg_transport transport;
uint16_t src, dst;

char going = 0;

static void reply_confirm(uint8_t type);
static void send_message_debug(const char* s1, const char* s2,
    uint32_t n1, uint32_t n2, uint32_t n3);

/* Receive samples from the other PRU, fill buffer and send to the 
ARM host if ready */
void grab_samples()
{
    struct 
    __attribute__((__packed__))
    _regbuf {
        // 32 bytes
        uint32_t x0;
        uint32_t x1;
        uint32_t x2;
        uint32_t x3;
        uint32_t x4;
        uint32_t x5;
        uint32_t x6;
        uint32_t x7;
    } regbuf; 
    // base address is r18
#ifdef TEST_CLOCK_OUT
    regbuf.x0 = 'c';
    regbuf.x1 = 'h';
    regbuf.x2 = 'a';
    regbuf.x3 = 'i';
    regbuf.x4 = 'r';
    regbuf.x5 = 'i';
    regbuf.x6 = 'o';
    regbuf.x7 = 't';
#else
    __xin(10, 18, 0, regbuf);
#endif

    if (!out_buffers[out_index])
    {
        // safety
        return;
    }

    /* Copy into payload */
    *((struct _regbuf*)&out_buffers[out_index][out_pos]) = regbuf;
    out_pos += XFER_SIZE;

    /* Buffer is full, send it */
    if (out_pos > buffer_size - XFER_SIZE) {
        struct bulk_samp_msg_ready *m = (void*)payload;
        m->type = BULK_SAMP_MSG_READY;
        m->buffer_index = out_index;
        m->size = out_pos;
        m->checksum = 0;
        m->magic = 12349876;
        pru_rpmsg_send(&transport, dst, src, payload, sizeof(struct bulk_samp_msg_ready));
        // reset buffers
        out_index = (out_index+1) % buffer_count;
        out_pos = 0;
#ifdef TEST_CLOCK_OUT
        going = 0;
#endif
    }
}

void poke_pru0(char msg, int value)
{
    uint32_t one = 1;
    pru_sharedmem->pru0_msg = msg;
    pru_sharedmem->pru0_val = value;

    // trigger
    __xout(10, 27, 0, one);
}

static void reply_confirm(uint8_t type)
{
    struct bulk_samp_msg_confirm msg = 
    { 
        .type = BULK_SAMP_MSG_CONFIRM,
        .confirm_type = type,
    };

    pru_rpmsg_send(&transport, dst, src, &msg, sizeof(msg));
}

static void send_message_debug(const char* s1, const char* s2,
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
    reset_cyclecount();
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
    /* Use a while loop to read all of the current messages in the mailbox */
    /* Receive the message */
    while (pru_rpmsg_receive(&transport, &src, &dst, payload, &len) == PRU_RPMSG_SUCCESS)
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

                    //send_message_debug("cycle", "stop message", PRU0_CTRL.CYCLE_bit.CYCLECOUNT, 666, PRU0_CTRL.STALL);

                }
                break;
                case BULK_SAMP_MSG_BUFFERS:
                {
                    struct bulk_samp_msg_buffers *m = (void*)payload;
                    buffer_count = m->buffer_count;
                    buffer_size = m->buffer_size;
                    for (int i = 0; i < buffer_count; i++)
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

static void clear_pru1_msg_flag()
{
    uint32_t zero = 0;
    __xout(10, 28, 0, zero);
}

static void setup_pru()
{
    clear_pru1_msg_flag();
    reset_cyclecount();
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

    /* Allow OCP master port access by the PRU so the PRU can read external memories */
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    /* Clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us */
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

    /* Make sure the Linux drivers are ready for RPMsg communication */
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

    /* Initialize the RPMsg transport structure */
    pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0, &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

    /* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
    while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, RPMSG_CHAN_NAME, RPMSG_CHAN_DESC, RPMSG_CHAN_PORT) != PRU_RPMSG_SUCCESS);

    for (int i = 0; i < BULK_SAMP_MAX_NUM_BUFFERS; i++)
    {
        out_buffers[i] = 0;
    }

    send_message_debug("bulksamp pru firmware 2019", "Matt Johnston matt@ucc.asn.au", 0, 0, 0);
}

// how many cycles between calls to GRAB_SAMPLE_INTERVAL, for testing.
#define GRAB_SAMPLE_INTERVAL (50 * XFER_SIZE)

void main() {
    uint32_t val;

    setup_rpmsg();
    setup_pru();

    //send_message_debug("cycle", "begin", PRU0_CTRL.CYCLE_bit.CYCLECOUNT, 888, PRU0_CTRL.STALL);

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

        /* Check bit 31 of register R31 to see if the ARM has kicked us */
        if (__R31 & HOST_INT) 
        {
            handle_rpmsg();
        }
    }
}
