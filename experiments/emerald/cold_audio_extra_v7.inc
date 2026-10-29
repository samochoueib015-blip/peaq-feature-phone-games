/* Read-only extension, called only before stopping the owned codec with
 * caches/MMU off. Every address already belongs to the qualified original
 * masked power/clock/DAC profile or its original register dump. No additional analog setting is applied. */
int peaq_cold_audio_extra(uint32_t out[22]){
 static const uint32_t analog[]={0x40608700u,0x40608704u,0x4060870cu,0x40608710u,
  0x40608714u,0x4060873cu,0x4060877cu,0x40608838u,
  0x40608c08u,0x40608c0cu,0x40608c14u};
 static const uint32_t digital[]={0x40000008u,0x40000010u,0x40000014u,0x40000038u,0x402d025cu};
 /* Original board pin table; these are observed, never configured here. */
 static const uint32_t pads[]={0x4060860cu,0x40608610u,0x40608614u,0x40608618u,0x40608600u};
 if(!out||!context(1)||!audio.r.initialized||!audio.r.powered||audio.r.stopped||!guards())return 0;
 out[21]=0;
 for(unsigned i=0;i<11;++i){
  if(!adi_read(analog[i],out+i))return 0;
  if((R(0x4060002c)>>16)!=((analog[i]>>2)&0x7fffu))out[21]|=1u<<i;
 }
 for(unsigned i=0;i<5;++i)out[11+i]=R(digital[i]);
 for(unsigned i=0;i<5;++i){
  if(!adi_read(pads[i],out+16+i))return 0;
  if((R(0x4060002c)>>16)!=((pads[i]>>2)&0x7fffu))out[21]|=1u<<(11+i);
 }
 return 1;
}
