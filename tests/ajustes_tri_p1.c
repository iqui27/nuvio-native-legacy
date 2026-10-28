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
// The hidden window follows the layout: portrait draws on a 1080 x 1920 target.
static void tela(int w, int h) {
  telaW = w; telaH = h;
  SDL_SetWindowSize(win, w, h); glViewport(0, 0, w, h); gfx_tamanho_alvo(w, h);
  layout_tela_definir(w, h); ponteiro_teste_janela(w, h);
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

// P1-1: portrait keeps a visible, navigable section rail.
static void retrato(void) {
  const PonteiroAlvo *v; int n, secoes = 0; float y0 = -1; int mesmaLinha = 1;
  layout_modo_definir(1); tela(1080, 1920);
  assert(ajRetrato());
  triIniciar(); triSecFoco = 0; focarSecao(0);
  quadros(30); png("p1-retrato-faixa");
  n = ponteiro_teste_lista(&v);
  for (int i = 0; i < n; i++) if (v[i].focar == triPontSecao) { secoes++; if (y0 < 0) y0 = v[i].y; else if (v[i].y != y0) mesmaLinha = 0; }
  confere(secoes >= 3, "Retrato: a faixa de secoes registra alvos");
  confere(secoes >= 3 && mesmaLinha, "Retrato: as secoes ficam numa faixa horizontal");
  key(SDLK_RIGHT); confere(triIlha == TRI_SECOES && triSecFoco == 1 && secAtual == 1, "Retrato: Direita troca a secao na faixa");
  key(SDLK_LEFT); confere(triIlha == TRI_SECOES && triSecFoco == 0 && !sair, "Retrato: Esquerda volta a secao anterior");
  key(SDLK_DOWN); confere(triIlha == TRI_LISTA && secAtual == 0 && focoOp >= 0, "Retrato: Baixo entra na lista");
  quadros(30); png("p1-retrato-lista");
  key(SDLK_UP); confere(triIlha == TRI_SECOES && triSecFoco == 0, "Retrato: Cima da 1a linha volta a faixa");
  key(SDLK_UP); confere(triIlha == TRI_CHIPS, "Retrato: Cima da faixa vai aos chips");
  key(SDLK_DOWN); confere(triIlha == TRI_SECOES, "Retrato: Baixo dos chips volta a faixa");
  quadros(30);
  n = ponteiro_teste_lista(&v);
  // A click focuses and the pointer delivers OK right after (ponteiro.c): one click opens.
  { const PonteiroAlvo *alvo = NULL; for (int i = 0; i < n; i++) if (v[i].focar == triPontSecao && v[i].a == 2) alvo = &v[i];
    confere(alvo != NULL, "Retrato: a 3a secao tem alvo");
    if (alvo) { clicar(alvo); confere(triIlha == TRI_LISTA && secAtual == 2 && triSecFoco == 2, "Retrato: toque na secao abre a lista dela"); } }
  // the last section is far to the right: the rail scrolls to it
  triIrSecoes(); for (int i = 0; i < nSecoes; i++) key(SDLK_RIGHT); quadros(40); png("p1-retrato-faixa-fim");
  n = ponteiro_teste_lista(&v);
  { const PonteiroAlvo *alvo = NULL; for (int i = 0; i < n; i++) if (v[i].focar == triPontSecao && v[i].a == nSecoes) alvo = &v[i];
    confere(triSecFoco == nSecoes && alvo && alvo->x + alvo->w <= NV_LAYOUT_REAL_W - 40.0f, "Retrato: a faixa rola ate a ultima secao"); }
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

int main(int argc, char **argv) {
  char dir[1024];
  if (argc > 1) dirPng = argv[1];
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-tri-p1-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Settings P1", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  if (!falhas) puts("PASS: three-island Settings P1s: portrait rail, utility targets, scrolling, parental chooser");
  return falhas ? 1 : 0;
}
