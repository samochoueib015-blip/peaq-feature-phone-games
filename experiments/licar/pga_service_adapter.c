#include "pga_adapter.h"
#include "mocor_host.h"
#include "../runtime/mocor_display.h"
#include "../runtime/mocor_time.h"
#include "../launcher/segment_profiles.h"
#include "../../sdk/gfx.h"
#include <stddef.h>

#define MAGIC 0x3541504cu
typedef struct {
 uint32_t magic,stage,error,generation,closed,paused,frames,back_exit,resume_key;
 int32_t display_result;
 const PgaApi *api;
 PeaqProposedGameLocation location;
 PeaqLicarMocorHost *host;
 PeaqLicarIoBridge *bridge;
 PeaqLicarRuntime *runtime;
 PeaqLicarIoMemory memory;
 uint32_t service_armed,service_at,paint_armed,paint_at;
} Adapter;
/* The frozen native file API has no cookie. This PGA owns a separate BSS
 * instance, and the resident owner permits only one active large game. The
 * pointer is used ONLY by serialized UI file calls, never by the raw worker. */
static Adapter *file_owner;
/* ABI5-v2 service candidate: one due service at5ms; redraw requests at50ms.
 * No timer or allocator is owned by SD code. The resident launcher owns both
 * the callback lifetime and the sole native timer. Native I/O can still take
 * longer than the byte/request bound; there is deliberately no catch-up loop. */
const uint32_t pga_state_bytes=sizeof(Adapter),pga_tick_ms=5,pga_required_abi=5;
__attribute__((used)) const PeaqPga5sDescriptor peaq_licar_pga_descriptor={0};
const PeaqPga5sSpec peaq_licar_pga_segments[7]={
 {PGS_PORT_MODULE,PEAQ_LICAR_IO_MODULE_BYTES,4,0},
 {PGS_PORT_FRAMES,PEAQ_LICAR_IO_FRAME_BYTES,4,0},
 {PGS_PORT_WORKER,PEAQ_LICAR_IO_BRIDGE_BYTES,4,0},
 {PGS_PORT_RUNTIME,PEAQ_LICAR_RUNTIME_BYTES,4,0},
 {PGS_PORT_HOST,PEAQ_LICAR_MOCOR_HOST_BYTES,4,0},
 {PGS_PORT_STACK,PEAQ_LICAR_WORKER_STACK,8,0},
 {PGS_PORT_TCB,PEAQ_LICAR_IO_TCB_BYTES,4,0}
};
static uint32_t address(const void *p){return (uint32_t)(uintptr_t)p;}
static int span(const void *p,uint32_t n){uint32_t a=address(p);return a&&!(a&3u)&&n&&n<=UINT32_MAX-a;}
static int overlap(const void *a,uint32_t n,const void *b,uint32_t m){return address(a)<address(b)+m&&address(b)<address(a)+n;}
static int zero(const void *p,uint32_t n){const uint8_t *b=p;while(n--)if(*b++)return 0;return 1;}
static Adapter *adapter(void *p){return p;}
static int valid(const Adapter *p,const PgaApi *a){return p&&p->magic==MAGIC&&p->api==a;}
static int location_valid(const PeaqProposedGameLocation *p){
 if(!span(p,sizeof(*p))||p->bytes!=sizeof(*p)||p->version!=1||(p->drive!='D'&&p->drive!='E')||
    p->reserved[0]||p->reserved[1]||p->reserved[2]||!p->folder[0]||p->folder[0]=='.')return 0;
 uint32_t i=0;for(;i<32&&p->folder[i];i++){
  uint16_t c=p->folder[i];if(c<32||c=='/'||c=='\\'||c==':'||c=='*'||c=='?')return 0;
 }
 if(i==32)return 0;
 for(;i<32;i++)if(p->folder[i])return 0;
 return 1;
}
static int arguments(const PgaApi *api,Adapter *p,const PeaqPga5sMemory *m,uint32_t count,uint32_t generation){
 if(!span(api,sizeof(PeaqProposedPga5Api))||!span(p,sizeof(*p))||!span(m,7*sizeof(*m))||count!=7||!generation||
    api->version!=5||api->width!=240||api->height!=320||!api->fill||!api->storage||
    (api->capabilities&(PGA_CAP_BACK|PGA_CAP_RAW_KEYS))!=(PGA_CAP_BACK|PGA_CAP_RAW_KEYS)||file_owner||!zero(p,sizeof(*p)))return 0;
 const PeaqProposedPga5Api *a=(const void *)api;
 if(a->bytes!=sizeof(*a)||a->extension_version!=1||!location_valid(a->location)||
    overlap(p,sizeof(*p),api,sizeof(*a))||overlap(p,sizeof(*p),a->location,sizeof(*a->location))||
    overlap(p,sizeof(*p),m,count*sizeof(*m)))return 0;
 uint32_t seen=0;
 for(uint32_t i=0;i<count;i++){
  uint32_t j=0;while(j<7&&peaq_licar_pga_segments[j].id!=m[i].id)j++;
  if(j==7||(seen&(1u<<j)))return 0;seen|=1u<<j;
  const PeaqPga5sSpec *s=&peaq_licar_pga_segments[j];
  if(m[i].bytes!=s->bytes||m[i].alignment!=s->alignment||!span(m[i].data,m[i].bytes)||
     (address(m[i].data)&(s->alignment-1))||overlap(m[i].data,m[i].bytes,p,sizeof(*p))||
     overlap(m[i].data,m[i].bytes,api,sizeof(*a))||overlap(m[i].data,m[i].bytes,a->location,sizeof(*a->location))||
     overlap(m[i].data,m[i].bytes,m,count*sizeof(*m)))return 0;
  for(uint32_t k=0;k<i;k++)if(overlap(m[i].data,m[i].bytes,m[k].data,m[k].bytes))return 0;
 }
 return seen==127;
}
static void *segment(const PeaqPga5sMemory *m,uint32_t id){for(uint32_t i=0;i<7;i++)if(m[i].id==id)return m[i].data;return 0;}
static int name_equal(const uint16_t *p,const char *s){uint32_t i=0;while(s[i]){if(p[i]!=(uint8_t)s[i])return 0;i++;}return p[i]==0;}
/* Preserve the fully qualified frozen host, but remap ONLY its three known
 * read-existing paths to this game's copied UTF16 SD folder. No arbitrary
 * worker path, write mode or implicit missing-file interpretation is added. */
static uint32_t selected_open(const uint16_t *path,uint32_t mode,uint32_t attr,void *overlapped){
 Adapter *p=file_owner;if(!p||p->stage<PLPA_START_ATTEMPT||p->stage==PLPA_DONE||!path||mode!=0x31||attr||overlapped)return 0;
 const char prefix[]="E:\\Games\\Racer\\";
 if(path[0]!=p->location.drive)return 0;
 for(uint32_t i=1;i<sizeof(prefix)-1;i++)if(path[i]!=(uint8_t)prefix[i])return 0;
 const char *names[]={"engine.pge","data","music.u8"};uint32_t selected=0;
 for(;selected<3;selected++)if(name_equal(path+sizeof(prefix)-1,names[selected]))break;
 if(selected==3)return 0;
 uint16_t out[96];const char root[]="E:\\Games\\";uint32_t n=0,j=0;
 while(root[n]){out[n]=(uint8_t)root[n];n++;}out[0]=p->location.drive;
 while(p->location.folder[j])out[n++]=p->location.folder[j++];out[n++]='\\';
 for(j=0;names[selected][j];j++)out[n++]=(uint8_t)names[selected][j];out[n]=0;
 return peaq_pmp122_files.open(out,mode,attr,overlapped);
}
int peaq_licar_pga_start(const PgaApi *api,void *state,const PeaqPga5sMemory *m,uint32_t count,uint32_t generation){
 Adapter *p=adapter(state);if(!arguments(api,p,m,count,generation))return 0;
 const PeaqProposedPga5Api *a=(const void *)api;
 p->magic=MAGIC;p->stage=PLPA_BOUND;p->api=api;p->generation=generation;p->location=*a->location;
 p->host=segment(m,PGS_PORT_HOST);p->bridge=segment(m,PGS_PORT_WORKER);p->runtime=segment(m,PGS_PORT_RUNTIME);
 p->memory=(PeaqLicarIoMemory){segment(m,PGS_PORT_MODULE),PEAQ_LICAR_IO_MODULE_BYTES,
  segment(m,PGS_PORT_FRAMES),PEAQ_LICAR_IO_FRAME_BYTES,segment(m,PGS_PORT_TCB),PEAQ_LICAR_IO_TCB_BYTES,
  segment(m,PGS_PORT_STACK),PEAQ_LICAR_WORKER_STACK};
 PeaqFileApi files=peaq_pmp122_files;files.open=selected_open;file_owner=p;
 if(!peaq_licar_mocor_prepare(p->host,&peaq_pmp122_licar_native,&files,p->bridge,&p->memory,(char)p->location.drive)){
  p->error=PLPA_HOST_ERROR;return 0;
 }
 p->stage=PLPA_HOST;
 if(!peaq_licar_io_prepare(p->bridge,generation,p->runtime,peaq_licar_mocor_services(p->host),&p->memory)){
  p->error=PLPA_BRIDGE_ERROR;return 0;
 }
 p->stage=PLPA_BRIDGE;if(p->closed)return 0;
 /* Publish the attempted stage BEFORE resume: worker execution and UI close
 * may occur before native Start returns. Only the bridge can prove release. */
 p->stage=PLPA_START_ATTEMPT;
 if(!peaq_licar_runtime_start(p->runtime,peaq_licar_io_services(p->bridge))){p->error=PLPA_START_ERROR;return 0;}
 return !p->closed;
}
void peaq_licar_pga_close(const PgaApi *api,void *state){
 Adapter *p=adapter(state);if(!valid(p,api)||p->closed)return;p->closed=1;p->service_armed=0;
 if(p->stage>=PLPA_BRIDGE&&p->stage<PLPA_DONE)peaq_licar_io_close(p->bridge);
}
int peaq_licar_pga_poll(const PgaApi *api,void *state,uint32_t now){
 Adapter *p=adapter(state);if(!p)return 0;
 if(!p->magic)return zero(p,sizeof(*p));
 if(!valid(p,api))return -1;if(p->stage==PLPA_DONE)return 1;
 int ready=0;
 if(p->stage<PLPA_START_ATTEMPT){
  ready=p->stage==PLPA_BOUND||peaq_licar_mocor_quiescent(p->host);
  if(p->stage==PLPA_BRIDGE){PeaqLicarIoStatus s;peaq_licar_io_status(p->bridge,&s);ready=ready&&!s.pool_claims&&!s.started;}
 }else {
  if(p->service_armed&&(uint32_t)(now-p->service_at)<5u)return 0;
  p->service_armed=1;p->service_at=now;
  ready=peaq_licar_io_pump(p->bridge,now);
 }
 if(ready==1){p->stage=PLPA_DONE;if(file_owner==p)file_owner=0;return 1;}
 return 0;
}
static void label(Adapter *p,const char *text){
 p->api->fill(0,0,240,320,0x0842);
 for(uint32_t i=0;text[i]&&i<32;i++){
  uint8_t c=(uint8_t)text[i];if(c<32||c>95)c='?';
  for(uint32_t x=0;x<5;x++)for(uint32_t y=0;y<7;y++)if(glyphs[c-32][x]&(1u<<y))
   p->api->fill(239-(104+(int)y*2)-1,24+(int)i*12+(int)x*2,2,2,0xffff);
 }
}
uint32_t pga_event(const PgaApi *api,void *state,uint32_t event,uint32_t value){
 Adapter *p=adapter(state);if(!valid(p,api)||p->closed||p->stage!=PLPA_START_ATTEMPT)return 0;
 if(event==PGA_PAUSE){p->paused=1;p->back_exit=p->resume_key=0;peaq_licar_runtime_pause(p->runtime,1);return PGA_REDRAW;}
 if(event==PGA_RESUME)return PGA_REDRAW; /* Explicit confirmation resumes. */
 if(event==PGA_TICK){
  uint32_t now=peaq_mocor_milliseconds();
  if(p->paint_armed&&(uint32_t)(now-p->paint_at)<50u)return 0;
  p->paint_armed=1;p->paint_at=now;return PGA_REDRAW;
 }
 if(event==PGA_RAW_KEY){
  uint32_t raw=value&255,kind=value&0xff00;
  if(p->resume_key==raw){if(kind==0xfb00||kind==0xfd00)p->resume_key=0;return 0;}
  if(p->paused&&(raw==0x16||raw==0x1b||raw==0x26||raw==0x0c)&&kind==0xfa00){
   p->paused=0;p->resume_key=raw;peaq_licar_runtime_pause(p->runtime,0);return PGA_REDRAW;
  }
  if(raw==0x17){
   if(kind==0xfa00){PeaqLicarStatus s;peaq_licar_runtime_status(p->runtime,&s);p->back_exit=p->paused||s.state!=PLR_RUNNING||s.engine_state==0;}
   if((kind==0xfb00||kind==0xfd00)&&p->back_exit)return PGA_QUIT;
  }
  if(!p->paused)peaq_licar_runtime_input(p->runtime,value);return 0;
 }
 if(event!=PGA_PAINT)return 0;
 if(p->paused){label(p,"PAUSE");return 0;}
 PeaqLicarFrame frame;
 if(!peaq_licar_runtime_frame_acquire(p->runtime,&frame)){PeaqLicarStatus s;peaq_licar_runtime_status(p->runtime,&s);if(s.state<PLR_RUNNING)label(p,"RACER LAEDT");return 0;}
 p->display_result=peaq_display_present(&peaq_pmp122_display,frame.pixels,240,320);
 int released=peaq_licar_runtime_frame_release(p->runtime,&frame);
 if(p->display_result!=1||!released){p->error=released?PLPA_DISPLAY_ERROR:PLPA_FRAME_ERROR;peaq_licar_io_close(p->bridge);return PGA_QUIT;}
 p->frames++;return 0;
}
void peaq_licar_pga_status(const void *state,PeaqLicarPgaStatus *out){
 const Adapter *p=state;if(!out)return;*out=(PeaqLicarPgaStatus){0};if(!p||p->magic!=MAGIC)return;
 *out=(PeaqLicarPgaStatus){p->stage,p->error,p->generation,p->closed,p->paused,p->frames,p->display_result};
}
