#include "game_controls.h"
#include "cold_key_input.h"
#include "peaq_key_matrix.h"
_Static_assert(sizeof(PeaqKeyInput)==104,"KIN1 state layout");
_Static_assert(sizeof(PeaqKeyFrame)==64,"frame layout");
_Static_assert(sizeof(PeaqKeySnapshot)==32,"snapshot layout");
#define A 1u
#define B 2u
#define SELECT 4u
#define START 8u
#define RIGHT 16u
#define LEFT 32u
#define UP 64u
#define DOWN 128u
#define R 256u
#define L 512u
static void zero(void *p,uint32_t n){uint8_t *b=p;while(n--)*b++=0;}
static int separate(const void *a,uint32_t n,const void *b,uint32_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x+n>=x&&y+m>=y&&(x+n<=y||y+m<=x);
}
static void bump(uint32_t *p){if(*p!=0xffffffffu)++*p;}
static uint32_t count4(uint32_t n){n&=15;return (n&1)+((n>>1)&1)+((n>>2)&1)+(n>>3);}
static void add(uint32_t *p,uint32_t n){while(n--)bump(p);}
uint32_t peaq_key_matrix_index(uint32_t b){return ((b&7u)<<3)|((b>>4)&7u);}
uint32_t peaq_key_logical_code(uint32_t i){return i<64?peaq_key_matrix[i]:0xffffu;}
uint32_t peaq_key_gba_code(uint32_t code){return peaq_emerald_game_key(code);}

static uint32_t host_code(uint32_t code){return code==9?PEAQ_KEY_HOST_BACK:code==8?PEAQ_KEY_HOST_OPTIONS:0;}
static uint32_t axes(uint32_t x){
 if((x&(UP|DOWN))==(UP|DOWN))x&=~(UP|DOWN);
 if((x&(LEFT|RIGHT))==(LEFT|RIGHT))x&=~(LEFT|RIGHT);
 return x;
}
static void level(PeaqKeyInput *s){
 uint32_t g=0,h=0;
 for(unsigned i=0;i<64;++i)if(s->held[i>>5]&(1u<<(i&31))){g|=peaq_key_gba_code(peaq_key_matrix[i]);h|=host_code(peaq_key_matrix[i]);}
 uint32_t normalized=axes(g);
 if(normalized!=g)s->flags|=PEAQ_KEY_OPPOSING_DIRECTIONS;
 s->gba_pressed|=normalized&~s->gba;s->gba_released|=s->gba&~normalized;s->gba=normalized;
 s->host_pressed|=h&~s->host;s->host_released|=s->host&~h;s->host=h;
}
static void change(PeaqKeyInput *s,uint32_t i,int pressed){
 unsigned word=i>>5;uint32_t bit=1u<<(i&31),old=s->held[word];
 if(pressed){s->held[word]|=bit;s->pressed[word]|=bit&~old;}
 else {s->held[word]&=~bit;s->released[word]|=bit&old;}
 level(s);
}
void peaq_key_input_init(PeaqKeyInput *s){if(s){zero(s,sizeof(*s));s->magic=PEAQ_KEY_INPUT_MAGIC;}}
void peaq_key_input_release_all(PeaqKeyInput *s){
 if(!s||s->magic!=PEAQ_KEY_INPUT_MAGIC)return;
 for(unsigned i=0;i<2;++i){s->released[i]|=s->held[i];s->held[i]=s->pressed[i]=0;}
 s->gba_released|=s->gba;s->host_released|=s->host;
 s->gba=s->host=s->gba_pressed=s->host_pressed=0;s->flags|=PEAQ_KEY_RESET;
 bump(&s->reset_count);bump(&s->generation);
}
static int invalid(PeaqKeyInput *s,uint32_t reason){s->flags|=reason;bump(&s->invalid_count);peaq_key_input_release_all(s);return 0;}
int peaq_key_input_feed(PeaqKeyInput *s,uint32_t raw,uint32_t status){
 if(!s||s->magic!=PEAQ_KEY_INPUT_MAGIC)return -1;
 s->raw=raw;s->status=status;bump(&s->samples);bump(&s->generation);
 if(raw&~0xfffu)return invalid(s,PEAQ_KEY_BAD_RAW);
 /* A nonexistent matrix position cannot silently strand another held key.
  * Long-only events do not decode a position, matching the original ISR. */
 for(unsigned i=0;i<4;++i)if((raw&(1u<<(i+4)))||(!(status&8u)&&(raw&(1u<<i))))
  if(peaq_key_logical_code(peaq_key_matrix_index(status>>(i*8)))==0xffffu)return invalid(s,PEAQ_KEY_BAD_POSITION);
 add(&s->long_count,count4(raw>>8));
 uint32_t simultaneous=(raw&(raw>>4))&15u;
 if(simultaneous){s->flags|=PEAQ_KEY_COALESCED;add(&s->coalesced_count,count4(simultaneous));}
 if((status&8u)&&(raw&15u)){s->flags|=PEAQ_KEY_PRESS_SUPPRESSED;add(&s->suppressed_count,count4(raw));}
 /* Original queue order: all four presses, then all four releases. Bit3
  * suppresses only the press half, never a pending release. */
 for(unsigned phase=0;phase<2;++phase)for(unsigned i=0;i<4;++i){
  if(!(raw&(1u<<(i+phase*4))))continue;
  if(!phase&&(status&8u))continue;
  uint32_t index=peaq_key_matrix_index(status>>(i*8));
  change(s,index,!phase);bump(phase?&s->release_count:&s->press_count);
 }
 return 1;
}
int peaq_key_input_take(PeaqKeyInput *s,PeaqKeyFrame *f){
 if(!s||!f||s->magic!=PEAQ_KEY_INPUT_MAGIC||!separate(s,sizeof(*s),f,sizeof(*f)))return 0;
 zero(f,sizeof(*f));f->generation=s->generation;f->flags=s->flags;
 for(unsigned i=0;i<2;++i){f->held[i]=s->held[i];f->pressed[i]=s->pressed[i];f->released[i]=s->released[i];s->pressed[i]=s->released[i]=0;}
 f->gba=s->gba;f->gba_pressed=s->gba_pressed;f->gba_released=s->gba_released;
 f->host=s->host;f->host_pressed=s->host_pressed;f->host_released=s->host_released;
 /* A complete tap between game frames becomes one active-high GBA pulse.
  * Currently held direction wins over an older opposite short-tap edge. */
 uint32_t g=s->gba|s->gba_pressed;
 if(s->gba&(UP|DOWN))g=(g&~(UP|DOWN))|(s->gba&(UP|DOWN));
 if(s->gba&(LEFT|RIGHT))g=(g&~(LEFT|RIGHT))|(s->gba&(LEFT|RIGHT));
 f->emerald_buttons=axes(g)&0x3ffu;
 s->gba_pressed=s->gba_released=s->host_pressed=s->host_released=0;
 return 1;
}
int peaq_key_capture(const PeaqKeyRead *r,PeaqKeySnapshot *s){
 if(!s||(r&&!separate(r,sizeof(*r),s,sizeof(*s))))return -3;
 zero(s,sizeof(*s));s->magic=PEAQ_KEY_SNAPSHOT_MAGIC;
 if(!r||!r->read){s->error=3;return -3;}
 for(unsigned i=0;i<4;++i){
  uint32_t a,b,c,d;++s->attempts;
  ++s->reads;if(r->read(r->cookie,PEAQ_KEY_RAW,&a)!=1){s->error=1;return -1;}
  ++s->reads;if(r->read(r->cookie,PEAQ_KEY_STATUS,&b)!=1){s->error=1;return -1;}
  ++s->reads;if(r->read(r->cookie,PEAQ_KEY_RAW,&c)!=1){s->error=1;return -1;}
  ++s->reads;if(r->read(r->cookie,PEAQ_KEY_STATUS,&d)!=1){s->error=1;return -1;}
  s->raw=c;s->status=d;
  if((a|c)&~0xfffu){s->error=4;return -4;}
  if(!a&&!c){s->flags=2;return 0;}
  if(a!=c||b!=d)continue;
  s->flags=1;s->ack_mask=0xfff;return 1;
 }
 s->error=2;return -2;
}
