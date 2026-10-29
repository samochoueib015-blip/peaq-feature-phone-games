.syntax unified
.cpu cortex-a7
.thumb
.section .text.peaq_launcher_anchor_control,"ax",%progbits
.global peaq_launcher_anchor_control
.type peaq_launcher_anchor_control,%function
.thumb_func
peaq_launcher_anchor_control:
 mrs r0,cpsr
 bx lr
.size peaq_launcher_anchor_control,.-peaq_launcher_anchor_control
