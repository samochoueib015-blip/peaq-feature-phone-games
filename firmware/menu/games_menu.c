/* Native main-menu category for the exact PEAQ PMP122 firmware.
 * Appends code to the verified 16 MiB User mapping; preserves the calculator.
 * Cooperative MMK window. No storage access, tasks, heap, or hardware registers.
 */
typedef unsigned int u32;
typedef unsigned short u16;
typedef signed short s16;
typedef struct { s16 left, top, right, bottom; } Rect;
typedef struct { s16 x,y; } Point;
typedef struct { u32 lcd, block; } Device;
typedef struct { const u16 *text; u16 length,reserved; } String;
typedef struct { u32 words[11]; } Style;

#define GAMES_ID 0x5047u
#define GAMES_ICON 0x4f504751u
#define GAMES_WINDOW 0x001300f0u

extern u32 create_window(const u32 *, void *);
extern void close_window(u32);
extern void set_calc_style(Style *, u32,u32,u16);
extern u32 draw_image(const Point *,const Rect *,const Rect *,u32,u32,const Device *);
extern void draw_text(const Device *,const Rect *,const Rect *,const String *,Style *,u32,signed char);
extern void draw_hline(const Device *,s16,s16,s16,u16);
u32 games_window(u32,u32,void *);
u32 games_open(void);

static const u16 title[]={'G','a','m','e','s',0};
static const u16 subtitle[]={'N','o','c','h',' ','k','e','i','n','e',' ','S','p','i','e','l','e',0};
static const u16 back[]={'Z','u','r','u','e','c','k',':',' ','r','e','c','h','t','s',0};
const u32 games_win_table[]={0xf001,(u32)games_window,0xf002,GAMES_WINDOW,0xf01f,0xf030};

/* The original renderer uses the active icon as a 240 x 230 image.
 * Its normal icon is zero for ALL seven stock categories; keep that convention.
 * Fields and the 64-byte structure were verified against the device's code.
 */
u32 games_item(u32 menu,u32 group,u32 *out) {
    if(!out || menu!=GAMES_ID || group) return 0;
    for(u32 i=0;i<16;i++) ((volatile u32 *)out)[i]=0;
    out[0]=group; out[1]=menu;
    out[2]=2; out[3]=(u32)title; out[4]=5;
    out[5]=1; out[6]=0;
    out[8]=1; out[9]=GAMES_ICON;
    out[11]=(u32)games_open;
    return 1;
}

u32 games_open(void) { return create_window(games_win_table,(void *)0); }

u32 games_window(u32 window,u32 message,void *parameter) {
    (void)parameter;
    if(message==0xfa17) return 1;
    if(message==0xfb17) { close_window(window); return 1; }
    if(message==0xf021 || message==0xf022 || message==0xf023 || message==0xf024) return 1;
    if(message!=0xf025) return 0;
    Device dev={0,0};
    Rect background={0,24,239,319};
    Rect row={10,64,229,104};
    Style style;
    String text;
    for(u32 i=0;i<11;i++) ((volatile u32 *)&style)[i]=0;
    set_calc_style(&style,6,28,0xffff);
    draw_image((void *)0,&background,&background,window,0x400001c3,&dev);
    for(u32 line=0;line<3;line++) {
        text.text=line==0?title:(line==1?subtitle:back);
        text.length=line==0?5:(line==1?17:15);text.reserved=0;
        row.top=line==0?64:(line==1?120:270);row.bottom=row.top+40;
        draw_text(&dev,&row,&row,&text,&style,0x10,-1);
    }
    return 1;
}

/* Vector-style icon expressed as RGB565 horizontal spans. It uses the original
 * OS's layer-aware HLine primitive, including its own clipping. Coordinates
 * below are additionally clipped to the caller's destination rectangle.
 */
static void box(const Device *dev,const Rect *clip,int ox,int oy,
                int x0,int y0,int x1,int y1,u16 color) {
    x0+=ox;x1+=ox;y0+=oy;y1+=oy;
    if(x0<clip->left)x0=clip->left;
    if(x1>clip->right)x1=clip->right;
    if(y0<clip->top)y0=clip->top;
    if(y1>clip->bottom)y1=clip->bottom;
    if(x0>x1 || y0>y1)return;
    for(int y=y0;y<=y1;y++)draw_hline(dev,(s16)x0,(s16)y,(s16)x1,color);
}

u32 games_icon(const Point *point,const Rect *dest,const Rect *source,
               u32 window,u32 image,const Device *dev) {
    (void)window;
    if(image!=GAMES_ICON || !dev || (!dest&&!point))return 0;
    Rect clip;
    int ox,oy;
    if(dest) {
        clip=*dest;
        if(clip.right<clip.left || clip.bottom<clip.top)return 0;
        ox=clip.left-(source?source->left:0);
        oy=clip.top-(source?source->top:0);
    } else {
        ox=point->x;oy=point->y;
        clip.left=point->x;clip.top=point->y;
        clip.right=(s16)(ox+239);clip.bottom=(s16)(oy+229);
    }
    /* Refuse extreme rectangles; every loop stays within a 240 x 230 image. */
    if(ox < -240 || ox > 479 || oy < -320 || oy > 639)return 0;
    box(dev,&clip,ox,oy,56,47,183,54,0x49bb);
    box(dev,&clip,ox,oy,48,55,191,62,0x49bb);
    box(dev,&clip,ox,oy,44,63,195,166,0x49bb);
    box(dev,&clip,ox,oy,48,167,191,174,0x49bb);
    box(dev,&clip,ox,oy,56,175,183,182,0x49bb);
    /* Controller shell and handles. */
    box(dev,&clip,ox,oy,76,92,163,131,0xffff);
    box(dev,&clip,ox,oy,68,100,171,139,0xffff);
    box(dev,&clip,ox,oy,64,116,87,147,0xffff);
    box(dev,&clip,ox,oy,152,116,175,147,0xffff);
    box(dev,&clip,ox,oy,72,140,83,151,0xffff);
    box(dev,&clip,ox,oy,156,140,167,151,0xffff);
    /* D-pad, two buttons, and start/select. */
    box(dev,&clip,ox,oy,85,101,92,124,0x18c6);
    box(dev,&clip,ox,oy,77,109,100,116,0x18c6);
    box(dev,&clip,ox,oy,144,103,153,112,0xfd45);
    box(dev,&clip,ox,oy,157,116,166,125,0x3e7a);
    box(dev,&clip,ox,oy,111,120,117,123,0x9cf3);
    box(dev,&clip,ox,oy,123,120,129,123,0x9cf3);
    return 1;
}
