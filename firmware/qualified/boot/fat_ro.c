/* Read-only cold FAT32 file layer, derived from the qualified fixed probe's
 * field/bounds rules. No device access: caller supplies a bounded sector reader.
 * Full bounded chain validation at open; no FAT walk during read or seek.
 */
#include "fat_ro.h"
#include <stddef.h>
#define MAGIC 0x31524650u
static uint16_t u16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static int fail(PeaqFat *f,int error){if(!f->error)f->error=error;return error;}
static void zero(void *p,uint32_t n){uint8_t *b=p;while(n--)*b++=0;}
static int separate(const void *p,uint32_t n,const void *q,uint32_t m){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
 return a+n>=a&&b+m>=b&&(a+n<=b||b+m<=a);
}
static int sector(PeaqFat *f,uint32_t lba,uint8_t *dst){
 if(f->error)return f->error;
 if(lba>=f->capacity)return fail(f,PEAQ_FAT_FORMAT);
 if(f->reads>=f->budget)return fail(f,PEAQ_FAT_LIMIT);
 ++f->reads;
 if(f->host.read_sector(f->host.cookie,lba,dst)!=1)return fail(f,PEAQ_FAT_IO);
 return 1;
}
static int data_sector(PeaqFat *f,uint32_t lba){
 if(f->data_valid&&f->data_cached==lba)return 1;
 f->data_valid=0;
 int r=sector(f,lba,f->data);if(r!=1)return r;
 f->data_cached=lba;f->data_valid=1;return 1;
}
static int valid_cluster(PeaqFat *f,uint32_t c){return c>=2&&c-2<f->cluster_count;}
static int next(PeaqFat *f,uint32_t c,uint32_t *result){
 if(!valid_cluster(f,c)||c/128>=f->fat_sectors)return fail(f,PEAQ_FAT_FORMAT);
 uint32_t lba=f->fat_lba+c/128;
 if(!f->fat_valid||f->fat_cached!=lba){
  f->fat_valid=0;int r=sector(f,lba,f->fat);if(r!=1)return r;
  f->fat_cached=lba;f->fat_valid=1;
 }
 uint32_t n=u32(f->fat+(c&127u)*4)&0x0fffffffu;
 if(n<0x0ffffff8u&&(!valid_cluster(f,n)||n==c))return fail(f,PEAQ_FAT_FORMAT);
 *result=n;return 1;
}
int peaq_fat_mount(PeaqFat *f,const PeaqFatHost *h){
 if(!f||!h||((uintptr_t)f&3u)||((uintptr_t)h&3u)||!h->read_sector||!h->capacity_sectors||!h->read_budget||h->read_budget>PEAQ_FAT_SECTOR_BUDGET_MAX||
    !separate(f,sizeof(*f),h,sizeof(*h)))return PEAQ_FAT_ARGUMENT;
 if(f->magic==MAGIC)return PEAQ_FAT_BUSY;
 zero(f,sizeof(*f));f->magic=MAGIC;f->host=*h;f->capacity=h->capacity_sectors;f->budget=h->read_budget;
 int result=data_sector(f,0);if(result!=1)return result;
 const uint8_t *p=f->data+446;uint32_t partition=u32(p+8),length=u32(p+12);
 if(u16(f->data+510)!=0xaa55||(p[0]!=0&&p[0]!=0x80)||(p[4]!=0xb&&p[4]!=0xc)||
    !partition||!length||partition>=f->capacity||length>f->capacity-partition)return fail(f,PEAQ_FAT_FORMAT);
 f->partition=partition;result=data_sector(f,partition);if(result!=1)return result;
 p=f->data;uint32_t reserved=u16(p+14),nfats=p[16],spc=p[13],fsz=u32(p+36),total=u32(p+32),flags=u16(p+40);
 if(u16(p+510)!=0xaa55||u16(p+11)!=512||!reserved||!spc||spc>128||(spc&(spc-1))||
    (nfats!=1&&nfats!=2)||u16(p+17)||u16(p+19)||u16(p+22)||u16(p+42)||
    (flags&~0x8fu)||!fsz||fsz>0x200000u||total>length||u32(p+28)!=partition)return fail(f,PEAQ_FAT_FORMAT);
 uint32_t overhead=reserved+nfats*fsz;
 if(total<=overhead)return fail(f,PEAQ_FAT_FORMAT);
 f->total=total;f->spc=spc;f->fat_sectors=fsz;f->fat_lba=partition+reserved;
 f->data_lba=partition+overhead;f->cluster_count=(total-overhead)/spc;f->root=u32(p+44);
 if(f->cluster_count<65525u||f->cluster_count>0x0fffffedu||f->cluster_count+2>fsz*128u||!valid_cluster(f,f->root))return fail(f,PEAQ_FAT_FORMAT);
 if(flags&0x80u){if((flags&15u)>=nfats)return fail(f,PEAQ_FAT_FORMAT);f->fat_lba+=(flags&15u)*fsz;}
 f->mounted=1;return 1;
}
static int find(PeaqFat *f,uint32_t directory,const char name[11],unsigned want_directory,uint32_t *first,uint32_t *bytes){
 uint32_t reads=0;
 for(unsigned hops=0;hops<32;++hops){
  if(!valid_cluster(f,directory))return fail(f,PEAQ_FAT_FORMAT);
  uint32_t lba=f->data_lba+(directory-2)*f->spc;
  for(uint32_t s=0;s<f->spc;++s){
   if(++reads>256)return fail(f,PEAQ_FAT_LIMIT);
   int r=data_sector(f,lba+s);if(r!=1)return r;
   for(unsigned at=0;at<512;at+=32){
    const uint8_t *p=f->data+at;unsigned attr=p[11],i;
    if(!p[0])return PEAQ_FAT_ABSENT;
    if(p[0]==0xe5||(attr&8)||attr==15)continue;
    for(i=0;i<11&&p[i]==(uint8_t)name[i];++i){}
    if(i==11){
     if(((attr&16)!=0)!=want_directory)return fail(f,PEAQ_FAT_FORMAT);
     *first=((uint32_t)u16(p+20)<<16)|u16(p+26);*bytes=u32(p+28);
     if(!valid_cluster(f,*first))return fail(f,PEAQ_FAT_FORMAT);
     return 1;
    }
   }
  }
  int r=next(f,directory,&directory);if(r!=1)return r;
  if(directory>=0x0ffffff8u)return PEAQ_FAT_ABSENT;
 }
 return fail(f,PEAQ_FAT_LIMIT);
}
static int game_folder(PeaqFat *f){
 if(f->folder_known)return f->folder_absent?0:1;
 uint32_t games,size;int r=find(f,f->root,"GAMES      ",1,&games,&size);
 if(r==1)r=find(f,games,"SMARAGD    ",1,&f->folder,&size);
 if(r>=0){f->folder_known=1;f->folder_absent=r==0;}
 return r;
}
int peaq_fat_open(PeaqFat *f,uint32_t role,uint32_t *handle,uint32_t *bytes){
 static const char names[3][12]={"PLAY    BIN","SAVE0   DAT","SAVE1   DAT"};
 if(!f||!handle||!bytes||((uintptr_t)handle&3u)||((uintptr_t)bytes&3u)||
    !separate(f,sizeof(*f),handle,4)||!separate(f,sizeof(*f),bytes,4)||
    !separate(handle,4,bytes,4))return PEAQ_FAT_ARGUMENT;
 *handle=*bytes=0;
 if(f->magic!=MAGIC||!f->mounted||role>=3)return PEAQ_FAT_ARGUMENT;
 if(f->error)return f->error;
 PeaqFatFile *file=&f->files[role];if(file->live)return PEAQ_FAT_BUSY;
 int result=game_folder(f);if(result!=1)return result;
 uint32_t first,size;result=find(f,f->folder,names[role],0,&first,&size);if(result!=1)return result;
 uint32_t max=role==PEAQ_FAT_ENGINE?16u*1024u*1024u:131072u+20u;
 if(!size||size>max)return fail(f,PEAQ_FAT_LIMIT);
 uint32_t cluster_bytes=f->spc*512u,n=(size-1)/cluster_bytes+1;
 if(n>PEAQ_FAT_CHAIN_MAX)return fail(f,PEAQ_FAT_LIMIT);
 /* Exact size-derived chain length plus mandatory final EOC excludes cycles,
  * early truncation and extra tail allocation before exposing a handle. */
 uint32_t c=first;
 for(uint32_t i=0;i<n;++i){
  if(!valid_cluster(f,c))return fail(f,PEAQ_FAT_FORMAT);
  file->chain[i]=c;result=next(f,c,&c);if(result!=1)return result;
  if((c>=0x0ffffff8u)!=(i+1==n))return fail(f,PEAQ_FAT_FORMAT);
 }
 if(f->next_token>=0x3fffffffu)return fail(f,PEAQ_FAT_LIMIT);
 file->token=(++f->next_token<<2)|(role+1u);file->role=role;file->bytes=size;
 file->position=0;file->clusters=n;file->live=1;
 *handle=file->token;*bytes=size;return 1;
}
static PeaqFatFile *get(PeaqFat *f,uint32_t token){
 if(!f||f->magic!=MAGIC||!f->mounted||!(token&3u))return 0;
 PeaqFatFile *file=&f->files[(token&3u)-1];
 return file->live&&file->token==token?file:0;
}
int peaq_fat_close(PeaqFat *f,uint32_t handle){
 PeaqFatFile *file=get(f,handle);if(!file)return PEAQ_FAT_HANDLE;
 file->live=0;return 1;
}
int peaq_fat_seek(PeaqFat *f,uint32_t handle,uint32_t at){
 PeaqFatFile *file=get(f,handle);if(!file)return PEAQ_FAT_HANDLE;
 if(f->error)return f->error;
 if(at>file->bytes)return PEAQ_FAT_ARGUMENT;
 file->position=at;return 1;
}
int peaq_fat_read(PeaqFat *f,uint32_t handle,void *output,uint32_t want,uint32_t *got){
 if(!f||!got||((uintptr_t)got&3u)||!separate(f,sizeof(*f),got,4)||
    !separate(output,want,got,4))return PEAQ_FAT_ARGUMENT;
 *got=0;
 PeaqFatFile *file=get(f,handle);if(!file)return PEAQ_FAT_HANDLE;
 if(f->error)return f->error;
 if(want>PEAQ_FAT_READ_MAX||(!output&&want)||!separate(output,want,f,sizeof(*f)))return PEAQ_FAT_ARGUMENT;
 uint32_t remaining=file->bytes-file->position;if(want>remaining)want=remaining;
 uint8_t *dst=output;uint32_t done=0,position=file->position;
 while(done<want){
  uint32_t index=position/(f->spc*512u),within=position%(f->spc*512u);
  if(index>=file->clusters)return fail(f,PEAQ_FAT_FORMAT);
  uint32_t lba=f->data_lba+(file->chain[index]-2)*f->spc+within/512u;
  int r=data_sector(f,lba);if(r!=1)return r;
  uint32_t offset=position&511u,n=512u-offset;if(n>want-done)n=want-done;
  for(uint32_t i=0;i<n;++i)dst[done+i]=f->data[offset+i];
  done+=n;position+=n;
 }
 file->position=position;*got=done;return 1;
}
int peaq_fat_module_read(void *cookie,void *output,uint32_t want,uint32_t *got){
 PeaqFatStream *s=cookie;
 if(!got)return 0;
 if(!s||!s->fs)return 0;
 return peaq_fat_read(s->fs,s->handle,output,want,got)==1;
}
int peaq_fat_save_read(void *cookie,uint32_t slot,uint32_t at,void *output,uint32_t want,uint32_t *got){
 PeaqFatSaves *s=cookie;
 if(!s||!s->fs||!got||slot>1||want>PEAQ_FAT_READ_MAX||((uintptr_t)got&3u)||
    !separate(s,sizeof(*s),got,4)||!separate(s,sizeof(*s),output,want)||
    !separate(s->fs,sizeof(*s->fs),got,4)||!separate(output,want,got,4))return -1;
 *got=0;
 if(s->fs->error)return -1;
 if(!(s->known&(1u<<slot))){
  int r=peaq_fat_open(s->fs,PEAQ_FAT_SAVE0+slot,&s->handle[slot],&s->bytes[slot]);
  if(r<0)return -1;
  s->known|=1u<<slot;if(!r)s->absent|=1u<<slot;
 }
 if(s->absent&(1u<<slot))return 0;
 if(at>s->bytes[slot])at=s->bytes[slot];
 if(peaq_fat_seek(s->fs,s->handle[slot],at)!=1)return -1;
 return peaq_fat_read(s->fs,s->handle[slot],output,want,got)==1?1:-1;
}
