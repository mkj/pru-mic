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

#include "resource_table_0.h"

#include "../bulk_samp_common.h"
#include "bulk_samp_pru.h"

static char going = 0;

static void clear_pru0_msg_flag()
{
	uint32_t zero = 0;
    __xout(10, 27, 0, zero);
}

void setup_pru()
{
	clear_pru0_msg_flag();
    PRU0_CTRL.CTRL_bit.CTR_EN = 1;
#if 0
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
#endif
}

void handle_pru_msg()
{
	uint32_t val;
    __xin(10, 27, 0, val);
    if (!val) {
    	return;
    }

    clear_pru0_msg_flag();

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

void main() 
{
	while (1)
	{
		if (going)
		{
			sampleloop();
		}
		handle_pru_msg();
	}
}
