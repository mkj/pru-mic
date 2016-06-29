#include <stdint.h>
#include <stdio.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_virtqueue.h>
#include <pru_rpmsg.h>
#include <sys_mailbox.h>
#include "resource_table_0.h"

#include "bulk_samp_common.h"
#include "bulk_samp_pru.h"

char going = 1;

void setup_pru_comm()
{
	/* Set up PRU1->PRU0 interrupts */
	/* Map event 17 (PRU1_PRU0_EVT) to channel 1 */
	CT_INTC.CMR4_bit.CH_MAP_17 = 1;
	/* Map channel 1 to host 1 */
	CT_INTC.HMR0_bit.HINT_MAP_1 = 1;
	/* Ensure event 17 is cleared */
	CT_INTC.SICR_bit.STS_CLR_IDX = PRU1_PRU0_EVT;
	/* Enable event 17 */
	CT_INTC.EISR_bit.EN_SET_IDX = PRU1_PRU0_EVT;
	/* Enable Host interrupt 1 */
	CT_INTC.HIEISR |= (1 << 0);
	/* Globally enable host interrupts */
	CT_INTC.GER = 1;
}

void handle_pru_msg()
{
	CT_INTC.SICR_bit.STS_CLR_IDX = PRU1_PRU0_EVT;
	switch (pru_sharedmem->pru0_msg)
	{
		case BULK_SAMP_MSG_START:
		{
			going = 1;
		}
		break;
		case BULK_SAMP_MSG_STOP:
		{
			going = 0;
		}
		break;
	}
}

// pr1_pru0_pru_r31_0 to pr1_pru0_pru_r31_3
#define PIN_MASK 0xf
#define CLOCK_OUT (1<<3)

void main() 
{
	while (1)
	{
		if(__R31 & PRU_INT) 
		{
			handle_pru_msg();
		}
		char pos = 0;
		bufferData regbuf;
		while (going)
		{
			// This is the timing critical loop
			// clock high
			__R30 |= CLOCK_OUT; // +1 cycle = 1
			__delay_cycles(8); // 40ns delay for data assert. +8 = 9
			regbuf.dat[pos] = __R31 & PIN_MASK; // +5 = 14
			__delay_cycles(9); // 9 cycles delay to get up to 25, half duty. +15 = 25

			__R30 ^= ~CLOCK_OUT; // +2 cycle = 27
			__delay_cycles(8); // 40ns delay for data assert. +8 = 35

			regbuf.dat[pos] |= ((__R31 & PIN_MASK) << 4) ^ 0xf0; // +7 = 42

			pos++;
			if (pos == XFER_SIZE) 
			{
				PRU0_PRU1_TRIGGER;
				__xin(14, XFER_SIZE, 0, regbuf);
				pos = 0;
				__delay_cycles(9); // XXX?
			}
			else
			{
				__delay_cycles(12); // XXX?
			}
		}
	}
}
