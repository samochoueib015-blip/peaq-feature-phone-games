#include "mocor_time.h"
uint32_t peaq_mocor_milliseconds(void){
 return ((uint32_t (*)(void))PEAQ_SCI_GET_TICK_COUNT_THUMB)();
}
