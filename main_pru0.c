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

asm("	.global	||waitcycle||");

void main() 
{
	while (1)
	{
		if(__R31 & PRU_INT) 
		{
			handle_pru_msg();
		}
		uint32_t highval, lowval;
		bufferData regbuf;
		uint8_t *p = regbuf.dat;
		initcycle();
		while (going)
		{
			// wait then immediately clock
			// waitcycle(25); 
			// __R30 |= CLOCK_OUT;
			// __delay_cycles(8);
			asm(
"        LDI       r14, 0x0019 \n"
"        JAL       r3.w2, ||waitcycle|| \n"
"        SET       r30, r30, 0x00000003 \n"
"        MOV       r0,r0 \n" // 40ns delay/8 cycles
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
);

#ifdef TEST_CLOCK_OUT
#warning 
#warning > > > > > > > > > > > > > > > > > > > > > > > >
#warning > > Building with TEST_CLOCK_OUT
#warning > > > > > > > > > > > > > > > > > > > > > > > >
#warning 
			lowval = (PRU0_CTRL.CYCLE & 0xf) << 4;
#else
			lowval = (__R31 & PIN_MASK) << 4;
#endif
			asm(
"	.global	||waitcycle|| \n"
"        LDI       r14, 0x0019 \n"
"        JAL       r3.w2, ||waitcycle|| \n"
"        SET       r30, r30, 0x00000003 \n"
"        MOV       r0,r0 \n" // 40ns delay/8 cycles
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
"        MOV       r0,r0 \n"
);
#ifdef TEST_CLOCK_OUT
			highval = PRU0_CTRL.CYCLE & 0xf
			//val |= ((PRU0_CTRL.CYCLE & 0xf) << 4);
#else
			highval = __R31 & PIN_MASK;
			//val |= ((__R31 & PIN_MASK) << 4);
#endif
			//regbuf.dat[pos] = lowval | (highval<<4);
			*p = lowval | highval;
			p++;
			if (p == (regbuf.dat + XFER_SIZE)) 
			{
				PRU0_PRU1_TRIGGER;
				__xout(14, XFER_SIZE, 0, regbuf);
				p = regbuf.dat;
			}
		}
	}
}
