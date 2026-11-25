/* Re-run the existing real MKV/P8 engine suite with the MP4 test compiled in,
 * then exercise actual dual-track MP4 through the same authenticated AVIO. */
#define main mkv_baseline_main
#include "dv7_engine.c"
#undef main

static int drain_split(DtsEngine *e,const char *rpu_path,FILE *pictures) {
  FILE *r=fopen(rpu_path,"rb");assert(r);uint8_t expected[8192];
  size_t expected_size=fread(expected,1,sizeof expected,r);fclose(r);
  assert(expected_size>2);check_dv(e);
  DtsFrame f;int result,videos=0,audios=0;int64_t last=INT64_MIN;
  while((result=dts_engine_next(e,&f))>0) {
    if(f.kind==DTS_AUDIO) {audios++;continue;}
    if(f.kind!=DTS_VIDEO)continue;
    assert(f.dts_ns>=last);last=f.dts_ns;videos++;
    int rpus=0;size_t pre,at=nv_dv7_start(f.data,f.size,0,&pre);
    while(at<(size_t)f.size) {
      size_t begin=at+pre,nextpre,end=nv_dv7_start(f.data,f.size,begin,&nextpre);
      assert(end-begin>=2);int type=(f.data[begin]>>1)&63;
      assert(type!=63 && !(f.data[begin]&1) && !(f.data[begin+1]>>3));
      if(type==62) {assert(end-begin==expected_size && !memcmp(f.data+begin,expected,expected_size));rpus++;}
      at=end;pre=nextpre;
    }
    assert(rpus==1);if(pictures)assert(fwrite(f.data,1,f.size,pictures)==(size_t)f.size);
  }
  check(e,result);assert(videos>0 && audios>0);return videos;
}
#ifndef MP4_BD_MAIN
#define MP4_BD_MAIN main
#endif
int MP4_BD_MAIN(int argc,char **argv) {
  assert(argc==17);
  char *baseline[]={argv[0],argv[1],argv[2],argv[3]};
  if(!getenv("NV_MP4_ONLY"))assert(!mkv_baseline_main(4,baseline));
  for(int mode=0;mode<=2;mode++) {
    DtsEngine *e=mapping_engine_create();assert(e);
    dts_engine_dv(e,1);dts_engine_allow_p7(e,1);dts_engine_dv_audio(e,mode);
    dts_engine_mp4_test(e,1);dts_engine_set_ahead_mib(64);
    fprintf(stderr,"MP4 fixture: mode=%d initial open/drain\n",mode);
    check(e,dts_engine_open(e,argv[4],"",-1,0,0));check_dv(e);
    assert(dts_engine_info(e)->video_stream==0);
    DtsBufferInfo b;dts_engine_buffer_info(e,&b);
    assert(b.capacity_bytes==13LL*(4*1024*1024-1));
    FILE *out=mode==0?fopen(argv[11],"wb"):NULL;if(mode==0)assert(out);
    assert(drain_split(e,argv[9],out)==20);if(out)fclose(out);
    for(int i=0;i<3;i++) {
      fprintf(stderr,"MP4 fixture: mode=%d seek/audio cycle=%d\n",mode,i);
      check(e,dts_engine_seek(e,.4));assert(drain_split(e,argv[9],NULL)>0);
      check(e,dts_engine_seek(e,0));assert(drain_split(e,argv[9],NULL)==20);
      check(e,dts_engine_audio(e,3,0,.4));assert(drain_split(e,argv[9],NULL)>0);
      check(e,dts_engine_audio(e,2,0,0));assert(drain_split(e,argv[9],NULL)==20);
    }
    // P8 MP4 and MKV are not candidates for the explicit dual-track open.
    assert(dts_engine_open(e,argv[3],"",-1,0,0)<0);
    assert(dts_engine_open(e,argv[1],"",-1,0,0)<0);
    // Every malformed pair fails instead of silently presenting HDR10 as DV.
    for(int bad=5;bad<=7;bad++) {
      int result=dts_engine_open(e,argv[bad],"",-1,0,0);
      if(result==0) {DtsFrame f;int n=0;while((result=dts_engine_next(e,&f))>0 && ++n<1000){} }
      assert(result<0);
    }
    check(e,dts_engine_open(e,argv[8],"",-1,0,0));
    assert(dts_engine_info(e)->video_stream==1);
    assert(drain_split(e,argv[9],NULL)==20);
    check(e,dts_engine_open(e,argv[10],"",-1,0,0));
    assert(drain_split(e,argv[9],NULL)==20);
    check(e,dts_engine_seek(e,.4));assert(drain_split(e,argv[9],NULL)>0);
    // Reproduce the TV failure: empty hvcC arrays and no container colr.
    // The minimal FFmpeg parser cannot publish colour fields from the SPS.
    for(int variant=12;variant<=13;variant++) {
      check(e,dts_engine_open(e,argv[variant],"",-1,0,0));
      const DtsMediaInfo *m=dts_engine_info(e);
      assert(m->color_primaries==9 && m->color_transfer==16 && m->color_matrix==9);
      FILE *recovered=mode==0 && variant==12?fopen(argv[11],"wb"):NULL;
      if(mode==0 && variant==12)assert(recovered);
      assert(drain_split(e,argv[9],recovered)==20);if(recovered)fclose(recovered);
      check(e,dts_engine_seek(e,.4));assert(drain_split(e,argv[9],NULL)>0);
      check(e,dts_engine_audio(e,3,0,0));assert(drain_split(e,argv[9],NULL)==20);
      check(e,dts_engine_open(e,argv[variant],"",-1,0,.4));assert(drain_split(e,argv[9],NULL)>0);
    }
    // Unknown colour is not proof of HDR10: SPS must actually confirm it.
    assert(dts_engine_open(e,argv[14],"",-1,0,0)<0);
    assert(strstr(dts_engine_error(e),"SPS missing or invalid"));
    assert(dts_engine_open(e,argv[15],"",-1,0,0)<0);
    assert(strstr(dts_engine_error(e),"not HDR10"));
    assert(dts_engine_open(e,argv[16],"",-1,0,0)<0);
    assert(strstr(dts_engine_error(e),"colour conflict"));
    // Turning the per-engine experiment off restores the baseline path.
    dts_engine_mp4_test(e,0);check(e,dts_engine_open(e,argv[4],"",-1,0,0));
    assert(!dts_engine_info(e)->dovi_profile);
    dts_engine_cancel(e);DtsFrame f;assert(dts_engine_next(e,&f)<0);dts_engine_destroy(e);
  }
  dts_engine_set_ahead_mib(0);
  puts("PASS real dual-track MEL/FEL MP4: P8.1 RPU on each base picture, in-band/hvcC SPS HDR10 recovery without colr, B-frames, seeks, audio changes, OFF baseline, MKV/P8 isolation, bounded cache, missing SPS, SDR refusal, corrupt/missing/mismatched RPU and cancellation");
  return 0;
}
