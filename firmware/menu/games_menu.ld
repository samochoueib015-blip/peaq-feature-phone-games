ENTRY(games_open)
SECTIONS {
  .patch.item 0x88050434 : { *(.patch.item) }
  .patch.image 0x8809ba68 : { *(.patch.image) }
  .games 0x88e46000 : { *(.text*) *(.rodata*) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
create_window = 0x883321e5;
close_window = 0x8809aa3f;
set_calc_style = 0x880618ed;
draw_image = 0x8809ba69;
draw_text = 0x881e4e9b;
draw_hline = 0x8870a287;
item_resume = 0x88050439;
image_resume = 0x8809ba6d;
ASSERT(SIZEOF(.patch.item)==4,"Item hook must replace only the prologue")
ASSERT(SIZEOF(.patch.image)==4,"Image hook must replace only the prologue")
ASSERT(SIZEOF(.games)<0xe00,"Payload and table must remain in one verified User page")
