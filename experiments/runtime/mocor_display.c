#include "mocor_display.h"

/* Observed stock paths: 0 is ARGB8888, the default 3 is a 16-bit surface.
 * Reject other format values until their layout has been established. */
const PeaqDisplayOps peaq_pmp122_display = {
    (int (*)(const PeaqDisplayDevice *, PeaqDisplaySurface *))0x8870a11fu,
    (uint32_t (*)(const PeaqDisplayDevice *))0x881c9fbdu,
    (void (*)(const PeaqDisplayDevice *, PeaqDisplayRect))0x882da8e5u
};

int peaq_display_present(const PeaqDisplayOps *ops, const uint16_t *source,
                         uint32_t width, uint32_t height)
{
    PeaqDisplayDevice device = {0, 0};
    PeaqDisplaySurface s = {0};
    PeaqDisplayRect dirty = {0, 0, 239, 319};
    uint32_t x, y, bytes, begin, end, input;
    if (!ops || !ops->query || !ops->format || !ops->invalidate || !source ||
        width != 240 || height != 320)
        return PEAQ_DISPLAY_ARGUMENT;
    if (ops->query(&device, &s) != 1 || !s.pixels)
        return PEAQ_DISPLAY_UNAVAILABLE;
    if (s.width != width || s.height != height || s.stride < width || s.stride > 1024)
        return PEAQ_DISPLAY_GEOMETRY;
    if ((s.format != 0 && s.format != 3) || ops->format(&device) != s.format)
        return PEAQ_DISPLAY_FORMAT;
    bytes = s.format == 0 ? 4 : 2;
    begin = (uint32_t)(uintptr_t)s.pixels;
    end = begin + (s.stride * (height - 1) + width) * bytes;
    input = (uint32_t)(uintptr_t)source;
    if ((begin & (bytes - 1)) || (input & 1) || end < begin ||
        input + width * height * 2 < input ||
        (input < end && input + width * height * 2 > begin))
        return PEAQ_DISPLAY_ADDRESS;
    if (s.format == 3) {
        uint16_t *pixels = (uint16_t *)s.pixels;
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x)
                pixels[y * s.stride + x] = source[y * width + x];
    } else {
        uint32_t *pixels = (uint32_t *)s.pixels;
        for (y = 0; y < height; ++y)
            for (x = 0; x < width; ++x) {
                uint32_t c = source[y * width + x];
                /* Match stock 0x8870a100, including zero low channel bits. */
                pixels[y * s.stride + x] = 0xff000000u |
                    ((c & 0xf800u) << 8) | ((c & 0x07e0u) << 5) | ((c & 31u) << 3);
            }
    }
    ops->invalidate(&device, dirty);
    return PEAQ_DISPLAY_OK;
}
