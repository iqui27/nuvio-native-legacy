#ifndef NUVIO_MP4_HEVC_SPS_H
#define NUVIO_MP4_HEVC_SPS_H
#include <stdint.h>
#include <stddef.h>

/* Read only a single-layer HEVC SPS prefix through VUI colour_description.
 * No picture decoder, external allocations or changes to compressed bytes.
 * Every input/loop is bounded; unrecognised syntax fails closed. */
typedef struct { const uint8_t *p; size_t bits, at; int bad; } NvMp4Bits;
typedef struct { int width,height,depth,primaries,transfer,matrix,full_range; } NvMp4Sps;
static inline unsigned nv_mp4_bits(NvMp4Bits *b,unsigned n) {
  if(n>32 || b->at>b->bits || n>b->bits-b->at) {b->bad=1;return 0;}
  unsigned v=0;
  for(unsigned i=0;i<n;i++,b->at++)v=(v<<1)|((b->p[b->at/8]>>(7-b->at%8))&1);
  return v;
}
static inline void nv_mp4_skip(NvMp4Bits *b,size_t n) {
  if(b->at>b->bits || n>b->bits-b->at)b->bad=1;
  else b->at+=n;
}
static inline unsigned nv_mp4_ue(NvMp4Bits *b) {
  unsigned zeros=0;
  while(!b->bad && !nv_mp4_bits(b,1)) {
    if(++zeros>30) {b->bad=1;return 0;}
  }
  return b->bad?0:((UINT32_C(1)<<zeros)-1)+nv_mp4_bits(b,zeros);
}
static inline int nv_mp4_sps(const uint8_t *p,size_t n,NvMp4Sps *out) {
  if(!p || !out || n<4 || n>8192 || p[0]!=66 || p[1]!=1)return -1;
  uint8_t rbsp[8192];size_t used=0;unsigned zeros=0;
  for(size_t i=2;i<n;i++) {
    if(zeros>=2 && p[i]==3) {
      if(i+1==n || p[i+1]>3)return -1;
      zeros=0;continue;
    }
    rbsp[used++]=p[i];zeros=p[i]?0:zeros+1;
  }
  NvMp4Bits b={rbsp,used*8,0,0};NvMp4Sps s={0};
  s.primaries=s.transfer=s.matrix=2;
  nv_mp4_skip(&b,4);unsigned sub=nv_mp4_bits(&b,3);
  if(sub>6)return -1;
  nv_mp4_skip(&b,1+96);
  unsigned prof[7]={0},lev[7]={0};
  for(unsigned i=0;i<sub;i++) {prof[i]=nv_mp4_bits(&b,1);lev[i]=nv_mp4_bits(&b,1);}
  if(sub)nv_mp4_skip(&b,2*(8-sub));
  for(unsigned i=0;i<sub;i++)nv_mp4_skip(&b,88*prof[i]+8*lev[i]);
  if(nv_mp4_ue(&b)>15)return -1;
  unsigned chroma=nv_mp4_ue(&b);
  if(chroma!=1)return -1; /* This recovery handles the HDR10 4:2:0 base only. */
  unsigned w=nv_mp4_ue(&b),h=nv_mp4_ue(&b),left=0,right=0,top=0,bottom=0;
  if(!w || !h || w>32768 || h>32768)return -1;
  if(nv_mp4_bits(&b,1)) {
    left=nv_mp4_ue(&b);right=nv_mp4_ue(&b);top=nv_mp4_ue(&b);bottom=nv_mp4_ue(&b);
    if(left>w/2 || right>w/2-left || top>h/2 || bottom>h/2-top)return -1;
  }
  s.width=(int)(w-2*(left+right));s.height=(int)(h-2*(top+bottom));
  unsigned depth=nv_mp4_ue(&b),chroma_depth=nv_mp4_ue(&b),poc=nv_mp4_ue(&b);
  if(depth>8 || chroma_depth!=depth || poc>12 || s.width<=0 || s.height<=0)return -1;
  s.depth=(int)depth+8;poc+=4;
  unsigned ordering=nv_mp4_bits(&b,1);
  for(unsigned i=ordering?0:sub;i<=sub;i++) {
    if(nv_mp4_ue(&b)>15 || nv_mp4_ue(&b)>15)return -1;
    nv_mp4_ue(&b);
  }
  for(unsigned i=0;i<6;i++)if(nv_mp4_ue(&b)>30)return -1;
  if(nv_mp4_bits(&b,1) && nv_mp4_bits(&b,1)) {
    for(unsigned size=0;size<4;size++)for(unsigned m=0;m<6;m+=size==3?3:1) {
      if(!nv_mp4_bits(&b,1)) {if(nv_mp4_ue(&b)>5)return -1;}
      else {
        if(size>1)nv_mp4_ue(&b); /* signed dc coefficient; value is not needed */
        for(unsigned k=0;k<(size?64:16);k++)nv_mp4_ue(&b);
      }
    }
  }
  nv_mp4_skip(&b,2); /* AMP, SAO */
  if(nv_mp4_bits(&b,1)) {nv_mp4_skip(&b,8);nv_mp4_ue(&b);nv_mp4_ue(&b);nv_mp4_skip(&b,1);}
  unsigned sets=nv_mp4_ue(&b),previous=0;
  if(sets>64)return -1;
  for(unsigned i=0;i<sets;i++) {
    unsigned count=0;
    if(i && nv_mp4_bits(&b,1)) {
      nv_mp4_skip(&b,1);if(nv_mp4_ue(&b)>32767)return -1;
      for(unsigned k=0;k<=previous;k++) {
        unsigned selected=nv_mp4_bits(&b,1);
        if(!selected)selected=nv_mp4_bits(&b,1);
        count+=selected;
      }
    } else {
      unsigned neg=nv_mp4_ue(&b),pos=nv_mp4_ue(&b);
      if(neg>15 || pos>15)return -1;
      count=neg+pos;
      for(unsigned k=0;k<count;k++) {if(nv_mp4_ue(&b)>32767)return -1;nv_mp4_skip(&b,1);}
    }
    if(count>31)return -1;
    previous=count;
  }
  if(nv_mp4_bits(&b,1)) {
    unsigned refs=nv_mp4_ue(&b);if(refs>32)return -1;
    nv_mp4_skip(&b,(poc+1)*refs);
  }
  nv_mp4_skip(&b,2); /* temporal MVP, strong intra smoothing */
  if(nv_mp4_bits(&b,1)) {
    if(nv_mp4_bits(&b,1) && nv_mp4_bits(&b,8)==255)nv_mp4_skip(&b,32);
    if(nv_mp4_bits(&b,1))nv_mp4_skip(&b,1);
    if(nv_mp4_bits(&b,1)) {
      nv_mp4_skip(&b,3);s.full_range=(int)nv_mp4_bits(&b,1);
      if(nv_mp4_bits(&b,1)) {
        s.primaries=(int)nv_mp4_bits(&b,8);s.transfer=(int)nv_mp4_bits(&b,8);s.matrix=(int)nv_mp4_bits(&b,8);
      }
    }
  }
  if(b.bad)return -1;
  *out=s;return 1;
}
#endif
