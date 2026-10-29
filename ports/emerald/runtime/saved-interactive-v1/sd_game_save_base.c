/* New writer variant sharing the unchanged cold initialization/read driver.
 * CMD24 mode matches the pinned UMS9117 FPDoom sdio.c. Unlike that upstream
 * helper, success requires transfer complete, DAT release, and CMD13 reporting
 * READY_FOR_DATA in TRAN. Every poll is bounded. No generic command interface.
 */
#include "sd_board_engine.c"
#include "sd_save_board.h"
static PeaqSdSavePermit permit;
static PeaqSdSaveStatus save_status;
static PeaqSdReport *save_owner;

static int cold_cpu(void){
 uint32_t v;__asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(v));return !(v&5u);
}
static int save_fail(unsigned error){
 if(!save_status.error)save_status.error=error;
 fail(error);poison();return 0;
}
static int separate(const void *a,uint32_t n,const void *b,uint32_t m){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return x+n>=x&&y+m>=y&&(x+n<=y||y+m<=x);
}
static int available(PeaqSdReport *r){
 return r&&r==save_owner&&save_status.armed&&!save_status.error&&
  !r->error&&!r->poisoned&&(r->flags&SD_F_CARD)&&cold_cpu();
}
int peaq_sd_save_arm(PeaqSdReport *r,const PeaqSdSavePermit *p){
 if(!r||!p||((uintptr_t)p&3u)||((uintptr_t)r&3u)||save_status.armed||save_owner||
    r!=state||r->error||r->poisoned||r->chip_id!=0x98180001u||
    !(r->flags&SD_F_CARD)||!(r->flags&SD_F_COLD)||!cold_cpu()||
    !separate(r,sizeof(*r),p,sizeof(*p))||!p->data_first||
    p->data_first>=r->capacity_lo||!p->data_sectors||
    p->data_sectors>r->capacity_lo-p->data_first||!p->write_limit||p->write_limit>514u)return 0;
 for(unsigned i=0;i<4;++i)if(p->cid[i]!=r->cid[i])return 0;
 uint32_t w[4];
 for(unsigned i=0;i<4;++i)w[i]=r->csd[i]<<8|(i?r->csd[i-1]>>24:0);
 /* BLOCK_WRITE command class and CSD permanent/temporary write protection. */
 if(!(bits(w,84,12)&16u)||bits(w,12,2))return 0;
 for(unsigned i=0;i<PEAQ_SD_SAVE_MAP_SIZE;++i){
  uint32_t lba=p->sectors[i];
  if(lba<p->data_first||lba-p->data_first>=p->data_sectors||
     (!r->block_addressed&&lba>0x7fffffu))return 0;
  for(unsigned j=0;j<i;++j)if(lba==p->sectors[j])return 0;
 }
 for(unsigned i=0;i<sizeof(permit);++i)((uint8_t *)&permit)[i]=((const uint8_t *)p)[i];
 save_owner=r;save_status.armed=1;return 1;
}
/* Exactly CMD13. No data buffer and no caller-selected RCA. */
static int card_status(uint32_t *result){
 if(!wait_reg(0x20300024u,1,0,100,SD_INHIBIT_TIMEOUT))return 0;
 SD(0x38)=0;SD(0x34)=0;SD(0x30)=0xffffffffu;SD(0x34)=ERRORS|1u;
 SD(8)=state->rca;
 __asm__ volatile("dsb sy":::"memory");
 SD(0x0c)=(13u<<24)|(2u<<16)|CHECKS|(1u<<18);
 ++state->commands;++save_status.status_queries;state->last_cmd=13;state->last_arg=state->rca;
 uint32_t start=now(),previous=start;
 for(unsigned i=0;i<POLLS;++i){
  uint32_t v=SD(0x30);state->last_status=v;
  if(v&ERRORS)return fail(SD_COMMAND_ERROR);
  if(v&1u){
   *result=SD(0x10);state->last_response=*result;
   if(*result&R1_ERRORS)return fail(SD_CARD_STATUS_ERROR);
   return 1;
  }
  if(elapsed(start,&previous,100))break;
 }
 return fail(SD_COMMAND_TIMEOUT);
}
int peaq_sd_save_sync(PeaqSdReport *r){
 if(!available(r))return 0;
 state=r;
 if(!wait_reg(0x20300024u,2,0,1000,SD_INHIBIT_TIMEOUT))return save_fail(r->error);
 uint32_t start=now(),previous=start;
 for(unsigned i=0;i<256;++i){
  uint32_t value;
  if(!card_status(&value))return save_fail(r->error);
  uint32_t card=(value>>9)&15u;
  if(card==4&&(value&0x100u)){++save_status.ready_checks;return 1;}
  if(card!=4&&card!=6&&card!=7)return save_fail(SD_CARD_STATUS_ERROR);
  if(elapsed(start,&previous,1000))break;
 }
 return save_fail(r->error?r->error:SD_COMMAND_TIMEOUT);
}
int peaq_sd_save_write(PeaqSdReport *r,uint32_t lba,const uint8_t data[512]){
 if(!available(r)||!data||!separate(data,512,&dma,sizeof(dma))||
    !separate(data,512,&permit,sizeof(permit))||!separate(data,512,r,sizeof(*r))||
    save_status.writes>=permit.write_limit)return 0;
 unsigned index;
 for(index=0;index<PEAQ_SD_SAVE_MAP_SIZE&&permit.sectors[index]!=lba;++index){}
 if(index==PEAQ_SD_SAVE_MAP_SIZE)return 0;
 state=r;
 /* Assert card readiness before submitting another write, too. */
 if(!peaq_sd_save_sync(r))return 0;
 if(!wait_reg(0x20300024u,3,0,100,SD_INHIBIT_TIMEOUT))return save_fail(r->error);
 for(unsigned i=0;i<16;++i)dma.before[i]=dma.after[i]=DMA_GUARD;
 for(unsigned i=0;i<512;++i)dma.bytes[i]=data[i];
 SD(0x38)=0;SD(0x34)=0;SD(0x30)=0xffffffffu;SD(0x34)=ERRORS|3u;
 SD(4)=512;SD(0)=1;SD(0x58)=(uintptr_t)dma.bytes;SD(0x5c)=0;
 uint32_t argument=r->block_addressed?lba:lba<<9;
 SD(8)=argument;__asm__ volatile("dsb sy":::"memory");
 ++save_status.writes;++r->commands;r->last_cmd=24;r->last_arg=argument;
 SD(0x0c)=(24u<<24)|(2u<<16)|CHECKS|(1u<<21)|1u;
 uint32_t start=now(),previous=start;unsigned complete=0;
 for(unsigned i=0;i<POLLS;++i){
  uint32_t v=SD(0x30);r->last_status=v;
  if(v&ERRORS)return save_fail(SD_COMMAND_ERROR);
  if((v&3u)==3u){complete=1;break;}
  if(elapsed(start,&previous,1000))break;
 }
 if(!complete)return save_fail(r->error?r->error:SD_COMMAND_TIMEOUT);
 r->last_response=SD(0x10);
 if(r->last_response&R1_ERRORS)return save_fail(SD_CARD_STATUS_ERROR);
 __asm__ volatile("dsb sy":::"memory");
 for(unsigned i=0;i<16;++i)if(dma.before[i]!=DMA_GUARD||dma.after[i]!=DMA_GUARD)return save_fail(SD_DMA_GUARD);
 return peaq_sd_save_sync(r);
}
const PeaqSdSaveStatus *peaq_sd_save_status(void){return &save_status;}
