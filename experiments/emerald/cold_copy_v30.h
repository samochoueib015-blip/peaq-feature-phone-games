/* Owned synchronous RAM buffers only. Prefix/tail stay byte-wise; word
 * accesses require both addresses aligned. No widened end reads/writes. */
#ifndef PEAQ_COLD_COPY_V30_H
#define PEAQ_COLD_COPY_V30_H
#include <stdint.h>
static inline void peaq_cold_copy(void *out,const void *in,uint32_t n){
 uint8_t *d=out;const uint8_t *s=in;
 if(!(((uintptr_t)d^(uintptr_t)s)&3u)){
  while(n&&((uintptr_t)d&3u)){*d++=*s++;--n;}
  while(n>=4u){*(uint32_t *)d=*(const uint32_t *)s;d+=4;s+=4;n-=4;}
 }
 while(n--)*d++=*s++;
}
#endif
