/* Small stock-OS front end; PLAY.BIN and the complete engine remain on SD.
 * The generated profile pins the installed early-boot bridge and payload.
 * The RAM marker carries the installed bridge's compatibility token. The
 * separate SD ticket carries the actual PLAY.BIN checksum; it must change
 * with each SD update. No payload checksum is substituted or bypassed.
 */
#include "pga.h"
#include "game_ui.h"
#include "menu_profile.h"
#include "boot_ticket.h"
#include "boot_ticket.c"
#include "retained_request.c"
typedef struct {u32 screen,selected,page,age,error,pending;u8 ticket[492];u32 owned,marker;} State;
const u32 pga_state_bytes=sizeof(State),pga_tick_ms=100,pga_required_abi=3;
enum {HOME,STARTING=4};
#define R(a) (*(volatile u32 *)(a))
static const char owner[]="peaq_menu_request";
typedef void *(*Allocate)(u32,const char *,u32);
typedef void (*Release)(void *,const char *,u32);
static void clean(u32 address){
 for(u32 p=address;p<address+64;p+=32)
  __asm__ volatile("mcr p15,0,%0,c7,c10,1"::"r"(p):"memory");
 __asm__ volatile("dsb sy":::"memory");
}
static void release(State *s){
 if(!s->owned)return;
 R(s->marker)=0;clean(s->marker);
 ((Release)0x88090850u)((void *)s->owned,owner,1);s->owned=s->marker=0;
}
static int reserve(State *s){
 u32 cpsr,ctr;__asm__ volatile("mrs %0,cpsr":"=r"(cpsr));
 if((cpsr&31u)==16u)return 0;
 __asm__ volatile("mrc p15,0,%0,c0,c0,1":"=r"(ctr));
 u32 line=(ctr>>16)&15u;if(line!=3&&line!=4)return 0;
 if(R(0x8178fcccu)!=0x42595445u||R(0x8178fce4u)!=PEAQ_REQUEST_POOL_FIRST||
    R(0x8178fce8u)!=PEAQ_REQUEST_POOL_END-PEAQ_REQUEST_POOL_FIRST)return 0;
 u32 p=(u32)((Allocate)0x88090848u)(PEAQ_REQUEST_ALLOCATION,owner,1);
 if(!p)return 0;
 u32 marker=(p+32768u+4095u)&~4095u;
 if(!peaq_request_owned(p,PEAQ_REQUEST_ALLOCATION,marker)){
  ((Release)0x88090850u)((void *)p,owner,1);return 0;
 }
 s->owned=p;s->marker=marker;R(marker)=0;clean(marker);return 1;
}
static void publish(State *s){
 PeaqRetainedRequest request;
 u32 nonce=s->owned^s->marker^PEAQ_MENU_PLAY_FNV;if(!nonce)nonce=1;
 peaq_request_make(&request,PEAQ_REQUEST_MAGIC,s->marker,PEAQ_MENU_GATE_PROFILE,nonce);
 /* Publish the magic last. The backing file has already been read back. */
 for(unsigned i=1;i<16;++i)((volatile u32 *)s->marker)[i]=request.word[i];
 R(s->marker)=request.word[0];clean(s->marker);
}
static int installed(void){
 /* The early extension can later be reused by the OS's BSS/heap. Check
  * its persistent kernel header, not extension bytes after OS startup. */
 return R(0x80a06008u)==PEAQ_MENU_BRIDGE&&R(0x80a05e30u)==PEAQ_MENU_OS_BODY_BYTES;
}
static void paint(const PgaApi *a,State *s){
 ui_background(a,0x05ef);center(a,28,3,0xffff,"EMERALD");
 if(s->error){
  center(a,128,1,0xfd45,s->error==1?"BOOT LOADER MISSING":
   s->error==2?"SD STORAGE ERROR":s->error==3?"START RAM UNAVAILABLE":"GAME DID NOT START");
  gsoftkeys(a,"RETRY","BACK");
 }else {center(a,140,2,0x07ef,"STARTING EMERALD");center(a,174,1,0xffff,"PLEASE WAIT");}
}
static u32 start(const PgaApi *a,State *s){
 s->error=0;
  if(!installed()){s->error=1;return PGA_REDRAW;}
  if(!reserve(s)){s->error=3;return PGA_REDRAW;}
  peaq_boot_request(s->ticket,PEAQ_MENU_PLAY_FNV,6);
  u8 verified[492];int ok=a->save(a->storage,s->ticket,492)&&a->load(a->storage,verified,492);
  if(ok){for(unsigned i=0;i<492;++i)if(verified[i]!=s->ticket[i]){ok=0;break;}}
  if(!ok){
   release(s);s->ticket[8]=0;(void)a->save(a->storage,s->ticket,492);
   s->error=2;return PGA_REDRAW;
  }
  publish(s);
  s->screen=STARTING;s->pending=1;s->age=0;a->silence(a->storage);return PGA_REDRAW;
}
u32 pga_event(const PgaApi *a,void *state,u32 event,u32 key){
 State *s=state;
 if(event==PGA_INIT){
  s->screen=HOME;
  if(a->load(a->storage,s->ticket,492)&&word(s->ticket)==PEAQ_BOOT_REQUEST&&
     word(s->ticket+4)==1&&word(s->ticket+8)==1&&word(s->ticket+12)==PEAQ_MENU_PLAY_FNV)
   s->error=4;
  return s->error?PGA_REDRAW:start(a,s);
 }
 if(event==PGA_PAINT){paint(a,s);return 0;}
 if(event==PGA_CLOSE||event==PGA_PAUSE){
  /* Losing focus cancels a still-pending reboot request. Never leave an
   * unintended active ticket for the next unrelated handset reboot. */
  if(s->pending){
   release(s);s->ticket[8]=0;(void)a->save(a->storage,s->ticket,492);
   s->pending=0;s->screen=HOME;
  }
  return 0;
 }
 if(event==PGA_TICK&&s->pending){
  if(++s->age>=20){
   s->pending=0;clean(s->marker);
   ((void (*)(u32))0x80a9e1e1u)(8);
   ((void (*)(void))0x80aa0439u)();
   /* The stock reset function leaves interrupts masked if hardware fails
    * to reset. Returning to the OS dispatcher in that state is forbidden. */
   for(;;)__asm__ volatile("wfi");
  }
  return 0;
 }
 if(event!=PGA_KEY||s->pending)return 0;
 if(key==PGA_BACK)return PGA_QUIT;
 if(ui_ok(key)&&s->error)return start(a,s);
 return 0;
}
