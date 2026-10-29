#ifndef PEAQ_DOOM_FILEIO_H
#define PEAQ_DOOM_FILEIO_H
#include "../runtime/mocor_files.h"

enum { PEAQ_DOOM_FILE_SLOTS=8 };
/* These operations match the existing four-word Newlib DoomIo callback. */
enum { DOOM_IO_OPEN,DOOM_IO_CLOSE,DOOM_IO_READ,DOOM_IO_WRITE,DOOM_IO_SEEK,
       DOOM_IO_FSTAT,DOOM_IO_STAT,DOOM_IO_UNLINK,DOOM_IO_RENAME,DOOM_IO_MKDIR,
       DOOM_IO_EXIT };
typedef struct {uint32_t fd,handle,poisoned;} PeaqDoomFile;
typedef struct {
 const PeaqFileApi *api;
 char folder[32],drive;
 uint32_t initialized,next_fd,exit_seen;
 int32_t exit_status;
 PeaqDoomFile files[PEAQ_DOOM_FILE_SLOTS];
} PeaqDoomFiles;

/* Zero-initialize once; the API and context must outlive the engine. Only
 * E:\Games\<folder> or D:\Games\<folder> is addressable. Engine names are
 * 1..31 ASCII letters/digits/underscore/hyphen/dot, never starting with dot.
 * No slash, drive, leading ./, subfolder or absolute fallback is accepted.
 * The host calls this and every operation on the serialized engine worker.
 * Return 0 on success, negative Newlib errno on failure. */
int peaq_doom_files_init(PeaqDoomFiles *,const PeaqFileApi *,char drive,const char *folder);
/* Supply the context through a host-owned four-argument DoomIo trampoline.
 * Existing file data is read only; mutations return -EROFS. stdout/stderr
 * are discarded successfully, stdin is EOF. No file can alias those handles.
 * IO_EXIT records status only: it MUST NOT close handles or free the context.
 * Native SFS errors become -EIO: this ABI does not expose an Open error code,
 * so it cannot distinguish missing files from media/permission errors.
 * Short reads are preserved. A failure after complete chunks returns those
 * bytes, then -EIO on the next read until a successful seek repositions it. */
int32_t peaq_doom_files_io(PeaqDoomFiles *,uint32_t op,uint32_t a,uint32_t b,uint32_t c);
/* Call only after a managed engine boundary returned EXITED/FAILED and no
 * module callback remains active. Failed closes retain ownership for retry;
 * the host must not discard this context when this returns an error. */
int peaq_doom_files_close_all(PeaqDoomFiles *);
#endif
