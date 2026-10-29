/* Fixed ARMv7-A operations. No system/security/clock register guessing. */
 .arch armv7-a
 .syntax unified
 .code 32
 .macro FN name
 .section .text.\name,"ax",%progbits
 .p2align 2
 .global \name
 .type \name,%function
\name:
 .endm
 .macro READ name,op1,crn,crm,op2
 FN peaq_dc_r_\name
 mrc p15,\op1,r0,\crn,\crm,\op2
 bx lr
 .endm
 .macro WRITE name,op1,crn,crm,op2
 FN peaq_dc_w_\name
 dsb sy
 mcr p15,\op1,r0,\crn,\crm,\op2
 isb sy
 bx lr
 .endm
 READ sctlr,0,c1,c0,0
 READ actlr,0,c1,c0,1
 READ midr,0,c0,c0,0
 READ ctr,0,c0,c0,1
 READ clidr,1,c0,c0,1
 READ ttbcr,0,c2,c0,2
 READ ttbr0,0,c2,c0,0
 READ ttbr1,0,c2,c0,1
 READ dacr,0,c3,c0,0
 READ csselr,2,c0,c0,0
 READ ccsidr,1,c0,c0,0
 WRITE sctlr,0,c1,c0,0
 WRITE ttbcr,0,c2,c0,2
 WRITE ttbr0,0,c2,c0,0
 WRITE ttbr1,0,c2,c0,1
 WRITE dacr,0,c3,c0,0
 WRITE csselr,2,c0,c0,0
 FN peaq_dc_r_cpsr
 mrs r0,cpsr
 bx lr
 FN peaq_dc_invalidate_setway
 mcr p15,0,r0,c7,c6,2
 bx lr
 FN peaq_dc_clean_setway
 mcr p15,0,r0,c7,c14,2
 bx lr
 FN peaq_dc_barrier
 dsb sy
 isb sy
 bx lr
 FN peaq_dc_tlb
 dsb sy
 mov r0,#0
 mcr p15,0,r0,c8,c7,0
 dsb sy
 isb sy
 bx lr
 FN peaq_dc_instruction
 dsb sy
 mov r0,#0
 mcr p15,0,r0,c7,c5,0
 mcr p15,0,r0,c7,c5,6
 dsb sy
 isb sy
 bx lr
