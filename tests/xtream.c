#include "stub_perfisessao.h"
// Xtream (src/xtream.c): cadastro, lista de canais e URL, sem rede e sem disco.
//
// rede_baixar e dados_* sao DUBLES: a lista vem de um corpo canned no formato
// que os paineis Xtream Codes emitem (category_id ora "5" ora 5, stream_id
// numero), e o cadastro vive num buffer em memoria.
#include "stub_perfisessao.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "xtream.h"
#include "perfisessao.h"

// --- dubles ----------------------------------------------------------------
static char disco[2048]; static int temDisco;
char *dados_ler(const char *nome) { (void)nome; return temDisco ? strdup(disco) : NULL; }
int dados_gravar(const char *nome, const char *c) { (void)nome; snprintf(disco, sizeof disco, "%s", c); temDisco = 1; return 1; }
int dados_apagar(const char *nome) { (void)nome; temDisco = 0; disco[0] = 0; return 1; }
int perfis_ativo(void) { return 1; }
static int prefFormato;
int ajustes_livetv_formato(void) { return prefFormato; }

static const char *respCats =
  "[{\"category_id\":\"5\",\"category_name\":\"Esportes\",\"parent_id\":0},"
  " {\"category_id\":7,\"category_name\":\"Not\\u00edcias\",\"parent_id\":0}]";
static const char *respStreams =
  "[{\"num\":1,\"name\":\"ESPN HD\",\"stream_type\":\"live\",\"stream_id\":101,"
  "  \"stream_icon\":\"http://x/espn.png\",\"epg_channel_id\":\"espn.br\",\"category_id\":\"5\"},"
  " {\"num\":2,\"name\":\"CNN\",\"stream_id\":\"202\",\"stream_icon\":\"\",\"category_id\":7},"
  " {\"num\":3,\"name\":\"\",\"stream_id\":303,\"category_id\":\"5\"},"
  " {\"num\":4,\"name\":\"Sem categoria\",\"stream_id\":404}]";
static const char *respAuth0 = "{\"user_info\":{\"auth\":0,\"status\":\"Disabled\"}}";
static int recusar, mudo, statusHttp = 200, pagina, semFormatos, soTs;
static char ultimaUrl[1200];      // a url DO PAINEL (desfeito o proxy)
static char ultimaPedida[4000];   // o endereco que a rede recebeu de fato
static char ultimoCorpo[1200];    // o corpo do POST ao proxy (Tizen)
static int ultimoPost;
// Build do teste do Tizen (-DNV_XTREAM_PROXY_TESTE): a chamada chega como
// POST <NV_REC_URL>/v1/xtream com a url do painel crua no corpo.
static char *responder(const char *url);
// #158: o chamar() do xtream.c agora usa rede_baixar_st (corpo + status),
// para o log e a tela saberem se foi 401, 403, 458, 429 ou pagina HTML.
char *rede_baixar_st(const char *url, int segundos, const char *const *cab, int *status) {
  (void)segundos; (void)cab;
  ultimoPost = 0;
  snprintf(ultimaPedida, sizeof ultimaPedida, "%s", url);
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  if (status) *status = mudo ? 0 : statusHttp;
  return responder(url);
}
char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cab, int *status, int *ra) {
  if (ra) *ra = 0;
  return rede_baixar_st(url, segundos, cab, status);
}
char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  (void)segundos;
  ultimoPost = 1;
  assert(cab && !strcmp(cab[0], "Content-Type: text/plain"));
  snprintf(ultimaPedida, sizeof ultimaPedida, "%s", url);
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo);
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", corpo);
  if (status) *status = mudo ? 504 : statusHttp;
  if (mudo) return strdup("{\"erro\":\"painel nao respondeu no prazo\"}");
  return responder(corpo);
}
// Resposta real de player_api.php sem action, no formato do Dispatcharr #652
// (quase tudo texto).
static const char *respConta =
  "{\"user_info\":{\"username\":\"u\",\"auth\":1,\"status\":\"Active\","
  "\"exp_date\":\"1786324017\",\"is_trial\":\"0\",\"active_cons\":\"2\","
  "\"created_at\":\"1754615217\",\"max_connections\":\"5\","
  "\"allowed_output_formats\":[\"m3u8\",\"ts\"],\"message\":\"\"},"
  "\"server_info\":{\"url\":\"host\",\"port\":\"8080\",\"https_port\":\"8443\","
  "\"server_protocol\":\"http\",\"timezone\":\"Europe/Bucharest\",\"timestamp_now\":1763250316}}";
static const char *respContaSoTs =
  "{\"user_info\":{\"auth\":1,\"status\":\"Active\",\"exp_date\":null,"
  "\"active_cons\":0,\"max_connections\":1,\"allowed_output_formats\":[\"ts\"]}}";
static const char *respContaSemFormatos =
  "{\"user_info\":{\"auth\":1,\"status\":\"Active\",\"allowed_output_formats\":[\"rtmp\"]}}";
// get_short_epg: titulo em base64 ("Știrile Pro TV" e "Vremea"), horario em
// epoch nos *_timestamp; "start" e hora local do painel e fica de fora.
static const char *respEpg =
  "{\"epg_listings\":[{\"id\":\"1\",\"epg_id\":\"9\",\"title\":\"yJh0aXJpbGUgUHJvIFRW\","
  "\"lang\":\"ro\",\"start\":\"2026-09-29 19:00:00\",\"end\":\"2026-09-29 20:00:00\","
  "\"description\":\"\",\"channel_id\":\"PRO.TV.ro\",\"start_timestamp\":\"1790697600\","
  "\"stop_timestamp\":\"1790701200\"},"
  " {\"title\":\"VnJlbWVh\",\"start_timestamp\":1790701200,\"stop_timestamp\":1790701800},"
  " {\"title\":\"U2VtIGhvcmE=\"}]}";
static char *responder(const char *url) {
  (void)url;
  if (mudo) return NULL;
  if (pagina) return strdup("<!DOCTYPE html><html><title>Just a moment...</title>[x]</html>");
  if (recusar) return strdup(respAuth0);
  if (strstr(ultimaUrl, "action=get_live_categories")) return strdup(respCats);
  if (strstr(ultimaUrl, "action=get_live_streams")) return strdup(respStreams);
  if (strstr(ultimaUrl, "action=get_short_epg")) return strdup(respEpg);
  if (!strstr(ultimaUrl, "action="))
    return strdup(soTs ? respContaSoTs : semFormatos ? respContaSemFormatos : respConta);
  return NULL;
}

static void lateCategoryRegression(void) {
  char *categories = malloc(160000); assert(categories);
  size_t used = (size_t)snprintf(categories, 160000, "[");
  for (int i = 0; i < 2049; i++)
    used += (size_t)snprintf(categories + used, 160000 - used,
      "%s{\"category_id\":%d,\"category_name\":\"%s\"}", i ? "," : "", i, i == 2048 ? "Hidden" : "Sports");
  snprintf(categories + used, 160000 - used, "]");
  const char *savedCats = respCats, *savedStreams = respStreams;
  respCats = categories;
  respStreams = "[{\"stream_id\":1,\"name\":\"Station 4K\",\"category_id\":2048}]";
  XtreamCanal channel;
  assert(xtream_canais(&channel, 1) == 1);
  assert(!strcmp(channel.categoria, "Hidden"));
  respCats = savedCats; respStreams = savedStreams; free(categories);
  puts("PASS: Xtream category beyond 2048 retains filter identity");
}
typedef struct {
  int categories, channels, expectedCategories, expectedChannels, stopAfter;
  unsigned source;
  unsigned char *seen;
} StreamCheck;
static void checkCategory(const char *name,int order,int count,void *u) {
  StreamCheck *s=u;
  assert(!s->channels && order==s->categories && order<s->expectedCategories);
  assert(!s->seen[order] && count>0);
  char expected[64];snprintf(expected,sizeof expected,"Category %03d",order);
  assert(!strcmp(name,expected));
  s->seen[order]=1;s->categories++;
}
static int checkChannel(const XtreamItem *c,void *u) {
  StreamCheck *s=u;
  assert(s->categories==s->expectedCategories);
  int i=s->channels++,cat=i%s->expectedCategories;
  char expected[80];
  if(s->source)snprintf(expected,sizeof expected,"xtream:@%08x:%d",s->source,i+1);
  else snprintf(expected,sizeof expected,"xtream:%d",i+1);
  assert(!strcmp(c->id,expected));
  snprintf(expected,sizeof expected,"Category %03d",cat);
  assert(!strcmp(c->categoria,expected) && c->catOrdem==cat);
  assert(c->num==i+1 && c->arquivo && c->arquivoDias==7);
  return !s->stopAfter || s->channels<s->stopAfter;
}

int main(void) {
  XtreamCanal c[16];
  char url[600];
  int n;

  assert(!xtream_configurado());
  assert(xtream_canais(c, 16) == 0);          // sem cadastro: nada, sem rede

  xtream_definir_servidor(" http://meu.servidor.tv:8080/ ");
  xtream_definir_usuario("joao@x");
  xtream_definir_senha("s&nha 1");
  assert(xtream_configurado());
  assert(!strcmp(xtream_servidor_curto(), "meu.servidor.tv:8080"));
  assert(!strcmp(xtream_usuario(), "joao@x"));
  assert(strstr(xtream_senha_mascarada(), "\xe2\x80\xa2") && !strstr(xtream_senha_mascarada(), "nha"));
  // Recarrega do "disco": o que foi gravado e o que volta.
  assert(strstr(disco, "servidor\thttp://meu.servidor.tv:8080\n"));
  assert(strstr(disco, "# CREDENCIAL"));
  puts("ok  cadastro: normaliza, mascara, grava com aviso");

  if (getenv("XTREAM_LATE_CATEGORY")) { lateCategoryRegression(); return 0; }
  n = xtream_canais(c, 16);
  assert(n == 3);                              // o de nome vazio fica de fora
  assert(xtream_ultima_falha() == XT_OK);
  assert(!strcmp(c[0].id, "xtream:101") && !strcmp(c[0].nome, "ESPN HD"));
  assert(!strcmp(c[0].categoria, "Esportes") && !strcmp(c[0].epgId, "espn.br"));
  printf("  c[1]: id=%s cat=%s\n", c[1].id, c[1].categoria);
  assert(!strcmp(c[1].id, "xtream:202") && !strcmp(c[1].categoria, "Not\xc3\xad" "cias"));
  assert(!strcmp(c[2].id, "xtream:404") && !strcmp(c[2].categoria, "Outros"));
  assert(strstr(ultimaUrl, "username=joao%40x&password=s%26nha%201&action=get_live_streams"));
  puts("ok  canais: id numero ou texto, categoria por id, sem categoria = Outros");
#ifdef NV_XTREAM_PROXY_TESTE
  // Tizen: painel http vai pelo worker, com a url INTEIRA escapada num
  // parametro so (o & da senha nao pode virar outro parametro do worker).
  // Tizen: painel http vai pelo worker, com a url no CORPO e o endereco de
  // entrada sem nada da pessoa (o que a plataforma registra).
  assert(ultimoPost && !strcmp(ultimaPedida, "https://rec.teste/v1/xtream"));
  assert(!strcmp(ultimoCorpo, "http://meu.servidor.tv:8080/player_api.php?username=joao%40x&password=s%26nha%201&action=get_live_streams"));
  puts("ok  tizen: lista http por POST NV_REC_URL/v1/xtream, url no corpo");
#else
  assert(!ultimoPost && !strncmp(ultimaPedida, "http://meu.servidor.tv:8080/player_api.php?", 43));
  puts("ok  lg: lista direto no painel");
#endif

  // #424: categories beyond 256 retain their names, including the last
  // channels of a large provider list. Preserve provider channel order.
  {
    const int nc = 2600, ns = 12000;
    char *cc = calloc((size_t)nc * 100 + 4, 1);
    char *ss = calloc((size_t)ns * 200 + 4, 1);
    XtreamCanal *large = calloc((size_t)ns, sizeof *large);
    assert(cc && ss && large);
    size_t a = 0, b = 0;
    a += sprintf(cc+a, "["); b += sprintf(ss+b, "[");
    for (int i = 0; i < nc; i++) a += sprintf(cc+a,
      "%s{\"category_id\":%d,\"category_name\":\"Category %03d\"}", i?",":"", i+1, i);
    for (int i = 0; i < ns; i++) b += sprintf(ss+b,
      "%s{\"stream_id\":%d,\"name\":\"Channel %d\",\"category_id\":%d,\"num\":%d,\"tv_archive\":1,\"tv_archive_duration\":7}", i?",":"", i+1, i, i%nc+1,i+1);
    strcpy(cc+a, "]"); strcpy(ss+b, "]");
    const char *oldCats = respCats, *oldStreams = respStreams;
    respCats = cc; respStreams = ss;
    assert(xtream_canais(large, ns) == ns && xtream_ultimo_total() == ns);
    for (int i = 0; i < ns; i++) {
      char expected[64]; snprintf(expected,sizeof expected,"Category %03d",i%nc);
      assert(!strcmp(large[i].categoria,expected));
      snprintf(expected,sizeof expected,"xtream:%d",i+1); assert(!strcmp(large[i].id,expected));
    }
    assert(xtream_canais(large, 17) == 17 && xtream_ultimo_total() == ns);
    // The latest guide consumes callbacks, including categories beyond its
    // former 2048-entry scratch limit, and must retain panel ordering even
    // though the backend indexes category IDs lexicographically.
    StreamCheck stream={.expectedCategories=nc,.expectedChannels=ns};
    stream.seen=calloc((size_t)nc,1);assert(stream.seen);
    int total=0;
    assert(xtream_canais_cb_sessao(checkCategory,checkChannel,&stream,&total,nv_perfil_sessao_ler())==ns);
    assert(total==ns && stream.channels==ns && stream.categories==nc);
    memset(stream.seen,0,(size_t)nc);stream.categories=stream.channels=0;
    // Different account selection does not alter a source-explicit guide
    // request. Qualified channel IDs still route playback to the right key.
#ifndef XTREAM_LEGACY_FIXTURE
    char savedDisk[sizeof disco];memcpy(savedDisk,disco,sizeof savedDisk);
    xtream_fonte_selecionar(37);
    assert(xtream_conta_definir("https://account37.test","other","fixture-password"));
    stream.source=37;stream.stopAfter=19;
    assert(xtream_fonte_canais_cb(37,checkCategory,checkChannel,&stream,&total,nv_perfil_sessao_ler())==ns);
    assert(total==ns && stream.channels==19 && xtream_fonte_total(37)==ns);
    assert(xtream_url("xtream:@00000025:101",url,sizeof url));
    assert(!strcmp(url,"https://account37.test/live/other/fixture-password/101.m3u8"));
    // Explicit legacy account request must not accidentally use the UI's
    // currently edited account, and must preserve account-zero IDs.
    memset(stream.seen,0,(size_t)nc);stream.categories=stream.channels=0;
    stream.source=0;stream.stopAfter=1;
    assert(xtream_fonte_canais_cb(0,checkCategory,checkChannel,&stream,&total,nv_perfil_sessao_ler())==ns);
    assert(stream.channels==1 && total==ns);
    assert(!strcmp(xtream_usuario(),"other"));
    xtream_fonte_selecionar(0);memcpy(disco,savedDisk,sizeof disco);
#endif
    free(stream.seen);
    respCats = oldCats; respStreams = oldStreams;
    free(large); free(ss); free(cc);
    puts("ok  #424: 12000 channels / 2600 categories preserve names, panel order, streaming account identity and early-stop total");
  }

  assert(xtream_e_id("xtream:101") && !xtream_e_id("stalker:1") && !xtream_e_id(NULL));
  assert(xtream_url("xtream:101", url, sizeof url));
  assert(!strcmp(url, "http://meu.servidor.tv:8080/live/joao%40x/s%26nha%201/101.m3u8"));
  assert(!xtream_url("tt123", url, sizeof url));
  puts("ok  url: servidor/live/usuario/senha/id.m3u8, com escape");

  recusar = 1;
  assert(xtream_canais(c, 16) == 0);           // auth 0 nao vira lista
  assert(xtream_ultima_falha() == XT_RECUSOU);
  recusar = 0;
  puts("ok  credencial recusada: zero canais, sem lixo");

  // #112: servidor que nao responde (o "falhou em http://..." do registro
  // 1647) e distinto de credencial recusada, e a proxima resposta boa limpa.
  mudo = 1;
  assert(xtream_canais(c, 16) == 0);
  assert(xtream_ultima_falha() == XT_SEM_RESPOSTA);
  mudo = 0;
  assert(xtream_canais(c, 16) == 3 && xtream_ultima_falha() == XT_OK);
  puts("ok  falha da lista: sem resposta, recusada e recuperada sao distintas");

  // #158: pagina HTML (Cloudflare) nao e "senha errada".
  pagina = 1;
  assert(xtream_canais(c, 16) == 0 && xtream_ultima_falha() == XT_PAGINA);
  pagina = 0;
  // 4xx que nao e credencial: o codigo fica para a tela.
  statusHttp = 458;
  assert(xtream_canais(c, 16) == 0 && xtream_ultima_falha() == XT_HTTP);
  assert(xtream_ultimo_http() == 458);
  statusHttp = 401;
  assert(xtream_canais(c, 16) == 0 && xtream_ultima_falha() == XT_RECUSOU);
  statusHttp = 200;
  assert(xtream_canais(c, 2) == 2 && xtream_ultima_falha() == XT_OK);   // corte anunciado
  puts("ok  lista: HTML, HTTP 458, 401 e corte sao distintos");

  { XtreamConta k;
    assert(xtream_conta(&k) && k.auth == 1 && !strcmp(k.status, "Active"));
    assert(k.expira == 1786324017LL && k.conexoes == 2 && k.maxConexoes == 5);
    assert(k.formatosDeclarados && k.temM3u8 && k.temTs);
    assert(xtream_conta_aviso(&k, 1786324017LL - 30 * 86400LL) == XA_NADA);
    assert(xtream_conta_aviso(&k, 1786324017LL - 3 * 86400LL) == XA_VENCE_LOGO);
    assert(xtream_conta_aviso(&k, 1786324017LL + 1) == XA_EXPIRADA);
    k.conexoes = 5;
    assert(xtream_conta_aviso(&k, 0) == XA_TELAS_CHEIAS);
    assert(xtream_conta_parse(respAuth0, &k) && !k.auth);
    assert(xtream_conta_aviso(&k, 0) == XA_RECUSOU);
    snprintf(k.status, sizeof k.status, "Banned"); k.auth = 1;
    assert(xtream_conta_aviso(&k, 0) == XA_DESATIVADA);
    assert(!xtream_conta_parse("<html>", &k) && k.conexoes == -1); }
  puts("ok  conta: user_info em texto, vencimento, telas, recusada, banida");

  { const char *ext[2]; int k;
    k = xtream_formatos(ext);
    assert(k == 2 && !strcmp(ext[0], "m3u8") && !strcmp(ext[1], "ts"));
    assert(xtream_url_formato("xtream:101", "ts", url, sizeof url));
    assert(!strcmp(url, "http://meu.servidor.tv:8080/live/joao%40x/s%26nha%201/101.ts"));
    // So .ts declarado: .m3u8 nem e tentado.
    soTs = 1; xtream_conta_ler(NULL); soTs = 0;
    k = xtream_formatos(ext);
    assert(k == 1 && !strcmp(ext[0], "ts"));
    assert(xtream_url("xtream:101", url, sizeof url) && strstr(url, "/101.ts"));
    // Declara so rtmp: vao os dois, na ordem de sempre.
    semFormatos = 1; xtream_conta_ler(NULL); semFormatos = 0;
    k = xtream_formatos(ext);
    assert(k == 2 && !strcmp(ext[0], "m3u8"));
    // O .ts tocou: vai na frente pelo resto da sessao.
    xtream_formato_funcionou("http://h/live/u/p/7.ts");
    k = xtream_formatos(ext);
    assert(k == 2 && !strcmp(ext[0], "ts") && !strcmp(ext[1], "m3u8"));
    xtream_formato_funcionou("http://h/live/u/p/7.m3u8");
    k = xtream_formatos(ext);
    assert(!strcmp(ext[0], "m3u8"));
    // Ajustes > Formato do Xtream: HLS pedido com a conta declarando so .ts
    // vem na frente, e o .ts declarado fica de segunda fonte (#158).
    soTs = 1; xtream_conta_ler(NULL); soTs = 0;
    prefFormato = 1; k = xtream_formatos(ext);
    assert(k == 2 && !strcmp(ext[0], "m3u8") && !strcmp(ext[1], "ts"));
    prefFormato = 2; k = xtream_formatos(ext);
    assert(k == 1 && !strcmp(ext[0], "ts"));
    prefFormato = 0;
    xtream_conta_ler(NULL); }
  puts("ok  formato: allowed_output_formats manda, o que tocou vai na frente, Ajustes pode pedir HLS");

  { XtreamProg pr[8]; int st = 0, k;
    k = xtream_epg_curto("xtream:101", pr, 8, &st);
    assert(st == 200 && k == 2);
    assert(strstr(ultimaUrl, "action=get_short_epg&stream_id=101&limit=8"));
    assert(!strcmp(pr[0].titulo, "\xc8\x98tirile Pro TV"));
    assert(pr[0].ini == 1790697600 && pr[0].fim == 1790701200);
    assert(!strcmp(pr[1].titulo, "Vremea") && pr[1].fim == 1790701800);
    assert(xtream_epg_curto("xtream:1&x=2", pr, 8, &st) == -1);   // id vai na query
    statusHttp = 404;
    assert(xtream_epg_curto("xtream:101", pr, 8, &st) == -1 && st == 404);
    statusHttp = 200;
    assert(xtream_epg_parse("{\"epg_listings\":[]}", pr, 8) == 0);
    assert(xtream_epg_parse("[]", pr, 8) == 0); }
  puts("ok  grade curta: base64, epoch, id so com digitos, 404");

  xtream_esquecer();
  assert(!xtream_configurado() && !temDisco);
  assert(!xtream_url("xtream:101", url, sizeof url));
  puts("ok  esquecer: apaga o arquivo e a URL deixa de existir");

  xtream_definir_servidor("https://seguro.tv");
  assert(!strcmp(xtream_servidor_curto(), "seguro.tv"));
  assert(strstr(disco, "servidor\thttps://seguro.tv\n"));
  puts("ok  https preservado quando digitado");
  // https nao tem o bloqueio: vai direto nos dois alvos, sem terceiro no meio.
  xtream_definir_usuario("u"); xtream_definir_senha("p");
  assert(xtream_canais(c, 16) == 3);
  assert(!ultimoPost && !strncmp(ultimaPedida, "https://seguro.tv/player_api.php?username=u&password=p&action=", 62));
  // O video nunca passa pelo proxy: e a url do painel, que vai ao AVPlay.
  assert(xtream_url("xtream:9", url, sizeof url) && !strcmp(url, "https://seguro.tv/live/u/p/9.m3u8"));
  xtream_definir_servidor("http://meu.servidor.tv:8080");
  assert(xtream_url("xtream:9", url, sizeof url) && !strcmp(url, "http://meu.servidor.tv:8080/live/u/p/9.m3u8"));
  puts("ok  https direto; url de video sempre direta");
  // #237: URL colada com caminho (o painel Xtream fica sempre na raiz) —
  // o caminho cai fora, o esquema e a porta ficam.
  xtream_definir_servidor("http://srv.tv:8080/c/");
  assert(!strcmp(xtream_servidor_curto(), "srv.tv:8080"));
  xtream_definir_servidor("  https://srv.tv/custom/player_api.php?x=1 ");
  assert(!strcmp(xtream_servidor_curto(), "srv.tv"));
  assert(strstr(disco, "servidor\thttps://srv.tv\n"));
  xtream_definir_servidor("srv.tv:8000/xyz");
  assert(strstr(disco, "servidor\thttp://srv.tv:8000\n"));
  puts("ok  servidor com caminho colado normaliza para a raiz");
  puts("xtream: tudo ok");
  return 0;
}
