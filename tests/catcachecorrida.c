// O CACHE DO CATALOGO NAO PODE LER O BLOCO DA TELA FORA DA TRAVA.
//
// Queda da 2.0.0 numa LG OLED (webOS 5+, Mali-G510): glibc aborta com
// "double free or corruption (!prev)", "malloc(): invalid next size
// (unsorted)", "malloc(): invalid size (unsorted)" ou o assert de sysmalloc,
// quase sempre logo depois de
//   [desc] catalogo montado com 113 titulos
//   [col] fileiras de catalogo=5 ... colecoes=190
//   [desc] fileiras remontadas sem rede: 6 de 6
//   [contalib] biblioteca da conta aplicada: 68 no catalogo (53 novos)
// e SEM a linha "[perf] cat cache: ..." que nas sessoes sem queda vem logo
// depois do contalib. Ou seja: o fio da descoberta estava codificando o cache
// (cat_gravar_cache_se_identidade) enquanto o fio principal acrescentava a
// biblioteca da conta (contalib_reconciliar -> cat_acrescentar_lote) e marcava
// naLista nos itens que ja estavam no bloco.
//
// A 2.0 passou a gravar o cache com RLE em DUAS passadas sobre `itens`: uma
// mede, malloc do tamanho medido, outra escreve. As duas liam o bloco da tela
// sem pubTrava. Se outro fio muda um byte zero para nao-zero entre elas (um
// naLista=1, um progresso) ou troca o bloco, a segunda passada sai MAIOR que a
// primeira e escreve alem do malloc: heap corrompido, abort no proximo free.
//
// Duas partes:
//   1. deterministica: o gancho NV_CAT_TEST_CACHE_MEIO roda entre as duas
//      passadas e faz o que o fio principal faz (aplica a biblioteca da conta).
//   2. corrida de verdade: fio da "descoberta" publicando, remontando sem rede
//      (com 190 pastas de colecao) e gravando o cache, contra o fio principal
//      rodando contalib_reconciliar e cat_quadro a cada "quadro".
//
//   bash tests/catcachecorrida.sh [publicacoes]   (ASan ligado por padrao)
//   NV_SO_CORRIDA=1 bash tests/catcachecorrida.sh  (so a parte 2)
#include "../src/catalogo.h"
#include "../src/colecoes.h"
#include "../src/contalib.h"
#include "../src/progresso.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "nvtmp.h"

// --- DUBLES ------------------------------------------------------------------
static char dirDados[512];
int         ajustes_idioma_ingles(void) { return 0; }
int         ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void) { return dirDados; }
const char *sessao_usuario(void) { return "0b8c3d1e-usuario-de-teste"; }
int         perfis_ativo(void) { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
const char *addons_base_por_id(const char *id) { (void)id; return "https://addon.exemplo/abc"; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}

// --- DADOS REALISTAS ---------------------------------------------------------
#define N_CAT   113   // "catalogo montado com 113 titulos"
#define N_FILS  6     // "fileiras remontadas sem rede: 6 de 6"
#define N_CONTA 68    // "68 no catalogo"
#define N_JA    13    // 68 - 55 novos
#define N_PASTAS 190  // "colecoes=190"

static CatItem lote[N_CAT];
static CatFileira fils[N_FILS];
static char *jsonConta;

static void montarLote(void) {
  int i;
  memset(lote, 0, sizeof lote);
  for (i = 0; i < N_CAT; i++) {
    CatItem *c = &lote[i];
    snprintf(c->imdb, sizeof c->imdb, "tt%07d", 100000 + i);
    snprintf(c->tipo, sizeof c->tipo, "%s", i % 2 ? "series" : "movie");
    snprintf(c->titulo, sizeof c->titulo, "Titulo do catalogo numero %d", i);
    snprintf(c->genero, sizeof c->genero, "Filme · Drama · Crime");
    snprintf(c->poster, sizeof c->poster,
             "https://image.tmdb.org/t/p/w500/poster%06d.jpg", i);
    snprintf(c->backdrop, sizeof c->backdrop,
             "https://image.tmdb.org/t/p/w1280/fundo%06d.jpg", i);
  }
  memset(fils, 0, sizeof fils);
  for (i = 0; i < N_FILS; i++) {
    CatFileira *f = &fils[i];
    snprintf(f->chave, sizeof f->chave, "addon.exemplo_movie_cat%d", i);
    snprintf(f->titulo, sizeof f->titulo, "Fileira %d", i);
    snprintf(f->tipo, sizeof f->tipo, "movie");
    snprintf(f->base, sizeof f->base, "https://addon.exemplo/abc");
    snprintf(f->catId, sizeof f->catId, "cat%d", i);
    f->ini = i * (N_CAT / N_FILS);
    f->n = N_CAT / N_FILS;
  }
}

// 68 linhas: 13 com titulo que ja esta no catalogo, 55 novas. Campos longos
// (nome, generos, poster) — mais compridos que os campos de destino.
static void montarJsonConta(void) {
  size_t cap = 400000, k = 0;
  int i, g;
  jsonConta = malloc(cap);
  assert(jsonConta);
  k += (size_t)snprintf(jsonConta + k, cap - k, "[");
  for (i = 0; i < N_CONTA; i++) {
    int id = i < N_JA ? 100000 + i * 7 : 900000 + i;
    k += (size_t)snprintf(jsonConta + k, cap - k,
        "%s{\"content_id\":\"tt%07d\",\"content_type\":\"%s\","
        "\"name\":\"Um titulo de biblioteca bem comprido para passar do campo "
        "%d: Subtitulo Estendido Edicao Especial do Diretor Remasterizada em 4K "
        "com Comentarios e Making Of e Cenas Deletadas\","
        "\"poster\":\"https://images.metahub.space/poster/medium/tt%07d/img?x=",
        i ? "," : "", id, i % 3 ? "movie" : "series", i, id);
    for (g = 0; g < 40; g++) k += (size_t)snprintf(jsonConta + k, cap - k, "abcdefghij");
    k += (size_t)snprintf(jsonConta + k, cap - k,
        "\",\"release_info\":\"2008-2013 e mais um texto que nao cabe em noventa "
        "e seis bytes de jeito nenhum porque e longo demais mesmo\","
        "\"imdb_rating\":\"8.%d\",\"added_at\":%lld,\"genres\":[",
        i % 10, 1700000000000LL + i * 1000LL);
    for (g = 0; g < 30; g++)
      k += (size_t)snprintf(jsonConta + k, cap - k, "%s\"Genero Muito Comprido %d\"",
                            g ? "," : "", g);
    k += (size_t)snprintf(jsonConta + k, cap - k, "]}");
    assert(k < cap - 4096);
  }
  snprintf(jsonConta + k, cap - k, "]");
}

static void montarColecoes(void) {
  size_t cap = 4u << 20, k = 0;
  char *json = malloc(cap);
  int p, s;
  assert(json);
  k += (size_t)snprintf(json + k, cap - k,
                        "{\"collections\":[{\"id\":\"c\",\"title\":\"C\",\"folders\":[");
  for (p = 0; p < N_PASTAS; p++) {
    k += (size_t)snprintf(json + k, cap - k,
                          "%s{\"id\":\"f%d\",\"title\":\"Pasta %d\",\"sources\":[",
                          p ? "," : "", p, p);
    for (s = 0; s < 4; s++)
      k += (size_t)snprintf(json + k, cap - k,
          "%s{\"addonBaseUrl\":\"https://addon.exemplo/abc\",\"type\":\"movie\","
          "\"catalogId\":\"col%d_%d\"}", s ? "," : "", p, s);
    k += (size_t)snprintf(json + k, cap - k, "]}");
  }
  snprintf(json + k, cap - k, "]}]}");
  if (col_definir_json(json) < 1) { puts("catcachecorrida: colecoes nao montaram"); exit(1); }
  free(json);
}

// O que desc_remontar_fileiras faz sem rede: pergunta a colecao por fileira
// (dentroDeColecaoVisivelBase) e republica a lista, as vezes em outra ordem.
static void remontarSemRede(int volta) {
  CatFileira saida[N_FILS];
  int i, q = 0;
  for (i = 0; i < N_FILS; i++) {
    const CatFileira *f = &fils[(i + volta) % N_FILS];
    (void)col_por_catalogo(f->base, f->tipo, f->catId);
    saida[q++] = *f;
  }
  cat_republicar_fileiras(saida, q);
}

// --- 1. DETERMINISTICO -------------------------------------------------------
static int noGancho;
void nv_cat_teste_cache_meio(void) {
  if (!noGancho) return;
  noGancho = 0;
  // O fio principal, entre as duas passadas do RLE.
  contalib_aplicar_catalogo();
}

static void parteDeterministica(void) {
  cat_definir_tudo(lote, N_CAT, fils, N_FILS);
  remontarSemRede(0);
  cat_quadro();
  noGancho = 1;
  assert(cat_gravar_cache_se_identidade("", sessao_usuario(), 1) == 1);
  assert(!noGancho);   // o gancho rodou
  assert(cat_n() == N_CAT + (N_CONTA - N_JA));
  cat_quadro();
  cat_quadro();
  printf("ok  cache gravado com a biblioteca da conta aplicada no meio\n");
}

// --- 2. CORRIDA --------------------------------------------------------------
static volatile int acabou;
static int voltasDesc = 40;

static void *fioDescoberta(void *u) {
  int v;
  (void)u;
  for (v = 0; v < voltasDesc; v++) {
    cat_definir_tudo(lote, N_CAT, fils, N_FILS);   // publicarMontagem
    remontarSemRede(v);                            // fileiras remontadas sem rede
    cat_gravar_cache_se_identidade("", sessao_usuario(), 1);
  }
  __atomic_store_n(&acabou, 1, __ATOMIC_RELEASE);
  return NULL;
}

static void parteCorrida(void) {
  pthread_t fio;
  int quadros = 0;
  acabou = 0;
  assert(pthread_create(&fio, NULL, fioDescoberta, NULL) == 0);
  while (!__atomic_load_n(&acabou, __ATOMIC_ACQUIRE)) {
    contalib_reconciliar();   // sync_passo, a cada quadro
    cat_quadro();             // comeco do quadro
    quadros++;
  }
  pthread_join(fio, NULL);
  cat_quadro();
  cat_quadro();
  printf("ok  corrida: %d publicacoes da descoberta, %d quadros\n", voltasDesc, quadros);
}

int main(int argc, char **argv) {
  const char *base = getenv("NV_TESTE_DIR");
  if (argc > 1) voltasDesc = atoi(argv[1]);
  snprintf(dirDados, sizeof dirDados, "%s/catcachecorrida-%d",
           base && *base ? base : nvt_dir(), (int)getpid());
  mkdir(dirDados, 0700);
  montarLote();
  montarJsonConta();
  montarColecoes();
  assert(contalib_ler_biblioteca(jsonConta) == N_CONTA);
  // NV_SO_CORRIDA=1 pula a parte 1: mede quantas vezes a corrida sozinha,
  // sem gancho nenhum, chega a corromper o heap.
  if (!getenv("NV_SO_CORRIDA")) parteDeterministica();
  parteCorrida();
  {
    char c[600];
    snprintf(c, sizeof c, "%s/catalogo-rede.bin", dirDados);
    remove(c);
    rmdir(dirDados);
  }
  free(jsonConta);
  printf("catcachecorrida: tudo ok\n");
  return 0;
}
