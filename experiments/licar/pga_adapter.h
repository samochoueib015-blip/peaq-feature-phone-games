#ifndef PEAQ_LICAR_PGA_ADAPTER_H
#define PEAQ_LICAR_PGA_ADAPTER_H
#include "../launcher/abi5_segments.h"
#include "../launcher/native_launch_design.h"

/* Experimental SD package for the separately qualified ABI5-v2 candidate.
 * Start receives only public API/location + declared resources; no private
 * launcher Context casts, allocation, window creation or firmware mutation.
 * All exports and the copied location remain pinned until Poll returns1.
 */
enum { PLPA_ZERO,PLPA_BOUND,PLPA_HOST,PLPA_BRIDGE,PLPA_START_ATTEMPT,PLPA_DONE };
enum { PLPA_OK,PLPA_ARGUMENT,PLPA_HOST_ERROR,PLPA_BRIDGE_ERROR,PLPA_START_ERROR,
       PLPA_DISPLAY_ERROR,PLPA_FRAME_ERROR,PLPA_PRESTART_ERROR };
typedef struct {uint32_t stage,error,generation,closed,paused,frames;int32_t display_result;} PeaqLicarPgaStatus;
int peaq_licar_pga_start(const PgaApi *,void *,const PeaqPga5sMemory *,uint32_t,uint32_t);
uint32_t pga_event(const PgaApi *,void *,uint32_t,uint32_t);
void peaq_licar_pga_close(const PgaApi *,void *);
int peaq_licar_pga_poll(const PgaApi *,void *,uint32_t);
void peaq_licar_pga_status(const void *,PeaqLicarPgaStatus *);
#endif
