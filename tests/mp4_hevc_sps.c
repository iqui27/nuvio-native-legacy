#include "../src/dts/mp4_hevc_sps.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t p[1024];size_t bits; } Writer;
static void put(Writer *w,unsigned v,unsigned n) {
  assert(w->bits+n<=sizeof(w->p)*8);
  for(unsigned i=n;i;i--,w->bits++)w->p[w->bits/8]|=((v>>(i-1))&1)<<(7-w->bits%8);
}
static void ue(Writer *w,unsigned v) {
  unsigned n=0,x=v+1;for(unsigned t=x;t>1;t>>=1)n++;
  put(w,0,n);put(w,x,n+1);
}
static size_t fixture(uint8_t *out,int extra,int hdr) {
  Writer w={{0},0};put(&w,0,4);put(&w,extra?2:0,3);put(&w,1,1);
  put(&w,0,32);put(&w,0,32);put(&w,0,32);
  if(extra) {put(&w,15,4);put(&w,0,12);for(int i=0;i<2;i++){put(&w,0,32);put(&w,0,32);put(&w,0,32);}}
  ue(&w,0);ue(&w,1);ue(&w,1920);ue(&w,1080);put(&w,0,1);
  ue(&w,2);ue(&w,2);ue(&w,4);put(&w,0,1);ue(&w,4);ue(&w,2);ue(&w,0);
  for(int i=0;i<6;i++)ue(&w,0);
  put(&w,extra?1:0,1);
  if(extra) {
    put(&w,1,1);
    for(int size=0;size<4;size++)for(int m=0;m<6;m+=size==3?3:1) {
      put(&w,1,1);if(size>1)ue(&w,0);
      for(int k=0;k<(size?64:16);k++)ue(&w,0);
    }
  }
  put(&w,0,2);put(&w,extra?1:0,1);
  if(extra){put(&w,0,8);ue(&w,0);ue(&w,0);put(&w,0,1);}
  ue(&w,extra?2:0);
  if(extra) {
    ue(&w,1);ue(&w,0);ue(&w,0);put(&w,1,1);
    put(&w,1,1);put(&w,0,1);ue(&w,0);put(&w,1,1);put(&w,0,1);put(&w,1,1);
  }
  put(&w,extra?1:0,1);if(extra){ue(&w,1);put(&w,0,9);}
  put(&w,0,2);put(&w,1,1);put(&w,extra?1:0,1);
  if(extra){put(&w,255,8);put(&w,1,16);put(&w,1,16);}
  put(&w,extra?1:0,1);if(extra)put(&w,0,1);
  put(&w,1,1);put(&w,5,3);put(&w,0,1);put(&w,1,1);
  put(&w,hdr?9:1,8);put(&w,hdr?16:1,8);put(&w,hdr?9:1,8);put(&w,1,1);
  size_t n=2;unsigned zeros=0;out[0]=66;out[1]=1;
  for(size_t i=0;i<(w.bits+7)/8;i++) {
    if(zeros>=2 && w.p[i]<=3){out[n++]=3;zeros=0;}
    out[n++]=w.p[i];zeros=w.p[i]?0:zeros+1;
  }
  return n;
}
int main(void) {
  uint8_t p[8193],copy[8193];NvMp4Sps s;
  for(int extra=0;extra<2;extra++)for(int hdr=0;hdr<2;hdr++) {
    size_t n=fixture(p,extra,hdr);assert(nv_mp4_sps(p,n,&s)==1);
    assert(s.width==1920 && s.height==1080 && s.depth==10);
    assert(s.primaries==(hdr?9:1) && s.transfer==(hdr?16:1) && s.matrix==(hdr?9:1));
    for(size_t i=0;i+2<n;i++)assert(nv_mp4_sps(p,i,&s)<0);
    memcpy(copy,p,n);copy[0]=64;assert(nv_mp4_sps(copy,n,&s)<0);
  }
  assert(nv_mp4_sps(NULL,4,&s)<0 && nv_mp4_sps(p,8193,&s)<0);
  uint32_t seed=19;
  for(unsigned run=0;run<20000;run++) {
    size_t n=4+(run%256);p[0]=66;p[1]=1;
    for(size_t i=2;i<n;i++){seed=seed*1664525+1013904223;p[i]=(uint8_t)(seed>>24);}
    (void)nv_mp4_sps(p,n,&s);
  }
  puts("PASS bounded SPS colour parsing: HDR/SDR, sublayers, scaling lists, PCM, predicted RPS, long-term refs, truncations and fuzz inputs");
}
