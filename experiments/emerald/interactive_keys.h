#ifndef PEAQ_EMERALD_INTERACTIVE_KEYS_H
#define PEAQ_EMERALD_INTERACTIVE_KEYS_H
#include "cold_key_input.h"
#include "keypad_capture_v3.h"
typedef struct {
 uint32_t initialized,stopped,polls,records,overwrites,max_gap_ms,error;
 uint32_t quiet_ms,quiet_ready;
} PeaqInteractiveKeys;
int peaq_interactive_keys_init(uint32_t,uint32_t,uint32_t);
int peaq_interactive_keys_poll(uint32_t);
int peaq_interactive_keys_take(PeaqKeyFrame *);
void peaq_interactive_keys_stop(void);
int peaq_interactive_keys_guards(void);
const PeaqInteractiveKeys *peaq_interactive_keys_status(void);
const PeaqKeyInput *peaq_interactive_keys_input(void);
const PeaqKeyRecord *peaq_interactive_keys_record(unsigned);
#endif
