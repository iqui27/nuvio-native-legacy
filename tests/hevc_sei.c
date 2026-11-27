#include "dts/hevc_sei.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { uint8_t p[4096];size_t n; } Bytes;
static void byte(Bytes *b,uint8_t v){assert(b->n<sizeof b->p);b->p[b->n++]=v;}
static void number(Bytes *b,size_t v){while(v>=255){byte(b,255);v-=255;}byte(b,(uint8_t)v);}
static void message(Bytes *b,size_t type,const uint8_t *p,size_t n) {
  number(b,type);number(b,n);for(size_t i=0;i<n;i++)byte(b,p[i]);
}
static void nal(Bytes *dst,unsigned type,const Bytes *rbsp,int start4) {
  byte(dst,0);byte(dst,0);if(start4)byte(dst,0);byte(dst,1);
  byte(dst,(uint8_t)(type<<1));byte(dst,1);unsigned zeros=0;
  for(size_t i=0;i<rbsp->n;i++) {
    uint8_t c=rbsp->p[i];
    if(zeros>=2 && c<=3){byte(dst,3);zeros=0;}
    byte(dst,c);zeros=c?0:zeros+1;
  }
}
static const uint8_t hdr[]={0xb5,0,0x3c,0,1,4,1,0,0,1,0,0,2,0,0,3};
static void append_sei(Bytes *out,int include,int order,int start4) {
  Bytes r={{0},0};uint8_t mastering[24]={0};mastering[20]=3;
  uint8_t cll[]={0,0,1,0};
  if(include && order==0)message(&r,4,hdr,sizeof hdr);
  message(&r,137,mastering,sizeof mastering);
  if(include && order==1)message(&r,4,hdr,sizeof hdr);
  message(&r,144,cll,sizeof cll);
  if(include && order==2)message(&r,4,hdr,sizeof hdr);
  byte(&r,128);nal(out,39,&r,start4);
}
static void append_picture(Bytes *out) {
  Bytes v={{0xab,0,0,1,0xde,0xad,0xbe,0xef},8};
  nal(out,32,&v,1);nal(out,33,&v,0);nal(out,34,&v,1);
  nal(out,20,&v,1);nal(out,62,&v,0);
}
int main(void) {
  NvHevcSeiReport r;size_t size;
  for(int start4=0;start4<2;start4++)for(int order=0;order<3;order++) {
    Bytes input={{0},0},expected={{0},0};
    append_sei(&input,1,order,start4);append_sei(&expected,0,order,start4);
    append_picture(&input);append_picture(&expected);
    assert(nv_hevc_sei_filter(input.p,input.n,&size,&r)==1 && r.removed==1 && !r.malformed);
    assert(size==expected.n && !memcmp(input.p,expected.p,size));
    assert(nv_hevc_sei_filter(input.p,size,&size,&r)==0 && !r.removed);
  }
  // Remove a metadata-only NAL while preserving following RPU and picture.
  for(int type=39;type<=40;type++) {
    Bytes input={{0},0},expected={{0},0},rbsp={{0},0};
    message(&rbsp,4,hdr,sizeof hdr);message(&rbsp,4,hdr,sizeof hdr);byte(&rbsp,128);
    nal(&input,(unsigned)type,&rbsp,1);append_picture(&input);append_picture(&expected);
    assert(nv_hevc_sei_filter(input.p,input.n,&size,&r)==1 && r.removed==2);
    assert(size==expected.n && !memcmp(input.p,expected.p,size));
  }
  // Every registration component matters; unknown providers/versions survive.
  for(size_t changed=0;changed<7;changed++) {
    Bytes input={{0},0},rbsp={{0},0};uint8_t other[sizeof hdr];memcpy(other,hdr,sizeof hdr);
    other[changed]=(uint8_t)(other[changed]+2);message(&rbsp,4,other,sizeof other);byte(&rbsp,128);
    nal(&input,39,&rbsp,1);Bytes saved=input;
    assert(!nv_hevc_sei_filter(input.p,input.n,&size,&r) && !r.removed);
    assert(size==saved.n && !memcmp(input.p,saved.p,size));
  }
  // Extended type/length fields and unknown user data remain byte-identical.
  Bytes input={{0},0},expected={{0},0},rbsp={{0},0};uint8_t opaque[300];
  for(size_t i=0;i<sizeof opaque;i++)opaque[i]=(uint8_t)i;
  message(&rbsp,300,opaque,sizeof opaque);message(&rbsp,4,hdr,sizeof hdr);byte(&rbsp,128);
  nal(&input,39,&rbsp,1);rbsp.n=0;message(&rbsp,300,opaque,sizeof opaque);byte(&rbsp,128);
  nal(&expected,39,&rbsp,1);
  assert(nv_hevc_sei_filter(input.p,input.n,&size,&r)==1);
  assert(size==expected.n && !memcmp(input.p,expected.p,size));
  // Transactional validation: a malformed later NAL preserves the ENTIRE input.
  input.n=0;append_sei(&input,1,1,1);byte(&input,0);byte(&input,0);byte(&input,1);
  byte(&input,78);byte(&input,1);byte(&input,4);byte(&input,255);byte(&input,128);
  Bytes saved=input;assert(!nv_hevc_sei_filter(input.p,input.n,&size,&r) && r.malformed);
  assert(size==saved.n && !memcmp(input.p,saved.p,size));
  // Truncated payload/signature, missing trailing bits and invalid EPB.
  for(size_t cut=1;cut<sizeof hdr+6;cut++) {
    Bytes bad={{0},0};rbsp.n=0;message(&rbsp,4,hdr,sizeof hdr);byte(&rbsp,128);nal(&bad,39,&rbsp,1);
    if(cut>=bad.n)continue;
    saved=bad;size_t n=bad.n-cut;
    assert(!nv_hevc_sei_filter(bad.p,n,&size,&r));assert(size==n && !memcmp(saved.p,bad.p,bad.n));
  }
  // Large SEI is passed through, never copied into a new allocation or truncated.
  uint8_t *big=calloc(NV_HEVC_SEI_NAL_MAX+16,1);assert(big);
  big[2]=1;big[3]=78;big[4]=1;
  assert(!nv_hevc_sei_filter(big,NV_HEVC_SEI_NAL_MAX+16,&size,&r) && r.oversized);
  free(big);
  // New message boundaries can require fresh EPB bytes after removal.
  for(unsigned run=0;run<1000;run++) {
    Bytes a={{0},0},b={{0},0},encoded={{0},0},want={{0},0};
    uint8_t zeros[4]={0};message(&a,5,zeros,4);message(&b,5,zeros,4);
    message(&a,4,hdr,sizeof hdr);
    uint8_t payload[64];size_t len=run%sizeof payload;
    for(size_t i=0;i<len;i++)payload[i]=(uint8_t)((i+run)%4);
    message(&a,run%2?0:300,payload,len);message(&b,run%2?0:300,payload,len);
    byte(&a,128);byte(&b,128);nal(&encoded,39,&a,run%2);nal(&want,39,&b,run%2);
    assert(nv_hevc_sei_filter(encoded.p,encoded.n,&size,&r)==1);
    assert(size==want.n && !memcmp(encoded.p,want.p,size));
  }
  // Fuzz bounded malformed packets; unchanged on rejection, never grow output.
  uint32_t seed=73;
  for(unsigned run=0;run<20000;run++) {
    Bytes fuzz={{0},0};size_t n=8+run%500;
    for(size_t i=0;i<n;i++){seed=seed*1664525+1013904223;fuzz.p[i]=(uint8_t)(seed>>24);}
    fuzz.p[0]=fuzz.p[1]=0;fuzz.p[2]=1;fuzz.p[3]=78;fuzz.p[4]=1;fuzz.n=n;saved=fuzz;
    int result=nv_hevc_sei_filter(fuzz.p,n,&size,&r);assert(result>=0 && size<=n);
    if(!result)assert(size==n && !memcmp(saved.p,fuzz.p,n));
    assert(!memcmp(saved.p+n,fuzz.p+n,sizeof fuzz.p-n));
  }
  puts("PASS HDR10+ SEI: exact registration, mixed metadata, static HDR10/VCL/RPU preservation, EPB, transactional rejection, bounds and fuzz");
}
