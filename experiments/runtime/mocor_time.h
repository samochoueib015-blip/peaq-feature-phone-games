#ifndef PEAQ_MOCOR_TIME_H
#define PEAQ_MOCOR_TIME_H
#include <stdint.h>

/* T45-V1.0 only, qualified against the exact original kernel/User image and
 * saved OS translation table. Native milliseconds, modulo 2^32; callers use
 * unsigned subtraction. No synthetic increments or assumed scheduler ticks. */
#define PEAQ_SCI_GET_TICK_COUNT_THUMB 0x810ae38bu
#define PEAQ_MMK_TIMER_CREATE_THUMB 0x881f0289u
#define PEAQ_MMK_TIMER_START_THUMB 0x881f0025u
#define PEAQ_MMK_TIMER_STOP_THUMB 0x881f02c7u
typedef uint8_t (*PeaqMmkTimerCreate)(uint32_t window,uint32_t milliseconds,
 uint32_t periodic,const char *source,uint32_t line);
typedef uint32_t (*PeaqMmkTimerStart)(uint32_t window,uint32_t timer_id,
 uint32_t milliseconds,uint32_t periodic,const char *source,uint32_t line);
typedef uint32_t (*PeaqMmkTimerStop)(uint8_t timer_id);
uint32_t peaq_mocor_milliseconds(void);

/* Timer creation starts it. Start replaces the timer behind the same u8 ID.
 * Interval 0 is rejected. Stop returns 1 for a currently registered ID and 0
 * otherwise; callers must own it, forget it after Stop and never retry a stale
 * ID, which the OS can reuse. Queue stamps protect expiry dispatch, not Stop.
 * These are serialized UI APIs, never raw-worker services. Timer callback
 * cadence and UI scheduling latency still require physical measurement.
 * The separate audit proves native conversion, queued generation validation
 * and synchronous window dispatch; the u8 UI parameter is valid only during
 * the callback and must be copied, never retained. */
#endif
