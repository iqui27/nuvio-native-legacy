/* Reuse the strict local 206 transport, not an alternate demux implementation. */
#define main existing_engine_fixture_main
#include "dts_engine.c"
#undef main
int main(int argc,char **argv) {
  assert(argc==3);DtsEngine *e=dts_engine_create();assert(e && dts_engine_available());
  FILE *out=fopen(argv[2],"wb");assert(out);
  check(e,dts_engine_open(e,argv[1],HEADERS,-1,0,0));
  const double targets[]={0,3,1};
  for(unsigned epoch=0;epoch<3;epoch++) {
    if(epoch)check(e,dts_engine_seek(e,targets[epoch]));
    unsigned video=0,audio=0;
    while(video<120) {
      DtsFrame f={0};int result=dts_engine_next(e,&f);check(e,result);assert(result==1);
      if(f.kind==DTS_VIDEO || f.kind==DTS_AUDIO) {
        assert(fwrite(&f.kind,sizeof f.kind,1,out)==1);
        assert(fwrite(&f.size,sizeof f.size,1,out)==1);
        assert(fwrite(&f.pts_ns,sizeof f.pts_ns,1,out)==1);
        assert(fwrite(f.data,1,f.size,out)==f.size);
        video+=f.kind==DTS_VIDEO;audio+=f.kind==DTS_AUDIO;
      }
    }
    assert(audio>0);
  }
  assert(!fclose(out));dts_engine_destroy(e);
  puts("PASS real HEVC/DTS open, seek and return: 120 video packets per epoch");
}
