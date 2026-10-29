#include "pga.h"
#include "game_ui.h"
typedef struct {u32 version;u8 cells[9];u8 mode,turn,winner,moves,cursor;u16 wins[3];} Saved;
typedef struct {Saved d;u32 preferences;} Persist;
typedef struct {Persist p;GameUi ui;} State;
const u32 pga_state_bytes=sizeof(State);
const u32 pga_tick_ms=50;
const u32 pga_required_abi=3;
static const u8 lines[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
static u8 winner(const u8 *cells){for(u32 i=0;i<8;i++){u8 a=cells[lines[i][0]];if(a&&a==cells[lines[i][1]]&&a==cells[lines[i][2]])return a;}return 0;}
static void reset(State *s){for(u32 i=0;i<9;i++)s->p.d.cells[i]=0;s->p.d.turn=1;s->p.d.winner=0;s->p.d.moves=0;s->p.d.cursor=4;}
static int minimax(u8 *b,int player,int depth,int alpha,int beta){
 u8 w=winner(b);if(w)return w==2?10-depth:depth-10;int best=player==2?-100:100,free=0;
 for(int i=0;i<9;i++)if(!b[i]){
  free=1;b[i]=(u8)player;int score=minimax(b,3-player,depth+1,alpha,beta);b[i]=0;
  if(player==2){if(score>best)best=score;if(best>alpha)alpha=best;}else{if(score<best)best=score;if(best<beta)beta=best;}
  if(beta<=alpha)break;
 }
 return free?best:0;
}
static int choice(u8 *b){
 static const u8 order[]={4,0,2,6,8,1,3,5,7};int move=-1,best=-100;
 for(int n=0;n<9;n++){int i=order[n];if(!b[i]){b[i]=2;int score=minimax(b,1,0,-100,100);b[i]=0;if(score>best){best=score;move=i;}}}return move;
}
static int valid(const Saved *d){
 if(d->version!=1||d->mode<1||d->mode>2||d->turn<1||d->turn>2||d->winner>2||d->moves>9||d->cursor>8)return 0;
 u32 x=0,o=0;for(u32 i=0;i<9;i++){if(d->cells[i]>2)return 0;x+=d->cells[i]==1;o+=d->cells[i]==2;}
 return x+o==d->moves && (x==o||x==o+1) && winner(d->cells)==d->winner && (d->winner||d->moves==9||d->turn==(x==o?1:2));
}
static void store(const PgaApi *a,State *s){s->p.preferences=s->ui.flags;s->ui.save_error=!a->save(a->storage,&s->p,sizeof(s->p));}
static void move(const PgaApi *a,State *s,int cell){
 Saved *d=&s->p.d;if(d->winner||d->moves==9||cell<0||cell>8||d->cells[cell])return;
 d->cells[cell]=d->turn;d->moves++;d->winner=winner(d->cells);
 if(!d->winner&&d->moves<9){d->turn=3-d->turn;if(d->mode==1&&d->turn==2){int i=choice(d->cells);
  if(i>=0){d->cells[i]=2;d->moves++;d->winner=winner(d->cells);}d->turn=1;}}
 if(d->winner||d->moves==9){u32 k=d->winner;if(d->wins[k]<9999)d->wins[k]++;}store(a,s);
}

static const Note song[]={{523,200},{659,200},{784,400},{659,200},{587,200},{523,400},{440,200},{523,200},{659,400},{587,200},{494,200},{523,600},{0,400}};
static const Lesson lessons[]={
 {"LAUTSTAERKE",{"DIE SEITENTASTEN MACHEN DEN TON","LAUTER ODER LEISER. DIE STUFE","GILT FUER ALLE SPIELE UND WIRD","BEIM VERLASSEN AUF SD GESICHERT.","DIE NIEDRIGSTE STUFE IST STUMM.","MUSIK UND SFX KANNST DU IN DEN","EINSTELLUNGEN GETRENNT SCHALTEN."}},
 {"ZIEL UND SPIELMODI",{"DREI GLEICHE ZEICHEN IN EINER","REIHE GEWINNEN: WAAGERECHT,","SENKRECHT ODER DIAGONAL.","EIN SPIELER: DU BIST X, DER","COMPUTER SPIELT O. ZWEI SPIELER:","X UND O WECHSELN SICH AM HANDY","AB. X BEGINNT. VOLLES BRETT","OHNE SIEGER: UNENTSCHIEDEN."}},
 {"STEUERUNG",{"PFEILTASTEN: FELD AUSWAEHLEN.","OK: ZEICHEN IN DAS FELD SETZEN.","ALTERNATIV ENTSPRICHT 1 BIS 9","DEN NEUN FELDERN: 1 OBEN LINKS,","5 IN DER MITTE, 9 UNTEN RECHTS.","BELEGTE FELDER BLEIBEN UNBERUEHRT.","RECHTE SOFTTASTE ODER #: PAUSE.","NEUES SPIEL: MODUS AUSWAEHLEN."}},
 {"MENUES UND SPEICHERN",{"MENUES: OBEN/UNTEN ODER 2/8.","OK ODER 5 BESTAETIGT. RECHTE","SOFTTASTE GEHT ZURUECK; IM","HAUPTMENUE ZUR SPIELELISTE.","MUSIK UND SFX SIND GETRENNT IN","EINSTELLUNGEN. DORT IST AUCH","DIESES TUTORIAL. AUTOMATISCHES","SPEICHERN NACH JEDEM ZUG UND","BEIM VERLASSEN AUF DIE SD-KARTE."}},
 {"FORTSETZEN",{"FORTSETZEN LAEDT DIE LETZTE","RUNDE. EIN NEUSTART ERSETZT","DIESE RUNDE; DIE SIEGZAHLEN","BLEIBEN. X / REMIS / O ZEIGT","DIE BISHERIGEN RUNDENERGEBNISSE.","BEI EINEM SD-FEHLER BLEIBT DER","LETZTE GUELTIGE STAND ERHALTEN.","TUTORIAL: LINKS/RECHTS BLAETTERN;","ZURUECK SCHLIESST ES."}}
};
#define PAGES (sizeof(lessons)/sizeof(*lessons))
static void cross(const PgaApi *a,int x,int y,int size,u16 color){
 for(int i=0;i<size;i++){a->fill(x+i,y+i,4,4,color);a->fill(x+size-i-1,y+i,4,4,color);}
}
static void ring(const PgaApi *a,int x,int y,int size,u16 color){
 a->fill(x+4,y,size-8,4,color);a->fill(x+4,y+size-4,size-8,4,color);
 a->fill(x,y+4,4,size-8,color);a->fill(x+size-4,y+4,4,size-8,color);
}
static void art(const PgaApi *a){
 center(a,25,2,0xffff,"TIC TAC TOE");
 a->fill(61,61,118,64,0x194b);cross(a,72,76,30,0x5e7b);ring(a,133,74,36,0xfd45);
 a->fill(119,68,2,50,0x4b33);
}
u32 pga_event(const PgaApi *a,void *memory,u32 event,u32 value){
 State *s=memory;Saved *d=&s->p.d;GameUi *u=&s->ui;
 if(event==PGA_INIT){
  if(a->version<3||!(a->capabilities&PGA_CAP_BACK)||!(a->capabilities&PGA_CAP_TONE))return PGA_QUIT;
  int restored=a->load(a->storage,&s->p,sizeof(s->p))&&prefs_valid(s->p.preferences)&&valid(d);
  if(!restored){s->p.preferences=UI_PREFS|3;restored=a->load(a->storage,d,sizeof(*d))&&valid(d);}
  if(!restored){d->version=1;d->mode=1;d->wins[0]=d->wins[1]=d->wins[2]=0;reset(s);}
  ui_init(u,s->p.preferences,restored);return PGA_REDRAW;
 }
 if(event==PGA_CLOSE||event==PGA_PAUSE){if(u->screen==UI_PLAY)ui_goto(u,UI_PAUSED);store(a,s);sound_stop(a,u);return 0;}
 if(event==PGA_RESUME)return PGA_REDRAW;
 if(event==PGA_TICK){sound_tick(a,u,song,sizeof(song)/sizeof(*song),value);return ui_tick(u,value)?PGA_REDRAW:0;}
 if(event==PGA_KEY){
  u32 action=ui_key(a,u,value,PAGES);
  if(action&UI_EXIT)return PGA_QUIT;
  if(action&UI_SAVE)store(a,s);
  if(action&UI_NEW){ui_goto(u,UI_MODE);u->selected=d->mode-1;}
  if(action&UI_CONTINUE){if(d->winner||d->moves==9)ui_goto(u,UI_RESULT);}
  if(action&UI_HANDLED)return PGA_REDRAW;
  if(u->screen==UI_MODE){
   if(ui_up(value)||ui_down(value))u->selected^=1;
   else if(value==PGA_BACK)ui_goto(u,UI_HOME);
   else if(ui_ok(value)){d->mode=(u8)(u->selected+1);reset(s);ui_goto(u,UI_PLAY);store(a,s);}
   return PGA_REDRAW;
  }
  if(u->screen==UI_PLAY){
   u8 before=d->moves;
   if(value>=1&&value<=9)move(a,s,(int)value-1);
   else if(value==PGA_OK)move(a,s,d->cursor);
   else if(value==PGA_UP&&d->cursor>=3)d->cursor-=3;
   else if(value==PGA_DOWN&&d->cursor<6)d->cursor+=3;
   else if(value==PGA_LEFT&&d->cursor%3)d->cursor--;
   else if(value==PGA_RIGHT&&d->cursor%3<2)d->cursor++;
   if(before!=d->moves)sound_effect(a,u,880,75);
   if(d->winner||d->moves==9){ui_goto(u,UI_RESULT);sound_effect(a,u,d->winner==1?1047:392,300);}
  }
  return PGA_REDRAW;
 }
 if(event!=PGA_PAINT)return 0;
 ui_background(a,0x5e7b);
 if(u->screen==UI_PLAY){
  center(a,18,2,0xffff,"TIC TAC TOE");center(a,46,1,0xad55,d->mode==1?"DU: X   COMPUTER: O":"SPIELER X / SPIELER O");
  for(int i=0;i<9;i++){
   int x=12+(i%3)*74,y=68+(i/3)*62;a->fill(x,y,68,56,d->cursor==i?0x2251:0x1927);
   if(d->cursor==i){a->fill(x,y,68,2,0x5e7b);a->fill(x,y+54,68,2,0x5e7b);}
   if(d->cells[i]==1)cross(a,x+20,y+14,26,0x5e7b);
   else if(d->cells[i]==2)ring(a,x+20,y+13,30,0xfd45);
  }
  center(a,268,2,d->turn==1?0x5e7b:0xfd45,d->turn==1?"X AM ZUG":"O AM ZUG");
  ui_notice(a,u);return 0;
 }
 if(u->screen!=UI_SETTINGS&&u->screen!=UI_TUTORIAL&&u->screen!=UI_PAUSED&&u->screen!=UI_CONFIRM)art(a);
 if(u->screen==UI_MODE){static const char *const modes[]={"EIN SPIELER","ZWEI SPIELER"};menu(a,u,modes,2,168,0x5e7b);}
 if(u->screen==UI_RESULT){
  center(a,140,2,0xffff,d->winner==1?"X GEWINNT":d->winner==2?"O GEWINNT":"UNENTSCHIEDEN");
  gt(a,34,170,1,0xad55,"X / REMIS / O");gn(a,130,170,1,0x5e7b,d->wins[1]);gn(a,166,170,1,0xad55,d->wins[0]);gn(a,202,170,1,0xfd45,d->wins[2]);
 }
 ui_menu(a,u,lessons,PAGES,0x5e7b);return 0;
}
