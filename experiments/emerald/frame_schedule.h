#ifndef PEAQ_EMERALD_FRAME_SCHEDULE_H
#define PEAQ_EMERALD_FRAME_SCHEDULE_H
#include <stdint.h>
enum {PEAQ_FRAME_ERROR=-1,PEAQ_FRAME_WAIT=0,PEAQ_FRAME_DRAW=1,PEAQ_FRAME_HIDDEN=2};
typedef struct {
 uint32_t initialized,due_ms,remainder,last_ms;
 uint32_t ticks,drawn,hidden,skipped_streak;
 uint32_t maximum_streak,resyncs,largest_lateness_ms,error;
} PeaqFrameSchedule;
void peaq_frame_schedule_init(PeaqFrameSchedule *,uint32_t now);
/* WAIT means poll input and call again; it never consumes a simulation step.
 * DRAW/HIDDEN each consume exactly one 60-Hz deadline. No clock/hardware I/O. */
int peaq_frame_schedule_next(PeaqFrameSchedule *,uint32_t now);
#endif
