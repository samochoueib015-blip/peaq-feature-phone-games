#ifndef PEAQ_MOCOR_FILES_H
#define PEAQ_MOCOR_FILES_H
#include <stdint.h>
typedef struct {
 uint32_t (*open)(const uint16_t *,uint32_t,uint32_t,void *);
 uint32_t (*read)(uint32_t,void *,uint32_t,uint32_t *,void *);
 uint32_t (*write)(uint32_t,const void *,uint32_t,uint32_t *,void *);
 uint32_t (*close)(uint32_t);
 uint32_t (*seek)(uint32_t,int64_t,uint32_t);
 uint32_t (*position)(uint32_t,uint32_t,int32_t *);
 uint32_t (*size)(uint32_t,uint32_t *);
 uint32_t (*flush)(uint32_t,void *);
} PeaqFileApi;
extern const PeaqFileApi peaq_pmp122_files;
/* Filename-only access beneath one explicit Games folder; no path traversal. */
int peaq_game_file_path(uint16_t out[96],char drive,const char *folder,const char *name);
/* Positioned, bounded reads for WAD/module/assets. Short EOF is successful;
 * errors clear the returned count. This helper never opens or writes a file. */
int peaq_file_read_at(const PeaqFileApi *,uint32_t handle,uint32_t offset,
                     void *data,uint32_t length,uint32_t *read);
#endif
