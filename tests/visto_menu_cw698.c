// #698: "mark watched" in the real episode menu ends that episode's resume.
#include "catalogo.h"
#include "dados.h"
#include "descoberta.h"
#include "episodios.h"
#include "perfis.h"
#include "progresso.h"
#include "rede.h"
#include "vistoep.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SERIES "tt1234567"
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0;
}
static long long agora = 1757000000000LL;
static long long relogio(void) { return agora; }
static CatEp eps[8];

static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k;
  episodios_menu_evento(&ev);
}
// First action toggles this episode; `descer` reaches "up to here".
static void menu(int e, int descer) {
  agora += 1000;
  // The menu's mark republishes the catalog: find the title by id, like the app.
  while (desc_montando()) SDL_Delay(5);
  int idx = cat_indice_por_imdb(SERIES);
  assert(idx >= 0);
  cat_definir_episodios(idx, eps, 8);
  episodios_menu_visto(idx, 2, e, "Episode");
  assert(episodios_menu_aberto());
  while (descer-- > 0) tecla(SDLK_DOWN);
  tecla(SDLK_RETURN); // apply
  tecla(SDLK_RETURN); // dismiss the confirmation
}
static void gravar(int e, double pos) {
  agora += 1000;
  assert(prog_gravar_local(SERIES, 2, e, pos, 3000, 1));
}
static int emResume(int e) {
  ProgRegistro r;
  return prog_continuar_de_perfil(1, 90, &r) && r.temporada == 2 && r.episodio == e;
}

int main(void) {
  alarm(30);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); perfis_definir_ativo(1);
  prog_definir_relogio(relogio);
  CatItem v[1] = {0}; CatFileira row[1] = {0};
  strcpy(v[0].imdb, SERIES); strcpy(v[0].tipo, "series");
  strcpy(row[0].chave, "series_top"); row[0].n = 1;
  cat_definir_tudo(v, 1, row, 1);
  for (int i = 0; i < 8; i++) { eps[i].temporada = 2; eps[i].episodio = i + 1; }
  ProgRegistro r;

  // Unfinished episode marked watched: no resume, and the old cloud row loses.
  gravar(6, 1200);
  assert(emResume(6));
  menu(6, 0);
  assert(vistoep_estado(SERIES, 2, 6) == 1);
  assert(!emResume(6));
  assert(prog_por_chave(SERIES "_s2e6", &r) && r.posSeg == 3000 && r.pendente);
  ProgRegistro velho = r;
  velho.posSeg = 1200; velho.lastWatchedMs = agora + 1; velho.pendente = 0;
  assert(!prog_aplicar_remoto(&velho));
  assert(!emResume(6));
  puts("ok  marking an unfinished episode ends its resume");

  // The report's stuck state: already watched, resume row back from elsewhere.
  gravar(6, 1200);
  vistoep_definir(SERIES, 2, 6, 1);
  menu(6, 0); // unmark invents and erases nothing
  assert(vistoep_estado(SERIES, 2, 6) == 0);
  assert(emResume(6));
  menu(6, 0);
  assert(vistoep_estado(SERIES, 2, 6) == 1);
  assert(!emResume(6));
  puts("ok  unmark then mark clears a stuck resume");

  // A batch finishes its own rows, leaves other episodes, invents no rows.
  gravar(4, 900);
  gravar(7, 600);
  menu(5, 1); // up to S2E5
  assert(prog_por_chave(SERIES "_s2e4", &r) && r.posSeg == 3000);
  assert(emResume(7));
  assert(!prog_por_chave(SERIES "_s2e5", &r));
  assert(!prog_por_chave(SERIES "_s2e3", &r));
  puts("ok  batch mark touches only its own unfinished rows");
  puts("PASS #698: episode menu mark ends resume");
  fflush(stdout);
  _exit(0);
}
