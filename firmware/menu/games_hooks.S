.syntax unified
.thumb
.section .text.hooks,"ax"
.global games_item_hook
.thumb_func
games_item_hook:
    movw r12,#0x5047
    cmp r0,r12
    bne 1f
    cmp r1,#0
    beq games_item
1:  push.w {r3,r4,r5,r6,r7,r8,r9,r10,r11,lr}
    b.w item_resume

.global games_image_hook
.thumb_func
games_image_hook:
    ldr r12,[sp]
    push {r3,lr}
    ldr r3,=0x4f504751
    cmp r12,r3
    pop {r3,lr}
    beq games_icon
    push.w {r4,r5,r6,r7,r8,r9,r10,lr}
    b.w image_resume

.section .patch.item,"ax"
    b.w games_item_hook
.section .patch.image,"ax"
    b.w games_image_hook
