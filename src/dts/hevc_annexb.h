#ifndef NV_HEVC_ANNEXB_H
#define NV_HEVC_ANNEXB_H
/* HEVC length-prefixed (hvcC) -> Annex B for the webOS packet adapter,
 * keeping the parameter sets the stream itself carries.
 *
 * FFmpeg's hevc_mp4toannexb (7.1.5) copies every hvcC parameter set in front
 * of the first IRAP slice of a packet, i.e. AFTER any VPS/SPS/PPS the packet
 * already carries. When a film changes its parameter sets along the way
 * (encoded in parts), the LG decoder then receives the current PPS 0 from the
 * stream followed by the container's older PPS 0, and decodes the following
 * pictures with the wrong one: broken blocks that appear and disappear with
 * the keyframes, only in some parts of the film. The LG diagnostics showed
 * exactly that: different PPS 0 before/after the filter at 5:33, 19:28,
 * 24:04 and 1:28:58, identical slices.
 *
 * Output is byte-identical to FFmpeg's filter except for IRAP packets that
 * carry parameter sets themselves:
 *  - VPS, SPS and PPS all present: nothing from hvcC is added;
 *  - only some of them present: the hvcC sets go to the START of the access
 *    unit (after a leading AUD), so the stream's own sets come later and win.
 * Same 4-byte start codes and same rejection of malformed packets as FFmpeg.
 * Pure C, no libav dependency, so it can be tested on its own. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
  NV_HEVC_ANNEXB_NO_IRAP = 0,   /* no IRAP slice: start codes only */
  NV_HEVC_ANNEXB_CONTAINER = 1, /* IRAP without own sets: hvcC before IRAP (as FFmpeg) */
  NV_HEVC_ANNEXB_INBAND = 2,    /* IRAP with own VPS+SPS+PPS: hvcC not added */
  NV_HEVC_ANNEXB_MIXED = 3      /* IRAP with some own sets: hvcC first, own sets after */
};

typedef struct {
  uint8_t *sets;      /* hvcC arrays as Annex B, 4-byte start codes */
  size_t sets_size;
  int length_size;    /* 1..4 */
} NvHevcAnnexB;

static inline void nv_hevc_annexb_close(NvHevcAnnexB *h) {
  if(!h)return;
  free(h->sets);
  memset(h,0,sizeof *h);
}

/* 1: usable. 0: not an hvcC this code handles; the caller keeps FFmpeg's
 * filter, which then behaves (or fails) exactly as before. -1: no memory. */
static inline int nv_hevc_annexb_open(NvHevcAnnexB *h,const uint8_t *x,size_t n) {
  size_t i,k=0,cap=0;
  int arrays;
  nv_hevc_annexb_close(h);
  /* FFmpeg's own test for "already Annex B" plus the hvcC version byte. */
  if(!x || n<23 || x[0]!=1)return 0;
  arrays=x[22];
  for(int pass=0;pass<2;pass++) {
    i=23;k=0;
    for(int a=0;a<arrays;a++) {
      unsigned type,count;
      if(n-i<3)return 0;
      type=x[i]&0x3f;count=((unsigned)x[i+1]<<8)|x[i+2];i+=3;
      if(type!=32 && type!=33 && type!=34 && type!=39 && type!=40)return 0;
      for(unsigned c=0;c<count;c++) {
        size_t l;
        if(n-i<2)return 0;
        l=((size_t)x[i]<<8)|x[i+1];i+=2;
        if(!l || l>n-i)return 0;
        if(pass) {
          h->sets[k]=0;h->sets[k+1]=0;h->sets[k+2]=0;h->sets[k+3]=1;
          memcpy(h->sets+k+4,x+i,l);
        }
        k+=4+l;i+=l;
      }
    }
    if(!pass) {
      cap=k;
      if(!(h->sets=malloc(cap?cap:1)))return -1;
    }
  }
  h->sets_size=k;
  h->length_size=(x[21]&3)+1;
  return 1;
}

/* Converts one packet. out==NULL only measures. Returns 0 and sets *size
 * (and *mode when not NULL), or -1 for a packet FFmpeg's filter rejects too
 * (truncated length, NAL shorter than 2 bytes or past the end). */
static inline int nv_hevc_annexb_packet(const NvHevcAnnexB *h,const uint8_t *d,size_t n,
                                        uint8_t *out,size_t cap,size_t *size,int *mode) {
  size_t i,first_irap=(size_t)-1,start=0,insert=(size_t)-1,total=0,k=0;
  int have=0,ls=h->length_size,m;
  for(i=0;i<n;) {   /* pass 1: validate, find the first IRAP and the sets before it */
    size_t l=0;
    unsigned t;
    if(n-i<(size_t)ls)return -1;
    for(int b=0;b<ls;b++)l=(l<<8)|d[i+b];
    if(l<2 || l>n-i-(size_t)ls)return -1;
    t=(d[i+(size_t)ls]>>1)&0x3f;
    if(first_irap==(size_t)-1) {
      if(t>=16 && t<=23)first_irap=i;
      else if(t>=32 && t<=34)have|=1<<(t-32);
    }
    if(i==0 && t==35)start=(size_t)ls+l;  /* AUD stays first */
    if(4+l>SIZE_MAX-total)return -1;
    total+=4+l;
    i+=(size_t)ls+l;
  }
  if(first_irap==(size_t)-1)m=NV_HEVC_ANNEXB_NO_IRAP;
  else if(have==7)m=NV_HEVC_ANNEXB_INBAND;
  else if(have)m=NV_HEVC_ANNEXB_MIXED,insert=start;
  else m=NV_HEVC_ANNEXB_CONTAINER,insert=first_irap;
  if(insert!=(size_t)-1) {
    if(h->sets_size>SIZE_MAX-total)return -1;
    total+=h->sets_size;
  }
  if(mode)*mode=m;
  *size=total;
  if(!out)return 0;
  if(cap<total)return -1;
  for(i=0;i<n;) {   /* pass 2: write */
    size_t l=0;
    if(i==insert && h->sets_size) { memcpy(out+k,h->sets,h->sets_size);k+=h->sets_size; }
    for(int b=0;b<ls;b++)l=(l<<8)|d[i+b];
    out[k]=0;out[k+1]=0;out[k+2]=0;out[k+3]=1;
    memcpy(out+k+4,d+i+(size_t)ls,l);
    k+=4+l;
    i+=(size_t)ls+l;
  }
  return 0;
}
#endif
