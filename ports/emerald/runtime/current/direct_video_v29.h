#ifndef PEAQ_DIRECT_VIDEO_V29_H
#define PEAQ_DIRECT_VIDEO_V29_H
/* Private, manifest-pinned agreement. The host sets this only after engine
 * initialization and binding both owned buffers. Raw GBA pixels remain the
 * authoritative last shown frame. A legacy host leaves this word zero. */
#define PEAQ_HOST_VIDEO_DIRECT_MAGIC 0x31445256u /* VRD1 */
#endif
