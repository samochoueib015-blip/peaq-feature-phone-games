/* Deliberately tiny ARM helpers, audited separately. No MMIO, clocks,
 * data-cache/TLB maintenance or security-register access. */
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
 FN peaq_cache_read_sctlr
 mrc p15,0,r0,c1,c0,0
 bx lr
 FN peaq_cache_read_actlr
 mrc p15,0,r0,c1,c0,1
 bx lr
 FN peaq_cache_read_cpsr
 mrs r0,cpsr
 bx lr
 FN peaq_cache_read_midr
 mrc p15,0,r0,c0,c0,0
 bx lr
 FN peaq_cache_write_sctlr
 dsb sy
 mcr p15,0,r0,c1,c0,0
 isb sy
 bx lr
 FN peaq_cache_write_actlr
 dsb sy
 mcr p15,0,r0,c1,c0,1
 isb sy
 bx lr
 FN peaq_cache_maintenance
 dsb sy
 mov r0,#0
 mcr p15,0,r0,c7,c5,0
 mcr p15,0,r0,c7,c5,6
 dsb sy
 isb sy
 bx lr
