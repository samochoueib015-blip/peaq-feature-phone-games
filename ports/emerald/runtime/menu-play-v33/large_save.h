#ifndef PEAQ_LARGE_SAVE_H
#define PEAQ_LARGE_SAVE_H
#include <stdint.h>

#define PEAQ_SAVE_MAX (256u*1024u)
#define PEAQ_SAVE_SCRATCH 4096u
/* Per-game, two-slot storage. The host confines both slots to one game folder.
 * read: 1 = success (short EOF allowed), 0 = definitely absent, -1 = I/O error.
 * Never turn an ambiguous open/read failure into 'absent'. Each read is at most
 * 4096 bytes. begin truncates ONLY its slot and owns the subsequent write stream.
 * finish flushes and closes that stream, including after a failed write.
 * begin failure must clean up its own resources. Methods return 1 on success.
 * The host must serialize operations; no concurrent writers or changing input.
 */
typedef struct {
 void *cookie;
 int (*read)(void *,uint32_t slot,uint32_t offset,void *,uint32_t,uint32_t *);
 int (*begin)(void *,uint32_t slot);
 int (*write)(void *,const void *,uint32_t,uint32_t *);
 int (*finish)(void *);
} PeaqSaveHost;

/* Load: 1 = validated payload, 0 = no valid record, -1 = I/O/argument/ambiguity.
 * Ignore output on failure; bytes is zero. Scratch is separate from data.
 * Commit returns 1 only after finish AND complete checksum/readback validation.
 * The latest valid slot is never truncated. If both existing slots are corrupt
 * commit refuses to replace them; the caller must preserve/recover them first.
 * This protects records against torn writes, not arbitrary FAT/media damage.
 */
int peaq_large_save_load(const PeaqSaveHost *,void *data,uint32_t capacity,
                         uint32_t *bytes,void *scratch,uint32_t scratch_bytes);
int peaq_large_save_commit(const PeaqSaveHost *,const void *data,uint32_t bytes,
                           void *scratch,uint32_t scratch_bytes);
#endif
