// CAPTURAS DA MODAL DE TECLADO, nos quatro alfabetos que o app abre.
//
// Existe por causa da #88: o alfabeto de usuario/senha Xtream tem 73 simbolos,
// a grade de 6 colunas pedia 13 fileiras, o teto era 8 e os digitos ficavam
// FORA da grade — nem desenhados nem alcancaveis. O dono fotografou o teclado
// cortado depois de "P". Aqui o teste nao so fotografa: ele varre a grade com o
// D-pad e confere que CADA um dos 73 simbolos foi digitado, porque "aparece na
// tela" e "da para apertar" sao duas afirmacoes. Na grade antiga a varredura
// para no 'Q' (indice 42 = 7 fileiras x 6 colunas).
//
// Os alfabetos estao COPIADOS de ajustes.c (la sao static). Se um dia mudarem
// la, este teste continua valido para o formato; o que ele mede e a grade.
#include "teclado.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *XT =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@!#$%&*+=";
static const char *PORTAL = "abcdefghijklmnopqrstuvwxyz0123456789.:-";
static const char *MAC = "0123456789abcdef:";

static SDL_Window *janela;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  teclado_evento(&e);
}
static void teclas(SDL_Keycode k, int n) { while (n-- > 0) tecla(k); }

static void quadro(void) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(12);
  gfx_novo_quadro();
  teclado_atualizar(1.0f / 60.0f, SDL_GetTicks());
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  teclado_desenhar(SDL_GetTicks());
  SDL_GL_SwapWindow(janela);
}

static void captura(const char *saida, const char *sufixo) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  char nome[600];
  int i, y;
  assert(pix);
  for (i = 0; i < 45; i++) quadro();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
           1920 * 4);
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, sufixo);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

// Fecha sem animacao residual: a proxima abertura parte do zero.
static void fechar(void) {
  int i;
  tecla(SDLK_ESCAPE);
  for (i = 0; i < 60; i++) quadro();
  (void)teclado_resultado();
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-teclado";
  const char *dir = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  int falhou = 0;
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: revisao do teclado", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, 1920, 1080,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(96);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(dir);
  ajustes_iniciar();

  // 1. PROVA DE ALCANCE, sem supor a geometria da grade: para cada fileira f,
  // reabre a modal, desce f vezes, encosta a esquerda e aperta OK em cada
  // coluna ate a borda direita, anotando todo simbolo que entrou no texto.
  // Reabrir por fileira e o que mantem o texto abaixo do teto (64 < 73) e o
  // foco longe de "pronto". Descer alem do fim e de proposito: o foco tem de
  // parar na fileira de apagar/limpar/pronto, que nao digita nada.
  { int f, c, k;
    char visto[256] = { 0 };
    const char *p;
    for (f = 0; f < 20; f++) {
      teclado_abrir_com("Usuário Xtream", "", 64, XT, NULL);
      teclas(SDLK_DOWN, f);
      teclas(SDLK_LEFT, 30);
      for (c = 0; c < 30; c++) {
        int antes = (int)strlen(teclado_texto());
        tecla(SDLK_RETURN);
        k = (int)strlen(teclado_texto());
        if (k > antes) visto[(unsigned char)teclado_texto()[k - 1]] = 1;
        tecla(SDLK_RIGHT);
      }
      fechar();
    }
    for (p = XT; *p; p++)
      if (!visto[(unsigned char)*p]) {
        printf("FALHA: simbolo '%c' inalcancavel no teclado Xtream\n", *p);
        falhou = 1;
      }
    // Sem sair cedo: as capturas servem justamente para ver a falha.
    if (!falhou) puts("ok: os 73 simbolos do teclado Xtream sao alcancaveis");
  }

  teclado_abrir_com("Usuário Xtream", "Como o provedor mandou", 48, XT, "joao2024");
  teclas(SDLK_DOWN, 4);          // foco na fileira dos digitos
  teclas(SDLK_RIGHT, 3);
  captura(saida, "xtream");
  fechar();

  // 2. Os curtos: o layout de 6 colunas NAO pode mudar.
  teclado_abrir_com("Portal Stalker (MAC)", "Endereço e porta, sem http://", 48,
                    PORTAL, "meu-portal.tv:8080");
  captura(saida, "portal");
  fechar();
  teclado_abrir_com("MAC do portal", "Formato 00:1a:79:xx:xx:xx", 17, MAC, NULL);
  captura(saida, "mac");
  fechar();
  teclado_abrir("Código do amigo", "Peça o código que aparece na tela dele", 6);
  captura(saida, "padrao");
  fechar();
  if (falhou) { puts("FAIL: capturas gravadas, mas ha simbolo inalcancavel."); return 1; }
  puts("PASS: capturas do teclado gravadas.");
  return 0;
}
