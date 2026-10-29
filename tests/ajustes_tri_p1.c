// Three-island Settings, review P1s (2.1): the portrait section rail, pointer
// targets on the utility routes, utility lists that scroll with the focus and
// the parental chooser without the inspector. Hidden GL window, real draw,
// isolated storage, no network. Every check prints PASS/FAIL; exit 1 on FAIL.

#include "../src/ajustes.c"
#include "nvtmp.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <unistd.h>

static int falhas;
static const char *dirPng;   // argv[1]: saves a PNG per scene (review evidence)
static SDL_Window *win;
static int telaW = 1920, telaH = 1080;
static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }
static void confere(int ok, const char *msg) { printf("%s: %s\n", ok ? "PASS" : "FAIL", msg); falhas += !ok; }
static void key(SDL_Keycode k) { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; ajustes_evento(&e); }
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); gfx_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    ponteiro_quadro(relogio);
    ajustes_atualizar(1.0f / 60, relogio); ajustes_desenhar(relogio);
    ponteiro_desenhar(); relogio += 16;
  }
}
static void png(const char *nome) {
  unsigned char *px; SDL_Surface *sf; char path[1024];
  if (!dirPng) return;
  px = malloc((size_t)telaW * telaH * 4); sf = SDL_CreateRGBSurfaceWithFormat(0, telaW, telaH, 32, SDL_PIXELFORMAT_RGBA32);
  assert(px && sf);
  glReadPixels(0, 0, telaW, telaH, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (int y = 0; y < telaH; y++) memcpy((char *)sf->pixels + y * sf->pitch, px + (size_t)(telaH - 1 - y) * telaW * 4, (size_t)telaW * 4);
  snprintf(path, sizeof path, "%s/%s.png", dirPng, nome);
  assert(IMG_SavePNG(sf, path) == 0); SDL_FreeSurface(sf); free(px);
}
// One square hidden window (1920 x 1920): the layout only moves the viewport,
// so portrait draws on a 1080 x 1920 target without resizing the window
// (SDL applies a resize of a hidden window late and the scissor lagged).
static void tela(int w, int h) {
  telaW = w; telaH = h;
  layout_tela_definir(w, h);   // first: gfx_tamanho_alvo reads NV_TELA_W/H for its projection
  glViewport(0, 0, w, h); gfx_tamanho_alvo(w, h); ponteiro_teste_janela(w, h);
}
static void clique(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)x; e.button.y = (int)y;
  ponteiro_evento(&e, ajustes_evento);
  e.type = SDL_MOUSEBUTTONUP; relogio += 60;
  ponteiro_evento(&e, ajustes_evento);
  quadros(8);
}
static void clicar(const PonteiroAlvo *a) { clique(a->x + a->w * 0.5f, a->y + a->h * 0.5f); }
// A finger drag through the pointer (Mobile: captured by the registered scroll region).
static void dedo(Uint32 tipo, float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo; e.tfinger.touchId = 1; e.tfinger.fingerId = 1;
  e.tfinger.x = x / NV_TELA_W; e.tfinger.y = y / NV_TELA_H;
  ponteiro_evento(&e, ajustes_evento);
}
static void arrastar(float x0, float y0, float x1, float y1) {
  dedo(SDL_FINGERDOWN, x0, y0); relogio += 40;
  for (int i = 1; i <= 20; i++) { dedo(SDL_FINGERMOTION, x0 + (x1 - x0) * i / 20.0f, y0 + (y1 - y0) * i / 20.0f); relogio += 40; }
  dedo(SDL_FINGERUP, x1, y1); relogio += 40;
  quadros(40);
}
static const PonteiroAlvo *secao(int a) {
  const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) if ((v[i].focar == triPontSecao || v[i].ativar == triPontSecaoAbrir) && v[i].a == a) return &v[i];
  return NULL;
}
static int visivelEm(const PonteiroAlvo *a, float topo, float base) { return a && a->y >= topo - 1.0f && a->y + a->h <= base + 1.0f; }
// Targets whose centre lies inside a virtual rect of the screen (real = virtual x scale).
static int dentro(const PonteiroAlvo *v, GfxRect r) {
  float s = ajustes_tamanho_ajustes(), cx = v->x + v->w * 0.5f, cy = v->y + v->h * 0.5f;
  return cx >= r.x * s && cx <= (r.x + r.w) * s && cy >= r.y * s && cy <= (r.y + r.h) * s;
}
static int listar(GfxRect r, const PonteiroAlvo **out, int max) {
  const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v), k = 0;
  for (int i = 0; i < n && k < max; i++) if (dentro(&v[i], r)) out[k++] = &v[i];
  // by y then x, so "the 3rd from the top" means something
  for (int i = 1; i < k; i++) for (int j = i; j > 0 && (out[j]->y < out[j-1]->y || (out[j]->y == out[j-1]->y && out[j]->x < out[j-1]->x)); j--) { const PonteiroAlvo *t = out[j]; out[j] = out[j-1]; out[j-1] = t; }
  return k;
}
static const PonteiroAlvo *linha(GfxRect r, int a) {
  const PonteiroAlvo *v[64]; int n = listar(r, v, 64);
  for (int i = 0; i < n; i++) if (v[i]->a == a && v[i]->focar != triPontChip && v[i]->focar != triPontSecao && v[i]->focar != ajPonteiroItem) return v[i];
  return NULL;
}

// Portrait uses a grouped section list and opens each section as a page.
static void retrato(void) {
  const PonteiroAlvo *v; int n, secoes = 0; float y0 = -1; int mesmaLinha = 1;
  layout_modo_definir(1); tela(1080, 1920);
  assert(ajRetrato());
  triIniciar(); triSecFoco = 0; focarSecao(0);
  quadros(30); png("p1-retrato-faixa");
  n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) if (v[i].ativar == triPontSecaoAbrir) { secoes++; if (y0 < 0) y0 = v[i].y; else if (v[i].y != y0) mesmaLinha = 0; }
  confere(secoes >= 3 && !mesmaLinha, "Portrait: sections register vertical list targets");
  key(SDLK_DOWN); confere(triIlha == TRI_SECOES && triSecFoco == 1 && secAtual == 1, "Portrait: Down selects next section");
  key(SDLK_UP); confere(triIlha == TRI_SECOES && triSecFoco == 0 && !sair, "Portrait: Up selects previous section");
  key(SDLK_RIGHT); confere(triIlha == TRI_LISTA && secAtual == 0 && focoOp >= 0, "Portrait: Right opens section page");
  quadros(30); png("p1-retrato-lista");
  key(SDLK_UP); confere(triIlha == TRI_SECOES && triSecFoco == 0, "Retrato: Cima da 1a linha volta a faixa");
  key(SDLK_UP); confere(triIlha == TRI_CHIPS, "Retrato: Cima da faixa vai aos chips");
  key(SDLK_DOWN); confere(triIlha == TRI_SECOES, "Retrato: Baixo dos chips volta a faixa");
  quadros(30);
  n = ponteiro_teste_lista(&v);
  // A click focuses and the pointer delivers OK right after (ponteiro.c): one click opens.
  { const PonteiroAlvo *alvo = NULL; for (int i = 0; i < n; i++) if (v[i].ativar == triPontSecaoAbrir && v[i].a == 2) alvo = &v[i];
    confere(alvo != NULL, "Retrato: a 3a secao tem alvo");
    if (alvo) { clicar(alvo); confere(triIlha == TRI_LISTA && secAtual == 2 && triSecFoco == 2, "Retrato: toque na secao abre a lista dela"); } }
  // The grouped list scrolls vertically to its last section.
  triIrSecoes(); for (int i = 0; i < nSecoes; i++) key(SDLK_DOWN); quadros(40); png("p1-retrato-faixa-fim");
  n = ponteiro_teste_lista(&v);
  { const PonteiroAlvo *alvo = NULL; for (int i = 0; i < n; i++) if (v[i].ativar == triPontSecaoAbrir && v[i].a == nSecoes) alvo = &v[i];
    confere(triSecFoco == nSecoes && alvo && alvo->y + alvo->h <= TRI_BASE * ajustes_tamanho_ajustes(), "Portrait: grouped list scrolls to last section"); }
  layout_modo_definir(0); tela(1920, 1080);
  sair = 0;
}

// P1-2: presets, review and parental rows, pills and the parental choices are targets.
static void alvosUtil(void) {
  const PonteiroAlvo *v[64], *a; int n;
  triIniciar(); triUtilAbrir(TRI_U_PRESETS); quadros(30); png("p1-presets");
  n = listar(triIlha2R(), v, 64);
  confere(n >= TRI_PRESET_N + 1, "Presets: linhas e pilula registram alvos");
  valor[AJ_DET_BLUR_NAO_VISTOS] = 1; valor[AJ_PAUSA_ELENCO] = 1; valor[AJ_HERO_TRAILER] = 1;
  a = linha(triIlha2R(), 2);
  confere(a != NULL, "Presets: a 3a linha tem alvo");
  if (a) { clicar(a); confere(triUtilLinhaVolta == 2 && triUtil == TRI_U_REVISAO, "Presets: toque na linha abre a previa desse preset"); }
  // Review: the pills close or apply; a click on a row is OK = apply.
  if (triUtil != TRI_U_REVISAO) { assert(triPresetPreparar(2)); triUtilAbrir(TRI_U_REVISAO); triUtilVolta = TRI_U_PRESETS; }
  quadros(30);
  n = listar(triIlha2R(), v, 64);
  confere(n >= rvN + 2, "Revisao: linhas e as duas pilulas registram alvos");
  if (n >= 2) { clicar(v[n - 1]); confere(triUtil == TRI_U_PRESETS && valor[AJ_HERO_TRAILER] == 1, "Revisao: toque em Manter volta sem mudar nada"); }
  assert(triPresetPreparar(2)); triUtilAbrir(TRI_U_REVISAO); triUtilVolta = TRI_U_PRESETS; quadros(30);
  n = listar(triIlha2R(), v, 64);
  if (n >= 2) { clicar(v[n - 2]); confere(triUtil == TRI_U_PRESETS && valor[AJ_HERO_TRAILER] == 0, "Revisao: toque em Aplicar aplica o preset"); }
  quadros(20);
  n = listar(triIlha2R(), v, 64);
  if (n) { clicar(v[n - 1]); confere(ajPenSaveKeyboard, "Presets: toque na pilula abre o nome do preset novo"); }
  key(SDLK_ESCAPE); quadros(5); key(SDLK_ESCAPE); quadros(5); if (triUtil) key(SDLK_ESCAPE); assert(!triUtil);
  // Parental rows and the chooser in the inspector
  perfil_regras_amostra(); triSecFoco = nSecoes; triEntrar(nSecoes); quadros(30);
  a = linha(triIlha2R(), TRI_P_TEMPO);
  confere(a != NULL, "Parental: a linha Tempo de tela tem alvo");
  if (a) { clicar(a); confere(triLinha == TRI_P_TEMPO && ajPenParentEditor == PR_TEMPO, "Parental: toque na linha abre a escolha dela"); }
  if (ajPenParentEditor != PR_TEMPO) ajPenParentOpen(PR_TEMPO);
  quadros(30); png("p1-parental-escolha");
  n = listar(triInspR(), v, 64);
  confere(n >= 7, "Parental: as escolhas na ilha 3 registram alvos");
  if (n >= 7) { clicar(v[2]); confere(ajPenParentEditor < 0 && perfil_regra(ajPenParentProfile(), PR_TEMPO) == 60, "Parental: toque na 3a escolha grava 60 min"); }
  if (ajPenParentEditor >= 0) key(SDLK_ESCAPE);
  key(SDLK_ESCAPE); assert(!triUtil);
}

// P1-3: a long review list scrolls so the focused row stays visible.
static void rolagem(void) {
  const PonteiroAlvo *v[64], *a; int n;
  triIniciar(); rvN = 0;
  for (int op = 0; op < AJ_N && rvN < 30; op++)
    if (triMutavel(op) && ajOpcaoDisponivel(op)) { rvOps[rvN] = op; rvAntes[rvN] = valor[op]; rvDepois[rvN] = valor[op]; rvN++; }
  assert(rvN == 30);
  triUtilAbrir(TRI_U_REVISAO); triUtilVolta = 0; triLinha = 29; quadros(120); png("p1-revisao-rolada");
  n = listar(triIlha2R(), v, 64);
  a = linha(triIlha2R(), 29);
  confere(a != NULL, "Revisao longa: a ultima linha em foco e visivel (tem alvo)");
  if (a && n) confere(a->y + a->h <= v[n - 1]->y + 1.0f, "Revisao longa: a linha em foco fica acima das pilulas");
  confere(triUtilRol > 100.0f, "Revisao longa: a lista rolou ate o foco");
  triLinha = 0; quadros(120);
  confere(triUtilRol < 1.0f, "Revisao longa: volta ao topo com a 1a linha em foco");
  a = linha(triIlha2R(), 0);
  confere(a != NULL, "Revisao longa: a 1a linha volta a ser visivel");
  key(SDLK_ESCAPE); triUtil = 0; triIlha = TRI_SECOES; rvN = 0;
}

// P1-4: without the inspector (layout Lista) the parental chooser still appears.
static void escolhaSemInspetor(void) {
  const PonteiroAlvo *v[64]; int n;
  valor[AJ_LAYOUT_AJUSTES] = 1; assert(!triTemInsp());
  triIniciar(); triSecFoco = nSecoes; triEntrar(nSecoes); triLinha = TRI_P_TEMPO; quadros(20);
  ajPenParentOpen(PR_TEMPO); assert(ajPenParentEditor == PR_TEMPO); quadros(30); png("p1-lista-parental-escolha");
  n = listar(triIlha2R(), v, 64);
  confere(n >= 7, "Lista: a escolha parental aparece sobre a ilha 2 (7 alvos)");
  if (n >= 7) { clicar(v[3]); confere(ajPenParentEditor < 0 && perfil_regra(ajPenParentProfile(), PR_TEMPO) == 90, "Lista: toque na 4a escolha grava 90 min"); }
  confere(triLinha == TRI_P_TEMPO && triUtil == TRI_U_PARENTAL, "Lista: a linha parental por baixo nao recebe o toque");
  if (ajPenParentEditor >= 0) key(SDLK_ESCAPE);
  key(SDLK_ESCAPE); valor[AJ_LAYOUT_AJUSTES] = 0;
}

// Re-review P1 (a): the portrait rail and the long route lists scroll by finger;
// the last section and the last control are reached by touch alone.
static void toque(void) {
  const PonteiroAlvo *a, *v[64]; int n;
  layout_modo_definir(1); tela(1080, 1920); assert(ajRetrato() && ajTelaCheia());
  triIniciar(); triSecRolY = triSecRolYAlvo = 0; triSecFoco = 0; focarSecao(0); quadros(30);
  a = secao(nSecoes);
  confere(!a || a->y > secao(0)->y, "Touch: sections run vertically before dragging");
  a = secao(0); assert(a);
  // Swipe the grouped list until its final section is visible.
  { int k;
    for (k = 0; k < 8; k++) { a = secao(nSecoes); if (a && a->y + a->h <= TRI_BASE * ajustes_tamanho_ajustes()) break; arrastar(540.0f, 1500.0f, 540.0f, 500.0f); } }
  png("p1-retrato-faixa-arrastada");
  a = secao(nSecoes);
  confere(a && a->y >= 0.0f && a->y + a->h <= TRI_BASE * ajustes_tamanho_ajustes(), "Touch: vertical drag reveals last section");
  if (a) { clicar(a); confere(triUtil == TRI_U_PARENTAL, "Toque: a ultima secao abre pelo toque"); }
  // parental: the last control by touch
  quadros(20);
  a = linha(triIlha2R(), TRI_P_N - 1);
  confere(a != NULL, "Toque: a ultima linha parental tem alvo");
  if (a) { clicar(a); confere(pfTela == PF_LIMITES, "Toque: a ultima linha parental abre pelo toque"); key(SDLK_ESCAPE); quadros(5); }
  key(SDLK_ESCAPE); quadros(5); if (triUtil) key(SDLK_ESCAPE); quadros(5);
  // a 30-row review: the last row only by dragging the list
  rvN = 0;
  for (int op = 0; op < AJ_N && rvN < 30; op++)
    if (triMutavel(op) && ajOpcaoDisponivel(op)) { rvOps[rvN] = op; rvAntes[rvN] = valor[op]; rvDepois[rvN] = valor[op]; rvN++; }
  assert(rvN == 30);
  triUtilAbrir(TRI_U_REVISAO); triUtilVolta = 0; quadros(40);
  n = listar(triIlha2R(), v, 64); assert(n >= 2);
  { float pilula = v[n - 1]->y, topo = v[0]->y;
    a = linha(triIlha2R(), 29);
    confere(!visivelEm(a, topo, pilula), "Toque: antes do arrasto a linha 30 da previa esta fora da janela");
    arrastar(540.0f, 1500.0f, 540.0f, 200.0f); arrastar(540.0f, 1500.0f, 540.0f, 200.0f);
    png("p1-retrato-revisao-arrastada");
    a = linha(triIlha2R(), 29);
    confere(visivelEm(a, topo, pilula), "Toque: o arrasto na previa traz a linha 30 para a janela");
    if (a) { clicar(a); confere(triUtil == TRI_U_REVISAO && triLinha == 29 && triAcao == 1, "Toque: a linha 30 responde ao toque (seleciona Aplicar, nao aplica)"); } }
  rvN = 0; triUtil = 0; triIlha = TRI_SECOES;
  layout_modo_definir(0); tela(1920, 1080); sair = 0;
}
// Re-review P1 (b): "Different from the standard" keeps the focused row inside the
// island, above the action pills, at 100% Settings scale.
static void difJanela(void) {
  const PonteiroAlvo *v[64], *a; int n, antes[AJ_N], feitos = 0;
  memcpy(antes, valor, sizeof antes);
  ajEscalaTestePct = 100; assert(ajustes_tamanho_ajustes() == 1.0f);   // the supported 100% scale
  for (int i = secIni[0] + 1; i < secFim(0) && feitos < 16; i++) {
    int op = TELA[i].tipo == IT_OPC ? TELA[i].op : -1;
    if (op < 0 || !triMutavel(op) || !ajOpcaoDisponivel(op) || uxDiferente(op) || OPCOES[op].tipo != OP_ESCOLHA || nValores(op) < 2) continue;
    valor[op] = (valorPadrao[op] + 1) % nValores(op); if (uxDiferente(op)) feitos++;
  }
  assert(feitos == 16);
  triIniciar(); triUtilAbrir(TRI_U_DIF); quadros(30);
  assert(uxNDifs >= 16);
  for (int i = 0; i < 12; i++) key(SDLK_DOWN);
  quadros(60); png("p1-diferentes-linha13");
  n = listar(triIlha2R(), v, 64); assert(n >= 2);
  { float pilula = v[n - 1]->y, topo = triListaR().y * ajustes_tamanho_ajustes();
    a = NULL; for (int i = 0; i < n; i++) if (v[i]->focar == ajPontDif && v[i]->a == uxDifFoco) a = v[i];
    confere(uxDifFoco == 12 && a != NULL, "Diferentes: a 13a linha em foco tem alvo");
    confere(visivelEm(a, topo, pilula), "Diferentes: a linha em foco fica dentro da ilha, acima das pilulas");
    for (int i = 0; i < 12; i++) key(SDLK_UP);
    quadros(60);
    n = listar(triIlha2R(), v, 64);
    a = NULL; for (int i = 0; i < n; i++) if (v[i]->focar == ajPontDif && v[i]->a == 0) a = v[i];
    confere(uxDifFoco == 0 && visivelEm(a, topo, pilula), "Diferentes: de volta ao topo a 1a linha esta visivel"); }
  key(SDLK_ESCAPE); quadros(5);
  memcpy(valor, antes, sizeof antes); ajEscalaTestePct = 0;
}
// A real finger tap on a target (FINGERDOWN/UP through the pointer).
static void tocar(const PonteiroAlvo *a) {
  float x = a->x + a->w * 0.5f, y = a->y + a->h * 0.5f;
  dedo(SDL_FINGERDOWN, x, y); relogio += 60; dedo(SDL_FINGERUP, x, y); relogio += 60; quadros(10);
}
static const PonteiroAlvo *alvoInline(int op, int diferenteDe) {
  const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) if (v[i].ativar == ajToqueInlineValor && v[i].b == op && v[i].a != diferenteDe) return &v[i];
  return NULL;
}
static const PonteiroAlvo *alvoAtivar(PonteiroFn fn) {
  const PonteiroAlvo *v; int n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) if (v[i].ativar == fn) return &v[i];
  return NULL;
}
// Re-review 3, P1 (a): on the phone the values of an inline editor (palette,
// segmented choice) are touch targets; a tap changes the draft, Confirm saves.
static void inlineToque(void) {
  int ops[2] = { AJ_TEMA, -1 }, antes[AJ_N];
  memcpy(antes, valor, sizeof antes);
  for (int i = 0; i < AJ_N_TELA && ops[1] < 0; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op != AJ_TEMA && ajSegmentada(TELA[i].op) && triDisponivel(TELA[i].op) && !inativa(TELA[i].op)) ops[1] = TELA[i].op;
  assert(ops[1] >= 0);
  layout_modo_definir(1); tela(1080, 1920); assert(ajTelaCheia());
  for (int k = 0; k < 2; k++) {
    int op = ops[k], salvo; const PonteiroAlvo *a;
    char msg[160];
    triIniciar(); focarOpcao(op); triFocar(op); quadros(30);
    key(SDLK_RETURN); quadros(40);
    snprintf(msg, sizeof msg, "Inline %s: OK abre a edicao na linha", CHAVE[op]); confere(uxEditor == 1 && ajEditandoLinha(op), msg);
    salvo = valor[op];
    a = alvoInline(op, uxPendente);
    snprintf(msg, sizeof msg, "Inline %s: cada valor tem alvo de toque", CHAVE[op]); confere(a != NULL, msg);
    if (!a) { if (uxEditor) key(SDLK_ESCAPE); continue; }
    { int v = a->a;
      if (k == 0) png("p1-inline-paleta");
      tocar(a);
      snprintf(msg, sizeof msg, "Inline %s: toque no valor muda o rascunho, nao grava", CHAVE[op]); confere(uxPendente == v && valor[op] == salvo && uxEditor == 1, msg);
      a = alvoAtivar(ajToqueConfirmar);
      snprintf(msg, sizeof msg, "Inline %s: Confirmar tem alvo de toque", CHAVE[op]); confere(a != NULL, msg);
      if (a) { tocar(a); snprintf(msg, sizeof msg, "Inline %s: Confirmar grava o valor tocado", CHAVE[op]); confere(!uxEditor && valor[op] == v, msg); } }
    if (uxEditor) key(SDLK_ESCAPE);
    quadros(5);
  }
  memcpy(valor, antes, sizeof antes);
  layout_modo_definir(0); tela(1920, 1080); sair = 0;
}
// Re-review 3, P1 (b): a preview row only previews/selects; only Apply applies.
static void previaNaoAplica(void) {
  const PonteiroAlvo *a;
  valor[AJ_DET_BLUR_NAO_VISTOS] = 1; valor[AJ_PAUSA_ELENCO] = 1; valor[AJ_HERO_TRAILER] = 1;
  triIniciar(); assert(triPresetPreparar(2)); triUtilAbrir(TRI_U_REVISAO); triUtilVolta = TRI_U_PRESETS; quadros(30);
  // remote: OK on a row
  key(SDLK_RETURN);
  confere(triUtil == TRI_U_REVISAO && valor[AJ_HERO_TRAILER] == 1, "Previa: OK numa linha nao aplica nada");
  confere(triAcao == 1, "Previa: OK numa linha leva o foco a Aplicar");
  // touch: tap on a row
  triAcao = 0; triLinha = 0; quadros(20);
  a = linha(triIlha2R(), 1);
  confere(a != NULL, "Previa: a 2a linha tem alvo");
  if (a) { tocar(a); confere(triUtil == TRI_U_REVISAO && valor[AJ_HERO_TRAILER] == 1 && valor[AJ_PAUSA_ELENCO] == 1, "Previa: toque numa linha nao aplica nada"); }
  // only Apply applies
  quadros(10);
  { const PonteiroAlvo *v[64]; int n = listar(triIlha2R(), v, 64);
    if (n >= 2) { tocar(v[n - 2]); confere(triUtil == TRI_U_PRESETS && valor[AJ_HERO_TRAILER] == 0, "Previa: so o toque em Aplicar aplica"); } }
  key(SDLK_ESCAPE); quadros(5); if (triUtil) key(SDLK_ESCAPE);
}
// Re-review 4, P1: at every supported Settings size (80/90/100%) x text size
// (85..130%) every section of the sections island is reachable and fully
// visible: by D-pad (the island follows the focus) and by touch (drag, tap).
static const PonteiroAlvo *secaoVisivel(int j) {
  const PonteiroAlvo *a = secao(j);
  float lado = TRI_SEC_LN * ajustes_tamanho_ajustes();
  if (!a || a->y < 0.0f || a->y + a->h > NV_LAYOUT_REAL_H + 0.5f || a->h < lado - 1.0f) return NULL;
  return a;
}
static void escalas(void) {
  int antesT = valor[AJ_TAMANHO_AJUSTES], antesE = valor[AJ_ESCALA_TEXTO];
  char msg[200];
  for (int t = 0; t < 3; t++) for (int e = 0; e < 6; e++) {
    int pct = 80 + 10 * t, txt = (int)(ajustes_escala_texto() * 100.0f + 0.5f), pad = 1, toque = 0;
    valor[AJ_TAMANHO_AJUSTES] = t; valor[AJ_ESCALA_TEXTO] = e; gfx_escala_txt_definir(ajustes_escala_texto());
    txt = (int)(ajustes_escala_texto() * 100.0f + 0.5f);
    // D-pad on the TV: Down from the first section to Parental control
    layout_modo_definir(0); tela(1920, 1080);
    triIniciar(); triFocarSecao(0); quadros(15);
    for (int j = 0; j <= nSecoes; j++) {
      if (triSecFoco != j || !secaoVisivel(j)) { pad = 0; printf("  Ajustes %d%% texto %d%%: secao %d em foco fora da tela\n", pct, txt, j); break; }
      key(SDLK_DOWN); quadros(15);
    }
    if (pct == 100 && txt == 130) png("p1-secoes-130");
    snprintf(msg, sizeof msg, "Escalas %d%% x texto %d%%: D-pad alcanca e mostra todas as secoes", pct, txt); confere(pad, msg);
    // Touch on a landscape phone: drag the island until Parental control shows, tap it
    layout_modo_definir(1); tela(2340, 1080); assert(!ajRetrato() && ajTelaCheia());
    triIniciar(); triFocarSecao(0); quadros(15);
    { const PonteiroAlvo *a0 = secao(0), *a;
      for (int k = 0; k < 8 && !(a = secaoVisivel(nSecoes)) && a0; k++) {
        float x = a0->x + a0->w * 0.5f;
        arrastar(x, NV_LAYOUT_REAL_H - 140.0f, x, 300.0f);
      }
      a = secaoVisivel(nSecoes);
      if (a) { tocar(a); toque = triUtil == TRI_U_PARENTAL; }
      if (triUtil) { key(SDLK_ESCAPE); quadros(5); } }
    snprintf(msg, sizeof msg, "Escalas %d%% x texto %d%%: toque alcanca Controle parental", pct, txt); confere(toque, msg);
  }
  valor[AJ_TAMANHO_AJUSTES] = antesT; valor[AJ_ESCALA_TEXTO] = antesE; gfx_escala_txt_definir(ajustes_escala_texto());
  layout_modo_definir(0); tela(1920, 1080); sair = 0;
}
int main(int argc, char **argv) {
  char dir[1024];
  if (argc > 1) dirPng = argv[1];
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-tri-p1-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Settings P1", 0, 0, 1920, 1920, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win)); glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64); IMG_Init(IMG_INIT_PNG);
  gfx_icones_dir("deploy/app/art"); ajustes_recursos("deploy/app/art");
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); player_dir(dir); fontefalha_dir(dir);
  ajustes_iniciar(); assert(gravar());
  layout_modo_definir(0); layout_tela_definir(1920, 1080);
  ponteiro_iniciar(); ponteiro_teste_toque(1); ponteiro_teste_relogio(agora); ponteiro_teste_janela(1920, 1080);
  quadros(5);
  retrato();
  alvosUtil();
  rolagem();
  escolhaSemInspetor();
  toque();
  difJanela();
  inlineToque();
  previaNaoAplica();
  escalas();
  if (!falhas) puts("PASS: three-island Settings P1s: portrait rail, utility targets, scrolling, parental chooser");
  return falhas ? 1 : 0;
}
