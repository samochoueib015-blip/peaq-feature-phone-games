#ifndef PEAQ_PGA_H
#define PEAQ_PGA_H
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed short s16;
enum { PGA_INIT=1, PGA_PAINT, PGA_KEY, PGA_TICK, PGA_CLOSE, PGA_PAUSE, PGA_RESUME, PGA_RAW_KEY };
enum { PGA_REDRAW=1, PGA_QUIT=2 };
enum { PGA_UP=16, PGA_DOWN, PGA_LEFT, PGA_RIGHT, PGA_OK, PGA_STAR, PGA_HASH, PGA_BACK };
enum { PGA_CAP_TONE=1, PGA_CAP_BACK=2, PGA_CAP_RAW_KEYS=4 };
typedef struct {
    u32 version, width, height;
    void (*fill)(int x,int y,int width,int height,u16 color);
    void (*text)(int x,int y,u16 color,const char *text);
    void *storage;
    int (*load)(void *storage,void *data,u32 bytes);
    int (*save)(void *storage,const void *data,u32 bytes);
    /* ABI 3 extension: never access these fields on an ABI 2 host. */
    u32 capabilities;
    int (*tone)(void *storage,u32 frequency,u32 duration_ms,u32 volume);
    void (*silence)(void *storage);
} PgaApi;
typedef u32 (*PgaEntry)(const PgaApi *,void *state,u32 event,u32 value);
/* Each callback must return promptly. Draw only during PGA_PAINT.
 * Mutable game state belongs in state, whose size is exported below.
 * ABI 2 reserves the right softkey for returning to the games list.
 * ABI 3 sends PGA_BACK; a game returns PGA_QUIT to leave its own menus.
 * ABI 2. load/save return 1 on success, otherwise 0; maximum 512 bytes.
 * Use only your own API.storage cookie. Saves stay in the game's folder.
 * Version your saved structure and validate all loaded fields before use.
 * PAUSE means focus loss: preserve progress; RESUME must not unpause play.
 * ABI 4 packages receive PGA_RAW_KEY instead of PGA_KEY for game buttons:
 * value = kind|key: 0xfa00 press, 0xfb00 short release, 0xfc00 long press,
 * 0xfd00 long release, 0xfe00 repeat. Both release kinds clear held state;
 * long press and repeat must not count as another button being pressed.
 * Keys: directions 1..4, digits/star/hash 8..19, OK 0x16/0x1b/0x26,
 * Back 0x17. Return PGA_QUIT on Back release at the game's top level.
 * Clear all held buttons on PGA_PAUSE. Power/system and volume keys remain
 * owned by the OS/launcher. The API struct layout is unchanged from ABI 3.
 */
extern const u32 pga_state_bytes;
extern const u32 pga_tick_ms;
/* Export 3 for audio/Back or 4 for complete game-key press/release events. */
extern const u32 pga_required_abi;
u32 pga_event(const PgaApi *,void *,u32,u32);
#endif
