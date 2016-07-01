    .bss     cyclestart,4,4  ; Define the variable

; from C code
;   cyclestart = PRU0_CTRL.CYCLE - 3;

	.sect	".text:newcycle"
	.clink
	.global	||newcycle||
||newcycle||
        LDI32     r0, 0x0002200c
        LDI       r1, ||cyclestart||
        LBBO      &r0, r0, 0, 4   ; load CYCLE register - extra delay from here onwards
        ADD       r0, r0, 4 ; cycle register 4 cycles later when this function returns
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
        LBBO      &r1, r0, 0, 4   ; load CYCLE register - extra delay from here
        LDI       r0, ||cyclestart||
        LBBO      &r0, r0, 0, 4   ; load cyclestart
        ADD       r0, r0, r14
        RSB       r0, r1, r0      ; delay = (cyclestart + until) - now
        SUB       r0, r0, 10       ; subtract extra delay=10
        LSR       r15, r0, 1      ; divide delay by two, delayloop is 2 insns SUB/QBNE
        QBBC      ||$delayloop||, r0, 1
        MOV       r0, r0         ; nop for odd delay extra instruction
||$delayloop||:
        SUB       r15, r15, 0x01    ; delay--
        QBNE      ||$delayloop||, r15, 0x00
        JMP       r3.w2
