ENTRY(pga_event)
SECTIONS {
  .image 0 : { *(.text*) *(.rodata*) *(.data*) *(.got*) }
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  .rel.dyn : { *(.rel.dyn) }
  .dynamic : { *(.dynamic) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .hash : { *(.hash) }
  .gnu.hash : { *(.gnu.hash) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
