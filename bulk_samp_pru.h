#ifndef BULK_SAMP_PRU_H
#define BULK_SAMP_PRU_H


// from waitcycle.asm
void newcycle();
void waitcycle(uint32_t until);

volatile register uint32_t __R31;
volatile register uint32_t __R30;

typedef struct{
	uint8_t dat[40];
	/*
	XXX why did sample code do it like this?
	uint32_t reg5;
	uint32_t reg6;
	uint32_t reg7;
	uint32_t reg8;
	uint32_t reg9;
	uint32_t reg10;
	*/
} bufferData;

#define XFER_SIZE (sizeof(bufferData) / 8)

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


#endif /* BULK_SAMP_PRU_H */
