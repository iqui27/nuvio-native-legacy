#include "trailerfonte.h"
#include "perfiltv.h"
#include "ajustes.h"
#include <stddef.h>

// Onde cada fonte toca. IMDb: a PERGUNTA a API deles exige Referer imdb.com
// (403 sem ele) e nao manda CORS para um wgt — na Samsung ela passa pelo
// servico de recomendacoes (/v1/trailer/imdb, #136); o MP4 que volta toca no
// <video> sem Referer. Sem esse servico na build, o IMDb nao existe la.
// YouTube: so ha player embutido onde ha pagina (Samsung); na LG o botao abre
// o navegador do webOS (extras_trailer_abrir), que nao e trailer "no app".
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
static int imdbTizen = -1;   // -1: o que a build diz (NV_REC_URL)
void trailerfonte_definir_imdb_tizen(int sim) { imdbTizen = sim ? 1 : 0; }
int  trailerfonte_imdb_tizen(void) { return imdbTizen >= 0 ? imdbTizen : NV_REC_URL[0] != 0; }

static int existe(int fonte, int tizen) {
  switch (fonte) {
    case TRF_APPLE:   return 1;
    case TRF_IMDB:
#ifdef NV_VIDAA
      return !tizen;  // VIDAA: IMDb nao toca (ver ajuda do AJ_TRAILER_FONTE)
#else
      return !tizen || trailerfonte_imdb_tizen();
#endif
    case TRF_YOUTUBE:
#if defined(NV_VIDAA) && !defined(NV_UM_FIO)
      return 0;  // mt build: YouTube iframe falha com COEP isolado
#else
      return tizen;  // Tizen e VIDAA st: YouTube habilitado
#endif
    default:          return 0;
  }
}

int trailerfonte_ordem(int ajuste, int tizen, int ordem[3]) {
  // A ordem de hoje, e por que: Apple e HLS matted ate 4K, sem tarja; IMDb e
  // MP4 16:9 com a tarja embutida, no <video> do app; YouTube na Samsung e o
  // embed que falha na AU7000 (#82/#86/#136). Melhor imagem primeiro, o que
  // mais falha por ultimo — e tudo DENTRO do app antes do iframe de terceiro.
  static const int AUTO[3] = { TRF_APPLE, TRF_IMDB, TRF_YOUTUBE };
  int i, n = 0;
  if (ajuste == TRF_APPLE || ajuste == TRF_IMDB || ajuste == TRF_YOUTUBE) {
    if (existe(ajuste, tizen)) ordem[n++] = ajuste;
    return n;
  }
  // Valor desconhecido (ajustes.txt de outra versao) le como Automatico.
  for (i = 0; i < 3; i++)
    if (existe(AUTO[i], tizen)) ordem[n++] = AUTO[i];
  return n;
}

// .tpk: Apple la e so video (ver trailerfonte.h, trailerfonte_ordem_cheia).
#ifdef NV_TPK
static int imdbPrimeiroCheia = 1;
#else
static int imdbPrimeiroCheia = 0;
#endif
int  trailerfonte_imdb_primeiro_cheia(void) { return imdbPrimeiroCheia; }
void trailerfonte_definir_imdb_primeiro_cheia(int sim) { imdbPrimeiroCheia = sim ? 1 : 0; }

// O IMDb sobe para a frente; o resto mantem a ordem relativa. So em
// Automatico: uma fonte escolhida no ajuste e respeitada.
static void imdbNaFrente(int ajuste, int *ordem, int n) {
  int i, j;
  if (ajuste == TRF_APPLE || ajuste == TRF_IMDB || ajuste == TRF_YOUTUBE) return;
  for (i = 1; i < n; i++)
    if (ordem[i] == TRF_IMDB) {
      for (j = i; j > 0; j--) ordem[j] = ordem[j - 1];
      ordem[0] = TRF_IMDB;
      break;
    }
}

int trailerfonte_ordem_cheia(int ajuste, int tizen, int som, int ordem[3]) {
  int n = trailerfonte_ordem(ajuste, tizen, ordem);
  if (som && imdbPrimeiroCheia) imdbNaFrente(ajuste, ordem, n);
  return n;
}

// Destaque/cartaz da home que pode CONTINUAR com som na pagina do titulo
// (ver trailerfonte.h). 1 so no .tpk com NV_TRAILER_CONTINUA_DETALHE.
static int imdbPrimeiroDestaque = NV_TRAILER_CONTINUA_DETALHE;
int  trailerfonte_imdb_primeiro_destaque(void) { return imdbPrimeiroDestaque; }
void trailerfonte_definir_imdb_primeiro_destaque(int sim) { imdbPrimeiroDestaque = sim ? 1 : 0; }

int trailerfonte_ordem_destaque(int ajuste, int tizen, int ordem[3]) {
  int n = trailerfonte_ordem(ajuste, tizen, ordem);
  if (imdbPrimeiroDestaque) imdbNaFrente(ajuste, ordem, n);
  return n;
}

int trailerfonte_depois_destaque(int ajuste,int tizen,int qual) {
  int ordem[3],n=trailerfonte_ordem_destaque(ajuste,tizen,ordem);
  for(int i=0;i+1<n;i++)if(ordem[i]==qual)return ordem[i+1];
  return 0;
}

static TrailerDecisao escolherNaOrdem(const int *ordem, int n, const TrailerCandidatos *c,
                                      const char **url, int *qual) {
  int i;
  if (url) *url = NULL;
  if (qual) *qual = 0;
  if (!c) return TRF_NENHUMA;
  // Trailer audio (Ajustes): where YouTube plays, the TMDB trailer in the
  // chosen language beats the English Apple/IMDb, and a pending TMDB answer
  // holds the queue. Automatic order only; a fixed source is respected.
  if (n > 1 && c->youtubeIdiomaPreferido)
    for (i = 0; i < n; i++) if (ordem[i] == TRF_YOUTUBE) {
      if (c->youtubeIdiomaPreferido < 0) return TRF_ESPERA;
      if (!c->youtube || !c->youtube[0]) break;
      if (url) *url = c->youtube;
      if (qual) *qual = TRF_YOUTUBE;
      return TRF_ABRE;
    }
  for (i = 0; i < n; i++) {
    const char *u = NULL;
    int respondeu = 1;
    switch (ordem[i]) {
      case TRF_APPLE:
        if (c->appleFalhou) continue;   // ja deu erro nesta tentativa: cede
        u = c->apple; respondeu = c->appleRespondeu; break;
      case TRF_IMDB:    u = c->imdb;    respondeu = c->imdbRespondeu; break;
      case TRF_YOUTUBE: u = c->youtube; respondeu = c->youtubeRespondeu; break;
    }
    if (u && u[0]) {
      if (url) *url = u;
      if (qual) *qual = ordem[i];
      return TRF_ABRE;
    }
    if (!respondeu) return TRF_ESPERA;
  }
  return TRF_NENHUMA;
}

TrailerDecisao trailerfonte_escolher(int ajuste, int tizen, const TrailerCandidatos *c,
                                     const char **url, int *qual) {
  int ordem[3], n = trailerfonte_ordem(ajuste, tizen, ordem);
  return escolherNaOrdem(ordem, n, c, url, qual);
}

TrailerDecisao trailerfonte_escolher_cheia(int ajuste, int tizen, int som,
                                           const TrailerCandidatos *c,
                                           const char **url, int *qual) {
  int ordem[3], n = trailerfonte_ordem_cheia(ajuste, tizen, som, ordem);
  return escolherNaOrdem(ordem, n, c, url, qual);
}

TrailerDecisao trailerfonte_escolher_destaque(int ajuste, int tizen, const TrailerCandidatos *c,
                                              const char **url, int *qual) {
  int ordem[3], n = trailerfonte_ordem_destaque(ajuste, tizen, ordem);
  return escolherNaOrdem(ordem, n, c, url, qual);
}

int trailerfonte_depois(int ajuste, int tizen, int qual) {
  int ordem[3], n = trailerfonte_ordem(ajuste, tizen, ordem), i;
  for (i = 0; i + 1 < n; i++)
    if (ordem[i] == qual) return ordem[i + 1];
  return 0;
}

// The dubbed YouTube trailer opened first (Trailer audio): the fallback walks
// YouTube -> Apple -> IMDb, so a failed embed (153/timeout) still gets the
// English trailer. Order without YouTube: same as trailerfonte_depois.
int trailerfonte_depois_dublado(int ajuste, int tizen, int qual) {
  int ordem[3], n = trailerfonte_ordem(ajuste, tizen, ordem), i, j;
  for (i = 1; i < n; i++)
    if (ordem[i] == TRF_YOUTUBE) {
      for (j = i; j > 0; j--) ordem[j] = ordem[j - 1];
      ordem[0] = TRF_YOUTUBE;
      break;
    }
  for (i = 0; i + 1 < n; i++)
    if (ordem[i] == qual) return ordem[i + 1];
  return 0;
}

const char *trailerfonte_nome(int qual) {
  switch (qual) {
    case TRF_APPLE:   return "apple";
    case TRF_IMDB:    return "imdb";
    case TRF_YOUTUBE: return "youtube";
    default:          return "-";
  }
}

int trailerfonte_com_som(int tizen) {
#ifdef NV_VIDAA
  return 1;  // VIDAA: plain <video> sem problema de autoplay policy
#else
  return !tizen;  // Tizen: mudo; LG: som
#endif
}

int trailerfonte_ajuste(void) { return ajustes_trailer_fonte(); }
int trailerfonte_tizen(void) {
#ifdef __EMSCRIPTEN__
  return 1;
#else
  return 0;
#endif
}
