// Faixa de audio que o caminho Dolby Vision nao alimenta (A_TRUEHD, DTS-HD MA
// sem conversao etc.): em vez de entregar o MKV inteiro ao player da TV (HDR10),
// troca por uma faixa do MESMO idioma que o caminho aceita. Remux de DV costuma
// trazer TrueHD Atmos + um E-AC-3 (DD+ 7.1) do mesmo idioma; o E-AC-3 mantem o DV.
#ifndef NV_VIDEO_DVAUDIO_H
#define NV_VIDEO_DVAUDIO_H

#include <string.h>
#include <ctype.h>
#include "dts/dts_audio_codec.h"

// 0 = o caminho nao alimenta; 1..3 = alimenta (3 = E-AC-3, melhor).
static inline int nv_dvaudio_nota_codec(const char *c) {
  if (!strcmp(c, "A_EAC3")) return 3;
  if (!strcmp(c, "A_AC3")) return 2;
  if (!strncmp(c, "A_DTS", 5)) return 1;
  return 0;
}

static inline int nv_dvaudio_alimentavel(const char *c) {
  return nv_dvaudio_nota_codec(c)>0 || nv_dts_truehd_codec(c);
}

static inline int nv_dvaudio_tem_atmos(const char *nome) {
  size_t n = strlen(nome), i, k;
  static const char *chave[2] = {"atmos", "joc"};
  for (k = 0; k < 2; k++) {
    size_t m = strlen(chave[k]);
    for (i = 0; i + m <= n; i++) {
      size_t j;
      for (j = 0; j < m && tolower((unsigned char)nome[i + j]) == chave[k][j]; j++) {}
      if (j == m) return 1;
    }
  }
  return 0;
}

// Escolhe a faixa que mantem o DV no lugar de `atual`. Devolve o indice ou -1.
// codec[i] = CodecID do Matroska; idioma[i] = idioma da lista da TV; rotulo[i] = nome
// mostrado. `casa` compara dois idiomas (ling_casa no app). Idioma vazio so casa
// com idioma vazio. Ordem: E-AC-3 > AC-3 > DTS; no empate, a que cita Atmos/JOC;
// no empate final, a primeira do arquivo.
static inline int nv_dvaudio_escolher(int n, int atual, const char (*codec)[16],
                                      const char (*idioma)[8], const char (*rotulo)[48],
                                      int (*casa)(const char *, const char *)) {
  int i, melhor = -1, notaMelhor = 0, atmosMelhor = 0;
  if (atual < 0 || atual >= n) return -1;
  for (i = 0; i < n; i++) {
    int nota = nv_dvaudio_nota_codec(codec[i]), atmos;
    if (i == atual || !nota) continue;
    if (!idioma[atual][0] ? idioma[i][0] != 0 : (!idioma[i][0] || !casa(idioma[i], idioma[atual]))) continue;
    atmos = nv_dvaudio_tem_atmos(rotulo[i]);
    if (nota > notaMelhor || (nota == notaMelhor && atmos && !atmosMelhor)) {
      melhor = i; notaMelhor = nota; atmosMelhor = atmos;
    }
  }
  return melhor;
}

// Localiza a faixa ATUAL da TV na lista do MKV. O indice da TV nao e indice do
// MKV: a TV pode filtrar ou reordenar (C9: uma faixa so; com o MKV [TrueHD eng,
// E-AC-3 eng, E-AC-3 por] e a TV expondo so a portuguesa, o indice 0 da TV
// apontava o TrueHD ingles, e a troca entregava ingles). Mesmo criterio do motor
// (dts_playback.c prepare()): listas do mesmo tamanho valem pela ordem se a
// faixa daquele indice nao contradiz a da TV; fora disso, so uma correspondencia
// UNICA por idioma, familia de codec (`familia`: audioinfo_codec no app, que le
// tanto "A_EAC3" quanto o "eac3" da TV) e canais. Campo vazio ou 0 de um dos
// lados nao contradiz. Lista da TV ainda vazia (sourceInfo nao chegou): a TV
// toca a faixa padrao, e vale o indice como sempre. Devolve o ordinal no MKV, ou
// -1 sem correspondencia segura (quem chama nao troca).
static inline int nv_dvaudio_localizar(int nTv, int nMkv, int atualTv, const char *tvIdioma,
                                       const char *tvCodec, int tvCanais,
                                       const char (*codec)[16], const char (*idioma)[8],
                                       const int *canais,
                                       int (*casa)(const char *, const char *),
                                       const char *(*familia)(const char *)) {
  int i, achou = -1, n = 0;
  const char *ft = familia(tvCodec ? tvCodec : "");
#define NV_DVAUDIO_CASA(i) \
  ((!tvIdioma || !tvIdioma[0] || !idioma[i][0] || !strcmp(idioma[i], "und") || casa(idioma[i], tvIdioma)) && \
   (!ft[0] || !familia(codec[i])[0] || !strcmp(ft, familia(codec[i]))) && \
   (tvCanais <= 0 || !canais || canais[i] <= 0 || canais[i] == tvCanais))
  if (atualTv < 0) return -1;
  if (nTv <= 0) return atualTv < nMkv ? atualTv : -1;
  if (nTv == nMkv && atualTv < nMkv && NV_DVAUDIO_CASA(atualTv)) return atualTv;
  for (i = 0; i < nMkv; i++) if (NV_DVAUDIO_CASA(i)) { achou = i; n++; }
#undef NV_DVAUDIO_CASA
  return n == 1 ? achou : -1;
}

typedef struct {
  const char *codec, *idioma;
  int canais;
} NvDvAudioTrack;

static inline int nv_dvaudio_par(const NvDvAudioTrack *a, const NvDvAudioTrack *b,
                                int (*casa)(const char *, const char *),
                                const char *(*familia)(const char *)) {
  const char *ac = familia(a->codec), *bc = familia(b->codec);
  if (!strncmp(ac, "DTS", 3)) ac = "DTS";
  if (!strncmp(bc, "DTS", 3)) bc = "DTS";
  return (!a->idioma[0] || !b->idioma[0] || !strcmp(a->idioma, "und") ||
          !strcmp(b->idioma, "und") || casa(a->idioma, b->idioma)) &&
         (!ac[0] || !bc[0] || !strcmp(ac, bc)) &&
         (a->canais <= 0 || b->canais <= 0 || a->canais == b->canais);
}

// sourceInfo may omit unsupported tracks. Match the WHOLE ordered list: remuxes
// often contain repeated language/codec/channel tuples. Earliest and latest
// complete subsequence embeddings must agree for the selected row. Ambiguous or
// reordered lists still need a unique individual match; never guess the first.
static inline int nv_dvaudio_lista(int nTv, int nMkv, int atual,
                                  const NvDvAudioTrack *tv, const NvDvAudioTrack *mkv,
                                  int (*casa)(const char *, const char *),
                                  const char *(*familia)(const char *)) {
  int first = -1, last = -1, j = 0, complete = 1;
  if (atual < 0 || nMkv <= 0) return -1;
  if (nTv <= 0) return atual < nMkv ? atual : -1;
  if (atual >= nTv) return -1;
  for (int i = 0; i < nTv; i++) {
    while (j < nMkv && !nv_dvaudio_par(&tv[i], &mkv[j], casa, familia)) j++;
    if (j == nMkv) { complete = 0; break; }
    if (i == atual) first = j;
    j++;
  }
  if (complete) {
    j = nMkv - 1;
    for (int i = nTv - 1; i >= 0; i--) {
      while (j >= 0 && !nv_dvaudio_par(&tv[i], &mkv[j], casa, familia)) j--;
      if (j < 0) { complete = 0; break; }
      if (i == atual) last = j;
      j--;
    }
    if (complete && first == last) return first;
  }
  int found = -1;
  for (int i = 0; i < nMkv; i++) if (nv_dvaudio_par(&tv[atual], &mkv[i], casa, familia)) {
    if (found >= 0) return -1;
    found = i;
  }
  return found;
}

// Decisao como o dvPronto a usa. A escolha vem SO da lista do MKV (nMkv faixas):
// o player da TV costuma listar UMA faixa de audio num remux com TrueHD+E-AC-3
// (C9: "faixas: audio=1"), entao a lista dele nao pode limitar os candidatos.
// nTv fica na assinatura so para o teste deixar essa regra explicita.
// `atual` ja e o ordinal NO MKV (nv_dvaudio_localizar), nunca o indice da TV.
// Devolve o indice da faixa NO MKV (ordinal entre as faixas de audio), ou -1.
static inline int nv_dvaudio_decidir(int nTv, int nMkv, int atual, const char (*codec)[16],
                                     const char (*idioma)[8], const char (*rotulo)[48],
                                     int (*casa)(const char *, const char *)) {
  (void)nTv;
  return nv_dvaudio_escolher(nMkv, atual, codec, idioma, rotulo, casa);
}

#endif
