#include "resident_tx_join.h"
#include <stddef.h>

int peaq_resident_tx_join_init(PeaqResidentTxJoin *j,const PeaqResidentTxApi *api,void *tcb,const char *name){
 if(!j||!api||!api->info||!api->delete_thread||!tcb||((uintptr_t)tcb&3)||!name)return 0;
 if(j->tcb&&!j->joined)return 0;
 *j=(PeaqResidentTxJoin){tcb,name,*api,0,0,0,0};return 1;
}
int peaq_resident_tx_join_poll(void *cookie,uint32_t handle){
 PeaqResidentTxJoin *j=cookie;
 if(!j||!j->tcb||!j->name||!j->api.info||!j->api.delete_thread)return -1;
 if(handle!=(uint32_t)(uintptr_t)j->tcb){j->error=PRJ_ARGUMENT;return -1;}
 if(j->joined)return 1;
 char *name=0;uint32_t state=UINT32_MAX,runs=0,priority=0,threshold=0,slice=0;
 void *next=0,*suspended=0;
 uint32_t result=j->api.info(j->tcb,&name,&state,&runs,&priority,&threshold,&slice,&next,&suspended);
 j->native_status=result;j->state=state;
 if(result){j->error=PRJ_INFO;return -1;}
 if(name!=j->name){j->error=PRJ_IDENTITY;return -1;}
 if(state==PEAQ_THREAD_TERMINATED){j->error=PRJ_TERMINATED;return -1;}
 if(state!=PEAQ_THREAD_COMPLETED){j->error=PRJ_OK;return 0;}
 result=j->api.delete_thread(j->tcb);j->native_status=result;
 if(result){j->error=PRJ_DELETE;return -1;}
 j->joined=1;j->error=PRJ_OK;return 1;
}
