#include "dts/dts_hevc_diag.h"
#include <assert.h>
#include <stdlib.h>

typedef struct { unsigned char p[2048];size_t n; } Bytes;
typedef struct { unsigned char p[256];unsigned bits; } Bits;
static void bit(Bits *b,unsigned v) {assert(b->bits<sizeof b->p*8);b->p[b->bits/8]|=(v&1)<<(7-b->bits%8);b->bits++;}
static void bits(Bits *b,unsigned v,unsigned n) {for(unsigned i=n;i;i--)bit(b,v>>(i-1));}
static void ue(Bits *b,unsigned v) {unsigned code=v+1,n=0;for(unsigned x=code;x>1;x>>=1)n++;for(unsigned i=0;i<n;i++)bit(b,0);bits(b,code,n+1);}
static Bytes nal(unsigned type,const Bits *b) {
  Bytes r={{0},2};r.p[0]=(unsigned char)(type<<1);r.p[1]=1;unsigned zeros=0;
  for(unsigned i=0;i<(b->bits+7)/8;i++) {
    if(zeros==2 && b->p[i]<=3){r.p[r.n++]=3;zeros=0;}
    r.p[r.n++]=b->p[i];zeros=b->p[i]?0:zeros+1;
  }return r;
}
static void append(Bytes *p,const Bytes *n,unsigned length) {
  assert(p->n+n->n+4<sizeof p->p);
  if(length){for(unsigned i=length;i;i--)p->p[p->n++]=(unsigned char)(n->n>>(8*(i-1)));}
  else {p->p[p->n++]=0;p->p[p->n++]=0;p->p[p->n++]=0;p->p[p->n++]=1;}
  memcpy(p->p+p->n,n->p,n->n);p->n+=n->n;
}
static Bytes pps(unsigned id,unsigned sps,unsigned tiles,unsigned wpp) {
  Bits b={{0},0};ue(&b,id);ue(&b,sps);bit(&b,1);bit(&b,0);bits(&b,2,3);bits(&b,0,2);
  ue(&b,0);ue(&b,0);ue(&b,0);bits(&b,0,2);bit(&b,1);ue(&b,0);
  ue(&b,0);ue(&b,0);bits(&b,0,4);bit(&b,tiles);bit(&b,wpp);bit(&b,1);
  return nal(34,&b);
}
int main(int argc,char **argv) {
  Bits b={{0},0};bits(&b,3,4);bits(&b,0,3);bit(&b,1);for(unsigned i=0;i<96;i++)bit(&b,0);ue(&b,2);bit(&b,1);
  Bytes sps=nal(33,&b),p=pps(5,2,1,1);
  memset(&b,0,sizeof b);bit(&b,1);bit(&b,0);ue(&b,5);bit(&b,1);Bytes slice=nal(20,&b);
  memset(&b,0,sizeof b);bits(&b,0x53,8);Bytes rpu=nal(62,&b),sei=nal(39,&b);
  Bytes ann={{0},0};append(&ann,&sps,0);append(&ann,&p,0);append(&ann,&sei,0);append(&ann,&slice,0);append(&ann,&rpu,0);
  unsigned char saved[sizeof ann.p];memcpy(saved,ann.p,sizeof saved);
  NvHevcDiag a=nv_hevc_diag_scan(ann.p,ann.n,0);
  assert(a.nals==5 && a.vcl==1 && a.params==2 && a.rpu==1 && a.sei==1 && !a.malformed && !a.clipped);
  assert(a.sps_id==2 && a.pps_id==5 && a.pps_sps==2 && a.slice_pps==5 && a.first_slice==1);
  assert(a.dependent==1 && a.extra_bits==2 && a.tiles==1 && a.wpp==1 && !a.syntax_unknown);
  for(unsigned length=1;length<=4;length++) {
    Bytes lp={{0},0};append(&lp,&sps,length);append(&lp,&p,length);append(&lp,&sei,length);append(&lp,&slice,length);append(&lp,&rpu,length);
    NvHevcDiag x=nv_hevc_diag_scan(lp.p,lp.n,length);
    assert(!x.malformed && x.nals==a.nals && x.vcl_hash==a.vcl_hash && x.param_hash==a.param_hash && x.rpu_hash==a.rpu_hash);
    assert(x.slice_pps==5 && x.sps_id==2 && x.tiles==1);
    for(size_t n=0;n<lp.n;n++)(void)nv_hevc_diag_scan(lp.p,n,length);
  }
  // Changing metadata leaves the picture signature stable; changing a VCL byte does not.
  Bytes changed={{0},0};sei.p[2]^=0x10;append(&changed,&sps,0);append(&changed,&p,0);append(&changed,&sei,0);append(&changed,&slice,0);append(&changed,&rpu,0);
  NvHevcDiag x=nv_hevc_diag_scan(changed.p,changed.n,0);assert(x.vcl_hash==a.vcl_hash && x.sei_hash!=a.sei_hash);
  slice.p[slice.n-1]^=0x20;Bytes v={{0},0};append(&v,&slice,0);x=nv_hevc_diag_scan(v.p,v.n,0);assert(x.vcl_hash!=a.vcl_hash);
  assert(!memcmp(saved,ann.p,sizeof saved));
  assert(nv_hevc_diag_scan(NULL,0,0).malformed);
  const unsigned char short_length[]={0xff,0xff,0xff,0xff};assert(nv_hevc_diag_scan(short_length,4,4).malformed);
  unsigned char *cap=calloc(NV_HEVC_DIAG_BYTES,1);assert(cap);
  x=nv_hevc_diag_scan(cap,NV_HEVC_DIAG_BYTES+100,0);assert(x.clipped);free(cap);
  Bytes many={{0},0};Bytes tiny={{124,1,128},3};
  for(unsigned i=0;i<270;i++)append(&many,&tiny,0);
  x=nv_hevc_diag_scan(many.p,many.n,0);assert(x.clipped && x.nals==256 && x.map_clipped);
  unsigned char fuzz[256];uint32_t seed=13;
  for(unsigned j=0;j<20000;j++) {
    for(size_t k=0;k<sizeof fuzz;k++){seed=seed*1664525u+1013904223u;fuzz[k]=(unsigned char)(seed>>24);}
    if(j%2==0){fuzz[0]=0;fuzz[1]=0;fuzz[2]=1;fuzz[3]=(unsigned char)((j%64)<<1);fuzz[4]=1;}
    (void)nv_hevc_diag_scan(fuzz,j%sizeof fuzz,j%5);
  }
  NvVideoTiming t={0};
  nv_video_timing_log(&t,"test",1,100,90,40,0);nv_video_timing_log(&t,"test",1,180,130,40,0);nv_video_timing_log(&t,"test",1,140,170,40,0);
  assert(t.pts_back==1 && !t.dts_back && t.min_step==-40 && t.max_step==80);
  nv_video_timing_log(&t,"test",1,INT64_MIN,INT64_MIN,0,0);assert(t.missing_pts==1 && t.missing_dts==1);
  nv_video_timing_log(&t,"test",1,-10,-20,0,0);assert(t.missing_pts==1 && t.missing_dts==1);
  assert(nv_video_diag_delta(INT64_MAX,-10)==INT64_MAX && nv_video_diag_delta(INT64_MIN,10)==INT64_MIN);
  for(unsigned i=0;i<200;i++)nv_video_timing_log(&t,"test",1,i,i,1,0);
  assert(t.count==120);memset(&t,0,sizeof t);nv_video_timing_log(&t,"test",2,10,5,1,0);assert(t.count==1 && !t.pts_back);
  if(argc==2) {
    FILE *f=fopen(argv[1],"rb");assert(f);unsigned char *data=malloc(NV_HEVC_DIAG_BYTES);assert(data);
    size_t n=fread(data,1,NV_HEVC_DIAG_BYTES,f);assert(!ferror(f));fclose(f);
    x=nv_hevc_diag_scan(data,n,0);assert(!x.clipped && !x.malformed && !x.syntax_unknown && x.vcl && x.sps_id>=0 && x.pps_id>=0);
    printf("REAL HEVC: nals=%u sps=%d pps=%d slicePps=%d tiles=%d wpp=%d\n",x.nals,x.sps_id,x.pps_id,x.slice_pps,x.tiles,x.wpp);free(data);
  }
  puts("PASS HEVC structure diagnostics: framing-independent hashes, SPS/PPS/slice references, PPS flags, immutability, malformed/fuzz/caps and bounded timestamp reordering summaries");
}
