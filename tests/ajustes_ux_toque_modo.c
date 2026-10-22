// Mobile targets/editing must not change TV state, even after a mode switch.
#include "text.h"
#include "gfx.h"
static int medirDica(TxtEstilo estilo, const char *s);
static void recorteTeste(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
static void semRecorteTeste(void) {}
#define txt_largura medirDica
#define gfx_recorte recorteTeste
#define gfx_sem_recorte semRecorteTeste
#include "../src/ajustes.c"
#undef txt_largura
#undef gfx_recorte
#undef gfx_sem_recorte
#include <assert.h>

static int falhas, dicaToque, dicaSetas;
#define CONFERE(c, msg) do { if (!(c)) { puts("FALHA: " msg); falhas++; } } while (0)
static int medirDica(TxtEstilo estilo, const char *s) {
  if (!strcmp(s, "Toque")) dicaToque++;
  if (!strcmp(s, "↑ ↓")) dicaSetas++;
  return txt_largura(estilo, s);
}
static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }
static void entregar(const SDL_Event *e) { (void)e; }
static void quadroFileiras(void) {
  ponteiro_quadro(relogio);
  desenhaFileiras();
  ponteiro_desenhar();
  relogio += 400;
}
static void clique(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
  e.button.x = (int)x; e.button.y = (int)y;
  ponteiro_evento(&e, entregar);
  e.type = SDL_MOUSEBUTTONUP; relogio += 60;
  ponteiro_evento(&e, entregar);
}
static void fileiras(int mobile) {
  const PonteiroAlvo *v;
  layout_modo_definir(mobile);
  fil_esquecer();
  fil_definir_perfil(mobile + 1);
  fil_registrar("test_a", "A", "Test", "movie", 1);
  fil_registrar("test_b", "B", "Test", "movie", 1);
  assert(fil_n() == 2 && fil_estado(0) == FIL_NA_HOME && fil_estado(1) == FIL_NA_HOME);
  filAberta = 1; filAba = 0; filNaBarra = 0;
  filFoco = 1; filCampo = 0; filPegou = 0; filTopo = 0;
  eventoFileiras(SDLK_RETURN);  // remote OK picks A in either mode
  assert(filPegou == 1);
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  dicaToque = dicaSetas = 0;
  quadroFileiras();
  AjFilMedidas m = ajFilMedir();
  float x = m.cx + 10, y = m.ilha.y + 183 + ajFilToqueTopo(2) + AJF_ROW_H * 0.5f;
  int n = ponteiro_teste_lista(&v);
  if (mobile) {
    CONFERE(n > 0, "Mobile: registra alvos de Fileiras");
    CONFERE(dicaToque && !dicaSetas, "Mobile: conserva dicas de toque");
  } else {
    CONFERE(n == 0, "TV: Fileiras nao registra alvos Mobile");
    CONFERE(!dicaToque && dicaSetas, "TV: conserva dicas do controle");
  }
  if (mobile) {
    int alvo = ponteiro_achar(v, n, x, y);
    assert(alvo >= 0 && v[alvo].a == 2 && v[alvo].ativar == ajFilToqueSoltar);
  }
  clique(x, y);
  if (mobile) {
    CONFERE(!strcmp(fil_chave(0), "test_b") && !strcmp(fil_chave(1), "test_a") && filPegou == 0,
            "Mobile: clicar B move A e solta");
    eventoFileiras(SDLK_RETURN);
    ponteiro_iniciar(); ponteiro_teste_toque(1);
    quadroFileiras();             // targets left over from Mobile
    layout_modo_definir(0);
    clique(x, y);
    CONFERE(!strcmp(fil_chave(0), "test_b") && filPegou == 1,
            "TV: alvo remanescente de Mobile nao solta nem reordena");
  } else {
    CONFERE(!strcmp(fil_chave(0), "test_a") && !strcmp(fil_chave(1), "test_b") && filPegou == 1,
            "TV: clicar B nao reordena nem solta A");
    ajFilToqueSoltar(2, 0);
    CONFERE(!strcmp(fil_chave(0), "test_a") && filPegou == 1, "TV: callback de soltar e inerte");
  }
  filPegou = 0; filFoco = 1; filCampo = 0;
  layout_modo_definir(0);
  ajFilToqueFocar(2, 3); ajFilToqueAba(1, 0);
  CONFERE(filFoco == 1 && filCampo == 0 && filAba == 0, "TV: callbacks de foco e aba sao inertes");
}

int main(void) {
  Aj2Pose p = {0};
  p.lA = 1.0f; p.lS = 1.0f;
  uxIndice = 2; uxEditor = 1; uxOp = AJ_TEMA; uxRestaurar = 0; uxAvisoRisco = 0;
  assert(ajEditandoLinha(AJ_TEMA));
  layout_modo_definir(0);
  assert(!ajInlineAtivo(0, &p));
  layout_modo_definir(1);
  assert(ajInlineAtivo(0, &p));
  assert(!ajInlineAtivo(1, &p));   // a modal still wins in Mobile
  puts("PASS: inline editor is Mobile only");
  dados_iniciar(getenv("NUVIO_DADOS"));
  gfx_opacidade_grupo = 0;  // exercise registration and hint measurement, skip pixels
  ajustes_iniciar();
  layout_tela_definir(1920, 1080);
  ponteiro_teste_relogio(agora); ponteiro_teste_janela(1920, 1080);
  fileiras(0); fileiras(1);
  if (!falhas) puts("PASS: Fileiras targets, callbacks and hints are Mobile only");
  return falhas ? 1 : 0;
}
