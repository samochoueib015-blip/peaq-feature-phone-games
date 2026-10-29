/* Included after menu_saved_impl.c, sharing only its owned cold SD service.
 * EPR1: words0..7 header,8..25 audio,32..55 counters,64..79 video,
 * v7 adds stop outcome26..31, extra registers56..63/74..79/119..126 and the
 * complete pre-stop audio report80..118. Word127 hashes bytes0..507.
 */
#define REPORT_GUARD 0xe6ad71c3u
static void menu_report_init(void){
 memset(&report_state,0,sizeof(report_state));
 for(unsigned i=0;i<16;++i)report_state.before[i]=report_state.after[i]=REPORT_GUARD;
}
static int menu_report_guards(void){
 for(unsigned i=0;i<16;++i)if(report_state.before[i]!=REPORT_GUARD||report_state.after[i]!=REPORT_GUARD)return 0;
 return 1;
}
static void menu_report_capture(unsigned reason){
 if(!saved_cold()||!menu_report_guards())return;
 if(report_state.sequence>=64){report_state.words[0]=0;return;}
 const PeaqColdAudioReport *a=peaq_cold_audio_status();
 menu_tone_status[0]=a->volume;menu_tone_status[1]=a->pcm_peak;
 menu_tone_status[2]=a->submitted;menu_tone_status[3]=a->completed;menu_tone_status[4]=a->error;
 for(unsigned i=5;i<18;++i)menu_tone_status[i]=0xffffffffu;
 unsigned route_ok=peaq_cold_audio_route(menu_tone_status+5);
 uint32_t *w=report_state.words;memset(w,0,512);
 w[0]=0x31525045u;w[1]=1;w[2]=512;w[3]=16;w[4]=report_state.sequence+1;w[5]=reason;
 w[6]=sys_timer_ms();w[7]=report_state.error;
 memcpy(w+8,menu_tone_status,sizeof(menu_tone_status));
 _Static_assert(sizeof(*a)==39u*4u,"audio report layout");
 memcpy(w+32,a,sizeof(*a));
 w[26]=route_ok|(peaq_cold_audio_delivery(w+71)<<1);
 w[81]=intro.frames;w[82]=logical_ms;w[83]=interactive.start_ms;
 w[84]=interactive.start_ms?w[6]-interactive.start_ms:0;
 w[85]=intro.step_ms;w[86]=intro.present_count;
 memcpy(w+87,&schedule,sizeof(schedule));
 w[99]=interactive.error;w[100]=runtime.last_error;w[101]=cache.cpu_faults;
 w[102]=peaq_fault.exception_kind;w[103]=peaq_fault.fault_pc;w[104]=peaq_fault.fault_pc_valid;
 w[105]=peaq_fault.dfsr;w[106]=peaq_fault.dfar;w[107]=peaq_fault.ifsr;w[108]=peaq_fault.ifar;
 w[109]=peaq_fault.fault_lr;w[110]=peaq_fault.interrupted_sp;
 w[111]=peaq_fault.fault_cpsr;w[112]=peaq_fault.fault_spsr;w[113]=peaq_fault.breadcrumb;
 w[114]=peaq_fault.armed_sctlr;w[115]=peaq_fault.control_restored;
 w[116]=saved_storage.store.writes;w[117]=saved.commits;w[118]=report_state.map.sector;
 w[119]=sd.error;w[120]=saved.error;
 if(runtime.active)memcpy(w+121,runtime.module.base+PEAQ_ENGINE_AUDIO_CLOCK_RVA,24);
 /* Audio routing is settled. Preserve first malformed music-stream context
  * instead of repeatedly asking the owner for register transcriptions. */
 if(runtime.active)memcpy(w+13,runtime.module.base+PEAQ_ENGINE_AUDIO_GUARD_RVA,32);
 peaq_menu_video_record(w);
 w[26]&=~1u;
 w[127]=save_hash(w,508);
}
static void menu_report_stopped(unsigned ok){
 if(!saved_cold()||!menu_report_guards()||!report_state.words[0]||
    report_state.words[4]!=report_state.sequence+1)return;
 const PeaqColdAudioReport *a=peaq_cold_audio_status();uint32_t *w=report_state.words;
 w[27]=ok?1:2;w[28]=a->error;w[29]=a->poisoned;w[30]=a->stop_complete;w[31]=a->owned_mask;
 w[127]=save_hash(w,508);
}
static int menu_report_commit(void){
 if(!report_state.words[0])return 1;
 if(!saved_cold()||!saved.armed||!menu_report_guards()||report_state.map.valid!=1||
    report_state.sequence>=64||saved_storage.store.poisoned||sd.error||sd.poisoned)return 0;
 const PeaqColdAudioReport *a=peaq_cold_audio_status();
 /* A timer fault may still have completed the safe teardown. A failed pause
  * must retain ownership and MUST NOT reach SD writing or buffer reuse. */
 if(!a->stopped||a->running||a->owned_mask||a->reserved_mask||a->vbc_owned||a->powered)return 0;
 if((MEM4(0x2030002cu)&0xffc7u)!=0x4007u)return 0;
 if(!peaq_sd_save_write(&sd,report_state.map.sector,(const uint8_t *)report_state.words)||
    !peaq_sd_save_sync(&sd)||!peaq_sd_read(&sd,report_state.map.sector,(uint8_t *)report_state.verify)){
  report_state.error=1;return 0;
 }
 for(unsigned i=0;i<128;++i)if(report_state.words[i]!=report_state.verify[i]){report_state.error=2;return 0;}
 ++report_state.sequence;return menu_report_guards();
}
static int menu_save_progress(unsigned writing){
 if(!report_state.save_active)return 1;
 uint32_t reads=saved_storage.store.reads-report_state.save_reads;
 uint32_t writes=saved_storage.store.writes-report_state.save_writes;
 unsigned phase=(writing||(writes&&saved_storage.store.writing))?2:writes?3:1;
 unsigned n=phase==1?reads*100u/514u:phase==2?writes*100u/257u:(reads>515?reads-515:0)*100u/257u;
 if(n>100)n=100;n=n/10*10;
 if(report_state.save_phase==phase&&report_state.save_percent==n)return 1;
 report_state.save_phase=phase;report_state.save_percent=n;
 return peaq_menu_saving(phase,n);
}
static int menu_save_begin(void){
 if(!saved_cold()||!menu_report_guards())return 0;
 report_state.save_reads=saved_storage.store.reads;report_state.save_writes=saved_storage.store.writes;
 report_state.save_active=1;report_state.save_phase=report_state.save_percent=0;
 peaq_menu_volume_clear();return menu_save_progress(0);
}
static int menu_save_end(unsigned force){
 if(!report_state.save_active)return 0;
 report_state.save_active=0;
 if(!peaq_menu_saving(3,100))return 0;
 if(!force&&interactive.active&&peaq_cold_display_present(runtime.portrait,240,320)!=1)return 0;
 uint32_t now=sys_timer_ms();schedule.due_ms=schedule.last_ms=now;
 schedule.remainder=schedule.skipped_streak=0;++schedule.resyncs;return 1;
}

static int menu_preferences_commit(void){
 if(!report_state.words[0]||report_state.words[4]!=report_state.sequence+1)return 1;
 peaq_menu_video_record(report_state.words);
 report_state.words[127]=save_hash(report_state.words,508);
 return menu_report_commit();
}
