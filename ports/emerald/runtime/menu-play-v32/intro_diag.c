/* Separate bounded cold intro runner. All USB actions are fixed and empty.
 * The unchanged PGE is permitted to execute only after its final EOF callback
 * has checked the pinned CRC/header/export RVAs and owned arena guards.
 * No keyboard, audio, card writer or OS transition is installed.
 */
#include "intro_diag.h"
#include "runtime.h"
#include "fat_ro.h"
#include "cold_display.h"
#include "engine_speed_probe.h"
#include "engine_load_manifest.h"
#include "peaq_sd_board_profile.h"
#include "common.h"
#include "packet.h"
#define GUARD 0x27ba4ec1u
extern uint8_t __bss_start[],__bss_end[];
static PeaqIntroReport intro;
static PeaqSdReport sd;
static PeaqEngineSpeedReport speed;
static PeaqEmeraldRuntime runtime;
static PeaqFatSaves saves;
static PeaqEmeraldServices services;
static PeaqSaveHost save_host;
static uint32_t handle,stream_crc,started_ms,logical_ms;
static struct {uint32_t before[16];PeaqFat fs;uint32_t after[16];}
 storage __attribute__((section(".file_state"),aligned(64)));
static struct {uint32_t before[16];uint8_t bytes[PEAQ_INTRO_ARENA_BYTES];uint32_t after[16];}
 runtime_arena __attribute__((section(".runtime_arena"),aligned(64)));
_Static_assert(sizeof(PeaqIntroReport)==240,"ELI1 word layout");
_Static_assert(sizeof(PeaqFat)==50352,"FAT context changed");
uint32_t sys_timer_ms(void){return MEM4(0x4023000cu);}
static int fail(unsigned e){if(!intro.error)intro.error=e;intro.flags|=INTRO_F_FAILED;intro.stage=INTRO_FAILED;return 0;}
static uint32_t crc(uint32_t h,const void *data,uint32_t n){
 const uint8_t *p=data;
 while(n&&((uintptr_t)p&3u)){h=peaq_engine_crc_table[(h^*p++)&255u]^(h>>8);--n;}
 while(n>=4u){
  uint32_t v=*(const uint32_t *)p;p+=4;n-=4;
  h=peaq_engine_crc_table[(h^v)&255u]^(h>>8);
  h=peaq_engine_crc_table[(h^(v>>8))&255u]^(h>>8);
  h=peaq_engine_crc_table[(h^(v>>16))&255u]^(h>>8);
  h=peaq_engine_crc_table[(h^(v>>24))&255u]^(h>>8);
 }
 while(n--)h=peaq_engine_crc_table[(h^*p++)&255u]^(h>>8);
 return h;
}
static uint32_t word(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int guards(void){
 intro.guards_ok=0;
 for(unsigned i=0;i<16;++i)if(storage.before[i]!=GUARD||storage.after[i]!=GUARD||runtime_arena.before[i]!=GUARD||runtime_arena.after[i]!=GUARD)return fail(INTRO_GUARD);
 if(!saved_guards())return fail(INTRO_GUARD);
 intro.guards_ok=1;return 1;
}
static void snapshot(void){
 const PeaqFrameReport *d=peaq_cold_display_status();
 intro.timer_ms=sys_timer_ms();intro.file_crc=~stream_crc;intro.sd_error=sd.error;intro.fs_error=(uint32_t)storage.fs.error;
 intro.sd_reads=sd.reads;intro.fs_reads=storage.fs.reads;intro.clock_control=sd.clock_control;intro.sd_poisoned=sd.poisoned;
 intro.display_error=d->error;intro.display_poisoned=d->poisoned;
 intro.runtime_error=(uint32_t)runtime.last_error;intro.arena_used=runtime.used;intro.module_base=(uintptr_t)runtime.module.base;
 intro.image_bytes=runtime.module.image_bytes;intro.memory_bytes=runtime.module.memory_bytes;intro.exports=runtime.module.export_count;
 intro.gba=(uintptr_t)runtime.gba;intro.portrait=(uintptr_t)runtime.portrait;intro.layers=(uintptr_t)runtime.layers;intro.save_memory=(uintptr_t)runtime.save_memory;
 intro.save_dirty=runtime.dirty;intro.save_generation=runtime.generation;intro.save_read_flags=saves.known|(saves.absent<<2);
 guards();
}
static int read_sector(void *cookie,uint32_t lba,uint8_t *out){
 if(cookie!=&sd||intro.error||intro.callback_fault||!guards())return 0;
 if(out!=storage.fs.fat&&out!=storage.fs.data&&out!=saved_storage.map.mirror&&out!=report_state.map.mirror)return fail(INTRO_FAT);
 return peaq_sd_read(&sd,lba,out);
}
static int module_read(void *cookie,void *out,uint32_t n,uint32_t *got){
 if(cookie!=&storage.fs||!got||intro.error)return 0;
 ++intro.module_reads;
 if(peaq_fat_read(&storage.fs,handle,out,n,got)!=1)return 0;
 if(!intro.stream_bytes){
  if(n!=32||*got!=32)return fail(INTRO_HEADER);
  for(unsigned i=0;i<32;++i)if(((uint8_t*)out)[i]!=peaq_engine_header[i])return fail(INTRO_HEADER);
  intro.header_matched=1;
 }
 if(*got>PEAQ_ENGINE_FILE_BYTES-intro.stream_bytes)return fail(INTRO_SIZE);
 stream_crc=crc(stream_crc,out,*got);intro.stream_bytes+=*got;
 if(!peaq_menu_loading(MENU_LOAD_ENGINE,intro.stream_bytes*100u/PEAQ_ENGINE_FILE_BYTES))return fail(INTRO_PRESENT);
 if(!*got){
  /* This check precedes PeaqModuleHost.sync_code, SaveMemory and Init.
  * Never defer the pinned-file integrity gate until runtime_start returns. */
  if(n!=1||intro.stream_bytes!=PEAQ_ENGINE_FILE_BYTES||~stream_crc!=PEAQ_ENGINE_FILE_CRC||!guards())return fail(INTRO_CRC);
  if(runtime.module.base!=runtime_arena.bytes)return fail(INTRO_RUNTIME);
  uint32_t offset=word(peaq_engine_header+16);
  for(unsigned i=0;i<5;++i)if(word(runtime.module.base+offset+4*i)!=(uintptr_t)runtime_arena.bytes+peaq_engine_export_rva[i])return fail(INTRO_RUNTIME);
 }
 return 1;
}
static void sync_code(void *base,uint32_t bytes){
 if(base!=runtime_arena.bytes||bytes!=PEAQ_ENGINE_IMAGE_BYTES){intro.callback_fault=INTRO_RUNTIME;return;}
 /* Cache/MMU remain disabled by the inherited cold contract. Synchronize
  * completed SD copies and the newly written instruction stream only. */
 __asm__ volatile("dsb sy\nisb sy" ::: "memory");
}
static void present(void *cookie,const uint16_t *p,uint32_t w,uint32_t h){
 ++intro.present_attempts;
 if(intro.callback_fault)return;
 int result;
 if(cookie!=&storage.fs||p!=runtime.portrait||w!=240u||h!=320u)result=PEAQ_DISPLAY_ARGUMENT;
 else result=peaq_cold_display_present(p,w,h);
 intro.display_result=(uint32_t)result;
 if(result!=1){intro.callback_fault=INTRO_PRESENT;return;}
 ++intro.present_count;
}
static int outer_result(void){
 /* Never unload or unwind the engine from inside its callback. All three
  * non-success display results become terminal here before another frame. */
 if(intro.callback_fault){peaq_emerald_runtime_pause(&runtime,1,logical_ms);return fail(intro.callback_fault);}
 if(runtime.last_error<0||intro.write_attempts){peaq_emerald_runtime_pause(&runtime,1,logical_ms);return fail(INTRO_SAVE);}
 return guards();
}
static int initialize(void){
 if(intro.error)return 0;
 if(intro.init_count)return fail(INTRO_REPEAT);
 ++intro.init_count;
 int r=peaq_cold_display_init(intro.entered_sctlr,intro.entered_cpsr);intro.display_result=(uint32_t)r;
 if(r!=1)return fail(INTRO_DISPLAY_INIT);
 intro.flags|=INTRO_F_DISPLAY;
 if(!peaq_menu_loading(MENU_LOAD_CARD,0))return fail(INTRO_PRESENT);
 /* LCD owns the original complete digital pad profile. Initialize it FIRST,
  * then the SD controller reapplies its12 explicit qualified pads. */
 if(!peaq_sd_init(&sd))return fail(INTRO_SD);
 if(sd.capacity_hi||sd.capacity_lo!=PEAQ_ENGINE_CARD_SECTORS)return fail(INTRO_CAPACITY);
 intro.flags|=INTRO_F_CARD;intro.stage=INTRO_HW_READY;return guards();
}
static int check_speed(void){
 if(intro.error)return 0;
 if(!(intro.flags&INTRO_F_CARD)||intro.start_count)return fail(INTRO_ORDER);
 if(!peaq_engine_speed_probe(&sd,&speed))return fail(INTRO_SD);
 intro.speed_passed=1;intro.flags|=INTRO_F_SPEED;intro.stage=INTRO_SPEED_READY;return guards();
}
static int start(void){
 if(intro.error)return 0;
 if(!(intro.flags&INTRO_F_SPEED))return fail(INTRO_ORDER);
 if(intro.start_count)return fail(INTRO_REPEAT);
 ++intro.start_count;
 PeaqFat *f=&storage.fs;PeaqFatHost host={&sd,read_sector,PEAQ_ENGINE_CARD_SECTORS,PEAQ_ENGINE_READ_BUDGET};
 if(peaq_fat_mount(f,&host)!=1)return fail(INTRO_FAT);
 if(f->partition!=8192u||f->total!=62325760u||f->spc!=64u||f->fat_lba!=9362u||f->fat_sectors!=7607u||f->data_lba!=24576u||
    f->cluster_count!=973584u||f->root!=2u||~crc(~0u,f->data,512)!=3259051948u)return fail(INTRO_GEOMETRY);
 if(peaq_fat_open(f,PEAQ_FAT_ENGINE,&handle,&intro.file_bytes)!=1)return fail(INTRO_FILE);
 if(intro.file_bytes!=PEAQ_ENGINE_FILE_BYTES)return fail(INTRO_SIZE);
 if(!peaq_menu_loading(MENU_LOAD_SAVE,0)||!saved_prepare())return fail(INTRO_SAVE);
 services=(PeaqEmeraldServices){f,module_read,sync_code,present,menu_audio,&save_host};
 uint32_t t=sys_timer_ms();
 int r=peaq_emerald_runtime_start(&runtime,&services,runtime_arena.bytes,sizeof(runtime_arena.bytes),0);
 intro.load_ms=sys_timer_ms()-t;intro.runtime_result=(uint32_t)r;
 if(intro.callback_fault)return outer_result();
 if(r!=1)return fail(r==PEAQ_EMERALD_SAVE_IO||r==PEAQ_EMERALD_SAVE_INVALID?INTRO_SAVE:INTRO_RUNTIME);
 if(!outer_result())return 0;
 if(runtime.used!=PEAQ_INTRO_ARENA_BYTES)return fail(INTRO_RUNTIME);
 if(!saved_arm())return fail(INTRO_SAVE);
 started_ms=sys_timer_ms();logical_ms=0;intro.flags|=INTRO_F_STARTED;intro.stage=INTRO_STARTED;return 1;
}
static int batch(void){
 if(intro.error)return 0;
 if(!(intro.flags&INTRO_F_STARTED))return fail(INTRO_ORDER);
 if(intro.frames>=PEAQ_INTRO_FRAMES)return fail(INTRO_REPEAT);
 ++intro.batch_count;intro.stage=INTRO_RUNNING;uint32_t t=sys_timer_ms();
 for(unsigned i=0;i<PEAQ_INTRO_BATCH_FRAMES&&intro.frames<PEAQ_INTRO_FRAMES;++i){
  if(!outer_result())break;
  uint32_t n=intro.frames,buttons=(n==180u||n==181u)?8u:0u;
  logical_ms+=17u;uint32_t begin=sys_timer_ms();
  int r=peaq_emerald_runtime_step(&runtime,logical_ms,buttons);
  intro.step_ms+=sys_timer_ms()-begin;intro.runtime_result=(uint32_t)r;
  if(r!=1){fail(INTRO_RUNTIME);break;}
  ++intro.frames;
  if(!outer_result())break;
 }
 intro.last_batch_ms=sys_timer_ms()-t;intro.wall_ms=sys_timer_ms()-started_ms;
 if(!intro.error&&intro.frames==PEAQ_INTRO_FRAMES){
  peaq_emerald_runtime_pause(&runtime,1,logical_ms);
  intro.flags|=INTRO_F_COMPLETE;intro.stage=INTRO_COMPLETE;
 }
 return !intro.error;
}
static int reply(unsigned kind,const void *data,unsigned bytes){uint8_t *p=dl_send_buf();WRITE16_BE(p,kind);WRITE16_BE(p+2,bytes);memcpy(p+4,data,bytes);dl_send_packet(p);return -1;}
int peaq_intro_dispatch(unsigned type,unsigned length){
 if(length)return BSL_REP_INVALID_CMD;if(type==0)return BSL_REP_ACK;
 if(type==PEAQ_INTRO_INIT)initialize();else if(type==PEAQ_INTRO_SPEED)check_speed();else if(type==PEAQ_INTRO_START)start();else if(type==PEAQ_INTRO_BATCH)batch();
 else if(type!=PEAQ_INTRO_INFO&&type!=PEAQ_INTRO_DISPLAY_INFO&&type!=PEAQ_INTRO_SPEED_INFO&&type!=PEAQ_INTRO_SD_INFO)return BSL_REP_INVALID_CMD;
 snapshot();
 if(type==PEAQ_INTRO_SPEED||type==PEAQ_INTRO_SPEED_INFO)return reply(0x94,&speed,sizeof(speed));
 if(type==PEAQ_INTRO_DISPLAY_INFO)return reply(0x93,peaq_cold_display_status(),sizeof(PeaqFrameReport));
 if(type==PEAQ_INTRO_SD_INFO)return reply(0x96,&sd,sizeof(sd));
 return reply(0x95,&intro,sizeof(intro));
}
void peaq_cold_main(uint32_t sctlr,uint32_t cpsr){
 memset(__bss_start,0,(size_t)(__bss_end-__bss_start));memset(&storage.fs,0,sizeof(storage.fs));stream_crc=~0u;
 for(unsigned i=0;i<16;++i)storage.before[i]=storage.after[i]=runtime_arena.before[i]=runtime_arena.after[i]=GUARD;
 intro.magic=PEAQ_INTRO_MAGIC;intro.version=1;intro.bytes=sizeof(intro);intro.flags=(!(sctlr&0x1005u)?INTRO_F_COLD:0)|INTRO_F_BOARD;
 intro.chip_id=MEM4(0x402e00fcu);intro.entered_sctlr=sctlr;intro.entered_cpsr=cpsr;
 intro.frames_target=PEAQ_INTRO_FRAMES;intro.batch_target=PEAQ_INTRO_BATCH_FRAMES;intro.arena_start=(uintptr_t)runtime_arena.bytes;
 intro.arena_bytes=sizeof(runtime_arena.bytes);intro.required_divider=4;intro.source_profile=PEAQ_SD_PROFILE_CRC32;
 sd.magic=PEAQ_SD_MAGIC;sd.version=1;sd.bytes=sizeof(sd);sd.flags=(!(sctlr&5u)?SD_F_COLD:0)|SD_F_BOARD;
 sd.chip_id=intro.chip_id;sd.entered_sctlr=sctlr;sd.entered_cpsr=cpsr;sd.profile_crc32=PEAQ_SD_PROFILE_CRC32;
 speed.magic=0x31475053u;speed.version=1;speed.bytes=sizeof(speed);speed.read_budget=PEAQ_ENGINE_READ_BUDGET;
 speed.required_crc=0x6337fde8u;speed.profile_crc32=PEAQ_SD_PROFILE_CRC32;guards();
 dl_packet_init();dl_send_ack(BSL_REP_ACK);
 for(;;){uint8_t *p=dl_get_packet();int r=peaq_intro_dispatch(READ16_BE(p),READ16_BE(p+2));if(r>=0)dl_send_ack(r);}
}
