/* Separate cold entry. No cache maintenance while inherited SMP is zero.
 * All three M/C/I bits must already be zero; never turns off a live OS. */
 .arch armv7-a
 .syntax unified
 .section .text._start,"ax",%progbits
 .p2align 2
 .code 32
 .global _start,cold_entry,cold_halt,peaq_cache_halt
_start:
 .ascii "DHTB"
 .long 1
 .org _start + 0x30,0
 .long __image_size - 0x200
 .org _start + 0x200,0
cold_entry:
 adr r2,_start
 ldr r3,=0x80100000
 cmp r2,r3
 bne cold_halt
 mrc p15,0,r0,c1,c0,0
 ldr r2,=0x1005
 tst r0,r2
 bne cold_halt
 mrs r1,cpsr
 msr CPSR_c,#0xdf
 ldr sp,=0x80100000
 dsb sy
 isb sy
 ldr r2,=peaq_cold_main
 bx r2
 .type cold_halt,%function
 .type peaq_cache_halt,%function
cold_halt:
peaq_cache_halt:
 b cold_halt
 .global clean_dcache_range,invalidate_dcache_range
 .type clean_dcache_range,%function
 .type invalidate_dcache_range,%function
clean_dcache_range:
invalidate_dcache_range:
 dsb sy
 bx lr
