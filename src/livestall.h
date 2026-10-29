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
// FONTE QUE SECOU ANTES DA IMAGEM CONGELAR (StallGuard de HLS, plx item 8).
// Onde o fim do buffer e conhecido (webOS), o buffer que NAO CRESCE ha
// NV_LS_SEMDADO_MS com so NV_LS_RESERVA_S de reserva a frente prova que a
// fonte parou de mandar segmentos: a posicao ainda anda, mas vai parar. Reabre
// ja, com a imagem ainda rodando, em vez de esperar mais NV_LS_PARADO_MS de
// quadro congelado. O prazo e maior que um segmento (6-10 s) para o canal no
// "live edge", que so ganha segmento novo a cada duracao dele, nao disparar.
// Fim do buffer desconhecido (0): o guarda nao arma. Android informa o buffer
// para diagnostico, mas deixa o Media3 chegar a buffering/erro antes de reabrir.
//
// So funcoes puras e sem SDL: o teste compila no Mac (tests/livestall.sh).
#ifndef NV_LIVESTALL_H
#define NV_LIVESTALL_H

#include <string.h>

#define NV_LS_PARADO_MS 8000u
#define NV_LS_FIRME_MS  20000u
#define NV_LS_MAX       4
#define NV_LS_SEMDADO_MS 15000u   // buffer sem crescer, com reserva curta: fonte seca
#define NV_LS_RESERVA_S  2.0      // "reserva curta" (s a frente da posicao)

typedef struct {
  double   pos;        // ultima posicao vista
  unsigned movEm;      // quando a posicao andou pela ultima vez (ms)
  unsigned movDesde;   // quando comecou a andar sem parar (0 = parada)
  double   fim;        // maior fim de buffer visto (s)
  unsigned fimEm;      // quando o fim do buffer cresceu pela ultima vez (ms)
  int      andou;      // a posicao andou nesta abertura (arma o vigia)
  int      tentativa;  // reaberturas usadas nesta queda
  int      eofGratis;  // TPK: a reabertura imediata desta queda ja foi usada
  int      pendente;   // esperando o prazo da reabertura
  int      fonteSeca;  // agendamento apenas por fonte seca, cancelavel se voltar
  int      bufferParado; // buffering armou a tentativa; READY pode cancela-la
  unsigned quando;     // instante (ms) da reabertura
} NvLiveStall;

enum { NV_LS_NADA = 0, NV_LS_AGENDOU, NV_LS_ESPERA, NV_LS_REABRIR, NV_LS_DESISTIR };

static inline void nv_ls_zerar(NvLiveStall *s) { memset(s, 0, sizeof *s); }

static inline unsigned nv_ls_espera_ms(int n) {
  return n <= 1 ? 1000u : n == 2 ? 3000u : n == 3 ? 6000u : 10000u;
}

// Depois de reabrir: nova abertura, vigia desarmado ate a posicao andar de novo.
static inline void nv_ls_reaberto(NvLiveStall *s, unsigned agora) {
  s->pos = 0.0; s->movEm = agora; s->movDesde = 0; s->andou = 0; s->pendente = 0; s->fonteSeca = 0; s->bufferParado = 0;
  s->fim = 0.0; s->fimEm = agora;
}

// Chamar por quadro com o canal aberto. `pronto`: o pipeline carregou. `pausado`:
// pausa DA PESSOA. `terminou`: fim de fluxo da plataforma. `bufMs`: ha quanto
// tempo esta bufferando. Devolve NV_LS_*; NV_LS_REABRIR pede reabrir a fonte e
// o chamador responde com nv_ls_reaberto. `fimBuf`: ate onde o buffer cobre (s,
// video_buffer_fim; 0 = desconhecido).
// HLS can take longer than the fixed watchdog to finish one segment. Allow
// three target durations while buffering, bounded even for malformed metadata.
static inline unsigned nv_ls_buffer_prazo(unsigned segmentoMs) {
  if (!segmentoMs) return NV_LS_PARADO_MS;
  if (segmentoMs >= 20000u) return 60000u;
  return segmentoMs > 10000u ? segmentoMs * 3u : 30000u;
}

static inline int nv_ls_passo_buf_prazo(NvLiveStall *s, unsigned agora, double pos, int pronto,
                                  int pausado, int terminou, unsigned bufMs, double fimBuf,
                                  unsigned prazoBuffer) {
  unsigned prazo = bufMs && prazoBuffer > NV_LS_PARADO_MS ? prazoBuffer : NV_LS_PARADO_MS;
  if (prazo > 60000u) prazo = 60000u;
  int parado, seco = 0;
  if (pausado) {                       // pausa da pessoa: nada conta, o relogio recomeca
    s->movEm = agora; s->movDesde = 0; s->pos = pos; s->fimEm = agora;
    return s->pendente ? NV_LS_ESPERA : NV_LS_NADA;
  }
  if (fimBuf > 0.5 && (fimBuf > s->fim + 0.05 || fimBuf < s->fim - 1.0 || !s->fim)) {
    s->fim = fimBuf; s->fimEm = agora;   // cresceu (ou o buffer recomecou)
  }
  if (s->pendente) {
    // A janela pode mover durante o backoff. Rebase nao prova progresso.
    if (pronto && pos < s->pos - 0.05) s->pos = pos;
    int recuperou = pronto && !terminou && !bufMs && !s->fonteSeca &&
      (s->bufferParado || pos > s->pos + 0.05);
    // READY apos buffering ou progresso real cancela ANTES do prazo. Dados
    // chegando so bastam para fonte seca, nunca para decoder parado ou EOF.
    int dadosVoltaram = s->fonteSeca && s->fim > 0.5 &&
      (unsigned)(agora - s->fimEm) < NV_LS_SEMDADO_MS;
    if (recuperou || dadosVoltaram) {
      s->pendente = 0; s->tentativa--; s->fonteSeca = 0; s->bufferParado = 0;
      if (recuperou) { s->pos = pos; s->movEm = agora; s->movDesde = 0; }
    } else return (int)(agora - s->quando) < 0 ? NV_LS_ESPERA : NV_LS_REABRIR;
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
#ifndef NV_ANDROID
  // Media3 can still be playing or loading the next HLS segment with a low
  // reserve. Reporting its buffer must not enable this proactive reopen rule.
  // Android retains the actual freeze, buffering, EOF and error recovery paths.
  if (pronto && s->andou && s->fim > 0.5 && pos <= s->fim &&
      s->fim - pos <= NV_LS_RESERVA_S && (unsigned)(agora - s->fimEm) >= NV_LS_SEMDADO_MS)
    seco = 1;
#endif
  // A sliding live window (or a seek) changes the position's origin. Rebase
  // the comparison, but only subsequent FORWARD motion can refresh movEm.
  // A backward clock alone must not hide a genuinely frozen stream.
  if (pronto && pos < s->pos - 0.05) s->pos = pos;
  if (pronto && pos > s->pos + 0.05) {
    if (!s->movDesde) s->movDesde = agora;
    s->pos = pos; s->movEm = agora; s->andou = 1;
    if ((s->tentativa || s->eofGratis) && (unsigned)(agora - s->movDesde) >= NV_LS_FIRME_MS) {
      s->tentativa = 0; s->eofGratis = 0;
    }
    if (!seco) return NV_LS_NADA;
  }
  if (!pronto) return NV_LS_NADA;      // abrindo: quem vigia e o prazo de abertura
  // A deliberate HLS buffering wait is timed by bufMs, not by the last
  // position sample. READY then gets a fresh frozen-decoder window even if
  // its first position sample has not advanced yet.
  if (bufMs && prazo > NV_LS_PARADO_MS && !terminou) {
    s->movEm = agora; s->movDesde = 0;
  }
  parado = terminou ||
           (s->andou && (unsigned)(agora - s->movEm) >= prazo) ||
           (s->andou && bufMs >= prazo);
  if (!parado && !seco) return NV_LS_NADA;
  s->movDesde = 0;
  if (s->tentativa >= NV_LS_MAX) return NV_LS_DESISTIR;
  s->tentativa++;
  s->pendente = 1;
  s->fonteSeca = seco && !parado;
  s->bufferParado = !terminou && bufMs >= prazo;
  s->quando = agora + nv_ls_espera_ms(s->tentativa);
  return NV_LS_AGENDOU;
}

static inline int nv_ls_passo_buf(NvLiveStall *s, unsigned agora, double pos, int pronto,
                                  int pausado, int terminou, unsigned bufMs, double fimBuf) {
  return nv_ls_passo_buf_prazo(s, agora, pos, pronto, pausado, terminou, bufMs, fimBuf, NV_LS_PARADO_MS);
}

static inline int nv_ls_passo(NvLiveStall *s, unsigned agora, double pos, int pronto,
                              int pausado, int terminou, unsigned bufMs) {
  return nv_ls_passo_buf(s, agora, pos, pronto, pausado, terminou, bufMs, 0.0);
}

#endif
