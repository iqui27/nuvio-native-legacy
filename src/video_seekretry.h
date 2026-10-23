// Seek recusado pelo pipeline (webOS uMS errorCode 700 "seek Failure").
// Medido (registro 54650, OLED48C1PVB, HEVC 4K): o erro tratado como falha de
// fonte derrubava o link ("pulou 1 link") 70 vezes. Seek recusado nao e fonte
// morta: tenta de novo com espera crescente e, esgotado, segue tocando de onde
// esta, com um aviso.
#ifndef NV_VIDEO_SEEKRETRY_H
#define NV_VIDEO_SEEKRETRY_H

#define NV_SEEK_MAX 3

typedef struct { int falhas; int desistiu; } NvSeekRetry;

static inline int nv_seek_e_recusa(double codigo, int textoSeekFailure) {
  return codigo == 700 || textoSeekFailure;
}

// Espera (ms) antes da tentativa n (1-based): 500, 1500, 3000.
static inline unsigned nv_seek_espera_ms(int n) {
  return n <= 1 ? 500u : n == 2 ? 1500u : 3000u;
}

// Registra uma recusa. Devolve a espera ate o proximo seek, ou 0 se desistiu
// (a reproducao continua de onde esta; nunca vira falha de fonte).
static inline unsigned nv_seek_recusado(NvSeekRetry *s) {
  if (s->falhas >= NV_SEEK_MAX) { s->desistiu = 1; return 0; }
  s->falhas++;
  return nv_seek_espera_ms(s->falhas);
}

static inline void nv_seek_ok(NvSeekRetry *s) { s->falhas = 0; }
static inline void nv_seek_zerar(NvSeekRetry *s) { s->falhas = 0; s->desistiu = 0; }

#endif
