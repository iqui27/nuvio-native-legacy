/* Unit checks for src/dts/hevc_annexb.h (no FFmpeg needed). */
#include <assert.h>
#include <stdio.h>
#include "dts/hevc_annexb.h"

typedef struct { uint8_t b[4096]; size_t n; } Buf;
static void put(Buf *o,const void *p,size_t n) { assert(o->n+n<=sizeof o->b);memcpy(o->b+o->n,p,n);o->n+=n; }
/* NAL with type t and a tag byte so different units differ. */
static size_t nal(uint8_t *p,unsigned t,uint8_t tag,size_t len) {
  p[0]=(uint8_t)(t<<1);p[1]=1;
  for(size_t i=2;i<len;i++)p[i]=(uint8_t)(tag+i);
  return len;
}
static void lp(Buf *o,int ls,unsigned t,uint8_t tag,size_t len) {
  uint8_t h[4],n[512];
  for(int b=0;b<ls;b++)h[b]=(uint8_t)(len>>(8*(ls-1-b)));
  put(o,h,(size_t)ls);put(o,n,nal(n,t,tag,len));
}
static void ab(Buf *o,unsigned t,uint8_t tag,size_t len) {
  static const uint8_t sc[4]={0,0,0,1};uint8_t n[512];
  put(o,sc,4);put(o,n,nal(n,t,tag,len));
}
static Buf hvcc(int ls) {
  Buf x={{0},0};uint8_t z[21]={1};uint8_t n[512];
  put(&x,z,21);
  uint8_t c=(uint8_t)(0xfc|(ls-1)),arrays=3;put(&x,&c,1);put(&x,&arrays,1);
  const unsigned types[3]={32,33,34};
  for(int a=0;a<3;a++) {
    uint8_t hd[5]={(uint8_t)(0x80|types[a]),0,1,0,(uint8_t)(10+a)};
    put(&x,hd,5);put(&x,n,nal(n,types[a],0xA0,(size_t)(10+a)));
  }
  return x;
}
static void sets(Buf *o) { ab(o,32,0xA0,10);ab(o,33,0xA0,11);ab(o,34,0xA0,12); }
static void expect(const NvHevcAnnexB *h,const Buf *in,const Buf *want,int want_mode) {
  size_t size=0;int mode=-1;uint8_t out[4096];
  assert(nv_hevc_annexb_packet(h,in->b,in->n,NULL,0,&size,&mode)==0);
  assert(size==want->n && mode==want_mode);
  assert(nv_hevc_annexb_packet(h,in->b,in->n,out,size,&size,NULL)==0);
  assert(!memcmp(out,want->b,size));
  if(size)assert(nv_hevc_annexb_packet(h,in->b,in->n,out,size-1,&size,NULL)==-1);
}

int main(void) {
  NvHevcAnnexB h={0};
  Buf x=hvcc(4);
  assert(nv_hevc_annexb_open(&h,x.b,x.n)==1 && h.length_size==4 && h.sets_size==4*3+10+11+12);
  { Buf w={{0},0};sets(&w);assert(!memcmp(h.sets,w.b,w.n)); }

  /* IRAP alone: container sets before it, as FFmpeg. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,39,1,8);lp(&in,4,19,2,40);
    ab(&w,39,1,8);sets(&w);ab(&w,19,2,40);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_CONTAINER); }
  /* Two IRAP slices: sets only before the first. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,20,3,30);lp(&in,4,20,4,30);
    sets(&w);ab(&w,20,3,30);ab(&w,20,4,30);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_CONTAINER); }
  /* Own VPS/SPS/PPS (different from hvcC): nothing added. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,35,5,3);lp(&in,4,32,6,10);lp(&in,4,33,6,11);lp(&in,4,34,6,12);lp(&in,4,39,7,8);lp(&in,4,21,8,50);lp(&in,4,62,9,20);
    ab(&w,35,5,3);ab(&w,32,6,10);ab(&w,33,6,11);ab(&w,34,6,12);ab(&w,39,7,8);ab(&w,21,8,50);ab(&w,62,9,20);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_INBAND); }
  /* Own PPS only: hvcC after the AUD, own PPS after hvcC. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,35,5,3);lp(&in,4,34,6,12);lp(&in,4,19,8,50);
    ab(&w,35,5,3);sets(&w);ab(&w,34,6,12);ab(&w,19,8,50);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_MIXED); }
  /* Own SPS+PPS without AUD: hvcC at the very start. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,33,6,11);lp(&in,4,34,6,12);lp(&in,4,16,8,50);
    sets(&w);ab(&w,33,6,11);ab(&w,34,6,12);ab(&w,16,8,50);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_MIXED); }
  /* PPS after the IRAP slice does not count. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,19,8,50);lp(&in,4,34,6,12);
    sets(&w);ab(&w,19,8,50);ab(&w,34,6,12);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_CONTAINER); }
  /* Non-IRAP picture: start codes only. */
  { Buf in={{0},0},w={{0},0};
    lp(&in,4,1,8,50);lp(&in,4,0,9,20);
    ab(&w,1,8,50);ab(&w,0,9,20);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_NO_IRAP); }
  /* Empty packet. */
  { Buf in={{0},0},w={{0},0};expect(&h,&in,&w,NV_HEVC_ANNEXB_NO_IRAP); }
  /* Malformed: rejected like FFmpeg. */
  { size_t s;const uint8_t a[]={0,0,0,1,0x26},b[]={0,0,0,9,0x26,1},c[]={0,0,0};
    assert(nv_hevc_annexb_packet(&h,a,sizeof a,NULL,0,&s,NULL)==-1);
    assert(nv_hevc_annexb_packet(&h,b,sizeof b,NULL,0,&s,NULL)==-1);
    assert(nv_hevc_annexb_packet(&h,c,sizeof c,NULL,0,&s,NULL)==-1); }

  /* 2-byte lengths. */
  { Buf x2=hvcc(2);assert(nv_hevc_annexb_open(&h,x2.b,x2.n)==1 && h.length_size==2);
    Buf in={{0},0},w={{0},0};
    lp(&in,2,34,6,12);lp(&in,2,19,8,300);
    sets(&w);ab(&w,34,6,12);ab(&w,19,8,300);
    expect(&h,&in,&w,NV_HEVC_ANNEXB_MIXED); }

  /* hvcC forms left to FFmpeg's filter. */
  { Buf y=hvcc(4);y.b[0]=0;assert(nv_hevc_annexb_open(&h,y.b,y.n)==0 && !h.sets); }
  { Buf y=hvcc(4);y.b[23]=0x80|1;assert(nv_hevc_annexb_open(&h,y.b,y.n)==0); }      /* slice type in hvcC */
  { Buf y=hvcc(4);y.b[26]=0;y.b[27]=0;assert(nv_hevc_annexb_open(&h,y.b,y.n)==0); } /* zero-length NAL */
  { Buf y=hvcc(4);assert(nv_hevc_annexb_open(&h,y.b,y.n-1)==0); }                   /* truncated */
  { const uint8_t ab3[]={0,0,1,0x40,1};assert(nv_hevc_annexb_open(&h,ab3,sizeof ab3)==0); }
  /* hvcC without arrays: works, adds nothing. */
  { Buf y={{0},0};uint8_t z[23]={1};z[21]=0xff;put(&y,z,23);
    assert(nv_hevc_annexb_open(&h,y.b,y.n)==1 && h.sets_size==0); }
  nv_hevc_annexb_close(&h);
  puts("PASS hevc_annexb: FFmpeg layout without own sets, own VPS/SPS/PPS kept last, malformed rejected");
  return 0;
}
