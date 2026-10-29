#include "audio_clock.h"
int peaq_audio_clock_advance(PeaqAudioClock *s,uint32_t now,void (*tick)(void)){
    if(!s||!tick||s->error)return 0;
    if(!s->active){s->active=1;s->last_ms=s->due_ms=now;s->remainder=0;}
    uint32_t gap=now-s->last_ms;
    if(gap&0x80000000u){s->error=1;return 0;}
    s->last_ms=now;if(gap>s->max_gap_ms)s->max_gap_ms=gap;
    if(gap>250u){s->due_ms=now;s->remainder=0;++s->resyncs;}
    uint32_t burst=0;
    while((int32_t)(now-s->due_ms)>=0){
        if(burst==16u){s->error=2;return 0;}
        tick();++burst;++s->ticks;
        s->remainder+=1000u;s->due_ms+=s->remainder/60u;s->remainder%=60u;
    }
    s->last_burst=burst;if(burst>s->max_burst)s->max_burst=burst;
    return 1;
}
