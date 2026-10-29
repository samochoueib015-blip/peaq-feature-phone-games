#include "audio_stream.h"
#include "game_controls.h"
#define MAGIC 0x31534145u
static int separate(const void *p,uint32_t n,const void *q,uint32_t m){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
 return a+n>=a&&b+m>=b&&(a+n<=b||b+m<=a);
}
static void add(uint32_t *v,uint32_t n){*v=n>0xffffffffu-*v?0xffffffffu:*v+n;}
static int valid(const PeaqEmeraldAudio *a){
 return a&&!((uintptr_t)a&3u)&&a->magic==MAGIC&&a->phase<21&&
  a->read_at<PEAQ_EMERALD_AUDIO_RING&&a->write_at<PEAQ_EMERALD_AUDIO_RING&&
  a->count<=PEAQ_EMERALD_AUDIO_RING&&a->previous>=-32768&&a->previous<=32512;
}
void peaq_emerald_audio_init(PeaqEmeraldAudio *a){
 if(!a||((uintptr_t)a&3u))return;
 for(unsigned i=0;i<sizeof(*a);++i)((uint8_t *)a)[i]=0;
 a->magic=MAGIC;
}
int peaq_emerald_audio_push(PeaqEmeraldAudio *a,const int8_t *src,uint32_t n,uint32_t rate){
 if(!valid(a)||!src||!n||n>PEAQ_EMERALD_AUDIO_INPUT_MAX||rate!=13440u||
    !separate(a,sizeof(*a),src,n))return 0;
 uint32_t need=(25u*n-a->phase+20u)/21u;
 if(need>PEAQ_EMERALD_AUDIO_RING-a->count){add(&a->overflow_blocks,1);return 0;}
 for(unsigned i=0;i<n;++i){
  int32_t sample=(int32_t)src[i]*256;
  while(a->phase<25u){
   int32_t v=a->previous+(sample-a->previous)*(int32_t)a->phase/25;
   a->ring[a->write_at]=(int16_t)v;
   a->write_at=(a->write_at+1u)&(PEAQ_EMERALD_AUDIO_RING-1u);++a->count;
   a->phase+=21u;
  }
  a->phase-=25u;a->previous=sample;
 }
 add(&a->input_samples,n);return 1;
}
int peaq_emerald_audio_pull(PeaqEmeraldAudio *a,int16_t *dst,uint32_t n,uint32_t volume){
 if(!valid(a)||!dst||((uintptr_t)dst&1u)||!n||n>PEAQ_EMERALD_AUDIO_PULL_MAX||
    volume>PEAQ_EMERALD_VOLUME_MAX||!separate(a,sizeof(*a),dst,n*2u))return 0;
 for(unsigned i=0;i<n;++i){
  int16_t v=0;
  if(a->count){
   v=a->ring[a->read_at];a->read_at=(a->read_at+1u)&(PEAQ_EMERALD_AUDIO_RING-1u);--a->count;
  }else add(&a->underflow_samples,1);
  dst[i]=peaq_emerald_volume_sample(v,volume);
 }
 add(&a->output_samples,n);return 1;
}
