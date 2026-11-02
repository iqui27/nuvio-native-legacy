// Palco blade stack contract (2.1, mockup claude-c): depth is read from state,
// every block opens as its own blade, Back restores the opener row, values
// change only on OK, and the stage sits where the mock puts it at each depth.
// Isolated storage, no window, no network.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e);
}
static void settle(void) { for (int i = 0; i < 240; i++) ajustes_atualizar(1.0f / 60.0f, 0); }
static int near(float a, float b) { return fabsf(a - b) < 1.5f; }
static int block(int sec, const char *title) {
  for (int i = secIni[sec] + 1; i < secFim(sec); i++)
    if (TELA[i].tipo == IT_ROT && !strcmp(TELA[i].titulo, title)) return i;
  return -1;
}
// The stage is centred with its caption: top = max(112, 578 - (h + 270) / 2 - 10).
static void stage(float x, float w, float h) {
  GfxRect p = triPalcoR();
  assert(near(p.x, x) && near(p.w, w) && near(p.h, h));
  assert(near(p.y, fmaxf(112.0f, 578.0f - (h + 270.0f) * 0.5f - 10.0f)));
}

int main(void) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-blades-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); player_dir(dir); fontefalha_dir(dir);
  ajustes_iniciar(); assert(gravar());
  assert(triBladeLayout());
  float x0 = triX0(), xf = triXFim();

  // Index: no blade at all, the wide stage (476 x 1396 x 640) under the chips.
  triIrSecoes(); settle();
  assert(triProfundidade() == 0 && near(triSecR().w, 380.0f));
  stage(x0 + 428.0f, xf - x0 - 428.0f, 640.0f);

  // Section (depth 1): every block is a drill row, even the two-option ones.
  triEntrar(AJS_APARENCIA); settle();
  assert(triProfundidade() == 1 && triIlha == TRI_LISTA);
  int cores = block(AJS_APARENCIA, "Cores e vidro"), fileiras = block(AJS_APARENCIA, "Fileiras");
  assert(cores >= 0 && fileiras >= 0 && triTopico(cores) && triTopico(fileiras));
  stage(x0 + 940.0f, xf - x0 - 940.0f, (xf - x0 - 940.0f) * 9.0f / 16.0f);

  // Open: Right on the block opens its blade (depth 2); the menu condenses to 104.
  focar(cores); key(SDLK_RIGHT); settle();
  assert(triSubmenu == cores && triProfundidade() == 2 && near(triSecR().w, 104.0f));
  stage(x0 + 1192.0f, xf - x0 - 1192.0f, (xf - x0 - 1192.0f) * 9.0f / 16.0f);

  // Depth 3: OK on the accent opens the palette blade. Moving inside it only
  // previews; Back cancels and lands on the opener row.
  focarOpcao(AJ_TEMA);
  int tema = valor[AJ_TEMA];
  key(SDLK_RETURN); settle();
  assert(uxEditor == 1 && uxOp == AJ_TEMA && triProfundidade() == 3);
  key(SDLK_RIGHT); settle();
  assert(valor[AJ_TEMA] == tema && uxPendente != tema);
  key(SDLK_ESCAPE); settle();
  assert(!uxEditor && triProfundidade() == 2 && focoOp == AJ_TEMA && valor[AJ_TEMA] == tema);

  // Back closes the topic: depth 1, focus on the block row that opened it.
  key(SDLK_ESCAPE); settle();
  assert(triSubmenu < 0 && focoItem == cores && triProfundidade() == 1);
  // Back again: the index, on the same section.
  key(SDLK_ESCAPE); settle();
  assert(triProfundidade() == 0 && triIlha == TRI_SECOES && triSecFoco == AJS_APARENCIA);

  // Measuring this two-value choice while editor state is active must not
  // recurse through list geometry, whether this host fits it inline or not.
  focarOpcao(AJ_TELA_ABERTURA);
  assert(nValores(AJ_TELA_ABERTURA) == 2);
  {
    int depth = triProfundidade() + !ajEmLinha(AJ_TELA_ABERTURA);
    uxAbrirEditor(AJ_TELA_ABERTURA);
    assert(uxEditor == 1 && uxOp == AJ_TELA_ABERTURA);
    assert(triProfundidade() == depth);
  }
  uxCancelar();
  triIrSecoes(); settle();

  // Values change only on OK: Right on a switch does nothing, OK flips it.
  triEntrar(AJS_REPRODUCAO);
  int durante = block(AJS_REPRODUCAO, "Durante e depois");
  assert(durante >= 0);
  focar(durante); key(SDLK_RIGHT); assert(triSubmenu == durante);
  int sw = -1;
  for (int i = durante + 1; i < secFim(secAtual) && TELA[i].tipo == IT_OPC; i++)
    if (focavel(i) && ajTipo(TELA[i].op) == AJL_TOG && !inativa(TELA[i].op) && !soLeitura(TELA[i].op)) { sw = i; break; }
  assert(sw >= 0);
  focar(sw);
  int op = focoOp, v = valor[op];
  key(SDLK_RIGHT); assert(valor[op] == v && !uxEditor);
  key(SDLK_RETURN); assert(valor[op] != v);
  key(SDLK_RETURN); assert(valor[op] == v);

  // A utility route (Presets) is one blade deep.
  triUtilAbrir(TRI_U_PRESETS); settle();
  assert(triProfundidade() == 1);
  printf("PASS: Palco blades: depth, drill rows, Back restores focus, OK-only values, stage geometry\n");
  return 0;
}
