 .arch armv7-a
 .syntax unified
 .arm
 .section .bridge,"ax",%progbits
 .global menu_kernel_bridge
menu_kernel_bridge:
 push {r0-r12,lr}
 mrs r12,cpsr
 mrc p15,0,r0,c1,c0,0
 push {r0,r12}
 ldr r1,=0x1005
 tst r0,r1
 bne fast_original
 bic r1,r0,#0x3800
 bic r1,r1,#2
 ldr r2,=0x00c50078
 cmp r1,r2
 bne fast_original
 mrc p15,0,r1,c1,c0,1
 ldr r2,=0x6040
 cmp r1,r2
 bne fast_original
 mrc p15,0,r1,c0,c0,0
 ldr r2,=0x410fc075
 cmp r1,r2
 bne fast_original
 and r1,r12,#0xff
 cmp r1,#0xd3
 bne fast_original
 ldr r0,=1904282498
 bl peaq_request_find
 cmp r0,#0
 beq fast_original
 /* The checksum-validated request is one-shot even if SD init hangs.
  * MMU/D-cache were checked off before this write to the retained marker. */
 mov r1,#0
 str r1,[r0]
 dsb sy
 ldr r0,=bootstrap
 ldr r1,=0x80100000
 ldr r2,=10588
copy_boot:
 ldr r3,[r0],#4
 str r3,[r1],#4
 subs r2,r2,#4
 bne copy_boot
 dsb sy
 mov r0,#0
 mcr p15,0,r0,c7,c5,0
 ldr r0,=0x00c52878
 mcr p15,0,r0,c1,c0,0
 isb sy
 ldr r0,=0x80100200
 blx r0
 cmp r0,#0
 beq original
 cmp r0,#0xf000
 bhi original
 tst r0,#3
 bne original
 /* Request was consumed and card stopped. Source and destination are
  * below this bridge and the original kernel. There is no path back now. */
 mov r2,r0
 ldr r0,=0x80300000
 ldr r1,=0x80100000
copy_game:
 ldr r3,[r0],#4
 str r3,[r1],#4
 subs r2,r2,#4
 bne copy_game
 dsb sy
 mov r0,#0
 mcr p15,0,r0,c7,c5,0
 isb sy
 ldr pc,=0x80100200
original:
 pop {r0,r12}
 mcr p15,0,r0,c1,c0,0
 isb sy
 b restore_flags
fast_original:
 pop {r0,r12}
restore_flags:
 msr cpsr_fsxc,r12
 pop {r0-r12,lr}
 ldr pc,=0x80a06230
 .ltorg
 .balign 4
bootstrap:
 .incbin "@BOOT_FILE@"
