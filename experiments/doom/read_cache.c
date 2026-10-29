#include "read_cache.h"
#include <errno.h>
#include <limits.h>
#include <stddef.h>

void peaq_doom_read_cache_init(PeaqDoomReadCache *c,PeaqDoomRemoteIo remote,void *cookie){
 *c=(PeaqDoomReadCache){0};c->remote=remote;c->cookie=cookie;
}
static PeaqDoomCachedFile *find(PeaqDoomReadCache *c,uint32_t fd){
 for(unsigned i=0;i<PEAQ_DOOM_FILE_SLOTS;i++)if(c->files[i].fd==fd&&fd>=3)return &c->files[i];
 return 0;
}
static int32_t remote(PeaqDoomReadCache *c,uint32_t op,uint32_t a,uint32_t b,uint32_t d){
 return c->remote(c->cookie,op,a,b,d);
}
static void discard(PeaqDoomReadCache *c,uint32_t fd){
 for(unsigned i=0;i<PEAQ_DOOM_CACHE_BLOCKS;i++)if(c->blocks[i].fd==fd)c->blocks[i].fd=0;
}
static void poison(PeaqDoomReadCache *c,PeaqDoomCachedFile *f){
 discard(c,f->fd);f->poisoned=1;
}
static PeaqDoomCachedBlock *hit(PeaqDoomReadCache *c,PeaqDoomCachedFile *f,int endpoint){
 for(unsigned i=0;i<PEAQ_DOOM_CACHE_BLOCKS;i++){
  PeaqDoomCachedBlock *b=&c->blocks[i];
  if(b->fd==f->fd&&f->position>=b->start&&
    (f->position-b->start<b->length||(endpoint&&f->position-b->start==b->length))){
   c->victim=(i+1)%PEAQ_DOOM_CACHE_BLOCKS;return b;
  }
 }
 return 0;
}
static int32_t seek_file(PeaqDoomReadCache *c,PeaqDoomCachedFile *f,int32_t offset,uint32_t origin){
 if(origin>2)return -EINVAL;
 if(f->close_uncertain)return -EIO;
 int64_t target=(int64_t)(origin==1?f->position:0)+offset;
 if(origin!=2){
  if(target<0)return -EINVAL;
  if(target>INT_MAX)return -EOVERFLOW;
  if(!f->poisoned){
   uint32_t old=f->position;f->position=(uint32_t)target;
   if(old==f->position||hit(c,f,1))return (int32_t)target;
   f->position=old;
  }
 }
 /* Relative seeks use the logical position, never the read-ahead native one.
  * END retains the real signed native seek/size/error semantics. */
 int32_t result=remote(c,DOOM_IO_SEEK,f->fd,origin==2?(uint32_t)offset:(uint32_t)target,origin==2?2:0);
 if(result<0){poison(c,f);return result;}
 f->position=(uint32_t)result;f->remote_position=(uint32_t)result;f->poisoned=0;
 return result;
}
static int32_t read_file(PeaqDoomReadCache *c,PeaqDoomCachedFile *f,uint8_t *out,uint32_t n){
 if(n>INT_MAX||(!out&&n))return -EINVAL;
 if(f->poisoned||f->close_uncertain)return -EIO;
 if(n>INT_MAX-f->position)return -EOVERFLOW;
 uint32_t done=0;
 while(done<n){
  PeaqDoomCachedBlock *b=hit(c,f,0);
  if(!b){
   if(f->remote_position!=f->position){
    int32_t at=remote(c,DOOM_IO_SEEK,f->fd,f->position,0);
    if(at<0||(uint32_t)at!=f->position){poison(c,f);return done?(int32_t)done:at<0?at:-EIO;}
    f->remote_position=f->position;
   }
   b=&c->blocks[c->victim];c->victim=(c->victim+1)%PEAQ_DOOM_CACHE_BLOCKS;
   b->fd=0;b->start=f->position;
   uint32_t want=PEAQ_DOOM_CACHE_BYTES;
   if(want>INT_MAX-f->position)want=INT_MAX-f->position;
   int32_t got=remote(c,DOOM_IO_READ,f->fd,(uint32_t)(uintptr_t)b->data,want);
   if(got<0||(uint32_t)got>want){poison(c,f);return done?(int32_t)done:got<0?got:-EIO;}
   if(!got)return (int32_t)done; /* zero bytes is EOF only after success */
   b->length=(uint32_t)got;b->short_read=(uint32_t)got<want;b->fd=f->fd;
   f->remote_position=f->position+(uint32_t)got;
  }
  uint32_t available=b->length-(f->position-b->start),take=n-done;
  if(take>available)take=available;
  for(uint32_t i=0;i<take;i++)out[done+i]=b->data[f->position-b->start+i];
  f->position+=take;done+=take;
  /* Honor a native short read at the end of this cached span. A subsequent
   * call may try again; short != a manufactured permanent EOF marker. */
  if(take==available&&b->short_read)break;
 }
 return (int32_t)done;
}
int32_t peaq_doom_read_cache_io(PeaqDoomReadCache *c,uint32_t op,uint32_t a,uint32_t b,uint32_t d){
 if(!c||!c->remote)return -EINVAL;
 if(op==DOOM_IO_OPEN){
  PeaqDoomCachedFile *slot=0;
  for(unsigned i=0;i<PEAQ_DOOM_FILE_SLOTS;i++)if(!c->files[i].fd){slot=&c->files[i];break;}
  if(!slot)return -EMFILE;
  int32_t fd=remote(c,op,a,b,d);
  if(fd>=3)*slot=(PeaqDoomCachedFile){(uint32_t)fd,0,0,0,0};
  return fd;
 }
 /* In particular EXIT's status is not a file descriptor, even if it equals
  * an open/uncertain fd. Always deliver exit notifications to the guard host. */
 if(op!=DOOM_IO_READ&&op!=DOOM_IO_SEEK&&op!=DOOM_IO_CLOSE&&op!=DOOM_IO_FSTAT)return remote(c,op,a,b,d);
 PeaqDoomCachedFile *f=find(c,a);
 if(!f)return remote(c,op,a,b,d);
 if(f->close_uncertain)return -EIO;
 if(op==DOOM_IO_READ)return read_file(c,f,(uint8_t *)(uintptr_t)b,d);
 if(op==DOOM_IO_SEEK)return seek_file(c,f,(int32_t)b,d);
 if(op==DOOM_IO_CLOSE){
  int32_t result=remote(c,op,a,b,d);discard(c,a);
  if(!result)*f=(PeaqDoomCachedFile){0};else{f->close_uncertain=1;f->poisoned=1;}
  return result;
 }
 return remote(c,op,a,b,d);
}
