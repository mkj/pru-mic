    .bss     cyclestart,4,4  ; Define the variable

; from C code
;   cyclestart = PRU0_CTRL.CYCLE - 3;

	.sect	".text:newcycle"
	.clink
	.global	||newcycle||
||newcycle||
        LDI32     r0, 0x0002200c
        LDI       r1, ||cyclestart||
        LBBO      &r0, r0, 0, 4
        SUB       r0, r0, 0x02
        SBBO      &r0, r1, 0, 4
        JMP       r3.w2


; from C code something like
;	uint32_t now = PRU0_CTRL.CYCLE;
;	uint32_t delay = (cyclestart + until) - now;
;	while (delay--)  {}

	.sect	".text:waitcycle"
	.clink
	.global	||waitcycle||
||waitcycle||
        LDI32     r0, 0x0002200c
        LBBO      &r1, r0, 0, 4   ; load CYCLE register
        LDI       r0, ||cyclestart||
        LBBO      &r0, r0, 0, 4   ; load cyclestart
        ADD       r0, r0, r14
        RSB       r0, r1, r0      ; delay = (cyclestart + until) - now;
        SUB       r0, r0, 8       ; subtract cycles used in this function
        QBBC      ||$delaycmp||, r0, 1    ; odd delay gets a single subtraction
||$delaysub||:
        SUB       r0, r0, 0x01    ; delay--
||$delaycmp||:
        QBNE      ||$delaysub||, r0, 0x00
        JMP       r3.w2
