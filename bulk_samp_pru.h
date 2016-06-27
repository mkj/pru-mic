#ifndef BULK_SAMP_PRU_H
#define BULK_SAMP_PRU_H

extern volatile register uint32_t __R31;

typedef struct{
	uint32_t reg5;
	uint32_t reg6;
	uint32_t reg7;
	uint32_t reg8;
	uint32_t reg9;
	uint32_t reg10;
} bufferData;

#define PRU0_PRU1_EVT (16)
#define PRU0_PRU1_TRIGGER (__R31 = (PRU0_PRU1_EVT - 16) | (1 << 5))

#define PRU1_PRU0_EVT (17)
#define PRU1_PRU0_TRIGGER (__R31 = (PRU1_PRU0_EVT - 16) | (1 << 5))

#endif /* BULK_SAMP_PRU_H */
