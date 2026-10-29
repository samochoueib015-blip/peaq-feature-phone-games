/* Pixel-identical specialization of the fixed GBA landscape fit. No device
 * or DMA access. The fallback preserves every other caller's geometry. */
#define peaq_landscape_fit peaq_landscape_generic
#define peaq_landscape_dpad peaq_landscape_generic_dpad
#include "../common/landscape.c"
#undef peaq_landscape_fit
#undef peaq_landscape_dpad

void peaq_landscape_fit(uint16_t *out,const uint16_t *src,int sw,int sh){
 if(sw!=240||sh!=160){peaq_landscape_generic(out,src,sw,sh);return;}
 if(!out||!src)return;
 /* Logical320x213 centered vertically: top13/bottom14 black pixels.
  * Physical output is 240x320 after the90-degree counterclockwise turn. */
 for(unsigned x=0;x<320;x++){
  uint16_t *column=out+x*240;
  for(unsigned y=0;y<14;y++)column[y]=0;
  for(unsigned y=227;y<240;y++)column[y]=0;
 }
 for(unsigned y=0;y<213;y++){
  const uint16_t *s=src+((212u-y)*160u/213u)*240u;
  uint16_t *d=out+14u+y;
  /* Horizontal4:3 expansion: each group samples indices0,0,1,2.
   * One row division, three source reads and four destination writes. */
  for(unsigned group=0;group<80;group++,s+=3,d+=960){
   uint16_t a=s[0],b=s[1],c=s[2];
   d[0]=a;d[240]=a;d[480]=b;d[720]=c;
  }
 }
}
uint32_t peaq_landscape_dpad(uint32_t b){return peaq_landscape_generic_dpad(b);}
