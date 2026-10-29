/* SC2720 shared 26MHz request. Same single-bit operation as the qualified
 * preclock RAM probe. Called before resets and after complete audio shutdown.
 * The previous request owner and all unrelated PMIC fields are preserved.
 */
static int crystal_read(uint32_t *value){
 const uint32_t address=0x40608de8u;
 if(!adi_read(address,value))return 0;
 if(((R(0x4060002cu)>>16)&0x7fffu)!=((address>>2)&0x7fffu))return fail(PEAQ_AUDIO_ADI);
 return 1;
}
static int crystal_acquire(void){
 uint32_t value,after;
 if(audio.crystal_owned||!audio.r.vbc_owned||audio.r.powered||audio.r.initialized||
    audio.r.running||!guards())return fail(PEAQ_AUDIO_ORDER);
 if(!crystal_read(&value))return 0;
 if(value&0x100u)return 1;
 audio.crystal_owned=1;
 if(!adi_write(0x40608de8u,value|0x100u)||!crystal_read(&after))return 0;
 if(after!=(value|0x100u))return fail(PEAQ_AUDIO_ROUTE);
 return delay(5);
}
static int crystal_release(void){
 uint32_t value,after;
 if(!audio.crystal_owned)return 1;
 if(audio.r.owned_mask||audio.r.running||audio.r.powered||audio.r.vbc_owned)
  return fail(PEAQ_AUDIO_ORDER);
 if(!crystal_read(&value)||!adi_write(0x40608de8u,value&~0x100u)||!crystal_read(&after))return 0;
 if(after!=(value&~0x100u))return fail(PEAQ_AUDIO_ROUTE);
 audio.crystal_owned=0;
 return 1;
}
