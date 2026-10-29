#ifndef PEAQ_MENU_PAUSE_V25_H
#define PEAQ_MENU_PAUSE_V25_H
#include "cold_key_input.h"
typedef struct {uint32_t phase,selected,polls,confirmed,page,revision;} PeaqMenuExit;
enum {PEAQ_MENU_STAY=0,PEAQ_MENU_RESUME=1,PEAQ_MENU_EXIT=2};
enum {MENU_PAUSE_HOME=0,MENU_PAUSE_SETTINGS=1,MENU_PAUSE_QUIT=2};
void peaq_menu_exit_begin(PeaqMenuExit *);
int peaq_menu_exit_poll(PeaqMenuExit *,const PeaqKeyFrame *);
int peaq_menu_pause_screen(const PeaqMenuExit *);
#endif
