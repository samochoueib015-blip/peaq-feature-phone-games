/* EPR1 revision8: read-only comparison of the VBC signal path and SC2720
 * audio pads. Clock/MMU/ownership checks and the 22-word output size are
 * unchanged. No mixer, pad, gain, clock or reset setting is written here. */
int peaq_cold_audio_extra(uint32_t out[22]){
 static const uint32_t analog[]={0x40608710u,0x40608714u,0x40608c08u,0x40608c0cu,0x40608c14u};
 static const uint32_t digital[]={0x40020004u,0x40020008u,0x40020014u,0x40020038u,
  0x40020040u,0x40020044u,0x40020048u,0x40020094u,0x40020098u};
 static const uint32_t pads[]={0x40608628u,0x4060862cu,0x40608630u,0x40608634u,
  0x40608638u,0x4060863cu,0x40608e08u};
 if(!out||!context(1)||!audio.r.initialized||!audio.r.powered||audio.r.stopped||!guards())return 0;
 out[21]=0;
 for(unsigned i=0;i<5;++i){
  if(!adi_read(analog[i],out+i))return 0;
  if((R(0x4060002c)>>16)!=((analog[i]>>2)&0x7fffu))out[21]|=1u<<i;
 }
 for(unsigned i=0;i<9;++i)out[5+i]=R(digital[i]);
 for(unsigned i=0;i<7;++i){
  if(!adi_read(pads[i],out+14+i))return 0;
  if((R(0x4060002c)>>16)!=((pads[i]>>2)&0x7fffu))out[21]|=1u<<(5+i);
 }
 return 1;
}
