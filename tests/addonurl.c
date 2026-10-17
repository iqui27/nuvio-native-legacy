// A URL DE UM ADDON CHEGA INTEIRA AO PEDIDO? (#201)
//
// Este teste existe por um defeito mudo: a base do addon era guardada em
// `char[600]`, e a URL do Comet — que embute a configuracao inteira num base64
// de ~830 caracteres — tem 870. Ela era cortada em silencio, o Comet recebia
// meia configuracao e respondia o manifesto "❌ | Comet" com uma fonte so. Nada
// no log dizia que a URL tinha sido cortada.
//
// O que se confere, com uma URL de 1500 caracteres:
//   1. instalar (addons_adicionar) guarda a base inteira;
//   2. exportar (o que o sync grava em disco e empurra para a conta) devolve a
//      mesma base;
//   3. recarregar da conta (addons_definir_lista) e do arquivo (addons_carregar)
//      devolve a mesma base;
//   4. o pedido de fontes sai para <base>/stream/movie/<id>.json, byte a byte.
// E, com uma URL acima de NV_ADDON_URL_MAX: ela e RECUSADA, nunca cortada —
// nenhum pedido sai para ela, e o log diz o nome do addon e o tamanho. O .sh
// confere a linha e confere que a URL em si nao apareceu no log.
//
// So addons.c e js.c; a rede e um duble que anota a URL pedida.
#include <stdio.h>
#include "nvtmp.h"
// A entrega real tem o harness watchstate_poda.sh; aqui nao ha diario.
void wsenvio_podar(void) {}
void wsenvio_podar_addon(const char *base) { (void)base; }
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "addons.h"
#include "fontecache.h"
#include "stub_perfisessao.h"

// ---------------------------------------------------------------- duble
static char pedido[16384];      // a ultima URL que chegou a rede
static int  nPedidos, nPedidosGrande;
const char *rede_ultimo_erro(void) { return ""; }
int dados_gravar_leve(const char *n, const char *c) { (void)n; (void)c; return 1; }
char *rede_baixar(const char *url, int s) {
  (void)s;
  // O que rede.c faz no OPT_URL: apelido de URL grande -> URL inteira (#203).
  char *grande = nv_longa_expandir(url);
  nPedidos++;
  if (strstr(url, "grande.test")) nPedidosGrande++;
  snprintf(pedido, sizeof pedido, "%s", grande ? grande : url);
  free(grande);
  return strdup("{\"streams\":[{\"url\":\"https://x/a.mp4\"}]}");
}
void ondever_pedir(const char *id, int series, long tmdb) { (void)id; (void)series; (void)tmdb; }
int stream_extrair(const char *json, const char *prov, Stream **saida) {
  int n = 0; const char *p = json;
  (void)prov;
  while ((p = strstr(p, "\"url\"")) != NULL) { n++; p += 5; }
  *saida = n ? calloc((size_t)n, sizeof(Stream)) : NULL;
  return n;
}
#include "jellyfin.h"
int servidores_fontes_pedir(const char *alvo) { (void)alvo; return 0; }
int servidores_fontes_colher(const char *alvo, Stream **l, int *n) {
  (void)alvo; if (l) *l = NULL; if (n) *n = 0; return JF_FONTES_FALHOU; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
void stream_folha_canal(int sim) { (void)sim; }
void stream_filtro_tipo(const char *tipo) { (void)tipo; }
void stream_definir_lista(const Stream *l, int n) { (void)l; (void)n; }
void stream_preparar_dica_p2p(const Stream *l, int n) { (void)l; (void)n; }
void stream_definir_lista_idade(const Stream *l, int n, Uint32 idade) {
  (void)idade; stream_definir_lista(l, n); }
void stream_lista_acrescentar(const Stream *l, int n, int o) { (void)l; (void)n; (void)o; }
void stream_invalidar(const char *p) { (void)p; }
int stream_n(void) { return 0; }
int stream_lista_do_alvo(const char *id) { (void)id; return 0; }
// addons.c recarimba o id normalizado ("tt:1:1") do pedido.
void stream_definir_alvo(const char *id) { (void)id; }
Uint32 SDL_GetTicks(void) { return 1000; }
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
void debrid_definir_episodio(int t, int e) { (void)t; (void)e; }
void debrid_nova_busca(void) {}
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
// So o host, como o de verdade: o caminho carrega a chave do addon.
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  const char *p = url ? strstr(url, "://") : NULL;
  p = p ? p + 3 : (url ? url : "");
  snprintf(dst, tam, "%.*s", (int)strcspn(p, "/"), p); return dst; }
void marco(const char *s) { (void)s; }
int  fontecache_pegar(const char *id, const char *tipo, Stream **l, int *n) {
  (void)id; (void)tipo; *l = NULL; *n = 0; return FC_NADA; }
void fontecache_guardar_sessao(const char *id, const char *tipo, const Stream *l, int n, uint64_t sessao) {
  (void)id; (void)tipo; (void)l; (void)n; (void)sessao; }
void fontecache_ceder(void) {}
void fontecache_avancar(void) {}
unsigned fontecache_vod_geracao(void) { return 0; }
void fontecache_vod_limpar(void) {}
void fontecache_vod_apagar(const char *id, const char *tipo, const char *origem,
                          const FontecacheEscopo *escopo) {
  (void)id; (void)tipo; (void)origem; (void)escopo;
}
void fontecache_vod_guardar(const char *id, const char *tipo, const char *origem,
                           const FontecacheEscopo *escopo,
                           const Stream *l, int n, Uint32 quando) {
  (void)id; (void)tipo; (void)origem; (void)escopo; (void)l; (void)n; (void)quando;
}
int fontecache_vod_pegar(const char *id, const char *tipo, const char *origem,
                        const FontecacheEscopo *escopo,
                        Stream **l, int *n, Uint32 *idade) {
  (void)id; (void)tipo; (void)origem; (void)escopo;
  *l = NULL; *n = 0; *idade = 0; return FC_NADA;
}

// ---------------------------------------------------------------- roteiro
static int falhas;
static void conferir(const char *o_que, long obtido, long esperado) {
  if (obtido == esperado) return;
  printf("FALHOU %s: obtido %ld, esperado %ld\n", o_que, obtido, esperado);
  falhas++;
}
// Compara sem imprimir nenhuma das duas: sao URLs de addon. Diz so onde
// divergem.
static void conferirUrl(const char *o_que, const char *obtido, const char *esperado) {
  size_t i = 0;
  if (!strcmp(obtido, esperado)) return;
  while (obtido[i] && obtido[i] == esperado[i]) i++;
  printf("FALHOU %s: %lu caracteres (esperado %lu), diverge na posicao %lu\n", o_que,
         (unsigned long)strlen(obtido), (unsigned long)strlen(esperado), (unsigned long)i);
  falhas++;
}

// "https://<host>/SEGREDO<enchimento>/manifest.json" com `total` caracteres. O
// enchimento NAO e uniforme: um corte ou um deslocamento muda o texto. A
// palavra SEGREDO esta la para o .sh provar que a URL nao foi parar no log.
static void montarUrl(char *dst, size_t total, const char *host) {
  size_t k = (size_t)snprintf(dst, total + 1, "https://%s/SEGREDO", host), i = 0;
  while (k < total - 14) dst[k++] = (char)('a' + (int)((i++ * 7) % 26));
  snprintf(dst + k, 15, "/manifest.json");
}

static void buscar(const char *id) {
  pedido[0] = 0;
  addons_buscar(id, "movie");
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
}

int main(void) {
  static char longa[1501], grande[2101], base[1501], esperado[1700];
  static AddonRemoto conta[2];
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-addonurl-XXXXXX");
  char caminho[600];
  FILE *f;

  montarUrl(longa, 1500, "longo.test");
  montarUrl(grande, 2100, "grande.test");
  snprintf(base, sizeof base, "%.*s", 1500 - 14, longa);   // sem /manifest.json
  conferir("a URL longa tem 1500 caracteres", (long)strlen(longa), 1500);
  conferir("a URL grande passa do limite", strlen(grande) >= NV_ADDON_URL_MAX, 1);

  // 1. instalar
  conferir("addon de URL longa entra", addons_adicionar("Longo", longa), 1);
  conferir("um addon na lista", addons_n(), 1);
  conferirUrl("base guardada", addons_base(0), base);

  // 2. o que o sync grava e empurra
  conferir("exporta um", addons_exportar(conta, 2), 1);
  conferirUrl("base exportada", conta[0].url, base);

  // 3a. recarregar da conta
  addons_esquecer();
  conferir("lista esquecida", addons_n(), 0);
  conferir("a lista da conta e aplicada", addons_definir_lista(conta, 1), 1);
  conferirUrl("base vinda da conta", addons_base(0), base);
  // A mesma lista de novo NAO muda nada (listaIgual compara a base inteira).
  conferir("lista identica nao reaplica", addons_definir_lista(conta, 1), 0);

  // 4. o pedido
  snprintf(esperado, sizeof esperado, "%s/stream/movie/tt0111161.json", base);
  buscar("tt0111161");
  conferir("a busca terminou com fonte", addons_estado(), ADD_PRONTO);
  conferirUrl("pedido de fontes", pedido, esperado);

  // 3b. recarregar do arquivo, com a linha de 1500 e uma linha acima do limite
  if (!mkdtemp(dir)) { printf("FALHOU: sem diretorio temporario\n"); return 1; }
  snprintf(caminho, sizeof caminho, "%s/addons.txt", dir);
  f = fopen(caminho, "w");
  if (!f) { printf("FALHOU: sem arquivo temporario\n"); return 1; }
  fprintf(f, "Grande\t%s\t1\t1\t0\n", grande);
  fprintf(f, "Longo\t%s\t1\t1\t0\n", longa);
  fprintf(f, "Curto\thttps://curto.test/manifest.json\n");
  fclose(f);
  conferir("o arquivo traz dois (o grande fica de fora)", addons_carregar(dir), 2);
  conferirUrl("base vinda do arquivo", addons_base(0), base);
  conferirUrl("a linha seguinte nao foi comida", addons_base(1), "https://curto.test");
  unlink(caminho); rmdir(dir);

  // ---- acima do limite (#203): entra como apelido, e o pedido leva a URL INTEIRA
  { static char enorme[7001], baseE[7001], exp[7100];
    static AddonRemoto sai[4];
    const char *bruto;
    montarUrl(enorme, 7000, "grande.test");
    snprintf(baseE, sizeof baseE, "%.*s", 7000 - 14, enorme);
    conferir("addon de URL de 7 KB entra", addons_adicionar("Grande", enorme), 1);
    conferir("a lista agora tem tres", addons_n(), 3);
    conferir("a base guardada e curta", strlen(addons_base(2)) < 100, 1);
    conferir("o mesmo de novo ja esta instalado", addons_adicionar("Grande", enorme), 0);
    conferir("exporta tres", addons_exportar(sai, 4), 3);
    bruto = nv_longa_bruto(sai[2].url);
    conferir("o apelido volta a URL exata que chegou", bruto != NULL && !strcmp(bruto, enorme), 1);
    nPedidos = nPedidosGrande = 0;
    buscar("tt0068646");
    conferir("os tres addons foram consultados", nPedidos, 3);
    conferir("um pedido saiu para a URL grande", nPedidosGrande, 1);
    snprintf(exp, sizeof exp, "%s/stream/movie/tt0068646.json", baseE);
    { char *e2 = nv_longa_expandir(sai[2].url), *e3;
      char ped[200];
      snprintf(ped, sizeof ped, "%s/stream/movie/tt0068646.json", sai[2].url);
      e3 = nv_longa_expandir(ped);
      conferir("expandir o pedido de 7 KB", e3 != NULL && !strcmp(e3, exp), 1);
      free(e2); free(e3); }
    // com query: o caminho entra antes dela
    { static char q[7100]; char ped[200], *e;
      snprintf(q, sizeof q, "%.*s/manifest.json?tok=abc", 6000, enorme);
      snprintf(baseE, sizeof baseE, "%.*s", 6000, enorme);
      conferir("URL de 6 KB com query entra", addons_adicionar("Q", q), 1);
      snprintf(ped, sizeof ped, "%s/catalog/movie/top.json?skip=20", addons_base(3));
      e = nv_longa_expandir(ped);
      snprintf(exp, sizeof exp, "%s/catalog/movie/top.json?tok=abc&skip=20", baseE);
      conferir("query do addon viaja com a do pedido", e != NULL && !strcmp(e, exp), 1);
      free(e); }
    addons_esquecer(); addons_adicionar("A", longa); addons_adicionar("B", "https://curto.test/manifest.json"); }
  nPedidos = nPedidosGrande = 0;
  buscar("tt0068646");
  conferir("os dois addons foram consultados", nPedidos, 2);

  // ---- paridade com o Nuvio oficial (#202): a query viaja, o id e codificado
  { char u[256];
    int w;
    // nv_addon_base: a mesma canonicalizeUrl/normalizeAddonUrl do Nuvio web
    nv_addon_base("  https://x.test/a/manifest.json?  ", u, sizeof u);
    conferirUrl("espaco, manifest e '?' vazio saem", u, "https://x.test/a");
    nv_addon_base("stremio://s.test/abc/Manifest.json", u, sizeof u);
    conferirUrl("stremio:// vira https:// e Manifest.json sai sem caixa", u, "https://s.test/abc");
    nv_addon_base("https://q.test/cfg/manifest.json?ver=3&k=a%2Fb", u, sizeof u);
    conferirUrl("a query fica na base", u, "https://q.test/cfg?ver=3&k=a%2Fb");
    nv_addon_base("https://q.test/cfg//?x=1", u, sizeof u);
    conferirUrl("barra antes da query sai", u, "https://q.test/cfg?x=1");
    nv_addon_base("https://q.test/cfg/manifest.json?muito-longa", u, 20);
    conferirUrl("base que nao cabe sai vazia", u, "");
    // nv_addon_url: o caminho entra ANTES da query
    w = nv_addon_url(u, sizeof u, "https://q.test/cfg?ver=3", "/stream/%s/%s.json", "movie", "tt1");
    conferirUrl("caminho antes da query", u, "https://q.test/cfg/stream/movie/tt1.json?ver=3");
    conferir("retorno = tamanho", w, (long)strlen(u));
    w = nv_addon_url(u, sizeof u, "https://q.test/cfg", "/manifest.json");
    conferirUrl("sem query, como antes", u, "https://q.test/cfg/manifest.json");
    w = nv_addon_url(u, 30, "https://q.test/cfg?ver=3", "/stream/%s/%s.json", "movie", "tt1");
    conferir("pedido que nao cabe: retorno >= tamanho", w >= 30, 1);
    conferirUrl("pedido que nao cabe sai vazio", u, "");
    // nv_addon_id: o que nao e pchar vira %XX; ":" e id comum saem iguais
    nv_addon_id(u, sizeof u, "tt0111161:1:2");
    conferirUrl("id de episodio igual", u, "tt0111161:1:2");
    nv_addon_id(u, sizeof u, "iptv:Canal Um/HD#1?a%\xc3\xa7");
    conferirUrl("id com espaco, barra, #, ?, % e acento", u, "iptv:Canal%20Um%2FHD%231%3Fa%25%C3%A7");
    conferir("id que nao cabe", nv_addon_id(u, 6, "abc def"), 0); }

  // E o pedido de verdade, pelo caminho da busca de fontes.
  addons_esquecer();
  conferir("addon com query entra", addons_adicionar("Query", "https://q.test/cfg/manifest.json?ver=3"), 1);
  conferirUrl("base com query", addons_base(0), "https://q.test/cfg?ver=3");
  buscar("tt0111161");
  conferirUrl("fontes: caminho antes da query", pedido, "https://q.test/cfg/stream/movie/tt0111161.json?ver=3");
  conferir("a mesma URL sem /manifest.json e o mesmo addon",
           addons_adicionar("Query", "https://q.test/cfg?ver=3"), 0);
  addons_esquecer();
  conferir("addon stremio:// entra", addons_adicionar("Canal", "stremio://c.test/x/manifest.json"), 1);
  buscar("iptv:Canal Um");
  conferirUrl("fontes: id codificado, https", pedido, "https://c.test/x/stream/movie/iptv:Canal%20Um.json");

  printf(falhas ? "addonurl: FALHOU (%d)\n" : "addonurl: ok\n", falhas);
  return falhas ? 1 : 0;
}
