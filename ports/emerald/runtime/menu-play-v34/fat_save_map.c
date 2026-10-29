/* Extend the frozen reader in a separate translation unit; do not link both.
 * No modifications to an already qualified reader or device image. */
#include "fat_ro.c"
#include "fat_save_map.h"
#define MAP_MAGIC 0x314d5350u
static int map_fail(PeaqFat *f,PeaqFatSaveMap *m,int code){
 m->valid=0;m->error=(uint32_t)-code;return fail(f,code);
}
static int mapped_cluster(PeaqFat *f,uint32_t c){
 for(unsigned slot=1;slot<=2;++slot)
  for(unsigned i=0;i<f->files[slot].clusters;++i)
   if(f->files[slot].chain[i]==c)return (int)slot;
 return 0;
}
static int audited_next(PeaqFat *f,PeaqFatSaveMap *m,uint32_t c,uint32_t *n){
 if(++m->hops>PEAQ_SAVE_MAP_HOPS)return map_fail(f,m,PEAQ_FAT_LIMIT);
 int r=next(f,c,n);if(r!=1)return map_fail(f,m,r);
 if(m->mirror_base){
  uint32_t lba=m->mirror_base+c/128u;
  if(!m->mirror_valid||m->mirror_cached!=lba){
   m->mirror_valid=0;r=sector(f,lba,m->mirror);if(r!=1)return map_fail(f,m,r);
   m->mirror_cached=lba;m->mirror_valid=1;
  }
  /* Compare the entire touched FAT sector, including reserved high bits. */
  for(unsigned i=0;i<512;++i)if(m->mirror[i]!=f->fat[i])return map_fail(f,m,PEAQ_FAT_FORMAT);
 }
 return 1;
}
static int file_chain(PeaqFat *f,PeaqFatSaveMap *m,uint32_t first,uint32_t bytes,unsigned owner){
 if(!bytes)return first?map_fail(f,m,PEAQ_FAT_FORMAT):1;
 uint32_t n=(bytes-1)/(f->spc*512u)+1,c=first;
 if(n>PEAQ_SAVE_MAP_HOPS-m->hops)return map_fail(f,m,PEAQ_FAT_LIMIT);
 for(uint32_t i=0;i<n;++i){
  if(!valid_cluster(f,c))return map_fail(f,m,PEAQ_FAT_FORMAT);
  int target=mapped_cluster(f,c);
  if((target&&target!=(int)owner)||(owner&&(!target||i>=f->files[owner].clusters||f->files[owner].chain[i]!=c)))
   return map_fail(f,m,PEAQ_FAT_FORMAT);
  int r=audited_next(f,m,c,&c);if(r!=1)return r;
  if((c>=0x0ffffff8u)!=(i+1==n))return map_fail(f,m,PEAQ_FAT_FORMAT);
 }
 return 1;
}
static int named(const uint8_t *p,const char *name){
 for(unsigned i=0;i<11;++i)if(p[i]!=(uint8_t)name[i])return 0;
 return 1;
}
static int audit_directory(PeaqFat *f,PeaqFatSaveMap *m,PeaqSaveDirectory dir){
 uint32_t c=dir.first,ended=0;
 for(unsigned hop=0;hop<32;++hop){
  if(!valid_cluster(f,c)||mapped_cluster(f,c))return map_fail(f,m,PEAQ_FAT_FORMAT);
  if(m->dir_clusters==PEAQ_SAVE_MAP_DIR_CLUSTERS)return map_fail(f,m,PEAQ_FAT_LIMIT);
  for(unsigned i=0;i<m->dir_clusters;++i)if(m->seen_dirs[i]==c)return map_fail(f,m,PEAQ_FAT_FORMAT);
  m->seen_dirs[m->dir_clusters++]=c;
  for(unsigned s=0;!ended&&s<f->spc;++s){
   int r=data_sector(f,f->data_lba+(c-2)*f->spc+s);if(r!=1)return map_fail(f,m,r);
   for(unsigned at=0;at<512;at+=32){
    const uint8_t *p=f->data+at;unsigned attr=p[11];
    if(!p[0]){ended=1;break;}
    if(p[0]==0xe5||attr==15)continue;
    if(attr&0xc0u)return map_fail(f,m,PEAQ_FAT_FORMAT);
    if(attr&8u){if(attr&16u)return map_fail(f,m,PEAQ_FAT_FORMAT);continue;}
    if(++m->entries>4096u)return map_fail(f,m,PEAQ_FAT_LIMIT);
    uint32_t first=((uint32_t)u16(p+20)<<16)|u16(p+26),bytes=u32(p+28);
    if(named(p,".          ")||named(p,"..         ")){
     unsigned parent=p[1]=='.';uint32_t expected=parent?dir.parent:dir.first;
     if(!(attr&16u)||bytes||(first!=expected&&!(parent&&expected==f->root&&!first)))
      return map_fail(f,m,PEAQ_FAT_FORMAT);
     continue;
    }
    if(attr&16u){
     if(bytes||!valid_cluster(f,first)||mapped_cluster(f,first))return map_fail(f,m,PEAQ_FAT_FORMAT);
     if(m->directories==PEAQ_SAVE_MAP_DIRS)return map_fail(f,m,PEAQ_FAT_LIMIT);
     m->queue[m->directories++]=(PeaqSaveDirectory){first,dir.first};
    }else{
     unsigned owner=0;
     if(dir.first==f->folder){
      if(named(p,"ENGINE0 SAV"))owner=1;
      if(named(p,"ENGINE1 SAV"))owner=2;
     }
     if(owner){
      if((attr&1u)||bytes!=PEAQ_COLD_SAVE_RECORD||(m->seen_slots&(1u<<owner)))
       return map_fail(f,m,PEAQ_FAT_FORMAT);
      m->seen_slots|=1u<<owner;
     }
     r=file_chain(f,m,first,bytes,owner);if(r!=1)return r;
    }
   }
  }
  int r=audited_next(f,m,c,&c);if(r!=1)return r;
  if(c>=0x0ffffff8u)return 1;
 }
 return map_fail(f,m,PEAQ_FAT_LIMIT);
}
int peaq_fat_save_map(PeaqFat *f,PeaqFatSaveMap *m){
 if(!f||!m||((uintptr_t)f&3u)||((uintptr_t)m&3u)||!separate(f,sizeof(*f),m,sizeof(*m))||
    f->magic!=MAGIC||!f->mounted)return PEAQ_FAT_ARGUMENT;
 if(f->error)return f->error;
 if(m->magic==MAP_MAGIC)return PEAQ_FAT_BUSY;
 zero(m,sizeof(*m));m->magic=MAP_MAGIC;
 uint32_t handle,bytes;
 for(unsigned slot=1;slot<=2;++slot){
  if(f->files[slot].live)return map_fail(f,m,PEAQ_FAT_BUSY);
  int r=peaq_fat_open(f,slot,&handle,&bytes);
  if(r!=1)return map_fail(f,m,r<0?r:PEAQ_FAT_FORMAT);
  if(bytes!=PEAQ_COLD_SAVE_RECORD)return map_fail(f,m,PEAQ_FAT_FORMAT);
  for(unsigned i=0;i<PEAQ_COLD_SAVE_SECTORS;++i){
   uint32_t c=f->files[slot].chain[i/f->spc];
   m->sectors[slot-1][i]=f->data_lba+(c-2)*f->spc+i%f->spc;
  }
 }
 for(unsigned i=0;i<f->files[1].clusters;++i)
  for(unsigned j=0;j<f->files[2].clusters;++j)
   if(f->files[1].chain[i]==f->files[2].chain[j])return map_fail(f,m,PEAQ_FAT_FORMAT);
 int r=data_sector(f,f->partition);if(r!=1)return map_fail(f,m,r);
 if(f->data[16]==2&&!(u16(f->data+40)&0x80u))m->mirror_base=f->fat_lba+f->fat_sectors;
 m->queue[0]=(PeaqSaveDirectory){f->root,f->root};m->directories=1;
 for(unsigned q=0;q<m->directories;++q){r=audit_directory(f,m,m->queue[q]);if(r!=1)return r;}
 if(m->seen_slots!=6)return map_fail(f,m,PEAQ_FAT_FORMAT);
 m->valid=1;return 1;
}
