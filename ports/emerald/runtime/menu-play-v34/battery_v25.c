#include <stdint.h>
#include "battery_v25.h"
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define FGU_CODES_PER_VOLT 674u
/* The original battery sampler at 80a9b502 takes three 40608A20 readings,
 * then uses (median*1000 + calibration/2)/calibration. Calibration674 is
 * captured from this unit's initialized 81734324, not a generic PMIC value.
 * Readiness fields are corroborated by the pinned SC2720 FGU driver;
 * see BATTERY_V25.md. Unsupported/unready measurements stay unavailable. */
static int read_adi(unsigned offset,uint32_t *out){
 uint32_t address=0x40608000u+offset,started=R(0x4023000cu),v;
 /* A preceding synchronous ADI operation must have finished. Do not
  * replace a busy request from an unexpectedly active owner. */
 if(R(0x4060002cu)&0x80000000u)return 0;
 R(0x40600028u)=address;
 __asm__ volatile("dsb sy":::"memory");
 for(unsigned i=0;i<2048;++i){
  v=R(0x4060002cu);
  if(!(v&0x80000000u)){
   if(((v>>16)&0x7fffu)!=((address>>2)&0x7fffu))return 0;
   *out=v&65535u;return 1;
  }
  if(R(0x4023000cu)-started>=2u)return 0;
 }
 return 0;
}
unsigned peaq_battery_millivolts(void){
 uint32_t control,psr;
 __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(control));
 __asm__ volatile("mrs %0,cpsr":"=r"(psr));
 if((control&0x1005u)||((psr&255u)!=0xdfu&&(psr&255u)!=0xd3u))return 0;
 static const uint16_t addresses[]={0xc00,0xc04,0xc08,0xc10,0xc14,0xe18,0xa04,0xa08,0xa0c,0xa18};
 uint32_t v[10],raw[3];
 for(unsigned i=0;i<10;++i)if(!read_adi(addresses[i],v+i))return 0;
 if(v[0]!=0xa003u||v[1]!=0x2720u||!(v[2]&0x80u)||!(v[3]&0x40u)||
    (v[4]&0x10u)||(v[5]&0x3000u)||(v[6]&0x1800u)||(v[7]&3u)||
    !(v[8]&0x40u)||!(v[9]&0x40u))return 0;
 for(unsigned i=0;i<3;++i)
  if(!read_adi(0xa20,raw+i)||!raw[i]||(raw[i]&0xf000u))return 0;
 for(unsigned i=0;i<2;++i)for(unsigned j=i+1;j<3;++j)
  if(raw[i]>raw[j]){uint32_t t=raw[i];raw[i]=raw[j];raw[j]=t;}
 unsigned mv=(raw[1]*1000u+FGU_CODES_PER_VOLT/2u)/FGU_CODES_PER_VOLT;
 return mv>=2500u&&mv<=5000u?mv:0;
}
