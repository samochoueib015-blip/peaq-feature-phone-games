#include "data_cache.h"

volatile PeaqDcState peaq_dc;
static int bad(unsigned e){if(!peaq_dc.error)peaq_dc.error=e;return 0;}
static int power2(uint32_t n){return n&&!(n&(n-1u));}
int peaq_dc_decode_level(uint32_t v,PeaqDcLevel *out){
 PeaqDcLevel x={v,(v&7u)+4u,((v>>13)&32767u)+1u,((v>>3)&1023u)+1u,0,0};
 if(!out||(v&0x70000000u)!=0x70000000u||x.line_shift!=6u||!power2(x.ways)||x.ways>16u||
    !power2(x.sets)||x.sets>2048u)return 0;
 x.way_shift=x.ways==1u?0u:(uint32_t)__builtin_clz(x.ways-1u);
 x.bytes=x.sets*x.ways*64u;
 if(x.bytes>2u*1024u*1024u)return 0;
 *out=x;return 1;
}
static int same_translation(void){
 return peaq_dc_r_ttbcr()==peaq_dc.ttbcr&&peaq_dc_r_ttbr0()==peaq_dc.ttbr0&&
  peaq_dc_r_ttbr1()==peaq_dc.ttbr1&&peaq_dc_r_dacr()==peaq_dc.dacr&&
  peaq_dc_r_csselr()==peaq_dc.csselr;
}
int peaq_dc_prepare(void){
 if(peaq_dc.snapshot||peaq_dc.prepared||peaq_dc.error)return bad(DC_ORDER);
 peaq_dc.sctlr=peaq_dc_r_sctlr();peaq_dc.actlr=peaq_dc_r_actlr();
 peaq_dc.cpsr=peaq_dc_r_cpsr();peaq_dc.midr=peaq_dc_r_midr();
 peaq_dc.ctr=peaq_dc_r_ctr();peaq_dc.clidr=peaq_dc_r_clidr();
 if(peaq_dc.midr!=0x410fc075u||peaq_dc.ctr!=0x84448003u||peaq_dc.clidr!=0x0a200023u||
    (peaq_dc.cpsr&255u)!=0xdfu||peaq_dc.actlr!=0x6040u||
    (peaq_dc.sctlr&(0x1005u|0x72000000u)))return bad(DC_CONTEXT);
 peaq_dc.ttbcr=peaq_dc_r_ttbcr();peaq_dc.ttbr0=peaq_dc_r_ttbr0();
 peaq_dc.ttbr1=peaq_dc_r_ttbr1();peaq_dc.dacr=peaq_dc_r_dacr();
 peaq_dc.csselr=peaq_dc_r_csselr();
 if(peaq_dc.ttbcr)return bad(DC_CONTEXT); /* refuse LPAE/PD/N settings */
 peaq_dc.snapshot=1; /* before the FIRST control write */
 for(unsigned i=0;i<PEAQ_DC_LEVELS;++i){
  peaq_dc_w_csselr(i*2u);
  if(peaq_dc_r_csselr()!=i*2u)return bad(DC_READBACK);
  PeaqDcLevel level;
  if(!peaq_dc_decode_level(peaq_dc_r_ccsidr(),&level))return bad(DC_GEOMETRY);
  peaq_dc.level[i]=level;
 }
 peaq_dc_w_csselr(peaq_dc.csselr);
 if(!same_translation())return bad(DC_READBACK);
 if(!peaq_dc_build_tables((uintptr_t)__cache_text_end))return bad(DC_TABLES);
 peaq_dc.prepared=1;return 1;
}
static void data_maintenance(int clean){
 /* Inner to outer. All control/report/stack data is noncached. Disable
  * allocation before cleaning, then complete each level before the next. */
 peaq_dc_barrier();
 for(unsigned l=0;l<PEAQ_DC_LEVELS;++l){
  PeaqDcLevel x=peaq_dc.level[l];
  for(unsigned way=x.ways;way--;)for(unsigned set=x.sets;set--;){
   uint32_t operand=(l<<1)|(set<<x.line_shift)|(way<<x.way_shift);
   if(clean){peaq_dc_clean_setway(operand);++peaq_dc.clean_ops;}
   else {peaq_dc_invalidate_setway(operand);++peaq_dc.invalidate_ops;}
  }
  peaq_dc_barrier();
 }
}
int peaq_dc_enter(unsigned use_data){
 if(use_data>1||!peaq_dc.prepared||peaq_dc.error||peaq_dc.active||peaq_dc.leaving)return bad(DC_ORDER);
 if(peaq_dc_r_sctlr()!=peaq_dc.sctlr||peaq_dc_r_actlr()!=peaq_dc.actlr||
    (peaq_dc_r_cpsr()&255u)!=0xdfu||!same_translation())return bad(DC_CONTEXT);
 peaq_dc.restored=0;peaq_dc.active=use_data?2u:1u;
 if(use_data){
  if(!peaq_dc_check_tables((uintptr_t)__cache_text_end))return bad(DC_TABLES);
  data_maintenance(0);
  peaq_dc_w_ttbcr(0);peaq_dc_w_ttbr0(PEAQ_DC_TABLE_BASE);
  peaq_dc_w_ttbr1(0);peaq_dc_w_dacr(1);
  if(peaq_dc_r_ttbcr()!=0||peaq_dc_r_ttbr0()!=PEAQ_DC_TABLE_BASE||
     peaq_dc_r_ttbr1()!=0||peaq_dc_r_dacr()!=1)return bad(DC_READBACK);
  peaq_dc_tlb();
 }
 peaq_dc_instruction();
 uint32_t want=peaq_dc.sctlr|(use_data?0x1005u:0x1000u);
 peaq_dc_w_sctlr(want);peaq_dc.observed_enabled=peaq_dc_r_sctlr();
 if(peaq_dc.observed_enabled!=want)return bad(DC_READBACK);
 ++peaq_dc.entries;return 1;
}
int peaq_dc_leave(void){
 if(peaq_dc.leaving)return bad(DC_RESTORE);
 if(!peaq_dc.snapshot)return !(peaq_dc_r_sctlr()&0x1005u);
 peaq_dc.leaving=1;
 uint32_t s=peaq_dc_r_sctlr();
 if((s&~0x1005u)!=peaq_dc.sctlr||peaq_dc_r_actlr()!=peaq_dc.actlr||
    (peaq_dc_r_cpsr()&255u)!=0xdfu)return bad(DC_RESTORE);
 if(s&4u){
  if(!peaq_dc.prepared||peaq_dc.active!=2u)return bad(DC_RESTORE);
  /* Stop new cache allocation before any clean. The fixed monitor's
   * stack/BSS are NC, so cleanup cannot load stale cached report fields. */
  peaq_dc_w_sctlr(s&~0x1004u);
  if(peaq_dc_r_sctlr()!=(s&~0x1004u))return bad(DC_RESTORE);
  data_maintenance(1);
 }
 peaq_dc_instruction();
 peaq_dc_w_sctlr(peaq_dc.sctlr);
 if(peaq_dc_r_sctlr()!=peaq_dc.sctlr)return bad(DC_RESTORE);
 peaq_dc_tlb();
 peaq_dc_w_ttbr0(peaq_dc.ttbr0);peaq_dc_w_ttbr1(peaq_dc.ttbr1);
 peaq_dc_w_ttbcr(peaq_dc.ttbcr);peaq_dc_w_dacr(peaq_dc.dacr);
 peaq_dc_w_csselr(peaq_dc.csselr);
 peaq_dc_barrier();
 if(!same_translation())return bad(DC_RESTORE);
 peaq_dc.active=0;peaq_dc.leaving=0;peaq_dc.restored=1;++peaq_dc.exits;
 return 1;
}
