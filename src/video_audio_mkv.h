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
  int dts_kind;   /* mkv_dts.h NV_DTS_KIND_*: 2 DTS-HD, 3 Master Audio, else plain or unknown */
} NvAudioMkvTrack;
/* The codec name for the menu line: DTS-HD and Master Audio told apart from
 * plain DTS by the file's frames (A_DTS and the TV say "dts" for all). */
static inline const char *nv_audio_mkv_rotulo_codec(const NvAudioMkvTrack *m) {
  return m->dts_kind == 3 ? "dts-hd ma" : m->dts_kind == 2 ? "dts-hd" : m->codec;
}
/* A TV row paired with a DTS-HD track of the file: its menu line says so
 * ("Russian · DTS-HD MA 5.1" instead of "DTS 5.1"). Only the label changes. */
static inline void nv_audio_mkv_rotulo_hd(VideoFaixa *f, const NvAudioMkvTrack *m) {
  char ai[40];
  if (m->dts_kind < 2 || !audioinfo_texto(nv_audio_mkv_rotulo_codec(m), f->canais ? f->canais : m->canais,
                                          strstr(f->rotulo, "Atmos") != NULL, ai, sizeof ai)) return;
  /* The TV's line is "<language>  ·  DTS 5.1": the language part (already
   * translated) stays, the codec part after the separator is replaced. */
  char *sep = strstr(f->rotulo, "  \xc2\xb7  ");
  if (!sep) return;
  sep += 6;
  snprintf(sep, sizeof f->rotulo - (size_t)(sep - f->rotulo), "%s", ai);
}

static inline int nv_audio_mkv_dts(const char *codec) {
  return !strncmp(audioinfo_codec(codec),"DTS",3);
}
/* Labels only, list untouched: the TV rows paired with a DTS-HD track of the
 * file say so. For DTS "Off" in Settings, where no row is recovered (the TV
 * plays DTS itself), the pairing is the merge's (whole ordered list), so two
 * identical rows are never guessed. Returns how many labels changed. */
static inline int nv_audio_mkv_rotular(VideoFaixa *list,int n,const NvAudioMkvTrack *file,int count) {
  NvDvAudioTrack tv[NV_FAIXA_MAX], mkv[NV_FAIXA_MAX];
  int mudou=0;
  if(n<1 || n>NV_FAIXA_MAX || count<1 || count>NV_FAIXA_MAX)return 0;
  for(int i=0;i<count;i++)mkv[i]=(NvDvAudioTrack){file[i].codec,file[i].idioma,file[i].canais};
  for(int i=0;i<n;i++)tv[i]=(NvDvAudioTrack){list[i].codec,list[i].idioma,list[i].canais};
  for(int i=0;i<n;i++) {
    if(list[i].numero<0)continue;
    int m=nv_dvaudio_lista(n,count,i,tv,mkv,ling_casa,audioinfo_codec);
    if(m<0 || !nv_audio_mkv_dts(file[m].codec) || file[m].dts_kind<2)continue;
    char antes[sizeof list[i].rotulo];
    snprintf(antes,sizeof antes,"%s",list[i].rotulo);
    nv_audio_mkv_rotulo_hd(&list[i],&file[m]);
    mudou+=strcmp(antes,list[i].rotulo)!=0;
  }
  return mudou;
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
  /* Preserve the earlier single-track metadata fallback for other codecs
   * too (e.g. TrueHD). It is not a synthetic DTS conversion request. */
  if(!original && cap>0 && count==1 && file[0].numero>0 && !nv_audio_mkv_dts(file[0].codec)) {
    VideoFaixa *f=&list[original++];memset(f,0,sizeof *f);
    f->stream_index=f->stream_id=-1;f->ordinalMkv=0;
  }
  for(int i=0;i<original;i++)
    tv[i]=(NvDvAudioTrack){list[i].codec,list[i].idioma,list[i].canais};
  for(int i=0;i<original;i++) {
    int m=nv_dvaudio_lista(original,count,i,tv,mkv,ling_casa,audioinfo_codec);
    if(m<0 || file[m].numero<=0)continue;
    native[m]=list[i].numero;
    list[i].ordinalMkv=m;list[i].stream_id=file[m].numero;
    if(nv_audio_mkv_dts(file[m].codec))nv_audio_mkv_rotulo_hd(&list[i],&file[m]);
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
    snprintf(f->rotulo,sizeof f->rotulo,"%.34s · %s",file[m].nome[0]?file[m].nome:file[m].idioma,
             audioinfo_codec(nv_audio_mkv_rotulo_codec(&file[m])));
  }
  return n;
}
#endif
