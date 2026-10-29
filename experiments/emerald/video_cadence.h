#ifndef PEAQ_EMERALD_VIDEO_CADENCE_H
#define PEAQ_EMERALD_VIDEO_CADENCE_H
#include <stdint.h>

/* Private engine/host extension, only for a separately pinned module. The
 * lower ten bits retain their GBA key meaning. Simulation/mixing never skip. */
#define PEAQ_EMERALD_STEP_HIDDEN 0x80000000u
#define PEAQ_VIDEO_MAGIC 0x31535645u /* EVS1 */
typedef struct {
 uint32_t magic,version,bytes,frames;
 uint32_t shown,hidden,ppu_calls,snapshots;
 uint32_t replays,dependent,independent,pending;
 uint32_t error,reserved[3];
} PeaqVideoStats;
typedef struct {const uint8_t *reg,*pal,*vram,*oam;} PeaqVideoSource;
typedef struct {
 PeaqVideoStats stats;
 PeaqVideoSource live;
 uint16_t *screen;
 uint8_t *layers;
 /* Preserve only the latest skipped independent frame. A later frame that
  * reads old pixels can reconstruct it before rendering its own snapshot. */
 uint8_t reg[0x400],pal[0x400],vram[0x18000],oam[0x400];
} PeaqVideoCadence;
int peaq_video_init(PeaqVideoCadence *,const PeaqVideoSource *,uint16_t *,uint8_t *);
int peaq_video_independent(const uint8_t *registers);
int peaq_video_frame(PeaqVideoCadence *,unsigned show);
#endif
