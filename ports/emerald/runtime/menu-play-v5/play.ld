ENTRY(_start)
SECTIONS {
 .text 0x80100000 : { __image_start = .; KEEP(*(.text._start)) *(.text*) *(.rodata*)
  /* Exact existing transport tables: descriptors are adjusted for USB64.
   * Do not allow any other initialized mutable state into the image. */
  *(.data.dl_usb_channel) *(.data.config_desc)
  . = ALIGN(4096); __image_end = .; __cache_text_end = .; }
 __image_size = __image_end - __image_start;
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(64); __bss_end = .; }
 ASSERT(__bss_end <= 0x80110000, "private64KiB exceeded")
 .file_state 0x80400000 (NOLOAD) : { KEEP(*(.file_state)) }
 ASSERT(. <= 0x8040d000, "FAT reservation exceeded")
 .save_state 0x8040d000 (NOLOAD) : { KEEP(*(.save_state)) }
 ASSERT(. <= 0x80410000, "save reservation exceeded")
 .runtime_arena 0x80410000 (NOLOAD) : { KEEP(*(.runtime_arena)) }
 ASSERT(. <= 0x810f0000, "cacheable arena exceeded")
 .audio_state 0x81140000 (NOLOAD) : { __audio_start = .; KEEP(*(.audio_state)) __audio_end = .; }
 ASSERT(. <= 0x81148000,"audio reservation exceeded")
 .cache_tables 0x81200000 (NOLOAD) : { KEEP(*(.cache_tables)) }
 ASSERT(SIZEOF(.cache_tables)==32768, "cache table reservation")
 .loading 0x813b0000 (NOLOAD) : { KEEP(*(.loading)) }
 ASSERT(SIZEOF(.loading)==153728,"loading canvas changed")
 ASSERT(. <= 0x813d8000,"loading and DMA overlap")
 .framebuffer 0x813d8000 (NOLOAD) : { KEEP(*(.framebuffer)) }
 ASSERT(SIZEOF(.framebuffer) == 153728, "display reservation changed")
 .unexpected : { *(.data*) }
 ASSERT(SIZEOF(.unexpected)==0, "initialized mutable data unexpected")
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
