/* The current pass already proved plain 4bpp output with no per-pixel mask.
 * Keep that decision outside the tile loop; all other passes use the existing
 * renderer. Flip a packed tile row once, instead of every individual pixel.
 * Inputs are an immutable per-frame snapshot; output pixels/layers are owned
 * separately, as in the checked PEAQ renderer contract. */
static __attribute__((noinline)) void textBgPlain4Line(const bgcfg_t *c,int y) {
    const int sy=(y+c->vofs)&c->hmask;
    const uint32_t by=sy>=256?(uint32_t)(c->sizebits==3?2:1)*0x800u:0;
    const uint32_t maprow=c->screenBase+by+(uint32_t)((sy&255)>>3)*64u;
    const uint32_t chars=c->charBase;
    const int hofs=c->hofs,wmask=c->wmask;
    const uint8_t lb=(uint8_t)g_passLayer;
    uint16_t *restrict dst=line565;
    uint8_t *restrict layers=g_layer+g_rowBase;
    const uint16_t *restrict palette=pal565;
    for(int x=0;x<WIDTH;) {
        const int sx=(x+hofs)&wmask,q=sx&7;
        int n=8-q;if(n>WIDTH-x)n=WIDTH-x;
        const uint16_t entry=ld16(VRAMb,maprow+(sx>=256?0x800u:0)+(uint32_t)((sx&255)>>3)*2u);
        const unsigned py=(entry&0x800u)?7u-(sy&7u):(sy&7u);
        uint32_t bits=ld32(VRAMb,chars+(uint32_t)(entry&0x3ffu)*32u+py*4u);
        if(bits) {
            if(entry&0x400u) {
                bits=__builtin_bswap32(bits);
                bits=((bits&0x0f0f0f0fu)<<4)|((bits>>4)&0x0f0f0f0fu);
            }
            bits>>=q*4;
            const unsigned bank=(entry>>12)*16u;
            if(n==8) {
                /* Most spans are whole tiles. Fixed offsets avoid a loop
                 * counter and variable pixel shifts, retaining transparency. */
#define PEAQ_BG_PIXEL(k) do { \
    unsigned ci=(bits>>((k)*4))&15u; \
    if(ci){dst[x+(k)]=palette[bank+ci];layers[x+(k)]=lb;} \
} while(0)
                PEAQ_BG_PIXEL(0);PEAQ_BG_PIXEL(1);PEAQ_BG_PIXEL(2);PEAQ_BG_PIXEL(3);
                PEAQ_BG_PIXEL(4);PEAQ_BG_PIXEL(5);PEAQ_BG_PIXEL(6);PEAQ_BG_PIXEL(7);
#undef PEAQ_BG_PIXEL
            } else {
                for(int k=0;k<n;++k,bits>>=4) {
                    const unsigned ci=bits&15u;
                    if(ci){dst[x+k]=palette[bank+ci];layers[x+k]=lb;}
                }
            }
        }
        x+=n;
    }
}
