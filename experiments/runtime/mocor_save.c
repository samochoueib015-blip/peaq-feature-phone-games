#include "mocor_save.h"
const PeaqFileAttr peaq_pmp122_file_attr=(PeaqFileAttr)0x886fc449u;

/* Returns absent/present/error only after a separate parent-directory check. */
static int inspect(PeaqMocorSave *s,uint32_t slot,uint16_t *attributes){
 if(!s||!s->files||!s->attributes||s->poisoned||s->handle||slot>1)return -1;
 uint16_t parent=0,attr=0;
 if(s->attributes(s->parent,&parent)||!(parent&0x10))return -1;
 uint32_t status=s->attributes(s->paths[slot],&attr);
 if(status==7)return 0;
 if(status||attr&(0x10|0x08))return -1;
 *attributes=attr;return 1;
}
static int close_handle(PeaqMocorSave *s){
 if(s->files->close(s->handle)){s->poisoned=1;return 0;}
 s->handle=0;return 1;
}
static int read_slot(void *cookie,uint32_t slot,uint32_t offset,void *data,uint32_t n,uint32_t *got){
 if(!got)return -1;*got=0;
 PeaqMocorSave *s=cookie;uint16_t attr=0;
 if(!data||!n||n>4096||offset>PEAQ_SAVE_MAX+20||n>PEAQ_SAVE_MAX+21-offset)return -1;
 int present=inspect(s,slot,&attr);
 if(present!=1)return present;
 s->handle=s->files->open(s->paths[slot],0x31,0,0);
 if(!s->handle)return -1;
 int ok=peaq_file_read_at(s->files,s->handle,offset,data,n,got);
 if(!close_handle(s))ok=0;
 if(!ok){*got=0;return -1;}
 return 1;
}
static int begin_slot(void *cookie,uint32_t slot){
 PeaqMocorSave *s=cookie;uint16_t attr=0;
 int present=inspect(s,slot,&attr);
 if(present<0||(present&&attr&1))return 0;
 s->handle=s->files->open(s->paths[slot],0x22,0,0);
 s->failed=0;return !!s->handle;
}
static int write_slot(void *cookie,const void *data,uint32_t n,uint32_t *wrote){
 if(!wrote)return 0;*wrote=0;
 PeaqMocorSave *s=cookie;
 if(!s||!s->files||!s->handle||s->poisoned||s->failed)return 0;
 if(!data||!n||n>4096){s->failed=1;return 0;}
 uint32_t actual=0,status=s->files->write(s->handle,data,n,&actual,0);
 if(status||actual>n){s->failed=1;return 0;}
 *wrote=actual;
 if(actual!=n){s->failed=1;return 0;}
 return 1;
}
static int finish_slot(void *cookie){
 PeaqMocorSave *s=cookie;
 if(!s||!s->files||!s->handle||s->poisoned)return 0;
 int ok=!s->failed;
 if(s->files->flush(s->handle,0))ok=0;
 if(!close_handle(s))ok=0;
 s->failed=0;return ok;
}
int peaq_mocor_save_init(PeaqMocorSave *s,const PeaqFileApi *api,PeaqFileAttr attr,char drive,const char *folder){
 if(!s||s->files||!api||!attr||!api->open||!api->read||!api->write||!api->close||!api->seek||!api->flush)return 0;
 if(!peaq_game_file_path(s->paths[0],drive,folder,"engine-save0.dat")||
    !peaq_game_file_path(s->paths[1],drive,folder,"engine-save1.dat"))return 0;
 uint32_t end=0,i=0;
 while(s->paths[0][i]){s->parent[i]=s->paths[0][i];if(s->parent[i]=='\\')end=i;i++;}
 s->parent[end]=0;s->handle=s->failed=s->poisoned=0;s->files=api;s->attributes=attr;
 s->host.cookie=s;s->host.read=read_slot;s->host.begin=begin_slot;
 s->host.write=write_slot;s->host.finish=finish_slot;return 1;
}
