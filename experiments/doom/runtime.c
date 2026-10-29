/* Newlib boundary for an isolated game runtime. No firmware addresses or
 * physical devices: the caller owns the heap and sandboxed file services. */
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef PEAQ_DOOM_GUARDED_EXIT
#include "guard.h"
#endif

typedef int32_t (*DoomIo)(uint32_t op, uint32_t a, uint32_t b, uint32_t c);
static DoomIo io;
static uintptr_t heap_start, heap_end, heap_pos;
enum { IO_OPEN, IO_CLOSE, IO_READ, IO_WRITE, IO_SEEK, IO_FSTAT,
       IO_STAT, IO_UNLINK, IO_RENAME, IO_MKDIR, IO_EXIT };

int peaq_doom_io_bind(DoomIo callback, void *begin, void *end) {
    uintptr_t lo=(uintptr_t)begin, hi=(uintptr_t)end;
    if (!callback || (lo&7) || hi<=lo) return 0;
    io=callback;heap_start=heap_pos=lo;heap_end=hi;return 1;
}
uint32_t peaq_doom_heap_used(void) { return heap_pos-heap_start; }
static int32_t dispatch(unsigned op, uint32_t a, uint32_t b, uint32_t c) {
    int32_t r=io?io(op,a,b,c):-ENOSYS;
    if(r<0){errno=-r;return -1;}return r;
}
void *_sbrk(ptrdiff_t amount) {
    uintptr_t old=heap_pos;
    if ((amount>=0 && (uintptr_t)amount>heap_end-old) ||
        (amount<0 && (uintptr_t)(-(amount+1))+1>old-heap_start)) {
        errno=ENOMEM;return (void *)-1;
    }
    heap_pos+=amount;return (void *)old;
}
int _open(const char *p,int flags,int mode){return dispatch(IO_OPEN,(uintptr_t)p,flags,mode);}
int _close(int fd){return dispatch(IO_CLOSE,fd,0,0);}
int _read(int fd,void *p,size_t n){return dispatch(IO_READ,fd,(uintptr_t)p,n);}
int _write(int fd,const void *p,size_t n){return dispatch(IO_WRITE,fd,(uintptr_t)p,n);}
off_t _lseek(int fd,off_t pos,int whence){return dispatch(IO_SEEK,fd,pos,whence);}
int _fstat(int fd,struct stat *s){
    int n=dispatch(IO_FSTAT,fd,0,0);if(n<0)return -1;
    *s=(struct stat){0};s->st_mode=fd<3?S_IFCHR:S_IFREG;s->st_size=n;s->st_blksize=4096;return 0;
}
int _stat(const char *p,struct stat *s){
    int n=dispatch(IO_STAT,(uintptr_t)p,0,0);if(n<0)return -1;
    *s=(struct stat){0};s->st_mode=S_IFREG;s->st_size=n;s->st_blksize=4096;return 0;
}
int _isatty(int fd){return fd>=0&&fd<3;}
int _unlink(const char *p){return dispatch(IO_UNLINK,(uintptr_t)p,0,0);}
int rename(const char *a,const char *b){return dispatch(IO_RENAME,(uintptr_t)a,(uintptr_t)b,0);}
int mkdir(const char *p,mode_t mode){return dispatch(IO_MKDIR,(uintptr_t)p,mode,0);}
int system(const char *command){(void)command;errno=ENOSYS;return -1;}
int _getpid(void){return 1;}
int _kill(int pid,int sig){(void)pid;(void)sig;errno=ENOSYS;return -1;}
__attribute__((noreturn)) void _exit(int status){
    dispatch(IO_EXIT,status,0,0);
#ifdef PEAQ_DOOM_GUARDED_EXIT
    peaq_doom_unwind(status);
#else
    for(;;){}
#endif
}
