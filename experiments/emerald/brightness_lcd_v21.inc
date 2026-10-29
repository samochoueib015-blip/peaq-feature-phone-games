/* Original four-channel LED PWM; retain its current and period. Only duty
 * changes. No panel gamma, analog-current or power-supply changes. */
int peaq_lcd_brightness(unsigned level){
 if(!state||state->error||!(state->flags&LCD_F_LIT)||level<1||level>8)return 0;
 unsigned duty=50u*level/8u,value=(duty<<8)|50u;
 for(unsigned i=0;i<3;++i)if(!adi_write(0x40608188u+16u*i,value))return 0;
 if(!adi_write(0x406081ccu,value))return 0;
 state->backlight_percent=duty*2u;return 1;
}
