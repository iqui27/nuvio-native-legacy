/* Actual DTS pipeline fixture driver; local file transport models strict 206 IO. */
#include "../src/dts/dts_engine.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef DTS_ENGINE_HTTP
#include <pthread.h>
#include <unistd.h>
#include <time.h>
static void *cancel_later(void *opaque) { usleep(200000); dts_engine_cancel(opaque); return NULL; }
#endif
static const char *HEADERS="X-Fixture: yes\nAuthorization: Bearer engine-test";
static unsigned requests;
#ifndef DTS_ENGINE_HTTP
char *rede_baixar_trecho64_cab(const char *,const char *,int64_t,int64_t,long *,int64_t *,int *,volatile int *);
/* The Dolby Vision path asks for the final address; the fixture has none. */
char *rede_baixar_trecho64_final(const char *url,const char *headers,int64_t start,int64_t end,long *size,int64_t *total,int *status,volatile int *cancelled,char *final,int *cross) {
  if(final) final[0]=0;
  if(cross) *cross=0;
  return rede_baixar_trecho64_cab(url,headers,start,end,size,total,status,cancelled);
}
char *rede_baixar_trecho64_cab(const char *url,const char *headers,int64_t start,int64_t end,long *size,int64_t *total,int *status,volatile int *cancelled) {
  assert(!strcmp(headers,HEADERS));
  assert(start>=0 && end>=start && end-start<1024*1024); __atomic_add_fetch(&requests,1,__ATOMIC_RELAXED);
  *size=0; *status=0; *total=-1;
  if(__atomic_load_n(cancelled,__ATOMIC_RELAXED)) return NULL;
  FILE *f=fopen(url,"rb"); if(!f) return NULL;
  assert(!fseeko(f,0,SEEK_END)); *total=ftello(f);
  if(start>=*total) { *status=416; fclose(f); return NULL; }
  assert(!fseeko(f,start,SEEK_SET));
  long n=(long)(end-start+1); if(n>*total-start)n=(long)(*total-start);
  char *data=malloc(n); assert(data); *size=fread(data,1,n,f); fclose(f); *status=206; return data;
}
#endif
static void check(DtsEngine *e,int result) { if(result<0) { fprintf(stderr,"%s\n",dts_engine_error(e)); exit(1); } }
static void aac_adts(FILE *out,const DtsFrame *f) {
  unsigned n=f->size+7; unsigned char h[7]={0xff,0xf1,0x4c,0x80,0,0x1f,0xfc};
  /* AAC-LC 48 kHz stereo, no CRC. */
  h[3]|=(n>>11)&3; h[4]=(n>>3)&255; h[5]|=(n&7)<<5;
  assert(fwrite(h,1,7,out)==7);
}
int main(int argc,char **argv) {
  assert(argc==3);
  DtsEngine *e=dts_engine_create(); assert(e && dts_engine_available());
  check(e,dts_engine_open(e,argv[1],HEADERS,-1,0,0));
  const DtsMediaInfo *info=dts_engine_info(e);
  assert(info->channels==2); assert(info->sample_rate==48000);
  assert(info->n_tracks==4 && info->audio_stream==1 && info->video_stream==0);
  assert(!strcmp(info->tracks[1].language,"eng") && !strcmp(info->tracks[2].language,"por"));
  assert(dts_engine_subtitle_parameters(e,3)); assert(!dts_engine_subtitle_parameters(e,0));
  FILE *audio=fopen(argv[2],"wb"); assert(audio);
  char video_path[2048]; snprintf(video_path,sizeof(video_path),"%s.video",argv[2]);
  FILE *video=fopen(video_path,"wb"); assert(video);
  int counts[4]={0},r; int64_t last=INT64_MIN,finish=0; DtsFrame f;
  while((r=dts_engine_next(e,&f))>0) {
    assert(f.size>0); counts[f.kind]++;
    if(f.kind==DTS_VIDEO) {
      assert(fwrite(f.data,1,f.size,video)==f.size);
      /* These generated no-B-frame fixtures use the unchanged 25fps timeline. */
      assert(f.pts_ns==(int64_t)(counts[DTS_VIDEO]-1)*40000000);
      assert((f.dts_ns==f.pts_ns || f.dts_ns==INT64_MIN) && f.duration_ns==40000000);
    }
    if(f.kind==DTS_AUDIO) {
      assert(f.pts_ns>=last);
      if(last!=INT64_MIN) {
        int64_t step=f.pts_ns-last;
        assert(step>=21333333 && step<=21333334);
      }
      assert(f.duration_ns>=21333333 && f.duration_ns<=21333334);
      last=f.pts_ns; finish=f.pts_ns+f.duration_ns;
      aac_adts(audio,&f);
      assert(fwrite(f.data,1,f.size,audio)==f.size);
    } else if(f.kind==DTS_SUBTITLE) { assert(f.stream_index==3); assert(f.pts_ns==500000000LL || (f.size==2 && f.data[0]==0 && f.data[1]==0)); }
    else if(!strcmp(info->video_codec,"h264") || !strcmp(info->video_codec,"hevc")) assert(f.kind==DTS_VIDEO && f.size>=4 && f.data[0]==0 && f.data[1]==0 && (f.data[2]==1 || (f.data[2]==0 && f.data[3]==1)));
  }
  check(e,r); fclose(audio); fclose(video); assert(dts_engine_has_core(e));
  DtsEngineMetrics metrics; dts_engine_metrics(e,&metrics);
  assert(metrics.range_requests && metrics.range_bytes && metrics.range_wait_ns);
#ifdef DTS_ENGINE_HTTP
  if(strstr(argv[1],"/slow/")) {
    assert(metrics.range_short_reads>0);
    assert(metrics.range_max_wait_ns>=40000000);
    assert(metrics.range_wait_ns>=metrics.range_requests*UINT64_C(40000000));
    fprintf(stderr,"Delayed256KiB ranges: requests=%llu bytes=%llu waitMs=%llu maxWaitMs=%llu shortReads=%llu\n",
            (unsigned long long)metrics.range_requests,(unsigned long long)metrics.range_bytes,
            (unsigned long long)(metrics.range_wait_ns/1000000),(unsigned long long)(metrics.range_max_wait_ns/1000000),
            (unsigned long long)metrics.range_short_reads);
  }
#endif
  assert(counts[DTS_VIDEO]==100 && counts[DTS_AUDIO]>100 && counts[DTS_SUBTITLE]>0);
  assert(finish>3900000000LL && finish<4100000000LL);
  for(int i=0;i<3;i++) {
    double target=i==1?0.15:2.25; check(e,dts_engine_seek(e,target)); assert(dts_engine_has_core(e));
    int audio_frames=0;
    while((r=dts_engine_next(e,&f))>0) {
      if(f.kind==DTS_AUDIO) {
        assert(f.pts_ns+f.duration_ns>(int64_t)(target*1e9));
        if(!audio_frames) assert(f.pts_ns<(int64_t)((target+0.05)*1e9));
        audio_frames++;
      }
    }
    check(e,r); assert(audio_frames>20);
  }
  /* AC3 5.1 mode (#313): 6 channels, "ac3", 640 kbps frames of 1536 samples = 2560 bytes / 32 ms, seekable. */
  dts_engine_set_ac3(1);
  check(e,dts_engine_open(e,argv[1],HEADERS,-1,0,0));
  info=dts_engine_info(e);
  assert(info->channels==6 && info->sample_rate==48000 && !strcmp(info->audio_codec,"ac3"));
  { int n=0; int64_t prev=INT64_MIN;
    while((r=dts_engine_next(e,&f))>0) if(f.kind==DTS_AUDIO) {
      assert(f.size==2560 && f.data[0]==0x0b && f.data[1]==0x77);   /* AC3 sync word */
      if(prev!=INT64_MIN) assert(f.pts_ns-prev==32000000);
      prev=f.pts_ns; n++;
    }
    check(e,r); assert(n>100); }
  check(e,dts_engine_seek(e,2.0));
  { int n=0; while((r=dts_engine_next(e,&f))>0) if(f.kind==DTS_AUDIO) { assert(f.size==2560); n++; } check(e,r); assert(n>30); }
  dts_engine_set_ac3(0);
  /* Exact selected track, metadata retention, cancel, and invalid-track refusal. */
  check(e,dts_engine_open(e,argv[1],HEADERS,2,1,0));
  assert(dts_engine_info(e)->audio_stream==2); assert(!dts_engine_has_core(e));
  dts_engine_metrics(e,&metrics); assert(metrics.range_requests && metrics.range_bytes);
  while(!dts_engine_has_core(e) && (r=dts_engine_next(e,&f))>0) {}
  check(e,r); assert(dts_engine_has_core(e));
  dts_engine_cancel(e); assert(dts_engine_next(e,&f)==-1); dts_engine_destroy(e);
  e=dts_engine_create(); assert(dts_engine_open(e,argv[1],HEADERS,0,0,0)==-1); dts_engine_destroy(e);
  #ifdef DTS_ENGINE_HTTP
  e=dts_engine_create(); assert(dts_engine_open(e,argv[1],"X-Fixture: yes",-1,0,0)==-1); dts_engine_destroy(e);
  char idle[1024]; snprintf(idle,sizeof idle,"%s",argv[1]); char *slash=strchr(idle+7,'/'); assert(slash); strcpy(slash,"/idle");
  e=dts_engine_create(); pthread_t thread;
  struct timespec before,after; clock_gettime(CLOCK_MONOTONIC,&before);
  assert(!pthread_create(&thread,NULL,cancel_later,e));
  assert(dts_engine_open(e,idle,HEADERS,-1,0,0)==-1);
  assert(!pthread_join(thread,NULL)); clock_gettime(CLOCK_MONOTONIC,&after);
  assert((after.tv_sec-before.tv_sec)*1e9+after.tv_nsec-before.tv_nsec<2500000000.0);
  dts_engine_destroy(e);
  #else
  assert(requests>0);
  #endif
  fprintf(stderr,"DTS AAC stereo: video=%d audio=%d requests=%u, repeated seeks/cancel/track selection OK\n",counts[1],counts[2],requests);
  return 0;
}
