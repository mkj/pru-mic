#include <stdint.h>
#include <stdio.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_virtqueue.h>
#include <pru_rpmsg.h>
#include <pru_ctrl.h>
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
		uint32_t pos = 0;
		uint32_t val;
		bufferData regbuf;
		while (going)
		{
			newcycle();
			// cycle 0
			// clock high 
			__R30 |= CLOCK_OUT; 
			__delay_cycles(8); // 40ns delay for data assert. 
#ifdef TEST_CLOCK_OUT
			val = PRU0_CTRL.CYCLE & 0xf;
#else
			val = __R31 & PIN_MASK; 
#endif
			waitcycle(25);
            // cycle 25
			// clock low
			__R30 &= ~CLOCK_OUT;
			__delay_cycles(8); // 40ns delay for data assert. 
#ifdef TEST_CLOCK_OUT
			val |= ((PRU0_CTRL.CYCLE & 0xf) << 4);
#else
			val |= ((__R31 & PIN_MASK) << 4);
#endif
			regbuf.dat[pos] = val;
			pos++;
			if (pos == XFER_SIZE) 
			{
				PRU0_PRU1_TRIGGER;
				__xin(14, XFER_SIZE, 0, regbuf);
				pos = 0;
			}
			waitcycle(50 - 3 - 2); // 50 cycles - 3 for this call - 2 for while loop
		}
	}
}
