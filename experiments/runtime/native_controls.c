#include "native_controls.h"
#include "../doom/platform.h"

#define KEY(n) (1u << (n))
/* Raw KEY_WEB is 0x26. Bit30 is otherwise unused by this bounded map. */
#define CENTER_WEB KEY(30)
uint32_t peaq_doom_control_bit(unsigned key){
 switch(key){
 case 1:case 2:case 3:case 4:case 0x0c:case 0x0f:
 case 0x12:case 0x11:case 0x16:case 0x1b:return KEY(key);
 case 0x26:return CENTER_WEB;
 default:return 0;
 }
}
uint32_t peaq_doom_control_actions(uint32_t p){
 uint32_t out=0;
 if(p&KEY(1))out|=PORT_LEFT;
 if(p&KEY(2))out|=PORT_RIGHT;
 if(p&KEY(3))out|=PORT_DOWN;
 if(p&KEY(4))out|=PORT_UP;
 if(p&KEY(0x0c))out|=PORT_LOOK_LEFT;  /* 5 */
 if(p&KEY(0x0f))out|=PORT_LOOK_RIGHT; /* 8 */
 if(p&KEY(0x12))out|=PORT_USE;        /* 0 */
 if(p&KEY(0x11))out|=PORT_MENU;       /* star */
 if(p&KEY(0x16))out|=PORT_ACCEPT;     /* left softkey */
 if(p&(KEY(0x1b)|CENTER_WEB))out|=PORT_FIRE|PORT_ACCEPT;
 return out;
}
uint32_t peaq_licar_control_actions(uint32_t low,uint32_t high,uint32_t state){
 /* Licar key bits: U,R,D,L,A,B. Its A means confirm in menus and reset
  * during a race. Keep those physical bindings separate. State0 is MENU. */
 uint32_t out=((!!(low&KEY(2)))<<1)|((!!(low&KEY(1)))<<3)|((!!(low&KEY(0x17)))<<5);
 if(state==0){
  if(low&KEY(4))out|=1;
  if(low&KEY(3))out|=4;
  if((low&(KEY(0x12)|KEY(0x16)|KEY(0x1b)))||(high&KEY(6)))out|=16;
 }else{
  if(low&KEY(0x0c))out|=1;  /* 5: throttle */
  if(low&KEY(0x0b))out|=4;  /* 4: brake/reverse */
  if(low&KEY(0x09))out|=16; /* 2: reset */
 }
 return out;
}
