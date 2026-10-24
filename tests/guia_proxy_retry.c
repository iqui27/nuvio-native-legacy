// Exercise the guide's real same-source fallback decision (#625/#682).
#define SDL_MAIN_HANDLED
#include "../src/app.c"
#include <assert.h>

static Stream source;
static char loaded[4200];
static double buffer = 9;
static int decoder, redirected, available = 1, webos = 11, loads;
const Stream *stream_item(int i) { return i == 0 ? &source : NULL; }
int proxyts_redirecionou(void) { return redirected; }
int proxyts_disponivel(void) { return available; }
int proxyts_candidata(const char *url, int mp4) {
  return !mp4 && !strncmp(url, "https://", 8);
}
void proxyts_definir_cabecalhos(const char *c) { (void)c; }
void video_definir_cabecalhos(const char *c) { (void)c; }
double video_buffer_fim(void) { return buffer; }
int video_decoder_anunciou(void) { return decoder; }
int nv_webos_major(void) { return webos; }
Uint32 SDL_GetTicks(void) { return 20000; }
void player_definir_fonte(const char *url) { snprintf(loaded, sizeof loaded, "%s", url); loads++; }
const char *player_id_canal(void) { return "xt:channel"; }
int xtream_e_id(const char *id) { (void)id; return 1; }
int stalker_e_id(const char *id) { (void)id; return 0; }
int xtream_url_limitada(const char *url) { (void)url; return 0; }
int ajustes_livetv_proxy(void) { return 0; }
static int savedProxy = -1;
int guia_proxy_do_canal(const char *id, int fallback) { (void)id; return savedProxy < 0 ? fallback : savedProxy; }

int main(void) {
  strcpy(source.url, "https://example.test/live/channel.ts");
  source.tsContinuo = 1;
  canalFonteIdx = 0;
  assert(retentarCanal() == 1); // BG8XY0: data arrived, but no decoder.
  assert(!strcmp(loaded, "nvproxy:https://example.test/live/channel.ts"));
  assert(canalFonteIdx == 0 && canalFonteDesde == 20000 && loads == 1);
  assert(retentarCanal() == 0); // Already tried direct: do not bounce back.
  assert(loads == 1);

  char url[4200];
  savedProxy = 1;
  assert(!strncmp(urlCanal(&source, url, sizeof url), PROXYTS_PREFIXO, 8));
  assert(retentarCanal() == 1); // Saved proxy may still fall back to direct.
  assert(!strcmp(loaded, source.url));
  assert(retentarCanal() == 0);
  savedProxy = -1;

  urlCanal(&source, url, sizeof url); // A new source attempt resets retry state.
  decoder = 1; assert(retentarCanal() == 0); decoder = 0;
  buffer = 0; assert(retentarCanal() == 0); buffer = 9;
  available = 0; assert(retentarCanal() == 0); available = 1;
  webos = 0; assert(retentarCanal() == 0); webos = 11;
  source.mp4 = 1; assert(retentarCanal() == 0); source.mp4 = 0;
  assert(retentarCanal() == 1);
  redirected = 1; assert(retentarCanal() == 0);
  puts("PASS: guide retries buffered/no-decoder live once via proxy; saved proxy, direct fallback and guards preserved");
}
