/* Early-OS boot service. No engine/codec/OS callbacks, no NAND writes.
 * Only a validated newest Games request allows PLAY.BIN to run. Both request
 * file chains are audited before the one fixed-sector consume write.
 * Return0 always means continue the original kernel startup.
 */
#include "boot_ticket.h"
#include "fat_save_map.h"
#include "sd_save_board.h"
#include "peaq_save_probe_profile.h"
#include "boot_watchdog.h"
#include <stddef.h>
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GUARD 0x85b2a36du
extern uint8_t __bss_start[],__bss_end[];
extern void *memset(void *,int,size_t);
extern int peaq_sd_boot_close(PeaqSdReport *);
static PeaqSdReport card;
static struct {uint32_t before[16];PeaqFat fat;uint32_t after[16];}
 files __attribute__((section(".file_state"),aligned(64)));
static struct {uint32_t before[16];PeaqFatSaveMap map;uint32_t after[16];}
 audit __attribute__((section(".save_state"),aligned(64)));
static uint8_t ticket[1536] __attribute__((aligned(64)));
static PeaqSdSavePermit permit;
static PeaqBootWatchdog watchdog;
static uint32_t quick_handles[2];
static int checked_guards(void){
 for(unsigned i=0;i<16;++i)if(files.before[i]!=GUARD||files.after[i]!=GUARD||audit.before[i]!=GUARD||audit.after[i]!=GUARD)return 0;
 return 1;
}
static int read_sector(void *cookie,uint32_t lba,uint8_t *out){
 return cookie==&card&&checked_guards()&&
  (out==files.fat.fat||out==files.fat.data||out==audit.map.mirror)&&peaq_sd_read(&card,lba,out);
}
static int ticket_read(void *cookie,unsigned slot,uint8_t *out){
 if(cookie!=&card||slot>1||!checked_guards()||!audit.map.valid)return 0;
 return peaq_sd_read(&card,audit.map.sectors[slot][0],out);
}
static int quick_read(void *cookie,unsigned slot,uint8_t *out){
 uint32_t got=0;
 return cookie==&card&&slot<2&&checked_guards()&&
  peaq_fat_seek(&files.fat,quick_handles[slot],0)==1&&
  peaq_fat_read(&files.fat,quick_handles[slot],out,512,&got)==1&&got==512;
}
static int ticket_write(void *cookie,unsigned slot,const uint8_t *p){
 return cookie==&card&&slot<2&&checked_guards()&&audit.map.valid&&
  peaq_sd_save_write(&card,audit.map.sectors[slot][0],p);
}
static int ticket_sync(void *cookie){return cookie==&card&&peaq_sd_save_sync(&card);}
static uint32_t read_word(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
/* Output is a private80300000 buffer; trampoline copies to80100000 only after
 * this function returns a positive file size. The request is already consumed
 * and the card stopped then. No game code runs on the bootstrap's stack. */
unsigned peaq_menu_boot(uint32_t sctlr,uint32_t cpsr){
 memset(__bss_start,0,(size_t)(__bss_end-__bss_start));memset(&files,0,sizeof(files));memset(&audit,0,sizeof(audit));
 for(unsigned i=0;i<16;++i)files.before[i]=files.after[i]=audit.before[i]=audit.after[i]=GUARD;
 card.magic=PEAQ_SD_MAGIC;card.version=1;card.bytes=sizeof(card);card.flags=SD_F_COLD|SD_F_BOARD;
 card.entered_sctlr=sctlr;card.entered_cpsr=cpsr;card.chip_id=REG(0x402e00fc);
 if(sctlr&0x1005u||card.chip_id!=0x98180001u)return 0;
 unsigned result=0;
 if(!peaq_boot_watchdog_suspend(&watchdog))goto done;
 if(!peaq_sd_init(&card))goto done;
 const uint32_t cid[4]=PEAQ_SAVE_EXPECTED_CID;
 for(unsigned i=0;i<4;++i)if(cid[i]!=card.cid[i])goto done;
 PeaqFatHost host={&card,read_sector,card.capacity_lo,32768};
 if(peaq_fat_mount(&files.fat,&host)!=1)goto done;
 /* Normal boots read only the two tiny records. Full ownership auditing is
  * necessary only for a requested game start that may consume one sector. */
 for(unsigned slot=0;slot<2;++slot){uint32_t size;
  if(peaq_fat_open(&files.fat,slot+1,quick_handles+slot,&size)!=1||size!=512)goto done;}
 PeaqBootTicketIo quick={&card,quick_read,0,0};uint32_t wanted=0,volume=0;
 if(peaq_boot_peek(&quick,&wanted,&volume,ticket)!=1)goto done;
 for(unsigned slot=0;slot<2;++slot)if(peaq_fat_close(&files.fat,quick_handles[slot])!=1)goto done;
 if(peaq_fat_save_map(&files.fat,&audit.map)!=1)goto done;
 PeaqBootTicketIo io={&card,ticket_read,ticket_write,ticket_sync};
 if(peaq_boot_peek(&io,&wanted,&volume,ticket)!=1)goto done;
 uint32_t handle=0,size=0,got=0;
 if(peaq_fat_open(&files.fat,0,&handle,&size)!=1||size<1024||size>0xf000u||(size&3u))goto done;
 uint8_t *payload=(uint8_t *)0x80300000u;uint32_t at=0,hash=2166136261u;
 while(at<size){
  unsigned n=size-at;if(n>4096)n=4096;
  if(peaq_fat_read(&files.fat,handle,payload+at,n,&got)!=1||got!=n)goto done;
  for(unsigned i=0;i<n;++i)hash=(hash^payload[at+i])*16777619u;
  at+=n;
 }
 if(hash!=wanted||read_word(payload)!=0x42544844u||read_word(payload+4)!=1||read_word(payload+48)!=size-512)goto done;
 /* Header words other than DHTB/version/length are reserved zero. */
 for(unsigned i=8;i<512;++i)if((i<48||i>=52)&&payload[i])goto done;
 for(unsigned i=0;i<4;++i)permit.cid[i]=card.cid[i];
 permit.data_first=files.fat.data_lba;permit.data_sectors=files.fat.cluster_count*files.fat.spc;permit.write_limit=1;
 permit.sectors[0]=audit.map.sectors[0][0];permit.sectors[1]=audit.map.sectors[1][0];
 if(!peaq_sd_save_arm(&card,&permit)||peaq_boot_consume(&io,wanted,&volume,ticket)!=1||!checked_guards())goto done;
 result=size;
done:
 if(!peaq_sd_boot_close(&card))result=0;
 if(!result)(void)peaq_boot_watchdog_restore(&watchdog);
 return result;
}
