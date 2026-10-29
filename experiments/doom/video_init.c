/* The pinned DoomGeneric I_InitGraphics allocates its indexed framebuffer
 * without clearing it. The first melt then displays recycled heap data.
 * Link-wrap only this initialization point; leave all rendering and the
 * upstream source/object unchanged. SCREENWIDTH/HEIGHT are vanilla 320x200. */
#include <string.h>
extern unsigned char *I_VideoBuffer;
extern void __real_I_InitGraphics(void);
void __wrap_I_InitGraphics(void){
 __real_I_InitGraphics();
 if(I_VideoBuffer)memset(I_VideoBuffer,0,320u*200u);
}
