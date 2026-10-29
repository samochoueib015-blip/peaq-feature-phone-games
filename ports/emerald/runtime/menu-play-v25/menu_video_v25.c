#include "menu_video_v25.h"
#include "gfx.h"
/* Clang may use the ARM EABI copy for small initialized stack strings. */
extern void *memcpy(void *,const void *,unsigned);
void __aeabi_memcpy(void *out,const void *in,unsigned n){(void)memcpy(out,in,n);}
static struct {
 const uint16_t *portrait,*gba;
 uint32_t flags,began,frames,fps,clock_valid,battery_mv;
} video;
#define PREF_MAGIC 0x31504656u /* VFP1 */
static uint32_t hash(const uint8_t *p,unsigned n){
 uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;
}
unsigned peaq_menu_video_flags(void){return video.flags;}
unsigned peaq_menu_video_fps(void){return video.fps;}
void peaq_menu_video_reset_clock(void){video.clock_valid=video.frames=video.fps=0;}
void peaq_menu_video_set(unsigned flags){if(!(flags&~MENU_VIDEO_MASK))video.flags=flags;}
void peaq_menu_video_bind(const uint16_t *portrait,const uint16_t *gba){
 video.portrait=portrait;video.gba=gba;peaq_menu_video_reset_clock();
}
void peaq_menu_video_battery(unsigned mv){video.battery_mv=mv>=2500&&mv<=4600?mv:0;}
void peaq_menu_video_restore(const uint32_t *r){
 video.flags=MENU_VIDEO_BATTERY;
 if(r&&r[0]==0x31525045u&&r[1]==1&&r[2]==512&&r[3]==16&&
    r[21]==PREF_MAGIC&&!(r[22]&~MENU_VIDEO_MASK)&&r[127]==hash((const uint8_t*)r,508))
  video.flags=r[22];
}
void peaq_menu_video_record(uint32_t *r){
 r[21]=PREF_MAGIC;r[22]=video.flags;r[23]=video.fps;r[24]=video.battery_mv;r[25]=0;
}
static void rect(uint16_t *p,unsigned x,unsigned y,unsigned w,unsigned h,uint16_t c){
 for(unsigned yy=y;yy<y+h;++yy)for(unsigned xx=x;xx<x+w;++xx)p[xx*240+239-yy]=c;
}
static void label(uint16_t *p,unsigned x,const char *s){
 for(unsigned i=0;s[i];++i)for(unsigned c=0;c<5;++c)for(unsigned r=0;r<7;++r)
  if(glyphs[(unsigned)s[i]-32][c]&(1u<<r))rect(p,x+i*6+c,3+r,1,1,0xffff);
}
void peaq_menu_video_copy(uint16_t *out,const uint16_t *source,uint32_t now){
 unsigned game=video.portrait&&source==video.portrait&&video.gba;
 if(game&&(video.flags&MENU_VIDEO_FULL)){
  /* Direct nearest-neighbour 240x160 -> 320x240. Same CCW orientation,
   * no cropping, no second framebuffer and no resampling of scaled pixels. */
  for(unsigned y=0;y<240;++y){
   const uint16_t *s=video.gba+((239-y)*2/3)*240;uint16_t *d=out+y;
   for(unsigned x=0;x<80;++x){
    uint16_t a=s[0],b=s[1],c=s[2];
    d[0]=a;d[240]=a;d[480]=b;d[720]=c;s+=3;d+=960;
   }
  }
 }else for(unsigned i=0;i<240u*320u;++i)out[i]=source[i];
 if(!game){peaq_menu_video_reset_clock();return;}
 /* Only game presentations reach this counter. Hidden simulation frames
  * never call it. Loading, pause and saving rebase the measurement. */
 if(!video.clock_valid){video.began=now;video.frames=0;video.clock_valid=1;}
 else {
  ++video.frames;uint32_t elapsed=now-video.began;
  if(elapsed&0x80000000u)peaq_menu_video_reset_clock();
  else if(elapsed>=1000){
   video.fps=(video.frames*1000u+elapsed/2)/elapsed;
   if(video.fps>99)video.fps=99;
   video.frames=0;video.began=now;
  }
 }
 if(video.flags&MENU_VIDEO_FPS){
  char s[]="FPS --";
  if(video.fps){s[4]='0'+video.fps/10;s[5]='0'+video.fps%10;}
  rect(out,1,1,41,11,0x0842);label(out,4,s);
 }
 if(video.flags&MENU_VIDEO_BATTERY){
  char s[]="AKKU --";
  char v[]="AKKU 0.00V";
  if(video.battery_mv){
   unsigned n=video.battery_mv/10;
   v[5]='0'+n/100;v[7]='0'+n/10%10;v[8]='0'+n%10;
  }
  rect(out,253,1,66,11,0x0842);label(out,256,video.battery_mv?v:s);
 }
}
