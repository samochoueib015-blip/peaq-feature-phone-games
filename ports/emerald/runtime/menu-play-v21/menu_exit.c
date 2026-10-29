#include "menu_exit.h"
void peaq_menu_exit_begin(PeaqMenuExit *s){s->phase=1;s->selected=0;s->polls=0;s->confirmed=0;}
int peaq_menu_exit_poll(PeaqMenuExit *s,const PeaqKeyFrame *f){
 if(!s||!f||s->phase<1||s->phase>4||s->selected>1)return -1;
 ++s->polls;
 /* The opening key and any simultaneous held action must be released.
  * Likewise, never deliver the confirmation/cancellation to the game. */
 if(s->phase==1||s->phase==3||s->phase==4){
  if(f->gba||f->host)return PEAQ_MENU_STAY;
  if(s->phase==3){s->phase=0;return PEAQ_MENU_RESUME;}
  if(s->phase==4){s->phase=0;return PEAQ_MENU_EXIT;}
  s->phase=2;return PEAQ_MENU_STAY;
 }
 if((f->host_pressed&PEAQ_KEY_HOST_BACK)||(f->gba_pressed&2u)){
  s->phase=3;return PEAQ_MENU_STAY;
 }
 uint32_t direction=f->gba_pressed&0xf0u;
 if(direction){
  /* An ambiguous chord never selects Quit; changing selection consumes
   * this edge, so Down+A together cannot also confirm the change. */
  s->selected=(direction==128u||direction==16u)?1u:0u;
  return PEAQ_MENU_STAY;
 }
 if((f->gba_pressed&1u)||(f->host_pressed&PEAQ_KEY_HOST_OPTIONS)){
  /* In particular release A/8 before resetting: holding 8 through a boot
   * is the handset's download-mode chord. */
  if(s->selected){s->confirmed=1;s->phase=4;return PEAQ_MENU_STAY;}
  s->phase=3;
 }
 return PEAQ_MENU_STAY;
}
