// Capturas do overlay de legenda ASS (#92, fase 2): \an8 no topo, fala em
// baixo, \pos, enfase, e a cor do arquivo contra a cor que a pessoa escolheu.
// Fora da suite (precisa de janela GL e de olho humano). O tempo do player
// anda por SALTOS de 10 s (SDLK_RIGHT), que e o unico relogio sem video.
//
//   bash tests/legenda_ass_shot.sh /tmp/nuvio-legass
#include "catalogo.h"
#include "player.h"
#include "faixas.h"
#include "legenda.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 40; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    player_atualizar(1.f / 60, SDL_GetTicks()); episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, SDL_GetTicks()); faixas_atualizar(1.f / 60, SDL_GetTicks());
    glClearColor(.10f, .12f, .16f, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(SDL_GetTicks());
    SDL_GL_SwapWindow(win);
  }
  { SDL_Surface *s = SDL_CreateRGBSurface(0, 1920, 1080, 24, 0xff, 0xff00, 0xff0000, 0);
    glReadPixels(0, 0, 1920, 1080, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
    // O GL le de baixo para cima.
    { int y; unsigned char *p = s->pixels, *t = malloc((size_t)s->pitch);
      for (y = 0; y < 540; y++) { memcpy(t, p + y * s->pitch, (size_t)s->pitch);
        memcpy(p + y * s->pitch, p + (1079 - y) * s->pitch, (size_t)s->pitch);
        memcpy(p + (1079 - y) * s->pitch, t, (size_t)s->pitch); } free(t); }
    // No OSD oculto, a legenda verde e o texto brilhante no rodape. A
    // contagem vira uma regressao executavel para o retorno que antes pulava
    // desenharLegendaExterna().
    { int vivos = 0, x, y;
      for (y = 760; y < 1080; y++) for (x = 0; x < 1920; x++) {
        Uint8 *px = (Uint8 *)s->pixels + y * s->pitch + x * 3;
        Uint32 pix = (Uint32)px[0] | ((Uint32)px[1] << 8) | ((Uint32)px[2] << 16);
        Uint8 r, g, b; SDL_GetRGB(pix, s->format, &r, &g, &b);
        if (g > 100 && g > r + 20 && g > b + 10) vivos++;
      }
      SDL_SaveBMP(s, nome); SDL_FreeSurface(s); printf("captura: %s\n", nome); return vivos; }
  }
  printf("captura: %s\n", nome);
  return 0;
}

static void esconderControles(void) {
  Uint32 base = SDL_GetTicks() + 5001u;
  int i;
  for (i = 0; i < 80; i++) player_atualizar(1.f / 60.f, base + (Uint32)i * 16u);
  assert(!player_controles_visiveis());
}
// Sem video o relogio do player e `posSeg += dt` enquanto toca; um dt grande
// e um salto. (SDLK_RIGHT com a barra em pe move o foco, nao o tempo.)
static void avancar(float seg) { player_atualizar(seg, SDL_GetTicks()); }
static char *ler(const char *nome) {
  FILE *f = fopen(nome, "rb"); long n; char *s;
  assert(f); fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  s = malloc((size_t)n + 1); assert(fread(s, 1, (size_t)n, f) == (size_t)n); s[n] = 0; fclose(f);
  return s;
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-legass";
  char nome[600], *corpo;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: legenda ASS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie"); snprintf(c.titulo, sizeof c.titulo, "Anime de teste");
    cat_definir(&c, 1); }
  player_abrir(0, NULL);
  // "Fonte morta que voltou": e o unico caminho publico que poe o player a
  // TOCAR sem video, e tocando o tempo anda.
  player_erro_fonte(); player_limpar_erro_fonte();
  // Padrao de fabrica da folha e cor NAO tocada: o arquivo manda na cor.
  *player_leg_estilo() = (VideoLegendaEstilo){ 120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER };
  player_leg_estilo_tocou(PLR_LEG_NADA);
  corpo = ler("tests/fixtures/ass/posicionado.ass");
  legenda_definir_corpo(corpo); free(corpo);

  // t=11: "Fala embaixo" (base) + "{\an8}Placa traduzida" (topo, amarelo, negrito)
  avancar(11.f);
  snprintf(nome, sizeof nome, "%s-an8-e-base.bmp", saida); captura(nome, w);
  // t=21: enfase (\b, \i) numa fala so
  avancar(10.f);
  snprintf(nome, sizeof nome, "%s-enfase.bmp", saida); captura(nome, w);
  // \pos(640,100) em 1280x720 -> (960,150) na tela, vermelho. O evento vive
  // em 12..13 s: legenda de teste deslocada para 30..36 s.
  { corpo = ler("tests/fixtures/ass/posicionado.ass");
    char *q = strstr(corpo, "0:00:12.00,0:00:13.00"); assert(q);
    memcpy(q, "0:00:30.00,0:00:36.00", 21);
    legenda_definir_corpo(corpo); free(corpo); }
  avancar(10.f);
  snprintf(nome, sizeof nome, "%s-pos.bmp", saida); captura(nome, w);
  // A PESSOA MEXEU NA COR (verde): a cor do arquivo deixa de valer, a ancora
  // continua valendo. Volta a t=10.
  player_leg_estilo()->cor = 2; player_leg_estilo_tocou(PLR_LEG_COR);
  { corpo = ler("tests/fixtures/ass/posicionado.ass");
    char *q = strstr(corpo, "0:00:10.00,0:00:14.00"); assert(q);
    memcpy(q, "0:00:40.00,0:01:00.00", 21);
    q = strstr(q + 21, "0:00:10.00,0:00:14.00"); assert(q);
    memcpy(q, "0:00:40.00,0:01:00.00", 21);
    legenda_definir_corpo(corpo); free(corpo); }
  avancar(10.f);
  snprintf(nome, sizeof nome, "%s-cor-da-pessoa.bmp", saida); captura(nome, w);
  esconderControles();
  snprintf(nome, sizeof nome, "%s-osd-oculto.bmp", saida);
  { int verdes = captura(nome, w); assert(verdes > 10); printf("legenda oculta: %d pixels verdes\n", verdes); }
  puts("legenda_ass_shot: ok");
  return 0;
}
