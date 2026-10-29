#include "fileio.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>

#define FILES_READY 0x4446494fu
#define SFS_READ_EXISTING 0x31u
/* Callback error/flag numbers belong to the linked Arm Newlib, not the
 * computer's host C runtime. Reject a mismatched cross-build environment. */
_Static_assert(O_RDONLY==0&&O_BINARY==0x10000&&EOVERFLOW==139&&ENOSYS==88,
               "Doom file adapter requires the engine's Arm Newlib ABI");
_Static_assert(INT_MAX==0x7fffffff&&sizeof(void *)==4,"Doom I/O uses 32-bit words");

static PeaqDoomFile *lookup(PeaqDoomFiles *f,uint32_t fd){
 for(unsigned i=0;i<PEAQ_DOOM_FILE_SLOTS;i++)
  if(f->files[i].handle&&f->files[i].fd==fd)return &f->files[i];
 return 0;
}
static int path(PeaqDoomFiles *f,uint16_t out[96],const char *name){
 return peaq_game_file_path(out,f->drive,f->folder,name)?0:-EACCES;
}
int peaq_doom_files_init(PeaqDoomFiles *f,const PeaqFileApi *api,char drive,const char *folder){
 uint16_t checked[96];
 if(!f||!api||!api->open||!api->read||!api->close||!api->seek||!api->position||!api->size)return -EINVAL;
 if(f->initialized)return -EBUSY;
 if(!peaq_game_file_path(checked,drive,folder,"probe"))return -EINVAL;
 /* Keep this host adapter freestanding, without importing a libc memset. */
 for(unsigned i=0;i<sizeof(*f);i++)((volatile uint8_t *)f)[i]=0;
 f->api=api;f->drive=drive;
 for(unsigned i=0;folder[i];i++)f->folder[i]=folder[i];
 f->next_fd=3;f->initialized=FILES_READY;return 0;
}
static int open_file(PeaqDoomFiles *f,const char *name,uint32_t flags){
 uint16_t full[96];int error=path(f,full,name);if(error)return error;
 /* Arm Newlib fopen("rb") adds O_BINARY. It changes no native access rights;
  * every flag besides this read-format bit must still be rejected. */
 if((flags&~(uint32_t)O_BINARY)!=(uint32_t)O_RDONLY)return -EROFS;
 PeaqDoomFile *slot=0;
 for(unsigned i=0;i<PEAQ_DOOM_FILE_SLOTS;i++)if(!f->files[i].handle){slot=&f->files[i];break;}
 if(!slot||f->next_fd>(uint32_t)INT_MAX)return -EMFILE;
 uint32_t handle=f->api->open(full,SFS_READ_EXISTING,0,0);if(!handle)return -EIO;
 *slot=(PeaqDoomFile){f->next_fd++,handle,0};return (int)slot->fd;
}
static int close_file(PeaqDoomFiles *f,PeaqDoomFile *slot){
 if(f->api->close(slot->handle))return -EIO;
 *slot=(PeaqDoomFile){0};return 0;
}
static int file_size(PeaqDoomFiles *f,PeaqDoomFile *slot){
 uint32_t size=0;if(f->api->size(slot->handle,&size))return -EIO;
 if(size>(uint32_t)INT_MAX)return -EOVERFLOW;
 return (int)size;
}
static int position(PeaqDoomFiles *f,PeaqDoomFile *slot){
 int32_t current=-1;if(f->api->position(slot->handle,0,&current))return -EIO;
 if(current<0)return -EOVERFLOW;
 return current;
}
static int read_file(PeaqDoomFiles *f,PeaqDoomFile *slot,void *buffer,uint32_t n){
 if(n>(uint32_t)INT_MAX||(!buffer&&n))return -EINVAL;
 if(slot->poisoned)return -EIO;
 if(!n)return 0;
 int current=position(f,slot);if(current<0)return current;
 if(n>(uint32_t)INT_MAX-(uint32_t)current)return -EOVERFLOW;
 uint32_t done=0;
 while(done<n){
  uint32_t got=0,want=n-done;if(want>4096)want=4096;
  uint32_t error=f->api->read(slot->handle,(uint8_t *)buffer+done,want,&got,0);
  if(error||got>want){slot->poisoned=1;return done?(int)done:-EIO;}
  done+=got;if(got<want)break;
 }
 return (int)done;
}
static int seek_file(PeaqDoomFiles *f,PeaqDoomFile *slot,int32_t offset,uint32_t origin){
 if(origin>2)return -EINVAL;
 int base=origin==0?0:origin==1?position(f,slot):file_size(f,slot);if(base<0)return base;
 int64_t target=(int64_t)base+offset;if(target<0)return -EINVAL;
 if(target>INT_MAX)return -EOVERFLOW;
 /* This exact PeaqFileApi signature places signed offset in r2:r3 and
  * origin on the stack. Never narrow the native seek to a 32-bit ABI. */
 if(f->api->seek(slot->handle,(int64_t)offset,origin)){slot->poisoned=1;return -EIO;}
 int current=position(f,slot);
 if(current<0||current!=target){slot->poisoned=1;return current<0?current:-EIO;}
 slot->poisoned=0;return current;
}
int32_t peaq_doom_files_io(PeaqDoomFiles *f,uint32_t op,uint32_t a,uint32_t b,uint32_t c){
 if(!f||f->initialized!=FILES_READY)return -EINVAL;
 if(op==DOOM_IO_EXIT){if(!f->exit_seen){f->exit_seen=1;f->exit_status=(int32_t)a;}return 0;}
 if(f->exit_seen)return -EPIPE;
 if(op==DOOM_IO_OPEN)return open_file(f,(const char *)(uintptr_t)a,b);
 if(op==DOOM_IO_STAT){
  int fd=open_file(f,(const char *)(uintptr_t)a,O_RDONLY);if(fd<0)return fd;
  PeaqDoomFile *slot=lookup(f,(uint32_t)fd);int size=file_size(f,slot);
  int closed=close_file(f,slot);return size<0?size:closed?closed:size;
 }
 if(op==DOOM_IO_UNLINK||op==DOOM_IO_RENAME||op==DOOM_IO_MKDIR)return -EROFS;
 if(op>DOOM_IO_EXIT)return -ENOSYS;
 if(a<3){
  if(op==DOOM_IO_FSTAT)return 0;
  if(op==DOOM_IO_READ&&a==0)return 0;
  if(op==DOOM_IO_WRITE&&(a==1||a==2))return c<=(uint32_t)INT_MAX&&(!c||b)?(int32_t)c:-EINVAL;
  if(op==DOOM_IO_SEEK)return -ESPIPE;
  return -EBADF;
 }
 PeaqDoomFile *slot=lookup(f,a);if(!slot)return -EBADF;
 if(op==DOOM_IO_CLOSE)return close_file(f,slot);
 if(op==DOOM_IO_READ)return read_file(f,slot,(void *)(uintptr_t)b,c);
 if(op==DOOM_IO_WRITE)return -EROFS;
 if(op==DOOM_IO_SEEK)return seek_file(f,slot,(int32_t)b,c);
 if(op==DOOM_IO_FSTAT)return file_size(f,slot);
 return -ENOSYS;
}
int peaq_doom_files_close_all(PeaqDoomFiles *f){
 if(!f||f->initialized!=FILES_READY)return -EINVAL;
 int error=0;
 for(unsigned i=0;i<PEAQ_DOOM_FILE_SLOTS;i++)
  if(f->files[i].handle&&close_file(f,&f->files[i]))error=-EIO;
 return error;
}
