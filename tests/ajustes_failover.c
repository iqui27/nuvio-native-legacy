// Settings contract: independent failover controls, stable identities and
// drafts which do not consume a saved link limit until explicitly confirmed.
#include "text.h"
#include "gfx.h"
static void recorteTeste(float x, float y, float w, float h) {
  (void)x; (void)y; (void)w; (void)h;
}
static void semRecorteTeste(void) {}
#define gfx_recorte recorteTeste
#define gfx_sem_recorte semRecorteTeste
#include "../src/ajustes.c"
#undef gfx_recorte
#undef gfx_sem_recorte
#include "../src/fontefalha.h"
#include <assert.h>

static Uint32 relogio = 1000;
static Uint32 agora(void) { return relogio; }
static void key(SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e); relogio += 80;
}
static void abrir(int op) {
  focarOpcao(op); key(SDLK_RETURN);
}
static FonteFalhaConfig configuracao(void) {
  FonteFalhaConfig c; memset(&c, 0, sizeof c);
  fontefalha_config(&c); return c;
}
static char *arquivo(void) {
  char caminho[1200];
  snprintf(caminho, sizeof caminho, "%s/fontefalha.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(caminho, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); assert(n >= 0); rewind(f);
  char *s = calloc((size_t)n + 1, 1); assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); fclose(f); return s;
}
static void igualArquivo(const char *antes) {
  char *depois = arquivo(); assert(!strcmp(antes, depois)); free(depois);
}
static void igual(const FonteFalhaConfig *antes) {
  FonteFalhaConfig depois = configuracao();
  assert(antes->provedores == depois.provedores && antes->addons == depois.addons);
  assert(antes->nAddons == depois.nAddons);
  for (int i = 0; i < 4; i++) {
    assert(antes->ordem[i] == depois.ordem[i]);
    assert(antes->limites[i] == depois.limites[i]);
  }
  for (int i = 0; i < antes->nAddons; i++) {
    assert(!strcmp(antes->addon[i].chave, depois.addon[i].chave));
    assert(antes->addon[i].limite == depois.addon[i].limite);
  }
}
static void recarregar(void) {
  fontefalha_config_ler(); ajustes_iniciar();
}
static void independentes(void) {
  const int legado = ajustes_fonte_repor();
  assert(DEBRID_RD == 0 && DEBRID_TB == 1 && DEBRID_PM == 2 && DEBRID_AD == 3);
  assert(!fontefalha_provedores() && !fontefalha_addons() && !fontefalha_ativa());
  for (int i = 0; i < DEBRID_N; i++) {
    assert(fontefalha_provedor_posicao(i) == i);
    assert(fontefalha_provedor_limite(i) == 3);
  }
  abrir(AJ_FALHA_PROVEDORES);
  assert(!uxEditor && fontefalha_provedores() && !fontefalha_addons());
  assert(ajustes_fonte_repor() == legado);
  recarregar();
  assert(fontefalha_provedores() && !fontefalha_addons());
  abrir(AJ_FALHA_ADDONS);
  assert(!uxEditor && fontefalha_provedores() && fontefalha_addons());
  abrir(AJ_FALHA_PROVEDORES);
  assert(!fontefalha_provedores() && fontefalha_addons());
  assert(ajustes_fonte_repor() == legado);
  recarregar();
  assert(!fontefalha_provedores() && fontefalha_addons());
  abrir(AJ_FALHA_ADDONS);
  assert(!fontefalha_provedores() && !fontefalha_addons());
  puts("PASS: failover toggles default off, persist and remain independent");
}

// Per-service rows left the screen (shared exclusions): the numeric editor
// lives in the provider order sheet and saves to the selected service.
static void limitesNaOrdem(void) {
  for (int i = 0; i < DEBRID_N; i++)
    assert(fontefalha_provedor_definir_limite(i, i + 1));
  abrir(AJ_FALHA_PROVEDOR_ORDEM); assert(frAberta == 4);
  // Links column (col 2) on the first row opens RD's numeric editor as a draft.
  key(SDLK_RIGHT); key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(uxEditor == 1 && uxOp == AJ_FALHA_RD_LIMITE && uxPendente == 1);
  FonteFalhaConfig antes = configuracao();
  char *disco = arquivo();
  key(SDLK_RIGHT); assert(uxPendente == 2); igual(&antes); igualArquivo(disco);
  key(SDLK_ESCAPE); assert(!uxEditor && frAberta == 4); igual(&antes); free(disco);
  // Second row (TB): col 2, then clamp 1..10 and persist.
  key(SDLK_DOWN); key(SDLK_RIGHT); key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(uxEditor == 1 && uxOp == AJ_FALHA_TB_LIMITE && uxPendente == 2);
  for (int i = 0; i < 20; i++) key(SDLK_RIGHT);
  assert(uxPendente == 10); key(SDLK_RETURN);
  assert(!uxEditor && frAberta == 4 && fontefalha_provedor_limite(DEBRID_TB) == 10);
  key(SDLK_ESCAPE); assert(!frAberta);
  recarregar(); assert(fontefalha_provedor_limite(DEBRID_TB) == 10);
  FonteFalhaConfig salvo = configuracao(); recarregar(); igual(&salvo);
}
static void provedores(void) {
  limitesNaOrdem();
  // Distinct saved limits must follow service IDs when the order changes.
  for (int i = 0; i < DEBRID_N; i++)
    assert(fontefalha_provedor_definir_limite(i, i + 1));
  abrir(AJ_FALHA_PROVEDOR_ORDEM); assert(frAberta == 4);
  key(SDLK_DOWN); key(SDLK_LEFT); key(SDLK_RETURN); // TB moves before RD.
  assert(fontefalha_provedor_posicao(DEBRID_TB) == 0);
  assert(fontefalha_provedor_posicao(DEBRID_RD) == 1);
  for (int i = 0; i < DEBRID_N; i++) assert(fontefalha_provedor_limite(i) == i + 1);
  fontefalha_config_ler(); // movement is already on disk while the sheet remains open.
  assert(frAberta == 4 && fontefalha_provedor_posicao(DEBRID_TB) == 0);
  key(SDLK_RIGHT); key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(uxEditor == 1 && uxOp == AJ_FALHA_TB_LIMITE && uxPendente == 2);
  FonteFalhaConfig antes = configuracao();
  key(SDLK_RIGHT); key(SDLK_ESCAPE);
  assert(!uxEditor && frAberta == 4); igual(&antes);
  key(SDLK_RETURN); assert(uxOp == AJ_FALHA_TB_LIMITE);
  key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(!uxEditor && frAberta == 4 && fontefalha_provedor_limite(DEBRID_TB) == 3);
  assert(fontefalha_provedor_limite(DEBRID_RD) == 1);
  FonteFalhaConfig salvo = configuracao();
  key(SDLK_ESCAPE); assert(!frAberta); igual(&salvo);
  recarregar(); igual(&salvo);
  puts("PASS: native limits count 1-10 total links and stay with stable service IDs");
}

static AddonRemoto addonsTeste[3];
static char addonChave[3][FONTEFALHA_CHAVE];
static void instalarAddons(void) {
  memset(addonsTeste, 0, sizeof addonsTeste);
  for (int i = 0; i < 3; i++) {
    char caminho[720];
    memset(caminho, 'a', sizeof caminho - 1); caminho[sizeof caminho - 1] = 0;
    snprintf(addonsTeste[i].nome, sizeof addonsTeste[i].nome, "%s", i < 2 ? "AIOStreams" : "Comet");
    snprintf(addonsTeste[i].url, sizeof addonsTeste[i].url,
             "https://fixture.invalid/%s/instance-%d/manifest.json", caminho, i);
    addonsTeste[i].ativo = 1;
  }
  assert(!strncmp(addonsTeste[0].url, addonsTeste[1].url, 600));
  assert(strcmp(addonsTeste[0].url, addonsTeste[1].url));
  assert(addons_definir_lista(addonsTeste, 3));
  for (int i = 0; i < 3; i++) {
    addons_manifesto_lido(i, "{\"id\":\"org.fixture.aiostreams\",\"name\":\"AIOStreams\",\"resources\":[\"stream\"]}");
    assert(!strcmp(addons_id_manifesto(i), "org.fixture.aiostreams"));
    assert(addons_sondado(i) && addons_fornece(i, ADD_STREAM));
    fontefalha_addon_chave(addons_base(i), addonChave[i], sizeof addonChave[i]);
    assert(addonChave[i][0]);
  }
  assert(strcmp(addonChave[0], addonChave[1]));
}

static void addonsPorInstancia(void) {
  abrir(AJ_FALHA_ADDON_ORDEM); assert(frAberta == 5);
  FonteFalhaConfig c = configuracao();
  assert(c.nAddons == 3); // neither repeated display names nor manifest IDs merge instances.
  for (int i = 0; i < 3; i++) {
    assert(fontefalha_addon_posicao(addonChave[i]) == i);
    assert(fontefalha_addon_limite(addonChave[i]) == 3);
  }
  key(SDLK_DOWN); key(SDLK_LEFT); key(SDLK_RETURN);
  assert(fontefalha_addon_posicao(addonChave[1]) == 0);
  assert(fontefalha_addon_posicao(addonChave[0]) == 1);
  fontefalha_config_ler();
  assert(frAberta == 5 && fontefalha_addon_posicao(addonChave[1]) == 0);

  // The shared numeric editor must save to the selected opaque instance key.
  key(SDLK_LEFT); key(SDLK_RIGHT); key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(uxEditor == 1 && uxOp == AJ_FALHA_ADDON_LIMITE && !frAberta);
  FonteFalhaConfig antes = configuracao();
  char *disco = arquivo();
  key(SDLK_RIGHT); key(SDLK_RIGHT); assert(uxPendente == 5);
  igual(&antes); igualArquivo(disco);
  key(SDLK_ESCAPE); assert(!uxEditor && frAberta == 5); igual(&antes); igualArquivo(disco); free(disco);
  key(SDLK_LEFT); key(SDLK_RIGHT); key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(uxEditor == 1 && uxOp == AJ_FALHA_ADDON_LIMITE);
  for (int i = 0; i < 20; i++) key(SDLK_LEFT);
  assert(uxPendente == 1); key(SDLK_RETURN);
  assert(!uxEditor && frAberta == 5);
  assert(fontefalha_addon_limite(addonChave[1]) == 1);
  assert(fontefalha_addon_limite(addonChave[0]) == 3);
  assert(fontefalha_addon_limite(addonChave[2]) == 3);
  key(SDLK_ESCAPE);
  FonteFalhaConfig salvo = configuracao(); recarregar(); igual(&salvo);
  disco = arquivo();
  assert(!strstr(disco, "fixture.invalid") && !strstr(disco, "AIOStreams")); free(disco);

  // A refreshed installed list changes indices and names, never identities.
  AddonRemoto troca = addonsTeste[0];
  addonsTeste[0] = addonsTeste[1]; addonsTeste[1] = addonsTeste[2]; addonsTeste[2] = troca;
  for (int i = 0; i < 3; i++)
    snprintf(addonsTeste[i].nome, sizeof addonsTeste[i].nome, "Renamed instance %d", i);
  assert(addons_definir_lista(addonsTeste, 3));
  abrir(AJ_FALHA_ADDON_ORDEM); assert(frAberta == 5);
  assert(!strcmp(faItem[0].chave, addonChave[1]) && faItem[0].instalado == 0);
  igual(&salvo);
  for (int i = 0; i < 3; i++) {
    char chave[FONTEFALHA_CHAVE];
    fontefalha_addon_chave(addons_base(i), chave, sizeof chave);
    assert(fontefalha_addon_posicao(chave) >= 0);
  }
  assert(!fontefalha_provedores() && !fontefalha_addons());
  key(SDLK_ESCAPE);
  puts("PASS: duplicate AIO instances retain separate full-URL identities, order and caps");
}

static void quadroNumero(void) {
  ponteiro_teste_janela((int)NV_TELA_W, (int)NV_TELA_H);
  ponteiro_quadro(relogio);
  ponteiro_camada(); ajToqueCamada();
  ajPontEditor = 1; // settled entrance, as in the existing windowless fixture.
  GfxRect m = aj2EditorR();
  if (m.w > 1100) m.w = 1100; // register production controls without the GL preview.
  AJ_ESCALA_INI(); aj2EditorNumero(m); AJ_ESCALA_FIM();
  ponteiro_desenhar(); relogio += 400;
}
static void tocarAlvo(PonteiroFn focar, PonteiroFn ativar, int a, int b) {
  const PonteiroAlvo *v;
  int n = ponteiro_teste_lista(&v), alvo = -1;
  for (int i = 0; i < n; i++)
    if (v[i].focar == focar && v[i].ativar == ativar && v[i].a == a && v[i].b == b) { alvo = i; break; }
  assert(alvo >= 0);
  float x = v[alvo].x + v[alvo].w * 0.5f, y = v[alvo].y + v[alvo].h * 0.5f;
  assert(x >= 0 && x < NV_LAYOUT_REAL_W && y >= 0 && y < NV_LAYOUT_REAL_H);
  assert(ponteiro_achar(v, n, x, y) == alvo);
  SDL_Event e; SDL_zero(e);
  e.type = SDL_FINGERDOWN; e.tfinger.touchId = 1; e.tfinger.fingerId = 1;
  e.tfinger.x = x / NV_TELA_W; e.tfinger.y = y / NV_TELA_H;
  assert(ponteiro_evento(&e, ajustes_evento));
  e.type = SDL_FINGERUP; relogio += 60;
  assert(ponteiro_evento(&e, ajustes_evento)); relogio += 400;
}
static void tocar(PonteiroFn ativar, int a) { tocarAlvo(NULL, ativar, a, 0); }
static void quadroFolha(void) {
  ponteiro_teste_janela((int)NV_TELA_W, (int)NV_TELA_H);
  ponteiro_quadro(relogio);
  AJ_ESCALA_INI(); desenhaFolhaFalha(); AJ_ESCALA_FIM();
  ponteiro_desenhar(); relogio += 400;
}
static void limitePorToque(void) {
  layout_modo_definir(1); layout_tela_definir(1080, 2340);
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  abrir(AJ_FALHA_ADDON_ORDEM); assert(frAberta == 5);
  quadroFolha(); tocarAlvo(faPont, NULL, 1, 0);
  assert(fontefalha_addon_posicao(addonChave[0]) == 0);
  quadroFolha(); tocarAlvo(faPont, NULL, 0, 1);
  assert(fontefalha_addon_posicao(addonChave[1]) == 0);
  quadroFolha(); tocarAlvo(faPont, NULL, 0, 2);
  assert(uxEditor == 1 && uxOp == AJ_FALHA_ADDON_LIMITE);
  FonteFalhaConfig antes = configuracao();
  char *disco = arquivo();
  assert(uxPendente == 1);
  quadroNumero(); tocar(ajPontEdPasso, 1);
  assert(uxEditor == 1 && uxPendente == 2); igual(&antes); igualArquivo(disco);
  quadroNumero(); tocar(ajPontEdPasso, -1);
  assert(uxEditor == 1 && uxPendente == 1); igual(&antes); igualArquivo(disco);
  quadroNumero(); tocar(ajPontEdPasso, 1);
  quadroNumero(); tocar(ajToqueCancelar, 0);
  assert(!uxEditor && frAberta == 5); igual(&antes); igualArquivo(disco);

  quadroFolha(); tocarAlvo(faPont, NULL, 0, 2);
  assert(uxEditor == 1 && uxPendente == 1);
  for (int i = 0; i < 12; i++) {
    quadroNumero(); tocar(ajPontEdPasso, 1);
  }
  assert(uxEditor == 1 && uxPendente == 10); igual(&antes); igualArquivo(disco); free(disco);
  quadroNumero(); tocar(ajToqueConfirmar, 0);
  assert(!uxEditor && frAberta == 5);
  assert(fontefalha_addon_limite(addonChave[1]) == 10);
  assert(fontefalha_addon_limite(addonChave[0]) == 3);
  FonteFalhaConfig salvo = configuracao();
  key(SDLK_ESCAPE); recarregar(); igual(&salvo);
  puts("PASS: touch +/- clamps the pending cap; Cancel and Confirm return to the same order sheet");
}

int main(void) {
  assert(getenv("NUVIO_DADOS"));
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  assert(gravar());
  layout_modo_definir(0); layout_tela_definir(1920, 1080);
  ponteiro_teste_relogio(agora); ponteiro_teste_janela(1920, 1080);
  gfx_opacidade_grupo = 0;
  independentes(); provedores(); instalarAddons(); addonsPorInstancia(); limitePorToque();
  SDL_Quit();
  return 0;
}
