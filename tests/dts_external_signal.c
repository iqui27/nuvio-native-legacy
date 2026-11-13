// Real matcher against the exact PCM beds used by dts_external_audio.sh.
// No FFmpeg: this proves the fixture's signal contract, not encoded playback.
#include "../src/dts/dts_match.inc"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static DtsMatchSig load(const char *dir, const char *name, float *env[4]) {
  char path[1024];snprintf(path,sizeof path,"%s/%s",dir,name);
  FILE *f=fopen(path,"rb");assert(f);
  unsigned char h[44];assert(fread(h,1,44,f)==44);
  assert(!memcmp(h,"RIFF",4) && !memcmp(h+8,"WAVEfmt ",8));
  assert(h[20]==1 && h[22]==1 && h[24]==0x40 && h[25]==0x1f && h[34]==16); // mono PCM 8 kHz
  assert(fseek(f,0,SEEK_END)==0);long count=(ftell(f)-44)/2;assert(count>0);
  assert(fseek(f,44,SEEK_SET)==0);
  float *pcm=malloc((size_t)count*sizeof *pcm);assert(pcm);
  for(long i=0;i<count;i++) {
    unsigned char b[2];assert(fread(b,1,2,f)==2);
    pcm[i]=(int16_t)((unsigned)b[0]|((unsigned)b[1]<<8))/32768.0f;
  }
  fclose(f);
  int cap=(int)(count/80)+1;
  for(int b=0;b<4;b++) { env[b]=calloc((size_t)cap,sizeof(float));assert(env[b]); }
  DtsMatchEnv v;dts_match_env_init(&v,8000,env,cap);dts_match_env_push(&v,pcm,(int)count);free(pcm);
  DtsMatchSig s={.n=v.n,.t0=0};
  for(int b=0;b<4;b++)s.env[b]=v.env[b];
  return s;
}
int main(int argc,char **argv) {
  assert(argc==2);float *em[4],*ed[4];
  DtsMatchSig m=load(argv[1],"y_ru.wav",em),d=load(argv[1],"y_pl.wav",ed);
  assert(m.n==12000 && d.n==15000);
  DtsMatchSig initial=m;initial.n=4000;
  DtsMatchResult r;assert(dts_match_search(&initial,&d,1,0,10,&r));
  assert(r.confident && fabs(r.offset_s)<0.06);
  // The same-cut mix keeps a peak here even through the long 72-112s dialogue.
  // Unbanded random noise labelled "speech" breaks this contract near 110s:
  // its unrelated low/high energy masks the shared effects in every band.
  for(int t=60;t<=120;t+=2) {
    DtsMatchSig window=m;int start=(t-60)*DTS_MATCH_HZ;
    window.n=60*DTS_MATCH_HZ;window.t0=t-60;
    for(int b=0;b<4;b++)window.env[b]=m.env[b]?m.env[b]+start:NULL;
    assert(dts_match_search_at(&window,&d,1,0,1.5,16,(window.n-900.0)/window.n,&r));
    if(fabs(r.offset_s)>=0.08 || r.score<0.05 || r.margin<0.02)
      fprintf(stderr,"same-cut fixture loses its shared bed at %ds: offset %.3f score %.3f margin %.3f\n",t,r.offset_s,r.score,r.margin);
    assert(fabs(r.offset_s)<0.08 && r.score>=0.05 && r.margin>=0.02);
  }
  for(int b=0;b<4;b++) { free(em[b]);free(ed[b]); }
  puts("PASS same-cut fixture: shared effects survive dubbed dialogue at the intended zero offset");
}
