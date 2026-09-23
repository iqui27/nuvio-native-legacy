// PONTEIRO (issue #99): hit-test, camadas, hover, clique = OK, rodinha,
// seta que esconde, avisos 484/485 do webOS e o freio do conteudo que anda.
//
//   bash tests/ponteiro.sh
//
// Compila so src/ponteiro.c. O desenho do cursor usa gfx_cor e o recorte, que
// aqui sao de mentira: o que se testa e a decisao (qual alvo, qual tecla), nao
// o pixel. O relogio e a janela sao injetados para o teste nao depender de SDL
// de video nem de tempo real.
#include "ponteiro.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>

float gfx_opacidade_grupo = 1.0f;
void gfx_sem_recorte(void) {}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}

static int falhas;
#define CONFERE(c, msg) do { if (!(c)) { printf("FALHA: %s (linha %d)\n", msg, __LINE__); falhas++; } } while (0)

static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }

// Teclas que o ponteiro entregou.
static SDL_Event entregues[64];
static int nEntregues;
static void entregar(const SDL_Event *e) { if (nEntregues < 64) entregues[nEntregues++] = *e; }
static void zerar(void) { nEntregues = 0; }

static int focoA = -1, focoB = -1, nFocar;
static void focar(int a, int b) { focoA = a; focoB = b; nFocar++; }
static int ativA = -1, nAtivar;
static void ativar(int a, int b) { (void)b; ativA = a; nAtivar++; }

static void mover(int x, int y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y;
  CONFERE(ponteiro_evento(&e, entregar) == 1, "movimento e consumido");
}
static void botao(Uint32 tipo, int x, int y, int b) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo; e.button.button = (Uint8)b; e.button.x = x; e.button.y = y;
  CONFERE(ponteiro_evento(&e, entregar) == 1, "botao e consumido");
}
static int tecla(Uint32 tipo, SDL_Keycode k, int sc) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo; e.key.keysym.sym = k; e.key.keysym.scancode = (SDL_Scancode)sc;
  return ponteiro_evento(&e, entregar);
}

// Um quadro: o que `desenhar` registra passa a valer para os eventos seguintes.
static void quadro(void (*desenhar)(void)) {
  ponteiro_quadro(relogio);
  if (desenhar) desenhar();
  ponteiro_desenhar();
  relogio += 16;
}

// Duas fileiras de tres cards, 200x100, com 20 de vao. (a, b) = (fileira, coluna).
static float rolagemY = 0.0f;
static int nFileiras = 2;
static void home(void) {
  for (int r = 0; r < nFileiras; r++)
    for (int c = 0; c < 3; c++)
      ponteiro_alvo(100 + c * 220.0f, 100 + r * 120.0f - rolagemY, 200, 100,
                    focar, NULL, r, c);
}
// Uma folha por cima: fundo que fecha + dois itens.
static void homeComFolha(void) {
  home();
  ponteiro_camada();
  ponteiro_alvo(0, 0, 1920, 1080, NULL, ativar, 99, 0);
  ponteiro_alvo(1500, 100, 300, 80, focar, NULL, 7, 0);
  ponteiro_alvo(1500, 200, 300, 80, focar, NULL, 7, 1);
}
static void homeComModalSemAlvo(void) { home(); ponteiro_camada(); }

int main(void) {
  ponteiro_teste_relogio(agora);
  ponteiro_teste_janela(1920, 1080);
  ponteiro_iniciar();

  // --- hit-test puro --------------------------------------------------------
  { PonteiroAlvo v[3] = {
      { 0, 0, 100, 100, NULL, NULL, 0, 0 },
      { 50, 50, 100, 100, NULL, NULL, 1, 0 },   // por cima do primeiro
      { 300, 0, 10, 10, NULL, NULL, 2, 0 } };
    CONFERE(ponteiro_achar(v, 3, 10, 10) == 0, "so o primeiro contem");
    CONFERE(ponteiro_achar(v, 3, 60, 60) == 1, "sobreposicao: o ultimo registrado ganha");
    CONFERE(ponteiro_achar(v, 3, 200, 200) == -1, "fora de tudo");
    CONFERE(ponteiro_achar(v, 3, 100, 10) == -1, "borda direita e exclusiva");
    CONFERE(ponteiro_achar(v, 3, 305, 5) == 2, "alvo pequeno"); }

  // --- custo zero sem ponteiro ---------------------------------------------
  CONFERE(!ponteiro_ativo(), "nasce escondido");
  quadro(home);
  zerar();
  // Clique sem nunca ter mexido: a lista esta vazia (ninguem registrou com o
  // cursor escondido), entao o clique cai no OK puro — o que o OK do controle
  // faria no item em foco.
  botao(SDL_MOUSEBUTTONDOWN, 5, 5, SDL_BUTTON_LEFT);
  botao(SDL_MOUSEBUTTONUP, 5, 5, SDL_BUTTON_LEFT);
  CONFERE(nEntregues == 2 && entregues[0].type == SDL_KEYDOWN &&
          entregues[0].key.keysym.sym == SDLK_RETURN && entregues[1].type == SDL_KEYUP,
          "clique com lista vazia = OK puro");
  relogio += 500;

  // --- hover foca -----------------------------------------------------------
  quadro(home); quadro(home);
  nFocar = 0;
  mover(110, 110);                      // fileira 0, coluna 0
  CONFERE(focoA == 0 && focoB == 0 && nFocar == 1, "hover foca o card sob o cursor");
  mover(120, 115);
  CONFERE(nFocar == 1, "mexer dentro do mesmo alvo nao refoca");
  quadro(home);
  relogio += 200;
  mover(560, 110);                      // coluna 2
  CONFERE(focoA == 0 && focoB == 2, "hover troca de coluna");
  mover(310, 95);                       // no vao entre as fileiras? (y<100) nada
  CONFERE(focoB == 2, "vao nao desfoca");

  // --- o conteudo que anda nao arrasta o hover -----------------------------
  // Tres fileiras. Foca a fileira 1; a "pagina" rola 120 para cima (como a
  // home faz ao focar uma fileira) e a fileira 2 sobe para debaixo do cursor
  // parado. O tremor seguinte da mao NAO pode foca-la — senao a pagina desce
  // sozinha. Assentado, mexer de novo foca, porque ai e a pessoa.
  nFileiras = 3;
  quadro(home); quadro(home);
  relogio += 200;
  mover(110, 230);
  CONFERE(focoA == 1 && focoB == 0, "hover na fileira 1");
  rolagemY = 60.0f;  quadro(home);      // rolando
  rolagemY = 120.0f; quadro(home);      // a fileira 2 esta sob o cursor
  mover(111, 231);
  CONFERE(focoA == 1, "durante a rolagem o hover nao troca de fileira");
  quadro(home); relogio += 200; quadro(home);   // assentou
  mover(112, 232);
  CONFERE(focoA == 2, "assentado, o movimento seguinte foca o que esta sob o cursor");
  nFileiras = 2;
  rolagemY = 0.0f; quadro(home); relogio += 300; quadro(home); quadro(home);

  // --- clique = foco + OK, long press sai da duracao -----------------------
  zerar(); nFocar = 0;
  relogio += 300;
  mover(340, 230);                      // fileira 1, coluna 1
  botao(SDL_MOUSEBUTTONDOWN, 340, 230, SDL_BUTTON_LEFT);
  CONFERE(focoA == 1 && focoB == 1, "clique foca antes do OK");
  CONFERE(nEntregues == 1 && entregues[0].type == SDL_KEYDOWN &&
          entregues[0].key.keysym.sym == SDLK_RETURN, "botao desce = KEYDOWN RETURN");
  relogio += 800;
  botao(SDL_MOUSEBUTTONUP, 340, 230, SDL_BUTTON_LEFT);
  CONFERE(nEntregues == 2 && entregues[1].type == SDL_KEYUP &&
          entregues[1].key.keysym.sym == SDLK_RETURN, "botao sobe = KEYUP RETURN (duracao = segurar)");

  // Clique no vazio de uma tela COM alvos nao dispara nada.
  quadro(home); zerar();
  relogio += 300;
  botao(SDL_MOUSEBUTTONDOWN, 1800, 900, SDL_BUTTON_LEFT);
  botao(SDL_MOUSEBUTTONUP, 1800, 900, SDL_BUTTON_LEFT);
  CONFERE(nEntregues == 0, "clique no vazio nao e OK");

  // --- dedupe: OK de tecla colado no clique vale uma vez -------------------
  quadro(home); zerar();
  relogio += 300;
  botao(SDL_MOUSEBUTTONDOWN, 110, 110, SDL_BUTTON_LEFT);
  CONFERE(tecla(SDL_KEYDOWN, SDLK_RETURN, 0) == 1, "RETURN logo apos o clique e engolido");
  botao(SDL_MOUSEBUTTONUP, 110, 110, SDL_BUTTON_LEFT);
  CONFERE(tecla(SDL_KEYUP, SDLK_RETURN, 0) == 1, "o KEYUP dele tambem");
  CONFERE(nEntregues == 2, "so o par do clique chegou");
  relogio += 300; zerar();
  CONFERE(tecla(SDL_KEYDOWN, SDLK_RETURN, 0) == 0, "RETURN sozinho passa");
  relogio += 20;
  botao(SDL_MOUSEBUTTONDOWN, 110, 110, SDL_BUTTON_LEFT);
  botao(SDL_MOUSEBUTTONUP, 110, 110, SDL_BUTTON_LEFT);
  CONFERE(nEntregues == 0, "clique colado num RETURN de tecla e engolido");
  CONFERE(tecla(SDL_KEYUP, SDLK_RETURN, 0) == 0, "KEYUP do RETURN de tecla passa");

  // --- camada: folha por cima ----------------------------------------------
  relogio += 300;
  quadro(homeComFolha); quadro(homeComFolha);
  relogio += 200;                       // o card focado sumiu: um freio curto
  nFocar = 0;
  mover(110, 110);
  CONFERE(nFocar == 0, "card da home por baixo da folha nao foca");
  mover(1600, 120);
  CONFERE(focoA == 7 && focoB == 0, "item da folha foca");
  zerar(); nAtivar = 0;
  relogio += 300;
  botao(SDL_MOUSEBUTTONDOWN, 400, 700, SDL_BUTTON_LEFT);
  CONFERE(nAtivar == 0, "ativar espera o botao subir");
  botao(SDL_MOUSEBUTTONUP, 400, 700, SDL_BUTTON_LEFT);
  CONFERE(nAtivar == 1 && ativA == 99 && nEntregues == 0,
          "fundo da folha: ativar proprio, sem OK");

  // Modal que nao registra nada: o clique vira OK puro, para ninguem ficar preso.
  relogio += 300;
  quadro(homeComModalSemAlvo); quadro(homeComModalSemAlvo);
  zerar();
  mover(110, 110);
  botao(SDL_MOUSEBUTTONDOWN, 110, 110, SDL_BUTTON_LEFT);
  botao(SDL_MOUSEBUTTONUP, 110, 110, SDL_BUTTON_LEFT);
  CONFERE(nEntregues == 2 && entregues[0].key.keysym.sym == SDLK_RETURN,
          "modal sem alvo: clique = OK");

  // --- rodinha vira seta, com freio ---------------------------------------
  zerar();
  relogio += 300;
  { SDL_Event e; SDL_zero(e);
    e.type = SDL_MOUSEWHEEL; e.wheel.y = -1;
    ponteiro_evento(&e, entregar);
    ponteiro_evento(&e, entregar);          // mesmo instante: freado
    CONFERE(nEntregues == 2 && entregues[0].key.keysym.sym == SDLK_DOWN,
            "rodinha para baixo = uma seta para baixo");
    relogio += 100;
    e.wheel.y = 1; ponteiro_evento(&e, entregar);
    CONFERE(nEntregues == 4 && entregues[2].key.keysym.sym == SDLK_UP, "rodinha para cima");
    relogio += 100;
    e.wheel.y = 0; e.wheel.x = 1; ponteiro_evento(&e, entregar);
    CONFERE(nEntregues == 6 && entregues[4].key.keysym.sym == SDLK_RIGHT, "rodinha lateral"); }
  CONFERE(ponteiro_ativo(), "rodinha nao esconde o cursor");

  // --- seta esconde; 484/485 sao do ponteiro ------------------------------
  CONFERE(tecla(SDL_KEYDOWN, SDLK_DOWN, 0) == 0, "a seta segue para a tela");
  CONFERE(!ponteiro_ativo(), "seta esconde o cursor");
  quadro(home);
  zerar();
  CONFERE(tecla(SDL_KEYDOWN, 0, 484) == 1, "484 (cursor mostrou) e consumido");
  CONFERE(ponteiro_ativo(), "484 mostra");
  CONFERE(tecla(SDL_KEYUP, 0, 484) == 1, "KEYUP do 484 tambem");
  CONFERE(tecla(SDL_KEYDOWN, 0, 485) == 1, "485 (cursor escondeu) e consumido");
  CONFERE(!ponteiro_ativo(), "485 esconde");
  CONFERE(nEntregues == 0, "nenhum aviso vira tecla");

  // --- dorme parado --------------------------------------------------------
  mover(110, 110);
  CONFERE(ponteiro_ativo(), "movimento acorda");
  relogio += 5000;
  quadro(home);
  CONFERE(!ponteiro_ativo(), "parado 5 s, dorme");

  // --- conversao janela -> logico -----------------------------------------
  ponteiro_teste_janela(960, 540);
  mover(480, 270);
  CONFERE(ponteiro_x() > 959 && ponteiro_x() < 961 && ponteiro_y() > 539 && ponteiro_y() < 541,
          "janela de metade: coordenada dobra");

  // --- botao direito = Voltar (Mac) ---------------------------------------
  zerar();
  botao(SDL_MOUSEBUTTONDOWN, 10, 10, SDL_BUTTON_RIGHT);
  botao(SDL_MOUSEBUTTONUP, 10, 10, SDL_BUTTON_RIGHT);
  CONFERE(nEntregues == 2 && entregues[0].key.keysym.sym == SDLK_AC_BACK, "direito = Voltar");

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("ponteiro: ok\n");
  return 0;
}
