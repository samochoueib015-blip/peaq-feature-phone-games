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
/* Same nearest-neighbour pixels and CCW orientation as the qualified v28
 * paths. Write one contiguous physical column at a time into the separately
 * owned DMA buffer. No source write, hidden-frame draw or asynchronous DMA. */
static const uint16_t fit_rows[213] = {
    38160,37920,37680,37440,37440,37200,36960,36720,36720,36480,36240,36000,36000,35760,35520,35280,35280,35040,34800,34560,34560,34320,34080,33840,33840,33600,33360,33120,33120,32880,32640,32400,32400,32160,31920,31680,31680,31440,31200,30960,30960,30720,30480,30240,30240,30000,29760,29520,29520,29280,29040,28800,28800,28560,28320,28080,28080,27840,27600,27360,27360,27120,26880,26640,26640,26400,26160,25920,25920,25680,25440,25200,25200,24960,24720,24480,24480,24240,24000,23760,23760,23520,23280,23040,23040,22800,22560,22320,22320,22080,21840,21600,21600,21360,21120,20880,20880,20640,20400,20160,20160,19920,19680,19440,19440,19200,18960,18720,18720,18480,18240,18000,18000,17760,17520,17280,17280,17040,16800,16560,16560,16320,16080,15840,15840,15600,15360,15120,15120,14880,14640,14400,14400,14160,13920,13680,13680,13440,13200,12960,12960,12720,12480,12240,12240,12000,11760,11520,11520,11280,11040,10800,10800,10560,10320,10080,10080,9840,9600,9360,9360,9120,8880,8640,8640,8400,8160,7920,7920,7680,7440,7200,7200,6960,6720,6480,6480,6240,6000,5760,5760,5520,5280,5040,5040,4800,4560,4320,4320,4080,3840,3600,3600,3360,3120,2880,2880,2640,2400,2160,2160,1920,1680,1440,1440,1200,960,720,720,480,240,0,0
};
static void peaq_video_scale_direct(uint16_t *out,const uint16_t *src,unsigned full){
 for(unsigned x=0;x<320;++x){
  const uint16_t *s=src+x*3u/4u;
  uint16_t *d=out+x*240u;
  if(full){
   s+=159u*240u;
   for(unsigned pair=0;pair<80;++pair){
    uint16_t a=s[0],b=s[-240];
    d[0]=a;d[1]=b;d[2]=b;d+=3;
    /* Do not form an out-of-object pointer after the final iteration. */
    if(pair<79)s-=480;
   }
  }else{
   for(unsigned y=0;y<14;++y)d[y]=0;
   for(unsigned y=0;y<213;++y)d[14+y]=s[fit_rows[y]];
   for(unsigned y=227;y<240;++y)d[y]=0;
  }
 }
}

void peaq_menu_video_copy(uint16_t *out,const uint16_t *source,uint32_t now){
 unsigned game=video.portrait&&source==video.portrait&&video.gba;
 if(game)peaq_video_scale_direct(out,video.gba,video.flags&MENU_VIDEO_FULL);
 else for(unsigned i=0;i<240u*320u;++i)out[i]=source[i];
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
