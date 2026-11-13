/* Real FFmpeg contexts and packets; reuse the authenticated file-range fixture. */
#define NV_DTS_FFMPEG
#include "../src/dts/dts_engine.c"
#define main original_engine_fixture
#include "dts_engine.c"
#undef main

static void drain(DtsEngine *e, const char *dir, int pass, double target, int stream) {
  char name[2048];snprintf(name,sizeof name,"%s/%d.aac",dir,pass);
  FILE *out=fopen(name,"wb");assert(out);
  DtsFrame f;int r,audio=0,video=0;int64_t previous=INT64_MIN;
  while((r=dts_engine_next(e,&f))>0) {
    if(f.kind==DTS_VIDEO) { video++;continue; }
    if(f.kind!=DTS_AUDIO) continue;
    assert(f.stream_index==stream && f.pts_ns>=previous);
    assert(f.pts_ns+f.duration_ns>(int64_t)(target*1e9));
    if(!audio) assert(f.pts_ns<(int64_t)((target+0.06)*1e9));
    previous=f.pts_ns;audio++;
    if(!strcmp(e->info.audio_codec,"aac")) { aac_adts(out,&f);assert(fwrite(f.data,1,f.size,out)==(size_t)f.size); }
  }
  check(e,r);fclose(out);assert(audio>20 && video>10);
}
int main(int argc,char **argv) {
  assert(argc==3);DtsEngine *e=dts_engine_create();assert(e);
  dts_engine_set_http_workers(4);dts_engine_dv(e,1);
  check(e,dts_engine_open(e,argv[1],HEADERS,1,0,0));
  AVFormatContext *fmt=e->fmt;AVIOContext *io=e->io;
  drain(e,argv[2],0,0,1);
  unsigned downloaded=__atomic_load_n(&requests,__ATOMIC_RELAXED);
  /* Forward/backward/zero and EOF -> track switch reuse actual contexts and
   * cached bytes. Different tones prove old decoder/FIFO output is gone. */
  const int streams[]={2,1,2,1,3,4,1};
  const double targets[]={1.2,0,2.1,0.15,1,0,0.5};
  for(unsigned i=0;i<sizeof streams/sizeof *streams;i++) {
    check(e,dts_engine_audio(e,streams[i],0,targets[i]));
    assert(e->fmt==fmt && e->io==io && e->info.audio_stream==streams[i]);
    assert(!strcmp(e->info.audio_codec,streams[i]==3?"ac3":streams[i]==4?"eac3":"aac"));
    drain(e,argv[2],(int)i+1,targets[i],streams[i]);
    assert(__atomic_load_n(&requests,__ATOMIC_RELAXED)==downloaded);
  }
  assert(dts_engine_audio(e,0,0,0)<0 && e->info.audio_stream==1);
  assert(dts_engine_audio(e,99,0,0)<0 && dts_engine_audio(e,1,0,NAN)<0);
  check(e,dts_engine_seek(e,0));drain(e,argv[2],8,0,1);
  dts_engine_cancel(e);assert(dts_engine_audio(e,2,0,0)<0);
  dts_engine_destroy(e);
  e=dts_engine_create();dts_engine_dv(e,0);
  check(e,dts_engine_open(e,argv[1],HEADERS,1,0,0));
  assert(dts_engine_audio(e,3,0,0)<0); /* Passthrough stays DV-only. */
  dts_engine_destroy(e);
  puts("Real FFmpeg: seek/track/EOF reuse, cached bytes, DTS/AAC and AC3/EAC3 transitions, cancel and validation passed.");
  return 0;
}
