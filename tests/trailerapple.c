// Resolve o trailer pelo caminho REAL do src (busca UTS, escolha por
// titulo/ano, hlsUrl, gravacao do master e reduzido de uma variante), mas com
// a rede TROCADA por um duble de link: respostas sinteticas fixas, nenhum
// byte sai do processo. Qualquer URL fora das tres esperadas vira falha — e
// o que separa "teste de transporte" de "a fixture falou certo".
#include "../src/trailerapple.h"
#include "nvtmp.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// Pasta temporaria de verdade: o reduzido de uma variante e um arquivo, e
// sem ele trailerapple_url devolve NULL de proposito.
#include <stdlib.h>
char *dados_caminho(char *dst, unsigned tam, const char *nome) { snprintf(dst, tam, "%s/nuvio-trailerapple-teste/%s", nvt_dir(), nome); return dst; }
void dados_marcar_sujo(int leve) { (void)leve; }
const char *dados_dir(void) { return nvt("/nuvio-trailerapple-teste"); }
int qualidade = 0;
int ajustes_trailer_qualidade(void) { return qualidade; }

// --- TRANSPORTE DUBLE ------------------------------------------------------
// As respostas sao no formato medido da UTS (shelves/items/type/title/
// releaseDate epoch-ms e data.content.backgroundVideo.assets.hlsUrl) e no
// formato do master da Apple (MEDIA + STREAM-INF com RESOLUTION/CODECS).
// As URIs das variantes sao http://127.0.0.1 sinteticas: o app exige URI
// "http" absoluta e a fixture prova que nenhum pedido de midia e feito —
// se fosse, cairia em rede real contra um host que nao existe.
static int fora = 0;

static const char *BUSCA_WEAPONS =
  "{\"shelves\":[{\"items\":["
  "{\"type\":\"Movie\",\"id\":\"umc.cmc.fixture1\",\"title\":\"Weapons\","
  "\"releaseDate\":1754600000000,\"images\":{\"shelfImageBackground\":"
  "{\"url\":\"https://is1-ssl.mzstatic.com/image/thumb/FIXTURE/{w}x{h}.{f}\"}}}]}]}";
static const char *BUSCA_TOMJERRY =
  "{\"shelves\":[{\"items\":["
  "{\"type\":\"Movie\",\"id\":\"umc.cmc.fixture2\",\"title\":\"Tom & Jerry\","
  "\"releaseDate\":1614556800000}]}]}";
static const char *BUSCA_VAZIA = "{\"shelves\":[{\"items\":[]}]}";

static const char *FILME_WEAPONS =
  "{\"data\":{\"content\":{\"backgroundVideo\":{\"assets\":{"
  "\"hlsUrl\":\"http://127.0.0.1:9/trailer/master-weapons.m3u8\"}}}}}";
static const char *FILME_TOMJERRY =
  "{\"data\":{\"content\":{\"backgroundVideo\":{\"assets\":{"
  "\"hlsUrl\":\"http://127.0.0.1:9/trailer/master-tomjerry.m3u8\"}}}}}";

static const char *MASTER =
  "#EXTM3U\n#EXT-X-VERSION:6\n#EXT-X-INDEPENDENT-SEGMENTS\n"
  "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud1\",NAME=\"fixture\",URI=\"http://127.0.0.1:9/trailer/a.m3u8\"\n"
  "#EXT-X-STREAM-INF:RESOLUTION=1920x1080,CODECS=\"avc1.4d401f\",AUDIO=\"aud1\"\n"
  "http://127.0.0.1:9/trailer/v1080.m3u8\n"
  "#EXT-X-STREAM-INF:RESOLUTION=1280x720,CODECS=\"avc1.4d401f\",AUDIO=\"aud1\"\n"
  "http://127.0.0.1:9/trailer/v720.m3u8\n"
  "#EXT-X-STREAM-INF:RESOLUTION=864x486,CODECS=\"avc1.4d401f\",AUDIO=\"aud1\"\n"
  "http://127.0.0.1:9/trailer/v486.m3u8\n";

static int nPedidos;
static char pedidos[8][1200];

static const char *fixture(const char *u) {
  if (strstr(u, "tv.apple.com/api/uts/v3/search")) {
    if (strstr(u, "Weapons"))    return BUSCA_WEAPONS;
    // Exact encoded term (src urlEncode: space -> %20, '&' -> %26), at the
    // end of the URL, so a broken '&' encoding misses and the test fails.
    { const char *t = "&searchTerm=Tom%20%26%20Jerry"; size_t lu = strlen(u), lt = strlen(t);
      if (lu >= lt && !strcmp(u + lu - lt, t)) return BUSCA_TOMJERRY; }
    return BUSCA_VAZIA;
  }
  if (strstr(u, "tv.apple.com/api/uts/v3/movies/umc.cmc.fixture1")) return FILME_WEAPONS;
  if (strstr(u, "tv.apple.com/api/uts/v3/movies/umc.cmc.fixture2")) return FILME_TOMJERRY;
  if (strstr(u, "127.0.0.1:9/trailer/master-")) return MASTER;
  return NULL;   // fora da fixture: violacao
}

char *rede_baixar_com(const char *u, int t, const char *const *c) {
  const char *r;
  (void)t; (void)c;
  if (nPedidos < 8) snprintf(pedidos[nPedidos], sizeof pedidos[0], "%s", u);
  nPedidos++;
  r = fixture(u);
  if (!r) { fora++; printf("FALHOU: pedido fora da fixture: %.200s\n", u); }
  return r ? strdup(r) : NULL;
}
char *rede_baixar_st(const char *u, int t, const char *const *c, int *status) {
  char *r = rede_baixar_com(u, t, c);
  if (status) *status = r ? 200 : 404;
  return r;
}
// ---------------------------------------------------------------------------

static int esperar(const char *id) { int i; for (i = 0; i < 300 && !trailerapple_respondeu(id); i++) usleep(100000); return trailerapple_respondeu(id); }
int main(void) {
  const char *u;
  { char cmd[2200]; snprintf(cmd, sizeof cmd, "rm -rf '%s'; mkdir -p '%s'", nvt("/nuvio-trailerapple-teste"), nvt("/nuvio-trailerapple-teste")); system(cmd); }
  trailerapple_pedir("tt26581740", "Weapons", "2025 · Terror", 0);
  esperar("tt26581740");
  u = trailerapple_url("tt26581740");
  if (!u || strncmp(u, "file://", 7) || !strstr(u, "-play.m3u8")) { printf("FALHOU: Weapons -> %s\n", u ? u : "nada"); return 1; }
  { FILE *f = fopen(u + 7, "r"); char l[2048]; int inf = 0, uri = 0;
    if (!f) { printf("FALHOU: reduzido nao existe\n"); return 1; }
    while (fgets(l, sizeof l, f)) { if (!strncmp(l, "#EXT-X-STREAM-INF", 17)) inf++; if (!strncmp(l, "http", 4)) uri++; }
    fclose(f);
    if (inf != 1 || uri != 1) { printf("FALHOU: reduzido com %d variantes e %d uris\n", inf, uri); return 1; }
    printf("ok  reduzido com uma variante\n"); }
  // Teto de 720p: a variante escolhida tem de caber em 1280 de largura.
  qualidade = 720;
  u = trailerapple_url("tt26581740");
  { FILE *f = u ? fopen(u + 7, "r") : NULL; char l[2048]; int w = 0;
    while (f && fgets(l, sizeof l, f)) { char *r = strstr(l, "RESOLUTION="); if (r) w = atoi(r + 11); }
    if (f) fclose(f);
    if (w <= 0 || w > 1280) { printf("FALHOU: teto 720 escolheu %dpx\n", w); return 1; }
    printf("ok  teto 720p -> %dpx\n", w); }
  qualidade = 0;
  printf("ok  Weapons 2025 -> %.80s\n", u);
  // Titulo com acento e '&': normalizacao. "Tom & Jerry" (2021).
  trailerapple_pedir("tt1361336", "Tom & Jerry", "2021", 0);
  esperar("tt1361336");
  u = trailerapple_url("tt1361336");
  if (!u || strncmp(u, "file://", 7) || !strstr(u, "-play.m3u8")) { printf("FALHOU: Tom & Jerry 2021 -> %s\n", u ? u : "nada"); return 1; }
  printf("ok  Tom & Jerry 2021 -> %.60s\n", u);
  // Sem ano: nao busca.
  trailerapple_pedir("tt0000001", "Weapons", "", 0);
  esperar("tt0000001");
  if (trailerapple_url("tt0000001")) { printf("FALHOU: sem ano nao devia achar\n"); return 1; }
  printf("ok  sem ano -> nada\n");
  if (fora) { printf("FALHOU: %d pedido(s) fora da fixture\n", fora); return 1; }
  printf("trailerapple: tudo ok\n");
  return 0;
}
