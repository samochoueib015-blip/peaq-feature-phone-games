#include "boot_ticket.h"
static uint32_t word(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t hash(const uint8_t *p,unsigned n,uint32_t h){while(n--)h=(h^*p++)*16777619u;return h;}
static uint32_t checksum(const uint8_t *p){return hash(p+20,492,hash(p,16,2166136261u));}
static int valid(const uint8_t *p){
 if(word(p)!=0x31534750u||word(p+4)!=1||word(p+8)!=492||word(p+16)!=checksum(p))return 0;
 if(word(p+20)!=PEAQ_BOOT_REQUEST||word(p+24)!=1||word(p+28)>1||word(p+36)>8)return 0;
 for(unsigned i=40;i<512;++i)if(p[i])return 0;
 return 1;
}
void peaq_boot_request(uint8_t p[492],uint32_t crc,uint32_t volume){
 for(unsigned i=0;i<492;++i)p[i]=0;
 put(p,PEAQ_BOOT_REQUEST);put(p+4,1);put(p+8,1);put(p+12,crc);put(p+16,volume<=8?volume:0);
}
static int latest(const PeaqBootTicketIo *io,uint8_t s[1536]){
 /* Both slots are provisioned and valid, including the initial inactive
  * tickets. A damaged slot is not treated as an absent request. */
 if(io->read(io->cookie,0,s)!=1||io->read(io->cookie,1,s+512)!=1||!valid(s)||!valid(s+512))return -1;
 uint32_t a=word(s+12),b=word(s+512+12),delta=b-a;
 if(delta==0x80000000u)return -1;
 unsigned slot=(int32_t)delta>0;
 if(a==b){for(unsigned i=0;i<512;++i)if(s[i]!=s[512+i])return -1;}
 return (int)slot;
}
int peaq_boot_peek(const PeaqBootTicketIo *io,uint32_t *crc,uint32_t *volume,uint8_t s[1536]){
 if(!io||!io->read||!crc||!volume||!s)return -1;
 int slot=latest(io,s);if(slot<0)return -1;const uint8_t *p=s+(unsigned)slot*512;
 if(!word(p+28))return 0;
 *crc=word(p+32);*volume=word(p+36);return 1;
}
int peaq_boot_consume(const PeaqBootTicketIo *io,uint32_t crc,uint32_t *volume,uint8_t s[1536]){
 if(!io||!io->read||!io->write||!io->sync||!volume||!s)return -1;
 int found=latest(io,s);if(found<0)return -1;unsigned slot=(unsigned)found;uint8_t *p=s+slot*512;
 if(!word(p+28))return 0;
 if(word(p+32)!=crc)return -1;
 uint32_t selected=word(p+36);
 put(p+28,0);put(p+16,checksum(p));
 if(io->write(io->cookie,slot,p)!=1||io->sync(io->cookie)!=1||io->read(io->cookie,slot,s+1024)!=1)return -1;
 for(unsigned i=0;i<512;++i)if(p[i]!=s[1024+i])return -1;
 *volume=selected;return 1;
}
