/* Built beside a frozen generated fast reader. The writer itself is the
 * physically checked fixed-map implementation; no writable driver is exposed
 * until the fast load finishes and its clock is returned to divider64. */
#include "sd_game_save_base.c"
int peaq_sd_save_slow(PeaqSdReport *r){
 if(!r||r!=state||r->error||r->poisoned||!(r->flags&SD_F_CARD)||!cold_cpu())return 0;
 if((REG(0x2150006cu)&7u)!=1u||(SD(0x28)&0x3eu)||(SD(0x2c)&0xffc7u)!=0x407u)
  return save_fail(SD_CLOCK_TIMEOUT);
 if(!wait_reg(0x20300024u,3u,0,100,SD_INHIBIT_TIMEOUT))return save_fail(r->error);
 SD(0x2c)&=~4u;SD(0x2c)&=~1u;
 if(SD(0x2c)&5u)return save_fail(SD_CLOCK_TIMEOUT);
 SD(0x2c)=(SD(0x2c)&~0xffc0u)|0x4000u;SD(0x2c)|=1u;
 if(!wait_reg(0x2030002cu,2u,2u,100,SD_CLOCK_TIMEOUT))return save_fail(r->error);
 SD(0x2c)|=4u;
 if(!wait_reg(0x2030002cu,0xffc7u,0x4007u,100,SD_CLOCK_TIMEOUT))return save_fail(r->error);
 __asm__ volatile("dsb sy":::"memory");
 r->clock_control=SD(0x2c);r->controller_state=SD(0x24);return 1;
}
