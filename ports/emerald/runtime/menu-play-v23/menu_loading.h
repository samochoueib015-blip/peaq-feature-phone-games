#ifndef PEAQ_MENU_LOADING_H
#define PEAQ_MENU_LOADING_H
#include <stdint.h>
enum {MENU_LOAD_CARD=1,MENU_LOAD_CHECK,MENU_LOAD_SAVE,MENU_LOAD_ENGINE,
 MENU_LOAD_KEYS,MENU_LOAD_CACHE,MENU_LOAD_AUDIO,MENU_LOAD_READY};
/* Cold-only startup canvas, permanently separate from engine and DMA. */
int peaq_menu_loading(unsigned stage,unsigned percent);
void peaq_menu_loading_error(unsigned code);
/* Sound snapshot: volume, PCM peak, submitted/completed blocks, error,
 * followed by the thirteen pre-mute route measurements. */
int peaq_menu_exit_screen(unsigned selected,const uint32_t sound[18]);
void peaq_menu_runtime_error(unsigned audio_error,unsigned other_error);
void peaq_menu_volume_notice(unsigned level,uint32_t now);
void peaq_menu_volume_clear(void);
void peaq_menu_volume_overlay(uint16_t *pixels,uint32_t now);
int peaq_menu_saving(unsigned phase,unsigned percent);
#endif
