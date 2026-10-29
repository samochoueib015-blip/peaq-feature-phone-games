#ifndef PEAQ_RETAINED_REQUEST_H
#define PEAQ_RETAINED_REQUEST_H
#include <stdint.h>
/* Pinned stock Dynamic App Heap, from its BYTE control block. These bounds
 * are a read-only scan allowlist, never a reservation or permission to write.
 * A producer must first obtain an allocation from the normal OS allocator.
 */
#define PEAQ_REQUEST_POOL_FIRST 0x82890124u
#define PEAQ_REQUEST_POOL_END   0x82db0db4u
#define PEAQ_REQUEST_ALLOCATION 65536u
#define PEAQ_REQUEST_MAGIC     0x31524d45u
#define PEAQ_REQUEST_PROBE     0x31504d45u
#define PEAQ_REQUEST_BYTES     64u
typedef struct { uint32_t word[16]; } PeaqRetainedRequest;
int peaq_request_address(uint32_t address);
int peaq_request_owned(uint32_t allocation,uint32_t bytes,uint32_t address);
void peaq_request_make(PeaqRetainedRequest *,uint32_t magic,uint32_t address,
                       uint32_t payload,uint32_t nonce);
int peaq_request_valid(const PeaqRetainedRequest *,uint32_t magic,
                       uint32_t address,uint32_t payload);
#endif
