#ifndef PEAQ_EMERALD_MEMORY_MAP_H
#define PEAQ_EMERALD_MEMORY_MAP_H

/* These symbols are regions inside the module's owned allocation. Using
 * addresses of arrays (not pointer variables) also permits original static
 * initializers to refer to the emulated GBA hardware. */
#ifdef PEAQ_EMERALD_RELOCATABLE
extern unsigned char peaq_emerald_ewram[], peaq_emerald_iwram[];
extern unsigned char peaq_emerald_vram[], peaq_emerald_palette[];
extern unsigned char peaq_emerald_oam[], peaq_emerald_registers[];
extern unsigned char peaq_emerald_save[];
#define PEAQ_EWRAM ((unsigned long)peaq_emerald_ewram)
#define PEAQ_IWRAM ((unsigned long)peaq_emerald_iwram)
#define PEAQ_VRAM ((unsigned long)peaq_emerald_vram)
#define PEAQ_PALETTE ((unsigned long)peaq_emerald_palette)
#define PEAQ_OAM ((unsigned long)peaq_emerald_oam)
#define PEAQ_REGISTERS ((unsigned long)peaq_emerald_registers)
#define PEAQ_SAVE ((unsigned long)peaq_emerald_save)
#else
/* The original isolated test layout, never an OS allocation strategy. */
#define PEAQ_EWRAM 0x20000000u
#define PEAQ_IWRAM 0x20040000u
#define PEAQ_VRAM 0x20048000u
#define PEAQ_PALETTE 0x20060000u
#define PEAQ_OAM 0x20060400u
#define PEAQ_REGISTERS 0x20060800u
#define PEAQ_SAVE 0x10fe0000u
#endif
#endif
