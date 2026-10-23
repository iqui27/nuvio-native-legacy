// Confirmacao de busca: o destino otimista da barra nao prova progresso real.
#ifndef NV_CREDITOSAUTO_H
#define NV_CREDITOSAUTO_H
#include <math.h>
#include <string.h>

// Busca que nunca pousa (destino trocado, seekTo falho) nao pode travar o
// pos-play: com o video tocando ou terminado, desiste depois deste tempo.
#define CREDITOSAUTO_LIMITE_MS 8000

typedef struct {
  int buscaPendente, buscaRecuo, fonteMudou, temFonte;
  double fimBusca, inicioBusca;
  unsigned long long fonteHash, desdeMs;
} CreditosAuto;

static inline void creditosauto_zerar(CreditosAuto *s) { memset(s, 0, sizeof *s); }

// Marcadores de rede podem pertencer a outro corte apos trocar a fonte.
// Compara o hash (FNV-1a) da URL inteira, qualquer que seja o tamanho.
static inline void creditosauto_fonte(CreditosAuto *s, const char *url) {
  unsigned long long h = 1469598103934665603ULL;
  if (!url || !*url) return;
  for (; *url; url++) h = (h ^ (unsigned char)*url) * 1099511628211ULL;
  if (!s->temFonte) { s->temFonte = 1; s->fonteHash = h; }
  else if (s->fonteHash != h) { s->fonteMudou = 1; s->buscaPendente = 0; }
}

static inline void creditosauto_buscar(CreditosAuto *s, double destino) {
  s->fimBusca = destino;
  s->buscaPendente = 1; s->buscaRecuo = 0;
  s->desdeMs = 0;
}

static inline void creditosauto_recuar(CreditosAuto *s, double de, double para) {
  if (!isfinite(de) || !isfinite(para) || para >= de) return;
  s->fimBusca = para; s->inicioBusca = de;
  s->buscaPendente = s->buscaRecuo = 1;
  s->desdeMs = 0;
}

// Toda busca enviada ao video passa por aqui: se o destino nao e o da busca
// armada (ex.: avanco depois de um recuo), ela foi substituida e nao pousara.
static inline void creditosauto_destino(CreditosAuto *s, double alvo) {
  if (s->buscaPendente && !(fabs(alvo - s->fimBusca) < 0.01)) s->buscaPendente = 0;
}

// No recuo, tempo/fim antigos nao confirmam a busca. Next so volta a contar
// apos progresso no destino; no avanco, o fim nativo tambem confirma.
static inline int creditosauto_aguardar(CreditosAuto *s, double pos, int tocando, int terminou,
                                         unsigned long long agoraMs) {
  if (!s->buscaPendente) return 0;
  if (!tocando && !terminou) s->desdeMs = 0;
  else if (!s->desdeMs) s->desdeMs = agoraMs + 1;   // 0 = relogio parado
  else if (agoraMs + 1 - s->desdeMs >= CREDITOSAUTO_LIMITE_MS) { s->buscaPendente = 0; return 0; }
  if ((!s->buscaRecuo && terminou) ||
      (tocando && isfinite(pos) && pos > s->fimBusca + 0.25 &&
       (!s->buscaRecuo || pos < s->inicioBusca))) s->buscaPendente = 0;
  return s->buscaPendente;
}
#endif
