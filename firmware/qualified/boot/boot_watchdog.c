/* Exact original boot1 watchdog stop masks, with bounded ADI transport.
 * The previous control values are restored on every stock-OS fallback.
 */
#include "boot_watchdog.h"
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
static void barrier(void){__asm__ volatile("dsb sy":::"memory");}
static int wait(uint32_t a,uint32_t mask){
 uint32_t start=R(0x4023000c);
 for(unsigned i=0;i<100000;++i){if(!(R(a)&mask))return 1;if(R(0x4023000c)-start>=20)break;}return 0;
}
static int read(uint32_t a,uint32_t *v){
 R(0x40600028)=a;if(!wait(0x4060002c,0x80000000u))return 0;*v=R(0x4060002c)&65535u;return 1;
}
static int write(uint32_t a,uint32_t v){if(!wait(0x40600030,0x800u))return 0;R(a)=v&65535u;barrier();return 1;}
static int pmic(uint32_t value){
 if(!write(0x40608060,0xe551)||!write(0x40608048,value)||!write(0x40608060,0x1aae))return 0;
 uint32_t got;return read(0x40608048,&got)&&got==value;
}
static int ap(uint32_t value){
 R(0x40310020)=0xe551;R(0x40310008)=value;R(0x40310020)=0xffff1aaeu;barrier();return R(0x40310008)==value;
}
int peaq_boot_watchdog_suspend(PeaqBootWatchdog *s){
 uint32_t cpu;__asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(cpu));
 if(!s||cpu&0x1005u||R(0x402e00fc)!=0x98180001u)return 0;
 s->changed=0;if(!read(0x40608048,&s->pmic))return 0;
 /* Original boot1's AP-watchdog init enables AON_APB_EB1 bit8 through
  * 0x402e1004 before touching 0x40310000. A normal boot is not the FDL
  * fixture: the AP register bank may be clock-gated. Do not probe or enable
  * that bank when its existing clock is off. The always-on gate is readable.
  */
 s->ap=(R(0x402e0004)&0x100u)?R(0x40310008):0;
 if(s->pmic&2u){s->changed|=1;if(!pmic(s->pmic&~2u))return 0;}
 if(s->ap&0xbu){s->changed|=2;if(!ap(s->ap&~0xbu))return 0;}
 return 1;
}
int peaq_boot_watchdog_restore(PeaqBootWatchdog *s){
 if(!s)return 0;int ok=1;
 if(s->changed&2u){if(ap(s->ap))s->changed&=~2u;else ok=0;}
 if(s->changed&1u){if(pmic(s->pmic))s->changed&=~1u;else ok=0;}
 return ok;
}
