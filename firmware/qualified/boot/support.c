#include <stddef.h>
void *memset(void *p,int c,size_t n){unsigned char *q=p;while(n--)*q++=(unsigned char)c;return p;}
void *memcpy(void *p,const void *s,size_t n){unsigned char *q=p;const unsigned char *r=s;while(n--)*q++=*r++;return p;}
