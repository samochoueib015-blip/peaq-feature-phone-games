/* Owned checkpoint for the serialized Cortex-A7 soft-float engine worker.
 * Only AAPCS callee-saved integer registers and the return stack are restored.
 * Caller-saved registers/flags are intentionally unspecified, as at a call.
 */
#if defined(__ARM_PCS_VFP)
#error The DOOM context boundary requires the soft-float ABI
#endif
.syntax unified
.arch armv7-a
.thumb
.section .text.peaq_doom_save_context,"ax",%progbits
.balign 4
.global peaq_doom_save_context
.type peaq_doom_save_context,%function
.thumb_func
peaq_doom_save_context:
    stmia r0!, {r4-r11}
    mov r2, sp
    mov r3, lr
    stmia r0, {r2,r3}
    movs r0, #0
    bx lr
.size peaq_doom_save_context, .-peaq_doom_save_context

.section .text.peaq_doom_restore_context,"ax",%progbits
.balign 4
.global peaq_doom_restore_context
.type peaq_doom_restore_context,%function
.thumb_func
peaq_doom_restore_context:
    ldmia r0!, {r4-r11}
    ldmia r0, {r2,r3}
    mov sp, r2
    mov lr, r3
    movs r0, #1
    bx lr
.size peaq_doom_restore_context, .-peaq_doom_restore_context
.section .note.GNU-stack,"",%progbits
