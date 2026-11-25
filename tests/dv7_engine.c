/* Actual FFmpeg demux/Annex-B/seek path, with a local Range transport. */
#include "../src/dts/dts_engine.h"
#include "../src/dts/dv7.h"
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
char *rede_baixar_trecho64_final(const char *u,const char *h,int64_t s,int64_t e,long *n,
                               int64_t *t,int *status,volatile int *c,char *final,int *cross) {
  if(final) *final=0;
  if(cross) *cross=0;
  return rede_baixar_trecho64_cab(u,h,s,e,n,t,status,c);
}
char *rede_baixar_trecho64_relato(const char *u,const char *h,int64_t s,int64_t e,long *n,
 int64_t *t,int *status,volatile int *c,char *f,int *x,volatile int *urgent,unsigned idle,int *stalled,int *retry_after) {
 (void)urgent;(void)idle;if(stalled)*stalled=0;if(retry_after)*retry_after=-1;
 return rede_baixar_trecho64_final(u,h,s,e,n,t,status,c,f,x);
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
static void check_dv(DtsEngine *e) {
  const DtsMediaInfo *m=dts_engine_info(e);
  assert(m->dovi_profile==8 && !m->dovi_el_present && m->dovi_rpu_present &&
         m->dovi_bl_present && m->dovi_bl_compatibility_id==1);
  assert(!strcmp(m->hdr,"DolbyVision"));
}
static uint8_t mapping_expected[8192];
static size_t mapping_expected_size;
static void mapping_expected_load(void) {
  mapping_expected_size=0;const char *path=getenv("NV_DV7_EXPECT_RPU");
  if(path) {FILE *f=fopen(path,"rb");assert(f);mapping_expected_size=fread(mapping_expected,1,sizeof mapping_expected,f);fclose(f);assert(mapping_expected_size>2);}
}
static DtsEngine *mapping_engine_create(void) {
  DtsEngine *e=dts_engine_create();assert(e);
#ifdef NV_DV7_MAPPING_TEST
  dts_engine_p7_mapping(e,getenv("NV_TEST_P7_MAPPING")!=NULL);
#endif
  return e;
}
static int drain(DtsEngine *e) {
  check_dv(e);
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
  const char *workers=getenv("NV_TEST_HTTP_WORKERS");
  if(workers)dts_engine_set_http_workers(atoi(workers));
  assert(argc==4);mapping_expected_load();
  DtsEngine *e=mapping_engine_create(); assert(e);
  dts_engine_allow_p7(e,1); dts_engine_dv(e,1); check(e,dts_engine_open(e,argv[1],"",-1,0,0));
  check_dv(e);
  assert(!strcmp(dts_engine_info(e)->audio_codec,"eac3"));
  assert(drain(e)==20);
  check(e,dts_engine_seek(e,.4)); assert(drain(e)>0);
  check(e,dts_engine_seek(e,0)); assert(drain(e)==20);
  /* The same container, AVIO cache and DV7 state survive both passthrough
   * and DTS transcoder switches, including return to the original track. */
  check(e,dts_engine_audio(e,2,0,.4));assert(drain(e)>0);
  check(e,dts_engine_audio(e,3,0,0));
  assert(!strcmp(dts_engine_info(e)->audio_codec,"aac"));assert(drain(e)==20);
  check(e,dts_engine_seek(e,.4));assert(drain(e)>0);
  check(e,dts_engine_audio(e,1,0,0));
  assert(!strcmp(dts_engine_info(e)->audio_codec,"eac3"));assert(drain(e)==20);
#ifdef NV_DV_TRUEHD_TEST
  // Actual TrueHD 5.1 packets decoded/re-encoded, with converted P8.1 RPU
  // on every picture before and after seeks and switches back to EAC3.
  for(int ac3=0;ac3<=1;ac3++) {
    dts_engine_set_ac3(ac3);
    check(e,dts_engine_audio(e,4,1,0)); // DTS core flag must not affect TrueHD.
    assert(!strcmp(dts_engine_info(e)->audio_codec,ac3?"ac3":"aac"));
    assert(dts_engine_info(e)->channels==(ac3?6:2));
    assert(!strcmp(dts_engine_info(e)->tracks[4].codec,"truehd"));
    assert(drain(e)==20 && !dts_engine_has_core(e));
    check(e,dts_engine_seek(e,.4));assert(drain(e)>0);
    check(e,dts_engine_audio(e,1,0,0));assert(drain(e)==20);
  }
  dts_engine_set_ac3(0);
  check(e,dts_engine_open(e,argv[1],"",4,0,0));check_dv(e);
  assert(!strcmp(dts_engine_info(e)->audio_codec,"aac") && drain(e)==20);
  puts("DV7 TrueHD: real 5.1 decode to AAC/AC3, seek, EAC3 return and initial selection retain P8.1 RPU; no DTS core fallback");
#endif
  dts_engine_destroy(e);
#ifdef NV_DV_FIXED_AUDIO_TEST
  // Real AC3 2.0/5.1, EAC3 2.0, DTS and TrueHD 5.1 all produce identical
  // output parameters from the FIRST Load. Still require P8.1 RPU on every
  // picture, decoded audio, correct track metadata, seek and bounded cache.
  for(int mode=1;mode<=2;mode++) {
    e=mapping_engine_create();assert(e);
    dts_engine_allow_p7(e,1);dts_engine_dv(e,1);dts_engine_dv_audio(e,mode);
    dts_engine_set_ahead_mib(128);
    dts_engine_set_ac3(mode==1); // Fixed mode is independent of DTS preference.
    check(e,dts_engine_open(e,argv[1],"",7,0,0));
    for(int pass=0;pass<2;pass++) for(int track=1;track<=7;track++) {
      check(e,dts_engine_audio(e,track,0,0));
      const DtsMediaInfo *m=dts_engine_info(e);
      assert(!strcmp(m->audio_codec,mode==2?"ac3":"aac"));
      assert(m->channels==(mode==2?6:2) && m->sample_rate==48000);
      assert(m->audio_stream==track && m->n_tracks==8);
      assert(!strcmp(m->tracks[5].codec,"ac3") && m->tracks[5].channels==6);
      assert(!strcmp(m->tracks[6].codec,"ac3") && m->tracks[6].channels==2);
      assert(!strcmp(m->tracks[7].codec,"ac3") && m->tracks[7].channels==1);
      assert(drain(e)==20);check(e,dts_engine_seek(e,.4));assert(drain(e)>0);
      DtsBufferInfo b={0};dts_engine_buffer_info(e,&b);
      // v12 counts the active 4 MiB range as well as the 128 MiB queue.
      assert(b.capacity_bytes==33*(4*1048576LL-1) && b.ready_bytes<=b.capacity_bytes);
    }
    dts_engine_cancel(e);DtsFrame cancelled;
    assert(dts_engine_next(e,&cancelled)<0);dts_engine_destroy(e);
  }
  // Setter cannot enable conversion on a non-DV path, and invalid mode is off.
  dts_engine_set_ac3(0);
  e=mapping_engine_create();dts_engine_allow_p7(e,1);dts_engine_dv_audio(e,2);
  check(e,dts_engine_open(e,argv[1],"",3,0,0));
  assert(!strcmp(dts_engine_info(e)->audio_codec,"aac") && dts_engine_info(e)->channels==2);
  dts_engine_destroy(e);
  e=mapping_engine_create();dts_engine_dv(e,1);dts_engine_allow_p7(e,1);dts_engine_dv_audio(e,99);
  check(e,dts_engine_open(e,argv[1],"",1,0,0));
  assert(!strcmp(dts_engine_info(e)->audio_codec,"eac3") && drain(e)==20);
  dts_engine_destroy(e);dts_engine_set_ac3(0);dts_engine_set_ahead_mib(0);
  puts("Fixed DV audio: real AC3/EAC3/DTS/TrueHD, stereo/surround, repeated switching/seeking, unchanged bounded cache, opt-in and cancellation passed");
#endif
  e=mapping_engine_create(); dts_engine_allow_p7(e,1); dts_engine_dv(e,1);
  check(e,dts_engine_open(e,argv[2],"",-1,0,0));
  DtsFrame f; int r;
  while ((r=dts_engine_next(e,&f))>0) {}
  assert(r<0 && strstr(dts_engine_error(e),"DV7 RPU conversion failed"));
  dts_engine_destroy(e);
  e=mapping_engine_create();assert(e);dts_engine_dv(e,1);
  mapping_expected_size=0;
  dts_engine_allow_p7(e,0); // Native MP4 P8 requires no P7 conversion.
  check(e,dts_engine_open(e,argv[3],"",-1,0,0));check_dv(e);
  assert(drain(e)==20);check(e,dts_engine_seek(e,.4));assert(drain(e)>0);
  check(e,dts_engine_seek(e,0));assert(drain(e)==20);dts_engine_destroy(e);
  puts("DV7 engine: actual MKV + FFmpeg + P8.1 output, audio passthrough/DTS switching, repeated seek with retained DV metadata corrupt-RPU fallback and native P8 MP4 passthrough/seek passed.");
  return 0;
}
