/* Included in cold_audio.c. Read-only snapshot of the actual DMA buffer and
 * digital interface. Does not fetch a FIFO, alter a register, or submit data.
 * The uncached outer loop has stopped calling pump; DMA can only read PCM. */
int peaq_cold_audio_signal(uint32_t out[31],int16_t pcm[128]){
 static const uint32_t registers[]={
  0x40000004u,0x40000008u,0x40000010u,0x40000014u,0x40000018u,
  0x4000001cu,0x40000020u,0x40000028u,0x4000002cu,0x40000030u,
  0x40000034u,0x40000038u,0x4000003cu,0x402d025cu,0x402e0020u,
  0x400200ecu,0x40020044u,0x40020048u,0x40020250u,0x40020254u};
 if(!out||!pcm||!context(1)||!audio.r.initialized||!audio.r.powered||
    audio.r.stopped||!guards()||audio.r.channels[0]>=32||audio.r.channels[1]>=32)return 0;
 for(unsigned i=0;i<20;++i)out[i]=R(registers[i]);
 out[20]=R(ch(0)+12);out[21]=R(ch(1)+12);
 out[22]=R(ch(0)+16);out[23]=R(ch(1)+16);
 int32_t minimum=32767,maximum=-32768,sum=0,previous=0;
 uint32_t nonzero=0,crossings=0,peak=0,at=0,hash=2166136261u;
 for(unsigned i=0;i<SAMPLES;++i){
  int32_t s=audio.samples[i];uint32_t magnitude=(uint32_t)(s<0?-s:s);
  if(s<minimum)minimum=s;if(s>maximum)maximum=s;sum+=s;
  if(s){++nonzero;if((s<0&&previous>0)||(s>0&&previous<0))++crossings;previous=s;}
  if(magnitude>peak){peak=magnitude;at=i;}
  uint16_t raw=(uint16_t)s;hash=(hash^(raw&255u))*16777619u;
  hash=(hash^(raw>>8))*16777619u;
 }
 unsigned offset=at>63?at-63:0;if(offset>SAMPLES-128)offset=SAMPLES-128;
 out[24]=(uint32_t)minimum;out[25]=(uint32_t)maximum;out[26]=(uint32_t)sum;
 out[27]=nonzero;out[28]=crossings;out[29]=hash;out[30]=offset;
 for(unsigned i=0;i<128;++i)pcm[i]=audio.samples[offset+i];
 return 1;
}
