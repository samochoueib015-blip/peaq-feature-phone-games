/* Includes the unchanged qualified reader and save audit; link this unit
 * instead of fat_save_map.c or fat_ro.c, never alongside either one. */
#include "fat_save_map.c"
#include "fat_report_map.h"
#define REPORT_MAGIC 0x31524d50u
_Static_assert(sizeof(PeaqFatReportMap)<=1024,"report audit state exceeds 1 KiB");
static int report_fail(PeaqFatReportMap *r,int code){
 r->valid=0;r->sector=0;r->error=(uint32_t)-code;return code;
}
static int report_next(PeaqFat *f,PeaqFatReportMap *r,uint32_t c,uint32_t *n){
 if(++r->hops>PEAQ_SAVE_MAP_HOPS)return report_fail(r,PEAQ_FAT_LIMIT);
 int result=next(f,c,n);if(result!=1)return report_fail(r,result);
 if(r->mirror_base){
  uint32_t lba=r->mirror_base+c/128u;
  if(!r->mirror_valid||r->mirror_cached!=lba){
   r->mirror_valid=0;result=sector(f,lba,r->mirror);
   if(result!=1)return report_fail(r,result);
   r->mirror_cached=lba;r->mirror_valid=1;
  }
  for(unsigned i=0;i<512;++i)
   if(r->mirror[i]!=f->fat[i])return report_fail(r,PEAQ_FAT_FORMAT);
 }
 return 1;
}
static int report_file(PeaqFat *f,PeaqFatReportMap *r,uint32_t first,uint32_t bytes,unsigned owner){
 if(!bytes)return first?report_fail(r,PEAQ_FAT_FORMAT):1;
 uint32_t n=(bytes-1)/(f->spc*512u)+1,c=first;
 if(n>PEAQ_SAVE_MAP_HOPS-r->hops)return report_fail(r,PEAQ_FAT_LIMIT);
 for(uint32_t i=0;i<n;++i){
  if(!valid_cluster(f,c))return report_fail(r,PEAQ_FAT_FORMAT);
  if((c==r->cluster)!=(owner==3u))return report_fail(r,PEAQ_FAT_FORMAT);
  unsigned target=(unsigned)mapped_cluster(f,c),save_owner=owner<3u?owner:0u;
  if((target&&target!=save_owner)||(save_owner&&(!target||i>=f->files[save_owner].clusters||
      f->files[save_owner].chain[i]!=c)))return report_fail(r,PEAQ_FAT_FORMAT);
  int result=report_next(f,r,c,&c);if(result!=1)return result;
  if((c>=0x0ffffff8u)!=(i+1==n))return report_fail(r,PEAQ_FAT_FORMAT);
 }
 return 1;
}
static int report_directory(PeaqFat *f,PeaqFatSaveMap *saved,PeaqFatReportMap *r,PeaqSaveDirectory dir){
 uint32_t c=dir.first,ended=0;
 for(unsigned hop=0;hop<32;++hop){
  if(!valid_cluster(f,c)||c==r->cluster||mapped_cluster(f,c))return report_fail(r,PEAQ_FAT_FORMAT);
  if(r->dir_clusters==PEAQ_SAVE_MAP_DIR_CLUSTERS)return report_fail(r,PEAQ_FAT_LIMIT);
  for(unsigned i=0;i<r->dir_clusters;++i)
   if(saved->seen_dirs[i]==c)return report_fail(r,PEAQ_FAT_FORMAT);
  saved->seen_dirs[r->dir_clusters++]=c;
  for(unsigned s=0;!ended&&s<f->spc;++s){
   int result=data_sector(f,f->data_lba+(c-2)*f->spc+s);
   if(result!=1)return report_fail(r,result);
   for(unsigned at=0;at<512;at+=32){
    const uint8_t *p=f->data+at;unsigned attr=p[11];
    if(!p[0]){ended=1;break;}
    if(p[0]==0xe5||attr==15)continue;
    if(attr&0xc0u)return report_fail(r,PEAQ_FAT_FORMAT);
    unsigned is_report=dir.first==f->folder&&named(p,"REPORT  BIN");
    if(is_report&&(attr&0x19u))return report_fail(r,PEAQ_FAT_FORMAT);
    if(attr&8u){if(attr&16u)return report_fail(r,PEAQ_FAT_FORMAT);continue;}
    if(++r->entries>4096u)return report_fail(r,PEAQ_FAT_LIMIT);
    uint32_t first=((uint32_t)u16(p+20)<<16)|u16(p+26),bytes=u32(p+28);
    if(named(p,".          ")||named(p,"..         ")){
     unsigned parent=p[1]=='.';uint32_t expected=parent?dir.parent:dir.first;
     if(!(attr&16u)||bytes||(first!=expected&&!(parent&&expected==f->root&&!first)))
      return report_fail(r,PEAQ_FAT_FORMAT);
     continue;
    }
    if(attr&16u){
     if(bytes||!valid_cluster(f,first)||first==r->cluster||mapped_cluster(f,first))
      return report_fail(r,PEAQ_FAT_FORMAT);
     if(r->directories==PEAQ_SAVE_MAP_DIRS)return report_fail(r,PEAQ_FAT_LIMIT);
     saved->queue[r->directories++]=(PeaqSaveDirectory){first,dir.first};
    }else{
     unsigned owner=0;
     if(dir.first==f->folder){
      if(named(p,"ENGINE0 SAV"))owner=1;
      if(named(p,"ENGINE1 SAV"))owner=2;
     }
     if(owner){
      if((attr&1u)||bytes!=PEAQ_COLD_SAVE_RECORD||(r->seen_slots&(1u<<owner)))
       return report_fail(r,PEAQ_FAT_FORMAT);
      r->seen_slots|=1u<<owner;
     }
     if(is_report){
      if(bytes!=PEAQ_REPORT_FILE_BYTES||first!=r->cluster||r->seen_report)
       return report_fail(r,PEAQ_FAT_FORMAT);
      r->seen_report=1;owner=3;
     }
     result=report_file(f,r,first,bytes,owner);if(result!=1)return result;
    }
   }
  }
  int result=report_next(f,r,c,&c);if(result!=1)return result;
  if(c>=0x0ffffff8u)return 1;
 }
 return report_fail(r,PEAQ_FAT_LIMIT);
}
int peaq_fat_report_map(PeaqFat *f,PeaqFatSaveMap *saved,PeaqFatReportMap *r){
 if(!f||!saved||!r||((uintptr_t)f&3u)||((uintptr_t)saved&3u)||((uintptr_t)r&3u)||
    !separate(f,sizeof(*f),saved,sizeof(*saved))||!separate(f,sizeof(*f),r,sizeof(*r))||
    !separate(saved,sizeof(*saved),r,sizeof(*r))||f->magic!=MAGIC||!f->mounted||
    saved->magic!=MAP_MAGIC||saved->valid!=1||saved->error||saved->seen_slots!=6||
    !f->folder_known||f->folder_absent||!valid_cluster(f,f->folder))return PEAQ_FAT_ARGUMENT;
 if(f->error)return f->error;
 if(r->magic==REPORT_MAGIC)return PEAQ_FAT_BUSY;
 zero(r,sizeof(*r));r->magic=REPORT_MAGIC;
 for(unsigned slot=1;slot<=2;++slot){
  const PeaqFatFile *file=&f->files[slot];
  if(!file->live||file->role!=slot||file->bytes!=PEAQ_COLD_SAVE_RECORD||
     file->clusters!=(PEAQ_COLD_SAVE_RECORD-1)/(f->spc*512u)+1)
   return report_fail(r,PEAQ_FAT_FORMAT);
  for(unsigned i=0;i<PEAQ_COLD_SAVE_SECTORS;++i){
   uint32_t c=file->chain[i/f->spc];
   if(!valid_cluster(f,c)||saved->sectors[slot-1][i]!=f->data_lba+(c-2)*f->spc+i%f->spc)
    return report_fail(r,PEAQ_FAT_FORMAT);
  }
 }
 /* Re-read metadata for this distinct audit. Live handles and positions do
  * not change; all reads still use the original lifetime sector budget. */
 f->fat_valid=f->data_valid=0;
 uint32_t first,bytes;int result=find(f,f->folder,"REPORT  BIN",0,&first,&bytes);
 if(result!=1)return report_fail(r,result<0?result:PEAQ_FAT_FORMAT);
 if(bytes!=PEAQ_REPORT_FILE_BYTES||mapped_cluster(f,first))return report_fail(r,PEAQ_FAT_FORMAT);
 r->cluster=first;r->mirror_base=saved->mirror_base;
 uint32_t n;result=report_next(f,r,first,&n);if(result!=1)return result;
 if(n<0x0ffffff8u)return report_fail(r,PEAQ_FAT_FORMAT);
 saved->queue[0]=(PeaqSaveDirectory){f->root,f->root};r->directories=1;
 for(unsigned q=0;q<r->directories;++q){
  result=report_directory(f,saved,r,saved->queue[q]);if(result!=1)return result;
 }
 if(r->seen_report!=1||r->seen_slots!=6)return report_fail(r,PEAQ_FAT_FORMAT);
 uint32_t lba=f->data_lba+(first-2)*f->spc;
 if(lba<f->data_lba||lba>=f->capacity)return report_fail(r,PEAQ_FAT_FORMAT);
 r->sector=lba;r->valid=1;return 1;
}
