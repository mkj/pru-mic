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
        .asg r15, rsample
        .asg r17, rcycleaddr
        ; r18-r22 are sample buffer
        .asg r27, rtmp27 ; trigger from pru1, general register
        .asg r28, rtmp28 ; triggers pru1, general register
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

        ldi r18, 0x18
        ldi r19, 0x19
        ldi r20, 0x20
        ldi r21, 0x21
        ldi r22, 0x22

        ; loop keeps running until an interrupt comes from pru1
        ; the loop is _exactly_ 50 cycles long, for 4mhz clock cycle

||$top||
        SET       r30, r30, 5 ; clock high
        XIN 10, &rtmp27, 4        ;    check r27.b0 for message from pru1
        QBBC      ||$keepgoing||, rtmp27, 1   
        JMP       r3.w2 ; return if message pending
||$keepgoing||
        NOP
        NOP
        NOP
        NOP
        LDI rsample, 0
        NOP ; 40ns/8 cycles wait done

        ; permute input gpio pin bits 1 2 3 7 -> 0 1 2 3
        ; P9_29 pr1_pru0_pru_r31_1 data0
        ; P9_30 pr1_pru0_pru_r31_2 data1
        ; P9_28 pr1_pru0_pru_r31_3 data2
        ; P9_25 pr1_pru0_pru_r31_7 data3
        ; P9_27 pr1_pru0_pru_r30_5 clock
        lsr rtmp28, r31, 1 ; data0, data1, data2 in place
        and rsample, rtmp28, 0111B ; mask 
        lsr rtmp27, rtmp28, 3; data3, total shift 4
        and rtmp27, rtmp27, 1000B
        or rsample, rsample, rtmp27

        ; untested optimisation using byte-offset operations
        ;lsl rtmp28, r31, 1
        ;or rtmp28, rtmp28, rtmp28.b1
        ;or rtmp28, rtmp28, rtmp28.b2
        ;lsr rtmp27, rtmp28, 4
        ;or rtmp28, rtmp28, rtmp27
        ;or rsample.b0, rsample.b0, rtmp28

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

        CLR       r30, r30, 5  ; clock low
        ;LBBO      &r22, rcycleaddr, 0, 4
        ; takes 4 cycles?
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP ; 40ns/8 cycle wait done

        ; permute input gpio pin bits 1 2 3 7 -> 4 5 6 7
        lsl rtmp28, r31, 3 ; data4, data5, data6 in place
        and rtmp27, rtmp28, 01110000B ; mask 10010000
        or rsample, rsample, rtmp27 
        lsr rtmp27, rtmp28, 3; data9, total shift right 1
        and rtmp27, rtmp27, 10000000B
        or rsample, rsample, rtmp27
        mvib *rindex++, rsample.b0     ; store sample in r18-r22 buffer
        NOP
        NOP
        NOP

        qblt ||$noxfer||, rindexend, rindex   ; take note, this "rindex < rindexend"

        ; transfer instructions
        xout 10, &r18, 20              ; transfer r18-r22 to bank0
        ldi rtmp28, 1
        xout 10, &rtmp28, 4               ; wake up pru1 with a flag 
        jmp ||$donexfer||

||$noxfer||
        nop ; nops to match the transfer instructions above
        nop
        nop
        nop

||$donexfer||

        jmp ||$top|| ; 25 cycles
