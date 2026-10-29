/* Native PGA2 launcher. Firmware owns only UI, package validation and APIs.
 * Game programs, metadata, thumbnails and dual-slot saves reside on SD.
 * No flash API is linked. All mutable runtime data belongs to this window. */
#include "pga.h"
#include "gfx.h"
#include "abi5_segments.h"
#include "anchor.h"
#include "native_launch_design.h"
#include "mocor_time.h"
#define CANDIDATE_CALLBACK_BASE 0x88fb7000u
#define CANDIDATE_CALLBACK_BYTES 4096u
typedef struct {s16 left,top,right,bottom;} Rect;
typedef struct {u32 lcd,block;} Device;
typedef struct {const u16 *text;u16 length,reserved;} String;
typedef struct {u32 words[11];} Style;
typedef struct {u8 dates[20];u16 attr,pad;u32 length;u16 name[256];u8 shortname[13];} FindData;
_Static_assert(sizeof(FindData)==556,"Firmware SFS_FIND_DATA ABI");
extern void *os_alloc(u32,u32,const char *,u32);
extern void os_free(void *,const char *,u32);
extern void *get_state(u32);
extern u32 set_state(u32,void *);
extern void close_window(u32);
extern u32 send_message(u32,u32,void *);
extern u32 file_open(const u16 *,u32,u32,void *);
extern u32 file_read(u32,void *,u32,u32 *,void *);
extern u32 file_write(u32,const void *,u32,u32 *,void *);
extern u32 file_close(u32);
extern u32 find_first(const u16 *,FindData *);
extern u32 find_next(u32,FindData *);
extern u32 find_close(u32);
extern u8 timer_create(u32,u32,u32,const char *,u32);
extern u32 timer_stop(u8);
extern void set_calc_style(Style *,u32,u32,u16);
extern void draw_text(const Device *,const Rect *,const Rect *,const String *,Style *,u32,signed char);
extern void draw_hline(const Device *,s16,s16,s16,u16);
extern void sync_code(void *,u32);
extern u32 audio_find(const char *);
extern u32 audio_request(const char *,const void *,const void *);
extern u32 audio_free(u32);
extern u32 audio_route(u32,u32);
extern u32 audio_volume(u32,u32);
extern u32 audio_play(u32,u32);
#define APP_MAGIC 0x32414750u
#define CONTEXT_MAGIC 0x32445347u
#define SAVE_MAGIC 0x31534750u
#define MAX_GAMES 32u
#define MAX_SAVE 512u
#define WAVE_RATE 16000u
#define MAX_WAVE (44u+WAVE_RATE*2u*2u)
typedef struct {u32 v[10];char title[32];u16 icon[1600];} Package;
_Static_assert(sizeof(Package)==3272,"PGA2 metadata layout");
typedef struct {u16 folder[32];Package meta;} Item;
typedef struct {u32 magic,version,bytes,sequence,hash;u8 data[MAX_SAVE];} Save;
typedef struct {
 u32 magic,count,selected,paused;
 void *allocation;u8 *base;void *game_state;PgaEntry entry;
 u32 tick;u8 timer,drive;const char *error;
 PgaApi api;
 u8 order[MAX_GAMES];
 Save saves[2];
 Package check;
 FindData found;
 Item items[MAX_GAMES];
 u8 *wave;
 u32 audio_diag[12];
 u32 master_volume,volume_dirty,note_volume;
 PeaqPga5sRun large_run;PeaqProposedPga5Api large_api;PeaqProposedGameLocation large_location;
 PeaqResident *large_owner;PeaqResidentUse large_use;
 u32 large_mode,large_window,large_generation,large_closed,large_forced,large_dispatching,large_leased,large_redraw;
} Context;
static int candidate_start(Context *,u32);
static int candidate_code_valid(Context *,const u32 *);
static u32 legacy_window(u32,u32,void *);
static const char source[]="peaq_games.c";
static const char audio_client[]="PEAQ GAMES";
void __aeabi_memcpy8(void *d,const void *s,u32 n){u8 *a=d;const u8 *b=s;while(n--)*a++=*b++;}
static void zero(void *p,u32 n){u8 *b=p;while(n--)*b++=0;}
static void *allocate(u32 n){return os_alloc(n,0x44444444u,source,1);}
static void release(void *p){if(p)os_free(p,source,1);}
static u32 fnv(u32 h,const void *p,u32 n){const u8 *b=p;while(n--)h=(h^*b++)*16777619u;return h;}
static int same(const void *a,const void *b,u32 n){const u8 *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static int read_exact(u32 h,void *p,u32 n){
 u8 *b=p;while(n){u32 got=0,k=n>4096?4096:n;if(file_read(h,b,k,&got,0)||got!=k)return 0;b+=k;n-=k;}return 1;
}
static int at_end(u32 h){u8 b;u32 n=0;return file_read(h,&b,1,&n,0)==0 && n==0;}
static u32 open_file(Context *c,const u16 *folder,const char *name,u32 mode){
 u16 path[64]={'E',':','\\','G','a','m','e','s','\\'};u32 i=9,j=0;path[0]=c->drive;
 while(folder[j] && j<31)path[i++]=folder[j++];path[i++]='\\';
 for(j=0;name[j] && j<12;j++)path[i++]=(u8)name[j];path[i]=0;
 return file_open(path,mode,0,0);
}
static u32 volume_file(Context *c,u32 mode){
 u16 path[]={'E',':','\\','G','a','m','e','s','\\','v','o','l','u','m','e','.','c','f','g',0};
 path[0]=c->drive;return file_open(path,mode,0,0);
}
static void load_volume(Context *c){
 u32 v[4],f=volume_file(c,0x31);c->master_volume=4;
 if(!f)return;
 int ok=read_exact(f,v,sizeof(v))&&at_end(f)&&v[0]==0x314c4f56&&v[1]==1&&v[2]<=8&&fnv(2166136261u,v,12)==v[3];
 if(file_close(f))ok=0;if(ok)c->master_volume=v[2];
}
static void save_volume(Context *c){
 if(!c->volume_dirty)return;
 u32 v[4]={0x314c4f56,1,c->master_volume,0},got=0;
 v[3]=fnv(2166136261u,v,12);u32 f=volume_file(c,0x22);if(!f)return;
 int ok=file_write(f,v,sizeof(v),&got,0)==0&&got==sizeof(v);if(file_close(f))ok=0;
 if(ok)c->volume_dirty=0;
}
static u32 effective_volume(Context *c,u32 note){u32 v=(note*c->master_volume+2)/4;return v>8?8:v;}
static Item *selected(Context *c){return c->items+c->order[c->selected];}
static int valid_meta(const Package *p){
 const u32 *v=p->v;
 if(v[0]!=APP_MAGIC || (v[1]!=2&&v[1]!=3&&v[1]!=4&&v[1]!=5) || (v[1]==5?((v[9]&3)||v[9]>v[2]||v[2]-v[9]<40):v[9]) || v[2]<2 || v[2]>65536 || v[3]<v[2] || v[3]>131072 ||
    !(v[4]&1) || (v[4]&~1u)>=v[2] || v[5]>32768 || (v[6]&&(v[6]<(v[1]==5?5u:50u)||v[6]>1000)) || v[7]>512)return 0;
 u32 n=0;while(n<32 && p->title[n]){if((u8)p->title[n]<32||(u8)p->title[n]>126)return 0;n++;}
 return n>0 && n<=24;
}
static int valid_folder(const u16 *p){
 u32 n=0;while(n<32 && p[n]){if(p[n]<32||p[n]=='/'||p[n]=='\\'||p[n]==':'||p[n]=='*'||p[n]=='?')return 0;n++;}
 return n>0 && n<=31 && p[0]!='.';
}
static int compare(const char *a,const char *b){while(*a && *a==*b){a++;b++;}return (u8)*a-(u8)*b;}
static void scan(Context *c){
 u16 path[]={'E',':','\\','G','a','m','e','s','\\','*',0};u32 search=0;
 c->count=0;c->selected=0;c->error=0;
 for(u32 k=0;k<2;k++){c->drive=path[0]=k?'D':'E';search=find_first(path,&c->found);if(search)break;}
 if(!search){c->error="SD/Games nicht gefunden";return;}
 for(u32 checked=0;checked<256;checked++){
  if((c->found.attr&0x10) && valid_folder(c->found.name)){
   Item *p=c->items+c->count;zero(p,sizeof(*p));
   for(u32 j=0;j<31 && c->found.name[j];j++)p->folder[j]=c->found.name[j];
   u32 f=open_file(c,p->folder,"game.pga",0x31);
   if(f){int ok=read_exact(f,&p->meta,sizeof(Package)) && valid_meta(&p->meta);if(file_close(f))ok=0;
    if(ok){c->order[c->count]=(u8)c->count;c->count++;if(c->count==MAX_GAMES){c->error="Maximal 32 Spiele";break;}}}
  }
  if(find_next(search,&c->found))break;
  if(checked==255)c->error="Zu viele SD-Ordner";
 }
 if(find_close(search))c->error="SD-Lesefehler";
 for(u32 i=1;i<c->count;i++)for(u32 j=i;j && compare(c->items[c->order[j-1]].meta.title,c->items[c->order[j]].meta.title)>0;j--){
  u8 t=c->order[j];c->order[j]=c->order[j-1];c->order[j-1]=t;
 }
}
static const char *save_name(u32 slot){return slot?"save1.dat":"save0.dat";}
static int read_save(Context *c,u32 slot){
 Save *s=c->saves+slot;s->magic=0;
 u32 h=open_file(c,selected(c)->folder,save_name(slot),0x31);if(!h)return 0;
 int ok=read_exact(h,s,20) && s->magic==SAVE_MAGIC && s->version==1 && s->bytes>0 && s->bytes<=MAX_SAVE;
 if(ok)ok=read_exact(h,s->data,s->bytes) && at_end(h) && fnv(fnv(2166136261u,s,16),s->data,s->bytes)==s->hash;
 if(file_close(h))ok=0;if(!ok)s->magic=0;return ok;
}
static int latest(Context *c){
 int a=read_save(c,0),b=read_save(c,1);
 if(!a)return b?1:-1;if(!b)return 0;
 return (int)(c->saves[1].sequence-c->saves[0].sequence)>0?1:0;
}
static int api_load(void *cookie,void *data,u32 n){
 Context *c=cookie;if(!data || !n || n>MAX_SAVE || !c->entry)return 0;
 int slot=latest(c);if(slot<0 || c->saves[slot].bytes!=n)return 0;
 __aeabi_memcpy8(data,c->saves[slot].data,n);return 1;
}
static int api_save(void *cookie,const void *data,u32 n){
 Context *c=cookie;if(!data || !n || n>MAX_SAVE || !c->entry)return 0;
 int old=latest(c);u32 slot=old<0?0:1-(u32)old,seq=old<0?1:c->saves[old].sequence+1;
 Save *s=c->saves+slot;s->magic=SAVE_MAGIC;s->version=1;s->bytes=n;s->sequence=seq;
 __aeabi_memcpy8(s->data,data,n);s->hash=fnv(fnv(2166136261u,s,16),s->data,n);
 u32 h=open_file(c,selected(c)->folder,save_name(slot),0x22),got=0;if(!h)return 0;
 int ok=file_write(h,s,n+20,&got,0)==0 && got==n+20;if(file_close(h))ok=0;
 if(ok)ok=read_save(c,slot) && s->sequence==seq && s->bytes==n && same(s->data,data,n);
 return ok;
}
static void api_silence(void *cookie){
 Context *c=cookie;if(!c||c->magic!=CONTEXT_MAGIC)return;
 /* Look up only our named service; a completed tone may already be freed. */
 u32 handle=audio_find(audio_client);if(handle)audio_free(handle);
}
static void write_audio_diag(Context *c){
 if(!c->entry||!c->audio_diag[1])return;
 u32 f=open_file(c,selected(c)->folder,"audio.chk",0x22),written=0;
 if(f){file_write(f,c->audio_diag,sizeof(c->audio_diag),&written,0);file_close(f);}
}
static u32 make_wave(u8 *wave,u32 hz,u32 ms){
 u32 n=ms*16,bytes=n*2,phase=0;
 /* PCM WAV, signed 16-bit mono at 16 kHz; owned until the service is freed. */
 const u32 header[11]={0x46464952,bytes+36,0x45564157,0x20746d66,16,
                      0x00010001,WAVE_RATE,WAVE_RATE*2,0x00100002,0x61746164,bytes};
 __aeabi_memcpy8(wave,header,44);
 s16 *pcm=(s16 *)(wave+44);
 static const s16 sine[65]={0,1176,2341,3483,4592,5657,6667,7613,8485,9276,9978,10583,11087,11483,11769,11942,
 12000,11942,11769,11483,11087,10583,9978,9276,8485,7613,6667,5657,4592,3483,2341,1176,
 0,-1176,-2341,-3483,-4592,-5657,-6667,-7613,-8485,-9276,-9978,-10583,-11087,-11483,-11769,-11942,
 -12000,-11942,-11769,-11483,-11087,-10583,-9978,-9276,-8485,-7613,-6667,-5657,-4592,-3483,-2341,-1176,0};
 u32 attack=n/4<128?n/4:128,release=n/2<800?n/2:800;
 for(u32 i=0;i<n;i++){
  /* Interpolated sine with 8 ms attack / up to 50 ms release. */
  u32 p=phase*64,index=p/WAVE_RATE,fraction=p%WAVE_RATE;
  int sample=sine[index]+(sine[index+1]-sine[index])*(int)fraction/(int)WAVE_RATE;
  if(i<attack)sample=sample*(int)i/(int)attack;
  if(n-1-i<release)sample=sample*(int)(n-1-i)/(int)release;
  pcm[i]=(s16)sample;
  phase+=hz;if(phase>=WAVE_RATE)phase-=WAVE_RATE;
 }
 return bytes+44;
}
static int api_tone(void *cookie,u32 hz,u32 ms,u32 volume){
 Context *c=cookie;
 if(!c||c->magic!=CONTEXT_MAGIC||!c->entry||c->paused)return 0;
 if(hz<80||hz>4000||ms<20||ms>2000||volume>8)return 0;
 api_silence(c);
 c->note_volume=volume;volume=effective_volume(c,volume);
 c->audio_diag[0]=0x34445541;c->audio_diag[1]++;
 c->audio_diag[2]=hz;c->audio_diag[3]=ms;c->audio_diag[4]=volume;
 for(u32 i=5;i<11;i++)c->audio_diag[i]=0;
 if(!volume)return 1;
 if(!c->wave)c->wave=allocate(MAX_WAVE);
 if(!c->wave){c->audio_diag[5]=1;return 0;}
 u32 length=make_wave(c->wave,hz,ms);
 /* Use the native media buffer path, not the accepted-but-inaudible legacy
  * custom-tone path on this handset. Type 2 / format 5 = PCM WAV buffer.
  * The stock 100-byte union and 232-byte object layout remain unchanged. */
 u32 req[5],audio[58];zero(req,sizeof(req));zero(audio,sizeof(audio));
 req[0]=190;req[1]=1;req[3]=(u32)audio_client;
 audio[0]=2;audio[1]=5;audio[2]=(u32)c->wave;audio[3]=length;
 ((u16 *)audio)[0xc8/2]=8;((u16 *)audio)[0xca/2]=8;
 audio[0xcc/4]=volume;((u16 *)audio)[0xe0/2]=1;
 u32 handle=audio_request("audio service",req,audio);c->audio_diag[6]=handle;
 if(!handle){c->audio_diag[5]=2;return 0;}
 c->audio_diag[7]=audio_route(handle,8);
 c->audio_diag[8]=audio_volume(handle,volume);
 c->audio_diag[9]=audio_play(handle,0);
 if(!c->audio_diag[7]||!c->audio_diag[8]||c->audio_diag[9]!=1){
  c->audio_diag[5]=3;audio_free(handle);return 0;}
 c->audio_diag[10]=length;
 return 1;
}
static void api_fill(int x,int y,int w,int h,u16 color){
 Device d={0,0};
 if(w<=0||h<=0||x>=240||y>=320||x< -240||y< -320||w>480||h>640)return;
 int right=x+w-1,bottom=y+h-1;if(x<0)x=0;if(y<0)y=0;if(right>239)right=239;if(bottom>319)bottom=319;
 if(right<x||bottom<y)return;
 for(int row=y;row<=bottom;row++)draw_hline(&d,(s16)x,(s16)row,(s16)right,color);
}
static void api_text(int x,int y,u16 color,const char *s){
 if(!s||x<0||y<0||x>=240||y>280)return;u16 text[32];u32 n=0;
 while(s[n] && n<31){text[n]=(u8)s[n];n++;}
 Device d={0,0};Rect r={(s16)x,(s16)y,239,(s16)(y+39)};Style style;String str={text,(u16)n,0};
 zero(&style,sizeof(style));set_calc_style(&style,6,28,color);draw_text(&d,&r,&r,&str,&style,0x10,-1);
}
static void stop_timer(Context *c){if(c->timer){timer_stop(c->timer);c->timer=0;}}
static void unload(Context *c){
 stop_timer(c);
 if(c->entry){c->entry(&c->api,c->game_state,PGA_CLOSE,0);write_audio_diag(c);c->entry=0;}
 api_silence(c);
 release(c->wave);c->wave=0;
 release(c->allocation);c->allocation=0;c->base=0;c->game_state=0;c->tick=0;
 save_volume(c);
}
static int load(Context *c,u32 window){
 u32 h=open_file(c,selected(c)->folder,"game.pga",0x31),sum=2166136261u,*relocs=0,*v=c->check.v;int ok=0;
 c->error="Spiel fehlt";if(!h)return 0;c->error="Spiel ungueltig";
 if(!read_exact(h,&c->check,sizeof(Package)) || !valid_meta(&c->check))goto done;
 c->allocation=allocate(((v[3]+7)&~7u)+v[5]+v[7]*4+40);
 if(!c->allocation){c->error="Speicher voll";goto done;}
 c->base=(u8 *)(((u32)c->allocation+31)&~31u);c->game_state=c->base+((v[3]+7)&~7u);
 relocs=(u32 *)(((u32)c->game_state+v[5]+3)&~3u);zero(c->base,((v[3]+7)&~7u)+v[5]);
 if(!read_exact(h,c->base,v[2]) || !read_exact(h,relocs,v[7]*4) || !at_end(h))goto done;
 sum=fnv(sum,v,32);sum=fnv(sum,v+9,sizeof(Package)-36);sum=fnv(sum,c->base,v[2]);sum=fnv(sum,relocs,v[7]*4);
 if(sum!=v[8]){c->error="Pruefsumme falsch";goto done;}
 for(u32 i=0;i<v[7];i++){u32 r=relocs[i];if((r&3)||v[2]<4||r>v[2]-4||(i&&r<=relocs[i-1])||*(u32 *)(c->base+r)>=v[3])goto done;}
 if(!candidate_code_valid(c,relocs)||c->large_closed||(v[1]==5)!=!!c->large_mode)goto done;
 for(u32 i=0;i<v[7];i++)*(u32 *)(c->base+relocs[i])+=(u32)c->base;ok=1;
 done:
 if(file_close(h)||c->large_closed)ok=0;
 if(!ok){release(c->allocation);c->allocation=0;c->base=0;c->game_state=0;return 0;}
 sync_code(c->base,v[3]);if(v[1]==5)return candidate_start(c,window);c->entry=(PgaEntry)((u32)c->base+v[4]);c->tick=v[6];c->error=0;
 zero(c->audio_diag,sizeof(c->audio_diag));
 if(c->entry(&c->api,c->game_state,PGA_INIT,0)&PGA_QUIT){unload(c);c->error="Start fehlgeschlagen";return 0;}
 if(c->tick&&!c->paused){c->timer=timer_create(window,c->tick,1,source,1);if(!c->timer){unload(c);c->error="Kein Timer frei";return 0;}}
 return 1;
}
static void paint(Context *c){
 if(c && c->entry){c->entry(&c->api,c->game_state,PGA_PAINT,0);return;}
 PgaApi fallback={4,240,320,api_fill,api_text,0,0,0,0,0,0};const PgaApi *a=c?&c->api:&fallback;
 banner(a,"GAMES",0x4e9c);gt(a,12,38,1,0xa556,"SPIELE VON DEINER SD-KARTE");
 if(!c){gt(a,12,110,1,0xffff,"SPEICHER VOLL");return;}
 u32 first=(c->selected/3)*3;
 for(u32 i=first;i<c->count && i<first+3;i++){
  Item *item=c->items+c->order[i];int y=58+(int)(i-first)*66;
  api_fill(6,y,228,60,i==c->selected?0x2251:0x10a4);
  if(i==c->selected)api_fill(6,y,3,60,0x4e9c);
  for(int row=0;row<40;row++)for(int col=0;col<40;){
   int end=col+1;u16 color=item->meta.icon[row*40+col];while(end<40&&item->meta.icon[row*40+end]==color)end++;
   api_fill(16+col,y+10+row,end-col,1,color);col=end;
  }
  char line[14];u32 j=0;for(;j<13&&item->meta.title[j];j++)line[j]=item->meta.title[j];line[j]=0;
  gt(a,68,y+12,2,0xffff,line);if(j==13 && item->meta.title[j])gt(a,68,y+32,1,0xc658,item->meta.title+j);
 }
 if(!c->count){gt(a,12,90,2,0xffff,"KEINE SPIELE");gt(a,12,122,1,0xc658,"SD: Games/Ordner/game.pga");}
 if(c->error)gt(a,10,262,1,0xfd45,c->error);
 else {gn(a,12,262,1,0xa556,c->count);gt(a,32,262,1,0xa556,"SPIELE");}
 gsoftkeys(a,"OK","ZURUECK");
}
static int key(u32 m){
 u32 k=m&255;if(k>=0x08&&k<=0x10)return (int)k-7;
 switch(k){case 1:return PGA_UP;case 2:return PGA_DOWN;case 3:return PGA_LEFT;case 4:return PGA_RIGHT;
 /* KEY_WEB is the centre confirm key used by this firmware's calculator;
  * KEY_OK is the left softkey, KEY_MIDDLE is an alternate centre code. */
 case 0x16:case 0x1b:case 0x26:return PGA_OK;case 0x11:return PGA_STAR;case 0x12:return 0;case 0x13:return PGA_HASH;default:return -1;}
}
static u32 legacy_window(u32 window,u32 message,void *parameter){
 Context *c=get_state(window);if(c&&c->magic!=CONTEXT_MAGIC)c=0;
 if(c&&((message&0xff00)==0xfa00))c->audio_diag[11]=message;
 if(message==0xf021){
  if(!c){c=allocate(sizeof(*c));if(c){zero(c,sizeof(*c));c->magic=CONTEXT_MAGIC;
   c->api=(PgaApi){4,240,320,api_fill,api_text,c,api_load,api_save,PGA_CAP_TONE|PGA_CAP_BACK|PGA_CAP_RAW_KEYS,api_tone,api_silence};if(!set_state(window,c)){release(c);c=0;}}}
  if(c){scan(c);load_volume(c);}return 1;
 }
 if(message==0xf022){if(c){set_state(window,0);unload(c);c->magic=0;release(c);}return 1;}
 /* Keep delivery inside the resident window: no module-owned child handler
  * survives unloading. ABI 2/3 continue to receive their existing actions. */
 u32 raw=message&255,kind=message&0xff00;
 if(c&&c->entry&&c->check.v[1]>=4 && kind>=0xfa00&&kind<=0xfe00 &&
    ((raw>=1&&raw<=4)||(raw>=8&&raw<=0x13)||raw==0x16||raw==0x17||raw==0x1b||raw==0x26)){
  if(c->paused)return 1;
  u32 result=c->entry(&c->api,c->game_state,PGA_RAW_KEY,message);
  if(result&PGA_QUIT){unload(c);result|=PGA_REDRAW;}
  if(result&PGA_REDRAW)send_message(window,0xf025,0);return 1;
 }
 if(message==0xfa17 || message==0xfe17)return 1;
 if(message==0xfb17){
  if(c&&c->paused)return 1;
  if(c&&c->entry){
   if(c->check.v[1]>=3){u32 r=c->entry(&c->api,c->game_state,PGA_KEY,PGA_BACK);if(r&PGA_QUIT)unload(c);}
   else unload(c);
   send_message(window,0xf025,0);
  }else close_window(window);return 1;
 }
 if(message==0xf025){paint(c);return 1;}if(!c)return 0;
 if(message==0xf023){c->paused=1;stop_timer(c);if(c->entry)c->entry(&c->api,c->game_state,PGA_PAUSE,0);api_silence(c);return 1;}
 if(message==0xf024){c->paused=0;if(c->entry){c->entry(&c->api,c->game_state,PGA_RESUME,0);
  if(c->tick&&!c->timer){c->timer=timer_create(window,c->tick,1,source,1);if(!c->timer){unload(c);c->error="Kein Timer frei";}}}return 1;}
 if(c->entry&&(raw==0x19||raw==0x1a||raw==0x61||raw==0x62)&&kind>=0xfa00&&kind<=0xfe00){
  if(!c->paused&&(kind==0xfa00||kind==0xfe00)){
   if(raw==0x19||raw==0x61){if(c->master_volume<8)c->master_volume++;}
   else if(c->master_volume)c->master_volume--;
   c->volume_dirty=1;u32 volume=effective_volume(c,c->note_volume),h=audio_find(audio_client);
   c->audio_diag[4]=volume;
   if(!volume)api_silence(c);else if(h)c->audio_diag[8]=audio_volume(h,volume);
  }return 1;
 }
 u32 result=0;
 if(message==0xf020){
  if(c->entry&&!c->paused&&c->timer&&parameter&&*(u8 *)parameter==c->timer)result=c->entry(&c->api,c->game_state,PGA_TICK,c->tick);else return 1;
 }else if((message&0xff00)==0xfa00 || (message&0xff00)==0xfe00){
  int k=key(message);if(k<0)return 0;
  if((message&0xff00)==0xfe00 && !(k==PGA_UP||k==PGA_DOWN||k==PGA_LEFT||k==PGA_RIGHT||k==2||k==4||k==6||k==8))return 1;
  if(c->paused)return 1;
  if(c->entry)result=c->entry(&c->api,c->game_state,PGA_KEY,(u32)k);
  else{
   c->error=0;
   if(k==PGA_UP||k==2){if(c->selected)c->selected--;}
   else if(k==PGA_DOWN||k==8){if(c->selected+1<c->count)c->selected++;}
   else if(k==PGA_HASH)scan(c);
   else if(k==PGA_OK||k==5){if(c->count)load(c,window);else scan(c);}
   result=PGA_REDRAW;
  }
 }else return 0;
 if(result&PGA_QUIT){unload(c);result|=PGA_REDRAW;}if(result&PGA_REDRAW)send_message(window,0xf025,0);return 1;
}
/* Keep the existing handler address stable for the bounded launcher patch. */
void __aeabi_memcpy(void *d,const void *s,u32 n){__aeabi_memcpy8(d,s,n);}

#define sd_window candidate_unreaped_window
#include "candidate_glue.inc"
#undef sd_window
#include "reaper_window.inc"
