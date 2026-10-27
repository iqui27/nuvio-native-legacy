// Captura da tela "Começar" (2.1): os 4 passos e o teclado da TV por cima, em
// 1920x1080, janela GL escondida e desenho num FBO — como
// tests/novidades148_shot.c. Antes das capturas, confere por evento de tecla,
// SO pelo caminho real (comecar_evento, o que app_evento chama; nada de falar
// com o teclado da TV direto):
//   passo 1: OK abre o teclado da TV e o D-pad digita/cancela/confirma; nao ha
//            linha de perfil infantil; nada vai ao disco antes do fim;
//   passo 2: Voltar volta ao 1 sem perder o nome; Conta Nuvio e Começar do
//            zero avançam; Stremio e AIOManager começam o import (não avançam);
//   passo 3: Trakt pede o código (traktauth), "Só nesta TV" avança;
//   passo 4: OK devolve COMECAR_PEDIU_TOUR ou NADA e grava estado=feito, nome e cor;
//   saidas:  Voltar no passo 1 pula (feito, nada mais); Voltar nos passos 2-4
//            volta um passo; Voltar segurado e "Pular tudo" pulam de qualquer
//            passo; repeticao de tecla nunca volta passo;
//   upgrade: instalacao com sessao/perfis/progresso e sem comecar.txt nunca abre;
//   D-pad: todo foco alcança e escapa — varredura por foco inicial (baixo,
//            cima, direita, esquerda) sem cair em item que não sai do lugar.
#include "comecar.h"
#include "importarfluxo.h"
#include "celbotao.h"
#include "comecarregra.h"
#include "ajustes.h"
#include "anim.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "teclado.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "addons.h"
#include "perfilinfantil.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

// Compile the real screen here so bounds checks cover the shared drawing,
// D-pad and pointer rectangles without adding a production test API.
static int checkBounds;
static int measureSummary, summaryLines;
static float summaryY[4];
static GfxRect previewRect;
static void measured_veil(GfxRect r, float radius, float fraction, float alpha) {
  if (measureSummary) previewRect = r;
  gfx_veu_base(r, radius, fraction, alpha);
}
static void check_rect(GfxRect r) {
  float k = gfx_escala();
  if (r.x * k < -0.01f || r.y * k < -0.01f || r.w <= 0 || r.h <= 0 ||
      (r.x + r.w) * k > 1920.01f || (r.y + r.h) * k > 1080.01f) {
    fprintf(stderr, "FAIL Onboarding step %d at %.0f%%: rect %.1f %.1f %.1f %.1f\n",
            comecar_passo() + 1, k * 100, r.x * k, r.y * k, r.w * k, r.h * k);
    abort();
  }
}
static void checked_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca);
static void offline_import(int source) {
  importarfluxo_teste(source, IMPF_ESPERA, source == IMPF_STREMIO ? "K7QX2M" : "",
                     source == IMPF_STREMIO ? "https://link.stremio.com/" : "", 0, 0);
}
#define importarfluxo_comecar offline_import
// Mock 3C state for captures only: Trakt linked, Simkl waiting on a code.
#include "trakt.h"
#include "simklauth.h"
static int shotContas;
static int shot_trakt_ativo(void) { return shotContas || trakt_ativo(); }
static SmkEstado shot_simkl_estado(void) { return shotContas ? SMK_AGUARDANDO : simklauth_estado(); }
static const char *shot_simkl_codigo(void) { return shotContas ? "PX49TD" : simklauth_codigo(); }
static const char *shot_simkl_url(void) { return shotContas ? "https://simkl.com/pin" : simklauth_url(); }
#define trakt_ativo shot_trakt_ativo
#define simklauth_estado shot_simkl_estado
#define simklauth_codigo shot_simkl_codigo
#define simklauth_url shot_simkl_url
#define gfx_cor checked_cor
#define gfx_veu_base measured_veil
#include "../src/comecar.c"
#undef gfx_veu_base
#undef gfx_cor
#undef importarfluxo_comecar
#undef trakt_ativo
#undef simklauth_estado
#undef simklauth_codigo
#undef simklauth_url
static void checked_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  if (checkBounds) check_rect(r);
  if (measureSummary && cr == 1 && cg == 1 && cb == 1 && ca == .07f && summaryLines < 4)
    summaryY[summaryLines++] = r.y * gfx_escala();
  gfx_cor(r, raio, cr, cg, cb, ca);
}
static void check_controls(void) {
  Item items[MAX_ITENS];
  TXT_INI();
  int n = montar(items);
  for (int i = 0; i < n; i++) {
    GfxRect r = items[i].r;
    check_rect(r);
    assert(r.x * gfx_escala() >= 119.99f);   // mock: 120 px margins
    assert((r.x + r.w) * gfx_escala() <= 1800.01f);
    assert(r.y * gfx_escala() >= 59.99f);
    assert((r.y + r.h) * gfx_escala() <= 1020.01f);
    if (items[i].id == B_PRINC)   // mock .rodape: 64 px row ending 72 px above the bottom
      assert(fabsf((r.y + r.h * .5f) * gfx_escala() - (1008 - 32 * gfx_escala())) < .01f);
    for (int j = 0; j < i; j++) {
      GfxRect q = items[j].r;
      int separate = r.x >= q.x + q.w - .01f || q.x >= r.x + r.w - .01f ||
                     r.y >= q.y + q.h - .01f || q.y >= r.y + r.h - .01f;
      if (!separate) fprintf(stderr, "Overlapping controls: step=%d scale=%.2f ids=%d,%d rects=(%.1f,%.1f,%.1f,%.1f),(%.1f,%.1f,%.1f,%.1f)\n",
                             passo, gfx_escala(), items[i].id, items[j].id,
                             r.x, r.y, r.w, r.h, q.x, q.y, q.w, q.h);
      assert(separate);
    }
  }
  assert(fabsf(items[0].r.x * gfx_escala() - 120) < .01f);
  TXT_FIM();
}

static GLuint fbo, fboTex;

void teclado_teste_sistema(const char *t);   // teclado.c (uso de teste)

static void tecla_em(SDL_Keycode k, int repeticao, Uint32 ts) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.repeat = (Uint8)repeticao;
  e.key.timestamp = ts;
  // So o caminho real: app_evento entrega a tecla ao Comecar e ele decide.
  // Nada de falar com o teclado da TV direto, senao o teste esconde o bug.
  comecar_evento(&e);
}

static void soltar(SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYUP;
  e.key.keysym.sym = k;
  comecar_evento(&e);
}

static void tecla(SDL_Keycode k) { tecla_em(k, 0, 1000); soltar(k); }

// Um quadro: fundo de mentira (a tela do app embaixo) e o Começar por cima.
static void quadro(float dt) {
  int i;
  comecar_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();   // como o app: sem isto o teto de rasterizacao acaba
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  // The app underneath: a real backdrop, so enter/exit frames show the fade over art.
  { GLuint t = tex_obter("design/universo-2026/assets/01.jpg");
    if (t) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, t, GFX_CARD, 0, 0, 0, 0, 0, 0, 0, 1);
    for (i = 0; i < 4 && !t; i++) tex_bombear(8); }
  if (checkBounds) check_controls();
  comecar_desenhar(SDL_GetTicks());
}

static void adiantar(int quadros) {
  int i;
  for (i = 0; i < quadros; i++) { quadro(1.0f / 60.0f); SDL_Delay(2); tex_bombear(8); }
}

static void captura(const char *nome, int quadros) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  adiantar(quadros);
  // Decode art at the current size without advancing motion-proof timestamps.
  int pending = 1;
  for (int k = 0; pending && k < 600; k++) {
    quadro(0);
    SDL_Delay(2);
    tex_bombear(8);
    tex_estatisticas(NULL, &pending, NULL, NULL, NULL);
  }
  assert(!pending);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

static char *ler(void) {
  char *s = dados_ler(COMECAR_ARQ);
  assert(s);
  return s;
}

static int existe(void) {
  char *s = dados_ler(COMECAR_ARQ);
  int r = s != NULL;
  free(s);
  return r;
}

static void contem(const char *agulha) {
  char *s = ler();
  if (!strstr(s, agulha)) { printf("FALHA: comecar.txt sem '%s' (tem: %s)\n", agulha, s); exit(1); }
  free(s);
}

static void nao_contem(const char *agulha) {
  char *s = ler();
  if (strstr(s, agulha)) { printf("FALHA: comecar.txt com '%s' (tem: %s)\n", agulha, s); exit(1); }
  free(s);
}

static void apagar(void) {
  dados_apagar(COMECAR_ARQ);
  comecar_teste_esquecer();
}

// Dados de pessoa de uma instalacao que vem da 2.0.x.
static void apagar_usuario(void) {
  dados_apagar("sessao.txt");
  dados_apagar("perfis.txt");
  dados_apagar("progresso.txt");
}

// Com o teclado da TV aberto: entrega `txt` como o teclado da TV faz, leva o
// foco do teclado a "pronto" so com o D-pad e confirma.
static void digitar(const char *txt) {
  int i;
  assert(teclado_aberto());
  teclado_teste_modos(0, 0, 0);
  teclado_teste_sistema(txt);
  assert(!strcmp(teclado_texto(), txt));
  for (i = 0; i < 12 && teclado_foco_tipo() != TECLADO_FOCO_ACOES; i++) tecla(SDLK_DOWN);
  assert(teclado_foco_tipo() == TECLADO_FOCO_ACOES);
  tecla(SDLK_RIGHT);   // 'limpar'
  tecla(SDLK_RIGHT);   // 'pronto'
  tecla(SDLK_RETURN);
  assert(!teclado_aberto());
  quadro(0.016f);      // comecar_atualizar le o PRONTO e guarda o nome
}

// Leva o foco a um item pelo D-pad: sobe ao campo, ou desce ao rodape e anda de lado.
static void ir_para(int id) {
  static const SDL_Keycode keys[] = {SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN};
  int start = foco, queue[128], parent[128], via[128], head = 0, tail = 0;
  for (int i = 0; i < 128; i++) parent[i] = -1;
  queue[tail++] = start; parent[start] = start;
  TXT_INI();
  while (head < tail && parent[id] < 0) {
    int current = queue[head++];
    for (int d = 0; d < 4; d++) {
      foco = current; mover(d == 1 ? 1 : d == 0 ? -1 : 0, d == 3 ? 1 : d == 2 ? -1 : 0);
      if (parent[foco] < 0) { parent[foco] = current; via[foco] = d; queue[tail++] = foco; }
    }
  }
  TXT_FIM();
  foco = start; assert(parent[id] >= 0);
  int path[128], n = 0;
  for (int i = id; i != start; i = parent[i]) path[n++] = via[i];
  while (n) tecla(keys[path[--n]]);
  assert(comecar_foco() == id);
}

static void previa(const char *conteudo) {
  apagar();
  dados_gravar(COMECAR_ARQ, conteudo);
  comecar_teste_esquecer();
  comecar_abrir();
}

// O D-pad tem de levar a todo item: comeca em cada foco de partida, anda em
// cada direcao e confere que o foco continua valido (id >= 0) e que a tela
// continua no mesmo passo — nenhum item rouba o OK do D-pad e nenhuma direcao
// tranca. Volta ao foco de partida por CIMA e ESQUERDA: escapar tambem conta.
static void navegacao(int passoEsperado) {
  int partida, dir, volta, andadas = 0;
  for (partida = 0; partida < 12; partida++) {
    apagar();
    dados_gravar(COMECAR_ARQ, "estado=nenhum\nnome=Ana\n");
    comecar_abrir();
    while (comecar_passo() != passoEsperado) {
      if (comecar_passo() == 2 && comecar_foco() == 0) { tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); }
      tecla(SDLK_RETURN);
    }
    // percorre o passo com DOWN/RIGHT para partir de lugares diferentes
    for (volta = 0; volta < partida; volta++) { tecla(SDLK_RIGHT); tecla(SDLK_DOWN); }
    for (dir = 0; dir < 4; dir++) {
      tecla(dir == 0 ? SDLK_RIGHT : dir == 1 ? SDLK_LEFT : dir == 2 ? SDLK_DOWN : SDLK_UP);
      assert(comecar_foco() >= 0);
      assert(comecar_passo() == passoEsperado || comecar_foco() >= 100);
      if (comecar_passo() != passoEsperado) break;   // botao avancou: recomeca
      andadas++;
    }
    apagar();
  }
  assert(andadas > 0);   // o D-pad mexeu alguma coisa em algum passo
}

static void motion_proof(const char *prefix, int step) {
  static const int times[] = {0,50,80,100,150,160,250,320,400};
  char path[900];
  for (int kind = 0; kind < 5; kind++) {
    previa("estado=nenhum\nnome=Marina\ncor=0\n");
    traktauth_cancelar();   // the offline import press above fails late: show the idle Trakt card
    ir_passo(step);
    if (kind) {
      adiantar(60);
      if (kind == 1) {
        int old = foco;
        tecla(step < 3 ? SDLK_UP : SDLK_RIGHT);
        assert(foco != old);
      } else if (kind == 2 || kind == 3) {
        pular();
        if (kind == 3) { quadro(.05f); aberto = 1; }
      } else {
        ir_passo((step + 1) % 4);
        assert(paginaAnterior.tex);
      }
    }
    int previous = 0;
    const char *kinds[] = {"entry", "focus", "exit", "reversal", "page"};
    for (unsigned i = 0; i < sizeof times / sizeof *times; i++) {
      for (int t = previous; t < times[i]; t += 10) quadro(.01f);
      if (!times[i]) quadro(0);
      snprintf(path, sizeof path, "%s-step%d-%s-%03d.png", prefix, step + 1, kinds[kind], times[i]);
      captura(path, 0); previous = times[i];
    }
    if (kind == 1) assert(focoAnim[foco] > .99f);
    if (kind == 2) assert(entrada == 0);
    if (kind == 3) assert(entrada == 1);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-comecar";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;

  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("comecar-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  char art[1024]; assert(realpath("deploy/app/art", art));
  gfx_icones_dir(art);
  dados_gravar("ajustes.txt", "idioma 0\n"); ajustes_dir(dir);
  ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO") ? atoi(getenv("NUVIO_SHOT_VIDRO")) : 0);

  if (getenv("NUVIO_NAV21_REGRESSION")) {
    int failures = 0;
    previa("estado=nenhum\nnome=Ana\n");
    tecla(SDLK_RETURN); tecla(SDLK_RETURN);
    ir_para(I_TV); tecla(SDLK_RETURN); tecla(SDLK_AC_BACK);
    if (comecar_passo() != 2 || comecar_foco() != I_TV) {
      puts("FAIL: Back must restore Only on this TV focus"); failures++;
    }
    ir_para(I_TV); tecla(SDLK_RETURN); SDL_Delay(260); tecla(SDLK_AC_BACK);
    if (comecar_foco() != I_TV) { puts("FAIL: repeated Back focus restoration"); failures++; }
    CatItem item = {0}; CatFileira row = {0};
    snprintf(item.titulo, sizeof item.titulo, "Fallout");
    snprintf(item.backdrop, sizeof item.backdrop, "design/universo-2026/assets/00.jpg");
    snprintf(row.chave, sizeof row.chave, "continue_watching"); row.n = 1;
    cat_definir_tudo(&item, 1, &row, 1);
    const float geometryScales[] = {.85f, 1};
    for (unsigned i = 0; i < sizeof geometryScales / sizeof *geometryScales; i++) {
      gfx_escala_txt_definir(geometryScales[i]);
      ir_passo(3); entrada = pagina = 1; tempoPasso = 2;
      measureSummary = 1; summaryLines = 0; quadro(0); measureSummary = 0;
      float k = geometryScales[i];
      if (fabsf(previewRect.x * k - 1040) > 4 || fabsf(previewRect.y * k - 210) > 4 ||
          fabsf(previewRect.w * k - 760) > 4 || fabsf(previewRect.h * k - 428) > 4) {
        printf("FAIL: preview at %.0f%%: %.1f %.1f %.1f %.1f\n", k * 100,
               previewRect.x*k, previewRect.y*k, previewRect.w*k, previewRect.h*k); failures++;
      }
      if (summaryLines != 4 || fabsf(summaryY[1] - summaryY[0] - 84) > 4 ||
          fabsf(summaryY[3] - summaryY[2] - 84) > 4) {
        printf("FAIL: summary row pitch at %.0f%%: %.1f\n", k * 100, summaryY[1] - summaryY[0]); failures++;
      }
    }
    gfx_escala_txt_definir(1);
    if (failures) return 1;
    for (int step = 0; step < PASSOS; step++) {
      previa("estado=nenhum\nnome=Ana\n");
      while (passo < step) { ir_para(B_PRINC); tecla(SDLK_RETURN); }
      Item items[MAX_ITENS];
      int count = montar(items);
      for (int i = 0; i < count; i++) ir_para(items[i].id);
      if (step < PASSOS - 1) {
        ir_para(B_PRINC); adiantar(40); tecla(SDLK_RETURN);
        assert(paginaAnterior.tex && pagina == 0);
        quadro(.05f); assert(pagina > 0 && pagina < 1);
        adiantar(40); assert(!paginaAnterior.tex && pagina == 1);
        SDL_Delay(260); tecla(SDLK_AC_BACK);
        assert(passo == step && foco == B_PRINC);
      }
    }
    adiantar(40); pular(); quadro(.05f); assert(entrada > 0 && entrada < 1);
    adiantar(40); assert(!comecar_visivel());
    puts("PASS: review regressions: Back focus, mock geometry, reachability and transitions");
  }

  // ---- Regras, por evento de tecla. (Ids: 0=campo, 1..6=cores, 100=Voltar,
  // 101=Continuar/principal, 102=secundario, 103=Pular tudo.)
  // Passo 1 (Perfil): o foco inicial e o campo; OK abre o teclado da TV, o
  // nome entra por ele (so D-pad) e o foco sobe para o Continuar.
  apagar(); apagar_usuario(); comecar_abrir();
  assert(comecar_passo() == 0 && comecar_aberto());
  assert(comecar_foco() == 0);
  assert(!existe());             // abrir nao grava nada
  tecla(SDLK_RETURN);            // abre o teclado
  assert(teclado_aberto());
  digitar("so");
  assert(comecar_foco() == 101);
  assert(!existe());             // o nome so vai ao disco no fim
  // Cancelar o teclado (Voltar nele) deixa o nome como estava e NAO pula o fluxo.
  ir_para(0); tecla(SDLK_RETURN);   // campo: reabre
  assert(teclado_aberto());
  tecla(SDLK_ESCAPE);
  quadro(0.016f);
  assert(!teclado_aberto() && comecar_passo() == 0 && comecar_aberto() && !existe());
  // Mock 3A order: field, colours, kids row, language row, then the footer.
  assert(comecar_foco() == 0);
  tecla(SDLK_DOWN);
  assert(comecar_foco() >= 1 && comecar_foco() <= 6);
  tecla(SDLK_DOWN);
  assert(comecar_foco() == I_INFANTIL);
  { int k = perfil_infantil_ativo();   // OK flips the real per-profile mark (no PIN here)
    tecla(SDLK_RETURN); assert(perfil_infantil_ativo() == !k);
    tecla(SDLK_RETURN); assert(perfil_infantil_ativo() == k); }
  tecla(SDLK_DOWN);
  assert(comecar_foco() == I_IDIOMA);
  { char antes[64]; snprintf(antes, sizeof antes, "%s", ajustes_idioma_nome());
    tecla(SDLK_RETURN); assert(strcmp(antes, ajustes_idioma_nome()));
    dados_gravar("ajustes.txt", "idioma 0\n"); ajustes_dir(dir); }
  tecla(SDLK_DOWN);
  assert(comecar_foco() >= 100);
  assert(!existe());
  // Escolher cor nao grava; Continuar com o nome feito avanca.
  ir_para(0); tecla(SDLK_DOWN); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_RETURN);   // cor 2
  assert(!existe());
  ir_para(101); tecla(SDLK_RETURN);
  assert(comecar_passo() == 1 && !existe());
  // Continuar com nome vazio reabre o teclado em vez de avancar.
  apagar(); comecar_abrir();
  ir_para(101); tecla(SDLK_RETURN);
  assert(comecar_passo() == 0 && teclado_aberto());
  tecla(SDLK_ESCAPE);
  quadro(0.016f);
  assert(comecar_passo() == 0 && !teclado_aberto() && comecar_aberto());

  // Passo 2 (Trazer): Stremio e AIOManager comecam o import e ficam no passo.
  previa("estado=nenhum\nnome=so\n");
  tecla(SDLK_RETURN);            // nome ja ha: Continuar
  assert(comecar_passo() == 1);
  assert(comecar_foco() == B_PRINC);
  ir_para(I_STREMIO); tecla(SDLK_RETURN);   // Stremio stays on this step.
  assert(comecar_passo() == 1);
  ir_para(I_AIO); tecla(SDLK_RETURN);                  // AIOManager stays on this step.
  assert(comecar_passo() == 1);
  importarfluxo_cancelar();
  // Voltar (botao) volta ao 1 sem perder o nome.
  ir_para(100); tecla(SDLK_RETURN);
  assert(comecar_passo() == 0);
  // De volta ao 2: "Pular este passo" (o principal) tambem avanca.
  tecla(SDLK_RETURN);
  assert(comecar_passo() == 1);
  ir_para(101); tecla(SDLK_RETURN);
  assert(comecar_passo() == 2);

  // Passo 3 (Acompanhar): the default chips step the real settings in place.
  assert(comecar_foco() == B_PRINC);
  ir_para(I_CH_CW); assert(comecar_foco() >= I_CH_CW && comecar_foco() <= I_CH_HIST);
  // "Importar o que ja foi visto" is an action, not a setting: no tracker here,
  // so OK starts the Trakt code and the step stays put.
  ir_para(I_CH_HIST);
  tecla(SDLK_RETURN); assert(comecar_passo() == 2);
  ir_para(I_CH_CW);
  { char v[64]; snprintf(v, sizeof v, "%s", ajustes_rapido_valor(op_chip(I_CH_CW)));
    tecla(SDLK_RETURN); assert(strcmp(v, ajustes_rapido_valor(op_chip(I_CH_CW))));
    while (strcmp(v, ajustes_rapido_valor(op_chip(I_CH_CW)))) tecla(SDLK_RETURN); }
  assert(comecar_passo() == 2);
  contem("estado=nenhum");   // chips change settings, never comecar.txt
  ir_para(I_TRAKT);
  // "So nesta TV" avanca; Back volta um passo.
  ir_para(I_TV); tecla(SDLK_RETURN);
  assert(comecar_passo() == 3);
  tecla(SDLK_AC_BACK);
  assert(comecar_passo() == 2 && comecar_aberto() && comecar_foco() == I_TV);
  tecla(SDLK_RETURN);
  assert(comecar_passo() == 3);

  // Passo 4 (Pronto): OK no principal = Ir para a Home (NADA), grava feito.
  assert(comecar_foco() == 101);
  tecla(SDLK_RETURN);
  assert(!comecar_aberto());
  assert(comecar_pedido() == COMECAR_PEDIU_NADA);
  contem("estado=feito"); contem("nome=so");
  nao_contem("infantil"); nao_contem("andamento");
  comecar_decidir(1);            // feito: nunca mais, nem com login novo
  assert(!comecar_aberto());
  // O secundario (Fazer o tour) pede o tour e tambem grava feito.
  previa("estado=nenhum\nnome=so\n");
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);   // passo 3
  ir_para(I_TV); tecla(SDLK_RETURN);   // Finish step.
  ir_para(102);
  for (int f = 0; f < 40; f++) comecar_atualizar(0.016f, SDL_GetTicks());
  tecla(SDLK_RETURN);   // secundario
  assert(!comecar_aberto());
  // Exit keeps painting (fade over the app) until it settles, not a cut.
  assert(comecar_visivel());
  comecar_atualizar(0.06f, SDL_GetTicks()); assert(comecar_visivel());
  for (int f = 0; f < 40; f++) comecar_atualizar(0.016f, SDL_GetTicks());
  assert(!comecar_visivel());
  assert(comecar_pedido() == COMECAR_PEDIU_TOUR);
  contem("estado=feito");

  // Escolhas e nome gravam JUNTOS, so no resumo: cor 2 + nome do teclado.
  apagar(); comecar_abrir();
  tecla(SDLK_RETURN); digitar("Lia");
  ir_para(0); tecla(SDLK_DOWN); tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  assert(!existe());
  ir_para(101); tecla(SDLK_RETURN); tecla(SDLK_RETURN);   // passo 3
  assert(comecar_passo() == 2 && !existe());
  ir_para(I_TV); tecla(SDLK_RETURN);   // Finish step.
  assert(comecar_passo() == 3 && !existe());
  // Fechar o app aqui (antes de confirmar) nao deixa rastro.
  comecar_teste_esquecer();
  assert(!existe());
  comecar_abrir();
  tecla(SDLK_RETURN); digitar("Lia");
  ir_para(101); tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  ir_para(I_TV); tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);            // Ir para a Home
  assert(!comecar_aberto());
  contem("estado=feito"); contem("nome=Lia");

  // ---- Saidas.
  // Voltar no passo 1 pula o fluxo todo: feito, nada do que foi digitado.
  apagar(); comecar_abrir();
  tecla(SDLK_RETURN); digitar("zz");
  tecla(SDLK_ESCAPE);
  assert(!comecar_aberto() && comecar_pedido() == COMECAR_PEDIU_NADA);
  contem("estado=feito"); nao_contem("zz"); contem("cor=0");
  comecar_decidir(1);
  assert(!comecar_aberto());
  // ...e, reaberto por Ajustes, deixa como estava o que ja estava gravado.
  previa("estado=feito\nnome=Ana\ncor=3\n");
  tecla(SDLK_RETURN);            // passo 2
  tecla(SDLK_AC_BACK);           // passo 1
  SDL_Delay(260);                // segundo toque fora do debounce
  tecla(SDLK_AC_BACK);           // pula
  assert(!comecar_aberto());
  contem("estado=feito"); contem("nome=Ana"); contem("cor=3");
  // main.c traduz o scancode LG 482 em um KEYDOWN AC_BACK zerado.
  // Duas entregas desse caminho em menos de 700 ms so voltam um passo.
  previa("estado=nenhum\nnome=Ana\n");
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);   // passo 3
  {
    SDL_Event back; SDL_zero(back);
    Uint32 inicio = SDL_GetTicks();
    back.type = SDL_KEYDOWN;
    back.key.keysym.sym = SDLK_AC_BACK;
    comecar_evento(&back);
    assert(comecar_passo() == 1 && comecar_aberto());
    SDL_Delay(100);
    assert(SDL_GetTicks() - inicio < 700);
    comecar_evento(&back);
    assert(comecar_passo() == 1 && comecar_aberto());
    nao_contem("feito");
    // O KEYUP LG segue sem traducao; a rajada seguinte tambem e ignorada.
    SDL_Event up; SDL_zero(up);
    up.type = SDL_KEYUP;
    up.key.keysym.scancode = (SDL_Scancode)482;
    comecar_evento(&up);
    comecar_evento(&back);
    assert(comecar_passo() == 1 && comecar_aberto());
    SDL_Delay(260);
    comecar_evento(&back);   // novo toque fora do debounce
    assert(comecar_passo() == 0 && comecar_aberto());
  }
  // Voltar segurado pula de qualquer passo; a repeticao nao volta passo.
  previa("estado=nenhum\nnome=Ana\n");
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);   // passo 3
  assert(comecar_passo() == 2);
  tecla_em(SDLK_AC_BACK, 0, 0);             // primeira descida: um passo
  assert(comecar_passo() == 1 && comecar_aberto());
  SDL_Delay(300);
  tecla_em(SDLK_AC_BACK, 0, 0);             // evento LG: repeat/timestamp zerados
  SDL_Delay(300);
  tecla_em(SDLK_AC_BACK, 1, 9000);          // timestamp falso nao pode pular
  assert(comecar_passo() == 1 && comecar_aberto());
  nao_contem("feito");
  SDL_Delay(150);
  tecla_em(SDLK_AC_BACK, 0, 0);             // >= 700 ms no relogio do Comecar
  assert(!comecar_aberto() && comecar_pedido() == COMECAR_PEDIU_NADA);
  contem("estado=feito"); contem("nome=Ana");
  // Soltar encerra a pressao: outro toque apos 700 ms nao e pressao longa.
  previa("estado=nenhum\nnome=Ana\n");
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  tecla(SDLK_AC_BACK);
  SDL_Delay(750);
  tecla(SDLK_AC_BACK);
  assert(comecar_aberto() && comecar_passo() == 0);
  // "Pular tudo" (visivel no rodape) pula de qualquer passo.
  apagar(); comecar_abrir();
  ir_para(103); tecla(SDLK_RETURN);
  assert(!comecar_aberto());
  contem("estado=feito"); contem("nome=\n");
  previa("estado=nenhum\nnome=Ana\n");
  tecla(SDLK_RETURN);            // passo 2
  ir_para(103); tecla(SDLK_RETURN);
  assert(!comecar_aberto());
  contem("estado=feito");
  previa("estado=nenhum\nnome=Ana\n");
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);   // passo 3
  ir_para(103); tecla(SDLK_RETURN);
  assert(!comecar_aberto());
  contem("estado=feito");

  // ---- Upgrade: dados de pessoa de antes + sem comecar.txt = nunca abre.
  { static const char *const ARQS[3] = { "sessao.txt", "perfis.txt", "progresso.txt" };
    int k;
    for (k = 0; k < 3; k++) {
      apagar(); apagar_usuario();
      dados_gravar(ARQS[k], "x\n");
      comecar_marcar_dados();
      comecar_decidir(0);                     // sessao restaurada
      assert(!comecar_aberto() && !existe());
      comecar_teste_esquecer();
      dados_gravar(ARQS[k], "x\n");
      comecar_marcar_dados();
      comecar_decidir(1);                     // login novo nessa mesma instalacao
      assert(!comecar_aberto() && !existe());
    } }
  // Instalacao limpa: login novo abre os 4 passos; sessao restaurada nao abre.
  apagar(); apagar_usuario();
  comecar_marcar_dados();
  comecar_decidir(1);
  assert(comecar_aberto() && comecar_passo() == 0);
  apagar(); apagar_usuario();
  comecar_marcar_dados();
  comecar_decidir(0);
  assert(!comecar_aberto());

  navegacao(0); navegacao(1); navegacao(2); navegacao(3);

  // ---- Fixtures for the captures: real art in Continue watching, real add-on count.
  { static const char *const ARTE[4][4] = {
      { "Fallout", "design/universo-2026/assets/00.jpg", "1", "38" },
      { "O Grande Truque", "design/universo-2026/assets/02.jpg", "0", "52" },
      { "From", "design/universo-2026/assets/04.jpg", "3", "19" },
      { "Os Suspeitos", "design/universo-2026/assets/09.jpg", "0", "71" } };
    static CatItem cw[4];
    CatFileira f; memset(&f, 0, sizeof f);
    for (int i = 0; i < 4; i++) {
      memset(&cw[i], 0, sizeof cw[i]);
      snprintf(cw[i].titulo, sizeof cw[i].titulo, "%s", ARTE[i][0]);
      snprintf(cw[i].backdrop, sizeof cw[i].backdrop, "%s", ARTE[i][1]);
      snprintf(cw[i].imdb, sizeof cw[i].imdb, "tt000000%d", i);
      cw[i].temporada = atoi(ARTE[i][2]); cw[i].episodio = cw[i].temporada ? 8 : 0;
      cw[i].restanteMin = atoi(ARTE[i][3]);
      snprintf(cw[i].tipo, sizeof cw[i].tipo, "%s", cw[i].temporada ? "series" : "movie");
    }
    snprintf(f.chave, sizeof f.chave, "continue_watching");
    snprintf(f.titulo, sizeof f.titulo, "Continuar assistindo");
    f.n = 4;
    cat_definir_tudo(cw, 4, &f, 1);
    // Step 4 library line reads real state: two saved, two watched.
    for (int i = 0; i < 4; i++) {
      if (i % 2) cat_historico_definir_id(cw[i].imdb, cw[i].tipo, 1);
      else salvos_definir(&cw[i], 1);
    }
    AddonRemoto ad[3] = {{"Cinemeta","https://v3-cinemeta.strem.io/manifest.json",1},
                         {"Torrentio","https://torrentio.strem.fun/manifest.json",1},
                         {"OpenSubtitles","https://opensubtitles-v3.strem.io/manifest.json",1}};
    assert(addons_definir_lista(ad, 3));
    for (int c = 0; c < ajustes_rapido_cor_n(); c++)   // the mock's accent (user choice; DESIGN §3)
      if (!strcmp(ajustes_rapido_cor_nome(c), "Rosa")) ajustes_rapido_cor_definir(c);
    for (int i = 0; i < 40; i++) { tex_bombear(8); SDL_Delay(5); } }

  // ---- Captures at the requested text scale (rules above stay at 100%).
  // Match Library's pinned Modern rail when comparing real content anchors.
  if (getenv("NUVIO_RAIL") && !strcmp(getenv("NUVIO_RAIL"), "moderna")) {
    dados_gravar("ajustes.txt", "homeLayoutLocal 0\nmodernSidebar 0\ncollapseSidebar 1\n");
    ajustes_dir(dir);
  }
  ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO") ? atoi(getenv("NUVIO_SHOT_VIDRO")) : 0);
  const char *requestedScale = getenv("NUVIO_SHOT_ESCALA_TEXTO");
  const float scales[] = { .8f, .85f, .9f, 1, 1.1f, 1.2f, 1.3f };
  for (unsigned scaleIndex = 0; scaleIndex < sizeof scales / sizeof *scales; scaleIndex++) {
  float scale = requestedScale ? (float)atof(requestedScale) : scales[scaleIndex];
  // nav21 acceptance covers the review's 85%/100% matrix; standalone capture keeps every scale.
  if (getenv("NUVIO_NAV21_REGRESSION") && !requestedScale && scale != .85f && scale != 1) continue;
  char prefix[700];
  snprintf(prefix, sizeof prefix, "%s", saida);
  if (!requestedScale) snprintf(prefix, sizeof prefix, "%s%.0f", saida, scale * 100);
  gfx_escala_txt_definir(scale);
  checkBounds = 1;
  previa("estado=nenhum\nnome=Marina\ncor=0\n");
  assert(comecar_passo() == 0 && comecar_foco() == B_PRINC);
  snprintf(nome, sizeof nome, "%s-1-perfil.png", prefix);
  captura(nome, 40);

  tecla(SDLK_RETURN);
  assert(comecar_passo() == 1);
  snprintf(nome, sizeof nome, "%s-2-trazer.png", prefix);
  captura(nome, 40);

  // Stremio e AIOManager: estados forcados (sem rede) so para a captura.
  ir_para(I_STREMIO);
  importarfluxo_teste(IMPF_STREMIO, IMPF_ESPERA, "K7QX2M", "https://link.stremio.com/", 0, 0);
  snprintf(nome, sizeof nome, "%s-2b-stremio-codigo.png", prefix);
  captura(nome, 40);
  assert(!strcmp(ultimoCodigo, "K7QX2M") && ultimoLink[0]);
  importarfluxo_teste(IMPF_STREMIO, IMPF_OK, "", "", 7, 42);
  assert(!strcmp(rotulo_botao(B_PRINC), i18n("Continuar")));
  snprintf(nome, sizeof nome, "%s-2c-stremio-ok.png", prefix);
  captura(nome, 40);
  assert(!strcmp(ultimoCodigo, "K7QX2M") && ultimoLink[0]);
  ir_para(I_AIO);
  importarfluxo_teste(IMPF_AIOM, IMPF_ESPERA, "", "", 0, 0);
  celb_abrir_embutido(CELB_IMPORTAR, "Link do AIOManager");
  for (int i = 0; i < 90; i++) celb_atualizar(0.016f);   // a mola do cartao
  snprintf(nome, sizeof nome, "%s-2d-aiom-link.png", prefix);
  captura(nome, 40);
  importarfluxo_cancelar();
  ir_para(I_NUVIO);
  tecla(SDLK_RETURN);   // Conta Nuvio
  assert(comecar_passo() == 2);
  shotContas = 1; foco = I_SIMKL;
  snprintf(nome, sizeof nome, "%s-3-acompanhar.png", prefix);
  captura(nome, 40);
  shotContas = 0; foco = I_TRAKT;

  ir_para(I_TV); tecla(SDLK_RETURN);
  assert(comecar_passo() == 3);
  shotContas = 1;   // mock 3D: tracker linked, collections on Home, skip intro on
  if (!col_n()) assert(col_definir_json("[{\"id\":\"c1\",\"title\":\"Casa\",\"folders\":["
    "{\"id\":\"f1\",\"title\":\"Cl\xc3\xa1ssicos\",\"sources\":[{\"provider\":\"addon\",\"addonBaseUrl\":\"https://x.test\",\"type\":\"movie\",\"catalogId\":\"top\"}]}]},"
    "{\"id\":\"c2\",\"title\":\"Anime\",\"folders\":["
    "{\"id\":\"f2\",\"title\":\"Temporada\",\"sources\":[{\"provider\":\"addon\",\"addonBaseUrl\":\"https://x.test\",\"type\":\"series\",\"catalogId\":\"top\"}]}]}]") > 0);
  if (!ajustes_pular_abertura()) ajustes_rapido_passo(ajustes_rapido_op("pularAberturaLocal"), 1);
  assert(col_n() > 1 && ajustes_pular_abertura());
  snprintf(nome, sizeof nome, "%s-4-pronto.png", prefix);
  // Continue watching art decodes at this scale's width first: settle until none pending.
  adiantar(160);
  for (int k = 0, pend = 1; pend && k < 600; k++) { tex_estatisticas(NULL, &pend, NULL, NULL, NULL); adiantar(1); }
  captura(nome, 20);
  shotContas = 0;

  checkBounds = 0;
  if (!getenv("NUVIO_SHOT_QUICK") && (scale == 1 || scale == .85f)) for (int step = 0; step < 4; step++) motion_proof(prefix, step);
  if (requestedScale) break;
  }

  // O teclado da TV por cima do passo 1: aberto pelo OK, desenhado pelo Comecar.
  apagar(); comecar_abrir();
  tecla(SDLK_RETURN);
  assert(teclado_aberto());
  snprintf(nome, sizeof nome, "%s-5-teclado.png", saida);
  captura(nome, 40);
  tecla(SDLK_ESCAPE);

  const char *modes[] = {"collapseSidebar 1\nmodernSidebar 1\nhomeLayoutLocal 0\n", "collapseSidebar 0\nmodernSidebar 1\nhomeLayoutLocal 0\n", "collapseSidebar 1\nmodernSidebar 1\nhomeLayoutLocal 2\n"};
  gfx_escala_txt_definir(1);
  for (int m = 0; m < 3; m++) for (int reduced = 0; reduced < 2; reduced++) {
    char settings[256];
    snprintf(settings, sizeof settings, "idioma 0\nperformanceAutoVersaoLocal 1\nselected_theme 6\nanimacoes %d\n%s", reduced, modes[m]);
    dados_gravar("ajustes.txt", settings); ajustes_dir(dir);
    ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO") ? atoi(getenv("NUVIO_SHOT_VIDRO")) : 0);
    previa("estado=nenhum\nnome=Ana\ncor=1\n");
    ir_passo(1);
    snprintf(nome, sizeof nome, "%s-rail%d-reduced%d.png", saida, m, reduced);
    captura(nome, 40);
    if (reduced) { assert(ajustes_animacoes_reduzidas()); assert(focoAnim[foco] == 1); }
  }
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: Onboarding rules, rendered bounds and controls at requested capture scales.");
  return 0;
}
