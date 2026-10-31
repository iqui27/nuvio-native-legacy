#define MP4_BD_MAIN split_baseline_main
#include "mp4_bd_engine.c"
#undef MP4_BD_MAIN
#include <libavformat/avformat.h>
#include <libavutil/dovi_meta.h>
/* A P8 recovery must not invoke P7 conversion even when built with libdovi. */
ptrdiff_t __wrap_nv_dv7_rpu(const uint8_t *p,size_t n,uint8_t *out,size_t cap) {
  (void)p;(void)n;(void)out;(void)cap;assert(!"P8 must retain its original metadata");return -1;
}
#ifdef NV_DV7_MAPPING_TEST
ptrdiff_t __wrap_nv_dv7_rpu_mapping(const uint8_t *p,size_t n,uint8_t *out,size_t cap,int mode) {
  (void)p;(void)n;(void)out;(void)cap;(void)mode;assert(!"P8 must not invoke experimental P7 conversion");return -1;
}
ptrdiff_t __wrap_nv_dv7_rpu_mapping_report(const uint8_t *p,size_t n,uint8_t *out,size_t cap,int mode,uint32_t *report) {
  (void)p;(void)n;(void)out;(void)cap;(void)mode;(void)report;assert(!"P8 must not invoke P7 diagnostic conversion");return -1;
}
#endif
int main(int argc,char **argv) {
  assert(argc==4);char path[4096],rpu[4096];
  snprintf(rpu,sizeof rpu,"%s/%s-output.nal",argv[1],argv[2]);
  const char *good[]={"p81","p81-inband-nocolr","p8-unknown","p8-zero-inband"};
  const char *bad[]={"p84","p8-compat2","p81-extra-video","p81-with-el","p81-no-bl",
    "p81-bad-rpu","p81-p7-rpu","p81-sdr","p5","dual","p8-zero-nosps",
    "p8-zero-sdr","p8-zero-hlg","p8-zero-colr-sdr","p8-zero-colr-hlg",
    "p8-zero-bad-rpu","p8-zero-p7-rpu","p8-zero-no-rpu","p8-zero-extra-video",
    "p8-zero-with-el","p8-zero-no-bl"};
  for(int mode=0;mode<=2;mode++)for(int mib=64;mib<=256;mib*=2) {
    DtsEngine *e=mapping_engine_create();assert(e);
    dts_engine_dv(e,1);dts_engine_allow_p7(e,0);dts_engine_mp4_test(e,1);
    dts_engine_dv_audio(e,mode);dts_engine_set_ahead_mib(mib);
    for(size_t i=0;i<sizeof good/sizeof good[0];i++) {
      snprintf(path,sizeof path,"%s/%s-%s.mp4",argv[1],argv[2],good[i]);
      check(e,dts_engine_open(e,path,"",-1,0,0));check_dv(e);
      AVFormatContext *fmt=dts_engine_avformat(e);
      const AVCodecParameters *params=fmt->streams[dts_engine_info(e)->video_stream]->codecpar;
      const AVPacketSideData *side=av_packet_side_data_get(params->coded_side_data,params->nb_coded_side_data,AV_PKT_DATA_DOVI_CONF);
      assert(side && side->size>=sizeof(AVDOVIDecoderConfigurationRecord));
      const AVDOVIDecoderConfigurationRecord *input=(const AVDOVIDecoderConfigurationRecord *)side->data;
      assert(input->dv_bl_signal_compatibility_id==(i>=2?0:1)); // Input remains untouched; only output signaling is corrected.
      DtsBufferInfo b;dts_engine_buffer_info(e,&b);
      assert(b.capacity_bytes==(mib/4-4+1)*(4*1024*1024LL-1));
      int capture=mode==0 && mib==64 && !strcmp(good[i],"p8-zero-inband");
      FILE *out=capture?fopen(argv[3],"wb"):NULL;
      if(capture)assert(out);
      assert(drain_split(e,rpu,out)==20);if(out)fclose(out);
      check(e,dts_engine_seek(e,.4));assert(drain_split(e,rpu,NULL)>0);
      check(e,dts_engine_seek(e,0));assert(drain_split(e,rpu,NULL)==20);
      check(e,dts_engine_audio(e,2,0,.4));assert(drain_split(e,rpu,NULL)>0);
      check(e,dts_engine_audio(e,1,0,0));assert(drain_split(e,rpu,NULL)==20);
      check(e,dts_engine_open(e,path,"",-1,0,.4));assert(drain_split(e,rpu,NULL)>0);
    }
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++) {
      snprintf(path,sizeof path,"%s/%s-%s.mp4",argv[1],argv[2],bad[i]);
      assert(dts_engine_open(e,path,"",-1,0,0)<0);
      if(strstr(bad[i],"colr-sdr") || strstr(bad[i],"colr-hlg"))
        assert(strstr(dts_engine_error(e),"does not confirm HDR10"));
    }
    snprintf(path,sizeof path,"%s/%s-p81.mp4",argv[1],argv[2]);
    dts_engine_mp4_test(e,0);check(e,dts_engine_open(e,path,"",-1,0,0));
    assert(drain_split(e,rpu,NULL)==20);
    // A proven correction must never leak into a later non-experimental open.
    snprintf(path,sizeof path,"%s/%s-p8-unknown.mp4",argv[1],argv[2]);
    check(e,dts_engine_open(e,path,"",-1,0,0));
    assert(dts_engine_info(e)->dovi_bl_compatibility_id==0);
    dts_engine_cancel(e);DtsFrame f;assert(dts_engine_next(e,&f)<0);dts_engine_destroy(e);
  }
  dts_engine_set_ahead_mib(0);
  puts("PASS verified P8.1 compat0/1: actual SPS proof, unchanged RPU, no P7 converter, presets64/128/256, seeks, resume, audio, OFF isolation; missing SPS, SDR/HLG including false HDR10 container tags, corrupt/missing/P7 RPU and unsupported layouts refused");
  return 0;
}
