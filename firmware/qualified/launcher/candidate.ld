ENTRY(sd_window)
SECTIONS {
 .patch.fat 0x88049fec : { *(.patch.fat) }
 .durability 0x88f9c000 : { *fat_mirror.o(.text* .rodata*) }
  .patch.window 0x88e46000 : { *(.patch.window) }
  .resident.glue 0x88fb7e80 : { *(.text.candidate_detach) *(.text.candidate_allocate) *(.text.candidate_free) *(.text.candidate_release) *(.text.candidate_finish_dispatch) *(.text.candidate_reap_list) *(.text.stop_timer) }
  .loader 0x88fbe000 : { *(.text.__aeabi_memcpy8) *(.text.__aeabi_memcpy) *(.text.sd_window) *(.text*) *(.rodata*) }
  .unexpected : { *(.data*) *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
get_state = 0x88333509;
set_state = 0x883334f1;
close_window = 0x8809aa3f;
send_message = 0x8809c4d5;
file_open = 0x886fbd71;
file_read = 0x886fc509;
file_write = 0x886fc563;
file_close = peaq_checked_file_close;
find_first = 0x886fc8a3;
find_next = 0x886fc9d5;
find_close = 0x886fca1b;
timer_create = 0x881f0289;
timer_stop = 0x881f02c7;
set_calc_style = 0x880618ed;
draw_text = 0x881e4e9b;
draw_hline = 0x8870a287;
audio_find = 0x886c48a3;
audio_request = 0x8830fac3;
audio_free = 0x8830fb6d;
audio_route = 0x886c4719;
audio_volume = 0x886c4681;
audio_play = 0x886c43c9;
ASSERT(SIZEOF(.patch.window)==4,"Only the existing Games handler entry may be hooked")
ASSERT(SIZEOF(.loader)<=0x2000,"Loader must fit its two audited User pages")
ASSERT(SIZEOF(.unexpected)==0,"No writable globals in firmware virtual memory")

peaq_launcher_anchor_control = 0x88fb7299;
peaq_launcher_anchor_name = 0x88fb7001;
peaq_launcher_anchor_owner = 0x88fb700b;
peaq_pga5s_close = 0x88fb793f;
peaq_pga5s_enter = 0x88fb78ff;
peaq_pga5s_event = 0x88fb7a55;
peaq_pga5s_leave = 0x88fb7aa7;
peaq_pga5s_native_allocate = 0x88fb7b0b;
peaq_pga5s_native_deallocate = 0x88fb7b21;
peaq_pga5s_prepare = 0x88fb74d5;
peaq_pga5s_start = 0x88fb7997;
peaq_pga5s_status = 0x88fb7ac7;
peaq_pga5s_validate = 0x88fb72a1;
peaq_resident_claim = 0x88fb7b39;
peaq_resident_close = 0x88fb7c87;
peaq_resident_enter = 0x88fb7bbb;
peaq_resident_leave = 0x88fb7c07;
peaq_resident_poll = 0x88fb7cd5;
peaq_resident_status = 0x88fb7db1;
ASSERT(SIZEOF(.resident.glue)<=0x180,"Resident glue must fit only the remaining audited page tail")

stock_file_flush = 0x886fc5b9;
stock_file_close = 0x886fc4d7;
stock_sector_read = 0x88044a75;
stock_sector_write = 0x88044a87;
ASSERT(SIZEOF(.durability)<=4096,"FAT helper page limit")
ASSERT(SIZEOF(.patch.fat)==4,"One FAT write call only")
