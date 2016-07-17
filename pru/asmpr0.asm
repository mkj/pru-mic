        .sect   ".text:sampleloop"
        .clink
;       "void sampleloop()"
        .global ||sampleloop||

||sampleloop||

        ; registers
        ; ref spruhv7a Table 6-4 Register Usage
        .asg r1.b0, rindex ; mvib only works on r0 and r1
        .asg r1.b1, rindexend
        ; r2 - stack
        ; r3.w2 - return address
        ; r4 - argument pointer
        ; r5-r13 save on entry
        .asg r15, rlowbits
        .asg r16, rhighbits
        .asg r17, rcycleaddr
        ; r18-r22 are sample buffer
        .asg r25, rsamplecount
        .asg r26, rcountindex
        .asg r27, rxinflag
        .asg r28, rtmp28
        .asg r29, rcounter
        ; r30/r31 control

        ; reset cycle counter to 0x7654
        LDI32     r0, 0x00022000        ; [ALU_PRU] |36| $O$C1
        LBBO      &r1, r0, 0, 4         ; [ALU_PRU] |36| 
        CLR       r1, r1, 0x00000003    ; [ALU_PRU] |36| 
        SBBO      &r1, r0, 0, 4         ; [ALU_PRU] |36| 
        LDI       r1, 0x7654            ; [ALU_PRU] |37| 
        SBBO      &r1, r0, 0xc, 4        ; [ALU_PRU] |37| $O$C1
        LBBO      &r1, r0, 0, 4         ; [ALU_PRU] |38| $O$C1
        SET       r1, r1, 0x00000003    ; [ALU_PRU] |38| 
        SBBO      &r1, r0, 0, 4         ; [ALU_PRU] |38| $O$C1

        ldi       rindex, &r18.b0 ; base address for samples which are stored in registers r18-r22
        ldi       rindexend, &r23.b0 ; end address
        LDI32     rcycleaddr, 0x0002200c

        LDI       rsamplecount, 0

        ldi       rcountindex, 24  ; send out msb first
        ;LBBO      &rcounter, rcycleaddr, 0, 4
        ldi32       rcounter, 0x1a2b3c4d

        ldi r18, 0x18
        ldi r19, 0x19
        ldi r20, 0x20
        ldi r21, 0x21
        ldi r22, 0x22

        ; loop keeps running until an interrupt comes from pru1
        ; the loop is _exactly_ 50 cycles long, for 4mhz clock cycle

||$top||
        SET       r30, r30, 0x00000003 ; clock high
        NOP       ; 40ns/8cycle delay for data
        NOP
        XIN 10, &rxinflag, 4        ;    check r27.b0 for message from pru1
        QBBC      ||$keepgoing||, rxinflag, 1   
        JMP       r3.w2 ; return if message pending
||$keepgoing||
        NOP
        NOP
        NOP
        NOP

; real read
;       AND       rtmp15, r31, 0xf     ; read low 4 bits, gpio samples from select-high mics
;       NOP

; cyclecount read
        lsr rlowbits, rcounter, rcountindex
        AND       rlowbits, rlowbits, 0xf     ; read low 4 bits, gpio samples from select-high mics

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

; real read
;       NOP
;       NOP
;       AND       rhighbits, r31, 0x0f         ; 4 bits, gpio samples from select-low mics

        lsr rhighbits, rcounter, rcountindex
        AND       rhighbits, rhighbits, 0xf0     ; read low 4 bits, gpio samples from select-high mics
        ;LSL       rhighbits, rhighbits, 4           ; store in high nibble
        NOP ; real case we would be reading the same bits again, need to shift

        SUB       rcountindex, rcountindex, 8
        AND       rcountindex, rcountindex, 31 ; 0-31 bits

        OR        rhighbits, rhighbits, rlowbits
        ; for some reason mvib only seems to work with b3 source byte?
        mvib *rindex++, rhighbits.b0     ; store sample in r18-r22 buffer

        qblt ||$noxfer||, rindexend, rindex   ; take note, this "rindex <= rindexend"

        ; transfer instructions
        LBBO      &r22, rcycleaddr, 4, 4
        add rsamplecount, rsamplecount, 1
        xout 10, &r18, 20              ; transfer r18-r22 to bank0
        ldi rtmp28, 1
        xout 10, &rtmp28, 4               ; wake up pru1 with a flag 
        ldi       rindex, &r18.b0 ; reset base address for samples
        ;ldi       rcountindex, 3  ; send out msb first
        ;LBBO      &rcounter, rcycleaddr, 0, 4
        ;ldi32       rcounter, 0x1a2b3c4d
        add       rcounter, rcounter, 1
        jmp ||$donexfer||

||$noxfer||
        nop ; nops to match the transfer instructions above
        nop
        nop
        nop
        nop
        nop
        nop 
        nop

||$donexfer||

        nop
        nop
        jmp ||$top|| ; 25 cycles
