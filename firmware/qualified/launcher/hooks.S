.syntax unified
.thumb
.section .patch.window,"ax"
    b.w sd_window
.section .text.sync_code,"ax"
.global sync_code
.thumb_func
sync_code:
    add r1,r0,r1
    bic r0,r0,#31
1:  mcr p15,0,r0,c7,c11,1
    add r0,r0,#32
    cmp r0,r1
    blo 1b
    dsb sy
    movs r0,#0
    mcr p15,0,r0,c7,c5,0
    mcr p15,0,r0,c7,c5,6
    dsb sy
    isb sy
    bx lr

/* Explicit interworking: the firmware veneers at these even addresses are
 * ARM instructions. B.W from Thumb must target this Thumb bridge, never the
 * ARM veneer directly. BX IP clears T; the original LDR PC veneer then enters
 * its Thumb kernel target. LR and all four argument registers are preserved.
 */
.section .text.os_interwork,"ax"
.global os_alloc
.type os_alloc,%function
.thumb_func
os_alloc:
    ldr.w ip,=0x88090710
    bx ip
.size os_alloc,.-os_alloc
.global os_free
.type os_free,%function
.thumb_func
os_free:
    ldr.w ip,=0x88090850
    bx ip
.size os_free,.-os_free
.ltorg
