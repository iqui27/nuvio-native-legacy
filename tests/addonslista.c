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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "addons.h"
#include "fontecache.h"

// Provider discovery is tested in ondever_lookup.c. This isolated addon
// transport fixture must not start an unrelated TMDB request.
void ondever_pedir(const char *id, int series, long tmdb) {
  (void)id; (void)series; (void)tmdb;
}

int dados_gravar_leve(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }

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
// lento.test (#182): as primeiras `falhasLento` requisicoes NAO respondem (o
// timeout do AIOStreams frio); as seguintes respondem com uma fonte.
static int pedidosDesligado, pedidosLigado;
static int falhasLento, chamadasLento, prazoLento[2], prazoCanal;
// legenda.test imita o OpenSubtitles v3 vindo da conta (TCL do dono, 09/10):
// instalado sem capacidades (nasce "fonte"), o manifesto diz so "subtitles" e
// /stream/ responde 404.
static int pedidosLegendaStream, pedidosLegendaManifesto, idxLegenda = -1;
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
  if (strstr(url, "canal.test")) {
    prazoCanal = s;
    int tv = strstr(url, "/stream/tv/") != NULL;
    strncat(pedidosCanal, tv ? "tv," : "channel,",
            sizeof pedidosCanal - strlen(pedidosCanal) - 1);
    r = tv ? respCanalTv : respCanalChannel;
    return r ? strdup(r) : NULL;
  }
  r = strstr(url, "exemplo.test") ? resp[0]
    : strstr(url, "antigo.test")  ? resp[1] : NULL;
  return r ? strdup(r) : NULL;
}
// conta um "url" por fonte; o bastante para distinguir lista vazia de cheia
int stream_extrair(const char *json, const char *prov, Stream **saida) {
  int n = 0; const char *p = json;
  (void)prov;
  while ((p = strstr(p, "\"url\"")) != NULL) { n++; p += 5; }
  *saida = n ? calloc((size_t)n, sizeof(Stream)) : NULL;
  return n;
}
// F11: Jellyfin targets are routed away from addons; not exercised here.
#include "jellyfin.h"
int servidores_fontes_pedir(const char *alvo) { (void)alvo; return 0; }
int servidores_fontes_colher(const char *alvo, Stream **l, int *n) {
  (void)alvo; if (l) *l = NULL; if (n) *n = 0; return JF_FONTES_FALHOU; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
void stream_definir_lista(const Stream *l, int n) { (void)l; (void)n; }
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
void debrid_nova_busca(void) { nDebridNovaBusca++; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  snprintf(dst, tam, "%s", url ? url : ""); return dst; }
void marco(const char *s) { (void)s; }
int  fontecache_pegar(const char *id, const char *tipo, Stream **l, int *n) {
  (void)id; (void)tipo; *l = NULL; *n = 0; return FC_NADA; }
void fontecache_guardar(const char *id, const char *tipo, const Stream *l, int n) {
  (void)id; (void)tipo; (void)l; (void)n; }
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

int main(void) {
  char dir[] = "/tmp/nuvio-addonslista-XXXXXX";
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
  // Formato antigo: fonte e catalogo valem 1, legenda 0.
  conferir("fonte[2]",   addons_fornece(2, ADD_STREAM),   1);
  conferir("legenda[2]", addons_fornece(2, ADD_LEGENDA), 0);

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
  // 5) so um, e responde vazio
  resp[1] = "{\"err\":\"Invalid debrid key\"}";
  conferirTexto("um vazio", buscarMotivo("tt0000005"),
                "Formato antigo respondeu: não tem este título");
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
  // 8) Com o manifesto do FrostView (catalogo "channel"), "channel" vai
  //    primeiro e sozinho: uma viagem por canal, nao duas.
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
  conferirTexto("ordem com manifesto", pedidosCanal, "channel,");
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
    addons_manifesto_lido(b, "{\"id\":\"m.b\",\"name\":\"Meta B\",\"resources\":[\"meta\",\"subtitles\"]}");
    addons_manifesto_lido(c, "{\"id\":\"m.c\",\"name\":\"Meta C\",\"resources\":[\"stream\"]}");
    conferir("meta (objeto)", addons_fornece(a, ADD_META), 1);
    conferir("meta (string)", addons_fornece(b, ADD_META), 1);
    conferir("meta (nao declara)", addons_fornece(c, ADD_META), 0);
    conferir("legenda segue lida do manifesto", addons_fornece(b, ADD_LEGENDA), 1); }

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

  remove(caminho);
  rmdir(dir);
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("addonslista: ok\n");
  return 0;
}
