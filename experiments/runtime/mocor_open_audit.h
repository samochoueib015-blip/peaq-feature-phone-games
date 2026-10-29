#ifndef PEAQ_MOCOR_OPEN_AUDIT_H
#define PEAQ_MOCOR_OPEN_AUDIT_H
#include <stdint.h>

/* Original T45-V1.0 audit only. These constants DO NOT bind a replacement and
 * do not change PeaqFileApi or installed firmware. Normal stock I/O works;
 * the claim being rejected is that opening under arbitrary OOM is fallible.
 */
#define PEAQ_MOCOR_OPEN_OOM_FALLIBLE 0
#define PEAQ_MOCOR_OPEN_THUMB 0x886fbd71u
#define PEAQ_MOCOR_PATH_COPY_THUMB 0x886fb8afu
#define PEAQ_MOCOR_OWNED_OPEN_THUMB 0x882d4c05u
#define PEAQ_MOCOR_REQUEST_ALLOC_THUMB 0x882d5c11u
#define PEAQ_MOCOR_REQUEST_DISPATCH_THUMB 0x882d492fu
#define PEAQ_MOCOR_SFS_ALLOC_ARM 0x88090710u
#define PEAQ_MOCOR_FALLIBLE_ALLOC_ARM 0x88090848u
#define PEAQ_MOCOR_FREE_ARM 0x88090850u
#define PEAQ_MOCOR_PATH_ALLOC_BYTES 0x208u
#define PEAQ_MOCOR_REQUEST_ALLOC_BYTES 0x318u
/* Audited input ABI. Both strings are native heap allocations, not stack or
 * static buffers. Passing them transfers ownership into native processing;
 * the early-rejection branches free them. A wrapper must not free them again
 * after invoking this backend, nor retain stack output for asynchronous use.
 * This declaration is documentation, NOT permission to call an unqualified
 * internal path. The request allocator still asserts on OOM.
 */
typedef uint32_t (*PeaqMocorOwnedOpen)(uint16_t *parent,uint16_t *name,
 uint32_t mode,uint32_t *handle,void *overlapped);
/* Synchronous caller uses overlapped=NULL. Original outer SFS_Open ignores
 * backend status and returns *handle; it is initialized to0 by the backend.
 * No "missing file" conclusion follows from handle0.
 *
 * Minimum observed caller-side transient payload: 2*520+792=1832 bytes, before
 * allocator metadata, block-class rounding, FAT/media work, concurrent OS I/O
 * and fragmentation. This is NOT a sufficient production headroom budget.
 * In the saved configuration all three requests initially use the 800-byte
 * block class (300 configured blocks): at least three such slots can coexist
 * for one open. If exhausted, 1100/1300/1600 classes are tried, then Dynamic
 * App Heap at0x8178fccc. Byte-pool requests sum to1946 bytes before ThreadX's
 * own overhead. Large game arena allocations bypass these fixed-block classes
 * and use that same App Heap directly. This reduces direct competition with
 * small I/O requests, but does NOT guarantee fallback capacity under pressure.
 * These counts describe historical configuration, not live free blocks.
 *
 * Feasible mitigation: bounded arena sizes, all fallible game allocations
 * before worker start, one outstanding game I/O request, persistent handles,
 * and measured post-reservation OS headroom on the intended game profile.
 * Keep the existing OS error checks; do not label stock open/read as guaranteed
 * fallible. Preopening alone is insufficient because other SFS operations also
 * allocate request records. Never reserve/free scratch and claim future OS
 * allocations are guaranteed, or reserve native small blocks away from the OS.
 *
 * A saturated native synchronous queue returns an error but retains its792-
 * byte request and event-wait bit in the observed original path. The caller
 * cannot safely recover that internal ownership through SFS_Open's handle0;
 * do not add an automatic retry loop. Normal completed FAT/media processing
 * is not emulated as success here; its deeper resource needs remain unmeasured.
 *
 * tools/check_mocor_open.py reproduces the three explicit NULL assertions,
 * original allocator behavior/pool routing and native request ownership.
 * No justified total runtime headroom number is established yet. This bounded
 * audit deliberately does not rewrite or globally patch the shared FAT stack.
 */
#endif
