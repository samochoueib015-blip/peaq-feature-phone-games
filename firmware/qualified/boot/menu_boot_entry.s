.arch armv7-a
.syntax unified
.section .text._start,"ax",%progbits
.arm
.global _start,menu_boot_entry
_start:
 .ascii "DHTB"
 .long 1
 .org _start+0x30,0
 .long __image_size-0x200
 .org _start+0x200,0
menu_boot_entry:
 /* Caller is the pinned early-kernel trampoline, with caches/MMU already
  * disabled by original boot1. Remember its banked SVC stack and LR. */
 mov r4,sp
 mov r5,lr
 mrs r1,cpsr
 mov r6,r1
 mrc p15,0,r0,c1,c0,0
 msr cpsr_c,#0xdf
 ldr sp,=0x80100000
 push {r4,r5,r6,lr}
 bl peaq_menu_boot
 pop {r4,r5,r6,lr}
 msr cpsr_c,r6
 mov sp,r4
 bx r5
