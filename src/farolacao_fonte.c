// FAROL — ACOES DE FONTE (issue #15). Contrato em farolacao_fonte.h.
#include "farolacao_fonte.h"
#include "farol.h"
#include "farolregra.h"
#include "faroltexto.h"
#include "fontefalha.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "idioma.h"
#include "streams.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

// Bits da taxonomia persistente de fontefiltro.c (a ordem e parte do formato):
// visuais "HDR+DV", "Somente DV", "DV"; audios "DTS:X" .. "DTS".
#define BITS_DV   ((UINT64_C(1) << 0) | (UINT64_C(1) << 1) | (UINT64_C(1) << 5))
#define BITS_DTS  ((UINT64_C(1) << 3) | (UINT64_C(1) << 4) | (UINT64_C(1) << 5) | \
                   (UINT64_C(1) << 6) | (UINT64_C(1) << 7))

#define TENTADAS_MAX 64

// Estado folha, so em memoria. Uma trava; nenhuma chamada externa com ela.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static FonteFiltroConfig filtroTemp;
static atomic_int filtroTempAtivo;
static unsigned tentadaGer;
static int tentadas[TENTADAS_MAX], nTentadas;
// "Esconder DV nesta TV": o que a acao ACRESCENTOU ao filtro persistente, para
// o Desfazer tirar so isso (a pessoa pode ter mexido nos filtros depois).
static uint64_t dvAcrescentado;

int farolacao_fonte_acao_valida(int acao) {
  return acao == FAROL_A_PROXIMA_FONTE || acao == FAROL_A_FONTE_SEM_DV ||
         acao == FAROL_A_FONTE_SEM_DTS || acao == FAROL_A_FONTE_MENOR;
}

// --- tentadas ---------------------------------------------------------------
static int tentadaSemTrava(unsigned geracao, int indice) {
  if (geracao != tentadaGer) return 0;
  for (int i = 0; i < nTentadas; i++) if (tentadas[i] == indice) return 1;
  return 0;
}
void farolacao_fonte_tentada_marcar(unsigned geracao, int indice) {
  if (indice < 0) return;
  pthread_mutex_lock(&trava);
  if (geracao != tentadaGer) { tentadaGer = geracao; nTentadas = 0; }
  if (!tentadaSemTrava(geracao, indice)) {
    // Cheio: descarta a mais antiga; a tentada recente e a que importa.
    if (nTentadas == TENTADAS_MAX) {
      memmove(tentadas, tentadas + 1, (TENTADAS_MAX - 1) * sizeof tentadas[0]);
      nTentadas--;
    }
    tentadas[nTentadas++] = indice;
  }
  pthread_mutex_unlock(&trava);
}
int farolacao_fonte_tentada(unsigned geracao, int indice) {
  pthread_mutex_lock(&trava);
  int r = tentadaSemTrava(geracao, indice);
  pthread_mutex_unlock(&trava);
  return r;
}

// --- filtro temporario ------------------------------------------------------
int farolacao_fonte_filtro_temporario(FonteFiltroConfig *out) {
  if (!atomic_load(&filtroTempAtivo)) return 0;
  pthread_mutex_lock(&trava);
  int ativo = atomic_load(&filtroTempAtivo);
  if (ativo && out) *out = filtroTemp;
  pthread_mutex_unlock(&trava);
  return ativo;
}
int farolacao_fonte_filtro_temporario_definir(const FonteFiltroConfig *cfg) {
  if (cfg && !fontefiltro_config_validar(cfg)) return 0;
  pthread_mutex_lock(&trava);
  if (cfg) { filtroTemp = *cfg; atomic_store(&filtroTempAtivo, 1); }
  else { memset(&filtroTemp, 0, sizeof filtroTemp); atomic_store(&filtroTempAtivo, 0); }
  pthread_mutex_unlock(&trava);
  return 1;
}
void farolacao_fonte_encerrar(void) {
  pthread_mutex_lock(&trava);
  memset(&filtroTemp, 0, sizeof filtroTemp);
  atomic_store(&filtroTempAtivo, 0);
  nTentadas = 0; tentadaGer = 0;
  pthread_mutex_unlock(&trava);
}

// --- parte pura -------------------------------------------------------------
static const FarolFonteLinha *linhaAtual(const FarolFonteLinha *l, int n) {
  for (int i = 0; i < n; i++) if (l[i].atual) return &l[i];
  return NULL;
}
// "Menor" que a atual: o tamanho decide quando os dois sao conhecidos; sem
// isso o bitrate; so sem ambos a altura. Maior em bytes mas de resolucao menor
// NAO alivia a rede e nao conta.
static int menorQue(const FarolFonteLinha *c, const FarolFonteLinha *a) {
  if (!a) return 0;
  if (c->bytes && a->bytes) return c->bytes < a->bytes;
  if (c->kbps > 0 && a->kbps > 0) return c->kbps < a->kbps;
  if (c->altura > 0 && a->altura > 0) return c->altura < a->altura;
  return 0;
}
static int candidata(const FarolFonteLinha *c, unsigned geracao) {
  return !c->atual && c->ok && c->indice >= 0 && !farolacao_fonte_tentada(geracao, c->indice);
}
void farolacao_fonte_elegibilidade(const FarolFonteLinha *l, int n,
                                   unsigned geracao, FarolFonteElegib *out) {
  if (!out) return;
  memset(out, 0, sizeof *out);
  if (!l || n <= 0) return;
  const FarolFonteLinha *a = linhaAtual(l, n);
  for (int i = 0; i < n; i++) {
    if (!candidata(&l[i], geracao)) continue;
    out->outra = 1;
    if (!l[i].dv) out->semDv = 1;
    if (!l[i].dts) out->semDts = 1;
    if (menorQue(&l[i], a)) out->menor = 1;
  }
}
int farolacao_fonte_escolher(int acao, const FarolFonteLinha *l, int n, unsigned geracao) {
  if (!l || n <= 0 || !farolacao_fonte_acao_valida(acao)) return -1;
  const FarolFonteLinha *a = linhaAtual(l, n);
  for (int i = 0; i < n; i++) {
    if (!candidata(&l[i], geracao)) continue;
    switch (acao) {
      case FAROL_A_FONTE_SEM_DV:  if (l[i].dv) continue; break;
      case FAROL_A_FONTE_SEM_DTS: if (l[i].dts) continue; break;
      case FAROL_A_FONTE_MENOR:   if (!menorQue(&l[i], a)) continue; break;
      default: break;
    }
    return l[i].indice;
  }
  return -1;
}

// --- "esconder DV nesta TV" --------------------------------------------------
static void desfazerDvCb(const void *ctx) { (void)ctx; farolacao_fonte_desfazer_dv(); }

int farolacao_fonte_desfazer_dv(void) {
  pthread_mutex_lock(&trava);
  uint64_t bits = dvAcrescentado;
  dvAcrescentado = 0;
  pthread_mutex_unlock(&trava);
  if (!bits) return 0;
  FonteFiltroConfig f;
  fontefalha_filtro(&f);
  f.excluir[FONTEFILTRO_VISUAL] &= ~bits;
  return fontefalha_filtro_definir(&f);
}

// Grava o filtro persistente com os bits de DV e avisa na ilha com Desfazer.
// Se nao gravar, a acao ja valeu so com o filtro temporario.
static void esconderDvNestaTv(void) {
  FonteFiltroConfig f;
  fontefalha_filtro(&f);
  uint64_t novos = BITS_DV & ~f.excluir[FONTEFILTRO_VISUAL];
  f.excluir[FONTEFILTRO_VISUAL] |= BITS_DV;
  if (!fontefalha_filtro_definir(&f)) return;
  pthread_mutex_lock(&trava);
  dvAcrescentado = novos;
  pthread_mutex_unlock(&trava);
  IlhaAcao a;
  memset(&a, 0, sizeof a);
  a.icone = "check";
  a.frase = i18n(FT_ACAO_ESCONDER_DV);   // chave ja traduzida em idioma_tab.h
  a.onde = i18n("Filtros das fontes");
  a.tipo = ILHA_INFO;
  a.desfazer = novos ? desfazerDvCb : NULL;
  ilhaacao_feita(&a);
}

// --- executor ---------------------------------------------------------------
static uint64_t bitsDaAcao(int acao, int *categoria) {
  if (acao == FAROL_A_FONTE_SEM_DV) { *categoria = FONTEFILTRO_VISUAL; return BITS_DV; }
  if (acao == FAROL_A_FONTE_SEM_DTS) { *categoria = FONTEFILTRO_AUDIO; return BITS_DTS; }
  *categoria = -1;
  return 0;
}

int farolacao_fonte_executar(int acao, int persistir) {
  FarolFonteLinha linhas[FAROL_FONTE_MAX];
  unsigned geracao = 0;
  int cat = -1, ok = 0;
  uint64_t bits;
  if (!farolacao_fonte_acao_valida(acao)) return 0;
  farol_registrar_acao(acao);
  int n = stream_farol_fontes(linhas, FAROL_FONTE_MAX, &geracao);
  int indice = n > 0 ? farolacao_fonte_escolher(acao, linhas, n, geracao) : -1;
  if (indice < 0) { farol_registrar_resultado(acao, 0); return 0; }

  // Filtro desta reproducao ANTES da troca: a falha seguinte da sessao ja nasce
  // sem DV/DTS. O que havia fica guardado para voltar se a troca nao sair.
  FonteFiltroConfig antes, novo;
  int tinha = farolacao_fonte_filtro_temporario(&antes);
  bits = bitsDaAcao(acao, &cat);
  if (bits) {
    if (tinha) novo = antes; else memset(&novo, 0, sizeof novo);
    novo.excluir[cat] |= bits;
    if (!farolacao_fonte_filtro_temporario_definir(&novo)) bits = 0;
  }

  // A atual e a escolhida contam como tentadas: nenhuma das duas volta.
  for (int i = 0; i < n; i++) if (linhas[i].atual) farolacao_fonte_tentada_marcar(geracao, linhas[i].indice);
  ok = stream_farol_trocar(indice, geracao);
  if (ok) {
    farolacao_fonte_tentada_marcar(geracao, indice);
    // A unica gravacao persistente e a escolha explicita, depois da troca.
    if (acao == FAROL_A_FONTE_SEM_DV && persistir) esconderDvNestaTv();
  } else if (bits) farolacao_fonte_filtro_temporario_definir(tinha ? &antes : NULL);
  farol_registrar_resultado(acao, ok);
  return ok;
}
