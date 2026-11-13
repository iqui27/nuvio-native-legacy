#define NV_WEBOS 1
#include "video_audio_mkv.h"
#include <assert.h>

static VideoFaixa native(const char *codec,const char *lang,int ch,int number) {
  VideoFaixa f={0};f.numero=number;f.stream_index=f.stream_id=f.ordinalMkv=-1;
  f.canais=ch;snprintf(f.codec,sizeof f.codec,"%s",codec);snprintf(f.idioma,sizeof f.idioma,"%s",lang);
  snprintf(f.rotulo,sizeof f.rotulo,"native-%d",number);return f;
}
int main(void) {
  NvAudioMkvTrack file[]={
    {2,6,"A_DTS","en","Original DTS-ES Surround Mix"},
    {3,6,"A_DTS","en","2026 Surround Remix"},
    {4,2,"A_FLAC","en","Commentary one"},
    {5,2,"A_FLAC","en","Commentary two"},
    {6,2,"A_FLAC","en","Commentary three"},
    {7,2,"A_FLAC","en","Commentary four"}};
  VideoFaixa list[NV_FAIXA_MAX];int map[NV_FAIXA_MAX];
  for(int i=0;i<4;i++)list[i]=native("flac","en",2,i);
  int n=nv_audio_mkv_merge(list,4,NV_FAIXA_MAX,file,6,map);assert(n==6);
  for(int i=0;i<4;i++) {
    assert(list[i].numero==i && list[i].stream_id==i+4 && map[i+2]==i);
    char label[32];snprintf(label,sizeof label,"native-%d",i);assert(!strcmp(label,list[i].rotulo));
  }
  assert(map[0]==-1 && map[1]==-1);
  assert(list[4].numero==-1 && list[4].ordinalMkv==0 && list[4].stream_id==2);
  assert(list[5].numero==-1 && list[5].ordinalMkv==1 && list[5].stream_id==3);
  assert(!strcmp(list[4].nome,file[0].nome) && !strcmp(list[5].nome,file[1].nome));
  assert(strcmp(list[4].rotulo,list[5].rotulo));
  assert(video_audio_numero_memoria(&list[4])==(0x40000000|2));
  assert(video_audio_numero_memoria(&list[5])==(0x40000000|3));
  assert(nv_audio_mkv_merge(list,n,NV_FAIXA_MAX,file,6,map)==6); // no duplicates
  // A fresh sourceInfo must recover exactly the same rows/IDs.
  for(int i=0;i<4;i++)list[i]=native("flac","en",2,i);
  assert(nv_audio_mkv_merge(list,4,NV_FAIXA_MAX,file,6,map)==6);
  // Already working native lists (SDR, HDR10 or DV) remain in native order.
  for(int i=0;i<6;i++)list[i]=native(i<2?"dts":"flac","en",i<2?6:2,i);
  assert(nv_audio_mkv_merge(list,6,NV_FAIXA_MAX,file,6,map)==6);
  for(int i=0;i<6;i++)assert(list[i].numero==i && map[i]==i);
  // Do not guess which of two identical DTS tracks firmware exposed.
  list[0]=native("dts","en",6,0);
  assert(nv_audio_mkv_merge(list,1,NV_FAIXA_MAX,file,2,map)==1);
  // Unknown codec is not evidence that the file track is missing.
  list[0]=native("","en",6,0);
  assert(nv_audio_mkv_merge(list,1,NV_FAIXA_MAX,file,2,map)==1);
  // The previous single-track recovery remains available.
  list[0]=native("","",0,0);
  assert(nv_audio_mkv_merge(list,1,NV_FAIXA_MAX,file,1,map)==1);
  assert(!strcmp(list[0].codec,"dts") && list[0].canais==6);
  assert(nv_audio_mkv_merge(list,0,NV_FAIXA_MAX,file,2,map)==2);
  for(int i=0;i<4;i++)list[i]=native("flac","en",2,i);
  assert(nv_audio_mkv_merge(list,4,5,file,6,map)==5); // bounded capacity
  NvAudioMkvTrack truehd={2,8,"A_TRUEHD","en","Original TrueHD"};
  assert(nv_audio_mkv_merge(list,0,NV_FAIXA_MAX,&truehd,1,map)==1);
  assert(list[0].numero==0 && !strcmp(list[0].codec,"A_TRUEHD") && list[0].canais==8);
  assert(!nv_audio_mkv_dts(list[0].codec)); // no DTS trigger without DTS
  list[0]=native("aac","en",2,0);
  assert(nv_audio_mkv_merge(list,1,NV_FAIXA_MAX,&truehd,1,map)==1);
  assert(!strcmp(list[0].codec,"aac")); // native metadata stays authoritative
  assert(nv_audio_mkv_merge(list,0,0,&truehd,1,map)==0);
  puts("PASS missing MKV DTS: two repeated DTS + four FLAC, stable native indices, exact identities, refresh, ambiguity, single-track and capacity");
}
