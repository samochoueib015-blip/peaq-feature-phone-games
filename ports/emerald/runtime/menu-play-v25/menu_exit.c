#include "menu_exit.h"
#include "menu_video_v25.h"
void peaq_menu_exit_begin(PeaqMenuExit *s){*s=(PeaqMenuExit){1,0,0,0,0,1};}
int peaq_menu_exit_poll(PeaqMenuExit *s,const PeaqKeyFrame *f){
 if(!s||!f||s->phase<1||s->phase>4||s->page>2||
    s->selected>(s->page==MENU_PAUSE_SETTINGS?3u:s->page==MENU_PAUSE_QUIT?1u:2u))return -1;
 ++s->polls;
 if(s->phase!=2){
  if(f->gba||f->host)return PEAQ_MENU_STAY;
  if(s->phase==3){s->phase=0;return PEAQ_MENU_RESUME;}
  if(s->phase==4){s->phase=0;return PEAQ_MENU_EXIT;}
  s->phase=2;return PEAQ_MENU_STAY;
 }
 unsigned cancel=(f->host_pressed&PEAQ_KEY_HOST_BACK)||(f->gba_pressed&2u);
 unsigned direction=f->gba_pressed&0xf0u;
 unsigned confirm=(f->gba_pressed&1u)||(f->host_pressed&PEAQ_KEY_HOST_OPTIONS);
 if(cancel){
  if(s->page){s->selected=s->page==MENU_PAUSE_SETTINGS?1:2;s->page=0;s->phase=1;}
  else s->phase=3;
 }else if(direction){
  /* Opposing/chord directions cannot accidentally select or confirm Quit. */
  if(direction&(direction-1u))return PEAQ_MENU_STAY;
  if(s->page==MENU_PAUSE_SETTINGS&&s->selected<3&&(direction==16||direction==32))
   peaq_menu_video_set(peaq_menu_video_flags()^(1u<<s->selected));
  else {
   unsigned count=s->page==MENU_PAUSE_SETTINGS?4:s->page==MENU_PAUSE_QUIT?2:3;
   s->selected=(s->selected+(direction==16||direction==128?1:count-1))%count;
  }
 }else if(confirm){
  if(s->page==MENU_PAUSE_SETTINGS){
   if(s->selected<3)peaq_menu_video_set(peaq_menu_video_flags()^(1u<<s->selected));
   else {s->page=0;s->selected=1;}
   s->phase=1;
  }else if(s->page==MENU_PAUSE_QUIT){
   if(s->selected){s->confirmed=1;s->phase=4;}
   else {s->page=0;s->selected=0;s->phase=1;}
  }else if(s->selected==0)s->phase=3;
  else {s->page=s->selected==1?MENU_PAUSE_SETTINGS:MENU_PAUSE_QUIT;s->selected=0;s->phase=1;}
 }else return PEAQ_MENU_STAY;
 ++s->revision;return PEAQ_MENU_STAY;
}
