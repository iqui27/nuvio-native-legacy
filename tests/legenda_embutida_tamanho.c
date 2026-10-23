/* Exercise the real track-selection path. LG plain text must use the app's
 * renderer (the same size as the style preview), not uMS's five presets. */
#include "../src/faixas.c"
#include <assert.h>
int xtream_url_limitada(const char *url) { (void)url; return 0; }

static VideoFaixa faixa;
static int nativa, ordinal, sonda = 1, paradas;
int video_n_legenda(void) { return 1; }
const VideoFaixa *video_legenda(int i) { return i == 0 ? &faixa : NULL; }
const char *video_url_atual(void) { return "https://fixture.invalid/episode.mkv"; }
int video_legenda_ordinal_mkv(int i) { return i == 0 && sonda == 1 ? 2 : -1; }
int video_mkv_sondado(void) { return sonda; }
void video_sondar_mkv_agora(void) {}
void video_escolher_legenda(int i) { nativa = i; }
void mkvass_parar(void) { paradas++; }
void mkvass_iniciar_ordinal(const char *url, int ord) {
  assert(!strcmp(url, video_url_atual()));
  /* Integration #421 keeps native text until the app's first cue; ASS is
   * disabled before collection. mkvass already accepts plain text by default. */
  assert(nativa == (ehAss(&faixa) ? -1 : 0));
  ordinal = ord;
}
void legenda_desligar(void) {}
void legsync_primaria_outra(int embutida) { (void)embutida; }
void legsync_primaria_externa(const char *u, const char *l, const char *p) { (void)u; (void)l; (void)p; assert(0); }
const Legenda *addons_legenda(int i) { (void)i; return NULL; }
void legendasui_id_addon(const Legenda *l, char out[24]) { (void)l; out[0] = 0; }
uint64_t legsync_hash_url(const char *u) { (void)u; return 1; }
Uint32 SDL_GetTicks(void) { return 1; }
const char *i18n(const char *s) { return s; }

int main(void) {
  const char *codecs[] = {"S_TEXT/UTF8", "S_TEXT/WEBVTT", "S_TEXT/ASS"};
  for (unsigned i = 0; i < sizeof codecs / sizeof codecs[0]; i++) {
    snprintf(faixa.codec, sizeof faixa.codec, "%s", codecs[i]);
    ordinal = -1;
    escolherLegenda(0);
    assert(ordinal == 2 && legOverlay == 0);
    assert(nativa == (ehAss(&faixa) ? -1 : 0));
    assert(legOverlayTV == !ehAss(&faixa));
  }
  /* Image captions cannot be restyled as text. */
  snprintf(faixa.codec, sizeof faixa.codec, "S_HDMV/PGS");
  ordinal = -1;
  escolherLegenda(0);
  assert(nativa == 0 && ordinal == -1 && legOverlay == -1);
  /* Keep native playback as fallback if extraction has failed. */
  snprintf(faixa.codec, sizeof faixa.codec, "S_TEXT/UTF8");
  legOverlayNoGo = 0;
  escolherLegenda(0);
  assert(nativa == 0 && ordinal == -1 && legOverlay == -1);
  /* Selection before the MKV probe completes schedules the existing handoff. */
  legOverlayNoGo = -1; sonda = 0; faixa.codec[0] = 0;
  escolherLegenda(0);
  assert(nativa == 0 && legOverlayEsperando == 0 && ordinal == -1);
  escolherLegenda(-1);
  assert(nativa == -1 && legOverlayEsperando == -1 && paradas > 0);
  puts("embedded subtitle sizing: app text rendering, native fallback and pending probe OK");
}
