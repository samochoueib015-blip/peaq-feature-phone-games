#ifndef PEAQ_COLD_KEY_INPUT_H
#define PEAQ_COLD_KEY_INPUT_H
#include <stdint.h>
#define PEAQ_KEY_INPUT_MAGIC 0x314e494bu /* KIN1 */
#define PEAQ_KEY_SNAPSHOT_MAGIC 0x31534b50u /* PKS1 */
enum {PEAQ_KEY_PRESS_SUPPRESSED=1,PEAQ_KEY_COALESCED=2,PEAQ_KEY_BAD_RAW=4,
 PEAQ_KEY_BAD_POSITION=8,PEAQ_KEY_RESET=16,PEAQ_KEY_OPPOSING_DIRECTIONS=32};
enum {PEAQ_KEY_HOST_BACK=1,PEAQ_KEY_HOST_OPTIONS=2};
enum {PEAQ_KEY_RAW=0x40250008u,PEAQ_KEY_STATUS=0x4025002cu};
typedef struct {
 uint32_t magic,flags,generation,samples;
 uint32_t held[2],pressed[2],released[2];
 uint32_t gba,gba_pressed,gba_released,host,host_pressed,host_released;
 uint32_t raw,status,press_count,release_count,long_count,suppressed_count;
 uint32_t invalid_count,reset_count,coalesced_count,reserved;
} PeaqKeyInput;
typedef struct {
 uint32_t generation,flags,held[2],pressed[2],released[2];
 uint32_t gba,gba_pressed,gba_released,host,host_pressed,host_released;
 uint32_t emerald_buttons,reserved;
} PeaqKeyFrame;
typedef struct {
 uint32_t magic,flags,raw,status,reads,attempts,ack_mask,error;
} PeaqKeySnapshot;
typedef struct {
 void *cookie;
 int (*read)(void *cookie,uint32_t address,uint32_t *value);
} PeaqKeyRead;
void peaq_key_input_init(PeaqKeyInput *input);
/* Pure snapshot decoder: original matrix order; no MMIO or acknowledgement. */
int peaq_key_input_feed(PeaqKeyInput *input,uint32_t raw,uint32_t status);
int peaq_key_input_take(PeaqKeyInput *input,PeaqKeyFrame *frame);
/* Explicit focus-loss/transport-failure neutralization. Never guesses timeout. */
void peaq_key_input_release_all(PeaqKeyInput *input);
uint32_t peaq_key_matrix_index(uint32_t status_byte);
uint32_t peaq_key_logical_code(uint32_t matrix_index);
uint32_t peaq_key_gba_code(uint32_t logical_code);
/* Read-only acquisition: <=4 attempts/16 callbacks, only RAW and STATUS.
 * 1 stable event,0 stable idle,-1 callback error,-2 unstable,-3 bad arguments,
 * -4 unsupported raw bits. ack_mask reports the original driver's fixed0xfff,
 * but this component NEVER writes it or initializes the keypad/pads/clock.
 */
int peaq_key_capture(const PeaqKeyRead *reader,PeaqKeySnapshot *snapshot);
#endif
