        .sect   ".text:sampleloop"
        .clink
;       "void sampleloop()"
        .global ||sampleloop||

||sampleloop||
        ldi       r1.b0, &r18.b0 ; base address for samples which are stored in registers r18-r22
        ldi       r1.b1, &r22.b3 ; end address
        LDI32     r0, 0x0002200c
        LBBO      &r0, r0, 0, 4   ; load CYCLE register
        SBBO      &r0, r1, 0, 4   ; save it to cyclestart

        ; loop keeps running until an interrupt comes from pru1
        ; the loop is _exactly_ 50 cycles long, for 4mhz clock cycle

||$top||
        SET       r30, r30, 0x00000003 ; clock high
        NOP       ; 40ns/8cycle delay for data
        NOP
        NOP
        QBBC      ||$keepgoing||, r31, 0x1e   ; test for PRU_INT (1<<30)
        JMP       r3.w2 ; return if an interrupt happened
||$keepgoing||
        NOP
        NOP
        NOP
        NOP

; real read
;       AND       r15, r31, 0xf     ; read low 4 bits, gpio samples from select-high mics
;       NOP
;       NOP

; cyclecount read
        LDI32     r15, 0x0002200c
        LBBO      &r15, r15, 0, 4   ; load CYCLE register
        AND       r15, r15, 0xf     ; read low 4 bits, gpio samples from select-high mics

        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP ; 25 cycles

        CLR       r30, r30, 0x00000003  ; clock low
        NOP       ; 40ns/8cycle delay for data
        NOP
        NOP
        NOP
        NOP
        NOP

; real read
;       NOP
;       NOP
;       AND       r16, r31, 0x0f         ; low 4 bits, gpio samples from select-low mics

; cyclecount read
        LDI32     r16, 0x0002200c
        LBBO      &r16, r16, 0, 4   ; load CYCLE register
        AND       r16, r16, 0xf     ; read low 4 bits, gpio samples from select-high mics

        LSL       r16, r16, 4           ; store in high nibble
        OR        r16, r16, 15
        mvib *r1.b0++, r16     ; store sample in r18-r22 buffer

        qble ||$noxfer||, r1.b0, r1.b1

        xout 14, &r18, 40              ; transfer r18-r22 to pru1
        LDI       r31, 0x0020          ; wake pru1
        ldi       r1.b0, &r18.b0 ; reset base address for samples
        jmp ||$donexfer||

||$noxfer||
        nop ; 4 nops to match the 4 transfer instructions above
        nop
        nop
        nop

||$donexfer||

        nop
        nop
        nop
        nop
        nop
        nop 
        jmp ||$top|| ; 25 cycles

