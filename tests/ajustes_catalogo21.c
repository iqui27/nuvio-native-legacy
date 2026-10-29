// No option lost (2.1): every key the integracao/2.1 catalog could reach (its
// TELA and the Pencil pages) still has a route. A route is a TELA row that some
// platform shows and that Down reaches with "Mais opcoes" open, or the parent
// sheet that really opens the sub-editor. Removals are listed with the reason;
// the shell proves nothing outside Settings reads them.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include "cat21.h"
#include <assert.h>
#include <unistd.h>

#define N(a) ((int)(sizeof a / sizeof *a))
// Rows that exist in TELA but only some builds show (foraDestaPlataforma #ifdef).
static const int PLATAFORMA[] = {
  AJ_INTERFACE_MODO,                                  // touch builds
  AJ_TECLADO_ANDROID,                                 // system keyboard builds
  AJ_DV7, AJ_HDR10P_STRIP,                            // webOS / DV7 conversion builds
  AJ_TV_HOME, AJ_PLAYER_EXTERNO, AJ_PLAYER_ANIME,     // Android (#501)
  AJ_FONTE_PRECARREGAR, AJ_LEG_SYNC_AUDIO, AJ_LIMPAR_CACHE, AJ_TUNNELING, AJ_AFR, // Android
  AJ_TRAILER_AUDIO,                                   // web (YouTube id)
  AJ_TRAILER_ZOOM_TPK, AJ_AUDIO_OFFSET,               // .tpk
  AJ_MIC_BUSCA,                                       // .tpk 6+ and .wgt
};
// Sub-editors: no row of their own, opened from the parent sheet (checked below).
static const int COMPOSTO[] = {
  AJ_FALHA_RD_LIMITE, AJ_FALHA_TB_LIMITE, AJ_FALHA_PM_LIMITE, AJ_FALHA_AD_LIMITE,
  AJ_FILTRO_TAM_MAX, AJ_FILTRO_GRUPOS,
};
// Removed features: the value is kept for sync/migration, nothing reads it.
static const int REMOVIDA[] = {
  AJ_DESCOBRIR,        // the web Discover screen does not exist on the TV
  AJ_DET_TRAILER_SOM,  // #509: one sound switch (AJ_HERO_TRAILER_SOM), migration only
  AJ_RELOGIO_POS,      // 7d69643a: the clock island is always top right
  AJ_HDR_INTERFACE,    // c09e7470a: experimental HDR menus removed, getter returns 0
  // 2.0.5 (owner, 12/10): depth is one switch plus one intensity. The per-place
  // and edge keys stay for sync; no renderer reads them (only the 2.1 Pencil
  // page listed them, as switches that changed nothing).
  AJ_PROF_BORDA, AJ_PROF_COBERTURA, AJ_PROF_POSTERS, AJ_PROF_CW,
  AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS,
};
static int naFonte(const char *nome) {
  for (int i = 0; i < N(TELA_FONTE); i++) if (!strcmp(TELA_FONTE[i], nome)) return 1;
  return 0;
}
static int em(const int *v, int n, int op) { for (int i = 0; i < n; i++) if (v[i] == op) return 1; return 0; }
static int linha(int op) {
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return i;
  return -1;
}
static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e);
}
static unsigned char visto[AJ_N];
static void percorrer(void) {
  for (int s = 0; s < nSecoes; s++) {
    triEntrar(s); maisAberto[s] = 1; focar(primeiroDaSecao(s));
    for (int passos = 0; passos <= AJ_N_TELA; passos++) {
      int item = focoItem;
      if (focoOp >= 0) visto[focoOp] = 1;
      key(SDLK_DOWN);
      if (focoItem == item) break;
    }
    key(SDLK_ESCAPE);
  }
}

int main(void) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/nuvio-catalogo21-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); player_dir(dir); fontefalha_dir(dir);
  ajustes_iniciar(); assert(gravar());
  percorrer();

  int falhas = 0, rotas = 0;
  for (int c = 0; c < N(CAT21); c++) {
    int op = CAT21[c], i = linha(op);
    const char *porque = NULL;
    if (em(REMOVIDA, N(REMOVIDA), op) || em(COMPOSTO, N(COMPOSTO), op)) continue;
    if (i < 0 || foraDestaPlataforma(op)) {
      if (!em(PLATAFORMA, N(PLATAFORMA), op)) porque = "no row on any platform";
      else if (!naFonte(CAT21_NOME[c])) porque = "platform row missing from ajustes_ux_tela.inc";
    } else if (visivel(i) && !visto[op]) porque = "visible row that Down never reaches";
    if (porque) { printf("FAIL %s: %s\n", CAT21_NOME[c], porque); falhas++; }
    else rotas++;
  }

  // Composite editors: the parent row is routed and its sheet opens each child.
  int filhos = 0;
  assert(linha(AJ_FILTROS_FONTES) >= 0 && visto[AJ_FILTROS_FONTES]);
  ffAbrir(); assert(frAberta == 6);
  ffFoco = 2; ffEscolher(); assert(uxEditor && uxOp == AJ_FILTRO_TAM_MAX); filhos++;
  uxCancelar(); ffAbrir(); ffFoco = 8; ffEscolher();
  assert(teclado_aberto() && stCampo == AJ_FILTRO_GRUPOS); filhos++;
  { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; teclado_evento(&e); }
  frAberta = 0;
  assert(linha(AJ_FALHA_PROVEDOR_ORDEM) >= 0 && visto[AJ_FALHA_PROVEDOR_ORDEM]);
  for (int s = 0; s < 4; s++) {
    faAbrir(1); assert(frAberta == 4 && faN == 4);
    int achou = 0;
    for (int j = 0; j < faN; j++)
      if (faItem[j].servico == s) { faAbrirLimite(j); achou = 1; break; }
    assert(achou && uxEditor && uxOp == AJ_FALHA_RD_LIMITE + s); filhos++;
    uxCancelar(); frAberta = 0;
  }
  assert(filhos == N(COMPOSTO));

  // Fifth review: the eight MDBList sources and the advanced switch are rows
  // again, reached by Down; the switch makes "Mais opcoes" unnecessary.
  for (int op = AJ_MDB_TRAKT; op <= AJ_MDB_MAL; op++)
    assert(linha(op) >= 0 && uxAvancada(op) && visto[op]);
  assert(linha(AJ_AVANCADAS) >= 0 && !uxAvancada(AJ_AVANCADAS) && visto[AJ_AVANCADAS]);
  memset(maisAberto, 0, sizeof maisAberto);
  assert(!visivel(linha(AJ_MDB_MAL)));
  focarOpcao(AJ_AVANCADAS); key(SDLK_RETURN); assert(lig(AJ_AVANCADAS));
  assert(visivel(linha(AJ_MDB_MAL)));
  for (int i = 0; i < AJ_N_TELA; i++) assert(TELA[i].tipo != IT_MAIS || !visivel(i));
  key(SDLK_RETURN); assert(!lig(AJ_AVANCADAS));

  if (falhas) { printf("FAIL: %d of %d catalog options have no route\n", falhas, N(CAT21)); return 1; }
  printf("PASS: %d catalog options routed, %d sub-editors opened, %d removals documented\n",
         rotas, filhos, N(REMOVIDA));
  return 0;
}
