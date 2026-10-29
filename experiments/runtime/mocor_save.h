#ifndef PEAQ_MOCOR_SAVE_H
#define PEAQ_MOCOR_SAVE_H
#include "large_save.h"
#include "mocor_files.h"

typedef uint32_t (*PeaqFileAttr)(const uint16_t *,uint16_t *);
extern const PeaqFileAttr peaq_pmp122_file_attr;
typedef struct {
 const PeaqFileApi *files;
 PeaqFileAttr attributes;
 PeaqSaveHost host;
 uint16_t parent[96],paths[2][96];
 uint32_t handle,failed,poisoned;
} PeaqMocorSave;

/* Zero-initialize once. Paths are fixed to engine-save0.dat/engine-save1.dat
 * within one Games folder. Existing small-game save0/1.dat are never used.
 * One serialized worker owns all calls; the folder/media must not change
 * concurrently (in particular, do not run during USB mass-storage sharing).
 * This is an experimental binding, not part of the installed PGA API.
 *
 * GetAttr status 7 is missing-path on the tested firmware. The parent must
 * independently exist and be a directory before a missing slot is accepted.
 * All other errors, and a failed open of an existing file, are I/O failures.
 * If close fails, the uncertain handle is retained and the binding poisoned:
 * the host must retain state and resolve cleanup, not reuse/truncate a slot.
 */
int peaq_mocor_save_init(PeaqMocorSave *,const PeaqFileApi *,PeaqFileAttr,
                         char drive,const char *folder);
#endif
