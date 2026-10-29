/* Three immutable AP-DMA descriptors per lane; only retired PCM is writable.
 * Register layout: Linux v6.12 drivers/dma/sprd-dma.c, Spreadtrum author.
 * A linked-list pointer addresses the next descriptor's source field (+16).
 * This is an independent implementation for the fixed, uncached PEAQ arena.
 */
#define BANKS 3u
#define CYCLE_BYTES (BANKS*BYTES)
#define COPY_LEAD_BYTES 1024u /* 32ms at16kHz; copying3200bytes is bounded below. */
static struct {
 uint32_t before[16];
 int16_t pcm[BANKS][SAMPLES];
 uint32_t descriptors[2][BANKS][16];
 uint32_t after[16];
} audio_dma __attribute__((section(".audio_dma"),aligned(64)));
_Static_assert(sizeof(audio_dma)<=16384,"DMA arena exceeded");
static int dma_guards(void){
 for(unsigned i=0;i<16;++i)if(audio_dma.before[i]!=GUARD||audio_dma.after[i]!=GUARD)return 0;
 return 1;
}
static void dma_init(void){
 for(unsigned i=0;i<sizeof(audio_dma);++i)((uint8_t *)&audio_dma)[i]=0;
 for(unsigned i=0;i<16;++i)audio_dma.before[i]=audio_dma.after[i]=GUARD;
}
static int dma_overlap(uintptr_t a,uint32_t n){
 return a<(uintptr_t)&audio_dma+sizeof(audio_dma)&&a+n>(uintptr_t)&audio_dma;
}
static int cursor(unsigned side,uint32_t *position){
 uint32_t status=R(ch(side)+12),src=R(ch(side)+16);
 audio.r.dma_status[side]=status;audio.r.dma_source[side]=src;
 if(status&0x100000u)return fail(PEAQ_AUDIO_DMA);
 uint32_t p=src-(uintptr_t)audio_dma.pcm;
 /* At a descriptor boundary SRC can briefly be the just-consumed end. */
 if(p>CYCLE_BYTES||(p&1u))return fail(PEAQ_AUDIO_DMA);
 *position=p==CYCLE_BYTES?0:p;return 1;
}
static int progress(uint32_t t){
 if(!audio.r.running)return 1;
 /* Modulo cursors cannot distinguish a whole unobserved300ms revolution.
  * Refuse ambiguous ownership instead of ever writing a possibly live bank. */
 if(t-audio.cycle_poll_ms>=280u)return fail(PEAQ_AUDIO_TIMEOUT);
 audio.cycle_poll_ms=t;
 for(unsigned i=0;i<2;++i){
  uint32_t p;if(!cursor(i,&p))return 0;
  uint32_t d=p>=audio.position[i]?p-audio.position[i]:CYCLE_BYTES-audio.position[i]+p;
  audio.position[i]=p;audio.travel[i]+=d;
  if(d)audio.lane_progress_ms[i]=t;
  if(t-audio.lane_progress_ms[i]>500u)return fail(PEAQ_AUDIO_TIMEOUT);
 }
 uint32_t done0=audio.travel[0]/BYTES,done1=audio.travel[1]/BYTES;
 audio.r.completed=done0<done1?done0:done1;
 if(audio.r.completed>audio.next)return fail(PEAQ_AUDIO_TIMEOUT);
 return 1;
}
static uint32_t bank_lead(unsigned bank){
 uint32_t minimum=CYCLE_BYTES;
 for(unsigned i=0;i<2;++i){
  uint32_t p;if(!cursor(i,&p))return 0;
  uint32_t start=bank*BYTES;
  if(p>=start&&p<start+BYTES)return fail(PEAQ_AUDIO_ORDER);
  uint32_t lead=(start+CYCLE_BYTES-p)%CYCLE_BYTES;
  if(lead<minimum)minimum=lead;
 }
 return minimum;
}
static int copy_safe(unsigned bank){
 return bank_lead(bank)>=COPY_LEAD_BYTES?1:fail(PEAQ_AUDIO_TIMEOUT);
}
static int publish(unsigned volume,unsigned silence){
 unsigned bank=audio.next%BANKS;
 if(!guards()||audio.r.poisoned)return fail(PEAQ_AUDIO_GUARD);
 if(audio.r.running&&audio.next>=audio.r.completed+BANKS)return fail(PEAQ_AUDIO_ORDER);
 if(silence){for(unsigned i=0;i<SAMPLES;++i)audio.samples[i]=0;++audio.r.silence_blocks;}
 else if(!peaq_emerald_audio_pull(&audio.stream,audio.samples,800,volume)||
         !peaq_emerald_audio_pull(&audio.stream,audio.samples+800,800,volume))return fail(PEAQ_AUDIO_GUARD);
 /* PCM is prepared in CPU-only staging before the second ownership check.
  * No descriptors are modified while either lane is enabled. */
 if(audio.r.running&&!copy_safe(bank))return 0;
 uint32_t begin=now();
 for(unsigned i=0;i<SAMPLES;++i){
  int32_t s=audio.samples[i];uint32_t m=(uint32_t)(s<0?-s:s);
  if(m>audio.r.pcm_peak)audio.r.pcm_peak=m;
  audio_dma.pcm[bank][i]=(int16_t)s;
 }
 barrier();
 if(now()-begin>=16u)return fail(PEAQ_AUDIO_TIMEOUT);
 uint32_t t=now();
 if(audio.next){
  uint32_t interval=t-audio.delivery[4];audio.delivery[5]+=interval;
  if(audio.next==1||interval<audio.delivery[6])audio.delivery[6]=interval;
  if(interval>audio.delivery[7])audio.delivery[7]=interval;
  if(interval>110u)++audio.delivery[8];
 }else audio.delivery[3]=t;
 audio.delivery[4]=t;++audio.next;
 audio.r.submitted=audio.next;audio.r.submitted_samples+=SAMPLES;
 return 1;
}
static int launch(void){
 if(audio.r.running||audio.r.owned_mask||audio.next!=BANKS||!guards())return fail(PEAQ_AUDIO_ORDER);
 for(unsigned side=0;side<2;++side){
  uint32_t b=ch(side);if(R(b+8)&1u)return fail(PEAQ_AUDIO_BUSY);
  for(unsigned j=0;j<BANKS;++j){
   uint32_t *d=audio_dma.descriptors[side][j];
   d[0]=d[1]=0;d[2]=0x01003010u;d[3]=0x1f000014u;
   d[4]=(uintptr_t)audio_dma.pcm[j];d[5]=side?0x40021000u:0x40021004u;
   d[6]=0x51300140u;d[7]=320;d[8]=BYTES;d[9]=2;
   d[10]=d[11]=d[13]=d[14]=d[15]=0;
   d[12]=(uintptr_t)&audio_dma.descriptors[side][(j+1u)%BANKS][4];
  }
  for(unsigned j=0;j<16;++j)R(b+j*4)=audio_dma.descriptors[side][0][j];
  /* Initial linked-list fetch starts with descriptor0; subsequent pointers
   * in the immutable descriptors form0->1->2->0. */
  R(b+48)=(uintptr_t)&audio_dma.descriptors[side][0][4];
 }
 barrier();audio.r.owned_mask=3;audio.r.completion_mask=0;
 uint32_t t=now();audio.r.dma_started_ms=audio.cycle_poll_ms=t;
 audio.lane_progress_ms[0]=audio.lane_progress_ms[1]=t;
 R(ch(0)+8)=0x01003011u;R(ch(1)+8)=0x01003011u;barrier();
 R(0x40020018)|=0x300u;barrier();audio.r.running=1;return 1;
}
static int service(unsigned volume,unsigned drain,uint32_t t){
 if(!progress(t))return 0;
 if(!audio.r.running){
  while(audio.next<BANKS&&(audio.stream.count>=SAMPLES||drain)){
   if(!publish(volume,drain&&!audio.stream.count))return 0;
  }
  if(audio.next==BANKS&&!launch())return 0;
 }
 if(audio.r.running)while(audio.next<audio.r.completed+BANKS&&!audio.quiet_until){
  /* Retirement makes a bank writable, not immediately due. Give the game
   * time to supply its next PCM instead of inserting avoidable silence. */
  if(!drain&&audio.stream.count<SAMPLES&&bank_lead(audio.next%BANKS)>4608u)break;
  unsigned quiet=drain&&!audio.stream.count;
  if(drain&&!quiet)++audio.r.drain_blocks;
  if(!publish(volume,quiet))return 0;
  if(quiet)audio.quiet_until=audio.next;
 }
 return 1;
}
