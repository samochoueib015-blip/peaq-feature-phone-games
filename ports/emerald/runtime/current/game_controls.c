#include "game_controls.h"
#define MAGIC 0x314d5645u
uint32_t peaq_emerald_game_key(uint32_t code){
 /* Physical labels, phone rotated 90 degrees counterclockwise. */
 switch(code){
 case 4:return 32;case 5:return 16;case 6:return 128;case 7:return 64;
 case '8':case 0x0d:return 1;
 case '7':return 2;case '*':return 4;case '#':return 8;
 case '5':return 512;case '0':return 256;
 default:return 0;
 }
}
int peaq_emerald_volume_init(PeaqEmeraldVolume *v,uint32_t level,uint32_t now){
 if(!v||((uintptr_t)v&3u)||level>PEAQ_EMERALD_VOLUME_MAX)return 0;
 v->magic=MAGIC;v->level=level;v->held=0;v->next_repeat=now;
 v->last_ms=now;v->changed=0;v->error=0;return 1;
}
int peaq_emerald_volume_poll(PeaqEmeraldVolume *v,uint32_t held,uint32_t pressed,uint32_t now){
 if(!v||((uintptr_t)v&3u)||v->magic!=MAGIC||v->error)return -1;
 if((held|pressed)&~3u||(now-v->last_ms)&0x80000000u){v->held=0;v->error=1;return -1;}
 v->last_ms=now;
 /* A complete between-polls tap is retained; a currently held opposite key
  * wins over that older edge, just like the game direction decoder. */
 uint32_t key=held?held:pressed,step=0;
 if(key==1||key==2){
  if(key!=v->held||(pressed&key)){step=1;v->next_repeat=now+400;}
  else if(!((now-v->next_repeat)&0x80000000u)){step=1;v->next_repeat=now+125;}
 }
 v->held=held;
 if(!step)return 0;
 uint32_t before=v->level;
 if(key==1){if(v->level<PEAQ_EMERALD_VOLUME_MAX)++v->level;}
 else if(v->level)--v->level;
 if(before!=v->level){v->changed=1;return 1;}
 return 0;
}
int peaq_emerald_volume_input(PeaqEmeraldVolume *v,PeaqKeyInput *input,uint32_t now){
 if(!v||!input||((uintptr_t)v&3u)||((uintptr_t)input&3u)||input->magic!=PEAQ_KEY_INPUT_MAGIC)return -1;
 uintptr_t a=(uintptr_t)v,b=(uintptr_t)input;
 if(a+sizeof(*v)<a||b+sizeof(*input)<b||
    !(a+sizeof(*v)<=b||b+sizeof(*input)<=a))return -1;
 uint32_t held=(input->held[1]>>8)&3u,pressed=(input->pressed[1]>>8)&3u;
 int result=peaq_emerald_volume_poll(v,held,pressed,now);
 if(result>=0)input->pressed[1]&=~0x300u;
 return result;
}
void peaq_emerald_volume_release(PeaqEmeraldVolume *v){
 if(v&&!((uintptr_t)v&3u)&&v->magic==MAGIC)v->held=0;
}
int16_t peaq_emerald_volume_sample(int16_t sample,uint32_t level){
 /* Approximately 6 dB increments, with a separate exact-mute level. */
 static const uint16_t gain[9]={0,128,256,512,1024,2048,4096,8192,16384};
 if(level>PEAQ_EMERALD_VOLUME_MAX)return 0;
 int32_t value=((int32_t)sample*gain[level])/16384;
 int32_t magnitude=value<0?-value:value;
 magnitude=magnitude<=18000?magnitude*3/2:27000+(magnitude-18000)/4;
 return (int16_t)(value<0?-magnitude:magnitude);
}
