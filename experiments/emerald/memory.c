#include <stddef.h>
void *memcpy(void *dest,const void *src,size_t size){
 unsigned char *d=dest;const unsigned char *s=src;while(size--)*d++=*s++;return dest;
}
void *memset(void *dest,int value,size_t size){
 unsigned char *d=dest;while(size--)*d++=(unsigned char)value;return dest;
}
void __aeabi_memcpy(void *d,const void *s,size_t n){memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n){memcpy(d,s,n);}
void __aeabi_memclr4(void *d,size_t n){memset(d,0,n);}
