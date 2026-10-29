/* A single function call with owned ARMv7 exception vectors and stacks.
 * Separate I-cache variant: entry requires M/C/I off; target may enable I.
 * Cache cleanup MUST verify I off before original vector/control restoration.
 * Abort/undefined/SVC/IRQ/FIQ never return to the interrupted instruction.
 * The caller's saved System-mode frame is resumed instead. No MMIO here.
 */
 .arch armv7-a
 .syntax unified
 .code 32
 .section .text.peaq_fault_vectors,"ax",%progbits
 .p2align 5
 .global peaq_fault_vectors
peaq_fault_vectors:
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 ldr pc,[pc,#24]
 .long peaq_fault_reset,peaq_fault_undef,peaq_fault_svc,peaq_fault_prefetch
 .long peaq_fault_abort,peaq_fault_reserved,peaq_fault_irq,peaq_fault_fiq

 .macro BANK_SAVE mode,slot
 msr CPSR_c,#(0xc0+\mode)
 str sp,[r6,#(320+\slot*4)]
 str lr,[r6,#(340+\slot*4)]
 mrs r0,spsr
 ldr r1,=peaq_fault_bank_spsr
 str r0,[r1,#(\slot*4)]
 ldr sp,=(peaq_fault_stacks+\slot*640+576)
 .endm
 .macro BANK_RESTORE mode,slot
 msr CPSR_c,#(0xc0+\mode)
 ldr sp,[r6,#(320+\slot*4)]
 ldr lr,[r6,#(340+\slot*4)]
 ldr r1,=peaq_fault_bank_spsr
 ldr r0,[r1,#(\slot*4)]
 msr SPSR_cxsf,r0
 .endm
 .macro CP_READ offset,opc1,crn,crm,opc2
 mrc p15,\opc1,r0,\crn,\crm,\opc2
 str r0,[r6,#\offset]
 .endm

 .section .text.peaq_fault_call,"ax",%progbits
 .p2align 2
 .global peaq_fault_call
 .type peaq_fault_call,%function
peaq_fault_call:
 push {r4-r11,lr}
 sub sp,sp,#4
 mov r4,r0
 mov r5,r1
 ldr r6,=peaq_fault
 str sp,[r6,#184]
 str lr,[r6,#188]
 str r4,[r6,#360]
 str r5,[r6,#364]
 mrs r0,cpsr
 str r0,[r6,#64]
 and r1,r0,#255
 cmp r1,#0xdf
 bne peaq_fault_bad_context
 mrc p15,0,r0,c1,c0,0
 str r0,[r6,#48]
 ldr r1,=0x1005
 tst r0,r1
 bne peaq_fault_bad_context
 /* Vector stubs are little-endian ARM instructions. */
 tst r0,#0x40000000
 bne peaq_fault_bad_context
 tst r0,#0x02000000
 bne peaq_fault_bad_context
 mrc p15,0,r1,c12,c0,0
 str r1,[r6,#52]
 BANK_SAVE 0x17,0
 BANK_SAVE 0x1b,1
 BANK_SAVE 0x13,2
 BANK_SAVE 0x12,3
 BANK_SAVE 0x11,4
 msr CPSR_c,#0xdf
 ldr r0,=peaq_fault_vectors
 str r0,[r6,#40]
 dsb sy
 mcr p15,0,r0,c12,c0,0
 ldr r0,[r6,#48]
 bic r0,r0,#0x2000
 mcr p15,0,r0,c1,c0,0
 isb sy
 str r0,[r6,#44]
 mov r0,#1
 str r0,[r6,#12]
 str r0,[r6,#16]
 str r0,[r6,#308]
 mov r0,#10
 str r0,[r6,#32]
 CP_READ 192,0,c0,c0,0
 CP_READ 196,0,c0,c0,5
 CP_READ 200,0,c0,c0,6
 CP_READ 204,0,c0,c0,1
 CP_READ 208,0,c0,c1,0
 CP_READ 212,0,c0,c1,1
 CP_READ 216,0,c0,c1,2
 CP_READ 220,0,c0,c1,3
 CP_READ 224,0,c0,c1,4
 CP_READ 228,0,c0,c1,5
 CP_READ 232,0,c0,c1,6
 CP_READ 236,0,c0,c1,7
 CP_READ 240,0,c0,c2,0
 CP_READ 244,0,c0,c2,1
 CP_READ 248,0,c0,c2,2
 CP_READ 252,0,c0,c2,3
 CP_READ 256,0,c0,c2,4
 CP_READ 260,0,c0,c2,5
 CP_READ 264,1,c0,c0,1
 CP_READ 268,0,c1,c0,1
 CP_READ 272,0,c1,c0,2
 CP_READ 276,0,c2,c0,2
 CP_READ 280,0,c2,c0,0
 CP_READ 284,0,c2,c0,1
 CP_READ 288,0,c3,c0,0
 CP_READ 292,0,c12,c0,0
 CP_READ 296,0,c1,c0,0
peaq_fault_verify_vectors:
 ldr r0,[r6,#292]
 ldr r1,[r6,#40]
 cmp r0,r1
 ldreq r0,[r6,#296]
 ldreq r1,[r6,#44]
 cmpeq r0,r1
 bne peaq_fault_vector_refused
 mov r0,r5
 blx r4
 str r0,[r6,#36]
 mov r0,#2
 str r0,[r6,#16]
 mov r0,#3
 str r0,[r6,#12]
 b peaq_fault_restore
peaq_fault_vector_refused:
 mov r0,#3
 str r0,[r6,#20]
 b peaq_fault_restore
peaq_fault_bad_context:
 mov r0,#1
 str r0,[r6,#20]
 mvn r0,#0
 add sp,sp,#4
 pop {r4-r11,pc}

peaq_fault_restore:
 bl peaq_cache_fault_leave
 /* Runs in System mode on the original caller frame, even after abort. */
 ldr r0,[r6,#52]
 dsb sy
 mcr p15,0,r0,c12,c0,0
 ldr r0,[r6,#48]
 mcr p15,0,r0,c1,c0,0
 isb sy
 BANK_RESTORE 0x17,0
 BANK_RESTORE 0x1b,1
 BANK_RESTORE 0x13,2
 BANK_RESTORE 0x12,3
 BANK_RESTORE 0x11,4
 msr CPSR_c,#0xdf
 CP_READ 56,0,c1,c0,0
 CP_READ 60,0,c12,c0,0
 ldr r0,[r6,#64]
 msr CPSR_cxsf,r0
 mrs r0,cpsr
 str r0,[r6,#300]
 mov r0,#0
 str r0,[r6,#308]
 mov r0,#1
 str r0,[r6,#312]
 ldr r1,[r6,#48]
 ldr r2,[r6,#56]
 cmp r1,r2
 ldreq r1,[r6,#52]
 ldreq r2,[r6,#60]
 cmpeq r1,r2
 moveq r0,#1
 movne r0,#0
 str r0,[r6,#316]
 ldr r1,[r6,#12]
 orr r1,r1,#8
 str r1,[r6,#12]
 tst r1,#4
 moveq r0,#1
 movne r0,#0
 add sp,sp,#4
 pop {r4-r11,pc}

 .macro FAULT_ENTRY name,kind
 .p2align 2
\name:
 push {r0-r12,lr}
 mrs r1,cpsr
 mov r0,#\kind
 b peaq_fault_capture
 .endm
 FAULT_ENTRY peaq_fault_reset,0
 FAULT_ENTRY peaq_fault_undef,1
 FAULT_ENTRY peaq_fault_svc,2
 FAULT_ENTRY peaq_fault_prefetch,3
 FAULT_ENTRY peaq_fault_abort,4
 FAULT_ENTRY peaq_fault_reserved,5
 FAULT_ENTRY peaq_fault_irq,6
 FAULT_ENTRY peaq_fault_fiq,7
peaq_fault_capture:
 ldr r6,=peaq_fault
 str r1,[r6,#68]
 ldr r1,[r6,#308]
 cmp r1,#1
 bne peaq_fault_nested_halt
 mov r1,#2
 str r1,[r6,#308]
 str r0,[r6,#28]
 mrs r1,spsr
 str r1,[r6,#72]
 ldr r2,[sp,#52]
 str r2,[r6,#76]
 mov r3,#0
 str r3,[r6,#84]
 cmp r0,#4
 subeq r3,r2,#8
 moveq r1,#1
 beq 1f
 cmp r0,#3
 subeq r3,r2,#4
 moveq r1,#1
 beq 1f
 mov r1,#0
1:
 str r3,[r6,#80]
 str r1,[r6,#84]
 add r1,sp,#56
 str r1,[r6,#88]
 mov r2,#0
2:
 ldr r3,[sp,r2]
 add r1,r6,#128
 str r3,[r1,r2]
 add r2,r2,#4
 cmp r2,#52
 bne 2b
 /* FIQ banks r8-r12; report the interrupted System/User registers. */
 add r1,r6,#160
 stmia r1,{r8-r12}^
 nop
 movw r1,#0x1fff
 str r1,[r6,#124]
 mov r1,#0
 cmp r0,#1
 moveq r1,#1
 cmp r0,#2
 moveq r1,#2
 cmp r0,#6
 moveq r1,#3
 cmp r0,#7
 moveq r1,#4
 str r1,[r6,#180]
 CP_READ 96,0,c5,c0,0
 CP_READ 100,0,c6,c0,0
 CP_READ 104,0,c5,c0,1
 CP_READ 108,0,c6,c0,2
 CP_READ 112,0,c5,c1,0
 CP_READ 116,0,c5,c1,1
 CP_READ 120,0,c13,c0,1
 mov r0,#1
 str r0,[r6,#372]
 mov r0,#3
 str r0,[r6,#16]
 mov r0,#5
 str r0,[r6,#12]
 msr CPSR_c,#0xdf
 str sp,[r6,#92]
 ldr sp,[r6,#184]
 b peaq_fault_restore
peaq_fault_nested_halt:
 b peaq_fault_nested_halt
 .ltorg
