// VIGIA DE TRAVA DO CANAL AO VIVO (#302), comum as quatro plataformas.
//
// O caso medido (log 45605, .tpk 6.5, provedor Xtream de TS continuo): o canal
// toca ~25-30 s, possivelmente a conexao fecha, o Tizen.Multimedia.Player avisa
// PlaybackCompleted (evento 4) e fica parado. Nada disso e erro nem
// buffering, entao o watchdog de canal do app.c (falhou / bufferando > 12 s /
// prazo de abertura) nunca disparava: a imagem congelava para sempre e so
// trocar de canal destravava. No relato, outros players mantem a reproducao.
//
// Regra: canal que JA andou e depois (a) terminou, (b) a posicao nao anda por
// NV_LS_PARADO_MS, ou (c) esta bufferando por NV_LS_PARADO_MS, e reaberto
// (a MESMA fonte), ate NV_LS_MAX vezes com espera crescente. Pausa da pessoa
// nunca conta. Passou NV_LS_FIRME_MS andando depois de uma reabertura, o
// contador volta a zero (o provedor que corta a cada 30 s reconecta para
// sempre, como os outros apps). Esgotou: o chamador cai no caminho de sempre
// (proxima fonte, cartao de erro).
// No TPK, o primeiro EOF depois de entregar video reabre sem espera. Outro
// EOF antes de NV_LS_FIRME_MS consome o limite, para clipe curto nao girar.
//
// So funcoes puras e sem SDL: o teste compila no Mac (tests/livestall.sh).
#ifndef NV_LIVESTALL_H
#define NV_LIVESTALL_H

#include <string.h>

#define NV_LS_PARADO_MS 8000u
#define NV_LS_FIRME_MS  20000u
#define NV_LS_MAX       4

typedef struct {
  double   pos;        // ultima posicao vista
  unsigned movEm;      // quando a posicao andou pela ultima vez (ms)
  unsigned movDesde;   // quando comecou a andar sem parar (0 = parada)
  int      andou;      // a posicao andou nesta abertura (arma o vigia)
  int      tentativa;  // reaberturas usadas nesta queda
  int      eofGratis;  // TPK: a reabertura imediata desta queda ja foi usada
  int      pendente;   // esperando o prazo da reabertura
  unsigned quando;     // instante (ms) da reabertura
} NvLiveStall;

enum { NV_LS_NADA = 0, NV_LS_AGENDOU, NV_LS_ESPERA, NV_LS_REABRIR, NV_LS_DESISTIR };

static inline void nv_ls_zerar(NvLiveStall *s) { memset(s, 0, sizeof *s); }

static inline unsigned nv_ls_espera_ms(int n) {
  return n <= 1 ? 1000u : n == 2 ? 3000u : n == 3 ? 6000u : 10000u;
}

// Depois de reabrir: nova abertura, vigia desarmado ate a posicao andar de novo.
static inline void nv_ls_reaberto(NvLiveStall *s, unsigned agora) {
  s->pos = 0.0; s->movEm = agora; s->movDesde = 0; s->andou = 0; s->pendente = 0;
}

// Chamar por quadro com o canal aberto. `pronto`: o pipeline carregou. `pausado`:
// pausa DA PESSOA. `terminou`: fim de fluxo da plataforma. `bufMs`: ha quanto
// tempo esta bufferando. Devolve NV_LS_*; NV_LS_REABRIR pede reabrir a fonte e
// o chamador responde com nv_ls_reaberto.
static inline int nv_ls_passo(NvLiveStall *s, unsigned agora, double pos, int pronto,
                              int pausado, int terminou, unsigned bufMs) {
  int parado;
  if (pausado) {                       // pausa da pessoa: nada conta, o relogio recomeca
    s->movEm = agora; s->movDesde = 0; s->pos = pos;
    return s->pendente ? NV_LS_ESPERA : NV_LS_NADA;
  }
  if (s->pendente) {
    if ((int)(agora - s->quando) < 0) return NV_LS_ESPERA;
    return NV_LS_REABRIR;
  }
#ifdef NV_TPK
  // #302: o primeiro EOF com video e continuidade de live, nao fim de arquivo.
  // Se a nova abertura morrer antes de ficar firme, passa a gastar o limite.
  if (pronto && terminou && (s->andou || pos > 0.05)) {
    if (!s->eofGratis) { s->eofGratis = 1; return NV_LS_REABRIR; }
    if (s->tentativa >= NV_LS_MAX) return NV_LS_DESISTIR;
    s->tentativa++;
    return NV_LS_REABRIR;
  }
#endif
  if (pronto && pos > s->pos + 0.05) {
    if (!s->movDesde) s->movDesde = agora;
    s->pos = pos; s->movEm = agora; s->andou = 1;
    if ((s->tentativa || s->eofGratis) && (unsigned)(agora - s->movDesde) >= NV_LS_FIRME_MS) {
      s->tentativa = 0; s->eofGratis = 0;
    }
    return NV_LS_NADA;
  }
  if (!pronto) return NV_LS_NADA;      // abrindo: quem vigia e o prazo de abertura
  parado = terminou ||
           (s->andou && (unsigned)(agora - s->movEm) >= NV_LS_PARADO_MS) ||
           (s->andou && bufMs >= NV_LS_PARADO_MS);
  if (!parado) return NV_LS_NADA;
  s->movDesde = 0;
  if (s->tentativa >= NV_LS_MAX) return NV_LS_DESISTIR;
  s->tentativa++;
  s->pendente = 1;
  s->quando = agora + nv_ls_espera_ms(s->tentativa);
  return NV_LS_AGENDOU;
}

#endif
