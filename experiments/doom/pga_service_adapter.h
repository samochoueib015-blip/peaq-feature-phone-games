#ifndef PEAQ_DOOM_PGA_SERVICE_ADAPTER_H
#define PEAQ_DOOM_PGA_SERVICE_ADAPTER_H
#include "../launcher/native_launch_design.h"
#include "../launcher/abi5_segments.h"
#include "mocor_host.h"

enum { PDP_ZERO,PDP_VALIDATING,PDP_READY,PDP_CLOSING,PDP_FAILED };
enum { PDP_OK,PDP_API,PDP_LOCATION,PDP_SEGMENTS,PDP_HOST,PDP_WORKER,PDP_START,PDP_DISPLAY,PDP_LEASE };
typedef struct {
 uint32_t magic,phase,error,generation,held,paused,paints,display_error;
 PeaqDoomMocorHost *host;PeaqDoomWorker *worker;
 char drive,folder[32];
 uint32_t paint_armed,paint_at;
} PeaqDoomPgaState;

/* Experimental ABI5 descriptor v2 exports. Only this package is new; frozen
 * worker/native host and production SDK/loader are not changed. The caller
 * owns the resident dispatch lease and all declared segments before Start.
 * Zero-state Close/Poll are quiescent. No export frees any allocation.
 * Poll uses the qualified due-gated mocor_service, whose sole release result
 * is the OUTER native mocor_pump gate ==1. No catch-up loop or SD-owned timer.
 * The separate candidate requests5ms service; ordinary redraw stays50ms. */
int peaq_doom_pga_start(const PgaApi *,void *,const PeaqPga5sMemory *,uint32_t,uint32_t);
void peaq_doom_pga_close(const PgaApi *,void *);
int peaq_doom_pga_poll(const PgaApi *,void *,uint32_t milliseconds);
#endif
