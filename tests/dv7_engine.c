/* Actual FFmpeg demux/Annex-B/seek path, with a local Range transport. */
#include "../src/dts/dts_engine.h"
#include "../src/dts/dv7.h"
#include "../src/dts/dts_audio_codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

char *rede_baixar_trecho64_cab(const char *url,const char *headers,int64_t start,int64_t end,
                             long *size,int64_t *total,int *status,volatile int *cancel) {
  (void)headers;
  *size=0; *total=-1; *status=0;
  if (*cancel) return NULL;
  FILE *f=fopen(url,"rb"); if (!f) return NULL;
  assert(!fseeko(f,0,SEEK_END)); *total=ftello(f);
  if (start>=*total) { *status=416; fclose(f); return NULL; }
  assert(start>=0 && end>=start && !fseeko(f,start,SEEK_SET));
  size_t n=(size_t)(end-start+1);
  if ((int64_t)n>*total-start) n=(size_t)(*total-start);
  char *p=malloc(n); assert(p); *size=(long)fread(p,1,n,f); fclose(f); *status=206; return p;
}
char *rede_baixar_trecho64_final(const char *,const char *,int64_t,int64_t,long *,int64_t *,int *,volatile int *,char *,int *);
char *rede_baixar_trecho64_vigiado(const char *u,const char *h,int64_t s,int64_t e,long *n,
 int64_t *t,int *status,volatile int *c,char *f,int *x,volatile int *urgent,unsigned idle,int *stalled) {
 (void)urgent;(void)idle;if(stalled)*stalled=0;
 return rede_baixar_trecho64_final(u,h,s,e,n,t,status,c,f,x);
}
char *rede_baixar_trecho64_vigiado_retry(const char *u,const char *h,int64_t s,int64_t e,long *n,
 int64_t *t,int *status,volatile int *c,char *f,int *x,volatile int *urgent,unsigned idle,int *stalled,
 void (*retry)(void *,int),void *arg) {
 (void)retry;(void)arg;return rede_baixar_trecho64_vigiado(u,h,s,e,n,t,status,c,f,x,urgent,idle,stalled);
}
char *rede_baixar_trecho64_final(const char *u,const char *h,int64_t s,int64_t e,long *n,
                               int64_t *t,int *status,volatile int *c,char *final,int *cross) {
  if(final)*final=0; if(cross)*cross=0; return rede_baixar_trecho64_cab(u,h,s,e,n,t,status,c);
}
/* 2.0.6 also links the Samsung streaming reader; the webOS DV fixture
 * must continue to use authenticated ranges, never the Samsung ring path. */
char *rede_range_headers(const char *raw,int public_only) {
  (void)public_only;return strdup(raw?raw:"");
}
int rede_fluxo64(const char *url,const char *headers,int64_t start,
                 int (*sink)(void *,const char *,size_t),void *opaque,
                 int64_t *total,int *status,volatile int *cancel,
                 char *final,int *cross,long stall_ms,int64_t *bytes) {
  (void)url;(void)headers;(void)start;(void)sink;(void)opaque;
  (void)total;(void)status;(void)cancel;(void)final;(void)cross;
  (void)stall_ms;(void)bytes;
  assert(!"webOS DV must not enter the Samsung streaming reader");
  return -1;
}
static void check(DtsEngine *e,int r) {
  if (r<0) { fprintf(stderr,"%s\n",dts_engine_error(e)); abort(); }
}
static uint8_t mapping_expected[8192];
static size_t mapping_expected_size;
static void mapping_expected_load(void) {
  mapping_expected_size=0;const char *path=getenv("NV_DV7_EXPECT_RPU");
  if(path) {FILE *f=fopen(path,"rb");assert(f);mapping_expected_size=fread(mapping_expected,1,sizeof mapping_expected,f);fclose(f);assert(mapping_expected_size>2);}
}
static DtsEngine *mapping_engine_create(void) {
  DtsEngine *e=dts_engine_create();assert(e);
  dts_engine_p7_mapping(e,getenv("NV_TEST_P7_MAPPING")!=NULL);
  return e;
}
static int drain(DtsEngine *e) {
  DtsFrame f; int r,video=0,audio=0,rpu=0;
  int64_t last=INT64_MIN;
  while ((r=dts_engine_next(e,&f))>0) {
    if (f.kind==DTS_AUDIO) { audio++; assert(f.size>0); continue; }
    if (f.kind!=DTS_VIDEO) continue;
    video++; assert(f.pts_ns>=last); last=f.pts_ns;
    size_t pre,at=nv_dv7_start(f.data,f.size,0,&pre);
    while (at<f.size) {
      size_t begin=at+pre,nextpre,end=nv_dv7_start(f.data,f.size,begin,&nextpre);
      assert(end-begin>=2);
      int type=(f.data[begin]>>1)&63;
      assert(type!=63 && !(f.data[begin]&1) && !(f.data[begin+1]>>3));
      if (type==62) {
        rpu++;
        if(mapping_expected_size)assert(end-begin==mapping_expected_size && !memcmp(f.data+begin,mapping_expected,mapping_expected_size));
      }
      at=end; pre=nextpre;
    }
  }
  check(e,r);
  assert(video && audio && rpu==video);
  return video;
}
int main(int argc,char **argv) {
  assert(argc==3);
  mapping_expected_load();
  DtsEngine *e=mapping_engine_create();
  dts_engine_allow_p7(e,1); dts_engine_dv(e,1); check(e,dts_engine_open(e,argv[1],"",-1,0,0));
  const DtsMediaInfo *m=dts_engine_info(e);
  assert(m->dovi_profile==8 && !m->dovi_el_present && m->dovi_rpu_present &&
         m->dovi_bl_present && m->dovi_bl_compatibility_id==1);
  assert(!strcmp(m->audio_codec,"eac3"));
  assert(drain(e)==20);
  check(e,dts_engine_seek(e,.4)); assert(drain(e)>0);
  check(e,dts_engine_seek(e,0)); assert(drain(e)==20);
  dts_engine_destroy(e);
  e=mapping_engine_create(); dts_engine_allow_p7(e,1); dts_engine_dv(e,1);
  check(e,dts_engine_open(e,argv[2],"",-1,0,0));
  DtsFrame f; int r;
  while ((r=dts_engine_next(e,&f))>0) {}
  assert(r<0 && strstr(dts_engine_error(e),"DV7 RPU conversion failed"));
  dts_engine_destroy(e);
  /* Real TrueHD 5.1 packets, fixed output across EAC3/TrueHD/AC3, and seek.
   * Reopen is this branch's existing audio-switch API. */
  for (int fixed=0; fixed<=2; fixed++) {
    e=mapping_engine_create(); dts_engine_allow_p7(e,1); dts_engine_dv(e,1);
    dts_engine_audio_route(e,1,fixed,0);
    for (int track=1; track<=3; track++) {
      check(e,dts_engine_open(e,argv[1],"",track,1,0));
      m=dts_engine_info(e);
      const char *expected=fixed==2?"ac3":fixed || track==2?"aac":track==1?"eac3":"ac3";
      assert(!strcmp(m->audio_codec,expected));
      assert(m->channels==(fixed==2?6:2));
      assert(drain(e)==20 && !dts_engine_has_core(e));
      check(e,dts_engine_seek(e,.4)); assert(drain(e)>0);
    }
    /* Off forbids TrueHD even after a previous successful conversion. */
    dts_engine_audio_route(e,1,fixed,1);
    assert(dts_engine_open(e,argv[1],"",2,0,0)<0);
    dts_engine_destroy(e);
  }
  /* DTS: converted by default; with the TV declaring DTS ("Original"), the DV
   * path passes the stream untouched (native, not converted); Off rejects. */
  e=mapping_engine_create(); dts_engine_allow_p7(e,1); dts_engine_dv(e,1);
  dts_engine_audio_route(e,1,0,0);
  check(e,dts_engine_open(e,argv[1],"",4,1,0));
  assert(!strcmp(dts_engine_info(e)->audio_codec,"aac") && drain(e)==20);
  dts_engine_audio_route(e,1,0,NV_DTS_ROUTE_NATIVE);
  check(e,dts_engine_open(e,argv[1],"",4,1,0));
  m=dts_engine_info(e);
  assert(!strcmp(m->audio_codec,"dts") && m->channels==6);
  { DtsFrame f; int audio=0,sync=1;
    while ((r=dts_engine_next(e,&f))>0) if (f.kind==DTS_AUDIO) {
      audio++; sync=sync && f.size>=4 && f.data[0]==0x7f && f.data[1]==0xfe && f.data[2]==0x80 && f.data[3]==0x01; }
    assert(audio>5 && sync); }
  check(e,dts_engine_seek(e,.4)); assert(drain(e)>0);
  /* Fixed DV audio wins over native DTS. */
  dts_engine_audio_route(e,1,2,NV_DTS_ROUTE_NATIVE);
  check(e,dts_engine_open(e,argv[1],"",4,1,0)); assert(!strcmp(dts_engine_info(e)->audio_codec,"ac3"));
  dts_engine_audio_route(e,1,0,1);
  assert(dts_engine_open(e,argv[1],"",4,0,0)<0);
  dts_engine_destroy(e);
  puts("DV7 engine: real TrueHD/fixed audio, Off, native/converted DTS, P8.1 RPU, seek and corrupt-RPU fallback passed.");
}
