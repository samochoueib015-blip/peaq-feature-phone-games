#ifndef PEAQ_PGA5_SEGMENT_PROFILES_H
#define PEAQ_PGA5_SEGMENT_PROFILES_H
#include "abi5_segments.h"
/* Offline manifest fixtures for the two existing ports, NEVER a hardcoded
 * resident launcher registry. A game's SD package declares its own table.
 * IDs are a convenience shared by these port fixtures, not a format enum.
 * Runtime minimum sizes already include internal alignment slack; preserve
 * them until those independently qualified runtimes are intentionally revised.
 */
enum { PGS_PORT_MODULE=1,PGS_PORT_HEAP,PGS_PORT_FRAMES,PGS_PORT_WORKER,
       PGS_PORT_STACK,PGS_PORT_TCB,PGS_PORT_RUNTIME,PGS_PORT_HOST };
#define PEAQ_PGA5S_LICAR_COUNT 7u
#define PEAQ_PGA5S_DOOM_COUNT 7u
extern const PeaqPga5sSpec peaq_pga5s_licar_profile[PEAQ_PGA5S_LICAR_COUNT];
extern const PeaqPga5sSpec peaq_pga5s_doom_profile[PEAQ_PGA5S_DOOM_COUNT];
/* Both profiles take native-host sizes directly from their current headers.
 * Size declarations do not substitute for independent native-host testing. */
#endif
