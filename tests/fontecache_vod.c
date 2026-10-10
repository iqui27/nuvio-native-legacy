// Metadados VOD: cache limitado, escopo e idade, com a busca real de addons.
// A rede aceita somente /stream/*.json; abrir video ou debrid falha o teste.
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../src/fontecache.c"

// Regional provider discovery has its own harness; keep this cache fixture
// limited to its mocked addon transport.
void ondever_pedir(const char *id, int series, long tmdb) {
  (void)id; (void)series; (void)tmdb;
}

int dados_gravar_leve(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }

static _Atomic Uint32 relogio = 100000;
static _Atomic int pedidos;
static const char *conta = "conta-a";
static int perfil = 1, fontesRede = 2, trocaNaRede;
static int nLista;
static Uint32 idadeLista;
static char pedidoAlvo[64], listaAlvo[64];
static Stream *listaAtiva;

Uint32 SDL_GetTicks(void) { return atomic_load(&relogio); }
const char *sessao_usuario(void) { return conta; }
int perfis_ativo(void) { return perfil; }
int player_carregando(void) { return 0; }
void debrid_nova_busca(void) { }
void debrid_definir_episodio(int t, int e) { (void)t; (void)e; }
int debrid_resolver(const char *hash, int indice, char *url, unsigned tam) {
  (void)hash; (void)indice; (void)url; (void)tam;
  assert(!"cache VOD nao pode resolver debrid"); return 0;
}
const char *i18n(const char *s) { return s; }
void marco(const char *s) { (void)s; }
const char *rede_url_publica(const char *url, char *d, unsigned tam) {
  snprintf(d, tam, "%s", url); return d;
}
const char *rede_ultimo_erro(void) { return ""; }
char *rede_baixar(const char *url, int timeout) {
  (void)timeout;
  assert(strstr(url, "/stream/") && strstr(url, ".json"));
  atomic_fetch_add(&pedidos, 1);
  if (strstr(url, "erro.invalid")) return NULL;
  if (trocaNaRede == 1) perfil = 2;
  if (trocaNaRede == 2) conta = "conta-b";
  if (trocaNaRede == 3) fontecache_vod_limpar();
  trocaNaRede = 0;
  return strdup("{\"streams\":[{\"url\":\"https://video.invalid/assinado.mp4\"}]}");
}
int stream_extrair(const char *json, const char *provedor, Stream **saida) {
  (void)json; (void)provedor;
  *saida = fontesRede ? calloc((size_t)fontesRede, sizeof(Stream)) : NULL;
  assert(!fontesRede || *saida);
  for (int i = 0; i < fontesRede; i++) {
    snprintf((*saida)[i].url, sizeof (*saida)[i].url, "https://video.invalid/%d", i);
    snprintf((*saida)[i].infoHash, sizeof (*saida)[i].infoHash, "%040d", i);
  }
  return fontesRede;
}
void stream_definir_alvo(const char *id) { snprintf(pedidoAlvo, sizeof pedidoAlvo, "%s", id); }
// F11: Jellyfin targets are routed away from addons; not exercised here.
#include "jellyfin.h"
int servidores_fontes_pedir(const char *alvo) { (void)alvo; return 0; }
int servidores_fontes_colher(const char *alvo, Stream **l, int *n) {
  (void)alvo; if (l) *l = NULL; if (n) *n = 0; return JF_FONTES_FALHOU; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
void stream_definir_lista_idade(const Stream *l, int n, Uint32 idade) {
  free(listaAtiva);
  listaAtiva = n > 0 ? malloc(sizeof(Stream) * (size_t)n) : NULL;
  if (listaAtiva) memcpy(listaAtiva, l, sizeof(Stream) * (size_t)n);
  nLista = n;
  idadeLista = idade;
  snprintf(listaAlvo, sizeof listaAlvo, "%s", pedidoAlvo);
}
void stream_definir_lista(const Stream *l, int n) { stream_definir_lista_idade(l, n, 0); }
// #221: a busca real publica por addon; a lista cresce sem ser trocada.
void stream_lista_acrescentar(const Stream *l, int n, int o) {
  Stream *t;
  (void)o;
  if (n <= 0) return;
  t = realloc(listaAtiva, sizeof(Stream) * (size_t)(nLista + n));
  assert(t);
  listaAtiva = t;
  memcpy(listaAtiva + nLista, l, sizeof(Stream) * (size_t)n);
  if (!nLista) { idadeLista = 0; snprintf(listaAlvo, sizeof listaAlvo, "%s", pedidoAlvo); }
  nLista += n;
}
void stream_invalidar(const char *p) { (void)p; stream_definir_lista(NULL, 0); listaAlvo[0] = 0; }
int stream_n(void) { return nLista; }
int stream_lista_do_alvo(const char *id) { return nLista > 0 && !strcmp(listaAlvo, id); }

static void buscarTituloModo(const char *id, const char *tipo, int renovar) {
  int limite;
  stream_definir_alvo(id);
  if (renovar) addons_buscar_renovar(id, tipo);
  else addons_buscar(id, tipo);
  for (limite = 0; limite < 4000 && addons_estado() == ADD_BUSCANDO; limite++) usleep(1000);
  assert(limite < 4000);
}
static void buscarTitulo(const char *id, const char *tipo) { buscarTituloModo(id, tipo, 0); }
static FontecacheEscopo escopo(void) {
  FontecacheEscopo e = {0};
  snprintf(e.conta, sizeof e.conta, "%s", conta);
  e.perfil = perfil; e.addons = addons_versao(); e.geracao = fontecache_vod_geracao();
  return e;
}
static size_t bytesCache(void) {
  size_t bytes = 0;
  pthread_mutex_lock(&trava);
  for (int i = 0; i < FONTECACHE_VOD_MAX; i++)
    bytes += (size_t)vod[i].resposta.n * sizeof(Stream);
  pthread_mutex_unlock(&trava);
  return bytes;
}
static void listaAddon(void) {
  AddonRemoto a = {0};
  strcpy(a.url, "https://addon.invalid/manifest.json");
  strcpy(a.nome, "Fixture"); a.ativo = 1;
  addons_definir_lista(&a, 1);
}
int main(void) {
  FontecacheEscopo e;
  Stream *l, *grande;
  int n, antes, teto = FONTECACHE_VOD_BYTES / sizeof(Stream);
  Uint32 idade;
  listaAddon();
  buscarTitulo("tt1", "movie");
  atomic_fetch_add(&relogio, 5000);
  buscarTitulo("tt2", "movie");
  atomic_fetch_add(&relogio, 5000);
  buscarTitulo("tt1", "movie");
  assert(pedidos == 2 && nLista == 2 && idadeLista == 10000);
  puts("3 navegacoes A/B/A: 2 HTTP de metadados, 0 video/debrid; idade 10000 ms preservada");
  // Renovar explicitamente o alvo ativo continua a rede.
  buscarTitulo("tt1", "movie");
  assert(pedidos == 3 && idadeLista == 0);
  puts("recarregar a lista ativa: 1 HTTP novo");
  atomic_fetch_add(&relogio, FONTECACHE_VOD_VALIDADE_MS);
  buscarTitulo("tt2", "movie");
  assert(pedidos == 4);

  fontecache_vod_limpar();
  buscarTitulo("tt10:1:1", "series");
  buscarTitulo("tt10:1:2", "series");
  buscarTitulo("tt10:1:1", "series");
  assert(pedidos == 6 && !strcmp(listaAlvo, "tt10:1:1"));
  e = escopo();
  assert(fontecache_vod_pegar("tt10:1:1", "movie", NULL, &e, &l, &n, &idade) == FC_NADA);
  assert(fontecache_vod_pegar("tt10:1:1", "series", "https://outro.invalid", &e, &l, &n, &idade) == FC_NADA);
  assert(fontecache_vod_pegar("tt10:1:1", "series", NULL, &e, &l, &n, &idade) == FC_ACERTO);
  strcpy(l[0].url, "alterada pelo consumidor"); free(l);
  assert(fontecache_vod_pegar("tt10:1:1", "series", NULL, &e, &l, &n, &idade) == FC_ACERTO);
  assert(strstr(l[0].url, "video.invalid")); free(l);

  antes = pedidos; perfil = 2;
  buscarTitulo("tt10:1:2", "series");
  assert(pedidos == antes + 1);
  antes = pedidos; conta = "conta-b";
  buscarTitulo("tt10:1:1", "series");
  assert(pedidos == antes + 1);
  fontecache_vod_limpar();
  for (int troca = 1; troca <= 3; troca++) {
    conta = "conta-a"; perfil = 1; trocaNaRede = troca;
    buscarTitulo("tt20", "movie");
    assert(!nLista && bytesCache() == 0 && addons_estado() == ADD_PARADO);
  }
  conta = "conta-a"; perfil = 1;
  buscarTitulo("tt30", "movie"); buscarTitulo("tt31", "movie");
  e = escopo();
  addons_alternar(0);
  assert(bytesCache() == 0);
  assert(fontecache_vod_pegar("tt30", "movie", NULL, &e, &l, &n, &idade) == FC_NADA);
  addons_alternar(0);

  // Exceder o teto nao trunca nem esconde fontes na lista publicada.
  fontecache_vod_limpar(); fontesRede = teto + 1;
  buscarTitulo("tt40", "movie");
  assert(nLista == teto + 1 && bytesCache() == 0);
  fontesRede = 2;
  buscarTitulo("tt41", "movie");
  // Simula a lista ativa vazia/filtrada, apesar da resposta crua cacheada.
  stream_definir_lista(NULL, 0);
  antes = pedidos;
  buscarTituloModo("tt41", "movie", 1);
  assert(pedidos == antes + 1 && nLista == 2 && idadeLista == 0);
  puts("recarregar explicito com lista ativa vazia: ignora cache e faz HTTP");
  // Uma resposta de parte dos addons continua visivel, mas nao vira cache.
  addons_adicionar("Falha fixture", "https://erro.invalid/manifest.json");
  buscarTitulo("tt42", "movie");
  assert(nLista == 2 && bytesCache() == 0);
  listaAddon();
  fontecache_vod_limpar();
  e = escopo();
  grande = calloc((size_t)teto + 1, sizeof(Stream)); assert(grande);
  for (int i = 0; i < 20; i++) {
    char id[64]; snprintf(id, sizeof id, "tt%d", 100 + i);
    atomic_fetch_add(&relogio, 1);
    fontecache_vod_guardar(id, "movie", NULL, &e, grande, teto, SDL_GetTicks());
    assert(bytesCache() <= FONTECACHE_VOD_MAX * (size_t)FONTECACHE_VOD_BYTES);
  }
  assert(bytesCache() == 2 * (size_t)teto * sizeof(Stream));
  printf("teto medido: %d fontes/lista; %zu B em 2 listas apos 20 titulos\n", teto, bytesCache());
  assert(fontecache_vod_pegar("tt117", "movie", NULL, &e, &l, &n, &idade) == FC_NADA);
  assert(fontecache_vod_pegar("tt119", "movie", NULL, &e, &l, &n, &idade) == FC_ACERTO); free(l);
  // Acertos nao renovam prazo; igualdade com TTL ja e expirada.
  atomic_fetch_add(&relogio, FONTECACHE_VOD_VALIDADE_MS);
  assert(fontecache_vod_pegar("tt119", "movie", NULL, &e, &l, &n, &idade) == FC_NADA);
  assert(bytesCache() == 0);
  fontecache_vod_guardar("tt200", "movie", NULL, &e, grande, teto + 1, SDL_GetTicks());
  assert(bytesCache() == 0);
  fontecache_vod_guardar("tt200", "movie", NULL, &e, grande, 0, SDL_GetTicks());
  assert(bytesCache() == 0);
  fontecache_vod_guardar("tt200", "movie", NULL, &e, grande, 1, SDL_GetTicks());
  assert(bytesCache() > 0);
  addons_esquecer();
  assert(bytesCache() == 0);
  fontecache_vod_guardar("tt201", "movie", NULL, &e, grande, 1, SDL_GetTicks());
  assert(bytesCache() == 0);
  free(grande); free(listaAtiva); listaAtiva = NULL;
  addons_encerrar();
  puts("fontecache VOD: PASS (HTTP, TTL, conta/perfil/episodio/config, respostas tardias, memoria)");
  return 0;
}
