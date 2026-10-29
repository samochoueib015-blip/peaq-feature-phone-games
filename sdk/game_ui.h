#ifndef PEAQ_GAME_UI_H
#define PEAQ_GAME_UI_H
#include "gfx.h"

/* Original, single-voice melodies. Effects temporarily take the voice; the
 * melody resumes afterwards. No busy waits or writable module globals. */
typedef struct {u16 hz,ms;} Note;
typedef struct {u32 position,remaining,held,failed;} Sound;
enum { UI_TITLE,UI_HOME,UI_PLAY,UI_PAUSED,UI_SETTINGS,UI_TUTORIAL,UI_MODE,UI_RESULT,UI_CONFIRM };
enum { UI_NEW=1,UI_CONTINUE=2,UI_SAVE=4,UI_EXIT=8,UI_HANDLED=16 };
#define UI_PREFS 0x50470000u
typedef struct {
 u32 screen,selected,parent,page,age,flags,save_error;
 Sound sound;
} GameUi;
typedef struct {const char *title;const char *lines[9];} Lesson;
static inline int prefs_valid(u32 p){return (p&~3u)==UI_PREFS;}
static inline void sound_stop(const PgaApi *a,GameUi *u){
 a->silence(a->storage);u->sound.remaining=0;u->sound.held=0;
}
static inline void sound_note(const PgaApi *a,GameUi *u,u32 hz,u32 ms){
 if(hz)u->sound.failed=!a->tone(a->storage,hz,ms,4);
}
static inline void sound_effect(const PgaApi *a,GameUi *u,u32 hz,u32 ms){
 if(!(u->flags&2))return;sound_note(a,u,hz,ms);u->sound.held=ms+50;
}
static inline void sound_tick(const PgaApi *a,GameUi *u,const Note *song,u32 count,u32 dt){
 if(u->sound.held){u->sound.held=u->sound.held>dt?u->sound.held-dt:0;return;}
 if(!(u->flags&1))return;
 if(u->sound.remaining>dt){u->sound.remaining-=dt;return;}
 const Note *n=song+u->sound.position;u32 duration=n->ms*2;
 sound_note(a,u,n->hz,duration>40?duration-30:20);
 u->sound.remaining=duration;u->sound.position=(u->sound.position+1)%count;
}
static inline void ui_init(GameUi *u,u32 prefs,int restored){
 u->flags=prefs_valid(prefs)?prefs:UI_PREFS|3;u->screen=UI_TITLE;u->selected=restored?0:1;
}
static inline int ui_tick(GameUi *u,u32 dt){
 if(u->screen!=UI_TITLE)return 0;u->age+=dt;
 if(u->age<1400)return 0;u->screen=UI_HOME;return 1;
}
static inline int ui_up(u32 k){return k==PGA_UP||k==2;}
static inline int ui_down(u32 k){return k==PGA_DOWN||k==8;}
static inline int ui_ok(u32 k){return k==PGA_OK||k==5;}
static inline void ui_goto(GameUi *u,u32 screen){u->screen=screen;u->selected=0;}
static inline u32 ui_key(const PgaApi *a,GameUi *u,u32 k,u32 pages){
 u32 screen=u->screen;
 if(screen==UI_PLAY){
  if(k==PGA_BACK||k==PGA_HASH){ui_goto(u,UI_PAUSED);sound_stop(a,u);return UI_SAVE|UI_HANDLED;}
  return 0;
 }
 if(screen==UI_MODE)return 0;
 if(screen==UI_TITLE){
  if(k==PGA_BACK)return UI_EXIT|UI_HANDLED;
  if(ui_ok(k)){u->screen=UI_HOME;return UI_HANDLED;}
  return UI_HANDLED;
 }
 if(screen==UI_TUTORIAL){
  if(k==PGA_BACK){ui_goto(u,UI_SETTINGS);u->selected=2;}
  else if(k==PGA_LEFT||ui_up(k)){if(u->page)u->page--;}
  else if(k==PGA_RIGHT||ui_down(k)||ui_ok(k)){if(u->page+1<pages)u->page++;else {ui_goto(u,UI_SETTINGS);u->selected=2;}}
  return UI_HANDLED;
 }
 if(k==PGA_BACK){
  if(screen==UI_HOME)return UI_EXIT|UI_HANDLED;
  if(screen==UI_PAUSED){ui_goto(u,UI_PLAY);return UI_CONTINUE|UI_HANDLED;}
  if(screen==UI_SETTINGS){ui_goto(u,u->parent);u->selected=u->parent==UI_HOME?3:2;}
  else ui_goto(u,UI_HOME);
  return UI_HANDLED;
 }
 u32 count=screen==UI_HOME||screen==UI_SETTINGS?4:screen==UI_PAUSED?3:2;
 if(ui_up(k)){u->selected=(u->selected+count-1)%count;sound_effect(a,u,440,35);return UI_HANDLED;}
 if(ui_down(k)){u->selected=(u->selected+1)%count;sound_effect(a,u,440,35);return UI_HANDLED;}
 if(!ui_ok(k))return UI_HANDLED;
 sound_effect(a,u,660,65);
 if(screen==UI_HOME){
  if(u->selected==0){ui_goto(u,UI_PLAY);return UI_CONTINUE|UI_HANDLED;}
  if(u->selected==1){ui_goto(u,UI_CONFIRM);u->selected=1;return UI_HANDLED;}
  if(u->selected==2)return UI_EXIT|UI_HANDLED;
  u->parent=UI_HOME;ui_goto(u,UI_SETTINGS);
 }else if(screen==UI_PAUSED){
  if(u->selected==0){ui_goto(u,UI_PLAY);return UI_CONTINUE|UI_HANDLED;}
  if(u->selected==1)ui_goto(u,UI_HOME);
  else {u->parent=UI_PAUSED;ui_goto(u,UI_SETTINGS);}
 }else if(screen==UI_SETTINGS){
  if(u->selected<2){u->flags^=1u<<u->selected;sound_stop(a,u);return UI_SAVE|UI_HANDLED;}
  if(u->selected==2){ui_goto(u,UI_TUTORIAL);u->page=0;}
  else {ui_goto(u,u->parent);u->selected=u->parent==UI_HOME?3:2;}
 }else if(screen==UI_CONFIRM){
  if(u->selected==0)return UI_NEW|UI_HANDLED;
  ui_goto(u,UI_HOME);u->selected=1;
 }else if(screen==UI_RESULT){
  if(u->selected==0)return UI_NEW|UI_HANDLED;
  ui_goto(u,UI_HOME);
 }
 return UI_HANDLED;
}
static inline void center(const PgaApi *a,int y,int scale,u16 color,const char *s){
 int n=0;while(s[n])n++;gt(a,(240-(n*6-1)*scale)/2,y,scale,color,s);
}
static inline void ui_background(const PgaApi *a,u16 accent){
 a->fill(0,0,240,320,0x0842);
 for(int i=0;i<9;i++)a->fill(i*28,0,1,320,0x1083);
 for(int i=0;i<12;i++)a->fill(0,i*28,240,1,0x1083);
 a->fill(0,0,240,4,accent);a->fill(0,316,240,4,accent);
}
static inline void menu_item(const PgaApi *a,int y,const char *s,int selected,u16 accent){
 a->fill(14,y,212,30,selected?0x2251:0x1083);
 if(selected)a->fill(14,y,4,30,accent);
 gt(a,26,y+8,2,selected?0xffff:0xad55,s);
}
static inline void menu(const PgaApi *a,GameUi *u,const char *const *items,u32 n,int y,u16 accent){
 for(u32 i=0;i<n;i++)menu_item(a,y+(int)i*36,items[i],u->selected==i,accent);
}
static inline void ui_notice(const PgaApi *a,GameUi *u){
 if(u->save_error)center(a,290,1,0xfd45,"SD-SPEICHERFEHLER");
 else if(u->sound.failed)center(a,290,1,0xfd45,"AUDIO NICHT VERFUEGBAR");
 gsoftkeys(a,"OK","ZURUECK");
}
static inline void tutorial(const PgaApi *a,GameUi *u,const Lesson *lessons,u32 n,u16 accent){
 center(a,22,2,accent,"TUTORIAL");const Lesson *p=lessons+u->page;
 center(a,54,1,0xffff,p->title);
 a->fill(12,75,216,1,accent);
 for(u32 i=0;i<9;i++)if(p->lines[i])gt(a,14,91+(int)i*21,1,0xe71c,p->lines[i]);
 gn(a,100,283,1,accent,u->page+1);gt(a,112,283,1,0xad55,"/");gn(a,124,283,1,accent,n);
}
/* Game draws its own title art above this shared menu. */
static inline void ui_menu(const PgaApi *a,GameUi *u,const Lesson *lessons,u32 pages,u16 accent){
 static const char *const home[]={"FORTSETZEN","NEUES SPIEL","BEENDEN","EINSTELLUNGEN"};
 static const char *const paused[]={"WEITERSPIELEN","HAUPTMENUE","EINSTELLUNGEN"};
 static const char *const result[]={"NOCH EINE RUNDE","HAUPTMENUE"};
 static const char *const confirm[]={"NEU STARTEN","ABBRECHEN"};
 if(u->screen==UI_HOME)menu(a,u,home,4,150,accent);
 else if(u->screen==UI_PAUSED){center(a,100,2,0xffff,"PAUSE");menu(a,u,paused,3,154,accent);}
 else if(u->screen==UI_RESULT)menu(a,u,result,2,198,accent);
 else if(u->screen==UI_CONFIRM){center(a,108,2,0xffff,"NEUE RUNDE?");menu(a,u,confirm,2,174,accent);}
 else if(u->screen==UI_SETTINGS){
  center(a,24,2,accent,"EINSTELLUNGEN");
  const char *items[]={u->flags&1?"MUSIK     AN":"MUSIK    AUS",u->flags&2?"SFX       AN":"SFX      AUS","TUTORIAL","ZURUECK"};
  menu(a,u,items,4,102,accent);
 }else if(u->screen==UI_TUTORIAL)tutorial(a,u,lessons,pages,accent);
 ui_notice(a,u);
}
#endif
