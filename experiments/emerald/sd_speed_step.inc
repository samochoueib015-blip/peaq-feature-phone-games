/* Appended ONLY to the isolated generated SD driver copy. All helpers and
 * permanent DMA lifetime are the qualified driver's. No arbitrary divider,
 * width, voltage or source arguments; no additional card commands.
 */
int peaq_sd_fixed_speed(PeaqSdReport *r){
 state=r;
 if(r->error||r->poisoned)return 0;
 if(!(r->flags&SD_F_CARD))return fail(SD_NOT_READY);
 /* Existing SDR1-bit normal-speed setup and the original source selector
  * must match. Do not repair or try alternate values after an unexpected state.
  */
 if((REG(0x2150006cu)&7u)!=1u||(SD(0x28)&0x3eu)||
    (SD(0x2c)&0xffc7u)!=0x4007u){fail(SD_CLOCK_TIMEOUT);poison();return 0;}
 if(!wait_reg(0x20300024u,3u,0,100,SD_INHIBIT_TIMEOUT)){poison();return 0;}
 SD(0x2c)&=~4u;SD(0x2c)&=~1u;
 if(SD(0x2c)&5u){fail(SD_CLOCK_TIMEOUT);poison();return 0;}
 /* Code4 is observed from original 0x80cf69e8; clock source remains1.
  * Firmware and FPDoom disagree on division formula, so no exact Hz claim.
  */
 SD(0x2c)=(SD(0x2c)&~0xffc0u)|0x400u;
 SD(0x2c)|=1u;
 if(!wait_reg(0x2030002cu,2u,2u,100,SD_CLOCK_TIMEOUT)){poison();return 0;}
 SD(0x2c)|=4u;
 if(!wait_reg(0x2030002cu,0xffc7u,0x407u,100,SD_CLOCK_TIMEOUT)){poison();return 0;}
 __asm__ volatile("dsb sy" ::: "memory");
 r->clock_control=SD(0x2c);r->controller_state=SD(0x24);return 1;
}
