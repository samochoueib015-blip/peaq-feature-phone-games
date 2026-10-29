/* Included after menu_saved_impl.c, sharing only its owned cold SD service.
 * EPR1: words0..7 header,8..25 audio,32..55 counters,64..79 video,
 * word127 FNV-1a over bytes0..507. Remaining fields are zero/reserved.
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
 uint32_t *w=report_state.words;memset(w,0,512);
 w[0]=0x31525045u;w[1]=1;w[2]=512;w[3]=6;w[4]=report_state.sequence+1;w[5]=reason;
 w[6]=sys_timer_ms();w[7]=report_state.error;
 memcpy(w+8,menu_tone_status,sizeof(menu_tone_status));
 const PeaqFrameReport *d=peaq_cold_display_status();
 w[32]=intro.frames;w[33]=report_state.started_ms?w[6]-report_state.started_ms:0;
 w[34]=intro.step_ms;w[35]=cache.transition_ms;w[36]=cache.target_returned;
 w[37]=d->frames_done;w[38]=d->generation_ms;w[39]=d->transfer_ms;
 w[40]=saved.save_ms;w[41]=saved.flushes;w[42]=saved.commits;
 w[43]=sd.reads;w[44]=peaq_sd_save_status()->writes;w[45]=runtime.generation;
 w[46]=runtime.dirty;w[47]=runtime.quiet_frames;w[48]=interactive.error;
 w[49]=intro.error;w[50]=cache.error;w[51]=saved.error;w[52]=sd.error;
 w[53]=sd.clock_control;w[54]=report_state.map.sector;w[55]=volume.level;
 const PeaqVideoStats *v=video_stats();if(v)memcpy(w+64,v,sizeof(*v));
 w[127]=save_hash(w,508);
}
static int menu_report_commit(void){
 if(!report_state.words[0])return 1;
 if(!saved_cold()||!saved.armed||!menu_report_guards()||report_state.map.valid!=1||
    report_state.sequence>=64||saved_storage.store.poisoned||sd.error||sd.poisoned)return 0;
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
 unsigned n=phase==1?reads*100u/580u:phase==2?writes*100u/257u:(reads>580?reads-580:0)*100u/290u;
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
