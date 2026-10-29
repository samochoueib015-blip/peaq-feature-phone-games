#include "pga.h"
#include "game_ui.h"
typedef struct {u32 version,seed,score,best;u16 body[168],length,food;u8 direction,next,over,pause;} Saved;
typedef struct {Saved d;u32 preferences;} Persist;
typedef struct {Persist p;GameUi ui;u32 elapsed;} State;
const u32 pga_state_bytes=sizeof(State);
const u32 pga_tick_ms=50;
const u32 pga_required_abi=3;
static int occupied(const Saved *d,u16 cell,u32 n){for(u32 i=0;i<n;i++)if(d->body[i]==cell)return 1;return 0;}
static void food(Saved *d){
 d->seed=d->seed*1664525u+1013904223u;u32 cell=(d->seed>>16)%168;
 for(u32 i=0;i<168;i++){if(!occupied(d,(u16)cell,d->length)){d->food=(u16)cell;return;}cell=(cell+1)%168;}d->over=2;
}
static void reset(State *s){
 Saved *d=&s->p.d;d->version=1;d->seed=d->seed*1664525u+1013904223u;
 d->score=0;d->length=3;d->body[0]=77;d->body[1]=76;d->body[2]=75;
 d->direction=3;d->next=3;d->over=0;d->pause=1;s->elapsed=0;food(d);
}
static int valid(const Saved *d){
 if(d->version!=1||d->length<3||d->length>168||d->food>=168||d->direction>3||d->next>3||d->over>2||d->pause>1||d->score>1650||d->best>1650)return 0;
 if(d->score!=(d->length-3u)*10 || d->best<d->score)return 0;
 for(u32 i=0;i<d->length;i++){
  if(d->body[i]>=168||occupied(d,d->body[i],i))return 0;
  if(i){int x=d->body[i]%12-d->body[i-1]%12,y=d->body[i]/12-d->body[i-1]/12;
   if(!((x==0&&(y==1||y== -1))||(y==0&&(x==1||x== -1))))return 0;}
 }
 return d->over==2 || !occupied(d,d->food,d->length);
}
static void store(const PgaApi *a,State *s){s->p.preferences=s->ui.flags;s->ui.save_error=!a->save(a->storage,&s->p,sizeof(s->p));}

static const Note song[]={{392,200},{494,200},{587,200},{494,200},{440,400},{392,200},{330,200},{392,400},{523,200},{494,200},{440,200},{330,200},{392,400},{0,400}};
static const Lesson lessons[]={
 {"LAUTSTAERKE",{"DIE SEITENTASTEN MACHEN DEN TON","LAUTER ODER LEISER. DIE STUFE","GILT FUER ALLE SPIELE UND WIRD","BEIM VERLASSEN AUF SD GESICHERT.","DIE NIEDRIGSTE STUFE IST STUMM.","MUSIK UND SFX KANNST DU IN DEN","EINSTELLUNGEN GETRENNT SCHALTEN."}},
 {"ZIEL",{"SAMMLE DAS ROTE FUTTER. MIT","JEDEM HAPPEN WAECHST DIE SCHLANGE","UM EIN FELD UND DU BEKOMMST","ZEHN PUNKTE. DAS TEMPO STEIGT.","BERUEHRE WEDER DEN RAND NOCH","DEINEN EIGENEN KOERPER. WENN DU","DAS GANZE FELD FUELLST, IST DIE","RUNDE GEWONNEN."}},
 {"STEUERUNG",{"PFEILTASTEN LENKEN DIE SCHLANGE.","ALTERNATIV: 2 OBEN, 8 UNTEN,","4 LINKS, 6 RECHTS. UMKEHREN IN","DIE ENTGEGENGESETZTE RICHTUNG","IST NICHT MOEGLICH. JE SCHRITT","WIRD EINE NEUE RICHTUNG ERLAUBT.","RECHTE SOFTTASTE, # ODER OK/5","OEFFNET DIE PAUSE. DORT KANNST","DU FORTSETZEN ODER ZUM MENUE."}},
 {"MENUES UND TON",{"MENUES: OBEN/UNTEN ODER 2/8.","OK ODER 5 BESTAETIGT. RECHTE","SOFTTASTE GEHT ZURUECK; IM","HAUPTMENUE ZUR SPIELELISTE.","EINSTELLUNGEN STEHT UNTEN IM","HAUPTMENUE. DORT KANNST DU","MUSIK UND SFX GETRENNT SCHALTEN","UND DIESES TUTORIAL OEFFNEN.","DIE EINSTELLUNGEN BLEIBEN."}},
 {"SPEICHERN",{"FORTSCHRITT UND REKORD WERDEN","NACH JEDEM FUTTER, BEI PAUSE","UND BEIM VERLASSEN AUF SD","GESICHERT. FORTSETZEN STARTET","DIE LETZTE RUNDE. NEUES SPIEL","ERSETZT SIE; DER REKORD BLEIBT.","BEI EINEM SD-FEHLER BLEIBT DER","LETZTE GUELTIGE STAND ERHALTEN.","HIER: LINKS/RECHTS; ZURUECK ENDE."}}
};
#define PAGES (sizeof(lessons)/sizeof(*lessons))
static void art(const PgaApi *a){
 center(a,24,3,0xffff,"SNAKE");
 for(int i=0;i<7;i++){int x=50+i*18,y=i<3?78:96;a->fill(x,y,17,17,i==6?0xaff5:0x4dab);}
 a->fill(162,100,3,3,0x0842);a->fill(162,109,3,3,0x0842);
 a->fill(182,78,13,13,0xfa6a);a->fill(188,72,3,6,0x66d0);
}
u32 pga_event(const PgaApi *a,void *memory,u32 event,u32 value){
 State *s=memory;Saved *d=&s->p.d;GameUi *u=&s->ui;
 if(event==PGA_INIT){
  if(a->version<3||!(a->capabilities&PGA_CAP_BACK)||!(a->capabilities&PGA_CAP_TONE))return PGA_QUIT;
  int restored=a->load(a->storage,&s->p,sizeof(s->p))&&prefs_valid(s->p.preferences)&&valid(d);
  if(!restored){s->p.preferences=UI_PREFS|3;restored=a->load(a->storage,d,sizeof(*d))&&valid(d);}
  if(!restored){d->best=0;d->seed=0x50454151;reset(s);}d->pause=1;
  ui_init(u,s->p.preferences,restored);return PGA_REDRAW;
 }
 if(event==PGA_PAUSE||event==PGA_CLOSE){d->pause=1;if(u->screen==UI_PLAY)ui_goto(u,UI_PAUSED);store(a,s);sound_stop(a,u);return 0;}
 if(event==PGA_RESUME)return PGA_REDRAW;
 if(event==PGA_KEY){
  if(u->screen==UI_PLAY&&ui_ok(value))value=PGA_BACK;
  u32 action=ui_key(a,u,value,PAGES);
  if(action&UI_EXIT)return PGA_QUIT;
  if(action&UI_SAVE){d->pause=1;store(a,s);}
  if(action&UI_NEW){reset(s);d->pause=0;ui_goto(u,UI_PLAY);store(a,s);}
  if(action&UI_CONTINUE){d->pause=0;s->elapsed=0;if(d->over)ui_goto(u,UI_RESULT);}
  if(action&UI_HANDLED)return PGA_REDRAW;
  int dir=value==PGA_UP||value==2?0:value==PGA_DOWN||value==8?1:value==PGA_LEFT||value==4?2:value==PGA_RIGHT||value==6?3:-1;
  if(u->screen==UI_PLAY&&dir>=0&&(dir^1)!=d->direction&&d->next==d->direction)d->next=(u8)dir;
  return 0;
 }
 if(event==PGA_TICK){
  sound_tick(a,u,song,sizeof(song)/sizeof(*song),value);if(ui_tick(u,value))return PGA_REDRAW;
  if(u->screen!=UI_PLAY||d->over)return 0;s->elapsed+=value;
  u32 interval=220-(d->score>140?140:d->score);if(s->elapsed<interval)return 0;s->elapsed-=interval;
  int x=d->body[0]%12,y=d->body[0]/12;d->direction=d->next;
  if(d->direction==0)y--;if(d->direction==1)y++;if(d->direction==2)x--;if(d->direction==3)x++;
  if(x<0||x>=12||y<0||y>=14){d->over=1;ui_goto(u,UI_RESULT);store(a,s);sound_effect(a,u,196,350);return PGA_REDRAW;}
  u16 next=(u16)(y*12+x);int grow=next==d->food;
  if(occupied(d,next,d->length-(grow?0:1))){d->over=1;ui_goto(u,UI_RESULT);store(a,s);sound_effect(a,u,196,350);return PGA_REDRAW;}
  if(grow&&d->length<168){d->length++;d->score+=10;if(d->score>d->best)d->best=d->score;}
  for(u32 i=d->length-1;i;i--)d->body[i]=d->body[i-1];d->body[0]=next;
  if(grow){food(d);store(a,s);sound_effect(a,u,1047,100);}
  if(d->over){ui_goto(u,UI_RESULT);store(a,s);}return PGA_REDRAW;
 }
 if(event!=PGA_PAINT)return 0;
 ui_background(a,0x66d0);
 if(u->screen==UI_PLAY){
  gt(a,12,14,2,0xffff,"SNAKE");gt(a,12,39,1,0xad55,"PUNKTE");gn(a,58,39,1,0xffff,d->score);
  gt(a,124,39,1,0xad55,"REKORD");gn(a,170,39,1,0x66d0,d->best);
  a->fill(22,53,196,228,0x2b2a);a->fill(24,55,192,224,0x1063);
  for(int y=0;y<14;y++)for(int x=0;x<12;x++)if((x+y)&1)a->fill(24+x*16,55+y*16,16,16,0x18c4);
  if(d->over!=2){int x=24+(d->food%12)*16,y=55+(d->food/12)*16;a->fill(x+4,y+4,9,9,0xfa6a);a->fill(x+8,y+1,3,4,0x66d0);}
  for(u32 i=0;i<d->length;i++){
   int x=25+(d->body[i]%12)*16,y=56+(d->body[i]/12)*16;a->fill(x,y,14,14,i?0x4dab:0xaff5);
   if(!i){int dx=d->direction==2?2:d->direction==3?10:3,dy=d->direction==0?2:d->direction==1?10:3;
    a->fill(x+dx,y+dy,2,2,0x1063);a->fill(x+(d->direction<2?9:dx),y+(d->direction<2?dy:9),2,2,0x1063);}
  }
  ui_notice(a,u);return 0;
 }
 if(u->screen!=UI_SETTINGS&&u->screen!=UI_TUTORIAL&&u->screen!=UI_PAUSED&&u->screen!=UI_CONFIRM)art(a);
 if(u->screen==UI_RESULT){center(a,138,2,0xffff,d->over==2?"GESCHAFFT!":"RUNDE BEENDET");gt(a,22,169,1,0xad55,"PUNKTE");gn(a,74,169,1,0xffff,d->score);gt(a,130,169,1,0xad55,"BEST");gn(a,164,169,1,0x66d0,d->best);}
 ui_menu(a,u,lessons,PAGES,0x66d0);return 0;
}
