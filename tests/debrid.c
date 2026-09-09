// Resolvedor Real-Debrid contra uma rede FALSA que devolve as respostas reais
// da API, e o parser aceitando torrent sem url. Sem SDL, sem rede.
#include "../src/debrid.h"
#include "../src/streams.h"
#include "../src/rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char ultimoCorpo[800], ultimaRota[200], ultimoCt[100];
static int  pediuUnrestrict; static char corpoSelect[100];
char *rede_postar_st(const char *url, int s, const char *const *cab, const char *corpo, int *st) {
  (void)s; snprintf(ultimaRota, sizeof ultimaRota, "%s", url);
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo);
  ultimoCt[0] = 0;
  for (int k = 0; cab && cab[k]; k++) if (!strncmp(cab[k], "Content-Type:", 13)) snprintf(ultimoCt, sizeof ultimoCt, "%s", cab[k]);
  if (strstr(url, "addMagnet"))   { *st = 201; return strdup("{\"id\":\"ABC123\",\"uri\":\"x\"}"); }
  if (strstr(url, "selectFiles")) { *st = 204; snprintf(corpoSelect, sizeof corpoSelect, "%s", corpo); return strdup(""); }
  if (strstr(url, "unrestrict"))  { *st = 200; pediuUnrestrict = 1;
    return strdup("{\"download\":\"https://x.download.real-debrid.com/d/QQ/Show.S02E05.mkv\",\"filename\":\"Show.S02E05.mkv\"}"); }
  *st = 404; return strdup("{}");
}
char *rede_baixar_st(const char *url, int s, const char *const *cab, int *st) {
  (void)s; (void)cab; *st = 200;
  assert(strstr(url, "torrents/info/ABC123"));
  return strdup("{\"id\":\"ABC123\",\"status\":\"downloaded\",\"files\":["
    "{\"id\":1,\"path\":\"/Show.S02E04.1080p.mkv\",\"bytes\":2000000000,\"selected\":0},"
    "{\"id\":2,\"path\":\"/Show.S02E05.1080p.mkv\",\"bytes\":1900000000,\"selected\":0},"
    "{\"id\":3,\"path\":\"/sample.txt\",\"bytes\":10,\"selected\":0}],"
    "\"links\":[\"https://real-debrid.com/d/LINK1\"]}");
}
// o parser puxa badges; dublê
unsigned long long badges_detectar(const char *t) { (void)t; return 0; }

int main(void) {
  char url[4096] = "";
  // sem chave: nada acontece
  assert(!debrid_ativo());
  assert(!debrid_resolver("deadbeef", -1, url, sizeof url));
  debrid_definir_chave("torbox", "k");   // sem resolvedor: ignorado
  assert(!debrid_ativo());
  debrid_definir_chave("realdebrid", "CHAVE");
  assert(debrid_ativo());

  // episodio 2x05: escolhe o arquivo S02E05, nao o maior (S02E04)
  debrid_definir_episodio(2, 5);
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!strcmp(url, "https://x.download.real-debrid.com/d/QQ/Show.S02E05.mkv"));
  assert(strstr(ultimoCorpo, "link=https%3A%2F%2Freal-debrid.com%2Fd%2FLINK1"));
  assert(strstr(ultimoCt, "x-www-form-urlencoded"));
  assert(!strcmp(corpoSelect, "files=2"));
  puts("ok  resolve 2x05 pelo nome do arquivo, form-urlencoded");

  // A URL QUE ACABOU DE SER RESOLVIDA E CREDENCIAL: quem tem o "/d/QQ/" baixa
  // na conta de quem pediu, e debrid.c a IMPRIME no log — que e lido, copiado
  // e colado em relato de defeito. Por isso este teste linka o
  // rede_url_publica DE VERDADE (src/redeurl.c) em vez de um stub: com um
  // stub, uma regressao que passasse a imprimir o link inteiro sairia verde.
  { char seg[120];
    const char *pub = rede_url_publica(url, seg, sizeof seg);
    assert(!strstr(pub, "/d/QQ"));            // a chave nao sai
    assert(!strstr(pub, "Show.S02E05.mkv"));  // nem o resto do caminho
    assert(strstr(pub, "x.download.real-debrid.com"));  // o host fica: e o que diagnostica
    assert(strstr(pub, "/..."));              // e diz que havia caminho
    puts("ok  o link do Real-Debrid nao vai inteiro para o log"); }

  // filme: maior video; sample.txt fora
  debrid_definir_episodio(0, 0);
  // (a rede falsa devolve o mesmo download; o que se testa e o selectFiles)
  assert(debrid_resolver("DEADBEEF", -1, url, sizeof url));
  assert(!strcmp(corpoSelect, "files=1"));
  puts("ok  filme escolhe o maior video");

  // parser: torrent sem url entra com infoHash; externalUrl http vale como url;
  // magnet em url NAO vira url
  { Stream *v = NULL; int n = stream_extrair(
      "{\"streams\":[{\"name\":\"Torrentio\",\"title\":\"Show S02E05 1080p\",\"infoHash\":\"abcdef0123\",\"fileIdx\":1},"
      "{\"name\":\"X\",\"externalUrl\":\"https://cdn/x.mp4\"},"
      "{\"name\":\"Y\",\"url\":\"magnet:?xt=urn:btih:ff\"},"
      "{\"name\":\"Z\",\"title\":\"cached\",\"clientResolve\":{\"type\":\"debrid\",\"service\":\"realdebrid\",\"infoHash\":\"0011\",\"isCached\":true}}]}",
      "Torrentio", &v);
    assert(n == 3);
    assert(!v[0].url[0] && !strcmp(v[0].infoHash, "abcdef0123") && v[0].fileIdx == 1);
    assert(!strcmp(v[1].url, "https://cdn/x.mp4") && !v[1].infoHash[0]);
    assert(!v[2].url[0] && !strcmp(v[2].infoHash, "0011"));
    free(v); }
  puts("ok  parser: infoHash no topo e em clientResolve, externalUrl, magnet fora");

  debrid_esquecer();
  assert(!debrid_ativo());
  puts("debrid: tudo ok");
  return 0;
}
