// O que addons_carregar tira de addons.txt fica USAVEL?
//
// Este teste existe por um defeito mudo: a leitura do arquivo preenchia nome,
// base e as tres capacidades e NAO preenchia `ativo`. O vetor de addons e
// estatico, nasce zerado, entao todo addon do arquivo ficava desligado — e
// desligado, em addons.c, quer dizer "nao consultado" por fonte, por legenda e
// por catalogo. Na tela ele aparecia; na busca de fontes ele nao existia, sem
// uma linha de log dizendo por que. E a forma extrema do sintoma da issue #83
// ("certain addons not showing").
//
// So o leitor: nada aqui vai a rede.
#include <stdio.h>
#include "nvtmp.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "addons.h"
#include "plugins.h"
#include "fontecache.h"
#include "stub_perfisessao.h"

// Provider discovery is tested in ondever_lookup.c. This isolated addon
// transport fixture must not start an unrelated TMDB request.
void ondever_pedir(const char *id, int series, long tmdb) {
  (void)id; (void)series; (void)tmdb;
}

int dados_gravar_leve(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }
void sync_sujar_addons(const char *url) { (void)url; }

// ---------------------------------------------------------------- duble
//
// A BUSCA DE FONTES DE VERDADE (addons_buscar -> fio -> addons_estado), com a
// rede e o parser trocados por dubles. O que se confere e o RESUMO que a folha
// de fontes vazia usa para dizer a causa (B6/#107, D5): quem foi consultado,
// quem respondeu vazio, quem nao respondeu.
//
// resp[0] e o corpo de exemplo.test, resp[1] o de antigo.test; NULL = sem
// resposta (rede_baixar devolve NULL em falha e em 4xx).
static const char *resp[2];
static int nDebridNovaBusca;

// canal.test imita o FrostView TV medido no #112: 200 {"streams":[]} para
// /stream/tv/ e a lista de verdade para /stream/channel/. `pedidosCanal`
// guarda o tipo de cada pedido, na ordem, para conferir quem foi primeiro.
static const char *respCanalTv, *respCanalChannel;
static char pedidosCanal[200];
// live.test: o addon de IPTV caseiro do relato — manifesto MISTO (movie +
// series + catalogo "live") e /stream/ so responde por "live".
static const char *respCanalLive;
static char pedidosLive[64];
// #588: custom catalog/item types must reach the stream endpoint unchanged.
static char pedidosSport[256];
static const char *tipoSport = "sport";
static int sportVazio;
static unsigned sportDelayMs;
static int sportTimeouts[16], sportRequests;
static _Thread_local Uint32 sportTicks;

// lento.test (#182): as primeiras `falhasLento` requisicoes NAO respondem (o
// timeout do AIOStreams frio); as seguintes respondem com uma fonte.
static int pedidosDesligado, pedidosLigado;
static int falhasLento, chamadasLento, prazoLento[2], prazoCanal;
// anime.test e o ARM falso: cada pedido, na ordem ("ids?source=kitsu&id=7442,...").
static char pedidosAnime[512];
static pthread_mutex_t pedidosAnimeTrava = PTHREAD_MUTEX_INITIALIZER;   // fios em paralelo
// legenda.test imita o OpenSubtitles v3 vindo da conta (TCL do dono, 09/10):
// instalado sem capacidades (nasce "fonte"), o manifesto diz so "subtitles" e
// /stream/ responde 404.
static int pedidosLegendaStream, pedidosLegendaManifesto, idxLegenda = -1;
char *rede_baixar(const char *url, int seconds);
char *rede_baixar_st(const char *url, int seconds, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, seconds);
  if (status) *status = body ? 200 : 0;
  return body;
}
const char *rede_ultimo_erro(void) { return ""; }
char *rede_baixar(const char *url, int s) {
  const char *r;
  if (strstr(url, "legenda.test")) {
    if (strstr(url, "/manifest.json")) {
      pedidosLegendaManifesto++;
      return strdup("{\"id\":\"org.stremio.opensubtitlesv3\",\"name\":\"OpenSubtitles v3\","
                    "\"resources\":[\"subtitles\"],\"types\":[\"movie\",\"series\"]}");
    }
    if (strstr(url, "/stream/")) {
      // A sonda (sondar, outro fio) termina DURANTE a busca, como na TCL: o
      // manifesto chega depois de a lista de baldes ser montada.
      if (!pedidosLegendaStream++ && idxLegenda >= 0)
        addons_manifesto_lido(idxLegenda,
          "{\"id\":\"org.stremio.opensubtitlesv3\",\"name\":\"OpenSubtitles v3\","
          "\"resources\":[\"subtitles\"],\"types\":[\"movie\",\"series\"]}");
    }
    return NULL;
  }
  if (strstr(url, "desligado.test")) pedidosDesligado++;
  if (strstr(url, "ligado.test") && !strstr(url, "desligado.test")) pedidosLigado++;
  if (strstr(url, "lento.test")) {
    if (chamadasLento < 2) prazoLento[chamadasLento] = s;
    if (++chamadasLento <= falhasLento) return NULL;
    return strdup("{\"streams\":[{\"url\":\"https://x/l.mp4\"}]}");
  }
  if (strstr(url, "sport.test")) {
    const char *p = strstr(url, "/stream/");
    if (!p) return NULL;
    p += strlen("/stream/");
    const char *fim = strchr(p, '/');
    if (!fim) return NULL;
    if (sportDelayMs) {
      if (sportRequests < 16) sportTimeouts[sportRequests] = s;
      sportRequests++;
      sportTicks += sportDelayMs < (unsigned)s * 1000u ? sportDelayMs : (unsigned)s * 1000u;
    }
    snprintf(pedidosSport + strlen(pedidosSport), sizeof pedidosSport - strlen(pedidosSport),
             "%.*s,", (int)(fim - p), p);
    if ((size_t)(fim - p) == strlen(tipoSport) && !strncmp(p, tipoSport, strlen(tipoSport)))
      return strdup("{\"streams\":[{\"url\":\"https://x/sport.m3u8\"}]}");
    return sportVazio ? strdup("{\"streams\":[]}") : NULL;
  }
  if (strstr(url, "live.test")) {
    int live = strstr(url, "/stream/live/") != NULL;
    strncat(pedidosLive, live ? "live," : strstr(url, "/stream/tv/") ? "tv," : "channel,",
            sizeof pedidosLive - strlen(pedidosLive) - 1);
    return live && respCanalLive ? strdup(respCanalLive) : NULL;
  }
  if (strstr(url, "canal.test")) {
    prazoCanal = s;
    int tv = strstr(url, "/stream/tv/") != NULL;
    strncat(pedidosCanal, tv ? "tv," : "channel,",
            sizeof pedidosCanal - strlen(pedidosCanal) - 1);
    r = tv ? respCanalTv : respCanalChannel;
    return r ? strdup(r) : NULL;
  }
  // Anime (armids.h): o ARM e um addon que declara "tt" e "kitsu" no /stream.
  if (strstr(url, "arm.haglund.dev")) {
    pthread_mutex_lock(&pedidosAnimeTrava);
    strncat(pedidosAnime, strstr(url, "/api/v2/") + 8, sizeof pedidosAnime - strlen(pedidosAnime) - 2);
    strcat(pedidosAnime, ",");
    pthread_mutex_unlock(&pedidosAnimeTrava);
    if (strstr(url, "ids?source=kitsu&id=7442"))
      return strdup("{\"kitsu\":7442,\"myanimelist\":16498,\"imdb\":\"tt2560140\","
                    "\"themoviedb-season\":1,\"thetvdb-season\":1,\"media\":\"TV\"}");
    if (strstr(url, "ids?source=kitsu&id=13569"))   // T3 parte I: a T3 tem duas entradas
      return strdup("{\"kitsu\":13569,\"imdb\":\"tt2560140\",\"themoviedb-season\":3,"
                    "\"thetvdb-season\":3,\"media\":\"TV\"}");
    if (strstr(url, "imdb?id=tt2560140"))
      return strdup("[{\"kitsu\":7442,\"myanimelist\":16498,\"themoviedb-season\":1,\"thetvdb-season\":1,\"media\":\"TV\"},"
                    "{\"kitsu\":13569,\"themoviedb-season\":3,\"thetvdb-season\":3,\"media\":\"TV\"},"
                    "{\"kitsu\":41982,\"themoviedb-season\":3,\"thetvdb-season\":3,\"media\":\"TV\"}]");
    return strdup("[]");
  }
  if (strstr(url, "aio.test")) {
    // URL de playback diferente por pedido, o mesmo arquivo: entra uma vez.
    const char *p = strstr(url, "/stream/");
    if (!p) return NULL;
    pthread_mutex_lock(&pedidosAnimeTrava);
    strncat(pedidosAnime, p + 8, sizeof pedidosAnime - strlen(pedidosAnime) - 2);
    strcat(pedidosAnime, ",");
    pthread_mutex_unlock(&pedidosAnimeTrava);
    if (strstr(p, "kitsu:7442:5"))
      return strdup("{\"streams\":[{\"url\":\"https://aio/play/k1\",\"behaviorHints\":{\"filename\":\"[SubsPlease] AoT - 05.mkv\",\"videoSize\":1000}},"
                    "{\"url\":\"https://aio/play/k2\",\"behaviorHints\":{\"filename\":\"AoT.S01E05.mkv\",\"videoSize\":2000}}]}");
    if (strstr(p, "tt2560140:1:5"))
      return strdup("{\"streams\":[{\"url\":\"https://aio/play/t1\",\"behaviorHints\":{\"filename\":\"AoT.S01E05.mkv\",\"videoSize\":2000}},"
                    "{\"url\":\"https://aio/play/t2\",\"behaviorHints\":{\"filename\":\"AoT.S01E05.1080p.mkv\",\"videoSize\":3000}}]}");
    return strdup("{\"streams\":[]}");
  }
  if (strstr(url, "anime.test")) {
    const char *p = strstr(url, "/stream/");
    if (!p) return NULL;
    pthread_mutex_lock(&pedidosAnimeTrava);
    strncat(pedidosAnime, p + 8, sizeof pedidosAnime - strlen(pedidosAnime) - 2);
    strcat(pedidosAnime, ",");
    pthread_mutex_unlock(&pedidosAnimeTrava);
    if (strstr(p, "kitsu%3A7442%3A5") || strstr(p, "kitsu:7442:5"))
      return strdup("{\"streams\":[{\"url\":\"https://x/abs-05.mkv\"},{\"url\":\"https://x/comum.mkv\"}]}");
    if (strstr(p, "tt2560140%3A1%3A5") || strstr(p, "tt2560140:1:5"))
      return strdup("{\"streams\":[{\"url\":\"https://x/comum.mkv\"},{\"url\":\"https://x/s01e05.mkv\"}]}");
    return strdup("{\"streams\":[]}");
  }
  r = strstr(url, "exemplo.test") ? resp[0]
    : strstr(url, "antigo.test")  ? resp[1] : NULL;
  return r ? strdup(r) : NULL;
}
// um Stream por "url", com a URL (a soma de listas do mesmo addon deduplica por ela)
int stream_extrair(const char *json, const char *prov, Stream **saida) {
  int n = 0, i = 0; const char *p = json;
  (void)prov;
  while ((p = strstr(p, "\"url\"")) != NULL) { n++; p += 5; }
  *saida = n ? calloc((size_t)n, sizeof(Stream)) : NULL;
  for (p = json; n && (p = strstr(p, "\"url\":\"")) != NULL && i < n; i++) {
    const char *f;
    p += 7;
    f = strchr(p, '"');
    if (!f) break;
    snprintf((*saida)[i].url, sizeof (*saida)[i].url, "%.*s", (int)(f - p), p);
    // behaviorHints.filename/videoSize, ate a proxima fonte (a deduplicacao do
    // AIOStreams, cuja URL de playback muda com o id do pedido)
    { const char *prox = strstr(f, "\"url\""), *fn = strstr(f, "\"filename\":\""), *vs = strstr(f, "\"videoSize\":");
      if (fn && (!prox || fn < prox)) {
        const char *ff = strchr(fn + 12, '"');
        if (ff) snprintf((*saida)[i].arquivo, sizeof (*saida)[i].arquivo, "%.*s", (int)(ff - fn - 12), fn + 12);
      }
      if (vs && (!prox || vs < prox)) (*saida)[i].tamanhoBytes = strtoull(vs + 12, NULL, 10); }
  }
  return n;
}
// F11: Jellyfin targets are routed away from addons; not exercised here.
#include "jellyfin.h"
int servidores_fontes_pedir(const char *alvo) { (void)alvo; return 0; }
int servidores_fontes_colher(const char *alvo, Stream **l, int *n) {
  (void)alvo; if (l) *l = NULL; if (n) *n = 0; return JF_FONTES_FALHOU; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
static int liveRequest, filterVodRequest;
void stream_folha_canal(int sim) { liveRequest = sim; }
void stream_filtro_tipo(const char *tipo) {
  filterVodRequest = tipo && (!strcasecmp(tipo, "movie") || !strcasecmp(tipo, "series"));
}
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
Uint32 SDL_GetTicks(void) { return 1000 + sportTicks; }
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
// A integracao da poda com addons.c e exercitada em watchstate_poda.sh.
void wsenvio_podar(void) {}
void wsenvio_podar_addon(const char *base) { (void)base; }
void debrid_definir_episodio(int t, int e) { (void)t; (void)e; }
void debrid_nova_busca(void) { nDebridNovaBusca++; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  snprintf(dst, tam, "%s", url ? url : ""); return dst; }
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

// Uma busca inteira, esperando o fio acabar, e a frase da folha.
static const char *buscarMotivo(const char *id) {
  static char m[200];
  addons_buscar(id, "movie");
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  if (!addons_motivo_vazio(m, sizeof m)) snprintf(m, sizeof m, "(sem causa)");
  return m;
}

static int falhas;

static void conferir(const char *o_que, int obtido, int esperado) {
  if (obtido == esperado) return;
  printf("FALHOU %s: obtido %d, esperado %d\n", o_que, obtido, esperado);
  falhas++;
}

static void conferirTexto(const char *o_que, const char *obtido, const char *esperado) {
  if (!strcmp(obtido, esperado)) return;
  printf("FALHOU %s: obtido \"%s\", esperado \"%s\"\n", o_que, obtido, esperado);
  falhas++;
}

static int sempreCancelado(void *u) { (void)u; return 1; }

static int pluginsEnabled, pluginSources;
static PlugResumo pluginSummary;
void plugins_resumo(const char *id, const char *tipo, PlugResumo *saida) {
  (void)id; (void)tipo;
  *saida = pluginsEnabled ? pluginSummary : (PlugResumo){0};
}
static int pluginsActive(void) { return pluginsEnabled; }
static int pluginProvider(const char *id, const char *tipo, int (*cancelado)(void *),
                          void *ctx, OrigemAviso aviso, void *avisoU, void *saida) {
  (void)id; (void)tipo; (void)cancelado; (void)ctx; (void)aviso; (void)avisoU;
  Stream **streams = saida;
  *streams = pluginSources ? calloc(1, sizeof **streams) : NULL;
  return pluginSources;
}

int main(void) {
  const char *liveTypes[] = {"tv", "channel", "channels", "live", "iptv", "LiVe", "sport", "sports"};
  for (unsigned i = 0; i < sizeof liveTypes / sizeof *liveTypes; i++) {
    addons_buscar("fixture:live", liveTypes[i]);
    conferir("live context before player/sheet opens", liveRequest, 1);
    conferir("live request outside VOD filters", filterVodRequest, 0);
    addons_buscar("tt123", "movie");
    conferir("VOD request restores filtering", liveRequest, 0);
    conferir("movie request uses VOD filters", filterVodRequest, 1);
  }
  const char *customTypes[] = {"custom-live-resource", "events", "hockey", "24-hour-channel"};
  for (unsigned i = 0; i < sizeof customTypes / sizeof *customTypes; i++) {
    addons_buscar("fixture:custom", customTypes[i]);
    conferir("arbitrary live resource outside VOD filters", filterVodRequest, 0);
  }
  addons_buscar("tt123:1:1", "series");
  conferir("episode request restores VOD filters", filterVodRequest, 1);
  nDebridNovaBusca = 0;
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-addonslista-XXXXXX");
  char caminho[600];
  FILE *f;
  int n;

  if (!mkdtemp(dir)) { printf("FALHOU: sem diretorio temporario\n"); return 1; }
  snprintf(caminho, sizeof caminho, "%s/addons.txt", dir);
  f = fopen(caminho, "w");
  if (!f) { printf("FALHOU: sem %s\n", caminho); return 1; }
  // Uma linha por forma que o arquivo aceita: com as tres colunas de
  // capacidade, e sem elas (formato antigo, em que tudo vale 1 menos legenda).
  fprintf(f, "# comentario ignorado\n");
  fprintf(f, "So legenda\thttps://opensubtitles-v3.strem.io\t0\t0\t1\n");
  fprintf(f, "Fonte e catalogo\thttps://exemplo.test/manifest.json\t1\t1\t0\n");
  fprintf(f, "Formato antigo\thttps://antigo.test\n");
  fclose(f);

  n = addons_carregar(dir);
  conferir("addons lidos", n, 3);

  // O QUE ESTE TESTE GUARDA: os tres nascem LIGADOS.
  conferir("ativo[0]", addons_ativo(0), 1);
  conferir("ativo[1]", addons_ativo(1), 1);
  conferir("ativo[2]", addons_ativo(2), 1);

  // E as capacidades continuam vindo das colunas, como antes.
  conferir("fonte[0]",   addons_fornece(0, ADD_STREAM),   0);
  conferir("legenda[0]", addons_fornece(0, ADD_LEGENDA), 1);
  conferir("fonte[1]",   addons_fornece(1, ADD_STREAM),   1);
  conferir("catalogo[1] (ativo e catalogo)", addons_tem_catalogo(1), 1);
  // Formato antigo: sem colunas vale tudo 1, como na lista da conta (#552) —
  // addon de legenda so com a URL no arquivo nao pode esperar a sonda.
  conferir("fonte[2]",   addons_fornece(2, ADD_STREAM),   1);
  conferir("legenda[2]", addons_fornece(2, ADD_LEGENDA), 1);

  // A base sai sem "/manifest.json" — a mesma regra de baseNormalizada.
  conferirTexto("base[1]", addons_base(1), "https://exemplo.test");
  conferirTexto("nome[1]", addons_nome(1), "Fonte e catalogo");

  // ---- a causa da folha vazia
  // 1) id 1504 (#107): todos responderam {"streams":[]}
  resp[0] = "{\"streams\":[]}"; resp[1] = "{\"streams\":[]}";
  conferirTexto("todos vazios", buscarMotivo("tt0000001"),
                "2 add-ons responderam: nenhum tem este título");
  conferir("debrid_nova_busca por busca", nDebridNovaBusca, 1);
  // 2) um vazio, um mudo
  resp[1] = NULL;
  conferirTexto("vazio + mudo", buscarMotivo("tt0000002"),
                "1 sem este título · 1 sem resposta");
  // 3) algum trouxe fonte: a lista nao esta vazia por culpa dos addons
  resp[0] = "{\"streams\":[{\"url\":\"https://x/a.mp4\"}]}";
  conferir("com fonte nao explica vazio",
           strcmp(buscarMotivo("tt0000003"), "(sem causa)"), 0);
  // 4) so um consultado, e ele nao responde
  addons_alternar(1);                       // "Fonte e catalogo" desligado
  conferirTexto("um mudo", buscarMotivo("tt0000004"), "Formato antigo não respondeu");
  // Malformed resource responses are failures, not empty stream arrays.
  resp[1] = "{\"err\":\"Invalid debrid key\"}";
  conferirTexto("invalid resource", buscarMotivo("tt0000005"),
                "Formato antigo não respondeu");
  // 6) D5: os de fonte desligados
  addons_alternar(2);
  conferirTexto("desligados", buscarMotivo("tt0000006"),
                "Os add-ons de fontes estão desligados");

  // ---- canal ao vivo: o segundo nome de tipo (issue #112)
  // 7) FrostView sem manifesto lido: "tv" responde 200 vazio. Antes o segundo
  //    nome so saia quando o primeiro NAO respondia, e o canal ficava sem
  //    fonte com o addon tendo quatro. Agora a lista vazia tambem dispara.
  conferir("addon de canal entrou", addons_adicionar("Canal TV", "https://canal.test/manifest.json"), 1);
  respCanalTv = "{\"streams\":[]}";
  respCanalChannel = "{\"streams\":[{\"url\":\"https://x/axn.m3u8\"}]}";
  pedidosCanal[0] = 0;
  addons_definir_origem("https://canal.test");
  addons_buscar("cs:channel:axn", "tv");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("tv vazio -> channel traz a fonte", addons_estado(), ADD_PRONTO);
  conferirTexto("ordem sem manifesto", pedidosCanal, "tv,channel,");
  // 8) The requested type stays first; the manifest type follows it.
  addons_manifesto_lido(3,
    "{\"id\":\"com.frostview\",\"name\":\"Canal TV\","
    "\"resources\":[\"catalog\",\"meta\",\"stream\"],\"types\":[\"channel\"],"
    "\"catalogs\":[{\"id\":\"froststream-channels\",\"type\":\"channel\",\"name\":\"Canais\"}]}");
  pedidosCanal[0] = 0;
  addons_definir_origem("https://canal.test");
  addons_buscar("cs:channel:globo", "tv");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("channel declarado traz a fonte", addons_estado(), ADD_PRONTO);
  conferirTexto("ordem com manifesto", pedidosCanal, "tv,channel,");
  // 9) Os dois nomes vazios: a causa fala de CANAL, e nao de titulo.
  respCanalChannel = "{\"streams\":[]}";
  addons_definir_origem("https://canal.test");
  { static char m[200];
    addons_buscar("cs:channel:bleach", "tv");
    addons_definir_origem(NULL);
    while (addons_estado() == ADD_BUSCANDO) usleep(1000);
    if (!addons_motivo_vazio(m, sizeof m)) snprintf(m, sizeof m, "(sem causa)");
    conferirTexto("canal vazio", m, "Canal TV não tem fonte para este canal agora"); }
  // 10) Segundo nome sem resposta nao apaga o "respondeu vazio" do primeiro.
  respCanalChannel = NULL;
  respCanalTv = "{\"streams\":[]}";
  addons_definir_origem("https://canal.test");
  { static char m[200];
    addons_buscar("cs:channel:sbt", "tv");
    addons_definir_origem(NULL);
    while (addons_estado() == ADD_BUSCANDO) usleep(1000);
    if (!addons_motivo_vazio(m, sizeof m)) snprintf(m, sizeof m, "(sem causa)");
    conferirTexto("channel mudo, tv vazio", m, "Canal TV não tem fonte para este canal agora"); }

  // ---- manifesto MISTO (o do relato: movie+series+canal num addon so, estilo
  // M3U Mux/custom IPTV). O catalogo declara "live" e o /stream/ so responde
  // por esse nome — "tv"/"channel" voltam 404. Antes o app pedia so tv e
  // channel e o canal nunca abria.
  conferir("addon misto entrou", addons_adicionar("IPTV Caseiro", "https://live.test/manifest.json"), 1);
  addons_manifesto_lido(addons_n() - 1,
    "{\"id\":\"org.livecaseiro\",\"name\":\"IPTV Caseiro\","
    "\"resources\":[\"stream\",\"meta\",\"catalog\"],\"types\":[\"movie\",\"series\",\"live\"],"
    "\"catalogs\":[{\"id\":\"m3u-movies\",\"type\":\"movie\",\"name\":\"Filmes\"},"
    "{\"id\":\"m3u-series\",\"type\":\"series\",\"name\":\"Series\"},"
    "{\"id\":\"m3u-live\",\"type\":\"live\",\"name\":\"Canais\"}]}");
  respCanalLive = "{\"streams\":[{\"url\":\"https://x/c1.m3u8\"}]}";
  pedidosLive[0] = 0;
  // O pedido chega pelo tipo declarado (guia_canal_tipo, app.c tocarCanal):
  // "live" primeiro e sozinho.
  addons_definir_origem("https://live.test");
  addons_buscar("m3u-live-1", "live");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("canal live declarado traz a fonte", addons_estado(), ADD_PRONTO);
  conferirTexto("ordem com tipo live", pedidosLive, "live,");
  // E pedido por nome generico ("tv", como uma renovacao) ainda acha: o
  // declarado vira a segunda tentativa.
  pedidosLive[0] = 0;
  addons_definir_origem("https://live.test");
  addons_buscar("m3u-live-1", "tv");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("tv sem fonte -> declarado live traz", addons_estado(), ADD_PRONTO);
  conferirTexto("ordem tv->live", pedidosLive, "tv,live,");

  // #588: fake manifest and transport; no network, real stream lookup worker.
  conferir("sport addon added", addons_adicionar("Live Sport", "https://sport.test/manifest.json"), 1);
  addons_manifesto_lido(addons_n() - 1,
    "{\"id\":\"org.sport\",\"resources\":[\"stream\",\"catalog\"],"
    "\"catalogs\":[{\"id\":\"events\",\"type\":\"sport\"}]}");
  {
    const char *requested[] = { "tv", "sport", "event", "event", "movie" };
    const char *served[] = { "sport", "sport", "event", "iptv", "sport" };
    const char *expected[] = { "tv,sport,", "sport,", "event,",
                              "event,sport,channel,tv,channels,live,iptv,", "movie,movie," };
    for (int k = 0; k < 5; k++) {
      Stream *sources = NULL;
      tipoSport = served[k]; sportVazio = k == 3; pedidosSport[0] = 0;
      int count = addons_consultar("sport:event-1", requested[k], "https://sport.test", 1,
                                  NULL, NULL, &sources);
      conferir("sport source received", count, k == 4 ? 0 : 1);
      conferir("sport source allocated", sources != NULL, k != 4);
      conferirTexto("exact item, catalog, then legacy types", pedidosSport, expected[k]);
      free(sources);
    }
  }

  // Slow live fallbacks share one deadline per addon and retry round.
  // Virtual transport time keeps timeout regression coverage deterministic.
  {
    Stream *sources = NULL;
    tipoSport = "unavailable";
    sportVazio = 1; sportDelayMs = 4500; sportRequests = 0; pedidosSport[0] = 0;
    conferir("slow custom fallback has no sources",
      addons_consultar("sport:deadline", "event", "https://sport.test", 1, NULL, NULL, &sources), 0);
    conferirTexto("deadline preserves item/catalog order", pedidosSport, "event,sport,channel,");
    conferir("fallbacks stop at shared deadline", sportRequests, 3);
    conferir("initial live timeout", sportTimeouts[0], 12);
    conferir("catalog gets remaining whole seconds", sportTimeouts[1], 7);
    conferir("legacy gets remaining whole seconds", sportTimeouts[2], 3);
    free(sources);

    sportDelayMs = 12000; sportRequests = 0; pedidosSport[0] = 0;
    conferir("timed-out tv lookup has no sources",
      addons_consultar("sport:deadline-tv", "tv", "https://sport.test", 1, NULL, NULL, &sources), 0);
    conferirTexto("no fallback after deadline", pedidosSport, "tv,");
    conferir("tv timeout is not multiplied", sportRequests, 1);
    free(sources);

    sportDelayMs = 11500; sportRequests = 0; pedidosSport[0] = 0;
    conferir("subsecond budget has no sources",
      addons_consultar("sport:deadline-fraction", "event", "https://sport.test", 1, NULL, NULL, &sources), 0);
    conferir("subsecond remainder cannot start a one-second request", sportRequests, 1);
    free(sources);

    sportVazio = 0; sportDelayMs = 30000; sportRequests = 0; pedidosSport[0] = 0;
    conferir("unresponsive custom lookup has no sources",
      addons_consultar("sport:deadline-retry", "event", "https://sport.test", 1, NULL, NULL, &sources), 0);
    conferirTexto("each retry round starts with item type", pedidosSport, "event,event,");
    conferir("one timeout per retry round", sportRequests, 2);
    conferir("first round keeps 12-second budget", sportTimeouts[0], 12);
    conferir("second round keeps independent 20-second budget", sportTimeouts[1], 20);
    free(sources);
    sportDelayMs = 0;
  }

  // ---- #182: addon lento aparece sozinho, sem recarregar a mao
  conferir("addon lento entrou", addons_adicionar("Lento", "https://lento.test/manifest.json"), 1);
  // 11) nao respondeu na 1a tentativa, respondeu na 2a: a lista ja o traz.
  // R2: uma busca real falha, seguida de prefetch cancelado antes do HTTP.
  falhasLento = 1000; chamadasLento = 0;
  addons_definir_origem("https://lento.test");
  buscarMotivo("tt-r2-primeira");
  conferir("R2 primeira busca tem duas tentativas", chamadasLento, 2);
  { Stream *l = NULL;
    conferir("R2 prefetch cancelado", addons_consultar("tt-r2:1:2", "series", "https://lento.test", 1,
             sempreCancelado, NULL, &l), -1);
    conferir("R2 cancelamento nao faz HTTP", chamadasLento, 2);
    free(l);
  }
  falhasLento = 1; chamadasLento = 0;
  addons_definir_origem("https://lento.test");
  buscarMotivo("tt-r2-seguinte");
  conferir("R2 busca real conserva segunda chance", chamadasLento, 2);
  conferir("R2 segunda chance recupera fontes", addons_estado(), ADD_PRONTO);

  falhasLento = 1; chamadasLento = 0;
  addons_definir_origem("https://lento.test");
  addons_buscar("tt0000011", "movie");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("lento respondeu na segunda tentativa", addons_estado(), ADD_PRONTO);
  conferir("duas requisicoes", chamadasLento, 2);
  // #202: filme/serie espera como o Nuvio web (60 s por addon): 30 s na 1a
  // rodada e 30 s na 2a, e nao 12 + 20. Canal ao vivo continua com 12 s.
  conferir("prazo da 1a rodada (filme)", prazoLento[0], 30);
  conferir("prazo da 2a rodada (filme)", prazoLento[1], 30);
  conferir("prazo de canal ao vivo", prazoCanal, 12);
  // 12) fora do ar de verdade: duas consultas gastam a 2a tentativa, a terceira
  //     nao (um addon morto nao pode dobrar o prazo de toda abertura).
  falhasLento = 1000; chamadasLento = 0;
  { int k, esperado[3] = { 2, 2, 1 };
    for (k = 0; k < 3; k++) {
      int antes = chamadasLento;
      addons_definir_origem("https://lento.test");
      addons_buscar("tt0000012", "movie");
      addons_definir_origem(NULL);
      while (addons_estado() == ADD_BUSCANDO) usleep(1000);
      conferir("addon morto: requisicoes da consulta", chamadasLento - antes, esperado[k]);
    } }

  // ---- add-on so de legenda vindo da conta (TCL, 09/10): ele nasce com
  // fonte=1 ate o manifesto ser lido; a busca de fontes nao pode pedir
  // /stream/ a ele nem chama-lo de "sem resposta".
  conferir("so legenda entrou", addons_adicionar("OpenSubtitles v3", "https://legenda.test/manifest.json"), 1);
  idxLegenda = addons_n() - 1;
  // Instalado pelo guia supoe legenda como a lista da conta (#552): sem isto o
  // addon so de legenda ficava fora da busca ate a sonda, e para sempre se
  // ela falhasse.
  conferir("instalado pelo guia supoe legenda", addons_fornece(idxLegenda, ADD_LEGENDA), 1);
  pedidosLegendaStream = 0;
  addons_definir_origem("https://legenda.test");
  addons_buscar("tt0000013", "movie");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  printf("legenda.test: %d pedido(s) de manifesto, %d de stream\n", pedidosLegendaManifesto, pedidosLegendaStream);
  // O 1o pedido saiu antes de a sonda terminar (inevitavel); depois dela, nada
  // de segunda chance, e a folha nao culpa o add-on de legenda.
  conferir("so de legenda: um pedido de stream, sem segunda chance", pedidosLegendaStream, 1);
  { char m[200];
    if (!addons_motivo_vazio(m, sizeof m)) m[0] = 0;
    printf("motivo: %s\n", m);
    conferir("so de legenda nao vira \"sem resposta\"", strstr(m, "OpenSubtitles") != NULL, 0); }
  // Busca seguinte, com o manifesto lido: nenhum pedido de stream.
  pedidosLegendaStream = 0;
  addons_definir_origem("https://legenda.test");
  addons_buscar("tt0000014", "movie");
  addons_definir_origem(NULL);
  while (addons_estado() == ADD_BUSCANDO) usleep(1000);
  conferir("so de legenda com manifesto lido: nenhum pedido de stream", pedidosLegendaStream, 0);

  // RESOURCE "meta" (#174/#175), num addon proprio no fim para nao mexer nos
  // nomes e capacidades que as verificacoes acima leem. Duas formas do
  // protocolo (objeto e string); quem nao declara fica sem.
  { int a = addons_n(), b, c;
    addons_adicionar("Meta A", "https://meta-a.test/manifest.json");
    addons_adicionar("Meta B", "https://meta-b.test/manifest.json");
    addons_adicionar("Meta C", "https://meta-c.test/manifest.json");
    b = a + 1; c = a + 2;
    addons_manifesto_lido(a, "{\"id\":\"m.a\",\"name\":\"Meta A\",\"resources\":"
                             "[\"catalog\",{\"name\":\"meta\",\"types\":[\"series\"]}]}");
    addons_manifesto_lido(b, "{\"id\":\"m.b\",\"name\":\"Meta B\",\"resources\":[\"meta\","
                             "{\"name\":\"subtitles\",\"types\":[\"movie\",\"series\"]}]}");
    addons_manifesto_lido(c, "{\"id\":\"m.c\",\"name\":\"Meta C\",\"resources\":[\"stream\"]}");
    conferir("meta (objeto)", addons_fornece(a, ADD_META), 1);
    conferir("meta (string)", addons_fornece(b, ADD_META), 1);
    conferir("legenda (objeto)", addons_fornece(b, ADD_LEGENDA), 1);
    conferir("meta (nao declara)", addons_fornece(c, ADD_META), 0);
    conferir("legenda segue lida do manifesto", addons_fornece(c, ADD_LEGENDA), 0); }

  // idPrefixes/types DO RESOURCE "meta" (addons_aceita_id). A raiz do manifesto
  // e o resource declaram; o do resource vence; o "idPrefixes" de OUTRO resource
  // (stream) ou de um catalogo nao pode vazar para o meta.
  { int a = addons_n(), b, c, d, e, f;
    addons_adicionar("Pref A", "https://pref-a.test/manifest.json");
    addons_adicionar("Pref B", "https://pref-b.test/manifest.json");
    addons_adicionar("Pref C", "https://pref-c.test/manifest.json");
    addons_adicionar("Pref D", "https://pref-d.test/manifest.json");
    addons_adicionar("Pref E", "https://pref-e.test/manifest.json");
    addons_adicionar("Pref F", "https://pref-f.test/manifest.json");
    b = a + 1; c = a + 2; d = a + 3; e = a + 4; f = a + 5;
    // A: no resource, com tipos ("anime" e o do Kitsu)
    addons_manifesto_lido(a, "{\"id\":\"p.a\",\"name\":\"Pref A\",\"types\":[\"movie\",\"series\"],"
                             "\"resources\":[\"catalog\",{\"name\":\"stream\",\"types\":[\"series\"],\"idPrefixes\":[\"tt\"]},"
                             "{\"name\":\"meta\",\"types\":[\"Anime\",\"series\"],\"idPrefixes\":[\"kitsu:\",\"mal:\"]}],"
                             "\"catalogs\":[{\"type\":\"anime\",\"id\":\"k\",\"idPrefixes\":[\"zzz:\"]}]}");
    conferir("prefixo do resource casa", addons_aceita_id(a, "series", "kitsu:41370"), 1);
    conferir("segundo prefixo casa", addons_aceita_id(a, "anime", "mal:456"), 1);
    conferir("tipo declarado sem caixa", addons_aceita_id(a, "ANIME", "kitsu:1"), 1);
    conferir("prefixo do stream nao vaza para o meta", addons_aceita_id(a, "series", "tt0111161"), 0);
    conferir("prefixo do catalogo nao vaza", addons_aceita_id(a, "series", "zzz:1"), 0);
    conferir("tipo fora dos declarados", addons_aceita_id(a, "movie", "kitsu:41370"), 0);
    // B: so na raiz
    addons_manifesto_lido(b, "{\"id\":\"p.b\",\"name\":\"Pref B\",\"idPrefixes\":[\"tt\"],"
                             "\"types\":[\"movie\",\"series\"],\"resources\":[\"catalog\",\"meta\",\"stream\"]}");
    conferir("raiz: tt casa", addons_aceita_id(b, "movie", "tt0111161"), 1);
    conferir("raiz: kitsu nao", addons_aceita_id(b, "series", "kitsu:1"), 0);
    // C: o resource declara e vence o da raiz
    addons_manifesto_lido(c, "{\"id\":\"p.c\",\"name\":\"Pref C\",\"idPrefixes\":[\"tt\"],"
                             "\"resources\":[{\"name\":\"meta\",\"idPrefixes\":[\"xperience:\"]}]}");
    conferir("resource vence a raiz", addons_aceita_id(c, "series", "xperience:abc"), 1);
    conferir("a raiz nao vale onde o resource declarou", addons_aceita_id(c, "series", "tt1"), 0);
    conferir("sem tipos declarados, qualquer tipo", addons_aceita_id(c, "anime", "xperience:abc"), 1);
    // D: meta sem nenhum prefixo -> nao da para saber
    addons_manifesto_lido(d, "{\"id\":\"p.d\",\"name\":\"Pref D\",\"resources\":[\"meta\"]}");
    conferir("sem idPrefixes: desconhecido", addons_aceita_id(d, "series", "kitsu:1"), -1);
    // E: nao tem meta
    addons_manifesto_lido(e, "{\"id\":\"p.e\",\"name\":\"Pref E\",\"idPrefixes\":[\"kitsu:\"],\"resources\":[\"stream\"]}");
    conferir("sem o resource meta: nao serve", addons_aceita_id(e, "series", "kitsu:1"), 0);
    // F: manifesto nunca lido -> desconhecido; indice ruim -> 0
    conferir("nao sondado: desconhecido", addons_aceita_id(f, "series", "kitsu:1"), -1);
    conferir("indice invalido", addons_aceita_id(999, "series", "kitsu:1"), 0);
    conferir("id vazio", addons_aceita_id(a, "series", ""), 0); }

  // Desligado na conta = NUNCA consultado (2.0.3, D1 58666): nem fonte, nem
  // legenda, nem catalogo/busca/guia (addons_base_desligada guarda esses).
  { AddonRemoto conta[2];
    memset(conta, 0, sizeof conta);
    snprintf(conta[0].nome, sizeof conta[0].nome, "Ligado");
    snprintf(conta[0].url, sizeof conta[0].url, "https://ligado.test/manifest.json");
    conta[0].ativo = 1;
    snprintf(conta[1].nome, sizeof conta[1].nome, "Desligado");
    snprintf(conta[1].url, sizeof conta[1].url, "https://desligado.test/manifest.json");
    conta[1].ativo = 0;
    conferir("lista da conta entra", addons_definir_lista(conta, 2), 1);
    conferir("base desligada reconhecida", addons_base_desligada("https://desligado.test"), 1);
    conferir("base desligada com manifest.json", addons_base_desligada("https://desligado.test/manifest.json"), 1);
    conferir("base ligada nao e desligada", addons_base_desligada("https://ligado.test"), 0);
    conferir("base desconhecida nao e desligada", addons_base_desligada("https://nenhum.test"), 0);
    pedidosDesligado = pedidosLigado = 0;
    addons_buscar("tt0000077", "movie");
    while (addons_estado() == ADD_BUSCANDO) usleep(1000);
    conferir("fonte: ligado consultado", pedidosLigado > 0, 1);
    conferir("fonte: desligado NAO consultado", pedidosDesligado, 0);
    pedidosLigado = 0;
    addons_buscar("cs:channel:zap", "tv");
    while (addons_estado() == ADD_BUSCANDO) usleep(1000);
    conferir("canal: desligado NAO consultado", pedidosDesligado, 0);
  }

  // Teto da conta: URLs distintas, incluindo um desligado depois do teto antigo.
  { static AddonRemoto conta[130];
    int i;
    for (i = 0; i < 130; i++) {
      snprintf(conta[i].nome, sizeof conta[i].nome, "Conta %d", i);
      snprintf(conta[i].url, sizeof conta[i].url, "https://conta%d.test/manifest.json", i);
      conta[i].ativo = i != 99;
    }
    conferir("100 da conta aplicados", addons_definir_lista(conta, 100), 1);
    conferir("100 da conta aceitos", addons_n(), 100);
    conferirTexto("ultimo dos 100 presente", addons_nome(99), "Conta 99");
    conferir("desligado entre os 100 continua desligado", addons_ativo(99), 0);
    conferir("130 da conta aplicados", addons_definir_lista(conta, 130), 1);
    conferir("130 da conta limitados a 128", addons_n(), 128);
    conferirTexto("ultimo dos 128 presente", addons_nome(127), "Conta 127");
  }

  // Local JS scrapers are source providers even without Stremio add-ons.
  f = fopen(caminho, "w");
  if (!f) return 1;
  fclose(f);
  addons_carregar(dir);
  conferir("zero Stremio add-ons", addons_n(), 0);
  addons_definir_origem_extra(pluginProvider, pluginsActive);
  conferirTexto("no providers", buscarMotivo("tt2050000"), "Nenhum add-on de fontes instalado");
  pluginsEnabled = 1;
  conferirTexto("plugins returned nothing", buscarMotivo("tt2050001"), "(sem causa)");
  pluginSummary = (PlugResumo){ .enfileirados = 3, .terminados = 2 };
  conferirTexto("plugin counts precede generic fallback", buscarMotivo("tt2050001"),
                "Plugins não encontraram fontes (2 de 3 scrapers verificados)");
  pluginSummary = (PlugResumo){0};
  pluginSources = 1;
  conferirTexto("plugin sources failed to open", buscarMotivo("tt2050002"), "(sem causa)");
  conferir("plugin list has sources", addons_estado(), ADD_PRONTO);
  addons_adicionar("Disabled source", "https://desligado.test/manifest.json");
  addons_alternar(0);
  pluginSources = 0;
  conferirTexto("plugins with disabled Stremio sources", buscarMotivo("tt2050003"), "(sem causa)");
  pluginsEnabled = 0;
  conferirTexto("all providers disabled", buscarMotivo("tt2050004"), "Os add-ons de fontes estão desligados");

  // ---- ANIME: o mesmo episodio pelos dois ids que o /stream declara (armids.h).
  conferir("addon de anime entrou", addons_adicionar("Anime TT Kitsu", "https://anime.test/manifest.json"), 1);
  addons_manifesto_lido(addons_n() - 1,
    "{\"id\":\"org.anime.tk\",\"name\":\"Anime TT Kitsu\",\"types\":[\"movie\",\"series\"],"
    "\"resources\":[\"stream\"],\"idPrefixes\":[\"tt\",\"kitsu\"]}");
  { Stream *l = NULL; int n, k, abs = 0, s01 = 0, comum = 0;
    // kitsu:7442:5 (AoT T1, uma entrada so na T1) -> tambem tt2560140:1:5; a
    // fonte que vem pelos dois entra uma vez.
    pedidosAnime[0] = 0;
    n = addons_consultar("kitsu:7442:5", "series", "https://anime.test", 1, NULL, NULL, &l);
    for (k = 0; k < n; k++) {
      abs += !strcmp(l[k].url, "https://x/abs-05.mkv");
      s01 += !strcmp(l[k].url, "https://x/s01e05.mkv");
      comum += !strcmp(l[k].url, "https://x/comum.mkv");
    }
    conferir("kitsu + tt: tres fontes distintas", n, 3);
    conferir("kitsu + tt: a fonte comum uma vez", comum, 1);
    conferir("kitsu + tt: as de cada id", abs + s01, 2);
    conferir("kitsu + tt: perguntou o tt", strstr(pedidosAnime, "tt2560140") != NULL, 1);
    free(l); l = NULL;
    // O caminho de volta: tt2560140:1:5 -> kitsu:7442:5.
    pedidosAnime[0] = 0;
    n = addons_consultar("tt2560140:1:5", "series", "https://anime.test", 1, NULL, NULL, &l);
    conferir("tt + kitsu: tres fontes distintas", n, 3);
    conferir("tt + kitsu: perguntou o kitsu", strstr(pedidosAnime, "kitsu%3A7442%3A5") != NULL ||
                                               strstr(pedidosAnime, "kitsu:7442:5") != NULL, 1);
    free(l); l = NULL;
    // T3 em duas entradas (parte I e II): o desfase do episodio nao se sabe,
    // entao nem kitsu -> tt nem tt -> kitsu.
    pedidosAnime[0] = 0;
    addons_consultar("kitsu:13569:5", "series", "https://anime.test", 1, NULL, NULL, &l);
    conferir("T3 partida: nada de tt", strstr(pedidosAnime, "series/tt2560140") != NULL, 0);
    free(l); l = NULL;
    pedidosAnime[0] = 0;
    addons_consultar("tt2560140:3:5", "series", "https://anime.test", 1, NULL, NULL, &l);
    conferir("T3 partida: nada de kitsu", strstr(pedidosAnime, "series/kitsu") != NULL, 0);
    free(l); l = NULL;
    // Um tt que nao e anime: o ARM responde [] e so o tt vai ao addon.
    pedidosAnime[0] = 0;
    addons_consultar("tt0111161", "movie", "https://anime.test", 1, NULL, NULL, &l);
    conferir("filme comum: so o proprio id", strstr(pedidosAnime, "movie/kitsu") != NULL, 0);
    free(l); }
  // AIOStreams declara os ids no resource "stream" (manifesto real da 2.36.0) e
  // so repassa a cada scraper o id que ele declara: pelos dois, acha mais.
  conferir("AIOStreams entrou", addons_adicionar("AIOStreams", "https://aio.test/manifest.json"), 1);
  addons_manifesto_lido(addons_n() - 1,
    "{\"id\":\"com.aiostreams.viren070.x\",\"name\":\"AIOStreams\",\"resources\":["
    "{\"name\":\"stream\",\"types\":[\"movie\",\"series\",\"anime\"],"
    "\"idPrefixes\":[\"tt\",\"kitsu\",\"anilist\",\"mal\",\"anidb\",\"kitsu:\",\"mal:\",\"tmdb:\"]}],"
    "\"types\":[\"movie\",\"series\",\"anime\"]}");
  { Stream *l = NULL; int n;
    pedidosAnime[0] = 0;
    n = addons_consultar("kitsu:7442:5", "series", "https://aio.test", 1, NULL, NULL, &l);
    conferir("AIOStreams: kitsu + tt, tres arquivos (o repetido uma vez)", n, 3);
    conferir("AIOStreams: queries declared MAL namespace", strstr(pedidosAnime, "series/mal") != NULL, 1);
    free(l); }

  remove(caminho);
  rmdir(dir);
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("addonslista: ok\n");
  return 0;
}
