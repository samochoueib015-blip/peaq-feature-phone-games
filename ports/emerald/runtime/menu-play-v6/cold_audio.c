#include "cold_audio.h"
#include "audio_stream.h"
#include "codec_profile.h"
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define MAGIC 0x31414345u
#define GUARD 0xa7c314e2u
#define SAMPLES 1600u
#define BYTES (SAMPLES*2u)
#define DMA_BASE 0x20101000u
#define POLLS 65536u
static struct {
 uint32_t before[16];PeaqColdAudioReport r;PeaqEmeraldAudio stream;
 uint32_t buffer_before[16];int16_t samples[SAMPLES];uint32_t after[16];
} audio __attribute__((section(".audio_state"),aligned(64)));
_Static_assert(sizeof(audio)<=32768,"audio reservation exceeded");
static uint32_t now(void){return R(0x4023000c);}
static void barrier(void){__asm__ volatile("dsb sy":::"memory");}
static int fail(uint32_t e){if(!audio.r.error)audio.r.error=e;audio.r.poisoned=1;return 0;}
static int guards(void){
 for(unsigned i=0;i<16;++i)if(audio.before[i]!=GUARD||audio.buffer_before[i]!=GUARD||audio.after[i]!=GUARD)return 0;
 return 1;
}
static uint32_t cp(unsigned which){
 uint32_t v=0;
 switch(which){
 case 0:__asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(v));break;
 case 1:__asm__ volatile("mrc p15,0,%0,c1,c0,1":"=r"(v));break;
 case 2:__asm__ volatile("mrc p15,0,%0,c2,c0,0":"=r"(v));break;
 case 3:__asm__ volatile("mrc p15,0,%0,c2,c0,1":"=r"(v));break;
 case 4:__asm__ volatile("mrc p15,0,%0,c2,c0,2":"=r"(v));break;
 case 5:__asm__ volatile("mrc p15,0,%0,c3,c0,0":"=r"(v));break;
 case 6:__asm__ volatile("mrs %0,cpsr":"=r"(v));break;
 case 7:__asm__ volatile("mrc p15,0,%0,c0,c0,0":"=r"(v));break;
 }
 return v;
}
static int context(unsigned cold){
 uint32_t c=cp(6)&255u,s=cp(0);
 if(c!=0xdfu&&c!=0xd3u)return 0;
 if(!(s&0x1005u))return 1;
 return !cold&&s==0x00c5187du&&cp(1)==0x6040u&&cp(2)==0x81200000u&&
  cp(3)==0&&cp(4)==0&&cp(5)==1;
}
static int delay(unsigned ms){
 uint32_t start=now(),last=start,stalled=0;
 for(unsigned i=0;i<64000000u;++i){
  uint32_t t=now();++audio.r.polls;
  if((t-last)&0x80000000u)return fail(PEAQ_AUDIO_TIMER);
  if(t-start>=ms)return 1;
  if(t==last){if(++stalled>=POLLS)return fail(PEAQ_AUDIO_TIMER);}else stalled=0;
  last=t;
 }
 return fail(PEAQ_AUDIO_TIMER);
}
static int wait(uint32_t address,uint32_t mask,uint32_t value,uint32_t ms,uint32_t error){
 uint32_t start=now(),last=start;
 for(unsigned i=0;i<POLLS;++i){
  ++audio.r.polls;if((R(address)&mask)==value)return 1;
  uint32_t t=now();if((t-last)&0x80000000u)return fail(PEAQ_AUDIO_TIMER);
  if(t-start>=ms)break;last=t;
 }
 return fail(error);
}
static int adi_read(uint32_t a,uint32_t *v){
 R(0x40600028)=a;
 if(!wait(0x4060002c,0x80000000u,0,20,PEAQ_AUDIO_ADI))return 0;
 *v=R(0x4060002c)&65535u;++audio.r.adi_reads;return 1;
}
static int adi_write(uint32_t a,uint32_t v){
 if(!wait(0x40600030,0x800u,0,20,PEAQ_AUDIO_ADI))return 0;
 R(a)=v&65535u;barrier();++audio.r.adi_writes;return 1;
}
static int apply(const PeaqCodecOp *ops,unsigned count,unsigned settle){
 for(unsigned i=0;i<count;++i){
  PeaqCodecOp o=ops[i];uint32_t v=0;
  if(o.kind==2){if(!delay(o.address))return 0;continue;}
  if(o.kind==1){
   if(o.keep&&!adi_read(o.address,&v))return 0;
   if(!adi_write(o.address,(v&o.keep)|o.set))return 0;
  }else {if(o.keep)v=R(o.address);R(o.address)=(v&o.keep)|o.set;barrier();}
  /* Covers the short original fixed reset/calibration pulse loops; retains
   * their order and gives each pulse a bounded millisecond to settle. */
  if(settle&&!delay(1))return 0;
 }
 return 1;
}
#define APPLY(name,settle) apply(codec_##name,sizeof(codec_##name)/sizeof(codec_##name[0]),settle)
static uint32_t ch(unsigned i){return DMA_BASE+audio.r.channels[i]*64u;}
static int halt_pair(void){
 unsigned ok=1;
 for(unsigned i=0;i<2;++i)if(audio.r.owned_mask&(1u<<i)){
  uint32_t b=ch(i);++audio.r.pause_attempts;
  if(R(b+8)&1u){
   R(b)|=1u;barrier();
   if(!wait(b,4,4,20,PEAQ_AUDIO_PAUSE)){ok=0;continue;}
   ++audio.r.pause_acks;R(b+8)&=~1u;barrier();
  }
  if(R(b+8)&1u){ok=0;fail(PEAQ_AUDIO_PAUSE);continue;}
  R(b)&=~1u;R(b+12)=0x1f000014u;barrier();
  audio.r.owned_mask&=~(1u<<i);
 }
 if(!ok||audio.r.owned_mask)return 0;
 audio.r.running=0;return 1;
}
static int free_channels(void){
 if(R(0x20102074)||R(0x20102070))return 0;
 uint32_t busy=R(0x2010001c);
 for(unsigned q=0;q<38;++q){
  uint32_t v=R(0x20102000u+q*4u);if(v>32)return 0;
  if(v)busy|=1u<<(v-1);
 }
 unsigned n=0;
 for(unsigned i=0;i<32&&n<2;++i)if(!(busy&(1u<<i))&&!(R(DMA_BASE+i*64u+8)&1u)&&!(R(DMA_BASE+i*64u)&1u))
  audio.r.channels[n++]=i;
 return n==2;
}
static int launch(unsigned volume,unsigned silence){
 if(audio.r.running||audio.r.owned_mask||audio.r.poisoned||!guards())return fail(PEAQ_AUDIO_ORDER);
 for(unsigned i=0;i<2;++i)if(R(ch(i)+8)&1u)return fail(PEAQ_AUDIO_BUSY);
 if(silence){for(unsigned i=0;i<SAMPLES;++i)audio.samples[i]=0;++audio.r.silence_blocks;}
 else if(!peaq_emerald_audio_pull(&audio.stream,audio.samples,800,volume)||
         !peaq_emerald_audio_pull(&audio.stream,audio.samples+800,800,volume))return fail(PEAQ_AUDIO_GUARD);
 for(unsigned i=0;i<SAMPLES;++i){
  int32_t sample=audio.samples[i];uint32_t magnitude=(uint32_t)(sample<0?-sample:sample);
  if(magnitude>audio.r.pcm_peak)audio.r.pcm_peak=magnitude;
 }
 for(unsigned i=0;i<2;++i){
  uint32_t b=ch(i);
  R(b)=0;R(b+4)=0;R(b+8)=0x3000;R(b+12)=0x1f000014u;
  R(b+16)=(uintptr_t)audio.samples;R(b+20)=i?0x40021000u:0x40021004u;
  R(b+24)=0x51300140u;R(b+28)=320;R(b+32)=BYTES;R(b+36)=2;
  for(unsigned a=40;a<64;a+=4)R(b+a)=0;
 }
 barrier();audio.r.completion_mask=0;audio.r.owned_mask=3;
 R(ch(0)+8)=0x01003001u;R(ch(1)+8)=0x01003001u;barrier();
 R(0x40020018)|=0x300;barrier();
 audio.r.running=1;audio.r.dma_started_ms=now();++audio.r.submitted;
 audio.r.submitted_samples+=SAMPLES;return 1;
}
static int progress(uint32_t t){
 if(!audio.r.running)return 1;
 for(unsigned i=0;i<2;++i){
  uint32_t status=R(ch(i)+12);audio.r.dma_status[i]=status;audio.r.dma_source[i]=R(ch(i)+16);
  if(status&0x100000u)return fail(PEAQ_AUDIO_DMA);
  if(status&0x40000u)audio.r.completion_mask|=1u<<i;
 }
 if(audio.r.completion_mask==3){
  if(!halt_pair())return 0;++audio.r.completed;return 1;
 }
 if(t-audio.r.dma_started_ms>500u)return fail(PEAQ_AUDIO_TIMEOUT);
 return 1;
}
int peaq_cold_audio_start(uint32_t volume){
 if(volume>8||!context(1)||cp(7)!=0x410fc075u||R(0x402e00fc)!=0x98180001u)return 0;
 if(audio.r.magic==MAGIC&&(!audio.r.stopped||audio.r.owned_mask||audio.r.poisoned))return 0;
 for(unsigned i=0;i<sizeof(audio);++i)((uint8_t *)&audio)[i]=0;
 for(unsigned i=0;i<16;++i)audio.before[i]=audio.buffer_before[i]=audio.after[i]=GUARD;
 audio.r.magic=MAGIC;audio.r.version=2;audio.r.bytes=sizeof(audio.r);audio.r.volume=volume;
 audio.r.started_ms=audio.r.last_ms=now();
 peaq_emerald_audio_init(&audio.stream);
 R(0x20e01000)=0x20;barrier(); /* AP-DMA clock set, no other clock clear */
 if(!free_channels())return fail(PEAQ_AUDIO_BUSY);
 /* A cold boot can leave the VBC bus clock off: even its status register
  * cannot be probed then. Preserve an already active owner before changing
  * routing, and follow original 80ab6df8's clock-before-register order. */
 if((R(0x402e0000)&0x80000u)&&(R(0x40020018)&0x303u))return fail(PEAQ_AUDIO_BUSY);
 R(0x402e2020)=0xc0000;R(0x402e1000)=0x80000;barrier();
 if(!(R(0x402e0000)&0x80000u))return fail(PEAQ_AUDIO_CONTEXT);
 if(R(0x40020018)&0x303u)return fail(PEAQ_AUDIO_BUSY);
 audio.r.reserved_mask=3;
 R(0x20102074)=audio.r.channels[0]+1;R(0x20102070)=audio.r.channels[1]+1;barrier();
 if(R(0x20102074)!=audio.r.channels[0]+1||R(0x20102070)!=audio.r.channels[1]+1)return fail(PEAQ_AUDIO_BUSY);
 audio.r.vbc_owned=1;
 R(0x402e1008)=0x100000;
 if(!delay(1))return 0;R(0x402e2008)=0x100000;
 R(0x4002003c)&=~0x1c7u;
 audio.r.powered=1; /* teardown is required even after partial codec startup */
 if(!APPLY(reset,1)||!delay(20))return 0;
 uint32_t outputs;if(!adi_read(0x40608724,&outputs))return 0;
 /* This speaker-only implementation deliberately rejects another analog
  * output surviving reset. It never guesses the headphone/depop branch. */
 if(outputs&~0x2001u)return fail(PEAQ_AUDIO_ROUTE);
 if(!APPLY(power,1)||!delay(20)||!APPLY(dac,1))return 0;
 audio.r.powered=1;
 R(0x400200c8)|=3u;R(0x40020018)|=3u;
 /* Original 80ab6a88(160,80) clears the complete 18-bit FIFO field. */
 R(0x400200e4)=(R(0x400200e4)&~0x3ffffu)|0xa0a0u;
 for(unsigned i=0;i<20;++i){R(0x40021000)=0;R(0x40021004)=0;}
 /* Restore the original output gain fields before enabling the internal PA.
  * Actual in-game level is applied in software to every submitted PCM block. */
 if(!APPLY(gain,0)||!APPLY(speaker,0))return 0;
 /* Original80aaebe8(0): enable the DAC mute-control field and request unmute.
  * Readback proves the requested field, not an analog depop measurement. */
 R(0x4000000c)|=0x8000u;R(0x4000000c)&=~0x4000u;barrier();
 if(!wait(0x4000000c,0x4000u,0,20,PEAQ_AUDIO_ROUTE))return 0;
 audio.r.initialized=1;audio.r.guards_ok=1;return 1;
}
int peaq_cold_audio_push(const int8_t *p,uint32_t n,uint32_t rate){
 if(audio.r.magic!=MAGIC||!audio.r.initialized||audio.r.stopped||audio.r.poisoned||!context(0)||!guards())return 0;
 uintptr_t a=(uintptr_t)p;
 if(a+n<a||(a<(uintptr_t)&audio+sizeof(audio)&&a+n>(uintptr_t)&audio))return 0;
 return peaq_emerald_audio_push(&audio.stream,p,n,rate);
}
int peaq_cold_audio_pump(uint32_t volume,uint32_t t){
 if(audio.r.magic!=MAGIC||!audio.r.initialized||audio.r.stopped||audio.r.poisoned)return 0;
 if(!context(0))return fail(PEAQ_AUDIO_CONTEXT);
 if(!guards())return fail(PEAQ_AUDIO_GUARD);
 if(volume>8)return fail(PEAQ_AUDIO_VOLUME);
 if((t-audio.r.last_ms)&0x80000000u)return fail(PEAQ_AUDIO_TIMER);
 audio.r.last_ms=t;audio.r.volume=volume;
 if(!progress(t))return 0;
 if(!audio.r.running&&audio.stream.count>=SAMPLES&&!launch(volume,0))return 0;
 audio.r.queued=audio.stream.count;audio.r.underflow=audio.stream.underflow_samples;audio.r.overflow=audio.stream.overflow_blocks;
 return 1;
}
int peaq_cold_audio_stop(uint32_t drain){
 if(audio.r.magic!=MAGIC||drain>1||!context(1))return 0;
 if(audio.r.stopped)return audio.r.stop_complete;
 if(!audio.r.vbc_owned){audio.r.stopped=1;return 0;}
 unsigned ok=guards(),quiet=0;uint32_t begin=now();
 if(drain&&!audio.r.poisoned&&audio.r.initialized){
  for(unsigned i=0;i<POLLS;++i){
   uint32_t t=now();if(!progress(t)){ok=0;break;}
   if(!audio.r.running){
    if(audio.stream.count){++audio.r.drain_blocks;if(!launch(audio.r.volume,0)){ok=0;break;}}
    else if(!quiet){quiet=1;if(!launch(0,1)){ok=0;break;}}
    else break;
   }
   if(t-begin>1000u){ok=0;fail(PEAQ_AUDIO_TIMEOUT);break;}
   if(i+1==POLLS){ok=0;fail(PEAQ_AUDIO_TIMER);}
  }
 }
 /* Mute even when a DMA transaction failed; permanent buffers remain pinned
  * until watchdog reset if the pause acknowledgment cannot be obtained. */
 if(audio.r.powered){R(0x4000000c)|=0xc000u;barrier();}
 if(audio.r.powered&&!APPLY(mute,0))ok=0;
 if(!halt_pair())ok=0;
 if(audio.r.owned_mask){audio.r.poisoned=1;return 0;}
 R(0x40020018)&=~0x303u;R(0x400200c8)&=~3u;
 for(unsigned i=0;i<320;++i){R(0x40021000)=0;R(0x40021004)=0;}
 if(audio.r.powered){if(!delay(1200)||!APPLY(off,1))ok=0;}
 if(audio.r.reserved_mask){
  if(R(0x20102074)!=audio.r.channels[0]+1||R(0x20102070)!=audio.r.channels[1]+1){ok=0;fail(PEAQ_AUDIO_BUSY);}
  else {R(0x20102074)=0;R(0x20102070)=0;audio.r.reserved_mask=0;}
 }
 R(0x402e1008)=0x100000;if(!delay(1))ok=0;R(0x402e2008)=0x100000;
 audio.r.powered=0;audio.r.stopped=1;audio.r.running=0;
 audio.r.vbc_owned=0;
 audio.r.stop_complete=ok&&!audio.r.poisoned&&guards();return audio.r.stop_complete;
}
const PeaqColdAudioReport *peaq_cold_audio_status(void){return &audio.r;}
int peaq_cold_audio_route(uint32_t out[13]){
 static const uint32_t digital[]={0x402e0000,0x40020018,0x4002003c,
  0x400200c8,0x400200e4,0x40000000,0x4000000c};
 static const uint32_t analog[]={0x40608718,0x40608720,0x40608724,0x40608728,0x4060872c};
 if(!out||!context(1)||!audio.r.initialized||!audio.r.powered||audio.r.stopped||!guards())return 0;
 out[12]=0;
 for(unsigned i=0;i<7;++i)out[i]=R(digital[i]);
 for(unsigned i=0;i<5;++i){
  if(!adi_read(analog[i],&out[i+7]))return 0;
  /* Original reader 80b1383c checks this echo. Capture mismatches without
   * changing any codec field, gain or DMA transaction. */
  if((R(0x4060002c)>>16)!=((analog[i]>>2)&0x7fffu))out[12]|=1u<<i;
 }
 return 1;
}
