// tests/proxyts.sh: o proxy de TS da Live TV contra o servidor falso.
#include "../src/proxyts.c"
#include "nvtmp.h"
#include "streams.h"
#include <assert.h>
int ajustes_livetv_resolucao(void) { return 0; }
static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)
static long baixar(const char *url, int segundos, unsigned char **out, char *cab, size_t ncab) {
  char cmd[600];
  FILE *f;
  long n = 0;
  unsigned char *b = malloc(64 << 20);
  snprintf(cmd, sizeof cmd, "curl -s -D '%s/nuvio-proxyts-cab' --max-time %d '%s'", nvt_dir(), segundos, url);
  f = popen(cmd, "r");
  while (f && n < (64 << 20)) { size_t r = fread(b + n, 1, 65536, f); if (!r) break; n += (long)r; }
  if (f) pclose(f);
  { FILE *c = fopen(nvt("/nuvio-proxyts-cab"), "r"); size_t r = c ? fread(cab, 1, ncab - 1, c) : 0; cab[r] = 0; if (c) fclose(c); }
  *out = b;
  return n;
}
int main(void) {
  char url[200], cab[2048];
  unsigned char *b;
  long n;
  PxLista l;
  char j[300];
  // juntar e ler playlist
  pxJuntar("http://h:1/live/u/p/9.m3u8?tok=1/2", "seg1.ts", j, sizeof j);
  OK(!strcmp(j, "http://h:1/live/u/p/seg1.ts"), "segmento relativo ao diretorio da playlist (query fora)");
  pxJuntar("http://h:1/a/b.m3u8", "/hls/s.ts", j, sizeof j);
  OK(!strcmp(j, "http://h:1/hls/s.ts"), "segmento absoluto no host");
  OK(pxLer("#EXTM3U\n#EXT-X-TARGETDURATION:4\n#EXT-X-MEDIA-SEQUENCE:120\n#EXTINF:4,\na.ts\n#EXTINF:4,\nb.ts\n", "http://h/x/l.m3u8", &l)
     && l.alvo == 4 && l.seq0 == 120 && l.n == 2 && !strcmp(l.uri[1], "http://h/x/b.ts"), "playlist de midia");
  OK(pxLer("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\nv1/i.m3u8\n#EXT-X-STREAM-INF:BANDWIDTH=2\nv2/i.m3u8\n", "http://h/m.m3u8", &l)
     && l.mestre && !strcmp(l.uri[0], "http://h/v2/i.m3u8"), "master: highest bandwidth in Automatic");

  // HLS ao vivo (o .ts que vira playlist): fluxo TS continuo, com PAT no comeco
  assert(proxyts_url("http://127.0.0.1:8765/live/u/p/105.ts", url, sizeof url));
  OK(proxyts_e_url(url), "URL local do proxy");
  n = baixar(url, 6, &b, cab, sizeof cab);
  OK(strstr(cab, "200 OK") && strstr(cab, "video/mp2t") && !strstr(cab, "Content-Length"), "200 video/mp2t sem Content-Length");
  OK(n > 4000000 && b[0] == 0x47 && b[188] == 0x47 && pxPid(b) == 0, "fluxo TS comeca pelo PAT");
  { long i, bons = 0; for (i = 0; i + 188 <= n; i += 188) if (b[i] == 0x47) bons++;
    OK(bons == n / 188, "todo pacote alinhado em 188"); }
  free(b);
  // TS continuo: 302 para a fonte
  assert(proxyts_url("http://127.0.0.1:8765/live/u/p/101.ts", url, sizeof url));
  { char cmd[300]; FILE *f; char r[512] = "";
    snprintf(cmd, sizeof cmd, "curl -s -o /dev/null -w '%%{http_code} %%{redirect_url}' --max-time 5 '%s'", url);
    f = popen(cmd, "r"); if (f) { fgets(r, sizeof r, f); pclose(f); }
    OK(!strncmp(r, "302 http://127.0.0.1:8765/live/u/p/101.ts", 41), "TS continuo: 302 para a fonte"); }
  // sessao velha: 404
  { char velha[200]; snprintf(velha, sizeof velha, "%s", url);
    assert(proxyts_url("http://127.0.0.1:8765/live/u/p/105.ts", url, sizeof url));
    n = baixar(velha, 3, &b, cab, sizeof cab); free(b);
    OK(strstr(cab, "404"), "sessao trocada: a URL antiga da 404"); }
  // CANAL DE ADDON COM CABECALHO (#283)
  OK(proxyts_candidata("https://cdn/x/canal.m3u8", 0), "candidata: m3u8");
  OK(proxyts_candidata("http://painel:80/live/1", 0), "candidata: sem extensao");
  OK(!proxyts_candidata("https://cdn/f.mp4?t=1", 0), "candidata: mp4 com query fica fora");
  OK(!proxyts_candidata("https://cdn/f.mkv", 0), "candidata: mkv fica fora");
  OK(!proxyts_candidata("https://cdn/hls", 1), "candidata: fonte marcada mp4 fica fora");
  OK(!proxyts_candidata("magnet:?xt=1", 0), "candidata: nao http fica fora");
  OK(pxLer("#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"k\"\n#EXTINF:4,\na.ts\n", "http://h/l.m3u8", &l) && l.cifrado,
     "playlist cifrada marcada");
  OK(pxLer("#EXTM3U\n#EXT-X-KEY:METHOD=NONE\n#EXTINF:4,\na.ts\n", "http://h/l.m3u8", &l) && !l.cifrado,
     "METHOD=NONE nao e cifrada");
  OK(pxLer("#EXTM3U\n#EXT-X-MAP:URI=\"i.mp4\"\n#EXTINF:4,\na.m4s\n", "http://h/l.m3u8", &l) && l.fmp4,
     "playlist fMP4 marcada");
  // sem os cabecalhos o CDN recusa: o proxy so redireciona (e diz que redirecionou)
  proxyts_definir_cabecalhos("");
  assert(proxyts_url("http://127.0.0.1:8765/addon/r", url, sizeof url));
  { char cmd[300]; FILE *f; char r[512] = "";
    snprintf(cmd, sizeof cmd, "curl -s -o /dev/null -w '%%{http_code}' --max-time 5 '%s'", url);
    f = popen(cmd, "r"); if (f) { fgets(r, sizeof r, f); pclose(f); }
    OK(!strncmp(r, "302", 3) && proxyts_redirecionou(), "sem cabecalho: 302 e redirecionou=1"); }
  // com o Referer do addon: playlist (apos o 302) e segmentos relativos viram TS continuo
  { Stream *s = NULL;
    assert(stream_extrair("{\"streams\":[{\"url\":\"http://127.0.0.1:8765/addon/r\","
      "\"behaviorHints\":{\"notWebReady\":true,\"proxyHeaders\":{\"request\":{"
      "\"Referer\":\"https://addon.example/\",\"Origin\":\"https://addon.example\","
      "\"User-Agent\":\"Teste/1\"}}}}]}", "Bsweb TV", &s) == 1);
    proxyts_definir_cabecalhos(s[0].cabecalhos);
    free(s); }
  assert(proxyts_url("http://127.0.0.1:8765/addon/r", url, sizeof url));
  OK(!proxyts_redirecionou(), "sessao nova zera o redirecionou");
  n = baixar(url, 6, &b, cab, sizeof cab);
  OK(strstr(cab, "200 OK") && strstr(cab, "video/mp2t"), "com cabecalho: 200 video/mp2t");
  OK(n > 2000000 && b[0] == 0x47 && pxPid(b) == 0, "com cabecalho: TS pelo PAT, segmentos relativos ao endereco final");
  OK(!proxyts_redirecionou(), "com cabecalho: nao redirecionou");
  free(b);
  // HLS cifrado: o proxy encerra sem entregar video (o app tenta direto)
  assert(proxyts_url("http://127.0.0.1:8765/addon/v/cifrado.m3u8", url, sizeof url));
  n = baixar(url, 4, &b, cab, sizeof cab);
  OK(n < 188 * 10, "cifrado: nenhum segmento entregue");
  free(b);
  proxyts_parar();
  if (falhas) return 1;
  printf("proxyts: tudo ok\n");
  return 0;
}
