/* Early-start discriminator only. No MMIO, allocation, SD access, CPU-control
 * changes or RAM writes. Caller must already have checked MMU/cache-off entry.
 * A valid retained request merely permits later SD ticket verification: it
 * never substitutes for the existing one-shot, file ownership and hash checks.
 */
#include "retained_request.h"
uint32_t peaq_request_find(uint32_t payload){
 uint32_t found=0;
 for(uint32_t a=(PEAQ_REQUEST_POOL_FIRST+4095u)&~4095u;
     a<=PEAQ_REQUEST_POOL_END-PEAQ_REQUEST_BYTES;a+=4096u){
  const volatile uint32_t *p=(const volatile uint32_t *)(uintptr_t)a;
  if(*p!=PEAQ_REQUEST_MAGIC)continue;
  PeaqRetainedRequest copy;
  for(unsigned i=0;i<16;++i)copy.word[i]=p[i];
  if(!peaq_request_valid(&copy,PEAQ_REQUEST_MAGIC,a,payload))continue;
  if(found)return 0; /* Ambiguous requests fail closed, never first-match. */
  found=a;
 }
 return found;
}
