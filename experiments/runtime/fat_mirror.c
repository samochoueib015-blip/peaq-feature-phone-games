/* T45-V1.0 FAT32 cache-write correction. Called only from 0x88049fec.
 * Original cache code marks a sector clean after writing FAT0 only. Keep
 * that bit dirty until both copies have been written and read back.
 * No heap, mutable globals, alternate volume handles, or raw-card driver.
 */
typedef unsigned int u32;
typedef unsigned char u8;
extern u32 stock_sector_read(void *,u32,u32,void *);
extern u32 stock_sector_write(void *,u32,u32,const void *);
extern u32 stock_file_flush(u32,void *);
extern u32 stock_file_close(u32);
static u32 word(const u8 *p){return (u32)p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
static u32 half(const u8 *p){return (u32)p[0]|(u32)p[1]<<8;}
static int same(const u8 *a,const u8 *b){for(u32 i=0;i<512;i++)if(a[i]!=b[i])return 0;return 1;}

u32 peaq_fat_mirror_write(void *device,u32 sector,u32 count,const void *data){
 const u8 *v=(const u8 *)device-0x50;
 /* Preserve original FAT16 and single-FAT behavior. The FAT12 bulk path
  * has a separate original callsite and is not intercepted. */
 if(word(v+0x1c)!=2||word(v+0x2c)==1)
  return stock_sector_write(device,sector,count,data);
 u32 scratch[128];u8 *b=(u8 *)scratch;
 u32 part=word(v+0x14),span=word(v+0x18),reserved=word(v+0x24),fat=word(v+0x28);
 if(word(v)!=0x46415453||word(v+0x134)!=0x46415445||word(v+0x20)!=512||
    word(v+0x2c)!=2||count!=1||!data||!span||part>0xffffffffu-span||
    !reserved||reserved>=span||!fat||fat>(span-reserved)/2||
    word(v+0x44)!=reserved+2*fat||sector<part+reserved||sector-(part+reserved)>=fat)return 0;
 /* Read the mounted volume's own BPB through the same serialized device
  * API. In particular, do not guess mirroring from a stale drive letter. */
 if(!stock_sector_read(device,part,1,b)||half(b+510)!=0xaa55||half(b+11)!=512||
    b[16]!=2||half(b+14)!=reserved||word(b+36)!=fat||half(b+17)||half(b+22)||
    !word(b+32)||word(b+32)>span||reserved+2*fat>=word(b+32)||half(b+42))return 0;
 u32 flags=half(b+40);
 /* Disabled mirroring with FAT0 selected keeps its original semantics.
  * An unsupported alternate active FAT must never be overwritten. */
 if(flags&0x80){
  if(flags!=0x80)return 0;
  return stock_sector_write(device,sector,count,data);
 }
 if(flags&0xff70)return 0;
 if(!stock_sector_write(device,sector,1,data)||
    !stock_sector_write(device,sector+fat,1,data))return 0;
 if(!stock_sector_read(device,sector,1,b)||!same(b,data)||
    !stock_sector_read(device,sector+fat,1,b)||!same(b,data))return 0;
 return 1;
}

/* The original close discards its internal flush result. Launcher calls
 * must observe an explicit synchronous flush before closing/readback. */
u32 peaq_checked_file_close(u32 handle){
 u32 flush=stock_file_flush(handle,(void *)0);
 u32 close=stock_file_close(handle);
 return flush?flush:close;
}
