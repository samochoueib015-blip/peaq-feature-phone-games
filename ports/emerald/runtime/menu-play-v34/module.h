#ifndef PEAQ_ENGINE_MODULE_H
#define PEAQ_ENGINE_MODULE_H
#include <stdint.h>
typedef struct {
    void *cookie;
    int (*read)(void *,void *,uint32_t,uint32_t *);
    void *(*allocate)(uint32_t);
    void (*release)(void *);
    void (*sync_code)(void *,uint32_t);
} PeaqModuleHost;
typedef struct {
    void *allocation;
    uint8_t *base;
    uint32_t image_bytes,memory_bytes;
    uint32_t *exports,export_count;
} PeaqModule;
/* No file opening or execution: the caller owns the stream and module lifetime.
 * Error releases any allocation and clears the result. Success transfers that
 * allocation to the caller, which must release it only after engine shutdown. */
int peaq_module_load(const PeaqModuleHost *,PeaqModule *);
#endif
