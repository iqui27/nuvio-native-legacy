/* Coarse EL seek index: over 64 obsolete RPUs precede the BL keyframe.
 * The original implementation fails to open/resume this valid MP4. */
#define MP4_BD_MAIN split_baseline_main
#include "mp4_bd_engine.c"
#undef MP4_BD_MAIN
int main(int argc,char **argv) {
  assert(argc==4);
  for(int mode=0;mode<=2;mode++) {
    DtsEngine *e=dts_engine_create();assert(e);
    dts_engine_dv(e,1);dts_engine_allow_p7(e,1);dts_engine_mp4_test(e,1);
    dts_engine_dv_audio(e,mode);dts_engine_set_ahead_mib(64);
    check(e,dts_engine_open(e,argv[1],"",-1,0,6.4));
    assert(drain_split(e,argv[2],NULL)>0);
    DtsBufferInfo b;dts_engine_buffer_info(e,&b);
    assert(b.capacity_bytes==13LL*(4*1024*1024-1));
    for(int i=0;i<3;i++) {
      check(e,dts_engine_seek(e,6.4));assert(drain_split(e,argv[2],NULL)>0);
      check(e,dts_engine_seek(e,3.2));assert(drain_split(e,argv[2],NULL)>0);
      check(e,dts_engine_seek(e,0));
      FILE *out=mode==0 && i==0?fopen(argv[3],"wb"):NULL;
      if(mode==0 && i==0)assert(out);
      assert(drain_split(e,argv[2],out)==200);if(out)fclose(out);
      check(e,dts_engine_audio(e,3,0,6.4));assert(drain_split(e,argv[2],NULL)>0);
      check(e,dts_engine_audio(e,2,0,3.2));assert(drain_split(e,argv[2],NULL)>0);
    }
    check(e,dts_engine_open(e,argv[1],"",-1,0,6.4));assert(drain_split(e,argv[2],NULL)>0);
    dts_engine_cancel(e);assert(dts_engine_seek(e,0)<0);dts_engine_destroy(e);
  }
  dts_engine_set_ahead_mib(0);
  puts("PASS coarse EL index: saved-position open, forward/back seek, reopen, original/AAC/AC3, exact RPU, monotonic DTS, unchanged RAM capacity and cancellation");
  return 0;
}
