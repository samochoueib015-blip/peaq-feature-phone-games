extern void peaq_emerald_init(void);
extern void peaq_emerald_step(void);
extern void peaq_emerald_frames(void);
extern void peaq_emerald_save_changes(void);
extern void peaq_emerald_save_memory(void);
const void *const peaq_emerald_exports[]={(const void *)peaq_emerald_init,(const void *)peaq_emerald_step,(const void *)peaq_emerald_frames,(const void *)peaq_emerald_save_changes,(const void *)peaq_emerald_save_memory};
