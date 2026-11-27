#include "ajustes.h"
#include "plrui.h"
#include "trailer.h"
#include "trailerfonte.h"
#include "trailerio.h"
#include "video.h"
#include <assert.h>
#include <stdlib.h>

static const char id[] = "tt33764258";
static const char mp4[] = "https://video.example.net/trailer.mp4";
static const char hls[] = "https://video.example.net/playlist.m3u8";
static const char apple[] = "file:///tmp/apple-trailer.m3u8";
static const char imdb[] = "https://video.example.net/imdb.mp4";
static Uint32 ticks = 100;
static int request, result, cancels, opens, stops, active, playing, failed, ended;
static int appleDone, imdbDone, mp4Mode, appleOpens, imdbOpens;
static const char *appleUrl, *imdbUrl;
static double position, duration;
static int seeks;
static float uiScale = 1.0f, drawScale = 1.0f;
static float barX, barY, barW, barScale;
static GfxRect pauseButton;
static int bars, buttons;

Uint32 SDL_GetTicks(void) { return ticks; }
float gfx_escala_tela_txt(void) { return 1.0f; }
float gfx_escala_ui(void) { return uiScale; }
float gfx_escala_entrar(void) { float anterior = drawScale; drawScale = uiScale; return anterior; }
void gfx_escala_sair(float anterior) { drawScale = anterior; }
void gfx_veu_css(GfxRect r, int edge, float curve, float end, float alpha) {
  (void)r; (void)edge; (void)curve; (void)end; (void)alpha;
#ifdef NV_WEBOS
  assert(0 && "No full-screen dimming over webOS trailers");
#endif
}
void gfx_icone(GfxRect r, const char *name, float red, float green, float blue, float a) {
  (void)r; (void)name; (void)red; (void)green; (void)blue; (void)a;
}
TxtLinha txt_linha(TxtEstilo style, const char *s, int r, int g, int b, int a) {
  (void)style; (void)r; (void)g; (void)b; (void)a;
  return (TxtLinha){.w = (int)strlen(s) * 12, .h = 28};
}
TxtLinha txt_linha_corta(TxtEstilo style, const char *s, int r, int g, int b, int a, float maxW) {
  TxtLinha line = txt_linha(style, s, r, g, b, a);
  if (line.w > maxW) line.w = (int)maxW;
  return line;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
void plrui_material(GfxRect r, float radius, int modal, float a) { (void)r; (void)radius; (void)modal; (void)a; }
void plrui_sep(float x, float y, float a) { (void)x; (void)y; (void)a; }
int plrui_tinta(void) { return 0; }
void plrui_pilula_foco(GfxRect r, float a) { (void)a; pauseButton = r; buttons++; }
void plrui_tempo(char *b, size_t n, double s) { snprintf(b, n, "%.0f", s); }
void plrui_barra(float x, float y, float w, float frac, float buf, int focus,
                 const float *caps, int nCaps, float a) {
  (void)frac; (void)buf; (void)focus; (void)caps; (void)nCaps; (void)a;
  bars++; barX = x; barY = y; barW = w; barScale = drawScale;
}
float ajustes_trailer_zoom(void) { return 1.0f; }
int ajustes_trailer_fonte(void) { return 0; }
int video_iniciar_auto(void) { return 1; }
int video_ativo(void) { return active; }
int video_pronto(void) { return playing; }
int video_tocando(void) { return playing; }
int video_terminou(void) { return ended; }
int video_falhou(void) { return failed; }
int video_conflito_recurso(void) { return 0; }
int video_largura(void) { return 0; }
int video_altura(void) { return 0; }
int video_recorte_fonte(void) { return 0; }
void video_recorte_reaplicar(void) {}
void video_bombear(void) {}
void video_pausar(int p) { playing = !p; }
double video_pos(void) { return position; }
double video_duracao(void) { return duration; }
double video_buffer_fim(void) { return duration; }
void video_buscar(double seconds) { seeks++; position = seconds; }
void video_volume(int volume) { (void)volume; }
void video_janela(int x, int y, int w, int h) {
  assert(x == 0 && y == 0 && w == 1920 && h == 1080);
}
void video_janela_fonte(int sx, int sy, int sw, int sh, int x, int y, int w, int h) {
  (void)sx; (void)sy; (void)sw; (void)sh; (void)x; (void)y; (void)w; (void)h;
}
#ifdef NV_ANDROID
int video_janela_pronta(void) { return 1; }
#endif
#ifdef NV_TPK
static int tpkTrailer;
int video_recorte_fonte_trailer(void) { return 0; }
int video_tpk_cartao_cover(void) { return 0; }
void video_tpk_cartao_marcar(int sim) { (void)sim; }
void video_tpk_trailer_marcar(int sim) { tpkTrailer = sim; }
#endif
int video_tocar_trailerio(const char *url, int ehHls) {
  assert(!strcmp(url, ehHls ? hls : mp4));
  opens++;
  failed = playing = ended = 0;
  active = 1;
  return 1;
}
int video_tocar(const char *url) {
  if (!strcmp(url, apple)) { assert(!mp4Mode); appleOpens++; }
  else { assert(!strcmp(url, imdb) && mp4Mode); imdbOpens++; }
  failed = playing = ended = 0;
  active = 1;
  return 1;
}
void video_definir_cabecalhos(const char *headers) { assert(!headers); }
void video_definir_dv(int dv) { assert(!dv); }
void video_definir_mp4(int ehMp4) { mp4Mode = ehMp4; }
void video_parar(void) {
  stops++; active = playing = failed = ended = 0;
#ifdef NV_TPK
  tpkTrailer = 0;
#endif
}
const char *trailerapple_url(const char *imdbId) { assert(!strcmp(imdbId, id)); return appleUrl; }
const char *trailerimdb_url(const char *imdbId, const char **name) {
  assert(!strcmp(imdbId, id) && !name);
  return imdbUrl;
}
int trailerapple_respondeu(const char *imdbId) { assert(!strcmp(imdbId, id)); return appleDone; }
int trailerimdb_respondeu(const char *imdbId) { assert(!strcmp(imdbId, id)); return imdbDone; }

int trailerio_id(const char *imdbId, const char *tipo) {
  return !strcmp(imdbId, id) && !strcmp(tipo, "movie");
}
int trailerio_iniciar(const char *imdb, const char *tipo) {
  assert(!request && !strcmp(imdb, id) && !strcmp(tipo, "movie"));
  request = 1;
  result = 0;
  return 1;
}
int trailerio_resultado(TrailerioLista **lista) {
  assert(request);
  if (result) request = 0;
  if (result > 0) {
    *lista = calloc(1, sizeof **lista);
    assert(*lista);
    (*lista)->n = 2;
    strcpy((*lista)->fontes[0].url, mp4);
    strcpy((*lista)->fontes[1].url, hls);
    (*lista)->fontes[1].hls = 1;
  }
  return result;
}
void trailerio_cancelar(void) { if (request) { request = 0; cancels++; } }

int main(void) {
  GfxRect tela = { 0, 0, 1920, 1080 };
  SDL_Event back = { .key = { .type = SDL_KEYDOWN, .keysym.sym = SDLK_AC_BACK } };
  SDL_Event left = { .key = { .type = SDL_KEYDOWN, .keysym.sym = SDLK_LEFT } };
  SDL_Event right = { .key = { .type = SDL_KEYDOWN, .keysym.sym = SDLK_RIGHT } };
  SDL_Event ok = { .key = { .type = SDL_KEYDOWN, .keysym.sym = SDLK_RETURN } };
  // The flow below is the Apple TV -> IMDb -> Trailerio order; the .tpk's
  // IMDb-first full-screen order is checked at the end.
  trailerfonte_definir_imdb_primeiro_cheia(0);
  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 0));
  trailer_atualizar(++ticks);
  assert(trailer_cheia() && !trailer_mostra_video() && !opens);
  assert(trailer_evento(&right) && seeks == 0);
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && cancels == 1 && !stops);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 0));
  result = 1;
  trailer_atualizar(++ticks);
  assert(opens == 1 && active && !trailer_mostra_video());
  failed = 1;
  trailer_atualizar(++ticks);
  assert(opens == 2 && trailer_cheia() && !trailer_mostra_video());
  position = 1.0;   // .tpk: the hole opens once the position moves
  playing = 1;
  trailer_atualizar(++ticks);
  assert(trailer_mostra_video());
#ifdef NV_TPK
  assert(tpkTrailer);
#endif
  assert(trailer_evento(&right) && seeks == 0); // No known duration yet.
  position = 4.0;
  duration = 26.0;
  assert(trailer_evento(&left) && seeks == 1 && position == 0.0);
  assert(trailer_evento(&left) && seeks == 1 && position == 0.0);
  assert(trailer_evento(&right) && seeks == 2 && position == 5.0 && playing);
  trailer_osd_desenhar("Test", 1.0f);
  assert(bars == 1 && buttons == 0);
#ifdef NV_WEBOS
  assert(barX == 64.0f && barY == 960.0f && barW == 1792.0f);
#endif
  assert(trailer_evento(&ok) && trailer_pausado() && !playing);
  trailer_osd_desenhar("Test", 1.0f);
  assert(bars == 2 && buttons == 1 && pauseButton.x == barX);
#ifdef NV_WEBOS
  assert(barY == 902.0f && pauseButton.y == 940.0f);
  uiScale = 1.25f;
  drawScale = 1.10f;
  trailer_osd_desenhar("Test", 1.0f);
  assert(bars == 3 && barScale == uiScale && drawScale == 1.10f);
  assert(barY == 686.0f && barW == 1408.0f && pauseButton.y == 724.0f);
#else
  trailer_osd_desenhar("Test", 1.0f);
  assert(bars == 3);
#endif
  assert(trailer_evento(&right) && seeks == 3 && position == 10.0 && !playing);
  position = 23.0;
  assert(trailer_evento(&right) && seeks == 4 && position == duration);
  right.key.repeat = 1;
  position = 10.0;
  assert(trailer_evento(&right) && seeks == 4 && position == 10.0);
  assert(trailer_evento(&ok) && !trailer_pausado() && playing);
  ticks += 3002;
  trailer_osd_desenhar("Test", 1.0f);
  assert(bars == 3 && buttons == 2);
#ifdef NV_WEBOS
  assert(drawScale == 1.10f);
#endif
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && !active && stops == 1);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 0));
  result = -1;
  trailer_atualizar(++ticks);
  assert(!trailer_aberto() && trailer_falhou() && opens == 2);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 0));
  result = 1;
  trailer_atualizar(++ticks);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(trailer_aberto() && opens == 4);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(!trailer_aberto() && trailer_falhou() && opens == 4 && stops == 2);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
  imdbUrl = imdb;
  trailer_atualizar(++ticks);
  assert(trailer_cheia() && !request && !appleOpens && !imdbOpens);
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && !request);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
  appleUrl = apple;
  trailer_atualizar(++ticks);
  assert(appleOpens == 1 && !imdbOpens && !request);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(appleOpens == 1 && !imdbOpens && !request); // Primary playback keeps its existing timeout.
  failed = 1;
  trailer_atualizar(++ticks);
  assert(imdbOpens == 1 && !request && mp4Mode);
  playing = 1;
  trailer_atualizar(++ticks);
  failed = 1;
  trailer_atualizar(++ticks);
  assert(request && !active && !mp4Mode);
  result = 1;
  trailer_atualizar(++ticks);
  assert(opens == 5 && !request);
  assert(trailer_evento(&back));

  appleUrl = NULL;
  appleDone = imdbDone = 1;
  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
  trailer_atualizar(++ticks);
  assert(!request && active && appleOpens == 1 && imdbOpens == 2);
  assert(trailer_evento(&back));

  imdbUrl = NULL;
  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
  trailer_atualizar(++ticks);
  assert(request && !active && appleOpens == 1 && imdbOpens == 2);
  assert(trailer_evento(&back));

  appleUrl = apple;
  assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
  trailer_atualizar(++ticks);
  playing = 1;
  trailer_atualizar(++ticks);
  ended = 1;
  trailer_atualizar(++ticks);
  assert(!trailer_aberto() && !request && !trailer_falhou());

  // Full-screen order with IMDb first (the .tpk default, trailerfonte.c):
  // IMDb, then Apple TV, then Trailerio. Without sound Apple TV stays first.
  trailerfonte_definir_imdb_primeiro_cheia(1);
  appleUrl = apple;
  imdbUrl = imdb;
  {
    const int a0 = appleOpens, i0 = imdbOpens, o0 = opens;
    assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
    trailer_atualizar(++ticks);
    assert(imdbOpens == i0 + 1 && appleOpens == a0 && mp4Mode && !request);
    failed = 1;
    trailer_atualizar(++ticks);
    assert(appleOpens == a0 + 1 && imdbOpens == i0 + 1 && !mp4Mode && !request);
    failed = 1;
    trailer_atualizar(++ticks);
    assert(request && !active);
    result = 1;
    trailer_atualizar(++ticks);
    assert(opens == o0 + 1 && !request);
    assert(trailer_evento(&back) && !trailer_aberto());

    assert(trailer_abrir_trailerio(id, "movie", tela, 0, 1));
    trailer_atualizar(++ticks);
    assert(appleOpens == a0 + 2 && imdbOpens == i0 + 1);
    assert(trailer_evento(&back) && !trailer_aberto());

    // A muted Apple TV background trailer: with IMDb first and sound, the
    // Trailer button opens IMDb instead of keeping the silent Apple stream.
    trailer_abrir(apple, tela, 0, 0);
    assert(appleOpens == a0 + 3 && trailer_aberto() && !trailer_cheia());
    assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
    trailer_atualizar(++ticks);
    assert(imdbOpens == i0 + 2 && appleOpens == a0 + 3 && trailer_cheia());
    assert(trailer_evento(&back) && !trailer_aberto());
    // Apple TV first (webOS, Android): the same trailer keeps playing.
    trailerfonte_definir_imdb_primeiro_cheia(0);
    trailer_abrir(apple, tela, 0, 0);
    assert(trailer_abrir_trailerio(id, "movie", tela, 1, 1));
    trailer_atualizar(++ticks);
    assert(appleOpens == a0 + 4 && imdbOpens == i0 + 2 && trailer_cheia() && trailer_com_som());
    assert(trailer_evento(&back) && !trailer_aberto());
    // The Trailerio setting never keeps an Apple TV or IMDb trailer.
    trailer_abrir(apple, tela, 0, 0);
    assert(trailer_abrir_trailerio(id, "movie", tela, 1, 0));
    assert(request && !active && appleOpens == a0 + 5);
    assert(trailer_evento(&back) && !trailer_aberto() && !request);
  }
}
