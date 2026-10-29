#include "pga.h"
#include "game_ui.h"
typedef struct {
 u32 version,seed,score,best,lines;
 u8 board[200],bag[7],bag_pos,piece,next,rotation;
 s16 x,y;u8 over,pause;
} Saved;
typedef struct {Saved d;u32 preferences;} Persist;
typedef struct {Persist p;GameUi ui;u32 gravity,lock;u8 lock_resets;} State;
const u32 pga_state_bytes=sizeof(State);
const u32 pga_tick_ms=50;
const u32 pga_required_abi=3;
static const u16 colors[7]={0x4e9c,0xfec9,0xb39f,0x66d0,0xfaaa,0x4b9e,0xfc88};
static const u16 shapes[7][4]={
 {0x00f0,0x4444,0x0f00,0x2222},{0x0066,0x0066,0x0066,0x0066},
 {0x0072,0x0262,0x0270,0x0232},{0x0036,0x0462,0x0360,0x0231},
 {0x0063,0x0264,0x0630,0x0132},{0x0071,0x0226,0x0470,0x0322},
 {0x0074,0x0622,0x0170,0x0223}
};
static u32 rng(Saved *d){d->seed=d->seed*1664525u+1013904223u;return d->seed>>16;}
static u8 draw_bag(Saved *d){
 if(d->bag_pos>=7){for(u32 i=0;i<7;i++)d->bag[i]=(u8)i;
  for(u32 i=6;i;i--){u32 j=rng(d)%(i+1);u8 t=d->bag[i];d->bag[i]=d->bag[j];d->bag[j]=t;}d->bag_pos=0;}
 return d->bag[d->bag_pos++];
}
static int fits(const Saved *d,int x,int y,int rotation){
 u16 bits=shapes[d->piece][rotation];
 for(int i=0;i<16;i++)if(bits&(1<<i)){
  int px=x+i%4,py=y+i/4;if(px<0||px>=10||py>=20)return 0;if(py>=0&&d->board[py*10+px])return 0;
 }
 return 1;
}
static void add_score(Saved *d,u32 n){if(n>99999999u-d->score)d->score=99999999;else d->score+=n;if(d->score>d->best)d->best=d->score;}
static void store(const PgaApi *a,State *s){s->p.preferences=s->ui.flags;s->ui.save_error=!a->save(a->storage,&s->p,sizeof(s->p));}
static void spawn(State *s){
 Saved *d=&s->p.d;d->piece=d->next;d->next=draw_bag(d);d->rotation=0;d->x=3;d->y=0;
 s->gravity=0;s->lock=0;s->lock_resets=0;if(!fits(d,d->x,d->y,0))d->over=1;
}
static void reset(State *s){
 Saved *d=&s->p.d;d->version=1;d->score=0;d->lines=0;
 for(u32 i=0;i<200;i++)d->board[i]=0;d->bag_pos=7;d->next=draw_bag(d);d->over=0;d->pause=1;spawn(s);
}
static int valid(const Saved *d){
 if(d->version!=1||d->piece>6||d->next>6||d->rotation>3||d->bag_pos>7||d->x< -3||d->x>9||d->y< -3||d->y>19||d->over>1||d->pause>1||d->score>99999999||d->best>99999999||d->best<d->score||d->lines>999999)return 0;
 u32 seen=0;for(u32 i=0;i<7;i++){if(d->bag[i]>6 || (seen&(1u<<d->bag[i])))return 0;seen|=1u<<d->bag[i];}
 for(u32 i=0;i<200;i++)if(d->board[i]>7)return 0;
 if(!d->over)for(u32 row=0;row<20;row++){u32 n=0;for(u32 col=0;col<10;col++)n+=d->board[row*10+col]!=0;if(n==10)return 0;}
 return d->over || fits(d,d->x,d->y,d->rotation);
}
static void lock_piece(const PgaApi *a,State *s){
 Saved *d=&s->p.d;u16 bits=shapes[d->piece][d->rotation];
 for(int i=0;i<16;i++)if(bits&(1<<i)){
  int x=d->x+i%4,y=d->y+i/4;if(y<0){d->over=1;store(a,s);return;}d->board[y*10+x]=d->piece+1;
 }
 u32 cleared=0,level=d->lines/10;int dest=19;
 for(int row=19;row>=0;row--){int full=1;for(int col=0;col<10;col++)if(!d->board[row*10+col])full=0;
  if(full){cleared++;continue;}for(int col=0;col<10;col++)d->board[dest*10+col]=d->board[row*10+col];dest--;}
 while(dest>=0){for(int col=0;col<10;col++)d->board[dest*10+col]=0;dest--;}
 static const u16 points[5]={0,100,300,500,800};if(cleared){add_score(d,points[cleared]*(level+1));d->lines+=cleared;if(d->lines>999999)d->lines=999999;}
 spawn(s);sound_effect(a,&s->ui,cleared?1047:330,cleared?180:45);store(a,s);
}
static void reset_lock(State *s){if(s->lock && s->lock_resets<12){s->lock=0;s->lock_resets++;}}
static void rotate(State *s){
 static const signed char kicks[9][2]={{0,0},{-1,0},{1,0},{-2,0},{2,0},{0,-1},{-1,-1},{1,-1},{0,-2}};
 Saved *d=&s->p.d;int r=(d->rotation+1)&3;
 for(u32 i=0;i<9;i++){int x=d->x+kicks[i][0],y=d->y+kicks[i][1];if(fits(d,x,y,r)){d->x=(s16)x;d->y=(s16)y;d->rotation=(u8)r;reset_lock(s);return;}}
}
static void block(const PgaApi *a,int x,int y,u16 color,int ghost){
 if(ghost){a->fill(x+1,y+1,9,1,color);a->fill(x+1,y+9,9,1,color);a->fill(x+1,y+1,1,9,color);a->fill(x+9,y+1,1,9,color);}
 else{a->fill(x+1,y+1,10,10,color);a->fill(x+2,y+2,8,1,0xffff);}
}

static const Note song[]={{330,150},{494,150},{659,300},{587,150},{494,150},{392,300},{440,150},{523,150},{659,300},{587,150},{440,150},{494,300},{330,300},{0,150},{392,150},{494,300}};
static const Lesson lessons[]={
 {"LAUTSTAERKE",{"DIE SEITENTASTEN MACHEN DEN TON","LAUTER ODER LEISER. DIE STUFE","GILT FUER ALLE SPIELE UND WIRD","BEIM VERLASSEN AUF SD GESICHERT.","DIE NIEDRIGSTE STUFE IST STUMM.","MUSIK UND SFX KANNST DU IN DEN","EINSTELLUNGEN GETRENNT SCHALTEN."}},
 {"DAS SPIEL",{"FUELL EINE WAAGERECHTE REIHE","OHNE LUECKEN. VOLLE REIHEN","VERSCHWINDEN. DIE STEINE DARUEBER","RUTSCHEN NACH UNTEN. DIE RUNDE","ENDET, WENN EIN NEUER STEIN","OBEN NICHT MEHR HINEINPASST.","DER UMRISS ZEIGT DIE LANDESTELLE.","DANACH ZEIGT DEN NAECHSTEN STEIN."}},
 {"ALLE SPIELTASTEN",{"LINKS/RECHTS ODER 4/6: BEWEGEN.","OBEN ODER 2: IM UHRZEIGERSINN","DREHEN. UNTEN ODER 8: SCHNELLER","SINKEN. OK ODER 5: SOFORT BIS","ZUM BODEN FALLEN UND ABLEGEN.","RECHTE SOFTTASTE ODER #: PAUSE.","BEWEGEN GEHT AUCH DURCH HALTEN.","NEUSTART IM HAUPTMENUE; DER","REKORD BLEIBT DABEI ERHALTEN."}},
 {"PUNKTE UND TEMPO",{"1/2/3/4 REIHEN GEBEN 100/300/","500/800 PUNKTE MAL DEM LEVEL.","MANUELLES SINKEN: 1 PUNKT JE","FELD. SOFORTIGES FALLEN: 2.","ALLE ZEHN REIHEN STEIGT DAS","LEVEL UND DAS TEMPO. AM BODEN","BLEIBT KURZ ZEIT ZUM KORRIGIEREN.","DER REKORD IST DEINE HOECHSTE","PUNKTZAHL UEBER ALLE RUNDEN."}},
 {"MENUES UND SPEICHERN",{"MENUES: OBEN/UNTEN ODER 2/8;","OK ODER 5 BESTAETIGT. RECHTE","SOFTTASTE ZURUECK, IM HAUPTMENUE","ZUR SPIELELISTE. MUSIK UND SFX","GETRENNT UNTER EINSTELLUNGEN.","STAND UND REKORD WERDEN NACH","JEDEM ABGELEGTEN STEIN, BEI","PAUSE UND BEIM VERLASSEN AUF","DER SD-KARTE GESPEICHERT."}},
 {"FORTSETZEN UND TUTORIAL",{"FORTSETZEN STARTET DIE GESICHERTE","RUNDE. NACH EINER UNTERBRECHUNG","BLEIBT DAS SPIEL ZUERST PAUSIERT.","BEI EINEM SD-FEHLER BLEIBT DER","LETZTE GUELTIGE STAND ERHALTEN.","HIER: LINKS/RECHTS BLAETTERN.","OK ZEIGT DIE NAECHSTE SEITE.","RECHTE SOFTTASTE SCHLIESST DAS","TUTORIAL."}}
};
#define PAGES (sizeof(lessons)/sizeof(*lessons))
static void art(const PgaApi *a){
 center(a,24,3,0xffff,"TETRIS");
 for(int n=0;n<7;n++){int px=36+(n%4)*45,py=67+(n/4)*32;u16 bits=shapes[n][0];
  for(int i=0;i<16;i++)if(bits&(1<<i))block(a,px+(i%4)*9,py+(i/4)*9,colors[n],0);}
}
u32 pga_event(const PgaApi *a,void *memory,u32 event,u32 value){
 State *s=memory;Saved *d=&s->p.d;GameUi *u=&s->ui;
 if(event==PGA_INIT){
  if(a->version<3||!(a->capabilities&PGA_CAP_BACK)||!(a->capabilities&PGA_CAP_TONE))return PGA_QUIT;
  int restored=a->load(a->storage,&s->p,sizeof(s->p))&&prefs_valid(s->p.preferences)&&valid(d);
  if(!restored){s->p.preferences=UI_PREFS|3;restored=a->load(a->storage,d,sizeof(*d))&&valid(d);}
  if(!restored){d->seed=0x54455452;d->best=0;reset(s);}d->pause=1;
  ui_init(u,s->p.preferences,restored);return PGA_REDRAW;
 }
 if(event==PGA_CLOSE||event==PGA_PAUSE){d->pause=1;if(u->screen==UI_PLAY)ui_goto(u,UI_PAUSED);store(a,s);sound_stop(a,u);return 0;}
 if(event==PGA_RESUME)return PGA_REDRAW;
 if(event==PGA_KEY){
  u32 action=ui_key(a,u,value,PAGES);
  if(action&UI_EXIT)return PGA_QUIT;
  if(action&UI_SAVE){d->pause=1;store(a,s);}
  if(action&UI_NEW){reset(s);d->pause=0;ui_goto(u,UI_PLAY);store(a,s);}
  if(action&UI_CONTINUE){d->pause=0;if(d->over)ui_goto(u,UI_RESULT);}
  if(action&UI_HANDLED)return PGA_REDRAW;
  if(u->screen!=UI_PLAY||d->over)return 0;
  if(value==PGA_LEFT||value==4){if(fits(d,d->x-1,d->y,d->rotation)){d->x--;reset_lock(s);}}
  else if(value==PGA_RIGHT||value==6){if(fits(d,d->x+1,d->y,d->rotation)){d->x++;reset_lock(s);}}
  else if(value==PGA_UP||value==2){rotate(s);sound_effect(a,u,523,35);}
  else if(value==PGA_DOWN||value==8){if(fits(d,d->x,d->y+1,d->rotation)){d->y++;add_score(d,1);s->gravity=0;s->lock=0;}}
  else if(value==PGA_OK||value==5){while(fits(d,d->x,d->y+1,d->rotation)){d->y++;add_score(d,2);}lock_piece(a,s);}
  if(d->over){ui_goto(u,UI_RESULT);sound_effect(a,u,196,400);}return PGA_REDRAW;
 }
 if(event==PGA_TICK){
  sound_tick(a,u,song,sizeof(song)/sizeof(*song),value);if(ui_tick(u,value))return PGA_REDRAW;
  if(u->screen!=UI_PLAY||d->over)return 0;
  u32 level=d->lines/10,interval=800-(level>18?18:level)*40;int changed=0;
  s->gravity+=value;if(s->gravity>=interval){s->gravity-=interval;if(fits(d,d->x,d->y+1,d->rotation)){d->y++;changed=1;}}
  if(!fits(d,d->x,d->y+1,d->rotation)){s->lock+=value;if(s->lock>=350){lock_piece(a,s);changed=1;}}else s->lock=0;
  if(d->over){ui_goto(u,UI_RESULT);sound_effect(a,u,196,400);}return changed?PGA_REDRAW:0;
 }
 if(event!=PGA_PAINT)return 0;
 ui_background(a,0xb39f);
 if(u->screen==UI_PLAY){
  gt(a,12,17,2,0xffff,"TETRIS");a->fill(9,47,113,223,0x4b33);a->fill(10,48,110,220,0x1063);
  for(int y=0;y<20;y++)for(int x=0;x<10;x++)if(d->board[y*10+x])block(a,10+x*11,48+y*11,colors[d->board[y*10+x]-1],0);
  int gy=d->y;while(fits(d,d->x,gy+1,d->rotation))gy++;
  u16 bits=shapes[d->piece][d->rotation];for(int i=0;i<16;i++)if(bits&(1<<i)){
   int x=d->x+i%4,y=d->y+i/4,g=gy+i/4;if(g>=0)block(a,10+x*11,48+g*11,0x7bcf,1);if(y>=0)block(a,10+x*11,48+y*11,colors[d->piece],0);}
  gt(a,136,50,1,0xad55,"PUNKTE");gn(a,136,66,1,0xffff,d->score);
  gt(a,136,91,1,0xad55,"REKORD");gn(a,136,107,1,0xb39f,d->best);
  gt(a,136,132,1,0xad55,"LINIEN");gn(a,136,148,1,0xffff,d->lines);
  gt(a,136,173,1,0xad55,"LEVEL");gn(a,136,189,1,0xffff,d->lines/10+1);
  gt(a,136,221,1,0xad55,"DANACH");u16 preview=shapes[d->next][0];
  for(int i=0;i<16;i++)if(preview&(1<<i))block(a,140+(i%4)*11,238+(i/4)*11,colors[d->next],0);
  ui_notice(a,u);return 0;
 }
 if(u->screen!=UI_SETTINGS&&u->screen!=UI_TUTORIAL&&u->screen!=UI_PAUSED&&u->screen!=UI_CONFIRM)art(a);
 if(u->screen==UI_RESULT){center(a,137,2,0xffff,"RUNDE BEENDET");gt(a,22,169,1,0xad55,"PUNKTE");gn(a,74,169,1,0xffff,d->score);gt(a,130,169,1,0xad55,"BEST");gn(a,164,169,1,0xb39f,d->best);}
 ui_menu(a,u,lessons,PAGES,0xb39f);return 0;
}
