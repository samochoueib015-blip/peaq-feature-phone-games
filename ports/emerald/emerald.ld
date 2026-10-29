ENTRY(peaq_emerald_init)
SECTIONS {
 .image 0 : { __peaq_image_start = .; *(.text*) *(.rodata*) *(.data*) *(script_data) *(song_data) *(.got*) __peaq_image_end = .; }
 . = ALIGN(32);
 .bss (NOLOAD) : {
  peaq_emerald_ewram = .; *(ewram_data)
  ASSERT(. - peaq_emerald_ewram <= 0x40000, "EWRAM overflow");
  . = peaq_emerald_ewram + 0x40000;
  peaq_emerald_iwram = .;
  *libpokeemerald-cooperative.a:*(.bss .bss.* .bss.code)
  *(common_data) *(iwram_data) *libpokeemerald-cooperative.a:*(COMMON)
  ASSERT(. - peaq_emerald_iwram <= 0x7ff0, "IWRAM overflow into BIOS words");
  . = peaq_emerald_iwram + 0x8000;
  peaq_emerald_vram = .; . += 0x18000;
  peaq_emerald_palette = .; . += 0x400;
  peaq_emerald_oam = .; . += 0x400;
  peaq_emerald_registers = .; . += 0x400;
  peaq_emerald_save = .; . += 0x20000;
  *(.bss*) *(COMMON)
 }
 .rel.dyn : { *(.rel.dyn) } .dynamic : { *(.dynamic) }
 .dynsym : { *(.dynsym) } .dynstr : { *(.dynstr) }
 .hash : { *(.hash) } .gnu.hash : { *(.gnu.hash) }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
