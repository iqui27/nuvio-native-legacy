#include "trailerfonte.h"
#include "ajustes.h"
#include <stddef.h>

// Onde cada fonte toca. IMDb: o MP4 exige Referer imdb.com, e o CORS dele so
// aceita imdb.com — no <video> da Samsung nem chega a pedir. YouTube: so ha
// player embutido onde ha pagina (Samsung); na LG o botao abre o navegador do
// webOS (extras_trailer_abrir), que nao e trailer "no app".
static int existe(int fonte, int tizen) {
  switch (fonte) {
    case TRF_APPLE:   return 1;
    case TRF_IMDB:    return !tizen;
    case TRF_YOUTUBE: return tizen;
    default:          return 0;
  }
}

int trailerfonte_ordem(int ajuste, int tizen, int ordem[3]) {
  // A ordem de hoje, e por que: Apple e HLS matted ate 4K, sem tarja; IMDb e
  // MP4 16:9 com a tarja embutida; YouTube na Samsung e o embed que falha na
  // AU7000 (#82/#86). Melhor imagem primeiro, o que mais falha por ultimo.
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

TrailerDecisao trailerfonte_escolher(int ajuste, int tizen, const TrailerCandidatos *c,
                                     const char **url, int *qual) {
  int ordem[3], n = trailerfonte_ordem(ajuste, tizen, ordem), i;
  if (url) *url = NULL;
  if (qual) *qual = 0;
  if (!c) return TRF_NENHUMA;
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

int trailerfonte_depois(int ajuste, int tizen, int qual) {
  int ordem[3], n = trailerfonte_ordem(ajuste, tizen, ordem), i;
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

int trailerfonte_com_som(int tizen) { return !tizen; }

int trailerfonte_ajuste(void) { return ajustes_trailer_fonte(); }
int trailerfonte_tizen(void) {
#ifdef __EMSCRIPTEN__
  return 1;
#else
  return 0;
#endif
}
