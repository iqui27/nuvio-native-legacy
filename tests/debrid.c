// Os tres resolvedores de debrid (Real-Debrid, TorBox, Premiumize) contra uma
// rede FALSA que devolve o formato de resposta de cada API, e o parser
// aceitando torrent sem url. Sem SDL, sem rede.
//
// A rede falsa DESPACHA PELA ROTA em vez de responder sempre a mesma coisa:
// e o unico jeito de o teste provar que cada servico foi pelo caminho certo —
// checkcached antes de createtorrent no TorBox, cache/check antes de directdl
// no Premiumize — e nao so que "alguma coisa devolveu uma URL".
//
// STDOUT E CAPTURADO durante os testes e conferido no fim: nenhuma das tres
// chaves pode aparecer no que o app imprime. As mensagens de progresso do
// proprio teste saem por stderr, que nao e capturado.
#include "../src/debrid.h"
#include "../src/streams.h"
#include "../src/rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define K_RD "CHAVE-RD-SEGREDO-1"
#define K_TB "CHAVE-TB-SEGREDO-2"
#define K_PM "CHAVE-PM-SEGREDO-3"

#define OK(s) fprintf(stderr, "ok  %s\n", s)

// ---------------------------------------------------------------- rede falsa

static int   pmEmCache = 1, tbEmCache = 1;     // o que a consulta de cache diz
static char  corpoSelectRD[100];               // files=<id> do Real-Debrid
static char  ctCriarTB[120];                   // Content-Type do createtorrent
static char  urlRequestdl[900];                // a URL com o token na query
static char  corpoCacheCheckPM[600], corpoDirectdlPM[600];
static int   bateuTB[4];        // checkcached, createtorrent, mylist, requestdl
static int   bateuPM[2];        // cache/check, directdl
static int   bateuRDUnrestrict;
// createtorrent do TorBox: 0 = 201 normal, 1 = 403 de CONTA (plano), 2 = 403
// que diz "nao esta em cache" (do torrent, nao da conta)
static int   tbCriar403;

static char *dup2s(const char *s) { return strdup(s); }

char *rede_postar_st(const char *url, int s, const char *const *cab,
                     const char *corpo, int *st) {
  int k;
  char ct[120] = "";
  (void)s;
  for (k = 0; cab && cab[k]; k++)
    if (!strncmp(cab[k], "Content-Type:", 13)) snprintf(ct, sizeof ct, "%s", cab[k]);

  // --- Real-Debrid
  if (strstr(url, "real-debrid.com")) {
    assert(strstr(ct, "x-www-form-urlencoded"));
    if (strstr(url, "addMagnet"))   { *st = 201; return dup2s("{\"id\":\"ABC123\",\"uri\":\"x\"}"); }
    if (strstr(url, "selectFiles")) { *st = 204;
      snprintf(corpoSelectRD, sizeof corpoSelectRD, "%s", corpo); return dup2s(""); }
    if (strstr(url, "unrestrict"))  { *st = 200; bateuRDUnrestrict = 1;
      return dup2s("{\"download\":\"https://x.download.real-debrid.com/d/QQ/Show.S02E05.mkv\","
                   "\"filename\":\"Show.S02E05.mkv\"}"); }
  }
  // --- TorBox: createtorrent e o unico POST
  if (strstr(url, "api.torbox.app")) {
    assert(strstr(url, "/v1/api/torrents/createtorrent"));
    snprintf(ctCriarTB, sizeof ctCriarTB, "%s", ct);
    // o corpo e multipart de verdade, com os dois campos que a API pede
    assert(strstr(corpo, "name=\"magnet\""));
    assert(strstr(corpo, "magnet:?xt=urn:btih:deadbeef"));
    assert(strstr(corpo, "name=\"add_only_if_cached\""));
    bateuTB[1]++;
    // O corpo de conta traz a PROPRIA CHAVE de proposito: a API real nao
    // deveria ecoa-la, e o teste prova que, se ecoar, o log nao a leva.
    if (tbCriar403 == 1) { *st = 403;
      return dup2s("{\"success\":false,\"error\":\"PLAN_RESTRICTED_FEATURE\","
                   "\"detail\":\"Your plan does not allow this. key=" K_TB "\",\"data\":null}"); }
    if (tbCriar403 == 2) { *st = 403;
      return dup2s("{\"success\":false,\"error\":\"DOWNLOAD_NOT_CACHED\","
                   "\"detail\":\"Torrent is not cached.\",\"data\":null}"); }
    *st = 201;
    return dup2s("{\"success\":true,\"error\":null,\"detail\":\"Found cached torrent\","
                 "\"data\":{\"hash\":\"deadbeef\",\"torrent_id\":77,\"auth_id\":\"z\"}}");
  }
  // --- Premiumize
  if (strstr(url, "premiumize.me")) {
    assert(strstr(ct, "x-www-form-urlencoded"));
    if (strstr(url, "cache/check")) {
      bateuPM[0]++; *st = 200;
      snprintf(corpoCacheCheckPM, sizeof corpoCacheCheckPM, "%s", corpo);
      return dup2s(pmEmCache
        ? "{\"status\":\"success\",\"response\":[true],\"transcoded\":[false],"
          "\"filename\":[\"Show S02\"],\"filesize\":[\"3900000000\"]}"
        : "{\"status\":\"success\",\"response\":[false],\"transcoded\":[false],"
          "\"filename\":[null],\"filesize\":[0]}");
    }
    if (strstr(url, "transfer/directdl")) {
      bateuPM[1]++; *st = 200;
      snprintf(corpoDirectdlPM, sizeof corpoDirectdlPM, "%s", corpo);
      return dup2s("{\"status\":\"success\",\"content\":["
        "{\"path\":\"Show S02/Show.S02E04.1080p.mkv\",\"size\":2000000000,"
         "\"link\":\"https://a.pm.me/dl/AAA/E04.mkv\",\"stream_link\":\"https://a.pm.me/st/AAA\"},"
        "{\"path\":\"Show S02/Show.S02E05.1080p.mkv\",\"size\":1900000000,"
         "\"link\":\"https://a.pm.me/dl/BBB/E05.mkv\",\"stream_link\":\"https://a.pm.me/st/BBB\"},"
        "{\"path\":\"Show S02/sample.txt\",\"size\":10,"
         "\"link\":\"https://a.pm.me/dl/CCC/sample.txt\"}]}");
    }
  }
  *st = 404; return dup2s("{}");
}

char *rede_baixar_st(const char *url, int s, const char *const *cab, int *st) {
  (void)s; (void)cab; *st = 200;

  if (strstr(url, "real-debrid.com")) {
    assert(strstr(url, "torrents/info/ABC123"));
    return dup2s("{\"id\":\"ABC123\",\"status\":\"downloaded\",\"files\":["
      "{\"id\":1,\"path\":\"/Show.S02E04.1080p.mkv\",\"bytes\":2000000000,\"selected\":0},"
      "{\"id\":2,\"path\":\"/Show.S02E05.1080p.mkv\",\"bytes\":1900000000,\"selected\":0},"
      "{\"id\":3,\"path\":\"/sample.txt\",\"bytes\":10,\"selected\":0}],"
      "\"links\":[\"https://real-debrid.com/d/LINK1\"]}");
  }
  if (strstr(url, "api.torbox.app")) {
    if (strstr(url, "torrents/checkcached")) {
      bateuTB[0]++;
      // hash SEMPRE em minusculas: o teste manda "DEADBEEF" e a API compara texto
      assert(strstr(url, "hash=deadbeef") && strstr(url, "format=list"));
      return dup2s(tbEmCache
        ? "{\"success\":true,\"error\":null,\"data\":"
          "[{\"name\":\"Show S02\",\"size\":3900000000,\"hash\":\"deadbeef\"}]}"
        : "{\"success\":true,\"error\":null,\"data\":[]}");
    }
    if (strstr(url, "torrents/mylist")) {
      bateuTB[2]++;
      assert(strstr(url, "id=77"));
      return dup2s("{\"success\":true,\"data\":{\"id\":77,\"name\":\"Show S02\","
        "\"download_finished\":true,\"files\":["
        "{\"id\":0,\"name\":\"Show S02/Show.S02E04.1080p.mkv\",\"size\":2000000000,"
         "\"short_name\":\"Show.S02E04.1080p.mkv\",\"mimetype\":\"video/x-matroska\"},"
        "{\"id\":1,\"name\":\"Show S02/Show.S02E05.1080p.mkv\",\"size\":1900000000,"
         "\"short_name\":\"Show.S02E05.1080p.mkv\",\"mimetype\":\"video/x-matroska\"},"
        "{\"id\":2,\"name\":\"Show S02/sample.txt\",\"size\":10,"
         "\"short_name\":\"sample.txt\",\"mimetype\":\"text/plain\"}]}}");
    }
    if (strstr(url, "torrents/requestdl")) {
      bateuTB[3]++;
      snprintf(urlRequestdl, sizeof urlRequestdl, "%s", url);
      return dup2s("{\"success\":true,\"error\":null,\"detail\":\"ok\","
                   "\"data\":\"https://store-1.torbox.app/dl/ZZZ/Show.S02E05.1080p.mkv\"}");
    }
  }
  *st = 404; return dup2s("{}");
}

// o parser puxa badges; duble
unsigned long long badges_detectar(const char *t) { (void)t; return 0; }

// ---------------------------------------------------------------- testes

static void semChaves(void) {
  char url[4096] = "";
  assert(!debrid_ativo());
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  debrid_definir_chave("offcloud", "k");     // servico que ninguem resolve aqui
  assert(!debrid_ativo());
  OK("sem chave nao resolve, e servico desconhecido continua ignorado");
}

static void realDebrid(void) {
  char url[4096] = "";
  debrid_esquecer();
  debrid_definir_chave("real-debrid", K_RD);
  assert(debrid_ativo());

  debrid_definir_episodio(2, 5);
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!strcmp(url, "https://x.download.real-debrid.com/d/QQ/Show.S02E05.mkv"));
  assert(!strcmp(corpoSelectRD, "files=2"));   // S02E05, e nao o maior (S02E04)
  assert(bateuRDUnrestrict);
  OK("Real-Debrid resolve 2x05 pelo nome do arquivo");

  debrid_definir_episodio(0, 0);
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!strcmp(corpoSelectRD, "files=1"));   // filme: maior video, sample.txt fora
  OK("Real-Debrid: filme escolhe o maior video");

  // A URL RESOLVIDA E CREDENCIAL: quem tem o "/d/QQ/" baixa na conta de quem
  // pediu. Este teste linka o rede_url_publica DE VERDADE (src/redeurl.c) em
  // vez de um stub — com stub, uma regressao que passasse a imprimir o link
  // inteiro sairia verde.
  { char seg[120];
    const char *pub = rede_url_publica(url, seg, sizeof seg);
    assert(!strstr(pub, "/d/QQ"));
    assert(!strstr(pub, "Show.S02E05.mkv"));
    assert(strstr(pub, "x.download.real-debrid.com"));  // o host fica: e o que diagnostica
    assert(strstr(pub, "/...")); }
  OK("o link do Real-Debrid nao vai inteiro para o log");
}

static void torbox(void) {
  char url[4096] = "";
  debrid_esquecer();
  memset(bateuTB, 0, sizeof bateuTB);
  debrid_definir_chave("torbox", K_TB);
  assert(debrid_ativo());          // debrid_ativo vale para os tres agora

  tbEmCache = 1;
  debrid_definir_episodio(2, 5);
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!strcmp(url, "https://store-1.torbox.app/dl/ZZZ/Show.S02E05.1080p.mkv"));
  assert(bateuTB[0] == 1 && bateuTB[1] == 1 && bateuTB[2] == 1 && bateuTB[3] == 1);
  // os campos do TorBox sao name/size, nao path/bytes: se escolherArquivo
  // tivesse ficado preso aos nomes do RD, nenhum arquivo casaria e file_id=-1
  assert(strstr(urlRequestdl, "file_id=1"));     // o S02E05, nao o maior
  assert(strstr(urlRequestdl, "torrent_id=77"));
  assert(strstr(ctCriarTB, "multipart/form-data; boundary="));
  OK("TorBox: checkcached -> createtorrent -> mylist -> requestdl, arquivo por SxxEyy");

  // fora de cache: para na primeira consulta e NAO manda criar nada — a regra
  // da casa e nunca pedir ao servico que comece a baixar
  memset(bateuTB, 0, sizeof bateuTB);
  tbEmCache = 0; url[0] = 0;
  assert(!debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!url[0]);
  assert(bateuTB[0] == 1 && bateuTB[1] == 0 && bateuTB[2] == 0 && bateuTB[3] == 0);
  OK("TorBox fora de cache devolve 0 sem criar torrent");
  tbEmCache = 1;
}

// B3, registro 1541: TorBox com createtorrent 403 x8 na mesma busca, e o
// Premiumize so perguntado depois de cada um.
static void torbox403(void) {
  char url[4096] = "", rec[64] = "";
  debrid_esquecer();
  debrid_definir_chave("torbox", K_TB);
  debrid_definir_chave("premiumize", K_PM);
  debrid_definir_episodio(2, 5);
  debrid_nova_busca();

  // 403 de CONTA: o primeiro torrent tenta TorBox, recebe 403, e cai no
  // Premiumize na mesma chamada
  memset(bateuTB, 0, sizeof bateuTB); memset(bateuPM, 0, sizeof bateuPM);
  tbCriar403 = 1; pmEmCache = 1;
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(strstr(url, "a.pm.me"));
  assert(bateuTB[1] == 1 && bateuPM[0] == 1);
  assert(debrid_recusa(rec, sizeof rec));
  assert(!strcmp(rec, "TorBox 403"));
  // os torrents seguintes da MESMA busca nem perguntam ao TorBox
  url[0] = 0;
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(bateuTB[0] == 1 && bateuTB[1] == 1);
  assert(bateuPM[0] == 3);
  OK("TorBox 403 de conta: para nesta busca e passa ao Premiumize");

  // busca nova: o TorBox volta a ser tentado (a conta pode ter sido acertada)
  debrid_nova_busca();
  assert(!debrid_recusa(rec, sizeof rec));
  tbCriar403 = 0;
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(strstr(url, "torbox.app"));
  assert(bateuTB[1] == 2);
  OK("debrid_nova_busca esquece a recusa");

  // 403 que diz "nao esta em cache" e do TORRENT: o seguinte tenta de novo
  debrid_nova_busca();
  memset(bateuTB, 0, sizeof bateuTB);
  tbCriar403 = 2; pmEmCache = 0;
  assert(!debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(bateuTB[1] == 2);
  assert(!debrid_recusa(rec, sizeof rec));
  OK("403 de 'not cached' nao bloqueia o servico");
  tbCriar403 = 0; pmEmCache = 1;
  debrid_esquecer();
}

static void premiumize(void) {
  char url[4096] = "";
  debrid_esquecer();
  memset(bateuPM, 0, sizeof bateuPM);
  debrid_definir_chave("premiumize", K_PM);
  assert(debrid_ativo());

  pmEmCache = 1;
  debrid_definir_episodio(2, 5);
  assert(debrid_resolver("deadbeef", -1, url, sizeof url));
  // "link" e nao "stream_link" (o transcodificado)
  assert(!strcmp(url, "https://a.pm.me/dl/BBB/E05.mkv"));
  assert(bateuPM[0] == 1 && bateuPM[1] == 1);
  assert(strstr(corpoCacheCheckPM, "items%5B%5D=magnet%3A%3Fxt%3Durn%3Abtih%3Adeadbeef"));
  assert(strstr(corpoDirectdlPM, "src=magnet%3A%3Fxt%3Durn%3Abtih%3Adeadbeef"));
  OK("Premiumize: cache/check -> transfer/directdl, link do S02E05");

  debrid_definir_episodio(0, 0);
  url[0] = 0;
  assert(debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(!strcmp(url, "https://a.pm.me/dl/AAA/E04.mkv"));   // maior video; sample.txt fora
  OK("Premiumize: filme escolhe o maior video");

  memset(bateuPM, 0, sizeof bateuPM);
  pmEmCache = 0; url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(!url[0]);
  assert(bateuPM[0] == 1 && bateuPM[1] == 0);   // nao chega a pedir o link
  OK("Premiumize fora de cache devolve 0 sem pedir directdl");
  pmEmCache = 1;
}

static void ordem(void) {
  char url[4096] = "";
  // Com TorBox e Premiumize os dois em cache, ganha o TorBox: a ordem e fixa
  // (Real-Debrid, TorBox, Premiumize) para o log ser previsivel.
  debrid_esquecer();
  memset(bateuPM, 0, sizeof bateuPM);
  debrid_definir_chave("premiumize", K_PM);
  debrid_definir_chave("torbox", K_TB);
  debrid_definir_episodio(2, 5);
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(strstr(url, "torbox.app"));
  assert(bateuPM[0] == 0);          // nem chegou a perguntar ao Premiumize
  OK("com duas chaves o TorBox vem antes do Premiumize");

  // E com as tres, o Real-Debrid vem antes de todo mundo.
  debrid_definir_chave("realdebrid", K_RD);
  memset(bateuTB, 0, sizeof bateuTB);
  url[0] = 0;
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(strstr(url, "real-debrid.com"));
  assert(bateuTB[0] == 0);
  OK("com as tres chaves o Real-Debrid resolve primeiro");

  debrid_esquecer();
  assert(!debrid_ativo());
}

static void parser(void) {
  Stream *v = NULL;
  int n = stream_extrair(
    "{\"streams\":[{\"name\":\"Torrentio\",\"title\":\"Show S02E05 1080p\",\"infoHash\":\"abcdef0123\",\"fileIdx\":1},"
    "{\"name\":\"X\",\"externalUrl\":\"https://cdn/x.mp4\"},"
    "{\"name\":\"Y\",\"url\":\"magnet:?xt=urn:btih:ff\"},"
    "{\"name\":\"Z\",\"title\":\"cached\",\"clientResolve\":{\"type\":\"debrid\",\"service\":\"realdebrid\",\"infoHash\":\"0011\",\"isCached\":true}}]}",
    "Torrentio", &v);
  assert(n == 3);
  assert(!v[0].url[0] && !strcmp(v[0].infoHash, "abcdef0123") && v[0].fileIdx == 1);
  assert(!strcmp(v[1].url, "https://cdn/x.mp4") && !v[1].infoHash[0]);
  assert(!v[2].url[0] && !strcmp(v[2].infoHash, "0011"));
  free(v);
  OK("parser: infoHash no topo e em clientResolve, externalUrl, magnet fora");
}

int main(void) {
  static char capt[200000];
  const char *arq = "/tmp/nuvio-debrid-saida.txt";
  int velho, f; FILE *lido; size_t lidos;

  // Captura o stdout do APP. Tudo o que debrid.c imprime vai para o arquivo, e
  // no fim conferimos que nenhuma chave saiu por ali. As chamadas OK() escrevem
  // em stderr de proposito, para continuarem visiveis.
  fflush(stdout);
  velho = dup(1);
  f = open(arq, O_RDWR | O_CREAT | O_TRUNC, 0600);
  assert(velho >= 0 && f >= 0);
  assert(dup2(f, 1) >= 0);

  semChaves();
  realDebrid();
  torbox();
  torbox403();
  premiumize();
  ordem();
  parser();

  fflush(stdout);
  assert(dup2(velho, 1) >= 0);
  close(velho); close(f);

  lido = fopen(arq, "r");
  assert(lido);
  lidos = fread(capt, 1, sizeof capt - 1, lido);
  capt[lidos] = 0;
  fclose(lido);
  remove(arq);

  // NENHUMA CHAVE NO QUE O APP IMPRIME. Vale para as tres, e vale tambem para
  // a URL do requestdl do TorBox, que leva o token na query porque a API nao
  // oferece outro jeito — ela nao pode vazar por um printf de diagnostico.
  assert(!strstr(capt, K_RD));
  assert(!strstr(capt, K_TB));
  assert(!strstr(capt, K_PM));
  assert(!strstr(capt, "token="));
  assert(strstr(urlRequestdl, "token=" K_TB));   // o teste de fato passou por la
  // nem o caminho das URLs resolvidas dos tres servicos
  assert(!strstr(capt, "/d/QQ"));
  assert(!strstr(capt, "/dl/ZZZ"));
  assert(!strstr(capt, "/dl/BBB"));
  // mas o host fica, que e o que diagnostica qual servico respondeu
  assert(strstr(capt, "store-1.torbox.app"));
  assert(strstr(capt, "a.pm.me"));
  OK("nenhuma chave e nenhum caminho de link sai no stdout do app");

  // O CORPO DO 403 ESTA NO LOG, com a linha no formato que se procura no D1,
  // e a chave que ele trazia virou ***.
  assert(strstr(capt, "[debrid] TorBox createtorrent: HTTP 403 ("));
  assert(strstr(capt, "PLAN_RESTRICTED_FEATURE"));
  assert(strstr(capt, "key=***"));
  assert(strstr(capt, "Torrent is not cached."));
  assert(strstr(capt, "[debrid] TorBox: recusa da conta (HTTP 403)"));
  OK("corpo do 403 no log, sem a chave");

  puts("debrid: tudo ok");
  return 0;
}
