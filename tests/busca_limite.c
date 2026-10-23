// Actual discovery Search/parser/worker. Only HTTP and allocation failure
// are controlled; reuse the metadata harness's typed module boundaries.
#include <assert.h>
#include <stdlib.h>
#include "../src/catalogo.h"
static size_t loteTam;
static int medirLote, falharLote, alocacoesLote, liberacoesLote;
static void *lotePonteiro;
static void *buscaMalloc(size_t n) {
  if (medirLote && n == loteTam) {
    alocacoesLote++;
    if (falharLote) return NULL;
    assert(!lotePonteiro);
    return lotePonteiro = malloc(n);
  }
  return malloc(n);
}
static void buscaFree(void *p) {
  if (p && p == lotePonteiro) { liberacoesLote++; lotePonteiro = NULL; }
  free(p);
}
#define malloc buscaMalloc
#define free buscaFree
#define main detalhe_fixture_main
#include "detalheanime.c"
#undef main
#undef malloc
#undef free

static char *lerFixture(void) {
  FILE *f = fopen("tests/fixtures/busca_bigg_boss.json", "rb");
  assert(f && !fseek(f, 0, SEEK_END));
  long n = ftell(f);
  assert(n > 0 && !fseek(f, 0, SEEK_SET));
  char *b = malloc((size_t)n + 1);
  assert(b && fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f); return b;
}
static void resposta(const char *b) {
  nRotas = nPedidos = 0; pedidoMetaHook = NULL;
  rota("catalog.nuvio.tv", b); metaprov_zerar_pausa();
}
static void prepararWorker(const char *termo) {
  desc_alvos_busca_zerar();
  alvos[0] = alvos[1]; nAlvos = 1; proximoAlvo = 0; fiosVivos = 1;
  snprintf(buscaPedido, sizeof buscaPedido, "%s", termo);
  buscaTermo[0] = 0; respGeracao = -1;
  alocacoesLote = liberacoesLote = 0; falharLote = 0;
}
static void executarWorker(void) {
  medirLote = 1; fioBusca(NULL); medirLote = 0;
  assert(fiosVivos == 0 && !lotePonteiro);
}
static void trocarTermo(const char *url) {
  assert(strstr(url, "catalog.nuvio.tv"));
  pedidoMetaHook = NULL;
  geracao++;
  snprintf(buscaPedido, sizeof buscaPedido, "outro termo");
}
int main(void) {
  assert(DESC_BUSCA_POR_ALVO == 24);
  loteTam = sizeof(CatItem) * DESC_BUSCA_POR_ALVO;
  CatItem *itens = calloc(DESC_BUSCA_POR_ALVO, sizeof *itens);
  assert(itens);
  char *publica = lerFixture();
  resposta(publica); desc_alvos_busca_zerar();
  int n = consultarAlvo(&alvos[1], "Bigg boss", itens, DESC_BUSCA_POR_ALVO);
  assert(n == 14);
  // Integration promotes exact titles stably, then keeps provider order.
  const char *ordem[] = {
    "tt2911448", "tmdb:72908", "tmdb:11436", "tt15087662",
    "tt12547184", "tmdb:80435", "tmdb:298957", "tmdb:288099",
    "tt7805494", "tmdb:207608", "tmdb:331758", "tmdb:331722",
    "tmdb:158134", "tmdb:75441"
  };
  for (int i = 0; i < n; i++) assert(!strcmp(itens[i].imdb, ordem[i]));
  assert(!strcmp(itens[2].titulo, "Bigg Boss"));
  assert(!strcmp(itens[2].meta, "2006"));
  assert(!strcmp(itens[2].tipo, "series") && itens[2].poster[0]);
  puts("ok busca_limite: exact-title promotion preserves all provider identities");

  char corpo[8192];
  size_t k = (size_t)snprintf(corpo, sizeof corpo, "{\"metas\":[");
  for (int i = 0; i < 25; i++)
    k += (size_t)snprintf(corpo+k, sizeof corpo-k,
      "%s{\"id\":\"%s%d\",\"name\":\"Title %d\",\"type\":\"series\",\"poster\":\"https://p/%d.jpg\"}",
      i ? "," : "", i % 2 ? "tmdb:" : "tt", 9000+i, i, i);
  assert(k+3 < sizeof corpo); snprintf(corpo+k, sizeof corpo-k, "]}");
  resposta(corpo);
  assert(consultarAlvo(&alvos[1], "bounded", itens, DESC_BUSCA_POR_ALVO) == 24);
  assert(!strcmp(itens[0].imdb, "tt9000") && !strcmp(itens[23].imdb, "tmdb:9023"));
  puts("ok busca_limite: mixed IDs retain provider order; 25th is bounded");

  prepararWorker("bounded"); resposta(corpo); executarWorker();
  assert(resAlvo[0].n == 24 && resAlvo[0].geracao == geracao);
  assert(!strcmp(resAlvo[0].itens[23].imdb, "tmdb:9023"));
  assert(nPedidos == 1 && alocacoesLote == 1 && liberacoesLote == 1);
  puts("ok busca_limite: worker publishes all 24; one allocation and final free");

  prepararWorker("old"); resposta(corpo); pedidoMetaHook = trocarTermo;
  executarWorker();
  assert(resAlvo[0].n == 0 && respGeracao == -1 && !buscaTermo[0]);
  assert(nPedidos == 1 && liberacoesLote == 1);
  puts("ok busca_limite: changed generation rejects stale response and frees buffer");

  prepararWorker("outro termo"); resposta(corpo); executarWorker();
  assert(resAlvo[0].n == 24 && !strcmp(buscaTermo, "outro termo"));
  assert(resAlvo[0].geracao == geracao && liberacoesLote == 1);
  puts("ok busca_limite: new term publishes independently");

  prepararWorker("wrong session"); resposta(corpo); buscaSessao = 2;
  executarWorker();
  assert(nPedidos == 0 && resAlvo[0].n == 0 && liberacoesLote == 1);
  puts("ok busca_limite: invalid captured session cannot request or publish");

  prepararWorker("oom"); resposta(corpo); falharLote = 1; executarWorker();
  assert(proximoAlvo == 0 && nPedidos == 0 && resAlvo[0].n == 0);
  assert(alocacoesLote == 1 && liberacoesLote == 0);
  falharLote = 0; fiosVivos = 1; executarWorker();
  assert(proximoAlvo == 1 && nPedidos == 1 && resAlvo[0].n == 24);
  assert(alocacoesLote == 2 && liberacoesLote == 1);
  puts("ok busca_limite: OOM releases worker count without consuming queued target");

  prepararWorker("empty queue"); resposta(corpo); nAlvos = 0; executarWorker();
  assert(nPedidos == 0 && alocacoesLote == 1 && liberacoesLote == 1);
  puts("ok busca_limite: empty queue releases its buffer");
  free(publica); free(itens);
  NvPerfilSessao *sessao = pthread_getspecific(descSessaoChave);
  pthread_setspecific(descSessaoChave, NULL); free(sessao);
  return 0;
}
