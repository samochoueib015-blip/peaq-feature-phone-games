#include "mocor_host.h"
#include <stdatomic.h>
#include <stddef.h>

#define MAGIC 0x31484d44u
static const char worker_name[]="PEAQ Doom";
/* Reuse the already-qualified instruction sequence, without editing its file. */
extern void peaq_licar_mocor_sync_code(void *,uint32_t);
const PeaqDoomMocorApi peaq_pmp122_doom_native={
 (PeaqTxCreate)PEAQ_TX_CREATE_THUMB,(PeaqTxThreadOp)PEAQ_TX_RESUME_THUMB,
 (PeaqTxThreadOp)0x80a07ff1u,(PeaqTxInfo)PEAQ_TX_INFO_THUMB,
 (PeaqTxThreadOp)PEAQ_TX_DELETE_THUMB,(void *)0x80a8441du,peaq_licar_mocor_sync_code
};
typedef struct { uint32_t handle,uncertain; } File;
typedef struct {
 uint32_t magic;PeaqDoomMocorApi api;PeaqFileApi files,ui_files;
 PeaqDoomWorkerHost services;PeaqDoomWorker *worker;PeaqDoomMemory memory;
 PeaqResidentTxJoin join;void (*entry)(void *);void *arg;
 uint32_t attempted,phase,pumping,open_calls,read_calls,read_bytes,failed_count;
 uint32_t service_seen,service_last;
 _Atomic uint32_t enabled,entered,error,native_status,park_calls;
 char drive,failed[8][32];File file[8];
} Host;
_Static_assert(sizeof(Host)<=PEAQ_DOOM_MOCOR_HOST_BYTES,"Doom native host storage");
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Doom native host requires lock-free atomics");
static _Atomic(Host *) active_host;
static Host *host(PeaqDoomMocorHost *p){return (Host *)(void *)p;}
static int valid(const Host *h){return h&&h->magic==MAGIC;}
static void failure(Host *h,uint32_t error,uint32_t status){uint32_t zero=0;atomic_compare_exchange_strong(&h->error,&zero,error);atomic_store(&h->native_status,status);}
static int info(Host *h,uint32_t *state,uint32_t *runs){
 char *name=0;uint32_t priority=0,threshold=0,slice=0;void *next=0,*suspended=0;
 uint32_t s=h->api.info(h->join.tcb,&name,state,runs,&priority,&threshold,&slice,&next,&suspended);
 if(s||name!=worker_name){failure(h,PDMH_IDENTITY,s);return 0;}return 1;
}
static int rollback(Host *h){
 uint32_t state=UINT32_MAX,runs=UINT32_MAX;
 if(atomic_load(&h->entered)||!info(h,&state,&runs)||runs||
    (state!=PEAQ_THREAD_SUSPENDED&&state!=PEAQ_THREAD_TERMINATED))return -1;
 if(state==PEAQ_THREAD_SUSPENDED){uint32_t s=h->api.terminate(h->join.tcb);if(s){failure(h,PDMH_ROLLBACK_ERROR,s);return -1;}}
 uint32_t s=h->api.delete_thread(h->join.tcb);if(s){failure(h,PDMH_ROLLBACK_ERROR,s);return -1;}
 h->phase=PDMH_NO_THREAD;h->join.joined=1;return 1;
}
static void trampoline(uint32_t cookie){Host *h=(Host *)(uintptr_t)cookie;atomic_store(&h->entered,1);if(atomic_load(&h->enabled))h->entry(h->arg);}
static uint32_t start(void *cookie,void (*entry)(void *),void *arg,void *tcb,uint32_t tcb_bytes,void *stack,uint32_t stack_bytes){
 Host *h=cookie,*empty=0;
 if(!entry||h->attempted||tcb!=(uint8_t *)h->memory.tcb+8||tcb_bytes!=188||stack!=h->memory.stack||stack_bytes!=PEAQ_DOOM_STACK_BYTES){failure(h,PDMH_ARGUMENT,0);return 0;}
 if(!atomic_compare_exchange_strong(&active_host,&empty,h)){failure(h,PDMH_ARGUMENT,0);return 0;}
 h->attempted=1;h->entry=entry;h->arg=arg;
 PeaqResidentTxApi join_api={h->api.info,h->api.delete_thread};
 if(!peaq_resident_tx_join_init(&h->join,&join_api,tcb,worker_name)){failure(h,PDMH_ARGUMENT,0);h->phase=PDMH_NO_THREAD;return 0;}
 uint32_t s=h->api.create(tcb,worker_name,trampoline,(uint32_t)(uintptr_t)h,stack,stack_bytes,160,160,1,0);
 if(s){failure(h,PDMH_CREATE,s);h->phase=PDMH_NO_THREAD;return 0;}
 h->phase=PDMH_RUNNING;atomic_store(&h->enabled,1);s=h->api.resume(tcb);
 if(!s)return (uint32_t)(uintptr_t)tcb;
 atomic_store(&h->enabled,0);failure(h,PDMH_RESUME,s);
 uint32_t state=UINT32_MAX,runs=UINT32_MAX;
 if(!atomic_load(&h->entered)&&info(h,&state,&runs)&&!runs&&state==PEAQ_THREAD_SUSPENDED){h->phase=PDMH_ROLLBACK;rollback(h);return 0;}
 h->phase=PDMH_QUARANTINED;peaq_doom_worker_close(h->worker);
 atomic_store(&h->enabled,1);return (uint32_t)(uintptr_t)tcb;
}
static int join(void *cookie,uint32_t handle){Host *h=cookie;int result=peaq_resident_tx_join_poll(&h->join,handle);if(result==1)h->phase=PDMH_JOINED;return result;}
static void park(void *cookie,uint32_t n){
 Host *h=cookie;if(!n)n=1;if(n>50)n=50;atomic_fetch_add(&h->park_calls,1);
 uint32_t s=h->api.sleep(n);if(s){failure(h,PDMH_PARK,s);peaq_doom_worker_close(h->worker);}
}
static int sync_code(void *cookie,void *p,uint32_t n){
 Host *h=cookie;uintptr_t at=(uintptr_t)p,base=(uintptr_t)h->memory.module;
 if(at<base||at-base>h->memory.module_bytes||n>h->memory.module_bytes-(at-base))return 0;
 h->api.sync_code(p,n);return 1;
}
static File *lookup(Host *h,uint32_t handle){if(!h||!handle)return 0;for(unsigned i=0;i<8;i++)if(h->file[i].handle==handle)return &h->file[i];return 0;}
static int basename(Host *h,const uint16_t *path,char name[32]){
 static const char rest[] = ":\\Games\\Doom\\";
 if(!path||path[0]!=(uint16_t)h->drive)return 0;
 unsigned p=1;for(unsigned i=0;rest[i];i++,p++)if(path[p]!=(unsigned char)rest[i])return 0;
 for(unsigned i=0;i<32;i++){
  unsigned c=path[p+i];if(!c){if(!i||name[i-1]=='.')return 0;name[i]=0;return 1;}
  if((!i&&c=='.')||!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
  /* FAT basenames are case-insensitive; aliases must not bypass no-retry. */
  name[i]=(char)((c>='A'&&c<='Z')?c+32:c);
 }
 return 0;
}
static int equal(const char *a,const char *b){for(unsigned i=0;i<32;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}return 0;}
static uint32_t file_open(const uint16_t *path,uint32_t mode,uint32_t attr,void *async){
 Host *h=atomic_load(&active_host);char name[32];
 if(!h||h->phase!=PDMH_RUNNING||mode!=0x31||attr||async||!basename(h,path,name))return 0;
 for(unsigned i=0;i<h->failed_count;i++)if(equal(name,h->failed[i]))return 0;
 if(h->failed_count==8)return 0;File *slot=0;
 for(unsigned i=0;i<8;i++)if(!h->file[i].handle){slot=&h->file[i];break;}
 if(!slot)return 0;h->open_calls++;uint32_t handle=h->files.open(path,mode,attr,async);
 if(!handle){
  for(unsigned i=0;i<32;i++){h->failed[h->failed_count][i]=name[i];if(!name[i])break;}
  h->failed_count++;failure(h,PDMH_OPEN,0);return 0;
 }
 File *duplicate=lookup(h,handle);
 if(duplicate){duplicate->uncertain=1;failure(h,PDMH_OPEN,handle);peaq_doom_worker_close(h->worker);h->phase=PDMH_QUARANTINED;return 0;}
 *slot=(File){handle,0};return handle;
}
static uint32_t file_close(uint32_t handle){
 Host *h=atomic_load(&active_host);File *f=lookup(h,handle);if(!f||f->uncertain)return 7;
 uint32_t s=h->files.close(handle);if(s){f->uncertain=1;failure(h,PDMH_CLOSE,s);return s;}
 *f=(File){0};return 0;
}
static uint32_t file_read(uint32_t handle,void *out,uint32_t n,uint32_t *got,void *async){
 Host *h=atomic_load(&active_host);File *f=lookup(h,handle);
 if(got)*got=0;if(!f||f->uncertain||!got||!out||!n||n>4096||async)return 7;
 h->read_calls++;uint32_t s=h->files.read(handle,out,n,got,0);
 if(s||*got>n){*got=0;failure(h,PDMH_READ,s?s:7);return s?s:7;}
 h->read_bytes+=*got;return 0;
}
static uint32_t file_seek(uint32_t handle,int64_t offset,uint32_t origin){
 Host *h=atomic_load(&active_host);File *f=lookup(h,handle);if(!f||f->uncertain||origin>2)return 7;
 uint32_t s=h->files.seek(handle,offset,origin);if(s)failure(h,PDMH_SEEK,s);return s;
}
static uint32_t file_position(uint32_t handle,uint32_t reserved,int32_t *out){
 Host *h=atomic_load(&active_host);File *f=lookup(h,handle);if(!f||f->uncertain||reserved||!out)return 7;
 uint32_t s=h->files.position(handle,0,out);if(s)failure(h,PDMH_POSITION,s);return s;
}
static uint32_t file_size(uint32_t handle,uint32_t *out){
 Host *h=atomic_load(&active_host);File *f=lookup(h,handle);if(!f||f->uncertain||!out)return 7;
 uint32_t s=h->files.size(handle,out);if(s)failure(h,PDMH_SIZE,s);return s;
}
static int range(uintptr_t a,uint32_t n){return a&&!(a&3)&&n&&a+n>=a;}
int peaq_doom_mocor_prepare(PeaqDoomMocorHost *p,const PeaqDoomMocorApi *api,const PeaqFileApi *files,
 PeaqDoomWorker *worker,const PeaqDoomMemory *memory,char drive){
 Host *h=host(p);if(!h||h->magic||atomic_load(&active_host)||!api||!files||!worker||!memory||(drive!='D'&&drive!='E')||
 !api->create||!api->resume||!api->terminate||!api->info||!api->delete_thread||!api->sleep||!api->sync_code||
 !files->open||!files->read||!files->close||!files->seek||!files->position||!files->size)return 0;
 if(memory->module_bytes<PEAQ_DOOM_MODULE_BYTES||memory->heap_bytes<PEAQ_DOOM_HEAP_BYTES||memory->frame_bytes<PEAQ_DOOM_FRAME_BYTES||
 memory->tcb_bytes<PEAQ_DOOM_TCB_BYTES||memory->stack_bytes<PEAQ_DOOM_STACK_BYTES||((uintptr_t)memory->stack&7))return 0;
 uintptr_t at[]={(uintptr_t)h,(uintptr_t)worker,(uintptr_t)memory->module,(uintptr_t)memory->heap,(uintptr_t)memory->frames,(uintptr_t)memory->tcb,(uintptr_t)memory->stack};
 uint32_t size[]={PEAQ_DOOM_MOCOR_HOST_BYTES,PEAQ_DOOM_WORKER_BYTES,memory->module_bytes,memory->heap_bytes,memory->frame_bytes,memory->tcb_bytes,memory->stack_bytes};
 for(unsigned i=0;i<7;i++){if(!range(at[i],size[i]))return 0;for(unsigned j=0;j<i;j++)if(at[i]<at[j]+size[j]&&at[j]<at[i]+size[i])return 0;}
 PeaqDoomMocorApi a=*api;PeaqFileApi f=*files;PeaqDoomMemory m=*memory;
 *h=(Host){0};h->magic=MAGIC;h->api=a;h->files=f;h->memory=m;h->worker=worker;h->drive=drive;
 h->ui_files=(PeaqFileApi){file_open,file_read,0,file_close,file_seek,file_position,file_size,0};
 h->services=(PeaqDoomWorkerHost){h,start,join,park,sync_code,&h->ui_files};return 1;
}
const PeaqDoomWorkerHost *peaq_doom_mocor_services(PeaqDoomMocorHost *p){Host *h=host(p);return valid(h)?&h->services:0;}
void peaq_doom_mocor_close(PeaqDoomMocorHost *p){Host *h=host(p);if(valid(h)){h->service_seen=0;peaq_doom_worker_close(h->worker);}}
int peaq_doom_mocor_pump(PeaqDoomMocorHost *p,uint32_t now){
 Host *h=host(p);if(h&&!h->magic)return 1;if(!valid(h)||h->pumping)return 0;h->pumping=1;
 int ready=peaq_doom_worker_pump(h->worker,now);
 if(h->phase==PDMH_ROLLBACK&&rollback(h)!=1)ready=0;
 if(h->phase!=PDMH_IDLE&&h->phase!=PDMH_NO_THREAD&&h->phase!=PDMH_JOINED)ready=0;
 for(unsigned i=0;i<8;i++)if(h->file[i].handle||h->file[i].uncertain)ready=0;
 if(ready){Host *owned=h;atomic_compare_exchange_strong(&active_host,&owned,0);}
 h->pumping=0;return ready;
}
int peaq_doom_mocor_service(PeaqDoomMocorHost *p,uint32_t now,uint32_t *delay){
 Host *h=host(p);if(!delay)return 0;*delay=PEAQ_DOOM_SERVICE_INTERVAL_MS;
 if(h&&!h->magic){*delay=0;return 1;}
 if(!valid(h)||h->pumping)return 0;
 uint32_t elapsed=now-h->service_last;
 if(h->service_seen&&elapsed<PEAQ_DOOM_SERVICE_INTERVAL_MS){*delay=PEAQ_DOOM_SERVICE_INTERVAL_MS-elapsed;return 0;}
 h->service_last=now;h->service_seen=1;
 int ready=peaq_doom_mocor_pump(p,now);if(ready)*delay=0;return ready;
}
void peaq_doom_mocor_status(const PeaqDoomMocorHost *p,PeaqDoomMocorStatus *out){
 const Host *h=(const Host *)(const void *)p;if(!out)return;*out=(PeaqDoomMocorStatus){0};if(!valid(h))return;
 *out=(PeaqDoomMocorStatus){h->phase,atomic_load(&h->error),atomic_load(&h->native_status),atomic_load(&h->entered),0,h->failed_count,0,
 h->open_calls,h->read_calls,h->read_bytes,atomic_load(&h->park_calls)};
 for(unsigned i=0;i<8;i++){out->open_handles+=!!h->file[i].handle;out->close_uncertain+=!!h->file[i].uncertain;}
}
