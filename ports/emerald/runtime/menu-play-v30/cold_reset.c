#include <stdint.h>
#include "cold_reset.h"
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
static int wait(uint32_t address,uint32_t mask,uint32_t value){
 uint32_t start=R(0x4023000c);
 for(unsigned i=0;i<1000000;++i){if((R(address)&mask)==value)return 1;if(R(0x4023000c)-start>=20)return 0;}
 return 0;
}
static int write_adi(uint32_t a,uint32_t v){
 if(!wait(0x40600030,0x800,0))return 0;R(a)=v&65535u;__asm__ volatile("dsb sy":::"memory");return 1;
}
static int update(uint32_t a,uint32_t clear,uint32_t set){
 R(0x40600028)=a;if(!wait(0x4060002c,0x80000000u,0))return 0;
 return write_adi(a,((R(0x4060002c)&65535u)&~clear)|set);
}
static int bits(uint32_t a,uint32_t set){return update(a,0,set);}
int peaq_cold_reset(void){
 uint32_t s;__asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(s));
 if(s&5u)return 0;
 __asm__ volatile("cpsid if\ndsb sy":::"memory");
 /* The stock caller80a9b02a sets reason8 in bits8:4 before its watchdog
  * reset. Its automatic restart is physically confirmed on this handset;
  * clearing the reason instead powered it off. Preserve all other bits.
  */
 if(!update(0x40608e24,0x1f0,0x80))return 0;
 /* UMS9117/SC2720 watchdog sequence; no GPIO or voltage changes. */
 if(!bits(0x40608c08,4)||!bits(0x40608c10,4)||!write_adi(0x40608060,0xe551))return 0;
 for(volatile unsigned i=0;i<10000;++i){}
 if(!bits(0x40608048,9)||!bits(0x40608048,4)||
    !write_adi(0x40608040,16384)||!write_adi(0x40608044,0)||
    !bits(0x40608048,2)||!write_adi(0x40608060,0x1aae))return 0;
 for(;;)__asm__ volatile("wfi");
}
