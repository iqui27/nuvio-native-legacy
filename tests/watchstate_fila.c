// watch_state v2, passo 2: corpo, id estavel, URL, fila persistente e retry.
// Spec: .mercado/aiostreams-watch-state.md. HTTP falso, sem rede e sem relogio.
//
//   bash tests/watchstate_fila.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watchstate.h"

static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)
#define STR(a, b) do { if (strcmp((a), (b))) { printf("FALHOU %d: textos diferentes\n", __LINE__); falhas++; } } while (0)

static WsEvento serie(int ev) {
  WsEvento e;
  memset(&e, 0, sizeof e);
  e.ev = ev; strcpy(e.tipo, "series");
  strcpy(e.metaId, "tt0903747"); strcpy(e.videoId, "tt0903747:3:7");
  e.atMs = 1757441718000LL; e.iniMs = 1757438900000LL; e.posMs = 2680000; e.durMs = 2820000;
  e.played = 1; e.season = 3; e.episode = 7;
  strcpy(e.imdb, "tt0903747"); strcpy(e.tmdb, "1396");
  return e;
}

// HTTP falso: devolve os status roteirizados e guarda o que recebeu.
static struct { int st[16], ra[16], n, i; char url[16][200]; } rede;
static int envia(const char *addon, const char *caminho, const char *corpo, int *ra, void *u) {
  (void)corpo; (void)u;
  if (rede.i < 16) snprintf(rede.url[rede.i], sizeof rede.url[0], "https://%s%s", addon, caminho);
  *ra = rede.i < rede.n ? rede.ra[rede.i] : 0;
  return rede.i < rede.n ? rede.st[rede.i++] : (rede.i++, 204);
}

int main(void) {
  char b[1024], id[160], url[300];
  WsEvento e = serie(WS_STOP);

  // corpo identico ao exemplo da spec (so os ids que conhecemos)
  OK(ws_corpo(&e, b, sizeof b));
  STR(b, "{\"id\":\"e|tt0903747:3:7|stop|2680000|1757438900000\",\"event\":\"stop\","
         "\"scope\":\"episode\",\"at\":1757441718,\"metaId\":\"tt0903747\","
         "\"videoId\":\"tt0903747:3:7\",\"positionMs\":2680000,\"durationMs\":2820000,"
         "\"played\":true,\"season\":3,\"episode\":7,"
         "\"ids\":{\"imdb\":\"tt0903747\",\"tmdb\":\"1396\"}}");
  OK(ws_id(&e, id, sizeof id));
  STR(id, "e|tt0903747:3:7|stop|2680000|1757438900000");

  // filme: sem season/episode, duracao desconhecida nao sai (nunca 0), sem ids
  e = serie(WS_PAUSE);
  strcpy(e.tipo, "movie"); strcpy(e.metaId, "tt0133093"); strcpy(e.videoId, "tt0133093");
  e.season = e.episode = -1; e.durMs = 0; e.played = 0; e.imdb[0] = e.tmdb[0] = 0;
  OK(ws_corpo(&e, b, sizeof b));
  STR(b, "{\"id\":\"e|tt0133093|pause|2680000|1757438900000\",\"event\":\"pause\","
         "\"scope\":\"movie\",\"at\":1757441718,\"metaId\":\"tt0133093\","
         "\"videoId\":\"tt0133093\",\"positionMs\":2680000,\"played\":false}");

  // played/unplayed: id termina no instante; posicao desconhecida nao sai
  e = serie(WS_UNPLAYED); e.posMs = -1; e.durMs = 0; e.played = 0;
  OK(ws_id(&e, id, sizeof id));
  STR(id, "e|tt0903747:3:7|unplayed|1757441718000");
  OK(ws_corpo(&e, b, sizeof b));
  OK(!strstr(b, "positionMs") && !strstr(b, "durationMs") && strstr(b, "\"played\":false"));

  // id estavel entre tentativas; muda com a posicao e com o inicio (reassistir)
  e = serie(WS_STOP);
  { char a[160], c[160], d[160];
    ws_id(&e, a, sizeof a); ws_id(&e, c, sizeof c); STR(a, c);
    e.posMs += 1; ws_id(&e, d, sizeof d); OK(strcmp(a, d));
    e.posMs -= 1; e.iniMs += 1000; ws_id(&e, d, sizeof d); OK(strcmp(a, d)); }

  // identidade ambigua ou texto sujo: nao ha evento
  e = serie(WS_STOP); e.season = -1;            OK(!ws_corpo(&e, b, sizeof b));
  e = serie(WS_STOP); e.videoId[0] = 0;         OK(!ws_id(&e, id, sizeof id));
  e = serie(WS_STOP); e.tipo[0] = 'x';          OK(!ws_id(&e, id, sizeof id));
  e = serie(WS_STOP); e.posMs = -1;             OK(!ws_id(&e, id, sizeof id));
  e = serie(WS_STOP); e.ev = WS_STOP | WS_PAUSE; OK(!ws_id(&e, id, sizeof id));
  e = serie(WS_STOP); strcpy(e.videoId, "tt1:1\t1"); OK(!ws_id(&e, id, sizeof id));
  e = serie(WS_STOP); OK(!ws_corpo(&e, b, 40));   // nao coube

  // URL: base sem /manifest.json, id do video em percent-encoding
  e = serie(WS_STOP);
  OK(ws_url("https://a.example/cfg/", &e, url, sizeof url));
  STR(url, "https://a.example/cfg/watch_state/push/series/tt0903747:3:7.json");
  strcpy(e.videoId, "kitsu:42/7 x");
  OK(ws_url("http://h:7000", &e, url, sizeof url));
  STR(url, "http://h:7000/watch_state/push/series/kitsu:42%2F7%20x.json");
  OK(!ws_url("ftp://a", &e, url, sizeof url));
  OK(!ws_url("https://a", &e, url, 20));

  // espera: 30 s, 2 min, 10 min, 1 h, 6 h, e 6 h dai em diante
  OK(ws_espera_s(1) == 30 && ws_espera_s(2) == 120 && ws_espera_s(3) == 600 &&
     ws_espera_s(4) == 3600 && ws_espera_s(5) == 21600 && ws_espera_s(9) == 21600);

  // Identidade inclui configuracao, conta e perfil, mesmo para o mesmo addon.
  { char a[20], b2[20];
    ws_destino("https://a?apikey=fixture", "conta-a", 1, a);
    ws_destino("https://a?apikey=fixture", "conta-a", 1, b2); STR(a, b2);
    ws_destino("https://a?apikey=outra", "conta-a", 1, b2); OK(strcmp(a, b2));
    ws_destino("https://a?apikey=fixture", "conta-b", 1, b2); OK(strcmp(a, b2));
    ws_destino("https://a?apikey=fixture", "conta-a", 2, b2); OK(strcmp(a, b2));
  }

  // ---- fila ----
  { WsFila *f = calloc(1, sizeof *f);
    const long long t0 = 1000000;
    memset(&rede, 0, sizeof rede);
    OK(ws_fila_empurrar(f, "a", "/x1", "id1", "{\"a\":1}", t0) == 1);
    OK(ws_fila_empurrar(f, "a", "/x1", "id1", "{\"a\":1}", t0) == 0);  // mesmo id
    OK(ws_fila_empurrar(f, "b", "/x1", "id1", "{\"a\":1}", t0) == 1);  // outro addon
    OK(!ws_fila_empurrar(f, "a", "/x", "id\t2", "{}", t0));            // TAB recusado
    OK(!ws_fila_empurrar(f, "a", "https://a/x", "id3", "{}", t0));     // sem caminho relativo
    OK(!ws_fila_empurrar(f, "a", "/x?apikey=fixture", "id3", "{}", t0));
    OK(!ws_fila_empurrar(f, "https://a", "/x", "id3", "{}", t0));
    OK(f->n == 2);

    // 204 entrega os dois e esvazia
    rede.n = 2; rede.st[0] = 204; rede.st[1] = 200;
    OK(ws_fila_passo(f, t0, envia, NULL) == 2);
    OK(f->n == 0 && ws_fila_proxima(f) == -1);

    // 503: tenta de novo so apos 30 s; 429 com Retry-After maior vale o maior
    memset(&rede, 0, sizeof rede);
    ws_fila_empurrar(f, "a", "/x1", "id1", "{}", t0);
    rede.n = 1; rede.st[0] = 503;
    OK(ws_fila_passo(f, t0, envia, NULL) == 1);
    OK(f->n == 1 && f->it[0].tentativas == 1 && ws_fila_proxima(f) == t0 + 30);
    OK(ws_fila_passo(f, t0 + 29, envia, NULL) == 0);                       // ainda cedo
    rede.n = 2; rede.st[1] = 429; rede.ra[1] = 500;
    OK(ws_fila_passo(f, t0 + 30, envia, NULL) == 1);
    OK(f->it[0].tentativas == 2 && ws_fila_proxima(f) == t0 + 30 + 500);   // Retry-After > 120
    rede.n = 3; rede.st[2] = 0;                                            // falha de rede
    OK(ws_fila_passo(f, t0 + 530, envia, NULL) == 1);
    OK(f->it[0].tentativas == 3 && ws_fila_proxima(f) == t0 + 530 + 600);
    OK(ws_fila_passo(f, t0 + 530 + 600, envia, NULL) == 1);                // 204 (alem do roteiro)
    OK(f->n == 0);

    // 401 segura o addon 6 h e o item de tras do MESMO addon espera; outro addon segue
    memset(&rede, 0, sizeof rede);
    ws_fila_empurrar(f, "a", "/1", "i1", "{}", t0);
    ws_fila_empurrar(f, "a", "/2", "i2", "{}", t0);
    ws_fila_empurrar(f, "b", "/1", "i1", "{}", t0);
    rede.n = 1; rede.st[0] = 401;
    OK(ws_fila_passo(f, t0, envia, NULL) == 2);            // a/1 (401) e b/1 (204)
    OK(f->n == 2 && f->it[0].tentativas == 0 && f->it[0].proxS == t0 + WS_SEGURA_S);
    STR(rede.url[1], "https://b/1");
    OK(ws_fila_proxima(f) == t0 + WS_SEGURA_S);  // a/2 vencido espera a/1
    ws_fila_empurrar(f, "b", "/2", "i2", "{}", t0 + 3600);
    OK(ws_fila_proxima(f) == t0 + 3600);         // outra cabeca vence antes
    OK(ws_fila_passo(f, t0 + 3599, envia, NULL) == 0);
    OK(ws_fila_passo(f, t0 + 3600, envia, NULL) == 1);
    OK(ws_fila_proxima(f) == t0 + WS_SEGURA_S);
    OK(ws_fila_passo(f, t0 + 3600, envia, NULL) == 0);     // continua segurado
    rede.i = 0; rede.n = 2; rede.st[0] = 403; rede.st[1] = 204;
    OK(ws_fila_passo(f, t0 + WS_SEGURA_S, envia, NULL) == 1);   // 403: segura de novo
    OK(f->n == 2 && f->it[0].proxS == t0 + 2 * WS_SEGURA_S);
    ws_fila_limpar(f);

    // WS_AUSENTE: destino fora da lista de agora. Segura sem contar tentativa
    // e sem fazer pedido — no arranque a lista e a de exemplo do pacote e o
    // pendente da conta tem que sobreviver a ela.
    memset(&rede, 0, sizeof rede);
    ws_fila_empurrar(f, "a", "/1", "i1", "{}", t0);
    rede.n = 1; rede.st[0] = WS_AUSENTE;
    OK(ws_fila_passo(f, t0, envia, NULL) == 0);
    OK(f->n == 1 && f->it[0].tentativas == 0 && ws_fila_proxima(f) == t0 + WS_AUSENTE_S);
    OK(ws_fila_passo(f, t0 + WS_AUSENTE_S - 1, envia, NULL) == 0);   // ainda segurado
    rede.i = 0; rede.n = 1; rede.st[0] = 204;
    OK(ws_fila_passo(f, t0 + WS_AUSENTE_S, envia, NULL) == 1 && f->n == 0);

    // outro 4xx descarta so aquele evento e o seguinte segue na hora
    memset(&rede, 0, sizeof rede);
    ws_fila_empurrar(f, "a", "/1", "i1", "{}", t0);
    ws_fila_empurrar(f, "a", "/2", "i2", "{}", t0);
    rede.n = 2; rede.st[0] = 404; rede.st[1] = 204;
    OK(ws_fila_passo(f, t0, envia, NULL) == 2 && f->n == 0);
    STR(rede.url[1], "https://a/2");

    // vence em 1 dia: nem tenta
    memset(&rede, 0, sizeof rede);
    ws_fila_empurrar(f, "a", "/1", "i1", "{}", t0);
    OK(ws_fila_passo(f, t0 + WS_VIDA_S, envia, NULL) == 0 && f->n == 0);

    // fila cheia: o mais antigo sai
    { int k; char u[40], i2[16];
      for (k = 0; k < WS_FILA_MAX + 3; k++) {
        snprintf(u, sizeof u, "/%d", k); snprintf(i2, sizeof i2, "i%d", k);
        ws_fila_empurrar(f, "a", u, i2, "{}", t0);
      }
      OK(f->n == WS_FILA_MAX); STR(f->it[0].id, "i3"); }
    ws_fila_limpar(f);

    // diario: ida e volta, preserva tentativas e proxima tentativa; lixo ignorado
    ws_fila_empurrar(f, "a", "/1", "i1", "{\"k\":\"v w\"}", t0);
    ws_fila_empurrar(f, "b", "/1", "i2", "{}", t0 + 5);
    f->it[0].tentativas = 3; f->it[0].proxS = t0 + 600;
    { char *s = ws_fila_serializar(f), *lixo;
      WsFila *g = calloc(1, sizeof *g);
      OK(s != NULL);
      lixo = malloc(strlen(s) + 64);
      sprintf(lixo, "NVWATCHSTATE 2\nlixo\n\t\t\n%s1\t2\tx\ta\tb\tc\td\n", s + 15);
      ws_fila_carregar(g, "100\t100\t1\thttps://a\thttps://a/x\tid\t{}\n");
      OK(g->n == 0); // diario antigo com URL e sem contexto nao e reenviado
      ws_fila_carregar(g, lixo);
      ws_fila_carregar(g, s);                              // repetido: nao duplica
      OK(g->n == 2);
      STR(g->it[0].corpo, "{\"k\":\"v w\"}"); STR(g->it[1].caminho, "/1");
      STR(g->it[1].addon, "b");
      OK(g->it[0].tentativas == 3 && g->it[0].proxS == t0 + 600 && g->it[0].criadoS == t0);
      OK(g->it[1].criadoS == t0 + 5);
      free(lixo); free(s); ws_fila_limpar(g); free(g); }
    ws_fila_limpar(f);
    free(f); }

  if (falhas) { printf("watchstate_fila: %d falha(s)\n", falhas); return 1; }
  printf("watchstate_fila: ok\n");
  return 0;
}
