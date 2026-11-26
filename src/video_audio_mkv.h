#ifndef NV_VIDEO_AUDIO_MKV_H
#define NV_VIDEO_AUDIO_MKV_H
/* DTS tracks the TV player left out of its list (a TV without DTS hides
 * them), recovered from the MKV header so the user can pick them and the
 * converter plays them. Adapted from Matis94 20f341328/fd7f1239f/f92a2371d.
 *
 * Native rows keep their selectTrack index. A recovered row has numero = -1,
 * stream_index = -1 and ordinalMkv = its ordinal among the file's audio
 * tracks. A file DTS track is added only when no native row could be it
 * (language, codec and channels all compatible): never guess which of two
 * look-alike tracks a firmware entry stands for. Several omitted tracks,
 * same language included, each get their own row. Idempotent: earlier
 * recovered rows are dropped and rebuilt. Returns the new list length. */
#include "video.h"
#include "video_dvaudio.h"
#include "audioinfo.h"
#include "linguas.h"
#include <stdio.h>
#include <string.h>

static inline int nv_audio_mkv_recuperado(const VideoFaixa *f) {
  return f->numero < 0 && f->stream_index < 0 && f->ordinalMkv >= 0;
}
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
static inline int nv_audio_mkv_omitidas(VideoFaixa *list, int n, int cap, int count,
                                        const char (*codec)[16], const char (*idioma)[8],
                                        const int *canais, const char (*nome)[48],
                                        int (*casa)(const char *, const char *),
                                        const char *(*familia)(const char *)) {
  int nativas = 0;
  if (n < 0 || n > cap || count < 0) return n;
  for (int i = 0; i < n; i++) if (!nv_audio_mkv_recuperado(&list[i])) list[nativas++] = list[i];
  n = nativas;
  for (int m = 0; m < count && n < cap; m++) {
    if (strncmp(codec[m], "A_DTS", 5)) continue;
    int pode = 0;
    for (int i = 0; i < nativas && !pode; i++) {
      const VideoFaixa *t = &list[i];
      int lingua = !t->idioma[0] || !idioma[m][0] || !strcmp(idioma[m], "und") || casa(t->idioma, idioma[m]);
      const char *a = familia(t->codec), *b = familia(codec[m]);
      int cod = !t->codec[0] || !strcmp(a, b) || (!strncmp(a, "DTS", 3) && !strncmp(b, "DTS", 3));
      int ch = !t->canais || !canais[m] || t->canais == canais[m];
      pode = lingua && cod && ch;
    }
    if (pode) continue;
    VideoFaixa *f = &list[n++];
    memset(f, 0, sizeof *f);
    f->numero = f->stream_index = f->stream_id = -1;
    f->ordinalMkv = m; f->canais = canais[m];
    snprintf(f->codec, sizeof f->codec, "dts");
    if (strcmp(idioma[m], "und")) snprintf(f->idioma, sizeof f->idioma, "%s", idioma[m]);
    snprintf(f->nome, sizeof f->nome, "%s", nome[m]);
  }
  return n;
}
#endif
