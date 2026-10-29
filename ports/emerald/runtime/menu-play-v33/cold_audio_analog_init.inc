/* Shared analog setup performed by the original OS before WAV playback.
 * Original leaves: 80aabf00(1), 80aabe0c(1), 80aab6e2(1), 80aab6ea(1).
 * These masked fields also appear in the independent SC2720 codec startup.
 * No headphone routing, PA gain, DAC gain or calibration data is changed.
 */
static const uint32_t analog_addresses[3]={0x40608838u,0x4060873cu,0x40608700u};
static const uint32_t analog_masks[3]={4u,2u,0x58u};
static const uint32_t analog_values[3]={4u,2u,0x48u};
static int analog_read(unsigned i,uint32_t *value){
 uint32_t address=analog_addresses[i];
 if(!adi_read(address,value))return 0;
 if(((R(0x4060002cu)>>16)&0x7fffu)!=((address>>2)&0x7fffu))return fail(PEAQ_AUDIO_ADI);
 return 1;
}
static int analog_setup(void){
 if(audio.analog_owned||!audio.r.vbc_owned||audio.r.initialized||audio.r.running||!guards())
  return fail(PEAQ_AUDIO_ORDER);
 for(unsigned i=0;i<3;++i){
  uint32_t value,after;if(!analog_read(i,&value))return 0;
  audio.analog_before[i]=value&analog_masks[i];
  if((value&analog_masks[i])==analog_values[i])continue;
  audio.analog_owned|=1u<<i;
  uint32_t wanted=(value&~analog_masks[i])|analog_values[i];
  if(!adi_write(analog_addresses[i],wanted)||!analog_read(i,&after))return 0;
  if(after!=wanted)return fail(PEAQ_AUDIO_ROUTE);
 }
 return 1;
}
static int analog_verify(void){
 for(unsigned i=0;i<3;++i){
  uint32_t value;if(!analog_read(i,&value))return 0;
  if((value&analog_masks[i])!=analog_values[i])return fail(PEAQ_AUDIO_ROUTE);
 }
 return 1;
}
static int analog_release(void){
 if(!audio.analog_owned)return 1;
 if(audio.r.running||audio.r.owned_mask||audio.r.powered||audio.r.vbc_owned)
  return fail(PEAQ_AUDIO_ORDER);
 /* Reverse acquisition order, after the amplifier and DAC have stopped. */
 for(unsigned j=3;j;--j){
  unsigned i=j-1;if(!(audio.analog_owned&(1u<<i)))continue;
  uint32_t value,after;if(!analog_read(i,&value))return 0;
  uint32_t wanted=(value&~analog_masks[i])|audio.analog_before[i];
  if(!adi_write(analog_addresses[i],wanted)||!analog_read(i,&after))return 0;
  if(after!=wanted)return fail(PEAQ_AUDIO_ROUTE);
  audio.analog_owned&=~(1u<<i);
 }
 return 1;
}
