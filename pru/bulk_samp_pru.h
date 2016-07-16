#ifndef BULK_SAMP_PRU_H
#define BULK_SAMP_PRU_H

// Matt Johnston <matt@ucc.asn.au> 2016

volatile register uint32_t __R31;
volatile register uint32_t __R30;

#define XFER_SIZE 20

#define PRU0_PRU1_EVT (16)
#define PRU0_PRU1_TRIGGER (__R31 = (PRU0_PRU1_EVT - 16) | (1 << 5))

#define PRU1_PRU0_EVT (17)
#define PRU1_PRU0_TRIGGER (__R31 = (PRU1_PRU0_EVT - 16) | (1 << 5))

/* Both use the same */
#define PRU_INT (1 << 30)

/* pru shared memory */
#define DPRAM_SHARED	0x00010000

struct bulk_samp_shared {
	/* message to pru0 */
	char pru0_msg;
	int pru0_val;

	/* to pru1 */
	char pru1_msg;
	int pru1_val;
};

volatile struct bulk_samp_shared *pru_sharedmem = (struct bulk_samp_shared*)((void*)DPRAM_SHARED);

static void reset_cyclecount() {
    PRU0_CTRL.CTRL_bit.CTR_EN = 0;
    PRU0_CTRL.CYCLE = 0;
    PRU0_CTRL.CTRL_bit.CTR_EN = 1;
}

#endif /* BULK_SAMP_PRU_H */
