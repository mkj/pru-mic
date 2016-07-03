    .global     ||going||

    .bss     samplebuf,40  ; Define the variable

        .sect   ".text:sampleloop"
        .clink
;       "void sampleloop()"
        .global ||sampleloop||

||sampleloop||
        LDI       r14, ||going||   ; going variable address r14
        ldi       r1.b0, &r18.b0 ; base address for samples which are stored in registers r18-r22
        ldi       r1.b1, &r22.b3 ; end address

||$top||
        SET       r30, r30, 0x00000003 ; clock high
        NOP       ; 40ns/8cycle delay for data
        NOP
        NOP
        LBBO      &r1, r14, 0, 4              ; test for !going
        QBNE      ||$keepgoing||, r1, 0
        JMP       r3.w2
||$keepgoing||
        NOP
        NOP
        NOP
        LSL       r15, r31, 0x1c         ; read gpio
        LSR       r15, r15, 0x18         ; <<4
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
        NOP
        NOP ; 25 cycles

        CLR       r30, r30, 0x00000003  ; clock low
        NOP       ; 40ns/8cycle delay for data
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        AND       r16, r31, 0x0f         ; [ALU_PRU] |120| highval
        OR        r16, r16, 15
        mvib *r1.b0++, r16     ; store sample in r18-r22 buffer

        qblt ||$noxfer||, r1.b0, r1.b1

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
        nop ; 24 cycles
        jmp ||$top||

