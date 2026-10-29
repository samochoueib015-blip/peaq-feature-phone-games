#include "runtime.h"
#include "platform.h"

static PeaqEmeraldRuntime *current;
static const EmeraldHost engine_host;
typedef int (*Init)(const EmeraldHost *,uint16_t *,uint16_t *,uint8_t *);
typedef void (*Step)(uint32_t);
typedef uint8_t *(*SaveMemory)(void);

static void zero(void *p,uint32_t n){
 uint8_t *b=p;
 while(n&&((uintptr_t)b&3u)){*b++=0;--n;}
 while(n>=4){*(uint32_t *)b=0;b+=4;n-=4;}
 while(n--)*b++=0;
}
static int separate(const void *p,uint32_t n,const void *q,uint32_t m){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
 return a+n>=a&&b+m>=b&&(a+n<=b||b+m<=a);
}
static void *reserve(PeaqEmeraldRuntime *r,uint32_t n){
 if(r->used>r->arena_bytes||n>r->arena_bytes-r->used)return 0;
 uint32_t at=r->used,pad=(32-(at&31))&31;
 if(pad>r->arena_bytes-at||n>r->arena_bytes-at-pad)return 0;
 r->used=at+pad+n;return r->arena+at+pad;
}
static void *allocate(uint32_t n){return reserve(current,n);}
static void release(void *p){(void)p;current->used=0;}
static void present(const uint16_t *p,uint32_t w,uint32_t h){
 if(current&&current->services)current->services->present(current->services->cookie,p,w,h);
}
static void audio(const int8_t *p,uint32_t n,uint32_t rate){
 if(current&&current->services&&current->services->audio &&
    !current->services->audio(current->services->cookie,p,n,rate))current->audio_dropped++;
}
static void dirty(void){if(current){current->dirty=1;current->generation++;current->quiet_frames=0;}}
extern uint32_t sys_timer_ms(void);
static const EmeraldHost engine_host={present,audio,dirty,sys_timer_ms};

void peaq_emerald_runtime_discard(PeaqEmeraldRuntime *r){
 if(!r||current!=r)return;
 current=0;zero(r,sizeof(*r));
}
static int fail(PeaqEmeraldRuntime *r,int code){
 peaq_emerald_runtime_discard(r);r->last_error=code;return code;
}
int peaq_emerald_runtime_start(PeaqEmeraldRuntime *r,const PeaqEmeraldServices *s,
                              void *arena,uint32_t bytes,uint32_t now){
 if(current)return PEAQ_EMERALD_BUSY;
 uintptr_t a=(uintptr_t)arena;
 if(!r||!s||!a||(a&31)||bytes<4096||bytes>64u*1024u*1024u||a+bytes<a||
    !separate(r,sizeof(*r),s,sizeof(*s))||!separate(arena,bytes,r,sizeof(*r))||
    !separate(arena,bytes,s,sizeof(*s)))return PEAQ_EMERALD_ARGUMENT;
 if(!s->save||!separate(r,sizeof(*r),s->save,sizeof(*s->save))||
    !separate(arena,bytes,s->save,sizeof(*s->save))||
    !s->module_read||!s->sync_code||!s->present||
    !s->save->read||!s->save->begin||!s->save->write||!s->save->finish)return PEAQ_EMERALD_ARGUMENT;
 zero(r,sizeof(*r));r->services=s;r->arena=arena;r->arena_bytes=bytes;current=r;
 PeaqModuleHost host;
 host.cookie=s->cookie;host.read=s->module_read;host.allocate=allocate;
 host.release=release;host.sync_code=s->sync_code;
 if(!peaq_module_load(&host,&r->module)||r->module.export_count!=5)return fail(r,PEAQ_EMERALD_MODULE);
 r->gba=reserve(r,240u*160u*2u);r->portrait=reserve(r,240u*320u*2u);
 r->layers=reserve(r,240u*160u);r->save_scratch=reserve(r,PEAQ_SAVE_SCRATCH);
 if(!r->gba||!r->portrait||!r->layers||!r->save_scratch)return fail(r,PEAQ_EMERALD_MEMORY);
 zero(r->gba,240u*160u*2u);zero(r->portrait,240u*320u*2u);zero(r->layers,240u*160u);
 r->save_memory=((SaveMemory)(uintptr_t)r->module.exports[4])();
 uintptr_t save=(uintptr_t)r->save_memory,base=(uintptr_t)r->module.base;
 /* The exported save mirror must be wholly inside this module's owned BSS. */
 if(save<base+r->module.image_bytes||save+131072u<save||save+131072u>base+r->module.memory_bytes)
  return fail(r,PEAQ_EMERALD_MODULE);
 uint32_t got=0;
 int loaded=peaq_large_save_load(s->save,r->save_memory,131072u,&got,r->save_scratch,PEAQ_SAVE_SCRATCH);
 if(loaded<0)return fail(r,PEAQ_EMERALD_SAVE_IO);
 if(loaded==0){
  for(uint32_t slot=0;slot<2;slot++){
   got=0;int status=s->save->read(s->save->cookie,slot,0,r->save_scratch,1,&got);
   if(status<0)return fail(r,PEAQ_EMERALD_SAVE_IO);
   if(status!=0||got)return fail(r,PEAQ_EMERALD_SAVE_INVALID);
  }
  for(uint32_t i=0;i<131072u;i++)r->save_memory[i]=255;
 }else if(got!=131072u)return fail(r,PEAQ_EMERALD_SAVE_INVALID);
 r->previous_ms=now;
 if(!((Init)(uintptr_t)r->module.exports[0])(&engine_host,r->gba,r->portrait,r->layers))
  return fail(r,PEAQ_EMERALD_INIT);
 r->active=1;return PEAQ_EMERALD_OK;
}
int peaq_emerald_runtime_flush(PeaqEmeraldRuntime *r){
 if(!r||current!=r||!r->active)return PEAQ_EMERALD_ARGUMENT;
 if(!r->dirty)return PEAQ_EMERALD_OK;
 if(!peaq_large_save_commit(r->services->save,r->save_memory,131072u,r->save_scratch,PEAQ_SAVE_SCRATCH)){
  r->last_error=PEAQ_EMERALD_SAVE_IO;return PEAQ_EMERALD_SAVE_IO;
 }
 r->dirty=0;r->quiet_frames=0;r->last_error=0;return PEAQ_EMERALD_OK;
}
int peaq_emerald_runtime_step(PeaqEmeraldRuntime *r,uint32_t now,uint32_t buttons){
 if(!r||current!=r||!r->active)return PEAQ_EMERALD_ARGUMENT;
 uint32_t elapsed=now-r->previous_ms;r->previous_ms=now;
 if(r->paused){r->phase=0;return PEAQ_EMERALD_IDLE;}
 if(elapsed>100)elapsed=100;
 r->phase+=elapsed*60;
 if(r->phase<1000)return PEAQ_EMERALD_IDLE;
 r->phase%=1000;uint32_t old=r->generation;
 ((Step)(uintptr_t)r->module.exports[1])(buttons&0x800003ffu);r->frames++;
 /* Defer SD until the outer boundary has restored uncached CPU state. */
 if(r->dirty&&r->generation==old&&r->quiet_frames<30)++r->quiet_frames;
 return PEAQ_EMERALD_OK;
}
void peaq_emerald_runtime_pause(PeaqEmeraldRuntime *r,int paused,uint32_t now){
 if(!r||current!=r||!r->active)return;
 r->paused=!!paused;r->previous_ms=now;r->phase=0;
}
int peaq_emerald_runtime_stop(PeaqEmeraldRuntime *r){
 if(!r||current!=r||!r->active)return PEAQ_EMERALD_ARGUMENT;
 r->paused=1;int result=peaq_emerald_runtime_flush(r);
 if(result!=PEAQ_EMERALD_OK)return result;
 peaq_emerald_runtime_discard(r);return PEAQ_EMERALD_OK;
}
