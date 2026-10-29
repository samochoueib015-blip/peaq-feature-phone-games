/* Standalone game service entered by the early-kernel SD bootstrap. All
 * engine/codec/input/SD state is owned here; no former OS service is called.
 */
#include "cold_audio.h"
#include "cold_reset.h"
#include "menu_loading.h"
#include "menu_exit.h"
static unsigned menu_audio_ready;
static unsigned menu_pause_pending;
static PeaqMenuExit menu_exit;
static uint32_t menu_tone_status[18];
static void menu_request_exit(void){menu_pause_pending=1;}
static int menu_audio(void *,const int8_t *,uint32_t,uint32_t);
static int menu_audio_begin(void),menu_audio_end(unsigned);
#include "menu_report.h"
static PeaqMenuReportState report_state __attribute__((section(".diagnostics"),aligned(64)));
static int menu_report_guards(void),menu_save_progress(unsigned),menu_save_begin(void),menu_save_end(unsigned);
#include "menu_saved_impl.c"
#include "menu_report_impl.c"
extern uint8_t __audio_start[],__audio_end[];

static int menu_audio(void *cookie,const int8_t *p,uint32_t n,uint32_t rate){
 if(cookie!=&storage.fs)return 0;
 return !menu_audio_ready||peaq_cold_audio_push(p,n,rate);
}
static int menu_audio_begin(void){
 if(menu_audio_ready)return 1;
 if(!peaq_cold_audio_start(volume.level))return 0;
 menu_audio_ready=1;return 1;
}
static int menu_audio_end(unsigned drain){
 if(!menu_audio_ready)return 1;
 int ok=peaq_cold_audio_stop(drain);menu_audio_ready=0;return ok;
}
static void menu_confirm_exit(void){
 if(!menu_pause_pending||interactive.stop_requested||in_engine)return;
 usb_gate();peaq_menu_volume_clear();peaq_menu_exit_begin(&menu_exit);
 const PeaqColdAudioReport *a=peaq_cold_audio_status();
 menu_tone_status[0]=a->volume;menu_tone_status[1]=a->pcm_peak;
 menu_tone_status[2]=a->submitted;menu_tone_status[3]=a->completed;menu_tone_status[4]=a->error;
 for(unsigned i=5;i<18;++i)menu_tone_status[i]=0xffffffffu;
 (void)peaq_cold_audio_route(menu_tone_status+5);
 menu_report_capture(1);
 if(!peaq_menu_exit_screen(0,menu_tone_status)||!menu_audio_end(1)){
  interactive_error(INTERACTIVE_ENGINE,INTERACTIVE_STOP_ENGINE);return;
 }
 if(!menu_report_commit()){interactive_error(INTERACTIVE_SAVE,INTERACTIVE_STOP_SAVE);return;}
 uint32_t last=sys_timer_ms();unsigned stalled=0;
 while(!interactive.stop_requested){
  uint32_t now=sys_timer_ms();service_input(now,0);
  if(!peaq_interactive_keys_take(&key_frame)){
   interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);break;
  }
  unsigned previous=menu_exit.selected;
  int choice=peaq_menu_exit_poll(&menu_exit,&key_frame);
  if(choice<0){interactive_error(INTERACTIVE_KEY_READ,INTERACTIVE_STOP_INPUT);break;}
  if(choice==PEAQ_MENU_EXIT){request_stop(INTERACTIVE_STOP_BACK);break;}
  if(choice==PEAQ_MENU_RESUME){
   if(!menu_audio_begin()||peaq_cold_display_present(runtime.portrait,240,320)!=1){
    interactive_error(INTERACTIVE_ENGINE,INTERACTIVE_STOP_ENGINE);break;
   }
   now=sys_timer_ms();schedule.due_ms=schedule.last_ms=now;
   schedule.remainder=schedule.skipped_streak=0;++schedule.resyncs;
   menu_pause_pending=0;idle_since=last_clock=last_service_ms=now;return;
  }
  if(previous!=menu_exit.selected&&!peaq_menu_exit_screen(menu_exit.selected,menu_tone_status)){
   interactive_error(INTERACTIVE_ENGINE,INTERACTIVE_STOP_ENGINE);break;
  }
  if(now==last){if(++stalled>=1000000u){interactive_error(INTERACTIVE_TIMER,INTERACTIVE_STOP_TIMER);break;}}
  else stalled=0;last=now;
 }
}
void peaq_cold_main(uint32_t sctlr,uint32_t cpsr){
 unsigned failure=1;
 menu_report_init();menu_init_state(sctlr,cpsr);
 /* NOLOAD memory survives some resets; initialize our new lifetime once.
  * Channel ownership is still checked independently by the hardware driver. */
 memset(__audio_start,0,(size_t)(__audio_end-__audio_start));
 interactive.flags&=~INTERACTIVE_F_NO_AUDIO;
 interactive.host_idle_limit_ms=0;interactive.owner_release_instruction_required=0;
 /* Everything after this point belongs to a consumed one-shot request.
  * Returning to overwritten kernel code is forbidden; all exits reboot. */
 if(!initialize())goto leave;
 failure=2;if(!peaq_menu_loading(MENU_LOAD_CHECK,0)||!check_speed())goto leave;
 failure=3;if(!start())goto leave;
 interactive.stage=INTERACTIVE_LOADED;
 failure=4;if(!peaq_menu_loading(MENU_LOAD_KEYS,0))goto leave;
 if(!peaq_interactive_keys_init(intro.entered_sctlr,intro.entered_cpsr,sys_timer_ms()))goto leave;
 interactive.stage=INTERACTIVE_KEYS_READY;
 quiet_window();if(interactive.stage!=INTERACTIVE_QUIET_READY)goto leave;
 failure=5;if(!peaq_menu_loading(MENU_LOAD_CACHE,0))goto leave;
 /* Boot1 has SMP enabled already. The RAM/FDL entry is also supported for
  * offline qualification and retains its existing guarded SMP transition. */
 if(cache.entered_actlr==0x6040u){cache.flags|=CACHE_F_SMP;cache.stage=CACHE_SMP_ON;}
 else smp_enable();
 if(cache.error||!(cache.flags&CACHE_F_SMP)||!cached_prepare())goto leave;
 cache.flags|=CACHE_F_RELEASED;cache.release_count=1;cache.stage=CACHE_RELEASED;
 interactive.stage=INTERACTIVE_RELEASED;
 failure=6;if(!peaq_menu_loading(MENU_LOAD_AUDIO,0)||!menu_audio_begin())goto leave;
 failure=7;if(!peaq_menu_loading(MENU_LOAD_READY,100))goto leave;
 failure=0;
 report_state.started_ms=sys_timer_ms();go();
 while(interactive.active&&!interactive.stopped){step();menu_confirm_exit();finish_stop();}
leave:
 peaq_menu_volume_clear();
 /* Keep every buffer allocated, including when a device refuses to stop.
  * A full hardware reset, rather than an unsafe OS return, releases them. */
 if(menu_audio_ready)(void)menu_audio_end(!interactive.error&&!intro.error);
 if(runtime.active&&!interactive.error&&!intro.error&&!cache.error)(void)saved_flush(1);
 if(failure)peaq_menu_loading_error(failure);
 else if(interactive.error||intro.error||cache.error||saved.error||peaq_cold_audio_status()->error)
  peaq_menu_runtime_error(peaq_cold_audio_status()->error,interactive.error?interactive.error:30);
 (void)peaq_cold_reset();
 for(;;)__asm__ volatile("wfi");
}
