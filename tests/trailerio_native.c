#include "ajustes.h"
#include "trailer.h"
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

Uint32 SDL_GetTicks(void) { return ticks; }
float gfx_escala_tela_txt(void) { return 1.0f; }
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
void video_buscar(double seconds) { seeks++; position = seconds; }
void video_volume(int volume) { (void)volume; }
void video_janela(int x, int y, int w, int h) {
  assert(x == 0 && y == 0 && w == 1920 && h == 1080);
}
void video_janela_fonte(int sx, int sy, int sw, int sh, int x, int y, int w, int h) {
  (void)sx; (void)sy; (void)sw; (void)sh; (void)x; (void)y; (void)w; (void)h;
}
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
void video_parar(void) { stops++; active = playing = failed = ended = 0; }
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
  assert(trailer_abrir_webos(id, "movie", tela, 1, 0));
  trailer_atualizar(++ticks);
  assert(trailer_cheia() && !trailer_mostra_video() && !opens);
  assert(trailer_evento(&right) && seeks == 0);
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && cancels == 1 && !stops);

  assert(trailer_abrir_webos(id, "movie", tela, 1, 0));
  result = 1;
  trailer_atualizar(++ticks);
  assert(opens == 1 && active && !trailer_mostra_video());
  failed = 1;
  trailer_atualizar(++ticks);
  assert(opens == 2 && trailer_cheia() && !trailer_mostra_video());
  playing = 1;
  trailer_atualizar(++ticks);
  assert(trailer_mostra_video());
  assert(trailer_evento(&right) && seeks == 0); // No known duration yet.
  position = 4.0;
  duration = 26.0;
  assert(trailer_evento(&left) && seeks == 1 && position == 0.0);
  assert(trailer_evento(&left) && seeks == 1 && position == 0.0);
  assert(trailer_evento(&right) && seeks == 2 && position == 10.0 && playing);
  assert(trailer_evento(&ok) && trailer_pausado() && !playing);
  assert(trailer_evento(&right) && seeks == 3 && position == 20.0 && !playing);
  assert(trailer_evento(&right) && seeks == 4 && position == duration);
  right.key.repeat = 1;
  position = 10.0;
  assert(trailer_evento(&right) && seeks == 4 && position == 10.0);
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && !active && stops == 1);

  assert(trailer_abrir_webos(id, "movie", tela, 1, 0));
  result = -1;
  trailer_atualizar(++ticks);
  assert(!trailer_aberto() && trailer_falhou() && opens == 2);

  assert(trailer_abrir_webos(id, "movie", tela, 1, 0));
  result = 1;
  trailer_atualizar(++ticks);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(trailer_aberto() && opens == 4);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(!trailer_aberto() && trailer_falhou() && opens == 4 && stops == 2);

  assert(trailer_abrir_webos(id, "movie", tela, 1, 1));
  imdbUrl = imdb;
  trailer_atualizar(++ticks);
  assert(trailer_cheia() && !request && !appleOpens && !imdbOpens);
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && !request);

  assert(trailer_abrir_webos(id, "movie", tela, 1, 1));
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
  assert(trailer_abrir_webos(id, "movie", tela, 1, 1));
  trailer_atualizar(++ticks);
  assert(!request && active && appleOpens == 1 && imdbOpens == 2);
  assert(trailer_evento(&back));

  imdbUrl = NULL;
  assert(trailer_abrir_webos(id, "movie", tela, 1, 1));
  trailer_atualizar(++ticks);
  assert(request && !active && appleOpens == 1 && imdbOpens == 2);
  assert(trailer_evento(&back));

  appleUrl = apple;
  assert(trailer_abrir_webos(id, "movie", tela, 1, 1));
  trailer_atualizar(++ticks);
  playing = 1;
  trailer_atualizar(++ticks);
  ended = 1;
  trailer_atualizar(++ticks);
  assert(!trailer_aberto() && !request && !trailer_falhou());
}
