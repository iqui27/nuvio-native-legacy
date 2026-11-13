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
#include <stdio.h>
#include <string.h>

static inline int nv_audio_mkv_recuperado(const VideoFaixa *f) {
  return f->numero < 0 && f->stream_index < 0 && f->ordinalMkv >= 0;
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
