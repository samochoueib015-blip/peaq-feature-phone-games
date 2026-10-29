#include "menu_loading.h"
#include "cold_display.h"
#include "gfx.h"
#define GUARD 0xc08ad137u
static struct {uint32_t before[16];uint16_t pixels[240*320];uint32_t after[16];}
 canvas __attribute__((section(".loading"),aligned(64)));
static unsigned loading_entered,loading_stage,loading_percent;
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
 /* Ten progress updates; no full framebuffer transfer per 4-KiB read. */
 percent=percent/10*10;
 if(stage==loading_stage&&percent==loading_percent)return 1;
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
void peaq_menu_loading_error(unsigned code){
 if(!loading_entered||!cold()||!guards())return;
 background();text(100,2,0xfd45,"STARTUP FAILED");
 char detail[]="ERROR  00";code=code%100;
 detail[7]='0'+code/10;detail[8]='0'+code%10;text(138,2,0xffff,detail);
 text(190,1,0xbdf7,"RETURNING TO PHONE MENU");
 if(!present())return;
 volatile uint32_t *timer=(volatile uint32_t *)0x4023000c;
 uint32_t began=*timer,last=began;unsigned stalled=0;
 for(unsigned i=0;i<64000000u;++i){
  uint32_t t=*timer;if((t-last)&0x80000000u||t-began>=3000u)break;
  if(t==last){if(++stalled>=65536u)break;}else stalled=0;last=t;
 }
}
static char *label(char *p,const char *s){while(*s)*p++=*s++;return p;}
static char *number(char *p,uint32_t v){
 char digits[10];unsigned n=0;do{digits[n++]='0'+v%10;v/=10;}while(v);
 while(n)*p++=digits[--n];return p;
}
static char *hex_number(char *p,uint32_t v){
 for(int i=7;i>=0;--i)*p++="0123456789ABCDEF"[(v>>(i*4))&15u];
 return p;
}
int peaq_menu_exit_screen(unsigned selected,const uint32_t sound[18]){
 if(!loading_entered||!cold()||!guards()||selected>1||!sound)return 0;
 rect(0,0,320,240,0x0842);text(15,3,0xffff,"EMERALD");text(55,2,0xbdf7,"RETURN TO PHONE MENU?");
 rect(40,80,240,26,selected==0?0x2251:0x1083);
 rect(40,112,240,26,selected==1?0x2251:0x1083);
 rect(40,selected?112:80,4,26,0x05ef);
 text(86,2,selected==0?0xffff:0xad55,"RESUME");
 text(118,2,selected==1?0xffff:0xad55,"EXIT GAME");
 /* Temporary read-only measurements for the reported silent handset.
  * Captured before stop/restart, never inferred from a modeled completion. */
 char line[48],*p=label(line,"TON ");p=number(p,sound[0]);p=label(p,"   PCM ");p=number(p,sound[1]);*p=0;
 p=label(p,"   ADR ");p=number(p,sound[17]);*p=0;text(146,1,0xbdf7,line);
 p=label(line,"DMA ");p=number(p,sound[3]);*p++='/';p=number(p,sound[2]);p=label(p,"   FEHLER ");p=number(p,sound[4]);*p=0;
 text(156,1,0xbdf7,line);
 for(unsigned row=0;row<6;++row){
  p=line;
  for(unsigned col=0;col<2;++col){
   unsigned i=row*2+col;*p++='R';*p++="0123456789AB"[i];*p++=' ';
   p=hex_number(p,sound[i+5]);if(!col){*p++=' ';*p++=' ';}
  }
  *p=0;text(169+row*10,1,0xbdf7,line);
 }
 text(231,1,0xbdf7,"OK                        BACK");
 return present();
}
void peaq_menu_runtime_error(unsigned audio_error,unsigned other_error){
 if(!loading_entered||!cold()||!guards())return;
 background();text(100,2,0xfd45,audio_error?"AUDIO ERROR":"GAME ERROR");
 unsigned code=(audio_error?audio_error:other_error)%100;
 char detail[]="ERROR  00";detail[7]='0'+code/10;detail[8]='0'+code%10;
 text(138,2,0xffff,detail);text(190,1,0xbdf7,"RETURNING TO PHONE MENU");
 if(!present())return;
 volatile uint32_t *timer=(volatile uint32_t *)0x4023000c;
 uint32_t began=*timer,last=began;unsigned stalled=0;
 for(unsigned i=0;i<640000000u;++i){
  uint32_t t=*timer;if((t-last)&0x80000000u||t-began>=8000u)break;
  if(t==last){if(++stalled>=65536u)break;}else stalled=0;last=t;
 }
}
void peaq_menu_volume_notice(unsigned level,uint32_t now){
 if(level>8)return;
 volume_overlay.level=level;volume_overlay.until=now+1400u;volume_overlay.active=1;
}
void peaq_menu_volume_clear(void){volume_overlay.active=0;}
void peaq_menu_volume_overlay(uint16_t *pixels,uint32_t now){
 /* Only the permanent LCD copy is decorated. The engine picture is never
  * changed, so the next clean frame removes the badge after its deadline. */
 if((uintptr_t)pixels!=0x813d8040u||!volume_overlay.active)return;
 if(!((now-volume_overlay.until)&0x80000000u)){volume_overlay.active=0;return;}
 pixels_rect(pixels,90,190,140,38,0x0842);
 pixels_rect(pixels,90,190,3,38,0x05ef);
 char label[]="VOL 0/8";label[4]='0'+volume_overlay.level;
 for(unsigned i=0;i<7;++i)for(unsigned col=0;col<5;++col)for(unsigned row=0;row<7;++row)
  if(glyphs[(unsigned)label[i]-32u][col]&(1u<<row))
   pixels_rect(pixels,118+(i*6+col)*2,196+row*2,2,2,0xffff);
 for(unsigned i=0;i<8;++i)pixels_rect(pixels,98+i*16,218,12,5,
  i<volume_overlay.level?0x05ef:0x39c7);
}
