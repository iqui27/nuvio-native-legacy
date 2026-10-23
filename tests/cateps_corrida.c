// CORRIDA NAS FAIXAS DE EPISODIO DO CATALOGO (#203).
//
// Provado com TSAN no app inteiro: garantirFaixas (realloc de epIni/epQtd),
// chamada por cat_trocar_continuar no fio de "Continuar assistindo" DEPOIS de
// soltar pubTrava, contra cat_n_episodios no fio principal, sem trava nenhuma.
// E cat_definir_episodios, no fio buscarEps (um por pagina de serie),
// escrevendo epIni[i]/epQtd[i] no ponteiro que o realloc do outro fio acabou
// de liberar — escrita em heap liberado, que so explode depois num free()
// qualquer.
//
// Aqui, os tres papeis ao mesmo tempo:
//   - "continuar": cat_trocar_continuar com 1..CW_MAX itens (o catalogo cresce
//     e encolhe, entao as faixas sao realocadas) e cat_acrescentar de vez em
//     quando (cresce no fim);
//   - "buscarEps": cat_definir_episodios em indices variados;
//   - "principal": cat_n_episodios/cat_episodio por titulo e cat_quadro a cada
//     "quadro", como o desenho.
// O que o leitor recebe tem de ser coerente: episodio de 1 a qtd, temporada 1.
//
//   bash tests/cateps_corrida.sh                  (ASAN+UBSAN: falha sem a trava)
//   SANITIZE=thread bash tests/cateps_corrida.sh  (TSAN: falha sem a trava)
//   SANITIZE=0 bash tests/cateps_corrida.sh       (so os asserts; nao pega)
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { (void)g; return 1; }
int perfis_sessao_travar(uint64_t g) { (void)g; return 1; }
void perfis_sessao_destravar(void) {}

// --- DUBLES ------------------------------------------------------------------
int         ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *dados_dir(void) { return ""; }
const char *sessao_usuario(void) { return ""; }
int         perfis_ativo(void) { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}
// Um registro de progresso, para a aplicacao do disco ter o que fazer em cada
// troca (e o caminho de aplicarUm que o TSAN tambem pegou).
int prog_ler(ProgRegistro *saida, int max) {
  if (max < 1) return 0;
  memset(saida, 0, sizeof *saida);
  snprintf(saida->contentId, sizeof saida->contentId, "tt0000003");
  saida->posSeg = 600; saida->durSeg = 1200;
  saida->temporada = 1; saida->episodio = 2;
  return 1;
}

#define N_BASE 300
#define CW_MAX 40
#define EP_MAX 60
#define VOLTAS 400

static atomic_int fim, cwFim;

static void item(CatItem *c, int i, const char *pre) {
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s%07d", pre, i);
  snprintf(c->tipo, sizeof c->tipo, "series");
  snprintf(c->titulo, sizeof c->titulo, "Titulo %d", i);
}

static void *continuar(void *u) {
  static CatItem cw[CW_MAX];
  int v, k;
  (void)u;
  for (v = 0; v < VOLTAS; v++) {
    int q = 1 + (v * 7) % CW_MAX;
    for (k = 0; k < q; k++) item(&cw[k], k, "tt");
    cat_trocar_continuar(cw, q);
    if (v % 16 == 0) { CatItem c; item(&c, 900000 + v, "tt"); cat_acrescentar(&c); }
  }
  atomic_store(&cwFim, 1);
  return NULL;
}

static void *buscarEps(void *u) {
  static CatEp eps[EP_MAX];
  int k, v = 0;
  (void)u;
  for (k = 0; k < EP_MAX; k++) {
    memset(&eps[k], 0, sizeof eps[k]);
    eps[k].temporada = 1;
    eps[k].episodio = k + 1;
    snprintf(eps[k].nome, sizeof eps[k].nome, "Episodio %d", k + 1);
  }
  while (!fim) {
    int idx = (v * 13) % (cat_n() > 0 ? cat_n() : 1);
    cat_definir_episodios(idx, eps, 1 + v % EP_MAX);
    v++;
  }
  return NULL;
}

int main(void) {
  static CatItem base[N_BASE];
  pthread_t tc, tb;
  long lidos = 0;
  int i, quadros = 0;
  for (i = 0; i < N_BASE; i++) item(&base[i], i, "tt");
  cat_definir(base, N_BASE);
  fim = 0;
  pthread_create(&tb, NULL, buscarEps, NULL);
  pthread_create(&tc, NULL, continuar, NULL);
  // O fio principal: le como o desenho e vira quadro, ate "continuar" acabar.
  for (;;) {
    int m = cat_n(), t;
    for (t = 0; t < m; t += 7) {
      int q = cat_n_episodios(t), e;
      assert(q >= 0 && q <= 1200);
      for (e = 0; e < q; e++) {
        const CatEp *ep = cat_episodio(t, e);
        if (!ep) break;   // a faixa pode ter sido zerada entre as duas chamadas
        assert(ep->temporada == 1 && ep->episodio >= 1 && ep->episodio <= EP_MAX);
        lidos++;
      }
    }
    (void)cat_revisao();
    cat_quadro();
    quadros++;
    if (atomic_load(&cwFim)) break;
    { struct timespec ts = {0, 200000}; nanosleep(&ts, NULL); }
  }
  pthread_join(tc, NULL);
  fim = 1;
  pthread_join(tb, NULL);
  cat_quadro();
  printf("cateps_corrida: ok (%d quadros, %ld episodios lidos)\n", quadros, lidos);
  return 0;
}
