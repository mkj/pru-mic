        .sect   ".text:sampleloop"
        .clink
;       "void sampleloop()"
        .global ||sampleloop||

||sampleloop||
        ldi       r1.b0, &r18.b0 ; base address for samples which are stored in registers r18-r22
        ldi       r1.b1, 91 ; end address
        LDI32     r0, 0x0002200c
        LBBO      &r0, r0, 0, 4   ; load CYCLE register
        SBBO      &r0, r1, 0, 4   ; save it to cyclestart

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
        XIN 10, &r27, 4        ;    check r27.b0 for message from pru1
        QBBC      ||$keepgoing||, r27, 1   
        JMP       r3.w2 ; return if message pending
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
        AND       r15, r29, 0xf     ; read low 4 bits, gpio samples from select-high mics
        ADD       r29, r29, 1
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

; real read
;       NOP
;       NOP
;       AND       r16, r31, 0x0f         ; low 4 bits, gpio samples from select-low mics

; cyclecount read
        AND       r16, r29, 0xf     ; read low 4 bits, gpio samples from select-high mics
        ADD       r29, r29, 1
        NOP

        LSL       r16, r16, 4           ; store in high nibble
        OR        r16, r16, r15
        mvib *r1.b0++, r16.b0     ; store sample in r18-r22 buffer

        qblt ||$noxfer||, r1.b1, r1.b0   ; take note, this "r1.b0 <= r1.b1"

        ; transfer instructions
        xout 10, &r18, 20              ; transfer r18-r22 to bank0
        ldi r28, 1
        xout 10, &r28, 4               ; wake up pru1 with a flag in scratchpad register r28
        ldi       r1.b0, &r18.b0 ; reset base address for samples
        jmp ||$donexfer||

||$noxfer||
        nop ; 5 nops to match the 5 transfer instructions above
        nop
        nop
        nop
        nop

||$donexfer||

        nop
        nop
        nop
        nop
        nop 
        jmp ||$top|| ; 25 cycles
