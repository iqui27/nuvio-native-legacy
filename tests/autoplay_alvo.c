// BLOQUEADOR 2.0.3 (TCL do dono, 21:24, Narcos: Mexico tt8714904): o auto-play
// com "Somente add-ons instalados" abriu "MegaEmbed - 1080" (plugin). No log:
//
//   [fonte] +8 de Debridio ... +20 de AIOStreams (lista com 28)
//   [fonte] 28 fontes de tt8714904 descartadas: episode changed
//   fonte: lista com 0ms e de outro alvo, renovando (tt8714904:1:1)
//   [fonte] +2 de MegaEmbed (lista com 2)
//   [fonte] escolha com 2 fontes ...; faltam 0 addon(s)
//
// Duas coisas, ambas aqui com addons.c REAL (rede, video e debrid dublados;
// a lista de streams.c imitada com a mesma regra de carimbo: a lista herda o
// alvo do pedido quando e trocada, nao quando cresce):
//
//  1. O detalhe abre a serie antes de a lista de episodios chegar e carimba o
//     pedido com o id cru ("tt9"); addons.c pergunta "tt9:1:1" aos addons, mas
//     a lista fica carimbada "tt9". O player confere "tt9:1:1", acha "de outro
//     alvo" e descarta 28 fontes do MESMO episodio.
//  2. Descartada a lista, o mesmo alvo pedido de novo com a busca ainda no ar
//     (o plugin lento segura o fio) nao fazia nada: as fontes de add-on ja
//     publicadas sumiam e so o plugin que chegasse depois entrava na lista —
//     add-on e plugin da MESMA busca em geracoes diferentes.
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
// A entrega real tem o harness watchstate_poda.sh; aqui nao ha diario.
void wsenvio_podar(void) {}
void wsenvio_podar_addon(const char *base) { (void)base; }
#include <string.h>
#include <unistd.h>
#include "stub_perfisessao.h"
#include "../src/fontecache.c"

void ondever_pedir(const char *id, int series, long tmdb) {
  (void)id; (void)series; (void)tmdb;
}

static _Atomic int pedidos, soltarPlugin, pluginRodou;
static int nLista;
static char pedidoAlvo[64], listaAlvo[64];
static Stream *listaAtiva;
static int falhas;

#define CONFERE(cond, ...) do { \
    if (cond) { printf("  ok   "); printf(__VA_ARGS__); putchar('\n'); } \
    else { printf("  FALHA "); printf(__VA_ARGS__); printf("  [%s]\n", #cond); falhas++; } \
  } while (0)

Uint32 SDL_GetTicks(void) { return 100000; }
const char *sessao_usuario(void) { return "conta"; }
int perfis_ativo(void) { return 1; }
int player_carregando(void) { return 0; }
void debrid_nova_busca(void) { }
void debrid_definir_episodio(int t, int e) { (void)t; (void)e; }
int debrid_resolver_cache(const char *hash, int indice, char *url, unsigned tam, int *fora) {
  *fora = 0;
  (void)hash; (void)indice; (void)url; (void)tam;
  assert(!"nao resolve debrid"); return 0;
}
const char *i18n(const char *s) { return s; }
void marco(const char *s) { (void)s; }
const char *rede_url_publica(const char *url, char *d, unsigned tam) {
  snprintf(d, tam, "%s", url); return d;
}
const char *rede_ultimo_erro(void) { return ""; }
void stream_filtro_tipo(const char *type) { (void)type; }
void stream_folha_canal(int enabled) { (void)enabled; }
char *rede_baixar(const char *url, int timeout);
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers; char *body = rede_baixar(url, seconds);
  if (status) *status = body ? 200 : 0;
  return body;
}
char *rede_baixar(const char *url, int timeout) {
  (void)timeout;
  assert(strstr(url, "/stream/") && strstr(url, ".json"));
  // O id que vai ao addon e sempre o do episodio, nunca o cru.
  assert(strstr(url, "tt9%3A1%3A1") || strstr(url, "tt9:1:1"));
  atomic_fetch_add(&pedidos, 1);
  return strdup("{\"streams\":[]}");
}
int stream_extrair(const char *json, const char *provedor, Stream **saida) {
  (void)json;
  *saida = calloc(2, sizeof(Stream));
  assert(*saida);
  for (int i = 0; i < 2; i++) {
    snprintf((*saida)[i].url, sizeof (*saida)[i].url, "https://video.invalid/addon/%d.mkv", i);
    snprintf((*saida)[i].provedor, sizeof (*saida)[i].provedor, "%s", provedor);
  }
  return 2;
}
#include "jellyfin.h"
int servidores_fontes_pedir(const char *alvo) { (void)alvo; return 0; }
int servidores_fontes_colher(const char *alvo, Stream **l, int *n) {
  (void)alvo; if (l) *l = NULL; if (n) *n = 0; return JF_FONTES_FALHOU; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
void addonstats_registrar(const char *nome, unsigned ms, int respondeu) { (void)nome; (void)ms; (void)respondeu; }
int addonstats_segunda_s(const char *nome, int padraoS) { (void)nome; return padraoS; }
void addonstats_salvar(void) { }

// --- a lista, com a regra de carimbo de streams.c ---------------------------
void stream_definir_alvo(const char *id) { snprintf(pedidoAlvo, sizeof pedidoAlvo, "%s", id); }
void stream_definir_lista_idade(const Stream *l, int n, Uint32 idade) {
  (void)idade;
  free(listaAtiva);
  listaAtiva = n > 0 ? malloc(sizeof(Stream) * (size_t)n) : NULL;
  if (listaAtiva) memcpy(listaAtiva, l, sizeof(Stream) * (size_t)n);
  nLista = n;
  snprintf(listaAlvo, sizeof listaAlvo, "%s", pedidoAlvo);   // streams.c:476
}
void stream_preparar_dica_p2p(const Stream *l, int n) { (void)l; (void)n; }
void stream_definir_lista(const Stream *l, int n) { stream_definir_lista_idade(l, n, 0); }
void stream_lista_acrescentar(const Stream *l, int n, int o) {
  Stream *t;
  (void)o;
  if (n <= 0) return;
  t = realloc(listaAtiva, sizeof(Stream) * (size_t)(nLista + n));
  assert(t);
  listaAtiva = t;
  memcpy(listaAtiva + nLista, l, sizeof(Stream) * (size_t)n);
  nLista += n;                                               // o carimbo nao muda
}
void stream_invalidar(const char *p) {
  printf("  [fonte] %d fontes de %s descartadas: %s\n", nLista, listaAlvo[0] ? listaAlvo : "(sem alvo)", p);
  stream_definir_lista(NULL, 0);
  listaAlvo[0] = 0;                                          // streams.c:486
}
int stream_n(void) { return nLista; }
int stream_lista_do_alvo(const char *id) {
  return id && *id && listaAlvo[0] && nLista > 0 && !strcmp(listaAlvo, id);
}
static int contar(const char *provedor) {
  int k = 0;
  for (int i = 0; i < nLista; i++) if (!strcmp(listaAtiva[i].provedor, provedor)) k++;
  return k;
}

// --- o plugin lento (MegaEmbed): segura o fio da busca ----------------------
static int pluginAtivo(void) { return 1; }
static int pluginLento(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                       OrigemAviso aviso, void *avisoU, void *saida) {
  Stream *s;
  (void)tipo;
  assert(!strcmp(id, "tt9:1:1"));
  atomic_store(&pluginRodou, 1);
  if (aviso) aviso(avisoU, 0, "MegaEmbed", 1, NULL, 0);
  for (int k = 0; k < 5000 && !atomic_load(&soltarPlugin); k++) usleep(1000);
  if (cancelado && cancelado(ctx)) return 0;
  s = calloc(1, sizeof *s);
  assert(s);
  snprintf(s->url, sizeof s->url, "https://video.invalid/plugin.mp4");
  snprintf(s->provedor, sizeof s->provedor, "MegaEmbed");
  if (aviso) aviso(avisoU, 0, "MegaEmbed", 2, s, 1);
  *(Stream **)saida = s;
  return 1;
}

static void esperarAddon(void) {
  for (int k = 0; k < 4000 && contar("Debridio") < 2; k++) { addons_estado(); usleep(1000); }
}

int main(void) {
  AddonRemoto a = {0};
  int k;
  setvbuf(stdout, NULL, _IONBF, 0);
  strcpy(a.url, "https://addon.invalid/manifest.json");
  strcpy(a.nome, "Debridio"); a.ativo = 1;
  addons_definir_lista(&a, 1);
  addons_definir_origem_extra(pluginLento, pluginAtivo);

  puts("1. o detalhe abre a serie sem episodios ainda: carimba o id cru (app.c idDoAlvo)");
  stream_definir_alvo("tt9");
  addons_buscar("tt9", "series");
  esperarAddon();
  CONFERE(contar("Debridio") == 2, "as 2 fontes do add-on entraram aos poucos (%d)", contar("Debridio"));
  CONFERE(stream_lista_do_alvo("tt9:1:1"),
          "a lista e de tt9:1:1, o episodio pedido aos addons (carimbo: \"%s\")", listaAlvo);

  puts("2. o player confere o dono da lista (player.c) e o app renova (app.c renovarListaDoPlayer)");
  if (!stream_lista_do_alvo("tt9:1:1")) {
    stream_invalidar("episode changed");
  } else {
    // Sem o descarte falso, o caminho 2 ainda pode acontecer (lista apagada
    // por outro motivo com a busca no ar): forca-lo prova a coerencia.
    stream_invalidar("teste: lista apagada com a busca no ar");
  }
  stream_definir_alvo("tt9:1:1");
  addons_buscar("tt9:1:1", "series");
  addons_estado();
  CONFERE(contar("Debridio") == 2,
          "mesmo alvo pedido de novo com a busca no ar: as fontes de add-on voltam (%d)", contar("Debridio"));
  CONFERE(stream_lista_do_alvo("tt9:1:1"), "e a lista e de tt9:1:1 (carimbo: \"%s\")", listaAlvo);

  puts("3. o plugin lento responde e a busca termina");
  atomic_store(&soltarPlugin, 1);
  for (k = 0; k < 6000 && addons_estado() == ADD_BUSCANDO; k++) usleep(1000);
  assert(k < 6000 && atomic_load(&pluginRodou));
  CONFERE(contar("MegaEmbed") == 1, "o plugin entrou uma vez (%d)", contar("MegaEmbed"));
  CONFERE(contar("Debridio") == 2 && nLista == 3,
          "a lista final tem add-on E plugin da mesma busca (add-on %d, total %d)", contar("Debridio"), nLista);
  CONFERE(stream_lista_do_alvo("tt9:1:1"), "lista final de tt9:1:1 (carimbo: \"%s\")", listaAlvo);
  CONFERE(atomic_load(&pedidos) == 1, "sem pedido HTTP repetido ao add-on (%d)", atomic_load(&pedidos));

  addons_encerrar();
  free(listaAtiva);
  if (falhas) { printf("autoplay_alvo: FALHA (%d)\n", falhas); return 1; }
  puts("autoplay_alvo: PASSA");
  return 0;
}
