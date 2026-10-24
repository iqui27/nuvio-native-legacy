// O MINI GUIA SEM TECLA COLORIDA (dono, 18/10/2026, TCL: "ta faltando o guide
// overlay quando ta tocando"). Canal em tela cheia sem OSD: OK abre a faixa,
// CIMA/BAIXO andam sem zapear, OK em outro canal zapeia, OK no canal que toca
// abre o guia completo, Voltar fecha. No filme o OK segue sendo do player.
//
//   bash tests/guiaoverlay_ok.sh               so as teclas
//   bash tests/guiaoverlay_ok.sh /tmp/x/shot   + x/shot-faixa.png e -guia.png
#include "plrui.h"
#include "idioma.h"
#include "idiomacod.h"
static int fullGuideHintTranslated;
static float guideHints(const char *const *keys, const char *const *labels, int n,
                        float x, float y, int right, float alpha);
#define plrui_dicas guideHints
#include "../src/guia.c"
#undef plrui_dicas

static float guideHints(const char *const *keys, const char *const *labels, int n,
                        float x, float y, int right, float alpha) {
  if (n == 3 && !strcmp(keys[0], "OK") && !strcmp(keys[1], "OK"))
    fullGuideHintTranslated = !strcmp(i18n(labels[1]), "Full guide");
  return plrui_dicas(keys, labels, n, x, y, right, alpha);
}
#include "catalogo.h"
#include "app.h"
#include "tpkteclas.h"
#include "player.h"
#include "video.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "shot_arte.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include "guide_requests.h"

typedef struct { const char *grupo, *nome; int numero; } TCanal;
static void semear(const TCanal *v, int n) {
  int i;
  stagingZerar();
  sTeto = 30000;
  for (i = 0; i < n; i++) {
    int fl = sFonte(LTV_ADDON, "FrostView", "FrostView", "");
    char id[32];
    GCru c;
    snprintf(id, sizeof id, "c%d", i);
    memset(&c, 0, sizeof c);
    c.id = id; c.nome = v[i].nome; c.logo = ""; c.desc = "";
    c.grupo = v[i].grupo; c.numero = v[i].numero;
    c.epgId = c.tvgNome = c.url = c.ua = c.ref = "";
    sGrupoInfo(fl, v[i].grupo, 1);
    sCanalAdd(fl, &c);
    sLtvF[fl].total++;
  }
  favLido = 1;
  adotarStaging();
  estado = G_PRONTO;
}

static void tecla(SDL_Keycode k, int sc) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; e.key.keysym.scancode = sc;
  app_evento(&e);
  e.type = SDL_KEYUP; app_evento(&e);
}

// O que app.c faz com player_pediu_guia (app.c, "BAIXO/AZUL COM CANAL NO AR").
static void abrirFaixa(void) {
  guia_overlay_abrir();
  guia_focar_id(player_id_canal());
  overlayDesde -= 1000;   // passou o repouso da tecla que abriu
}

static void comFonte(void) {
  static const VideoSimulacao sim = {.aceita = 1, .pronto = 1, .tocando = 1};
  video_simular(&sim); player_definir_fonte("https://example.invalid/c.m3u8");
}

// O primeiro quadro acorda o OSD; o prazo de esconder corre depois dele.
static void esconderOsd(void) {
  static Uint32 t;
  if (!t) t = SDL_GetTicks();
  t += 60000; player_atualizar(0.016f, t);
  t += 60000; player_atualizar(0.016f, t);
  assert(!player_controles_visiveis());
}

static void foto(const char *nome, SDL_Window *w) {
  int q, y;
  for (q = 0; q < 70; q++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    shot_arte_desenhar(0);   // Frozen fixture frame behind the guide.
    bandaUlt = SDL_GetTicks();
    guia_desenhar(SDL_GetTicks());
    if (q == 69) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(pix && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      if (player_mini_no_guia_ativo()) {
        // Composite the fixture behind the real transparent video hole on Mac.
        SDL_Surface *frame = IMG_Load(getenv("NUVIO_SHOT_ARTE"));
        SDL_Surface *back = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect pv = { G_PREVIEW_X, G_PREVIEW_Y, G_PREVIEW_W, G_PREVIEW_H };
        assert(frame && back);
        SDL_FillRect(back, NULL, SDL_MapRGBA(back->format, 0, 0, 0, 255));
        assert(SDL_BlitScaled(frame, NULL, back, &pv) == 0);
        SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_BLEND);
        assert(SDL_BlitSurface(s, NULL, back, NULL) == 0);
        SDL_FreeSurface(frame); SDL_FreeSurface(s); s = back;
      }
      assert(IMG_SavePNG(s, nome) == 0); SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(w);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  static const TCanal C[] = {
    { "Abertos", "Globo RJ", 4 }, { "Abertos", "GNT", 41 }, { "Abertos", "Cartoon Network", 33 },
    { "Esportes", "SporTV", 101 }, { "Esportes", "ESPN", 102 },
  };
  CatItem tv = {0}, it;
  char id[64];
  SDL_Event e = {0};
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  char settings[1024];
  snprintf(settings, sizeof settings, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(settings, "w");
  assert(f);
  fprintf(f, "performanceAutoVersaoLocal 1\nvidroLocal 1\nidioma %d\n", IDIOMA_EN);
  fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(ajustes_idioma() == IDIOMA_EN);
  assert(!ajustes_vidro());
  semear(C, 5);

  // CANAL NO AR, TELA CHEIA, SEM OSD: OK pede a faixa.
  snprintf(tv.tipo, sizeof tv.tipo, "channel");
  snprintf(tv.titulo, sizeof tv.titulo, "%s", canais[2].nome);
  snprintf(tv.imdb, sizeof tv.imdb, "%s", canais[2].id);
  snprintf(id, sizeof id, "%s", canais[2].id);
  cat_definir(&tv, 1); player_abrir(0, NULL); player_marcar_canal(&tv); comFonte();
  assert(player_aberto() && !strcmp(player_id_canal(), id));
  esconderOsd();
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  app_evento(&e);
  assert(player_pediu_guia() == 1 && !player_pediu_guia());
  // A repeticao do firmware do mesmo OK nao pede de novo.
  e.key.repeat = 1; esconderOsd(); player_evento(&e); assert(!player_pediu_guia());
  e.key.repeat = 0;

  // DOWN through app routing: opening may happen AFTER the frame timestamp.
  tecla(SDLK_DOWN, 0);
  testGuideRequests();
  assert(guia_overlay_aberta() && !strcmp(guia_id_focado(), id));
  { AddonRemoto addon = { .nome = "Fixture", .url = "https://example.invalid/manifest.json", .ativo = 1 };
    assert(addons_definir_lista(&addon, 1));
    ltv_chave_fonte(LTV_ADDON, addons_base(0), ltvF[0].chave, sizeof ltvF[0].chave); }
  guia_atualizar(0.016f, bandaUlt - 1u);
  assert(guia_overlay_aberta() && entrada > 0.01f && video_tocando());
  guia_atualizar(0.016f, bandaUlt + G_B_OCIOSO_MS);
  assert(guia_overlay_aberta());
  guia_atualizar(0.016f, bandaUlt + G_B_OCIOSO_MS + 1u);
  assert(!guia_overlay_aberta());
  // SDL ticks wrap: an ordinary short interval must remain visible.
  guia_overlay_abrir(); bandaUlt = (Uint32)-10;
  guia_atualizar(0.016f, 10u);
  assert(guia_overlay_aberta());
  overlay = 0;

  // A FAIXA: abre no canal que toca; o OK que a abriu, repetido dentro do
  // repouso, nao conta como o segundo OK.
  guia_overlay_abrir(); guia_focar_id(id);
  assert(guia_overlay_aberta() && !strcmp(guia_id_focado(), id));
  tecla(SDLK_RETURN, 0);
  assert(guia_overlay_aberta() && !guia_pediu_guia_cheio() && !guia_pediu_canal(&it));
  // Holding the opening OK past 400 ms must never arm short/long OK.
  for (int key = 0; key < 2; key++) {
    SDL_Keycode ok = key ? SDLK_KP_ENTER : SDLK_RETURN;
    guia_overlay_abrir(); guia_focar_id(id);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = ok; e.key.repeat = 1;
    app_evento(&e); // Repeat inside the opening suppression window.
    overlayDesde -= 1000;
    app_evento(&e); // Firmware repeats continue after the window expires.
    assert(!okDesde && !okLongo);
    e.type = SDL_KEYUP; app_evento(&e);
    assert(guia_overlay_aberta() && !guia_pediu_guia_cheio() && !guia_pediu_canal(&it));
    assert(!canais[2].fav);

    // A fresh press still arms OK; its repeats must not cancel that press.
    e.type = SDL_KEYDOWN; e.key.repeat = 0; app_evento(&e);
    assert(okDesde && !okLongo);
    e.key.repeat = 1; app_evento(&e);
    e.type = SDL_KEYUP; app_evento(&e);
    assert(!guia_overlay_aberta() && guia_pediu_guia_cheio() && !guia_pediu_canal(&it));
  }
  e.key.repeat = 0;
  abrirFaixa();
  // BAIXO/CIMA andam sem zapear.
  tecla(SDLK_DOWN, 0);
  assert(guia_overlay_aberta() && strcmp(guia_id_focado(), id) && !guia_pediu_canal(&it));
  tecla(SDLK_UP, 0);
  assert(!strcmp(guia_id_focado(), id));
  // Segundo OK no canal que toca: guia completo, sem zap.
  tecla(SDLK_RETURN, 0);
  assert(!guia_overlay_aberta() && guia_pediu_guia_cheio() == 1 && !guia_pediu_canal(&it));
  // OK em outro canal: zapeia e fecha.
  abrirFaixa(); tecla(SDLK_DOWN, 0); tecla(SDLK_RETURN, 0);
  assert(!guia_overlay_aberta() && !guia_pediu_guia_cheio());
  assert(guia_pediu_canal(&it) && !strcmp(it.imdb, canais[3].id));
  // LG BLUE scancode and keyboard alias keep opening the full guide.
  abrirFaixa(); tecla(SDLK_UNKNOWN, NV_SCANCODE_BLUE);
  assert(!guia_overlay_aberta() && guia_pediu_guia_cheio());
  abrirFaixa(); tecla(SDLK_s, 0);
  assert(!guia_overlay_aberta() && guia_pediu_guia_cheio() == 1);
  abrirFaixa(); tecla(SDLK_ESCAPE, 0);
  assert(!guia_overlay_aberta() && !guia_pediu_guia_cheio() && !guia_pediu_canal(&it));

  // Visible live OSD keeps OK; Guide/Menu also work while it is visible.
  tecla(SDLK_UP, 0);
  assert(player_controles_visiveis());
  tecla(SDLK_RETURN, 0);
  assert(!player_pediu_guia() && player_pediu_guia_cheio());
  assert(tpkteclas_evento("XF86ChannelGuide", 1, &e));
  app_evento(&e);
  assert(player_pediu_guia_cheio() && !player_pediu_guia());
  abrirFaixa(); tecla(SDLK_F10, 0);
  assert(!guia_overlay_aberta() && guia_pediu_guia_cheio());
  tecla(SDLK_MENU, 0);
  assert(player_pediu_guia_cheio());
  abrirFaixa(); tecla(SDLK_MENU, 0);
  assert(!guia_overlay_aberta() && guia_pediu_guia_cheio());

  // Xtream uses the same frozen live identity even after catalog replacement.
  snprintf(tv.imdb, sizeof tv.imdb, "xtream:42");
  player_marcar_canal(&tv);
  { CatItem movie = {0}; snprintf(movie.tipo, sizeof movie.tipo, "movie"); cat_definir(&movie, 1); }
  esconderOsd(); tecla(SDLK_KP_ENTER, 0);
  assert(player_pediu_guia() && !strcmp(player_id_canal(), "xtream:42"));
  // OSD Guide must keep the SAME stream even with a one-connection account.
  xtream_definir_conexoes_modo(1);
  assert(xtream_conexoes_limitadas());
  tecla(SDLK_UP, 0); tecla(SDLK_RETURN, 0);
  testGuideRequests();
  assert(player_mini_no_guia_ativo() && previewPreso);
  assert(!strcmp(previewId, "xtream:42") && video_tocando());
  // CONTA LIMITADA + Preview ON: foco descansado noutro canal TROCA o
  // preview — a sessao anterior fecha ANTES de a nova abrir (uma conexao).
  pedPreview = 1; snprintf(pedPreviewItem.imdb, sizeof pedPreviewItem.imdb, "xtream:43");
  testPreviewRequests();
  assert(testPreviewConnections == 1 && !player_mini_ativo());
  { CatItem novo = {0};
    snprintf(novo.tipo, sizeof novo.tipo, "channel");
    snprintf(novo.titulo, sizeof novo.titulo, "Canal 43");
    snprintf(novo.imdb, sizeof novo.imdb, "xtream:43");
    cat_definir(&novo, 1);
    int ni = cat_indice_por_imdb("xtream:43"); assert(ni >= 0);
    float x, y, w, h; guia_preview_rect(&x, &y, &w, &h);
    player_mini_no_guia(x, y, w, h); player_manter_mini();
    player_abrir(ni, NULL); player_marcar_canal(&novo); comFonte();
    // Mesmo com querMini=0 (conta limitada) o destino armado no guia segura
    // a sessao nova no preview — nao sobe a tela cheia.
    assert(player_mini_no_guia_ativo() && !strcmp(player_id_canal(), "xtream:43")); }
  player_restaurar();
  assert(player_aberto() && !strcmp(player_id_canal(), "xtream:43"));
  xtream_definir_conexoes_modo(0);

  // GUIA CHEIO, PREVIEW LIGADO, SEM CANAL NO AR: o foco descansado num canal
  // pede o preview (regressao TCL: preview ligado nunca arrancava).
  player_encerrar();
  aberta = 0; overlay = 0;
  previewLigado = 1; previewLido = 1; previewPreso = 0; previewId[0] = 0;
  guia_abrir();
  assert(aberta && previewLigado && !pedPreview);
  { Uint32 t0 = previewFocoMudou;
    guia_atualizar(0.016f, t0 + G_PREVIEW_ESPERA_MS - 1u);
    assert(!guia_pediu_preview(&it));            // dentro do descanso: nada
    guia_atualizar(0.016f, t0 + G_PREVIEW_ESPERA_MS + 1u);
    assert(guia_pediu_preview(&it) && !strcmp(it.imdb, guia_id_focado()));
  }
  snprintf(previewId, sizeof previewId, "%s", it.imdb); // o app ja atendeu
  // O app atende ao pedido: sessao "mini no guia" e o canal tocando.
  { float x, y, w, h;
    int ni;
    guia_preview_rect(&x, &y, &w, &h);
    player_mini_no_guia(x, y, w, h);
    player_manter_mini();
    ni = cat_indice_por_imdb(it.imdb);
    if (ni < 0) ni = cat_acrescentar(&it);
    assert(ni >= 0);
    player_abrir(ni, NULL); player_marcar_canal(&it); comFonte();
    assert(player_mini_no_guia_ativo() && video_pronto());
  }
  // CANAL TOCANDO NO PREVIEW + foco noutro canal ANTES do descanso: o heroi
  // segue furando para o video que toca (o foco nao esconde o canal no ar).
  { int sessao = aberta && !overlay && player_mini_no_guia_ativo();
    GCanal *outro = linhaItem(0, 1);
    assert(outro && strcmp(outro->id, previewId));
    int focado = sessao || previewFocoEh(outro->id);
    assert(focado); }
  guia_focar_id(previewId);   // o foco pode ter ido para o canal no ar
  // O furo do video abre so para o canal focado — por ID, nao por ponteiro:
  // o mesmo canal em duas fontes (M3U + addon) tem GCanal* diferentes.
  assert(previewFocoEh(guia_id_focado()));
  { int colFoco = focoCol, colOutro = focoCol ? 0 : 1;
    focoCol = colOutro;              // outro canal da mesma linha
    assert(!previewFocoEh(guia_id_focado()));
    focoCol = colFoco;
    assert(previewFocoEh(guia_id_focado())); }
  // MESMO ID EM DOIS GCanal (favorito/duplicata): o foco no duplicado ainda
  // conta como o canal do preview.
  { int dup = nCanais;
    canais = realloc(canais, sizeof(GCanal) * (size_t)(nCanais + 1));
    assert(canais);
    { GCanal *alvo = canalPorId(previewId);
      canais[dup] = alvo ? *alvo : canais[0]; }
    nCanais = dup + 1;
    // foco direto no duplicado: canalPorId acha o PRIMEIRO, linhaItem daria o segundo
    { GCanal *a = canalPorId(previewId);
      assert(a && a != &canais[dup]);
      assert(!strcmp(canais[dup].id, previewId));
      assert(previewFocoEh(canais[dup].id));
    }
    nCanais = dup;                   // restaura
  }
  // BACK num canal AO VIVO aberto do guia = botao GUIA: a MESMA sessao vai
  // para o preview, sem nova abertura (conta limitada inclusa: sem
  // player_minimizavel no caminho).
  { char tocandoAgora[64];
    snprintf(tocandoAgora, sizeof tocandoAgora, "%s", player_id_canal());
    player_restaurar();
    assert(player_aberto() && video_tocando());
    esconderOsd();
    tecla(SDLK_ESCAPE, 0);
    assert(player_quer_sair());
    aberta = 1;
    testBackToGuide();
    assert(player_mini_no_guia_ativo() && video_tocando());
    assert(!strcmp(player_id_canal(), tocandoAgora) && !strcmp(previewId, tocandoAgora));
    assert(previewPreso);
    // E volta a tela cheia pelo mesmo fluxo, para o resto do teste seguir.
    player_restaurar();
    assert(player_aberto());
    // De volta ao preview para o bloco seguinte (furo/opcoes).
    { float x, y, w, h;
      guia_preview_rect(&x, &y, &w, &h);
      player_minimizar_para_guia(x, y, w, h);
      guia_adotar_canal(tocandoAgora);
      assert(player_mini_no_guia_ativo()); } }
  // DESLIGA/LIGA pelo menu Opcoes: o preview volta a pedir no mesmo foco.
  topoAcionar(G_TOPO_OPCOES);
  assert(opcoesAberto);
  opcoesFoco = G_TOPO_PREVIEW;
  { SDL_Event ok = {0};
    ok.type = SDL_KEYDOWN; ok.key.keysym.sym = SDLK_RETURN;
    guia_evento(&ok); }
  assert(!previewLigado && guia_pediu_parar_preview());
  topoAcionar(G_TOPO_OPCOES);
  opcoesFoco = G_TOPO_PREVIEW;
  { SDL_Event ok = {0};
    ok.type = SDL_KEYDOWN; ok.key.keysym.sym = SDLK_RETURN;
    guia_evento(&ok); }
  assert(previewLigado && previewUltLin == -1);
  guia_atualizar(0.016f, SDL_GetTicks() + G_PREVIEW_ESPERA_MS + 1u);
  assert(guia_pediu_preview(&it) && !strcmp(it.imdb, guia_id_focado()));
  previewParar();

  // FILME: o OK sem OSD continua do player (acorda os controles), nunca o guia.
  player_encerrar();
  { CatItem filme = {0};
    snprintf(filme.tipo, sizeof filme.tipo, "movie");
    snprintf(filme.imdb, sizeof filme.imdb, "tt0111161");
    cat_definir(&filme, 1); player_abrir(0, NULL); comFonte(); }
  assert(!player_id_canal()[0]);
  esconderOsd();
  tecla(SDLK_RETURN, 0);
  assert(!player_pediu_guia() && !player_pediu_guia_cheio() && player_controles_visiveis());
  player_encerrar();
  puts("PASS: live OK/band, second OK/full guide, browse/zap/Back, Guide/Menu, Xtream, VOD unchanged; glass off.");

  if (argc > 1) {
    char nome[600];
    SDL_Window *w;
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    w = SDL_CreateWindow("guiaoverlay", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    assert(w && SDL_GL_CreateContext(w));
    glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
    assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
    gfx_icones_dir("deploy/app/art");
    // Every supported translation gets measured title/clock separation.
    for (int lang = 0; lang < IDIOMA_N; lang++) {
      f = fopen(settings, "w"); assert(f);
      fprintf(f, "idioma %d\nidiomaAutoLocal 0\n", lang); fclose(f);
      ajustes_dir(getenv("NUVIO_DADOS"));
      txt_novo_quadro();
      const char *labels[G_TOPO_N]; float widths[G_TOPO_N], xs[G_TOPO_N];
      gTopoRotulos(labels); gTopoLarguras(labels, widths);
      float right = G_AREA_DIR - txt_linha(TXT_CALLOUT, "23:59", 255, 255, 255, 255).w - 36;
      gTopoAjustar(widths, xs, right);
      float titleRight = G_AREA_X + txt_linha(TXT_ILHA_TITULO, i18n("Guia TV"), 255, 255, 255, 255).w;
      assert(xs[0] == G_AREA_X && titleRight < right);
      assert(G_CHIPS_Y >= G_TOPO_Y + G_TOPO_H + 12);
      for (int chip = 0; chip < G_TOPO_PRIMARIOS; chip++) {
        float icon = chip == G_TOPO_PREVIEW ? 20 :
                     (chip == G_TOPO_BUSCAR || chip == G_TOPO_CATEGORIAS) ? 26 : 0;
        if (widths[chip] <= 2 * G_CHIP_PAD + icon)
          fprintf(stderr, "header language=%d chip=%d width=%.1f text=%s area=%.1f\n", lang, chip, widths[chip], labels[chip], G_INFO_W);
        assert(widths[chip] > 2 * G_CHIP_PAD + icon);
        assert(xs[chip] + widths[chip] <= G_INFO_X + G_INFO_W + 0.1f);
        if (chip) assert(xs[chip] >= xs[chip - 1] + widths[chip - 1]);
      }
    }
    f = fopen(settings, "w"); assert(f);
    fprintf(f, "idioma %d\nidiomaAutoLocal 0\n", IDIOMA_EN); fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS"));
    snprintf(tv.imdb, sizeof tv.imdb, "%s", id);
    cat_definir(&tv, 1); player_abrir(0, NULL); player_marcar_canal(&tv); comFonte();
    aberta = 0;
    tecla(SDLK_DOWN, 0); testGuideRequests();
    guia_atualizar(0.016f, bandaUlt - 1u);
    assert(guia_overlay_aberta() && entrada > 0.01f && video_tocando());
    entrada = 1; guia_focar_id(id);
    snprintf(nome, sizeof nome, "%s-faixa.png", argv[1]); foto(nome, w);
    assert(fullGuideHintTranslated);
    { float x, y, w, h;
      guia_preview_rect(&x, &y, &w, &h);
      player_minimizar_para_guia(x, y, w, h);
      for (int step = 0; step < 40; step++)
        player_atualizar(0.016f, SDL_GetTicks() + (Uint32)step * 100u);
      assert(!player_janela_animando(NULL, NULL, NULL, NULL));
      guia_adotar_canal(id);
      assert(player_mini_no_guia_ativo() && previewPreso && !overlay);
    }
    entrada = heroA = 1; dicaDesde = 0; previewLigado = 1;
    snprintf(nome, sizeof nome, "%s-guia.png", argv[1]); foto(nome, w);
    modoLista = 1;
    snprintf(nome, sizeof nome, "%s-lista.png", argv[1]); foto(nome, w);
    focoLin = 0; focoCol = 1;
    snprintf(nome, sizeof nome, "%s-outro-canal.png", argv[1]); foto(nome, w);
    assert(video_tocando() && player_mini_no_guia_ativo());
  }
  return 0;
}
