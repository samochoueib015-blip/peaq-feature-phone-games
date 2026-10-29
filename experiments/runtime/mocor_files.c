#include "mocor_files.h"
/* Exact original T45-V1.0 User wrappers, including the 64-bit seek ABI. */
const PeaqFileApi peaq_pmp122_files={
 (void *)0x886fbd71u,(void *)0x886fc509u,(void *)0x886fc563u,(void *)0x886fc4d7u,
 (void *)0x886fc5f7u,(void *)0x886fc649u,(void *)0x886fc869u,(void *)0x886fc5b9u
};
static int component(const char *s){
 if(!s||!s[0]||s[0]=='.')return 0;
 for(uint32_t i=0;i<32;i++){
  uint8_t c=(uint8_t)s[i];if(!c)return (int)i;
  if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
 }return 0;
}
int peaq_game_file_path(uint16_t out[96],char drive,const char *folder,const char *name){
 if(!out)return 0;out[0]=0;
 int a=component(folder),b=component(name);if((drive!='D'&&drive!='E')||!a||!b)return 0;
 const char prefix[]="E:\\Games\\";uint32_t i=0;
 while(prefix[i]){out[i]=(uint8_t)prefix[i];i++;}out[0]=(uint8_t)drive;
 for(int j=0;j<a;j++)out[i++]=(uint8_t)folder[j];out[i++]='\\';
 for(int j=0;j<b;j++)out[i++]=(uint8_t)name[j];out[i]=0;return 1;
}
int peaq_file_read_at(const PeaqFileApi *api,uint32_t h,uint32_t offset,void *data,uint32_t n,uint32_t *got){
 if(!got)return 0;*got=0;
 if(!api||!api->seek||!api->read||!h||(!data&&n)||offset>0x7fffffffu||n>0x7fffffffu-offset)return 0;
 if(api->seek(h,(int64_t)offset,0))return 0;
 uint8_t *p=data;uint32_t done=0;
 while(done<n){
  uint32_t count=0,want=n-done;if(want>4096)want=4096;
  if(api->read(h,p+done,want,&count,0)||count>want)return 0;
  done+=count;if(count<want)break;
 }
 *got=done;return 1;
}
