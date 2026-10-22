// #409: tecla e botao Voltar dispensam o erro Android e pedem a folha.
#include "player.h"
#include "catalogo.h"
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Ponte Android fora do escopo deste teste de teclas (nao abre video).
int video_tocar_retomada(const char *u, double s, double p) { (void)u; (void)s; (void)p; return 0; }
int video_retomada_inicial_estado(void) { return -1; }
int video_superficie_estavel(void) { return 1; }
static int falhas;
static void tecla(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; player_evento(&e);
}
static void caso(SDL_Keycode k, int botao) {
  player_abrir(0, NULL);
  player_erro_fonte_motivo("A fonte não respondeu a tempo.", NULL);
  if (botao) tecla(SDLK_RIGHT);
  tecla(k);
  int ok = player_pediu_fontes() && !player_fonte_falhou() && !player_quer_sair();
  printf("%s erro409: %s %d volta as fontes sem modal\n", ok ? "PASS" : "FAIL", botao ? "botao" : "tecla", k);
  falhas += !ok;
  player_encerrar();
}
int main(void) {
  CatItem c = {0};
  SDL_Init(SDL_INIT_TIMER);
  ajustes_dir(getenv("NUVIO_DADOS"));
  strcpy(c.imdb, "fixture409"); strcpy(c.tipo, "movie"); strcpy(c.titulo, "Fixture");
  cat_definir(&c, 1);
  caso(SDLK_AC_BACK, 0); caso(SDLK_ESCAPE, 0); caso(SDLK_BACKSPACE, 0); caso(SDLK_DELETE, 0);
  caso(SDLK_RETURN, 1); caso(SDLK_RETURN, 0);
  return falhas ? 1 : 0;
}
