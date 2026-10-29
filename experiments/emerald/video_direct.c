/* Paired only with the corrected frame-independent RGB565 PPU.
 * GBA window bit 5 controls colour effects, not backdrop visibility. The
 * backdrop initializes every output pixel/layer, so no previous framebuffer
 * or hidden VRAM snapshot contributes to a later displayed frame.
 * Simulation and PCM still execute for every logical frame. */
#include "video_cadence.h"
#include "ppu.h"
#include <string.h>
int peaq_video_init(PeaqVideoCadence *v,const PeaqVideoSource *s,uint16_t *screen,uint8_t *layers){
 if(!v||!s||!s->reg||!s->pal||!s->vram||!s->oam||!screen||!layers)return 0;
 memset(&v->stats,0,sizeof(v->stats));
 v->stats.magic=PEAQ_VIDEO_MAGIC;v->stats.version=1;v->stats.bytes=sizeof(v->stats);
 v->live=*s;v->screen=screen;v->layers=layers;return 1;
}
int peaq_video_frame(PeaqVideoCadence *v,unsigned show){
 if(!v||v->stats.magic!=PEAQ_VIDEO_MAGIC)return 0;
 if(show>1u||v->stats.error){v->stats.error=1;return 0;}
 if(show){
  ppu_set_memory(v->live.reg,v->live.pal,v->live.vram,v->live.oam);
  ppu_render_rgb565(v->screen,v->layers);++v->stats.ppu_calls;++v->stats.shown;
 }else ++v->stats.hidden;
 ++v->stats.frames;++v->stats.independent;return 1;
}
