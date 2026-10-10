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
#define K_AD "CHAVE-AD-SEGREDO-4"
#define K_AD_LOCAL "CHAVE-AD-LOCAL-5"

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
// 3 = 403 de conta que PASSA (ACTIVE_LIMIT): recusa so desta busca.
// Premiumize de conta gratuita: directdl responde 200 "Account not premium."
// (registros 1731-1774, texto de verdade).
static int   pmSemPlano;
// "P2P" do TorBox: o valor de add_only_if_cached do ultimo createtorrent
// (1 = "true") e se o mylist diz que o torrent ainda esta baixando.
static int   tbSoCache = -1, tbBaixando;

static char *dup2s(const char *s) { return strdup(s); }

// --- AllDebrid. Modos: o que o upload responde, a sequencia de statusCode que o
// status devolve, e o que cada rota recebeu (para provar a ORDEM e a auth).
static int  adUploadModo;       // 0 pronto, 1 fora de cache, 2 AUTH_BAD_APIKEY,
                                // 3 MUST_BE_PREMIUM, 4 TOO_MANY_ACTIVE, 5 magnet invalido
static int  adSeqStatus[8], adNSeq, adIdxStatus;   // statusCode em cada olhada
static int  bateuAD[8];         // upload, status, files, unlock, delete, user, delayed
static char adAuthVisto[200], adUrlVista[400], adCorpoUnlock[900];
static int  adUserPremium = 1, adUserRuim;
static int  adOrdem[16], adNOrdem;
static void adRegistra(int rota) { if (adNOrdem < 16) adOrdem[adNOrdem++] = rota; bateuAD[rota]++; }
static char *adResp(const char *url, const char *const *cab, const char *corpo, int *st) {
  int k;
  *st = 200;
  snprintf(adUrlVista, sizeof adUrlVista, "%s", url);
  for (k = 0; cab && cab[k]; k++)
    if (!strncmp(cab[k], "Authorization:", 14)) snprintf(adAuthVisto, sizeof adAuthVisto, "%s", cab[k]);
  assert(strstr(url, "agent=nuvio"));
  assert(!strstr(url, "CHAVE-AD"));               // a chave nunca vai na URL
  if (strstr(url, "/v4/magnet/upload")) {
    adRegistra(0);
    assert(corpo && strstr(corpo, "magnets%5B%5D=magnet%3A%3Fxt%3Durn%3Abtih%3Adeadbeef"));
    if (adUploadModo == 2) {
      // a API real nao ecoa a chave; se ecoasse (esta), o log a cortaria
      char b[400];
      snprintf(b, sizeof b, "{\"status\":\"error\",\"error\":{\"code\":\"AUTH_BAD_APIKEY\","
               "\"message\":\"The auth apikey is invalid %s\"}}", adAuthVisto + 22);
      return dup2s(b);
    }
    if (adUploadModo == 3) return dup2s("{\"status\":\"success\",\"data\":{\"magnets\":[{\"magnet\":\"x\","
                                        "\"error\":{\"code\":\"MAGNET_MUST_BE_PREMIUM\",\"message\":\"Premium required\"}}]}}");
    if (adUploadModo == 4) return dup2s("{\"status\":\"error\",\"error\":{\"code\":\"MAGNET_TOO_MANY_ACTIVE\","
                                        "\"message\":\"Too many active magnets\"}}");
    if (adUploadModo == 5) return dup2s("{\"status\":\"success\",\"data\":{\"magnets\":[{\"magnet\":\"x\","
                                        "\"error\":{\"code\":\"MAGNET_INVALID_URI\",\"message\":\"Magnet is not valid\"}}]}}");
    return dup2s(adUploadModo == 1
      ? "{\"status\":\"success\",\"data\":{\"magnets\":[{\"magnet\":\"x\",\"hash\":\"deadbeef\",\"name\":\"Show S02\",\"size\":1000,\"ready\":false,\"id\":555}]}}"
      : "{\"status\":\"success\",\"data\":{\"magnets\":[{\"magnet\":\"x\",\"hash\":\"deadbeef\",\"name\":\"Show S02\",\"size\":3900000000,\"ready\":true,\"id\":555}]}}");
  }
  if (strstr(url, "/v4.1/magnet/status")) {
    int sc = adSeqStatus[adIdxStatus < adNSeq ? adIdxStatus : adNSeq - 1];
    adRegistra(1); adIdxStatus++;
    assert(strstr(corpo, "id=555"));
    { char b[300];
      snprintf(b, sizeof b, "{\"status\":\"success\",\"data\":{\"magnets\":[{\"id\":555,\"filename\":\"Show S02\","
               "\"size\":1000,\"status\":\"x\",\"statusCode\":%d,\"downloaded\":%d,\"seeders\":3}]}}", sc, sc == 4 ? 1000 : sc == 0 ? 0 : 370);
      return dup2s(b); }
  }
  if (strstr(url, "/v4/magnet/files")) {
    adRegistra(2);
    assert(strstr(corpo, "id%5B%5D=555"));
    return dup2s("{\"status\":\"success\",\"data\":{\"magnets\":[{\"id\":\"555\",\"files\":["
      "{\"n\":\"Show.S02\",\"e\":["
        "{\"n\":\"Show.S02E04.1080p.mkv\",\"s\":2000000000,\"l\":\"https:\\/\\/alldebrid.com\\/f\\/AAA\"},"
        "{\"n\":\"Show.S02E05.1080p.mkv\",\"s\":1900000000,\"l\":\"https:\\/\\/alldebrid.com\\/f\\/BBB\"},"
        "{\"n\":\"sample.txt\",\"s\":10,\"l\":\"https:\\/\\/alldebrid.com\\/f\\/CCC\"}]}]}]}}");
  }
  if (strstr(url, "/v4/link/unlock")) {
    adRegistra(3);
    snprintf(adCorpoUnlock, sizeof adCorpoUnlock, "%s", corpo);
    if (strstr(corpo, "%2Ff%2FBBB"))
      return dup2s("{\"status\":\"success\",\"data\":{\"link\":\"https:\\/\\/s1.debrid.it\\/dl\\/UNL5\\/Show.S02E05.mkv\","
                   "\"filename\":\"Show.S02E05.1080p.mkv\",\"host\":\"magnet\",\"filesize\":1900000000}}");
    return dup2s("{\"status\":\"success\",\"data\":{\"link\":\"https:\\/\\/s1.debrid.it\\/dl\\/UNL4\\/Show.S02E04.mkv\"}}");
  }
  if (strstr(url, "/v4/magnet/delete")) { adRegistra(4); return dup2s("{\"status\":\"success\",\"data\":{}}"); }
  if (strstr(url, "/v4/user")) {
    adRegistra(5);
    if (adUserRuim) return dup2s("{\"status\":\"error\",\"error\":{\"code\":\"AUTH_BAD_APIKEY\",\"message\":\"bad\"}}");
    return dup2s(adUserPremium
      ? "{\"status\":\"success\",\"data\":{\"user\":{\"username\":\"fulano-segredo\",\"email\":\"f@x.com\","
        "\"isPremium\":true,\"premiumUntil\":1798761600}}}"
      : "{\"status\":\"success\",\"data\":{\"user\":{\"username\":\"fulano-segredo\",\"isPremium\":false,\"premiumUntil\":0}}}");
  }
  return dup2s("{}");
}


char *rede_postar_st(const char *url, int s, const char *const *cab,
                     const char *corpo, int *st) {
  int k;
  char ct[120] = "";
  (void)s;
  for (k = 0; cab && cab[k]; k++)
    if (!strncmp(cab[k], "Content-Type:", 13)) snprintf(ct, sizeof ct, "%s", cab[k]);

  // --- AllDebrid
  if (strstr(url, "api.alldebrid.com")) {
    assert(strstr(ct, "x-www-form-urlencoded"));
    return adResp(url, cab, corpo, st);
  }

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
    tbSoCache = strstr(corpo, "name=\"add_only_if_cached\"\r\n\r\ntrue\r\n") ? 1 : 0;
    bateuTB[1]++;
    // O corpo de conta traz a PROPRIA CHAVE de proposito: a API real nao
    // deveria ecoa-la, e o teste prova que, se ecoar, o log nao a leva.
    if (tbCriar403 == 1) { *st = 403;
      return dup2s("{\"success\":false,\"error\":\"PLAN_RESTRICTED_FEATURE\","
                   "\"detail\":\"Your plan does not allow this. key=" K_TB "\",\"data\":null}"); }
    if (tbCriar403 == 3) { *st = 403;
      return dup2s("{\"success\":false,\"error\":\"ACTIVE_LIMIT\","
                   "\"detail\":\"Too many active downloads.\",\"data\":null}"); }
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
      if (pmSemPlano)
        return dup2s("{\"status\":\"error\",\"message\":\"Account not premium.\"}");
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
  (void)s; *st = 200;

  if (strstr(url, "api.alldebrid.com")) return adResp(url, cab, NULL, st);
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
      // Baixando: o TorBox ja listou os arquivos (leu os metadados), mas o
      // download nao terminou — o requestdl falharia.
      if (tbBaixando)
        return dup2s("{\"success\":true,\"data\":{\"id\":77,\"name\":\"Show S02\","
          "\"download_state\":\"downloading\",\"download_finished\":false,"
          "\"download_present\":false,\"progress\":0.37,\"files\":["
          "{\"id\":1,\"name\":\"Show S02/Show.S02E05.1080p.mkv\",\"size\":1900000000}]}}");
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
unsigned long long badges_bit(const char *id) { (void)id; return 0; }

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

// O "P2P" DO TORBOX (relato do Reddit, 1.4.4: "TorBox P2P videos don't work,
// they were working fine in 1.3.5"). Torrent fora de cache no TorBox:
//   - o AUTOMATICO continua so com cache: nao cria nada, conta como fora;
//   - a ESCOLHA MANUAL manda o TorBox baixar (add_only_if_cached=false) e,
//     enquanto baixa, devolve DEBRID_BAIXANDO com o progresso; terminado,
//     toca pelo requestdl.
static void torboxP2P(void) {
  char url[4096] = "", serv[32] = "";
  int pct = 0;
  debrid_esquecer();
  debrid_definir_chave("torbox", K_TB);
  debrid_definir_episodio(2, 5);
  debrid_nova_busca();
  tbEmCache = 0; tbCriar403 = 0;

  memset(bateuTB, 0, sizeof bateuTB);
  assert(!debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(bateuTB[0] == 1 && bateuTB[1] == 0);
  assert(debrid_fora_de_cache() == 1);
  OK("P2P TorBox: o automatico nao manda baixar, e conta o torrent fora de cache");

  memset(bateuTB, 0, sizeof bateuTB);
  tbBaixando = 1; tbSoCache = -1; url[0] = 0;
  assert(debrid_resolver_escolhido("DEADBEEF", -1, url, sizeof url, serv, sizeof serv, &pct)
         == DEBRID_BAIXANDO);
  assert(!url[0]);
  assert(!strcmp(serv, "TorBox") && pct == 37);
  assert(tbSoCache == 0);                      // mandou BAIXAR, nao "so se em cache"
  assert(bateuTB[0] == 1 && bateuTB[1] == 1);  // checkcached uma vez, um createtorrent
  assert(bateuTB[3] == 0);                     // requestdl de torrent inacabado falharia
  OK("P2P TorBox: escolha manual cria o torrent sem add_only_if_cached e diz 37% baixando");

  memset(bateuTB, 0, sizeof bateuTB);
  tbBaixando = 0; url[0] = 0;
  assert(debrid_resolver_escolhido("DEADBEEF", -1, url, sizeof url, serv, sizeof serv, &pct) == 1);
  assert(!strcmp(url, "https://store-1.torbox.app/dl/ZZZ/Show.S02E05.1080p.mkv"));
  assert(strstr(urlRequestdl, "file_id=1"));
  OK("P2P TorBox: terminado o download, a mesma escolha toca");

  // Em cache: a escolha manual e igual ao automatico (add_only_if_cached=true)
  memset(bateuTB, 0, sizeof bateuTB);
  tbEmCache = 1; tbSoCache = -1;
  assert(debrid_resolver_escolhido("DEADBEEF", -1, url, sizeof url, serv, sizeof serv, &pct) == 1);
  assert(tbSoCache == 1 && bateuTB[1] == 1);
  OK("escolha manual de torrent em cache nao pede download");

  // Cacheado no Premiumize e fora no TorBox: toca pelo Premiumize e o TorBox
  // NAO e mandado baixar.
  debrid_definir_chave("premiumize", K_PM);
  memset(bateuTB, 0, sizeof bateuTB); memset(bateuPM, 0, sizeof bateuPM);
  tbEmCache = 0; pmEmCache = 1; pmSemPlano = 0; url[0] = 0;
  assert(debrid_resolver_escolhido("DEADBEEF", -1, url, sizeof url, serv, sizeof serv, &pct) == 1);
  assert(strstr(url, "a.pm.me") && bateuTB[1] == 0);
  OK("escolha manual: em cache noutro servico ganha de mandar o TorBox baixar");

  // Conta sem plano continua fora tambem na escolha manual
  debrid_esquecer();
  debrid_definir_chave("torbox", K_TB);
  tbCriar403 = 1; tbEmCache = 1;
  assert(!debrid_resolver("DEADBEEF", -1, url, sizeof url));
  tbEmCache = 0;
  memset(bateuTB, 0, sizeof bateuTB);
  assert(debrid_resolver_escolhido("DEADBEEF", -1, url, sizeof url, serv, sizeof serv, &pct) == 0);
  assert(bateuTB[0] == 0 && bateuTB[1] == 0);
  OK("escolha manual respeita conta sem plano");
  tbCriar403 = 0; tbEmCache = 1; pmEmCache = 1;
  debrid_esquecer();
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
  tbCriar403 = 3; pmEmCache = 1;
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

// CONTA SEM PLANO, registros 1731-1774: TorBox 403 PLAN_RESTRICTED_FEATURE e
// Premiumize "Account not premium.". Diferente do 403 de cima, vale a SESSAO:
// uma busca nova nao volta a perguntar, e com os dois assim o debrid deixa de
// contar como ativo (streams.c descarta os torrents sem url).
static void semPlano(void) {
  char url[4096] = "", rec[64] = "";
  int i;
  debrid_esquecer();
  debrid_definir_chave("torbox", K_TB);
  debrid_definir_chave("premiumize", K_PM);
  debrid_nova_busca();
  memset(bateuTB, 0, sizeof bateuTB); memset(bateuPM, 0, sizeof bateuPM);
  tbCriar403 = 1; pmEmCache = 1; pmSemPlano = 0;
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));   // PM ainda serve
  assert(bateuTB[1] == 1);
  assert(debrid_sem_plano() == 2);                  // bit do TorBox
  assert(debrid_ativo());
  // busca nova NAO devolve o TorBox
  for (i = 0; i < 3; i++) { debrid_nova_busca(); url[0] = 0;
    assert(debrid_resolver("DEADBEEF", -1, url, sizeof url)); }
  assert(bateuTB[0] == 1 && bateuTB[1] == 1);
  assert(debrid_recusa(rec, sizeof rec) && !strcmp(rec, "TorBox free-plan"));
  OK("TorBox PLAN_RESTRICTED_FEATURE: fora pela sessao, nao so pela busca");

  // aviso: uma vez so
  assert(debrid_sem_plano_novo() == 2);
  assert(debrid_sem_plano_novo() == 0);
  assert(strstr(debrid_sem_plano_frase(2), "TorBox é gratuita"));
  OK("aviso de conta sem plano sai uma vez por sessao");

  // Premiumize gratuito: o directdl de 200 "Account not premium." e de CONTA
  pmSemPlano = 1;
  debrid_nova_busca();
  memset(bateuPM, 0, sizeof bateuPM);
  for (i = 0; i < 5; i++) { url[0] = 0; assert(!debrid_resolver("DEADBEEF", -1, url, sizeof url)); }
  assert(bateuPM[1] == 1);                          // uma vez, nao cinco
  assert(debrid_sem_plano() == (2 | 4));
  assert(!debrid_ativo());
  assert(strstr(debrid_sem_plano_frase(4), "Premiumize é gratuita"));
  assert(strstr(debrid_sem_plano_frase(6), "contas de debrid"));
  assert(debrid_sem_plano_novo() == 4);
  OK("Premiumize 'Account not premium.': fora pela sessao; sem servico, debrid inativo");

  // chave NOVA do servico volta a valer (a pessoa assinou)
  debrid_definir_chave("premiumize", "outra-chave-pm");
  assert(!(debrid_sem_plano() & 4));
  assert(debrid_ativo());
  pmSemPlano = 0; tbCriar403 = 0;
  debrid_esquecer();
  assert(!debrid_sem_plano());
  OK("chave nova ou logout limpam a conta sem plano");
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


static void adZera(int modo, int a, int b, int c) {
  memset(bateuAD, 0, sizeof bateuAD);
  memset(adSeqStatus, 0, sizeof adSeqStatus);
  adUploadModo = modo; adIdxStatus = 0; adNSeq = 0; adNOrdem = 0;
  if (a >= 0) adSeqStatus[adNSeq++] = a;
  if (b >= 0) adSeqStatus[adNSeq++] = b;
  if (c >= 0) adSeqStatus[adNSeq++] = c;
}

static void alldebrid(void) {
  char url[4096] = "", serv[32]; int pct = 0, r;
  debrid_esquecer();
  debrid_definir_chave("alldebrid", K_AD);
  assert(debrid_ativo() && debrid_origem("alldebrid") == 1);

  // em cache: upload -> status -> files -> unlock, nessa ordem, episodio S02E05
  adZera(0, 4, -1, -1);
  debrid_definir_episodio(2, 5);
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!strcmp(url, "https://s1.debrid.it/dl/UNL5/Show.S02E05.mkv"));
  assert(adNOrdem == 4 && adOrdem[0] == 0 && adOrdem[1] == 1 && adOrdem[2] == 2 && adOrdem[3] == 3);
  assert(!strcmp(adAuthVisto, "Authorization: Bearer " K_AD));     // chave no cabecalho
  // o link do arquivo (\/ do JSON ja desfeito) vai urlencoded ao unlock
  assert(strstr(adCorpoUnlock, "link=https%3A%2F%2Falldebrid.com%2Ff%2FBBB"));
  OK("AllDebrid: upload -> status -> files (arvore) -> unlock, S02E05, chave so no cabecalho");

  // filme: maior video, sample.txt fora
  adZera(0, 4, -1, -1); url[0] = 0; debrid_definir_episodio(0, 0);
  assert(debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(strstr(url, "UNL4"));
  OK("AllDebrid: filme escolhe o maior video");

  // fileIdx do addon (indice 1 na lista plana = E05) vence o maior
  adZera(0, 4, -1, -1); url[0] = 0;
  assert(debrid_resolver("deadbeef", 1, url, sizeof url));
  assert(strstr(url, "UNL5"));
  OK("AllDebrid: fileIdx escolhe o arquivo");

  // status em processamento e depois pronto (ready no upload, status 2 -> 4):
  // uma espera de 1 s e segue
  adZera(0, 2, 4, -1); url[0] = 0; debrid_definir_episodio(2, 5);
  assert(debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(bateuAD[1] == 2 && strstr(url, "UNL5"));
  OK("AllDebrid: polling do status ate statusCode 4");

  // AUTOMATICO: fora de cache nao espera, apaga o que acabou de criar (fila,
  // 0 bytes) e marca fora de cache
  adZera(1, 0, -1, -1); url[0] = 0;
  debrid_nova_busca();
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(bateuAD[0] == 1 && bateuAD[1] == 1 && bateuAD[4] == 1 && bateuAD[2] == 0 && bateuAD[3] == 0);
  assert(debrid_fora_de_cache() == 1);
  OK("AllDebrid: fora de cache no automatico -> FORA, magnet novo descartado, contado");

  // ...mas um magnet que ja andava (370 bytes) NAO e apagado
  adZera(1, 1, -1, -1); url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(bateuAD[4] == 0);
  OK("AllDebrid: magnet que ja estava baixando nao e apagado");

  // 'ready' mas o status nunca chega a 4 no prazo: automatico nao toca
  adZera(0, 1, 1, 1); url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(bateuAD[1] == 3 && bateuAD[2] == 0);
  OK("AllDebrid: timeout do polling no automatico -> nao toca");

  // ESCOLHA MANUAL fora de cache: deixa baixar e diz o progresso
  adZera(1, 1, 1, 1); url[0] = 0; serv[0] = 0;
  r = debrid_resolver_escolhido("deadbeef", -1, url, sizeof url, serv, sizeof serv, &pct);
  assert(r == DEBRID_BAIXANDO && !strcmp(serv, "AllDebrid") && pct == 37);
  assert(bateuAD[4] == 0);                        // nada apagado: a pessoa escolheu
  OK("AllDebrid: escolha manual fora de cache -> baixando 37%, fica na conta");

  // manual e depois pronto
  adZera(1, 1, 4, -1); url[0] = 0; serv[0] = 0; debrid_definir_episodio(2, 5);
  r = debrid_resolver_escolhido("deadbeef", -1, url, sizeof url, serv, sizeof serv, &pct);
  assert(r == 1 && strstr(url, "UNL5"));
  OK("AllDebrid: escolha manual que termina durante a espera toca");

  // statusCode de erro do servico (7 = 20 min sem baixar)
  adZera(0, 7, -1, -1); url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(bateuAD[2] == 0 && debrid_ativo());     // erro do torrent, nao da conta
  OK("AllDebrid: statusCode de erro -> nao toca, servico segue valendo");

  // magnet invalido (erro por magnet): do torrent, nao da conta
  adZera(5, -1, -1, -1); url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(debrid_ativo() && !debrid_sem_plano());
  {char rec[64]; assert(!debrid_recusa(rec, sizeof rec));}
  OK("AllDebrid: MAGNET_INVALID_URI e do torrent");

  // limite de magnets ativos: recusa da conta NESTA busca ("AllDebrid 429")
  adZera(4, -1, -1, -1); url[0] = 0;
  debrid_nova_busca();
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  { char rec[64]; assert(debrid_recusa(rec, sizeof rec) && !strcmp(rec, "AllDebrid 429")); }
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(bateuAD[0] == 1);                        // a segunda nem tenta
  debrid_nova_busca();
  OK("AllDebrid: MAGNET_TOO_MANY_ACTIVE recusa a conta so nesta busca");

  // nao premium: fora pela sessao, com a frase
  adZera(3, -1, -1, -1); url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(debrid_sem_plano() == 8 && !debrid_ativo());
  assert(strstr(debrid_sem_plano_frase(8), "AllDebrid não é premium"));
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url) && bateuAD[0] == 1);
  OK("AllDebrid: MAGNET_MUST_BE_PREMIUM -> conta sem plano, fora pela sessao");

  // chave invalida: sessao inteira, frase propria, log sem a chave que o corpo ecoou
  debrid_definir_chave("alldebrid", K_AD "2");
  assert(!debrid_sem_plano());
  debrid_nova_busca();
  adZera(2, -1, -1, -1); url[0] = 0;
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(debrid_sem_plano() == 8 && !debrid_ativo());
  assert(strstr(debrid_sem_plano_frase(8), "chave do AllDebrid foi recusada"));
  { char rec[64]; assert(debrid_recusa(rec, sizeof rec)); }
  OK("AllDebrid: AUTH_BAD_APIKEY -> chave recusada, fora pela sessao");
}

static void alldebridLocal(void) {
  char url[4096] = "", m[32];
  debrid_esquecer();
  debrid_definir_chave("alldebrid", K_AD);
  assert(debrid_origem("alldebrid") == 1);
  debrid_chave_mascarada("alldebrid", m, sizeof m);
  assert(!strncmp(m, "····", 4));
  assert(!strstr(m, "CHAVE") && strlen(m) < 16);

  // a local vence a da conta
  debrid_definir_chave_local("alldebrid", K_AD_LOCAL);
  assert(debrid_origem("alldebrid") == 2);
  adZera(0, 4, -1, -1); debrid_definir_episodio(0, 0);
  assert(debrid_resolver("deadbeef", -1, url, sizeof url));
  assert(!strcmp(adAuthVisto, "Authorization: Bearer " K_AD_LOCAL));
  OK("chave local vence a da conta");

  // logout tira a da conta e MANTEM a local
  debrid_esquecer();
  assert(debrid_origem("alldebrid") == 2 && debrid_ativo());
  OK("logout mantem a chave local");

  // apagar a local sem conta = sem chave; com conta volta a conta
  debrid_definir_chave_local("alldebrid", "");
  assert(debrid_origem("alldebrid") == 0 && !debrid_ativo());
  debrid_definir_chave("alldebrid", K_AD);
  debrid_definir_chave_local("alldebrid", K_AD_LOCAL);
  debrid_definir_chave_local("alldebrid", "");
  assert(debrid_origem("alldebrid") == 1);
  OK("apagar a local devolve a da conta");

  // outros servicos aceitam chave local pelo mesmo caminho
  debrid_definir_chave_local("torbox", K_TB);
  assert(debrid_origem("torbox") == 2 && debrid_ativo());
  debrid_definir_chave_local("torbox", "");
  debrid_esquecer();
  debrid_definir_chave_local("alldebrid", "");
  assert(!debrid_ativo());
  OK("chave local dos outros servicos");
}

static void alldebridTeste(void) {
  char msg[64], data[16];
  debrid_esquecer();
  assert(!debrid_testar_alldebrid(msg, sizeof msg, data, sizeof data) && !strcmp(msg, "sem chave"));
  debrid_definir_chave_local("alldebrid", K_AD_LOCAL);
  adZera(0, 4, -1, -1); adUserPremium = 1; adUserRuim = 0;
  assert(debrid_testar_alldebrid(msg, sizeof msg, data, sizeof data));
  assert(!strcmp(msg, "premium") && data[2] == '/' && data[5] == '/' && strlen(data) == 10);
  assert(!strstr(msg, "fulano") && !strstr(data, "fulano"));
  assert(strstr(adUrlVista, "/v4/user") && !strcmp(adAuthVisto, "Authorization: Bearer " K_AD_LOCAL));
  adUserPremium = 0;
  assert(!debrid_testar_alldebrid(msg, sizeof msg, data, sizeof data) && !strcmp(msg, "conta sem premium"));
  adUserPremium = 1; adUserRuim = 1;
  assert(!debrid_testar_alldebrid(msg, sizeof msg, data, sizeof data) && !strcmp(msg, "chave recusada"));
  adUserRuim = 0;
  debrid_definir_chave_local("alldebrid", "");
  debrid_esquecer();
  OK("Testar chave: premium ate a data, sem premium, chave recusada; sem usuario/e-mail");
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

  // Marcas de "fora de cache" que os addons mandam de verdade (registros
  // 1136/2191/2501: "⏳ UHD" do AIOStreams; "[TB download]" do Torrentio).
  n = stream_extrair(
    "{\"streams\":[{\"name\":\"\xe2\x8f\xb3 FHD\",\"url\":\"https://aio/p/1\"},"
    "{\"name\":\"\xe2\x9a\xa1 FHD\",\"url\":\"https://aio/p/2\"},"
    "{\"name\":\"[TB download] Torrentio\\n1080p\",\"url\":\"https://t/r/3\"},"
    "{\"name\":\"[TB+] Torrentio\\n1080p WEB-DL\",\"url\":\"https://t/r/4\"},"
    "{\"name\":\"Comet\",\"description\":\"Uncached\",\"url\":\"https://c/5\"}]}",
    "AIOStreams", &v);
  assert(n == 5);
  assert(v[0].foraCache && !v[1].foraCache && v[2].foraCache && !v[3].foraCache && v[4].foraCache);
  free(v);
  OK("parser: marca fora de cache (⏳, [TB download], uncached) e nao a cacheada (⚡, [TB+])");
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
  torboxP2P();
  semPlano();
  premiumize();
  alldebrid();
  alldebridLocal();
  alldebridTeste();
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
  assert(!strstr(capt, "CHAVE-AD"));               // AllDebrid: conta, "2" e local
  assert(!strstr(capt, "fulano-segredo"));         // nem o usuario da conta
  assert(!strstr(capt, "token="));
  assert(strstr(urlRequestdl, "token=" K_TB));   // o teste de fato passou por la
  // nem o caminho das URLs resolvidas dos tres servicos
  assert(!strstr(capt, "/d/QQ"));
  assert(!strstr(capt, "/dl/ZZZ"));
  assert(!strstr(capt, "/dl/BBB"));
  // mas o host fica, que e o que diagnostica qual servico respondeu
  assert(strstr(capt, "store-1.torbox.app"));
  // AllDebrid: o caminho do link desbloqueado fica de fora, o host fica; e o
  // corpo de erro que ecoou a chave foi cortado
  assert(!strstr(capt, "/dl/UNL"));
  assert(strstr(capt, "s1.debrid.it"));
  assert(strstr(capt, "[debrid] AllDebrid upload: HTTP 200 sem o esperado; error=AUTH_BAD_APIKEY"));
  assert(strstr(capt, "[debrid] AllDebrid: chave recusada; fora pelo resto da sessao"));
  assert(strstr(capt, "[debrid] AllDebrid: recusa da conta (HTTP 401)"));
  assert(strstr(capt, "[debrid] AllDebrid: conta sem plano para a API"));
  assert(strstr(capt, "[debrid] chave local do AllDebrid definida"));
  assert(strstr(capt, "a.pm.me"));
  OK("nenhuma chave e nenhum caminho de link sai no stdout do app");

  // O CORPO DO 403 ESTA NO LOG, com a linha no formato que se procura no D1,
  // e a chave que ele trazia virou ***.
  assert(strstr(capt, "[debrid] TorBox createtorrent: HTTP 403 ("));
  assert(strstr(capt, "PLAN_RESTRICTED_FEATURE"));
  assert(strstr(capt, "key=***"));
  assert(strstr(capt, "Torrent is not cached."));
  assert(strstr(capt, "[debrid] TorBox: recusa da conta (HTTP 403)"));
  assert(strstr(capt, "[debrid] TorBox: conta sem plano para a API; fora pelo resto da sessao"));
  assert(strstr(capt, "[debrid] Premiumize: conta sem plano para a API; fora pelo resto da sessao"));
  // a linha que se procura no D1 quando alguem diz "o P2P nao toca"
  assert(strstr(capt, "[debrid] TorBox: deadbeef baixando no TorBox (37%, estado=downloading)"));
  OK("corpo do 403 no log, sem a chave");

  puts("debrid: tudo ok");
  return 0;
}
