#ifndef PEAQ_GFX_H
#define PEAQ_GFX_H
#include "pga.h"
/* Original code-native 5x7 glyphs, columns least-significant bit at top.
 * The small font keeps every game independent of the phone's text metrics. */
static const u8 glyphs[][5]={
 {0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},
 {36,42,127,42,18},{35,19,8,100,98},{54,73,85,34,80},{0,5,3,0,0},
 {0,28,34,65,0},{0,65,34,28,0},{20,8,62,8,20},{8,8,62,8,8},
 {0,80,48,0,0},{8,8,8,8,8},{0,96,96,0,0},{32,16,8,4,2},
 {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
 {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
 {54,73,73,73,54},{6,73,73,41,30},{0,54,54,0,0},{0,86,54,0,0},
 {8,20,34,65,0},{20,20,20,20,20},{0,65,34,20,8},{2,1,81,9,6},
 {62,65,93,85,30},{126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},
 {127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},
 {127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},
 {127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},
 {127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},
 {1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},
 {99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67},{0,127,65,65,0},
 {2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64}
};
static inline void gt(const PgaApi *a,int x,int y,int scale,u16 color,const char *s){
 for(u32 n=0;s[n] && n<40;n++,x+=6*scale){
  u8 c=(u8)s[n];if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
  if(x+5*scale>240)break;
  for(int col=0;col<5;col++)for(int row=0;row<7;row++)
   if(glyphs[c-32][col]&(1<<row))a->fill(x+col*scale,y+row*scale,scale,scale,color);
 }
}
static inline void gn(const PgaApi *a,int x,int y,int scale,u16 color,u32 value){
 char s[11];u32 n=10;s[n]=0;do{s[--n]='0'+value%10;value/=10;}while(value && n);
 gt(a,x,y,scale,color,s+n);
}
static inline void banner(const PgaApi *a,const char *title,u16 accent){
 a->fill(0,0,240,320,0x0842);a->fill(0,0,240,4,accent);gt(a,12,14,2,0xffff,title);
}
static inline void gsoftkeys(const PgaApi *a,const char *left,const char *right){
 a->fill(0,298,240,22,0x0842);a->fill(0,298,240,1,0x4a69);
 if(left)gt(a,8,302,2,0xffff,left);
 if(right){int n=0;while(right[n])n++;gt(a,232-(n*6-1)*2,302,2,0xffff,right);}
}
#endif
