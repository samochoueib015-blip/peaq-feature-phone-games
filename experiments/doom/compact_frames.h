/* Preincluded by the separate compact build. The public worker ABI and its
 * fixed control storage stay unchanged. One private render buffer plus one
 * published buffer replaces the previous private-plus-two-published layout.
 *
 * The generated worker retains the original atomic FREE/WRITING/READY/READING
 * protocol. A leased published frame cannot be overwritten; while it is held,
 * presentation drops the newer frame instead of waiting or allocating.
 */
#ifndef __ASSEMBLER__
#include "worker.h"
#undef PEAQ_DOOM_FRAME_BYTES
#define PEAQ_DOOM_FRAME_BYTES (2u*240u*320u*2u+31u)
#endif
