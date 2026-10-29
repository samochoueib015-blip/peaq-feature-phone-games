/* Included ONLY by the generated offline launcher candidate. The production
 * loader and SDK stay untouched. All callbacks live in permanent User code;
 * Context, API, state and SD code remain pinned until the resident gate. */
static void candidate_detach(void *cookie,u32 generation){
 Context *c=cookie;(void)generation;c->large_closed=1;
 /* Remove reachability before a native stop can reenter the window. */
 if(get_state(c->large_window)==c)set_state(c->large_window,0);
 u8 timer=c->timer;c->timer=0;if(timer)timer_stop(timer);
}
static void candidate_release(void *cookie){
 Context *c=cookie;
 /* ABI5 advertises no legacy tone/save service. No shared audio operation or
  * file write is allowed here: a new view might already own those services. */
 release(c->allocation);c->magic=0;release(c);
}
static void *candidate_allocate(void *cookie,u32 bytes){return peaq_pga5s_native_allocate(cookie,bytes);}
static void candidate_free(void *cookie,void *p){peaq_pga5s_native_deallocate(cookie,p);}
static void candidate_close(Context *c){
 if(!peaq_pga5s_close(&c->large_run))candidate_detach(c,0);
}
static int candidate_code_valid(Context *c,const u32 *relocs){
 if(c->check.v[1]!=5)return 1;
 PeaqPga5sPlan plan;if(peaq_pga5s_validate(c->check.v,c->base,&plan)!=1)return 0;
 u32 descriptor=c->check.v[9],table=plan.descriptor.table_offset,table_bytes=plan.descriptor.segment_count*16;
 for(u32 i=0;i<c->check.v[7];i++){
  u32 p=relocs[i];
  if((p<descriptor+40 && descriptor<p+4)||(p<table+table_bytes && table<p+4))return 0;
 }
 return 1;
}
static int candidate_start(Context *c,u32 window){
 c->large_window=window;c->large_owner=peaq_launcher_anchor_owner(0);
 if(!c->large_owner||c->large_closed)goto failed;
 PeaqResidentStatus status;peaq_resident_status(c->large_owner,&status);
 if(status.state||status.busy){c->error="Vorheriges Spiel beendet noch";goto failed;}
 c->large_api.legacy=c->api;c->large_api.legacy.version=5;
 c->large_api.legacy.load=0;c->large_api.legacy.save=0;
 c->large_api.legacy.tone=0;c->large_api.legacy.silence=0;
 c->large_api.legacy.capabilities=PGA_CAP_BACK|PGA_CAP_RAW_KEYS;
 c->large_api.bytes=sizeof(c->large_api);c->large_api.extension_version=1;
 c->large_api.location=&c->large_location;
 c->large_location.bytes=sizeof(c->large_location);c->large_location.version=1;c->large_location.drive=c->drive;
 for(u32 i=0;i<32;i++)c->large_location.folder[i]=selected(c)->folder[i];
 PeaqPga5sBinding binding={c->check.v,c->base,c->game_state,&c->large_api.legacy,c,sizeof(*c),
  CANDIDATE_CALLBACK_BASE,CANDIDATE_CALLBACK_BYTES,candidate_allocate,candidate_free,candidate_detach,candidate_release};
 c->large_generation=peaq_pga5s_prepare(&c->large_run,c->large_owner,&binding);
 if(!c->large_generation)goto failed;
 /* This lease also covers caller postwork, including synchronous paint. */
 if(!peaq_pga5s_enter(&c->large_run,&c->large_use)){candidate_close(c);return 0;}
 c->large_leased=1;c->tick=c->check.v[6];c->error=0;
 if(!peaq_pga5s_start(&c->large_run,&c->large_use))return 0;
 if(c->tick&&!c->paused){
  c->timer=timer_create(window,c->tick,1,source,1);
  if(!c->timer||c->large_closed){candidate_detach(c,c->large_generation);candidate_close(c);return 0;}
 }
 c->large_redraw=1;return 1;
failed:
 if(!c->error)c->error="Grosser Spielstart fehlgeschlagen";
 release(c->allocation);c->allocation=0;c->base=0;c->game_state=0;return 0;
}
static u32 candidate_dispatch(Context *c,u32 event,u32 value){
 u32 result=peaq_pga5s_event(&c->large_run,&c->large_use,event,value);
 if(result&PGA_REDRAW)c->large_redraw=1;return result;
}
static void candidate_finish_dispatch(Context *c){
 if(c->large_leased){
  if(c->large_redraw&&!c->large_closed){
   c->large_redraw=0;candidate_dispatch(c,PGA_PAINT,0);
  }
  if(c->large_closed&&!c->large_forced&&!get_state(c->large_window)){
   /* The list owns a NEW Context. The old API/storage must not be reused. */
   legacy_window(c->large_window,0xf021,0);send_message(c->large_window,0xf025,0);
  }
  c->large_dispatching=0;c->large_leased=0;
  /* MUST be the last access to the old Context by this dispatch. The leave
   * may deliver deferred close; a later owner poll alone may release it. */
  peaq_pga5s_leave(&c->large_run,&c->large_use);return;
 }
 c->large_dispatching=0;
 if(!c->large_generation){
  if(c->large_closed){candidate_release(c);return;}
  c->large_mode=0;zero(&c->large_run,sizeof(c->large_run));
 }
}
static u32 candidate_window(Context *c,u32 message,void *parameter){
 if(message==0xf022){c->large_forced=1;candidate_close(c);return 1;}
 if(c->large_dispatching){if(message==0xf025)c->large_redraw=1;return 1;}
 if(message==0xf021)return 1; /* Never rescan/mutate the pinned selection. */
 if(!c->large_generation)return 1;
 if(message==0xf020){
  if(!c->timer||!parameter||*(u8 *)parameter!=c->timer)return 1;
  PeaqResident *owner=c->large_owner;u32 window=c->large_window;
  int status=peaq_resident_poll(owner,peaq_mocor_milliseconds());
  /* Poll can reenter CLOSE and release Context. Reacquire native state before
   * any further access; pointer comparison itself does not dereference it. */
  if(get_state(window)!=c)return 1;
  if(status==PRG_COMPLETE_ATTACHED){
   candidate_close(c);legacy_window(window,0xf021,0);send_message(window,0xf025,0);return 1;
  }
 }
 if(!peaq_pga5s_enter(&c->large_run,&c->large_use))return 1;
 c->large_dispatching=1;c->large_leased=1;u32 handled=1,kind=message&0xff00,raw=message&255;
 if(message==0xf025)c->large_redraw=1;
 else if(message==0xf023){c->paused=1;candidate_dispatch(c,PGA_PAUSE,0);}
 else if(message==0xf024){c->paused=0;candidate_dispatch(c,PGA_RESUME,0);}
 else if(message==0xf020){if(!c->paused)candidate_dispatch(c,PGA_TICK,c->tick);}
 else if(kind>=0xfa00&&kind<=0xfe00 &&
  ((raw>=1&&raw<=4)||(raw>=8&&raw<=0x13)||raw==0x16||raw==0x17||raw==0x1b||raw==0x26)){
  if(!c->paused)candidate_dispatch(c,PGA_RAW_KEY,message);
 }else handled=0;
 candidate_finish_dispatch(c);return handled;
}
u32 sd_window(u32 window,u32 message,void *parameter){
 Context *c=get_state(window);if(c&&c->magic!=CONTEXT_MAGIC)c=0;
 if(c&&c->large_mode)return candidate_window(c,message,parameter);
 if(message==0xf021){
  /* First OPEN creates one permanent160-byte anchor. An unfinished instance
   * never prevents a fresh independent list; another large claim is refused. */
  PeaqResident *owner=peaq_launcher_anchor_owner(0);if(owner)peaq_resident_poll(owner,peaq_mocor_milliseconds());
  c=get_state(window);if(c&&c->magic!=CONTEXT_MAGIC)c=0;
 }
 if(c&&!c->entry&&!c->paused&&c->count&&(message==0xfa26||message==0xfa1b||message==0xfa16||message==0xfa0c) &&
    selected(c)->meta.v[1]==5){
  c->large_mode=1;c->large_dispatching=1;c->large_window=window;
  u32 result=legacy_window(window,message,parameter);
  candidate_finish_dispatch(c);return result;
 }
 return legacy_window(window,message,parameter);
}
