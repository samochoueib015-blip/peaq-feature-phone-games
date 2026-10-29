/* Included after the frozen candidate glue with its sd_window renamed.
 * Only the current list owns this timer; no callback points into SD code or
 * a detached Context. Closing the list stops its timer through legacy Close.
 * The pinned old game remains owned until its exact join/drain gate passes. */
__attribute__((noinline)) static Context *candidate_list(u32 window){
 Context *c=get_state(window);
 return c&&c->magic==CONTEXT_MAGIC&&!c->large_mode&&!c->entry&&!c->paused?c:0;
}
__attribute__((noinline)) static void candidate_reap_list(u32 window){
 Context *c=candidate_list(window);if(!c)return;
 PeaqResident *owner=peaq_launcher_anchor_owner(0);if(!owner)return;
 int done=peaq_resident_poll(owner,peaq_mocor_milliseconds());
 /* Native I/O may reenter CLOSE. Never retain the current list pointer. */
 c=candidate_list(window);if(!c)return;
 if(done==PRG_REAPED)stop_timer(c);
 else if(!c->timer){
  u8 timer=timer_create(window,50,1,source,1);
  /* Timer creation can also reenter CLOSE or open a different view. */
  c=candidate_list(window);
  if(c&&!c->timer)c->timer=timer;
  else if(timer)timer_stop(timer);
 }
}
u32 sd_window(u32 window,u32 message,void *parameter){
 Context *c=candidate_list(window);
 /* A list timer must not be overwritten by a newly launched game's timer. */
 if(c&&(message&0xff00)==0xfa00)stop_timer(c);
 u32 result=candidate_unreaped_window(window,message,parameter);
 candidate_reap_list(window);return result;
}
