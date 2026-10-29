#include "platform.h"
#include "doomgeneric.h"
#include "doomkeys.h"
static const DoomHost *host;
static uint16_t *frame;
static uint32_t desired,reported,head,tail,overflow;
static uint16_t queue[64];
static const uint8_t keys[11]={KEY_UPARROW,KEY_DOWNARROW,KEY_STRAFE_L,KEY_STRAFE_R,0,KEY_FIRE,KEY_LEFTARROW,KEY_RIGHTARROW,KEY_USE,KEY_ESCAPE,KEY_ENTER};
/* PORT_OK is reserved for a future jumping-capable engine; vanilla Doom
 * cannot jump. Do not silently turn the requested jump button into Use. */
int peaq_doom_bind(const DoomHost *h,uint16_t *buffer){
 if(!h||!buffer||!h->milliseconds||!h->sleep_ms||!h->present)return 0;
 host=h;frame=buffer;desired=reported=head=tail=overflow=0;return 1;
}
void peaq_doom_buttons(uint32_t b){
 b&=0x7efu;uint32_t changed=b^desired;desired=b;
 for(unsigned i=0;i<11;i++)if(changed&(1u<<i)){
  unsigned next=(head+1)&63;
  if(next==tail){head=tail=0;overflow++;return;}
  queue[head]=(uint16_t)(i|((b&(1u<<i))?0x100:0));head=next;
 }
}
uint32_t peaq_doom_overflows(void){return overflow;}
void DG_Init(void){desired=reported=head=tail=0;}
int DG_GetKey(int *pressed,unsigned char *key){
 if(!pressed||!key)return 0;
 unsigned index=0;int down;
 if(tail!=head){unsigned v=queue[tail];tail=(tail+1)&63;index=v&255;down=(v>>8)&1;}
 else {
  uint32_t changed=reported^desired;if(!changed)return 0;
  uint32_t releases=changed&reported;if(releases)changed=releases;
  while(!(changed&(1u<<index)))index++;down=(desired&(1u<<index))!=0;
 }
 if(down)reported|=1u<<index;else reported&=~(1u<<index);
 *pressed=down;*key=keys[index];return 1;
}
void DG_DrawFrame(void){
 if(!host||!frame||!DG_ScreenBuffer)return;
 for(int x=0;x<320;x++)for(int y=0;y<240;y++){
  uint32_t c=DG_ScreenBuffer[(y*DOOMGENERIC_RESY/240)*DOOMGENERIC_RESX+x*DOOMGENERIC_RESX/320];
  frame[x*240+239-y]=(uint16_t)(((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31));
 }
 host->present(frame,240,320);
}
uint32_t DG_GetTicksMs(void){return host?host->milliseconds():0;}
void DG_SleepMs(uint32_t ms){if(host)host->sleep_ms(ms);}
void DG_SetWindowTitle(const char *title){(void)title;}
