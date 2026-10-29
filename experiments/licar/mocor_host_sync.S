.syntax unified
.thumb
.section .text.peaq_licar_mocor_sync_code,"ax"
.global peaq_licar_mocor_sync_code
.type peaq_licar_mocor_sync_code,%function
.thumb_func
peaq_licar_mocor_sync_code:
    cbz r1,2f
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
2:  bx lr
.size peaq_licar_mocor_sync_code,.-peaq_licar_mocor_sync_code
