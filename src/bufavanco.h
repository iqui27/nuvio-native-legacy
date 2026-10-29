// Veto do watchdog de buffer (app.c, tentarProximaFonteVOD): a troca de fonte
// por "bufferando ha 30 s" so fica retida enquanto o fim do buffer ANDA. O
// estado e por fonte e por episodio de buffering: a chave (fonteVODDesde) muda
// a cada abertura e `ms` volta a zero quando o buffering reinicia, entao uma
// fonte nova nunca herda a janela morta da anterior. Sem avanco de
// BUFAV_MIN_S dentro de BUFAV_JANELA_MS, a fonte nao entrega: libera a troca.
// So funcoes puras e sem SDL: o teste compila no Mac (tests/bufavanco.sh).
#ifndef NV_BUFAVANCO_H
#define NV_BUFAVANCO_H

#define BUFAV_JANELA_MS 10000u
#define BUFAV_MIN_S 0.5

typedef struct { unsigned chave, ms, t0; double buf0; int armado; } BufAvanco;

// Borda do buffer em segundos a partir do que o player informa: posicao mais
// segundos a frente (VideoStats.bufferSeg, < 0 = nao sabe -> 0, sem avanco).
static inline double bufavanco_borda(double pos, double bufferSeg) {
  return bufferSeg >= 0.0 ? pos + bufferSeg : 0.0;
}

// 1 = segura a troca (buffer andou ou ainda dentro da janela); 0 = pode trocar.
static inline int bufavanco_segura(BufAvanco *s, unsigned chave, unsigned ms,
                                   unsigned agora, double fim) {
  if (!s->armado || s->chave != chave || ms < s->ms) {
    s->armado = 1; s->chave = chave; s->t0 = agora; s->buf0 = fim;
  }
  s->ms = ms;
  if (fim > s->buf0 + BUFAV_MIN_S) { s->t0 = agora; s->buf0 = fim; return 1; }
  return (unsigned)(agora - s->t0) < BUFAV_JANELA_MS;
}
#endif
