.syntax unified
.arch armv7-a
.thumb
.section .patch.fat,"ax",%progbits
bl peaq_fat_mirror_write
