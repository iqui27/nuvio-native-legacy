// Escolha da variante de MIDIA da Samsung (trailerapple.c, varianteMidia),
// sem rede e sem TV. O master abaixo imita o da Apple nos pontos que importam:
// tres pathways de CDN com as mesmas variantes, hvc1 mais largo que o avc1,
// Dolby Vision, uma URI relativa e o grupo de I-frames. O motor HLS da Samsung
// travava com o master inteiro (ver o comentario de varianteMidia); aqui se
// prova que o <video> recebe UMA playlist de midia avc1, absoluta, no teto.
#define __EMSCRIPTEN__ 1
#define static
#include "../src/trailerapple.c"
#undef static
#include <assert.h>

char *dados_caminho(char *dst, unsigned tam, const char *nome) { (void)nome; if (tam) dst[0] = 0; return NULL; }
void dados_marcar_sujo(int leve) { (void)leve; }
int ajustes_trailer_qualidade(void) { return 0; }
char *rede_baixar_com(const char *url, int segundos, const char *const *cab) { (void)url; (void)segundos; (void)cab; return NULL; }

static const char *MASTER =
  "#EXTM3U\n#EXT-X-VERSION:7\n#EXT-X-INDEPENDENT-SEGMENTS\n"
  "#EXT-X-CONTENT-STEERING:PATHWAY-ID=\"ap\",SERVER-URI=\"data:application/vnd.apple.steering-list;base64,e30=\"\n"
  "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a\",NAME=\"en\",DEFAULT=YES,URI=\"https://cdn/a.m3u8\"\n"
  "#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=63951,CODECS=\"avc1.64001f\",RESOLUTION=556x232,URI=\"https://cdn/iframe.m3u8\"\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=422773,CODECS=\"avc1.64001f,mp4a.40.29\",RESOLUTION=554x232,AUDIO=\"a\",PATHWAY-ID=\"ap\"\n"
  "https://vod-ap/avc-554.m3u8\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=4698088,CODECS=\"avc1.640020,mp4a.40.2\",RESOLUTION=1482x620,AUDIO=\"a\",PATHWAY-ID=\"ap\"\n"
  "https://vod-ap/avc-1482.m3u8\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=10609037,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=1920x804,AUDIO=\"a\",PATHWAY-ID=\"ap\"\n"
  "https://vod-ap/avc-1920.m3u8\r\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=20000000,CODECS=\"hvc1.2.20000000.L150.B0,mp4a.40.2\",RESOLUTION=3840x1608,AUDIO=\"a\",PATHWAY-ID=\"ap\"\n"
  "https://vod-ap/hvc-3840.m3u8\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=25000000,CODECS=\"dvh1.05.06,mp4a.40.2\",RESOLUTION=3840x1608,AUDIO=\"a\",PATHWAY-ID=\"ap\"\n"
  "https://vod-ap/dv-3840.m3u8\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=1000000,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=1280x536,AUDIO=\"a\",PATHWAY-ID=\"fa\"\n"
  "relativa-1280.m3u8\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=10609037,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=1920x804,AUDIO=\"a\",PATHWAY-ID=\"fa\"\n"
  "https://vod-fa/avc-1920.m3u8\n";

static const char *SO_HEVC =
  "#EXTM3U\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=1,CODECS=\"hvc1.2.4.L123.B0\",RESOLUTION=1186x496\n"
  "https://vod/hvc-1186.m3u8\n";

static int falhas;
static void confere(const char *nome, int ok) { printf("%s %s\n", ok ? "ok " : "FALHOU", nome); if (!ok) falhas++; }

int main(void) {
  char u[600];
  confere("sem teto: avc1 mais largo, primeiro pathway, sem \\r",
          varianteMidia(MASTER, 0, u, sizeof u) && !strcmp(u, "https://vod-ap/avc-1920.m3u8"));
  confere("avc1 ganha de hvc1 mais largo e Dolby Vision fica fora", !strstr(u, "hvc") && !strstr(u, "dv-"));
  confere("teto 720: cabe em 1280 e URI relativa e descartada",
          varianteMidia(MASTER, 720, u, sizeof u) && !strcmp(u, "https://vod-ap/avc-554.m3u8"));
  confere("teto 1080 cabe em 1920", varianteMidia(MASTER, 1080, u, sizeof u) && !strcmp(u, "https://vod-ap/avc-1920.m3u8"));
  confere("nunca devolve o master nem playlist de I-frames", !strstr(u, "iframe") && strncmp(u, "https://play-edge", 17));
  confere("so hvc1: usa hvc1", varianteMidia(SO_HEVC, 0, u, sizeof u) && !strcmp(u, "https://vod/hvc-1186.m3u8"));
  confere("master vazio ou nulo: nada", !varianteMidia("#EXTM3U\n", 0, u, sizeof u) && !varianteMidia(NULL, 0, u, sizeof u));
  puts(falhas ? "trailerapple-midia: FALHOU" : "trailerapple-midia: tudo ok");
  return falhas ? 1 : 0;
}
