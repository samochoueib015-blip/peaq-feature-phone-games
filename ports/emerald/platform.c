#include "direct_video_v29.h"
/* Boundary services for the complete native engine, with no physical I/O. */
#include "platform.h"
#include "memory_map.h"
#include "ppu.h"
#include "landscape.h"
#include "video_cadence.h"
#include "audio_clock.h"
#include <string.h>
extern void AgbMain(void);
extern void WasmRunFrame(void);
extern int Rp2350MixFrame(int8_t *, int);
extern volatile uint32_t gRp2350FrameCount;
static const EmeraldHost *host;
static uint16_t *screen, *portrait;
static uint8_t *layers;
static uint32_t changes;
static unsigned show_frame;
volatile uint32_t peaq_host_video_direct;
PeaqVideoCadence peaq_video;
static int8_t pcm[256];
PeaqAudioClock peaq_audio_clock;
extern void PeaqAudioMix(void);
static void audio_tick(void);
static uint8_t libc_heap[65536] __attribute__((aligned(8)));
extern int peaq_emerald_io_bind(int32_t (*)(uint32_t,uint32_t,uint32_t,uint32_t),void *,void *);
static int32_t io(uint32_t op,uint32_t a,uint32_t b,uint32_t c) {
    (void)b;
    /* Game data uses linked assets and the cartridge-save mirror. This runtime
     * grants no filesystem access; only stdout/stderr have discard sinks. */
    if(op==3 && (a==1 || a==2))return (int32_t)c;
    return -38; /* ENOSYS */
}
#define SAVE ((uint8_t *)PEAQ_SAVE)
#define REG ((volatile uint16_t *)PEAQ_REGISTERS)

int peaq_emerald_init(const EmeraldHost *h,uint16_t *gba,uint16_t *port,uint8_t *scratch) {
    if (!h || !h->present || !h->audio || !h->clock_ms || !gba || !port || !scratch) return 0;
    host=h;screen=gba;portrait=port;layers=scratch;changes=0;show_frame=1;peaq_host_video_direct=0;
    PeaqVideoSource regions={(void *)PEAQ_REGISTERS,(void *)PEAQ_PALETTE,
                             (void *)PEAQ_VRAM,(void *)PEAQ_OAM};
    if(!peaq_video_init(&peaq_video,&regions,screen,layers))return 0;
    if(!peaq_emerald_io_bind(io,libc_heap,libc_heap+sizeof(libc_heap)))return 0;
    REG[0x130/2]=0x3ff;
    ppu_set_memory((void *)PEAQ_REGISTERS,(void *)PEAQ_PALETTE,
                   (void *)PEAQ_VRAM,(void *)PEAQ_OAM);
    /* Only main.c uses its existing WASM return-after-init seam. All other
     * translation units retain RP2350's native audio, BIOS and save hooks. */
    memset(&peaq_audio_clock,0,sizeof(peaq_audio_clock));
    AgbMain();
    return 1;
}
__attribute__((noinline)) void peaq_emerald_audio_service(void){
    peaq_audio_clock_advance(&peaq_audio_clock,host->clock_ms(),audio_tick);
}
void peaq_emerald_step(uint32_t buttons) {
    if (!host) return;
    show_frame=!(buttons&PEAQ_EMERALD_STEP_HIDDEN);
    REG[0x130/2]=(uint16_t)(~buttons & 0x3ff);
    peaq_emerald_audio_service();
    WasmRunFrame();
    peaq_emerald_audio_service();
}
void Rp2350PresentFrame(void) {
    if (!host) return;
    if(!peaq_video_frame(&peaq_video,show_frame))return;
    if(show_frame&&peaq_host_video_direct!=PEAQ_HOST_VIDEO_DIRECT_MAGIC)
        peaq_landscape_fit(portrait,screen,240,160);
    /* Keep one presentation callback per logical frame. Audio has its own clock. */
    host->present(portrait,240,320);

}
static void dirty(void) { ++changes; if (host && host->save_dirty) host->save_dirty(); }
uint16_t Rp2350SaveEraseChip(void) { memset(SAVE,255,131072);dirty();return 0; }
uint16_t Rp2350SaveEraseSector(uint16_t sector) {
    if(sector>=32)return 1;
    memset(SAVE+(uint32_t)sector*4096,255,4096);dirty();return 0;
}
uint16_t Rp2350SaveProgramSector(uint16_t sector,uint8_t *src) {
    if(sector>=32 || !src)return 1;
    for(uint32_t i=0;i<4096;i++)SAVE[(uint32_t)sector*4096+i]&=src[i];
    dirty();return 0;
}
uint16_t Rp2350SaveProgramByte(uint16_t sector,uint32_t offset,uint8_t data) {
    if(sector>=32 || offset>=4096)return 1;
    SAVE[(uint32_t)sector*4096+offset]&=data;dirty();return 0;
}
void Rp2350SaveSync(void) { /* The RAM mirror changes synchronously. */ }
uint32_t peaq_emerald_frames(void) { return gRp2350FrameCount; }
uint32_t peaq_emerald_save_changes(void) { return changes; }
uint8_t *peaq_emerald_save_memory(void) { return SAVE; }

static void audio_tick(void){
    PeaqAudioMix();
    int n=Rp2350MixFrame(pcm,sizeof(pcm));
    if(n>0 && n<=(int)sizeof(pcm) && host->audio) host->audio(pcm,(uint32_t)n,13440);
}
