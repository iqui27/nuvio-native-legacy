#ifndef NV_VIDEO_AUDIO_MKV_H
#define NV_VIDEO_AUDIO_MKV_H
#include "video.h"
#include "video_dvaudio.h"
#include "audioinfo.h"
#include "linguas.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  int numero, canais;
  char codec[24], idioma[8], nome[48];
} NvAudioMkvTrack;

static inline int nv_audio_mkv_dts(const char *codec) {
  return !strncmp(audioinfo_codec(codec),"DTS",3);
}
/* Preserve every native selectTrack index. Only definitely missing DTS rows
 * get a negative native index and an explicit file identity. Never infer which
 * of two identical tracks a filtered/ambiguous firmware entry represents. */
static inline int nv_audio_mkv_merge(VideoFaixa *list,int n,int cap,
                                     const NvAudioMkvTrack *file,int count,int *native) {
  NvDvAudioTrack tv[NV_FAIXA_MAX], mkv[NV_FAIXA_MAX];
  int original=0;
  if(n<0 || n>cap || cap>NV_FAIXA_MAX || count<0 || count>NV_FAIXA_MAX)return n;
  for(int i=0;i<n;i++) {
    if(list[i].numero<0 && list[i].stream_index<0)continue;
    list[original++]=list[i];
  }
  for(int i=0;i<count;i++) {
    native[i]=-1;
    mkv[i]=(NvDvAudioTrack){file[i].codec,file[i].idioma,file[i].canais};
  }
  for(int i=0;i<original;i++)
    tv[i]=(NvDvAudioTrack){list[i].codec,list[i].idioma,list[i].canais};
  for(int i=0;i<original;i++) {
    int m=nv_dvaudio_lista(original,count,i,tv,mkv,ling_casa,audioinfo_codec);
    if(m<0 || file[m].numero<=0)continue;
    native[m]=list[i].numero;
    list[i].ordinalMkv=m;list[i].stream_id=file[m].numero;
    if(!list[i].nome[0])snprintf(list[i].nome,sizeof list[i].nome,"%s",file[m].nome);
    if(count==1 && !list[i].codec[0]) {
      snprintf(list[i].codec,sizeof list[i].codec,"%s",nv_audio_mkv_dts(file[m].codec)?"dts":file[m].codec);
      if(!list[i].idioma[0])snprintf(list[i].idioma,sizeof list[i].idioma,"%s",file[m].idioma);
      if(!list[i].canais)list[i].canais=file[m].canais;
      snprintf(list[i].rotulo,sizeof list[i].rotulo,"%s",file[m].nome[0]?file[m].nome:file[m].codec);
    }
  }
  n=original;
  for(int m=0;m<count && n<cap;m++) {
    if(native[m]>=0 || file[m].numero<=0 || !nv_audio_mkv_dts(file[m].codec))continue;
    int possible=0;
    for(int i=0;i<original;i++)
      if(nv_dvaudio_par(&tv[i],&mkv[m],ling_casa,audioinfo_codec)) {possible=1;break;}
    if(possible)continue;
    VideoFaixa *f=&list[n++];memset(f,0,sizeof *f);
    f->numero=f->stream_index=-1;f->stream_id=file[m].numero;f->ordinalMkv=m;
    f->canais=file[m].canais;
    snprintf(f->codec,sizeof f->codec,"dts");
    snprintf(f->idioma,sizeof f->idioma,"%s",file[m].idioma);
    snprintf(f->nome,sizeof f->nome,"%s",file[m].nome);
    snprintf(f->rotulo,sizeof f->rotulo,"%.34s · DTS",file[m].nome[0]?file[m].nome:file[m].idioma);
  }
  return n;
}
#endif
