#include "audio_stream.h"
#include "game_controls.h"
#define MAGIC 0x31534145u
#define TAPS 14u
_Static_assert(TAPS<=16&&!(TAPS&1),"even filter length fits the private history");
/* Lanczos-windowed sinc, normalized independently at each 1/25 phase.
 * The exact integer phase is handled directly. Phases13..24 are the
 * time reversals of12..1, so only half the Q14 table is stored. */
static const int16_t coefficients[12][TAPS]={
 {1,-18,47,-91,161,-292,660,16339,-606,277,-154,87,-44,17},
 {2,-38,96,-185,328,-595,1370,16208,-1154,537,-299,168,-85,31},
 {5,-60,146,-280,496,-904,2126,15992,-1643,775,-433,242,-122,44},
 {9,-82,197,-375,664,-1216,2921,15693,-2070,989,-554,309,-155,54},
 {13,-104,247,-469,829,-1524,3751,15310,-2433,1178,-661,368,-183,62},
 {19,-127,296,-559,987,-1824,4608,14851,-2733,1339,-753,418,-206,68},
 {25,-149,343,-644,1137,-2112,5486,14318,-2970,1471,-828,459,-224,72},
 {31,-170,387,-722,1275,-2380,6378,13716,-3145,1574,-888,491,-237,74},
 {38,-190,426,-792,1398,-2626,7275,13057,-3260,1647,-930,513,-246,74},
 {45,-207,460,-852,1504,-2842,8170,12339,-3316,1690,-955,525,-249,72},
 {52,-222,488,-901,1590,-3023,9056,11573,-3316,1705,-965,528,-249,68},
 {58,-235,509,-936,1653,-3166,9923,10766,-3265,1692,-958,523,-244,64},
};
static int separate(const void *p,uint32_t n,const void *q,uint32_t m){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
 return a+n>=a&&b+m>=b&&(a+n<=b||b+m<=a);
}
static void add(uint32_t *v,uint32_t n){*v=n>0xffffffffu-*v?0xffffffffu:*v+n;}
static int valid(const PeaqEmeraldAudio *a){
 return a&&!((uintptr_t)a&3u)&&a->magic==MAGIC&&a->phase<21&&
  a->read_at<PEAQ_EMERALD_AUDIO_RING&&a->write_at<PEAQ_EMERALD_AUDIO_RING&&
  a->count<=PEAQ_EMERALD_AUDIO_RING&&a->history_at<16;
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
  unsigned at=a->history_at;
  a->history[at]=src[i];
  while(a->phase<25u){
   unsigned phase=a->phase;
   int32_t v=(int32_t)a->history[(at-TAPS/2)&15u]*256;
   if(phase){
    unsigned reverse=phase>12u;
    const int16_t *row=coefficients[(reverse?25u-phase:phase)-1u];
    int32_t sum=0;
    for(unsigned j=0;j<TAPS;++j)
     sum+=(int32_t)a->history[(at-j)&15u]*row[reverse?TAPS-1u-j:j];
    /* Q14 signed8 -> signed16, rounded once. Bound ringing at the PCM
     * rails explicitly, never wrap an overshoot to the opposite sign. */
    v=__builtin_arm_ssat((sum+32)>>6,16);
   }
   a->ring[a->write_at]=(int16_t)v;
   a->write_at=(a->write_at+1u)&(PEAQ_EMERALD_AUDIO_RING-1u);++a->count;
   a->phase+=21u;
  }
  a->phase-=25u;
  a->history_at=(at+1u)&15u;
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
