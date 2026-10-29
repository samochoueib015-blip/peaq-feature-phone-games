#include "landscape.h"
void peaq_landscape_fit(uint16_t *out,const uint16_t *src,int sw,int sh){
 if(!out||!src||sw<1||sh<1||sw>640||sh>480)return;
 int dw=320,dh=sh*320/sw;
 if(dh>240){dh=240;dw=sw*240/sh;}
 if(dw<1)dw=1;if(dh<1)dh=1;
 int ox=(320-dw)/2,oy=(240-dh)/2;
 for(int x=0;x<320;x++)for(int y=0;y<240;y++){
  uint16_t c=0;if(x>=ox&&x<ox+dw&&y>=oy&&y<oy+dh)c=src[((y-oy)*sh/dh)*sw+(x-ox)*sw/dw];
  out[x*240+239-y]=c;
 }
}
uint32_t peaq_landscape_dpad(uint32_t b){
 return (b&~15u)|((b&PORT_UP)?PORT_LEFT:0)|((b&PORT_DOWN)?PORT_RIGHT:0)|
        ((b&PORT_LEFT)?PORT_DOWN:0)|((b&PORT_RIGHT)?PORT_UP:0);
}
