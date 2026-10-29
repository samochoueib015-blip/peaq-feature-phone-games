#ifndef PEAQ_MOCOR_DISPLAY_H
#define PEAQ_MOCOR_DISPLAY_H
#include <stdint.h>

typedef struct { uint32_t lcd, block; } PeaqDisplayDevice;
typedef struct {
    uint32_t stride, width, height;
    void *pixels;
    uint32_t format;
} PeaqDisplaySurface;
typedef struct { int16_t left, top, right, bottom; } PeaqDisplayRect;
typedef struct {
    int (*query)(const PeaqDisplayDevice *, PeaqDisplaySurface *);
    uint32_t (*format)(const PeaqDisplayDevice *);
    void (*invalidate)(const PeaqDisplayDevice *, PeaqDisplayRect);
} PeaqDisplayOps;

enum {
    PEAQ_DISPLAY_OK = 1,
    PEAQ_DISPLAY_ARGUMENT = -1,
    PEAQ_DISPLAY_UNAVAILABLE = -2,
    PEAQ_DISPLAY_GEOMETRY = -3,
    PEAQ_DISPLAY_FORMAT = -4,
    PEAQ_DISPLAY_ADDRESS = -5
};

/* Call only from the owning MMI window's paint handler, while it has focus.
 * The source is already rotated to physical portrait 240 x 320. Query the
 * current surface on every paint; never retain the OS's framebuffer pointer.
 * These bindings are specific to the audited PMP122 User image, not a generic
 * Mocor API. Hardware validation is still required before game deployment. */
extern const PeaqDisplayOps peaq_pmp122_display;
int peaq_display_present(const PeaqDisplayOps *, const uint16_t *, uint32_t width,
                         uint32_t height);
#endif
