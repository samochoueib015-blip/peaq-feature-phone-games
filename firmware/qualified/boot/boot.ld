ENTRY(_start)
SECTIONS {
 .text 0x80100000 : { __image_start = .; KEEP(*(.text._start)) *(.text*) *(.rodata*) . = ALIGN(4); __image_end = .; }
 __image_size = __image_end - __image_start;
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(64); __bss_end = .; }
 ASSERT(__bss_end<=0x80110000,"bootstrap private memory exceeded")
 .file_state 0x80400000 (NOLOAD) : { KEEP(*(.file_state)) }
 ASSERT(.<=0x8040d000,"file state overflow")
 .save_state 0x8040d000 (NOLOAD) : { KEEP(*(.save_state)) }
 ASSERT(.<=0x80410000,"audit state overflow")
 .unexpected : { *(.data*) }
 ASSERT(SIZEOF(.unexpected)==0,"unexpected data")
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
