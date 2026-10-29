#include "retained_request.h"
static uint32_t retained_checksum(const PeaqRetainedRequest *r){
 uint32_t h=2166136261u;
 for(unsigned i=0;i<14;++i)for(unsigned b=0;b<4;++b)
  h=(h^((r->word[i]>>(8*b))&255u))*16777619u;
 return h;
}
int peaq_request_address(uint32_t a){
 return !(a&4095u)&&a>=PEAQ_REQUEST_POOL_FIRST&&a<=PEAQ_REQUEST_POOL_END-64u;
}
int peaq_request_owned(uint32_t p,uint32_t bytes,uint32_t a){
 return bytes==PEAQ_REQUEST_ALLOCATION&&p>=PEAQ_REQUEST_POOL_FIRST&&
  p<=PEAQ_REQUEST_POOL_END-bytes&&peaq_request_address(a)&&a>=p&&a-p<=bytes-64u;
}
void peaq_request_make(PeaqRetainedRequest *r,uint32_t magic,uint32_t a,
                       uint32_t payload,uint32_t nonce){
 for(unsigned i=0;i<16;++i)r->word[i]=0;
 r->word[0]=magic;r->word[1]=~magic;r->word[2]=1;
 r->word[3]=a;r->word[4]=~a;r->word[5]=payload;
 r->word[6]=nonce;r->word[7]=~nonce;r->word[8]=1;r->word[9]=~1u;
 r->word[10]=0x504d5031u;r->word[11]=0x32322d34u;r->word[12]=0x534d4152u;
 r->word[14]=retained_checksum(r);r->word[15]=~r->word[14];
}
int peaq_request_valid(const PeaqRetainedRequest *r,uint32_t magic,uint32_t a,uint32_t payload){
 return peaq_request_address(a)&&r->word[0]==magic&&r->word[1]==~magic&&
  r->word[2]==1&&r->word[3]==a&&r->word[4]==~a&&r->word[5]==payload&&
  r->word[6]&&r->word[7]==~r->word[6]&&r->word[8]==1&&r->word[9]==~1u&&
  r->word[10]==0x504d5031u&&r->word[11]==0x32322d34u&&r->word[12]==0x534d4152u&&
  !r->word[13]&&r->word[14]==retained_checksum(r)&&r->word[15]==~r->word[14];
}
