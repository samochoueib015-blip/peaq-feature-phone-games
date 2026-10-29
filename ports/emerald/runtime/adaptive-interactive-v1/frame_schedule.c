#include "frame_schedule.h"
#include <string.h>
void peaq_frame_schedule_init(PeaqFrameSchedule *s,uint32_t now){
 if(!s)return;memset(s,0,sizeof(*s));s->initialized=1;s->due_ms=s->last_ms=now;
}
int peaq_frame_schedule_next(PeaqFrameSchedule *s,uint32_t now){
 if(!s||!s->initialized||s->error)return PEAQ_FRAME_ERROR;
 if((now-s->last_ms)&0x80000000u){s->error=1;return PEAQ_FRAME_ERROR;}
 s->last_ms=now;
 int32_t late=(int32_t)(now-s->due_ms);
 if(late<0)return PEAQ_FRAME_WAIT;
 if((uint32_t)late>s->largest_lateness_ms)s->largest_lateness_ms=(uint32_t)late;
 /* Bound catch-up after a long host pause/stall; never invent game steps. */
 if(late>250){s->due_ms=now;s->remainder=0;++s->resyncs;late=0;}
 int draw=!s->ticks||late<=1||s->skipped_streak>=3;
 if(draw){++s->drawn;s->skipped_streak=0;}
 else {++s->hidden;++s->skipped_streak;}
 if(s->skipped_streak>s->maximum_streak)s->maximum_streak=s->skipped_streak;
 ++s->ticks;s->remainder+=1000u;
 s->due_ms+=s->remainder/60u;s->remainder%=60u;
 return draw?PEAQ_FRAME_DRAW:PEAQ_FRAME_HIDDEN;
}
