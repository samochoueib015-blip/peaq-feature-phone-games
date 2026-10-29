/* Separate playable-save candidate. Frozen adaptive code is included through
 * a generated copy whose hooks are checked by the builder. No old image edit.
 * SD writes occur only after the cache/exception boundary has restored cold
 * CPU state. This trial permits two complete saves per cold session.
 */
#include "cold_save.h"
#include "fat_save_map.h"
#include "sd_save_board.h"
#include "game_controls.h"
#include "peaq_save_probe_profile.h"
typedef struct {
 uint32_t magic,version,bytes,error,prepared,armed,flushes,commits;
 uint32_t writes,reads,ready_checks,store_poisoned,dirty,generation,quiet_frames;
 uint32_t save_ms,volume,volume_changed,map_valid,map_crc,guards,last_payload;
 uint32_t read_budget,write_budget,clock_control,pending,stop_flushed,reserved;
} PeaqSavedReport;
static struct {
 uint32_t before[16];PeaqFatSaveMap map;PeaqColdSave store;
 PeaqSdSavePermit permit;PeaqSaveHost host;uint32_t after[16];
} saved_storage __attribute__((section(".save_state"),aligned(64)));
static PeaqSavedReport saved;
static PeaqEmeraldVolume volume;
static unsigned saved_waiting;
static int saved_prepare(void),saved_arm(void),saved_flush(unsigned force),saved_reply(void),saved_guards(void);
static void saved_init(void),saved_volume(uint32_t);
extern int peaq_sd_save_slow(PeaqSdReport *);
#include "adaptive_saved.c"

_Static_assert(sizeof(saved_storage)<=0x3000,"save context overlaps game arena");
_Static_assert(sizeof(saved)==112,"ESV1 report size");
static uint32_t save_hash(const void *p,uint32_t n){
 const uint8_t *b=p;uint32_t h=2166136261u;while(n--)h=(h^*b++)*16777619u;return h;
}
static int saved_guards(void){
 for(unsigned i=0;i<16;++i)if(saved_storage.before[i]!=GUARD||saved_storage.after[i]!=GUARD)return 0;
 return 1;
}
static int saved_fail(unsigned error){if(!saved.error)saved.error=error;return 0;}
static int saved_cold(void){
 return !(peaq_cache_read_sctlr()&0x1005u)&&!in_engine&&!peaq_dc.active&&!peaq_dc.leaving;
}
static int saved_read_sector(void *cookie,uint32_t lba,void *out){
 return cookie==&saved_storage&&saved_cold()&&saved_guards()&&out==saved_storage.store.read_buffer&&peaq_sd_read(&sd,lba,out);
}
static int saved_write_sector(void *cookie,uint32_t lba,const void *data){
 return cookie==&saved_storage&&saved.armed&&saved_cold()&&saved_guards()&&
  data==saved_storage.store.write_buffer&&(MEM4(0x2030002cu)&0xffc7u)==0x4007u&&peaq_sd_save_write(&sd,lba,data);
}
static int saved_sync(void *cookie){
 return cookie==&saved_storage&&saved.armed&&saved_cold()&&saved_guards()&&peaq_sd_save_sync(&sd);
}
static void saved_init(void){
 memset(&saved_storage,0,sizeof(saved_storage));
 for(unsigned i=0;i<16;++i)saved_storage.before[i]=saved_storage.after[i]=GUARD;
 saved.magic=0x31565345;saved.version=1;saved.bytes=sizeof(saved);
 saved.read_budget=65536;saved.write_budget=16448;
 if(!peaq_emerald_volume_init(&volume,6,sys_timer_ms()))saved_fail(1);
}
static int saved_prepare(void){
 if(saved.prepared||saved.error||!saved_cold()||!saved_guards())return saved_fail(2);
 static const uint32_t cid[4]=PEAQ_SAVE_EXPECTED_CID;
 for(unsigned i=0;i<4;++i)if(sd.cid[i]!=cid[i])return saved_fail(3);
 if(peaq_fat_save_map(&storage.fs,&saved_storage.map)!=1)return saved_fail(4);
 PeaqColdSaveHost host={&saved_storage,saved_read_sector,saved_write_sector,saved_sync,
  sd.capacity_lo,storage.fs.data_lba,storage.fs.cluster_count*storage.fs.spc};
 if(!peaq_cold_save_init(&saved_storage.store,&host,saved_storage.map.sectors)||
    !peaq_cold_save_bind(&saved_storage.store,&saved_storage.host))return saved_fail(5);
 memcpy(saved_storage.permit.cid,sd.cid,16);
 saved_storage.permit.data_first=host.data_first;saved_storage.permit.data_sectors=host.data_sectors;
 saved_storage.permit.write_limit=16448;
 memcpy(saved_storage.permit.sectors,saved_storage.map.sectors,sizeof(saved_storage.map.sectors));
 saved.map_crc=save_hash(saved_storage.map.sectors,sizeof(saved_storage.map.sectors));
 saved.prepared=1;save_host=saved_storage.host;saves.known=3;saves.absent=0;return 1;
}
static int saved_arm(void){
 if(!saved.prepared||saved.armed||saved.error||!saved_cold())return saved_fail(6);
 /* Engine and existing save are loaded first at the qualified read setting.
  * Restore the physically qualified conservative divider before any write. */
 if(!peaq_sd_save_slow(&sd)||!peaq_sd_save_arm(&sd,&saved_storage.permit))return saved_fail(7);
 saved.armed=1;return 1;
}
static void saved_volume(uint32_t now){
 if(saved.error||!peaq_interactive_keys_status()->initialized||peaq_interactive_keys_status()->stopped)return;
 PeaqKeyInput *input=(PeaqKeyInput *)(uintptr_t)peaq_interactive_keys_input();
 unsigned side=(input->held[1]|input->pressed[1])&0x300u;
 if(peaq_emerald_volume_input(&volume,input,now)<0)saved_fail(8);
 else if(side&&interactive.active&&!menu_pause_pending)peaq_menu_volume_notice(volume.level,now);
}
static int saved_flush(unsigned force){
 if(saved.error)return 0;
 if(!runtime.active||!runtime.dirty)return 1;
 if(!force&&runtime.quiet_frames<30)return 1;
 if(!saved.armed||!saved_cold()||!saved_guards()||interactive.error||cache.error||intro.error||intro.callback_fault)
  return saved_fail(9);
 if(saved.commits>=64)return saved_fail(10);
 usb_gate();if(!menu_audio_end(1))return saved_fail(12);
 uint32_t began=sys_timer_ms();++saved.flushes;
 int result=peaq_emerald_runtime_flush(&runtime);
 saved.save_ms+=sys_timer_ms()-began;
 /* The host could not send STEP while SD was busy. That time is not an
  * idle-host timeout. The adaptive scheduler rebases at its next deadline. */
 idle_since=last_clock=last_service_ms=sys_timer_ms();
 if(result!=1||runtime.dirty||!saved_guards()||saved_storage.store.poisoned)return saved_fail(11);
 ++saved.commits;if(force)saved.stop_flushed=1;
 saved.last_payload=save_hash(runtime.save_memory,131072);
 if(!force&&interactive.active&&!menu_audio_begin())return saved_fail(13);return 1;
}
static int saved_reply(void){
 const PeaqSdSaveStatus *s=peaq_sd_save_status();
 saved.writes=s->writes;saved.reads=sd.reads;saved.ready_checks=s->ready_checks;
 saved.store_poisoned=saved_storage.store.poisoned;saved.dirty=runtime.dirty;
 saved.generation=runtime.generation;saved.quiet_frames=runtime.quiet_frames;
 saved.volume=volume.level;saved.volume_changed=volume.changed;saved.map_valid=saved_storage.map.valid;
 saved.guards=saved_guards();saved.clock_control=sd.clock_control;saved.pending=runtime.dirty&&runtime.quiet_frames>=30;
 return checked_reply(0xa7,&saved,sizeof(saved));
}
