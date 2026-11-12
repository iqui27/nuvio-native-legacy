/* Real engine path: dump the whole Annex-B video the webOS adapter receives. */
#define main existing_engine_fixture_main
#include "dts_engine.c"
#undef main
int main(int argc,char **argv) {
  assert(argc==3);DtsEngine *e=dts_engine_create();assert(e && dts_engine_available());
  FILE *out=fopen(argv[2],"wb");assert(out);
  check(e,dts_engine_open(e,argv[1],HEADERS,-1,0,0));
  unsigned video=0;
  for(;;) {
    DtsFrame f={0};int result=dts_engine_next(e,&f);check(e,result);
    if(!result)break;
    if(f.kind==DTS_VIDEO) { assert(fwrite(f.data,1,f.size,out)==f.size);video++; }
  }
  assert(!fclose(out));dts_engine_destroy(e);
  printf("[fixture] video=%u\n",video);
  return 0;
}
