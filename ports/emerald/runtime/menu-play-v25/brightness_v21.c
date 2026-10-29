#include "brightness_v21.h"
int peaq_brightness_input(PeaqEmeraldVolume *v,PeaqKeyInput *input,uint32_t now){
 if(!v||!input||input->magic!=PEAQ_KEY_INPUT_MAGIC)return -1;
 uint32_t held=(input->held[1]>>1)&3u,pressed=(input->pressed[1]>>1)&3u;
 uint32_t before=v->level;
 int result=peaq_emerald_volume_poll(v,held,pressed,now);
 if(result<0)return result;
 input->pressed[1]&=~6u;
 if(!v->level)v->level=1;
 return v->level!=before;
}
