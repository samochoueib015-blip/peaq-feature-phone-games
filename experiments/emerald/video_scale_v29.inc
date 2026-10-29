/* Same nearest-neighbour pixels and CCW orientation as the qualified v28
 * paths. Write one contiguous physical column at a time into the separately
 * owned DMA buffer. No source write, hidden-frame draw or asynchronous DMA. */
static const uint16_t fit_rows[213] = {
    PEAQ_FIT_ROWS_V29
};
static void peaq_video_scale_direct(uint16_t *out,const uint16_t *src,unsigned full){
 for(unsigned x=0;x<320;++x){
  const uint16_t *s=src+x*3u/4u;
  uint16_t *d=out+x*240u;
  if(full){
   s+=159u*240u;
   for(unsigned pair=0;pair<80;++pair){
    uint16_t a=s[0],b=s[-240];
    d[0]=a;d[1]=b;d[2]=b;d+=3;
    /* Do not form an out-of-object pointer after the final iteration. */
    if(pair<79)s-=480;
   }
  }else{
   for(unsigned y=0;y<14;++y)d[y]=0;
   for(unsigned y=0;y<213;++y)d[14+y]=s[fit_rows[y]];
   for(unsigned y=227;y<240;++y)d[y]=0;
  }
 }
}
