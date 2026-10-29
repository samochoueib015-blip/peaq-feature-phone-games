#include "guard.h"
#include <stdlib.h>

extern void doomgeneric_Create(int,char **);
extern void doomgeneric_Tick(void);
extern void __real_I_Quit(void);

/* Cortex-A7 soft-float AAPCS: r4-r11, sp, lr. This local context boundary
 * avoids importing an unrelated C++ unwinder through Newlib's ARM exidx.
 * No VFP state is used by the soft-float engine or its Thumb-1 libc. */
typedef struct {uint32_t registers[10];} ExitTarget;
extern int peaq_doom_save_context(ExitTarget *) __attribute__((returns_twice));
extern void peaq_doom_restore_context(const ExitTarget *) __attribute__((noreturn));
static ExitTarget exit_target __attribute__((aligned(8)));
static unsigned active,started,stopped;
static int exit_status;

uint32_t peaq_doom_runtime_abi(void){return 1;}
int peaq_doom_exit_status(void){return exit_status;}

/* The pinned generic I_Quit runs its shutdown hooks but excludes exit(0)
 * under !ORIGCODE. Complete that boundary without changing game source. */
void __wrap_I_Quit(void){__real_I_Quit();exit(0);}

__attribute__((noreturn)) void peaq_doom_unwind(int status){
    exit_status=status;stopped=1;
    /* An unguarded engine entry is a host programming error. Never jump to
     * an expired checkpoint. Only the managed entries are exported. */
    if(!active)__builtin_trap();
    peaq_doom_restore_context(&exit_target);
}

static int run(unsigned operation,int argc,char **argv){
    if(active||stopped||(operation==0?started:!started))return PEAQ_DOOM_BAD_STATE;
    if(operation==0&&(argc<1||argc>64||!argv))return PEAQ_DOOM_BAD_STATE;
    if(peaq_doom_save_context(&exit_target)){
        active=0;
        return exit_status?PEAQ_DOOM_FAILED:PEAQ_DOOM_EXITED;
    }
    active=1;
    if(operation==0){started=1;doomgeneric_Create(argc,argv);}
    else if(operation==1)doomgeneric_Tick();
    else __wrap_I_Quit();
    active=0;
    return PEAQ_DOOM_RUNNING;
}
int peaq_doom_create(int argc,char **argv){return run(0,argc,argv);}
int peaq_doom_tick(void){return run(1,0,0);}
int peaq_doom_quit(void){return run(2,0,0);}
