#ifndef PEAQ_NATIVE_LAUNCH_DESIGN_H
#define PEAQ_NATIVE_LAUNCH_DESIGN_H
#include "../../sdk/pga.h"
#include <stddef.h>

/* EXPERIMENTAL integration boundary, not installed, not part of the audited
 * 3701-byte resident page. The existing ABI5-v2 Start signature gives the
 * package memory segments and its owner generation, but does NOT currently
 * expose the selected SD drive/folder through its opaque PgaApi.storage.
 * A port must not cast that cookie to the private firmware Context or guess
 * D/E. The following append-only API tail is the proposed next step.
 *
 * Prefix remains byte-identical to sdk/pga.h. Only ABI5 gets this tail;
 * ABI2/3/4 allocations and code paths stay unchanged. The location and full
 * API live inside the pinned Context, and bind must validate both ranges,
 * length/version/reserved fields and folder terminator BEFORE Start. Native
 * operations occur only in UI callbacks. No new firmware flag is enabled by
 * declaring these types. Serialized folder is the selected manifest folder,
 * never a worker-supplied path and never a PC drive letter.
 */
typedef struct {
 u32 bytes,version;
 u8 drive,reserved[3]; /* 'D' or 'E' on this handset; reserved all zero */
 u16 folder[32];      /* selected SD directory, bounded NUL-terminated copy */
} PeaqProposedGameLocation;
typedef struct {
 PgaApi legacy;
 u32 bytes,extension_version;
 const PeaqProposedGameLocation *location;
} PeaqProposedPga5Api;
_Static_assert(sizeof(PgaApi)==44,"Preserve existing ARM API prefix");
_Static_assert(sizeof(PeaqProposedGameLocation)==76,"Bounded copied location");
_Static_assert(offsetof(PeaqProposedPga5Api,bytes)==44,"Append-only API tail");
_Static_assert(sizeof(PeaqProposedPga5Api)==56,"Proposed ARM extension size");

/* Concrete ownership/callback sequence is in native_launch_plan.json.
 * candidate_glue.inc now populates this extension in a separate offline
 * launcher candidate; check_launcher_candidate.py exercises copied location
 * and full Context lifetime. No production SDK or actual engine PGA uses it.
 * Its final page manifest and native timing/IO gates remain mandatory.
 */
#endif
