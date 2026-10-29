// Estatisticas ao vivo (estatvivoui.h): abrir pela central e os tres tamanhos.
// Sem GL nem player. O bug de #338: o OK que ATIVOU o botao "Estatisticas" (eco da
// tecla, repeticao, clique do Magic Remote) chegava ao cartao ja aberto e o
// fixava no canto na hora — "o live nao abre". Ver tests/estatvivo_ui.sh.
#include "estatvivoui.h"
#include "ajustes.h"
#include "dados.h"
#include "central.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

static int tecla(SDL_Keycode k, int repetida) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.repeat = (Uint8)repetida;
  return estatvivo_evento(&e, 0);
}
static int teclaCima(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return estatvivo_evento(&e, 1);   // outra camada e dona do CIMA neste quadro
}
static void espera(void) { SDL_Delay(650); }   // passa a graca do OK

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char arq[700];
  FILE *f;
  assert(dir && *dir);
  SDL_Init(SDL_INIT_TIMER);
  snprintf(arq, sizeof arq, "%s/ajustes.txt", dir);
  f = fopen(arq, "w"); assert(f);
  fprintf(f, "idioma 0\n");
  fclose(f);
  ajustes_dir(dir);
  dados_iniciar(NULL);

  // 1. O BUG: o OK do botao chega logo depois de abrir (ou repetido) e o cartao
  // tem de ficar de pe, no tamanho que abriu.
  central_teste_estatisticas(1);
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_PAGEUP; e.key.timestamp = 1000;
    CONFERE(central_tecla(&e, 1) == 1, "CH+ consumido");
    e.type = SDL_KEYUP; e.key.timestamp = 1100;
    central_tecla(&e, 1); central_tecla_quadro(1200, NULL); // prova KEYUP real
    e.type = SDL_KEYDOWN; e.key.timestamp = 2000;
    central_tecla(&e, 1); central_tecla_quadro(2600, NULL);
    CONFERE(central_aberta(), "CH+ segurado abre a central da ilha");
    e.type = SDL_KEYUP; e.key.timestamp = 2700;
    central_tecla(&e, 1); central_tecla_quadro(2800, NULL);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    central_evento(&e);
    CONFERE(!central_aberta(), "Estatisticas fecha a central");
  }
  CONFERE(estatvivo_aberto() && !estatvivo_fixado(), "abriu pela central");
  CONFERE(tecla(SDLK_RETURN, 0) == 1, "o OK do eco e consumido");
  CONFERE(estatvivo_aberto() && !estatvivo_fixado(), "o OK do eco nao fixa o cartao no canto");
  estatvivo_abrir();   // o eco tambem pode reabrir
  CONFERE(estatvivo_aberto() && !estatvivo_fixado(), "reabrir aberto nao muda nada");
  espera();
  CONFERE(tecla(SDLK_RETURN, 1) == 1 && estatvivo_aberto() && !estatvivo_fixado(),
          "OK repetido (segurado) nao anda");
  CONFERE(tecla(SDLK_RIGHT, 1) == 1 && estatvivo_aberto() && !estatvivo_fixado(),
          "seta repetida nao anda");

  // 2. OK de verdade depois da graca: Completo -> Compacto -> Minimo.
  CONFERE(estatvivo_aberto(), "ainda aberto");
  tecla(SDLK_RETURN, 0);
#ifndef ANTES
  CONFERE(estatvivo_aberto() && estatvivo_tamanho() == EV_TAM_COMPACTO, "OK: Completo -> Compacto");
  tecla(SDLK_RETURN, 0);
  CONFERE(estatvivo_aberto() && estatvivo_tamanho() == EV_TAM_MINIMO, "OK: Compacto -> Minimo");
  tecla(SDLK_RETURN, 0);
  CONFERE(estatvivo_tamanho() == EV_TAM_COMPLETO, "OK: Minimo -> Completo");

  // 3. Esquerda/Direita trocam o LADO (e ficam nos Ajustes), nao o tamanho.
  CONFERE(!ajustes_estat_lado(), "o lado padrao e a direita");
  tecla(SDLK_LEFT, 0);
  CONFERE(ajustes_estat_lado() == 1 && estatvivo_tamanho() == EV_TAM_COMPLETO, "Esquerda: lado esquerdo, mesmo tamanho");
  tecla(SDLK_RIGHT, 0);
  CONFERE(ajustes_estat_lado() == 0 && estatvivo_tamanho() == EV_TAM_COMPLETO, "Direita: lado direito, mesmo tamanho");
  tecla(SDLK_RETURN, 0); tecla(SDLK_RETURN, 0);
  CONFERE(estatvivo_tamanho() == EV_TAM_MINIMO, "Minimo");

  // 4. Baixo fixa NO TAMANHO ESCOLHIDO (nao vira a linha fina) e nao rouba tecla.
  tecla(SDLK_DOWN, 0);
  CONFERE(!estatvivo_aberto() && estatvivo_fixado() && estatvivo_tamanho() == EV_TAM_MINIMO, "Baixo fixa no tamanho escolhido");
  CONFERE(tecla(SDLK_RIGHT, 0) == 0 && tecla(SDLK_RETURN, 0) == 0, "fixado nao rouba tecla");
  estatvivo_abrir(); espera();
  CONFERE(estatvivo_aberto() && !estatvivo_fixado() && estatvivo_tamanho() == EV_TAM_MINIMO, "central desafixa");
  tecla(SDLK_RETURN, 0);   // Completo
  tecla(SDLK_DOWN, 0);
  CONFERE(estatvivo_fixado() && estatvivo_tamanho() == EV_TAM_COMPLETO, "fixa o Completo tambem");
  // P2: com uma camada de pe (cimaDela), o CIMA que focaria o fixado e dela;
  // as demais teclas continuam do player (fixado nao rouba nada).
  CONFERE(teclaCima(SDLK_UP) == 0 && !estatvivo_aberto() && estatvivo_fixado(),
          "CIMA com camada de pe fica dela, nao foca o fixado");
  CONFERE(teclaCima(SDLK_LEFT) == 0 && teclaCima(SDLK_RETURN) == 0,
          "as outras teclas passam do mesmo jeito");
  CONFERE(tecla(SDLK_UP, 0) == 1 && estatvivo_aberto() && estatvivo_fixado(),
          "sem camada o CIMA volta a focar o fixado");
  tecla(SDLK_ESCAPE, 0);   // devolve o foco (fixado segue)
  estatvivo_abrir(); espera();

  // Lembrar o ultimo: fechar e abrir de novo reabre no mesmo tamanho.
  estatvivo_fechar();
  estatvivo_abrir();
  CONFERE(estatvivo_tamanho() == EV_TAM_COMPLETO && estatvivo_aberto(), "reabre no ultimo tamanho");
  espera();
  tecla(SDLK_ESCAPE, 0);
  CONFERE(!estatvivo_aberto() && !estatvivo_fixado(), "Voltar fecha");

  // 5. O tamanho dos Ajustes manda sobre o ultimo usado.
  f = fopen(arq, "w"); assert(f);
  fprintf(f, "idioma 0\nestatTamanhoLocal 2\n");
  fclose(f);
  ajustes_dir(dir);
  CONFERE(ajustes_estat_tamanho() == EV_TAM_COMPACTO, "Ajustes: Compacto (%d)", ajustes_estat_tamanho());
  estatvivo_abrir();
  CONFERE(estatvivo_aberto() && estatvivo_tamanho() == EV_TAM_COMPACTO, "abre no tamanho dos Ajustes");
  estatvivo_fechar();
#endif
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  puts("PASS: estatisticas ao vivo (abrir e tamanhos)");
  return 0;
}
