#include "ajustes.h"
#include "trailer.h"
#include "trailerio.h"
#include "video.h"
#include <assert.h>
#include <stdlib.h>

static const char id[] = "tt33764258";
static const char mp4[] = "https://video.example.net/trailer.mp4";
static const char hls[] = "https://video.example.net/playlist.m3u8";
static Uint32 ticks = 100;
static int request, result, cancels, opens, stops, active, playing, failed;
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
int video_terminou(void) { return 0; }
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
  failed = playing = 0;
  active = 1;
  return 1;
}
void video_definir_mp4(int ehMp4) { assert(!ehMp4); }
void video_parar(void) { stops++; active = playing = 0; }

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
  assert(trailer_abrir_trailerio(id, "movie", tela, 1));
  trailer_atualizar(++ticks);
  assert(trailer_cheia() && !trailer_mostra_video() && !opens);
  assert(trailer_evento(&right) && seeks == 0);
  assert(trailer_evento(&back));
  assert(!trailer_aberto() && cancels == 1 && !stops);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1));
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

  assert(trailer_abrir_trailerio(id, "movie", tela, 1));
  result = -1;
  trailer_atualizar(++ticks);
  assert(!trailer_aberto() && trailer_falhou() && opens == 2);

  assert(trailer_abrir_trailerio(id, "movie", tela, 1));
  result = 1;
  trailer_atualizar(++ticks);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(trailer_aberto() && opens == 4);
  ticks += 8000;
  trailer_atualizar(ticks);
  assert(!trailer_aberto() && trailer_falhou() && opens == 4 && stops == 2);
}
