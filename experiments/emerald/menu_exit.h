#ifndef PEAQ_MENU_EXIT_H
#define PEAQ_MENU_EXIT_H
#include "cold_key_input.h"
typedef struct {uint32_t phase,selected,polls,confirmed;} PeaqMenuExit;
enum {PEAQ_MENU_STAY=0,PEAQ_MENU_RESUME=1,PEAQ_MENU_EXIT=2};
void peaq_menu_exit_begin(PeaqMenuExit *);
int peaq_menu_exit_poll(PeaqMenuExit *,const PeaqKeyFrame *);
#endif
