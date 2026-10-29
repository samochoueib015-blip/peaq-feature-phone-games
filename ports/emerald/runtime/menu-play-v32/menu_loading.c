#include "menu_exit.h"
#include "menu_video_v25.h"
#include "menu_loading.h"
#include "cold_display.h"
#include "gfx.h"
#define GUARD 0xc08ad137u
static struct {uint32_t before[16];uint16_t pixels[240*320];uint32_t after[16];}
 canvas __attribute__((section(".loading"),aligned(64)));
static unsigned loading_entered,loading_stage,loading_percent;
static uint32_t progress_ms;
static struct {uint32_t level,until,active;} volume_overlay;
static int cold(void){uint32_t s;__asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(s));return !(s&0x1005u);}
static int guards(void){
 for(unsigned i=0;i<16;++i)if(canvas.before[i]!=GUARD||canvas.after[i]!=GUARD)return 0;
 return 1;
}
static void pixels_rect(uint16_t *pixels,unsigned x,unsigned y,unsigned w,unsigned h,uint16_t color){
 if(x+w>320||y+h>240)return;
 for(unsigned yy=y;yy<y+h;++yy)for(unsigned xx=x;xx<x+w;++xx)
  pixels[xx*240+239-yy]=color;
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h,uint16_t color){
 pixels_rect(canvas.pixels,x,y,w,h,color);
}
static void text(unsigned y,unsigned scale,uint16_t color,const char *s){
 unsigned len=0;while(s[len]&&len<40)++len;
 unsigned width=(len*6-1)*scale;if(!len||width>320)return;
 unsigned x=(320-width)/2;
 for(unsigned i=0;i<len;++i){
  unsigned c=(uint8_t)s[i];if(c<32||c>95)c='?';
  for(unsigned col=0;col<5;++col)for(unsigned row=0;row<7;++row)
   if(glyphs[c-32][col]&(1u<<row))rect(x+(i*6+col)*scale,y+row*scale,scale,scale,color);
 }
}
static void background(void){
 rect(0,0,320,240,0x0842);rect(0,0,320,4,0x05ef);rect(0,236,320,4,0x05ef);
 text(38,4,0xffff,"EMERALD");
}
static int present(void){return guards()&&peaq_cold_display_present(canvas.pixels,240,320)==1;}
int peaq_menu_loading(unsigned stage,unsigned percent){
 static const char *const labels[]={"","READING SD CARD","CHECKING SD CARD",
  "CHECKING SAVE DATA","LOADING GAME","PREPARING INPUT",
  "PREPARING GAME","STARTING AUDIO","STARTING GAME"};
 if(!cold()||stage<MENU_LOAD_CARD||stage>MENU_LOAD_READY||percent>100)return 0;
 if(!loading_entered){
  for(unsigned i=0;i<16;++i)canvas.before[i]=canvas.after[i]=GUARD;
  loading_entered=1;
 }else if(!guards())return 0;
 /* Exact integer percent; bounded painting avoids slowing SD I/O. */
 uint32_t now=*(volatile uint32_t *)0x4023000cu;
 if(stage==loading_stage&&(percent==loading_percent||(percent<100&&now-progress_ms<100)))return 1;
 progress_ms=now;
 loading_stage=stage;loading_percent=percent;
 background();text(104,2,0xbdf7,labels[stage]);
 rect(30,148,260,14,0x39c7);
 if(stage==MENU_LOAD_ENGINE){
  rect(30,148,260*percent/100,14,0x05ef);
  char n[5]={(char)('0'+percent/100),(char)('0'+percent/10%10),(char)('0'+percent%10),'%',0};
  text(182,2,0xffff,n+(percent<10?2:percent<100?1:0));
 }else rect(30,148,260*stage/MENU_LOAD_READY,14,0x05ef);
 return present();
}
__attribute__((noinline)) static void error_wait(unsigned budget,unsigned duration){
 volatile uint32_t *timer=(volatile uint32_t *)0x4023000c;
 uint32_t began=*timer,last=began;unsigned stalled=0;
 for(unsigned i=0;i<budget;++i){
  uint32_t t=*timer;if((t-last)&0x80000000u||t-began>=duration)break;
  if(t==last){if(++stalled>=65536u)break;}else stalled=0;last=t;
 }
}
__attribute__((noinline)) static void error_screen(const char *label,unsigned code,unsigned budget,unsigned duration){
 if(!loading_entered||!cold()||!guards())return;
 background();text(100,2,0xfd45,label);
 char detail[]="ERROR  00";code%=100;
 detail[7]='0'+code/10;detail[8]='0'+code%10;text(138,2,0xffff,detail);
 text(190,1,0xbdf7,"RETURNING TO PHONE MENU");
 if(present())error_wait(budget,duration);
}
void peaq_menu_loading_error(unsigned code){error_screen("STARTUP FAILED",code,64000000u,3000u);}
int peaq_menu_pause_screen(const PeaqMenuExit *s,const char lessons[8][10][41]){
 if(!loading_entered||!cold()||!guards()||!s||s->page>3)return 0;
 unsigned flags=peaq_menu_video_flags();
 if(s->page==MENU_PAUSE_TUTORIAL){
  if(!lessons||s->selected>=8)return 0;
  rect(0,0,320,240,0x0842);
  const char (*page)[41]=lessons[s->selected];
  text(18,2,0x05ef,page[0]);
  for(unsigned i=1;i<10;++i)text(48+(i-1)*17,1,0xffff,page[i]);
  char number[]="TUTORIAL 1/8";number[9]+=(char)s->selected;
  text(208,1,0x05ef,number);
  text(225,1,0xbdf7,"OK: NEXT                 BACK");return present();
 }
 const char *labels[5];unsigned count;
 if(s->page==MENU_PAUSE_SETTINGS){
  labels[0]=flags&MENU_VIDEO_FULL?"SCREEN: FULL":"SCREEN: ORIGINAL";
  labels[1]=flags&MENU_VIDEO_FPS?"FPS DISPLAY: ON":"FPS DISPLAY: OFF";
  labels[2]=flags&MENU_VIDEO_BATTERY?"BATTERY DISPLAY: ON":"BATTERY DISPLAY: OFF";
  labels[3]="TUTORIAL";labels[4]="BACK";count=5;
 }else if(s->page==MENU_PAUSE_QUIT){labels[0]="RESUME";labels[1]="EXIT GAME";count=2;}
 else {labels[0]="RESUME";labels[1]="SETTINGS";labels[2]="EXIT GAME";count=3;}
 if(s->selected>=count)return 0;
 rect(0,0,320,240,0x0842);
 text(25,3,0xffff,s->page==MENU_PAUSE_SETTINGS?"SETTINGS":"EMERALD");
 if(s->page==MENU_PAUSE_QUIT)text(64,2,0xbdf7,"RETURN TO PHONE MENU?");
 for(unsigned i=0;i<count;++i){
  unsigned y=(s->page==MENU_PAUSE_SETTINGS?66:84)+i*28,selected=i==s->selected;
  rect(20,y,280,26,selected?0x2251:0x1083);
  if(selected)rect(20,y,4,26,0x05ef);
  text(y+6,2,selected?0xffff:0xad55,labels[i]);
 }
 text(223,1,0xbdf7,"OK                        BACK");return present();
}
int peaq_menu_saving(unsigned phase,unsigned percent){
 static const char *const labels[]={"","CHECKING SAVE","SAVING GAME","VERIFYING SAVE"};
 if(!loading_entered||!cold()||!guards()||phase<1||phase>3||percent>100)return 0;
 uint32_t now=*(volatile uint32_t *)0x4023000cu;
 if(phase+16u==loading_stage&&percent<100&&now-progress_ms<100)return 1;
 loading_stage=phase+16u;progress_ms=now;
 background();text(104,2,0xbdf7,labels[phase]);rect(30,148,260,14,0x39c7);
 rect(30,148,260*percent/100,14,0x05ef);
 char n[5]={(char)('0'+percent/100),(char)('0'+percent/10%10),(char)('0'+percent%10),'%',0};
 text(182,2,0xffff,n+(percent<10?2:percent<100?1:0));return present();
}
void peaq_menu_runtime_error(unsigned audio_error,unsigned other_error){
 error_screen(audio_error?"AUDIO ERROR":"GAME ERROR",audio_error?audio_error:other_error,640000000u,8000u);
}
void peaq_menu_volume_notice(unsigned level,uint32_t now){
 if(level>8)return;
 volume_overlay.level=level;volume_overlay.until=now+1400u;volume_overlay.active=1;
}
void peaq_menu_brightness_notice(unsigned level,uint32_t now){
 if(level<1||level>8)return;peaq_menu_volume_notice(level,now);volume_overlay.active=2;
}
void peaq_menu_volume_clear(void){volume_overlay.active=0;}
void peaq_menu_volume_overlay(uint16_t *pixels,uint32_t now){
 /* Only the permanent LCD copy is decorated. The engine picture is never
  * changed, so the next clean frame removes the badge after its deadline. */
 if((uintptr_t)pixels!=0x813d8040u||!volume_overlay.active)return;
 if(!((now-volume_overlay.until)&0x80000000u)){volume_overlay.active=0;return;}
 pixels_rect(pixels,65,190,190,38,0x0842);
 pixels_rect(pixels,65,190,3,38,0x05ef);
 char label[]="BRIGHTNESS 0/8";label[11]='0'+volume_overlay.level;
 char sound[]="VOL 0/8";sound[4]='0'+volume_overlay.level;
 unsigned len=volume_overlay.active==2?14:7,x=(320-(len*6-1)*2)/2;
 const char *value=volume_overlay.active==2?label:sound;
 for(unsigned i=0;i<len;++i)for(unsigned col=0;col<5;++col)for(unsigned row=0;row<7;++row)
  if(glyphs[(unsigned)value[i]-32u][col]&(1u<<row))
   pixels_rect(pixels,x+(i*6+col)*2,196+row*2,2,2,0xffff);
 for(unsigned i=0;i<8;++i)pixels_rect(pixels,98+i*16,218,12,5,
  i<volume_overlay.level?0x05ef:0x39c7);
}
