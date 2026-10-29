#ifndef PEAQ_NATIVE_CONTROLS_H
#define PEAQ_NATIVE_CONTROLS_H
#include <stdint.h>
/* Owner-selected physical key labels. The handset remains 90 degrees CCW.
 * DOOM keeps independent physical bits until after release processing, so
 * releasing a softkey cannot release an OK key which is still held. */
uint32_t peaq_doom_control_bit(unsigned raw_key);
uint32_t peaq_doom_control_actions(uint32_t physical);
uint32_t peaq_licar_control_actions(uint32_t low, uint32_t high, uint32_t engine_state);
#endif
