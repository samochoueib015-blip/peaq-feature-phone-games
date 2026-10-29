/* Bounded FAT32 reader for ONE fixed test path. This is not yet the game
 * filesystem backend. No allocation, recursive directories, LFN resolution,
 * metadata updates, file creation, writes or unchecked chain traversal.
 * Standard FAT field layout cross-checked against pinned FPDoom microfat.
 */
#include "sd_diag.h"
static uint8_t sector[512],second[512];
static uint32_t fat_sectors,sectors_per_cluster;
static uint16_t u16(const uint8_t *p) {return p[0]|(uint16_t)p[1]<<8;}
static uint32_t u32(const uint8_t *p) {return u16(p)|(uint32_t)u16(p+2)<<16;}
static int fail(PeaqSdReport *r,unsigned error) {if(!r->error)r->error=error;return 0;}
uint32_t peaq_sd_crc(uint32_t crc,const uint8_t *p,unsigned size) {
 unsigned i,j;
 for(i=0;i<size;++i) {crc^=p[i];for(j=0;j<8;++j)crc=crc>>1^((0u-(crc&1u))&0xedb88320u);}
 return crc;
}
static int same(const uint8_t *a,const uint8_t *b,unsigned n) {
 unsigned i;for(i=0;i<n;++i)if(a[i]!=b[i])return 0;return 1;
}
int peaq_sd_sector0(PeaqSdReport *r) {
 const uint8_t *p;
 if(r->error)return 0;
 if(r->sector_count)return fail(r,SD_ALREADY_RUN);
 ++r->sector_count;r->stage=SD_MBR;
 if(!peaq_sd_read(r,0,sector)||!peaq_sd_read(r,0,second))return 0;
 r->mbr_crc0=~peaq_sd_crc(~0u,sector,512);r->mbr_crc1=~peaq_sd_crc(~0u,second,512);
 if(!same(sector,second,512))return fail(r,SD_DATA_MISMATCH);
 p=sector+446;r->partition_lba=u32(p+8);r->partition_sectors=u32(p+12);
 if(u16(sector+510)!=0xaa55||(p[0]!=0&&p[0]!=0x80)||(p[4]!=0x0b&&p[4]!=0x0c)||
    !r->partition_lba||!r->partition_sectors||r->partition_lba>=r->capacity_lo||
    r->partition_sectors>r->capacity_lo-r->partition_lba)return fail(r,SD_MBR_INVALID);
 r->flags|=SD_F_SECTOR0;return 1;
}
static int cluster_ok(PeaqSdReport *r,uint32_t c) {return c>=2&&c-2<r->cluster_count;}
static int volume(PeaqSdReport *r) {
 uint32_t reserved,fats,total,overhead,root,spc,fsz,flags;
 r->stage=SD_VOLUME;
 if(!peaq_sd_read(r,r->partition_lba,sector))return 0;
 r->boot_crc=~peaq_sd_crc(~0u,sector,512);
 reserved=u16(sector+14);fats=sector[16];spc=sector[13];fsz=u32(sector+36);
 total=u32(sector+32);root=u32(sector+44);flags=u16(sector+40);
 r->bpb_total=total;r->bpb_cluster=spc;r->root_cluster=root;
 if(u16(sector+510)!=0xaa55||u16(sector+11)!=512||!reserved||!spc||spc>128||(spc&(spc-1))||
    (fats!=1&&fats!=2)||u16(sector+17)||u16(sector+19)||u16(sector+22)||u16(sector+42)||
    (flags&~0x008fu)||!fsz||fsz>0x200000u||total>r->partition_sectors||
    u32(sector+28)!=r->partition_lba)return fail(r,SD_BPB_INVALID);
 overhead=reserved+fats*fsz;
 if(total<=overhead)return fail(r,SD_BPB_INVALID);
 r->cluster_count=(total-overhead)/spc;
 if(r->cluster_count<65525u||r->cluster_count>0x0fffffedu||
    r->cluster_count+2>fsz*128u||!cluster_ok(r,root))return fail(r,SD_BPB_INVALID);
 r->fat_lba=r->partition_lba+reserved;
 if(flags&0x80u) {if((flags&15u)>=fats)return fail(r,SD_BPB_INVALID);r->fat_lba+=(flags&15u)*fsz;}
 r->data_lba=r->partition_lba+overhead;
 fat_sectors=fsz;sectors_per_cluster=spc;r->flags|=SD_F_FAT;return 1;
}
static int next(PeaqSdReport *r,uint32_t cluster,uint32_t *result) {
 uint32_t offset=cluster>>7;
 if(!cluster_ok(r,cluster)||offset>=fat_sectors)return fail(r,SD_CHAIN_INVALID);
 if(!peaq_sd_read(r,r->fat_lba+offset,sector))return 0;
 *result=u32(sector+(cluster&127u)*4)&0x0fffffffu;
 if(*result>=0x0ffffff8u)return 1;
 if(!cluster_ok(r,*result)||*result==cluster)return fail(r,SD_CHAIN_INVALID);
 return 1;
}
static int cluster_lba(PeaqSdReport *r,uint32_t c,uint32_t *lba) {
 if(!cluster_ok(r,c))return fail(r,SD_CHAIN_INVALID);
 *lba=r->data_lba+(c-2)*sectors_per_cluster;return 1;
}
static int find(PeaqSdReport *r,uint32_t directory,const uint8_t name[11],unsigned is_directory,uint32_t *found,uint32_t *size) {
 unsigned clusters=0,read_count=0;
 r->stage=SD_DIRECTORY;
 for(clusters=0;clusters<32;++clusters) {
  uint32_t lba,s;
  if(!cluster_lba(r,directory,&lba))return 0;
  for(s=0;s<sectors_per_cluster;++s) {
   unsigned offset;
   if(++read_count>256)return fail(r,SD_DIRECTORY_LIMIT);
   if(!peaq_sd_read(r,lba+s,sector))return 0;
   for(offset=0;offset<512;offset+=32) {
    const uint8_t *p=sector+offset;unsigned attr=p[11];
    if(!p[0])return fail(r,SD_FILE_MISSING);
    if(p[0]==0xe5||(attr&8)||attr==15)continue;
    if(same(p,name,11)) {
     if(((attr&16)!=0)!=is_directory)return fail(r,SD_FILE_INVALID);
     *found=((uint32_t)u16(p+20)<<16)|u16(p+26);*size=u32(p+28);
     if(!cluster_ok(r,*found))return fail(r,SD_CHAIN_INVALID);
     return 1;
    }
   }
  }
  if(!next(r,directory,&directory))return 0;
  if(directory>=0x0ffffff8u)return fail(r,SD_FILE_MISSING);
 }
 return fail(r,SD_DIRECTORY_LIMIT);
}
static uint8_t expected(uint32_t position) {
 return (uint8_t)((position*73u+(position>>8)*19u+0x35u)^(position>>3));
}
static int file_pass(PeaqSdReport *r,uint32_t *crc_out) {
 uint32_t done=0,c=r->file_cluster,crc=~0u;unsigned clusters;
 r->stage=SD_FILE_DATA;
 for(clusters=0;clusters<64;++clusters) {
  uint32_t lba,s,next_cluster;
  if(!cluster_lba(r,c,&lba))return 0;
  for(s=0;s<sectors_per_cluster&&done<PEAQ_SD_FILE_BYTES;++s) {
   unsigned i;
   if(!peaq_sd_read(r,lba+s,sector))return 0;
   for(i=0;i<512;++i)if(sector[i]!=expected(done+i)) {
    r->file_mismatch=done+i+1;return fail(r,SD_DATA_MISMATCH);
   }
   crc=peaq_sd_crc(crc,sector,512);done+=512;
  }
  if(!next(r,c,&next_cluster))return 0;
  if(done==PEAQ_SD_FILE_BYTES) {
   if(next_cluster<0x0ffffff8u)return fail(r,SD_CHAIN_INVALID);
   *crc_out=~crc;return 1;
  }
  if(next_cluster>=0x0ffffff8u)return fail(r,SD_CHAIN_INVALID);
  c=next_cluster;
 }
 return fail(r,SD_CHAIN_INVALID);
}
int peaq_sd_file(PeaqSdReport *r) {
 uint32_t directory,size;
 if(r->error)return 0;
 if(!(r->flags&SD_F_SECTOR0))return fail(r,SD_NOT_READY);
 if(r->file_count)return fail(r,SD_ALREADY_RUN);
 ++r->file_count;
 if(!volume(r)||!find(r,r->root_cluster,(const uint8_t*)"GAMES      ",1,&directory,&size)||
    !find(r,directory,(const uint8_t*)"COLDCHK BIN",0,&r->file_cluster,&r->file_bytes))return 0;
 if(r->file_bytes!=PEAQ_SD_FILE_BYTES)return fail(r,SD_FILE_INVALID);
 if(!file_pass(r,&r->file_crc0)||!file_pass(r,&r->file_crc1))return 0;
 if(r->file_crc0!=r->file_crc1)return fail(r,SD_DATA_MISMATCH);
 r->flags|=SD_F_FILE;r->stage=SD_COMPLETE;return 1;
}
