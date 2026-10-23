// Exercise the real Home renderer with local art and audit its draw calls.
#define main homelayouts_fixture_main
#include "homelayouts_shot.c"
#undef main
#include <math.h>

static int heroInfo, heroBackdrops, stageArtwork;
static GLuint artworkTexture, stageTextures[8], labelTexture;
static float labelY = -1;
static const char *labelTitle;
static int regressionFailures;
static TxtLinha auditLine(TxtEstilo style, const char *s, int r, int g, int b, int a, float width) {
  TxtLinha line = txt_linha_corta(style, s, r, g, b, a, width);
  if (labelTitle && style == TXT_ROW_TITULO && !strcmp(s, labelTitle)) labelTexture = line.tex;
  return line;
}
static void checkRegression(int pass, const char *name) {
  printf("%s: %s\n", pass ? "PASS" : "FAIL", name);
  if (!pass) regressionFailures++;
}
static void auditLabel(TxtLinha l, float x, float y) {
  if (l.tex == labelTexture) labelY = y * gfx_escala();
  txt_desenhar(l, x, y);
}
static void auditRect(GfxRect r, GLuint tex, GfxModo modo, float foco,
                      float px, float py, float raio, float cr, float cg, float cb, float ca) {
  if (r.y * gfx_escala() < 480 && ca * gfx_opacidade_grupo > .004f) {
    if (modo == GFX_HERO || modo == GFX_HERO_CHEIO ||
        modo == GFX_HERO_CAM || modo == GFX_HERO_CHEIO_CAM) heroBackdrops++;
    if (modo == GFX_TEXTO || modo == GFX_MARCA) heroInfo++;
    if (modo == GFX_VITRINE && tex && stageArtwork < 8) stageTextures[stageArtwork++] = artworkTexture = tex;
    if (modo == GFX_PALCO_ARTE && tex && stageArtwork < 8) stageTextures[stageArtwork++] = artworkTexture = tex;
  }
  gfx_rect(r, tex, modo, foco, px, py, raio, cr, cg, cb, ca);
}
static void auditText(TxtLinha l, float x, float y, float a) {
  if (y * gfx_escala() < 480 && a * gfx_opacidade_grupo > .004f && l.tex) heroInfo++;
  txt_desenhar_alpha(l, x, y, a);
}
// Keep instrumentation in the test translation unit, with no production hooks.
#define gfx_rect auditRect
#define txt_desenhar_alpha auditText
#define txt_desenhar auditLabel
#define txt_linha_corta auditLine
#ifdef PALCO_BASELINE
#include PALCO_BASELINE
#else
#include "../src/home.c"
#endif
#undef gfx_rect
#undef txt_desenhar_alpha
#undef txt_desenhar
#undef txt_linha_corta

static char shotPath[900];
static void capture(const char *dir, const char *phase, int mode, const char *state) {
  if (!strcmp(phase, "dynamic-only") && strcmp(state, "dynamic")) return;
  snprintf(shotPath, sizeof shotPath, "%s/%s-%d-g%d-m%d-%s.bmp", dir, phase,
           (int)roundf(gfx_escala_txt() * 100), ajustes_vidro(), mode, state);
  heroInfo = heroBackdrops = stageArtwork = 0; artworkTexture = 0;
  memset(stageTextures, 0, sizeof stageTextures);
  if (!strcmp(state, "stage")) holoA = 1.0f;
  quadros(1, shotPath);
}
static void blankHero(void) {
  SDL_Surface *s = SDL_LoadBMP(shotPath);
  assert(s);
  int changed = 0;
  for (int y = 110; y < 350; y++) for (int x = 100; x < 700; x++) {
    Uint32 pixel; Uint8 r, g, b;
    memcpy(&pixel, (char *)s->pixels + y * s->pitch + x * 4, 4);
    SDL_GetRGB(pixel, s->format, &r, &g, &b);
    if (fabsf(r - NV_COR_FUNDO_R * 255) > 2 ||
        fabsf(g - NV_COR_FUNDO_G * 255) > 2 ||
        fabsf(b - NV_COR_FUNDO_B * 255) > 2) changed++;
  }
  SDL_FreeSurface(s);
  assert(changed == 0);
}
static void setting(int layout, int glass, int mode) {
  char extra[120];
  snprintf(extra, sizeof extra, "socialLocal 1\npalcoFundoLocal %d", mode);
  setenv("NV_AJ", extra, 1);
  ajusta(layout, glass);
  assert(ajustes_palco_fundo() == mode);
}

#ifndef PALCO_ORIGINAL
static void regressions(const char *dir, const char *phase, int glass) {
  int sr = foco.fileira;
  float expectedScroll = 0, expectedLabel;
  { TXT_INI();
    for (int r = 0; r < sr; r++) expectedScroll += NV_LEGACY_ROW_HEAD_H + alturaTotalFil(r) + fileiraGap();
    expectedLabel = topoFileiras() * gfx_escala();
    TXT_FIM(); }
  for (int mode = 0; mode <= PALCO_MONUMENTO; mode++) {
    setting(0, glass, mode); quadros(90, NULL);
    labelTitle = fileiras[sr].titulo; labelTexture = 0; labelY = -1;
    capture(dir, phase, mode, "geometry");
    checkRegression(fabsf(scrollY - expectedScroll) < .1f, "Stage cards and info keep approved Y");
    printf("Stage mode=%d scroll=%.2f expected=%.2f label=%.2f expected=%.2f\n", mode, scrollY, expectedScroll, labelY, expectedLabel);
    checkRegression(fabsf(labelY - expectedLabel) < .1f, "Stage label stays above cards in every mode");
    labelTitle = NULL; labelTexture = 0;
  }
  // Wider than the budget, independent of the bundled 1280px JPEG fixtures.
  char art[900], warm[900];
  snprintf(art, sizeof art, "%s/budget.bmp", dirDados);
  snprintf(warm, sizeof warm, "%s/budget-warm.bmp", dirDados);
  SDL_Surface *large = SDL_CreateRGBSurfaceWithFormat(0, 2560, 1440, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(large); SDL_FillRect(large, NULL, SDL_MapRGB(large->format, 180, 70, 90));
  assert(SDL_SaveBMP(large, art) == 0);
  assert(SDL_SaveBMP(large, warm) == 0); SDL_FreeSurface(large);
  // Stage must also respect its ceiling when Current already cached this URL.
  tex_qualidade(1);
  GLuint warmed = 0;
  for (int f = 0; f < 90; f++) {
    warmed = tex_obter_hero(warm); quadros(1, NULL);
  }
  assert(warmed);
  GLint warmedWidth = 0; glBindTexture(GL_TEXTURE_2D, warmed);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &warmedWidth);
  assert(warmedWidth > 1280);
  CatItem *live = (CatItem *)cat_item_exato(fileiraItemIndice(&fileiras[sr], foco.coluna));
  CatItem saved = *live;
  snprintf(live->backdrop, sizeof live->backdrop, "%s", art);
  snprintf(live->backdropTmdb, sizeof live->backdropTmdb, "%s", warm);
  for (int quality = 0; quality < 3; quality++) {
    tex_qualidade(quality); tex_escala(2);
    setting(0, glass, PALCO_TRIPTICO); quadros(90, NULL);
    capture(dir, phase, PALCO_TRIPTICO, "budget");
    int within = stageArtwork == 3;
    for (int t = 0; t < stageArtwork; t++) {
      GLint width = 0; glBindTexture(GL_TEXTURE_2D, stageTextures[t]);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
      printf("Stage quality=%d texture width=%d\n", quality, width);
      within &= width > 0 && width <= 1280;
    }
    checkRegression(within, "Stage and Triptych textures never exceed 1280");
  }
  tex_qualidade(1); tex_escala(1); *live = saved;
  for (int mode = PALCO_CORTINA; mode <= PALCO_MONUMENTO; mode++) {
    if (mode == PALCO_PRETO || mode == PALCO_ATUAL) continue;
    setting(0, glass, mode); quadros(90, NULL);
    const CatItem *next = cat_item_exato(fileiraItemIndice(&fileiras[sr], foco.coluna + 1));
    assert(next);
    // Warm the exact Stage request through its real draw path.
    int column = foco.coluna;
    tecla(SDLK_RIGHT); quadros(90, NULL);
    tecla(SDLK_LEFT); quadros(90, NULL);
    capture(dir, phase, mode, "focus-old");
    GLuint old = artworkTexture;
    tecla(SDLK_RIGHT); quadros(5, NULL);
    capture(dir, phase, mode, "focus-mid");
    int oldSeen = 0, newSeen = 0;
    for (int t = 0; t < stageArtwork; t++) {
      oldSeen |= stageTextures[t] == old;
      newSeen |= stageTextures[t] != old;
    }
    checkRegression(oldSeen && newSeen, "Cached focus change draws both titles together");
    quadros(90, NULL);
    // No uploads while waiting: old art must remain visible past a full fade.
    CatItem *cold = (CatItem *)cat_item_exato(fileiraItemIndice(&fileiras[sr], foco.coluna + 1));
    CatItem coldSaved = *cold;
    snprintf(cold->backdrop, sizeof cold->backdrop, "%s/cold-%d.bmp", dirDados, mode);
    assert(SDL_SaveBMP((large = SDL_LoadBMP(art)), cold->backdrop) == 0); SDL_FreeSurface(large);
    capture(dir, phase, mode, "cold-old"); old = artworkTexture;
    tecla(SDLK_RIGHT);
    for (int f = 0; f < 20; f++) {
      heroInfo = heroBackdrops = stageArtwork = 0; artworkTexture = 0;
      memset(stageTextures, 0, sizeof stageTextures);
      home_atualizar(1.0f / 60.0f, SDL_GetTicks());
      home_desenhar(SDL_GetTicks());
    }
    oldSeen = 0;
    for (int t = 0; t < stageArtwork; t++) oldSeen |= stageTextures[t] == old;
    checkRegression(oldSeen, "Cache miss retains outgoing artwork until upload");
    quadros(90, NULL); *cold = coldSaved;
    while (foco.coluna > column) tecla(SDLK_LEFT);
    quadros(90, NULL);
  }
  printf("Stage regression failures=%d\n", regressionFailures);
}

static void reviewRegressions(const char *dir, const char *phase, int glass) {
  // Stored IDs are append-only: old Black=0 now selects Cortina, 1/2 survive.
  for (int stored = 0; stored <= 2; stored++) {
    setting(0, glass, stored); quadros(90, NULL);
    capture(dir, phase, stored, "stored-setting");
    checkRegression(stored == 0 ? stageArtwork == 1 && heroInfo > 0 && !heroBackdrops :
                    stored == 1 ? heroBackdrops > 0 : stageArtwork == 1 && !heroInfo,
                    "Original stored Stage setting IDs render their migrated choice");
  }
  // GLSL requires ascending smoothstep edges; desktop drivers can hide this bug.
  FILE *shader = fopen("src/gfx.c", "r");
  assert(shader);
  char line[2048];
  int inStage = 0, masks = 0, ordered = 1;
  while (fgets(line, sizeof line, shader)) {
    if (strstr(line, "// GFX_PALCO_ARTE")) inStage = 1;
    if (!inStage) continue;
    const char *p = line;
    while ((p = strstr(p, "smoothstep("))) {
      float low, high;
      if (sscanf(p, "smoothstep(%f, %f", &low, &high) == 2) {
        masks++;
        ordered &= low < high;
      }
      p++;
    }
    if (strstr(line, "gl_FragColor")) break;
  }
  fclose(shader);
  checkRegression(masks > 0 && ordered, "Triptych smoothstep edges are defined by GLSL");

  setting(0, glass, PALCO_CORTINA); quadros(90, NULL);
  capture(dir, phase, PALCO_CORTINA, "backdrop-old");
  GLuint old = artworkTexture;
  assert(old && heroInfo > 0);
  CatItem *next = (CatItem *)cat_item_exato(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna + 1));
  assert(next);
  CatItem saved = *next;
  // Keep the late-art fade's first frame deterministic, independent of decode timing.
  for (int f = 0; f < 120; f++) { tex_obter_palco(saved.backdrop); quadros(1, NULL); }
  assert(tex_obter_palco(saved.backdrop));
  next->backdrop[0] = 0;
  tecla(SDLK_RIGHT); quadros(5, NULL);
  capture(dir, phase, PALCO_CORTINA, "backdrop-missing-mid");
  checkRegression(stageArtwork == 1 && artworkTexture == old && heroInfo > 0 &&
                  palcoItemA > 0.0f && palcoItemA < 1.0f,
                  "Missing backdrop fades outgoing artwork and logo without incoming art");
  quadros(90, NULL);
  capture(dir, phase, PALCO_CORTINA, "backdrop-missing-end");
  checkRegression(stageArtwork == 0 && heroInfo == 0 && palcoItemA == 1.0f,
                  "Missing backdrop settles to Black");
  blankHero();
  *next = saved;
  capture(dir, phase, PALCO_CORTINA, "same-title-late-start");
  checkRegression(stageArtwork == 0 && palcoItemA == 0.0f,
                  "Same-title late backdrop starts from Black with zero alpha");
  quadros(5, NULL);
  capture(dir, phase, PALCO_CORTINA, "same-title-late-mid");
  checkRegression(stageArtwork == 1 && heroInfo > 0 && palcoItemA > 0 && palcoItemA < 1,
                  "Same-title late backdrop and logo fade in together");
  quadros(90, NULL);
  capture(dir, phase, PALCO_CORTINA, "same-title-replace-old");
  old = artworkTexture;
  char replacement[900];
  snprintf(replacement, sizeof replacement, "%s/replacement.bmp", dirDados);
  SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(image);
  SDL_FillRect(image, NULL, SDL_MapRGB(image->format, 30, 170, 90));
  assert(SDL_SaveBMP(image, replacement) == 0); SDL_FreeSurface(image);
  snprintf(next->backdrop, sizeof next->backdrop, "%s", replacement);
  // Hold uploads so the replacement remains cold beyond a full fade duration.
  for (int f = 0; f < 30; f++) {
    stageArtwork = heroInfo = 0; artworkTexture = 0;
    home_atualizar(1.0f / 60.0f, SDL_GetTicks());
    home_desenhar(SDL_GetTicks());
  }
  checkRegression(stageArtwork == 1 && artworkTexture == old && heroInfo > 0,
                  "Same-title replacement retains outgoing artwork and logo while loading");
  for (int f = 0; f < 120 && !tex_obter_palco(replacement); f++) quadros(1, NULL);
  quadros(5, NULL);
  capture(dir, phase, PALCO_CORTINA, "same-title-replace-mid");
  int oldSeen = 0, newSeen = 0;
  for (int t = 0; t < stageArtwork; t++) {
    oldSeen |= stageTextures[t] == old;
    newSeen |= stageTextures[t] != old;
  }
  checkRegression(oldSeen && newSeen && palcoItemA > 0 && palcoItemA < 1,
                  "Same-title replacement crossfades outgoing and incoming artwork");
  quadros(90, NULL);
  capture(dir, phase, PALCO_CORTINA, "same-title-replace-end");
  checkRegression(stageArtwork == 1 && artworkTexture != old && palcoItemA == 1,
                  "Same-title replacement settles to incoming artwork");
  *next = saved;
  quadros(90, NULL);
  // Warm a real poster first: even a ready poster must never become Stage art.
  for (int f = 0; f < 90; f++) { tex_obter_palco(next->poster); quadros(1, NULL); }
  const int concepts[] = { PALCO_CORTINA, PALCO_ARTE, PALCO_AURA, PALCO_TRIPTICO, PALCO_MONUMENTO };
  for (int i = 0; i < 5; i++) {
    setting(0, glass, concepts[i]); quadros(90, NULL);
    next->backdrop[0] = 0; quadros(90, NULL);
    capture(dir, phase, concepts[i], "never-poster-missing");
    checkRegression(!stageArtwork && !heroBackdrops && !heroInfo,
                    "Missing backdrop never falls back to a ready poster");
    snprintf(next->backdrop, sizeof next->backdrop, "%s", next->poster);
    quadros(90, NULL);
    capture(dir, phase, concepts[i], "never-poster-same");
    checkRegression(!stageArtwork && !heroBackdrops && !heroInfo,
                    "Poster supplied as backdrop stays Black");
    *next = saved; quadros(90, NULL);
  }
  setting(0, glass, PALCO_TRIPTICO); quadros(90, NULL);
  capture(dir, phase, PALCO_TRIPTICO, "triptych");
  checkRegression(stageArtwork == 3, "Triptych renders three artwork panels");
}

#endif

int main(int argc, char **argv) {
  assert(argc == 5);
  const char *dir = argv[1], *phase = argv[2];
  int glass = atoi(argv[4]);
  setenv("NV_STAGE_SCALE", argv[3], 1);
  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  assert(dirDados && strstr(dirDados, "nuvio-homelayouts-shot"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Stage background regression", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  SDL_GLContext gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_borrao_iniciar(480, 270);
  artehero_definir_falhou(tex_falhou);
  gfx_icones_dir("deploy/app/art");
  ajustes_dir(dirDados);
  assert(ajustes_palco_fundo() == PALCO_CORTINA); // Old stored 0 (Black) is the new default.
  assert(ajustes_escala_texto() == .85f);
  setting(0, glass, PALCO_PRETO); // Run legacy appearance migrations once.
  setting(0, glass, PALCO_PRETO); // Then use one stable theme for every mode.
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&addon, 1));
  static CatItem items[12];
  static CatFileira rows[2];
  for (int r = 0; r < 2; r++) {
    snprintf(rows[r].chave, sizeof rows[r].chave, "%s", r ? "stage_movie" : "pop_movie");
    snprintf(rows[r].titulo, sizeof rows[r].titulo, "%s", r ? "Latest Horror - Movie" : "Popular - Movie");
    snprintf(rows[r].tipo, sizeof rows[r].tipo, "movie");
    rows[r].ini = r * 6; rows[r].n = 6;
    for (int c = 0; c < 6; c++) {
      int i = r * 6 + c;
      CatItem *it = &items[i];
      snprintf(it->imdb, sizeof it->imdb, "tt90%05d", i);
      snprintf(it->tipo, sizeof it->tipo, "movie");
      snprintf(it->titulo, sizeof it->titulo, "%s", NOMES[i]);
      snprintf(it->poster, sizeof it->poster, "deploy/app/art/poster/%02d.jpg", i);
      snprintf(it->backdrop, sizeof it->backdrop, "deploy/app/art/%02d.jpg", i);
      snprintf(it->logo, sizeof it->logo, "deploy/app/art/logo/%02d.png", i);
      snprintf(it->meta, sizeof it->meta, "2026 · 112 min");
      snprintf(it->genero, sizeof it->genero, "Movie · Comedy · Horror");
      snprintf(it->sinopse, sizeof it->sinopse, "A quiet town discovers a mystery. One night changes everything for the friends who return home.");
      it->nota = 76;
    }
  }
  cat_definir_tudo(items, 12, rows, 2);
  quadros(1, NULL);
  assert(fil_definir_tipo("stage_movie", FIL_TIPO_PALCO));
  quadros(90, NULL);
  tecla(SDLK_DOWN); quadros(90, NULL);
  capture(dir, phase, PALCO_PRETO, "normal");
  assert(heroInfo > 0 && heroBackdrops > 0);
  tecla(SDLK_DOWN); quadros(1, NULL);
  capture(dir, phase, PALCO_PRETO, "enter");
  assert(heroBackdrops > 0); // First focus frames retain the fading hero.
  quadros(90, NULL);
  if (!strcmp(phase, "dynamic-only")) {
    for (int mode = PALCO_CORTINA; mode <= PALCO_MONUMENTO; mode++) {
      setting(HOME_LAYOUT_DINAMICA, glass, mode); quadros(90, NULL);
      capture(dir, phase, mode, "dynamic");
      assert(palcoFundoDestino() == mode && foco.fileira == 1);
      if (mode == PALCO_PRETO) {
        assert(palcoFundoAlpha == 0 && !heroInfo && !heroBackdrops && !stageArtwork);
      } else if (mode == PALCO_ATUAL) assert(heroInfo > 0 && heroBackdrops > 0);
      else {
        assert(!heroBackdrops && (heroInfo > 0) == (mode != PALCO_ARTE));
        assert(stageArtwork == (mode == PALCO_TRIPTICO ? 3 : 1));
      }
    }
    for (int mode = PALCO_CORTINA; mode <= PALCO_MONUMENTO; mode++) {
      setting(HOME_LAYOUT_DINAMICA, glass, mode);
      assert(fil_definir_tipo("stage_movie", FIL_TIPO_HEROI_DIVIDIDO));
      quadros(90, NULL);
      assert(palcoFundoDestino() == PALCO_ARTE);
    }
    tex_encerrar(); txt_encerrar();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
    puts("Dynamic L2: seven Stage backgrounds and unchanged Split passed");
    return 0;
  }
  capture(dir, phase, PALCO_PRETO, "stage");
#ifdef PALCO_ORIGINAL
  for (int stored = 0; stored <= 2; stored++) {
    setting(0, glass, stored); quadros(90, NULL);
    capture(dir, phase, stored, "stored-setting");
    checkRegression(stored == 0 ? stageArtwork == 1 && heroInfo > 0 && !heroBackdrops :
                    stored == 1 ? heroBackdrops > 0 : stageArtwork == 1 && !heroInfo,
                    "Original stored Stage setting IDs render their migrated choice");
  }
  CatItem *live = (CatItem *)cat_item(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
  for (int f = 0; f < 90; f++) { tex_obter_palco(live->poster); quadros(1, NULL); }
  live->backdrop[0] = 0; quadros(90, NULL);
  capture(dir, phase, 2, "never-poster-missing");
  checkRegression(!stageArtwork && !heroBackdrops && !heroInfo,
                  "Missing backdrop never falls back to a ready poster");
  snprintf(live->backdrop, sizeof live->backdrop, "%s", live->poster);
  quadros(90, NULL);
  capture(dir, phase, 2, "never-poster-same");
  checkRegression(!stageArtwork && !heroBackdrops && !heroInfo,
                  "Poster supplied as backdrop stays Black");
  return regressionFailures ? 1 : 0;
#else
  if (!strcmp(phase, "review-before") || !strcmp(phase, "review-after")) {
    reviewRegressions(dir, phase, glass);
    tex_encerrar(); txt_encerrar();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
    return regressionFailures ? 1 : 0;
  }
  regressions(dir, phase, glass);
  reviewRegressions(dir, phase, glass);
#ifdef PALCO_BASELINE
  return regressionFailures ? 1 : 0;
#else
  assert(regressionFailures == 0);
  setting(0, glass, PALCO_PRETO); quadros(90, NULL);
  capture(dir, phase, PALCO_PRETO, "stage");
  assert(heroInfo == 0 && heroBackdrops == 0 && stageArtwork == 0);
  blankHero();
  double blackFill = fillVisUlt;
  int blackRects = rectUlt;
  setting(0, glass, 1); quadros(600, NULL); // hero re-enters; settle timed fades
  capture(dir, phase, 1, "stage");
  assert(heroInfo > 0 && heroBackdrops > 0);
  double currentFill = fillVisUlt;
  int currentRects = rectUlt;
  setting(0, glass, 2); quadros(90, NULL);
  capture(dir, phase, 2, "stage");
  assert(heroInfo == 0 && heroBackdrops == 0 && stageArtwork == 1);
  const CatItem *focused = cat_item_exato(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
  assert(focused && palcoArte(focused));
  assert(artworkTexture != 0);
  assert(artworkTexture != tex_obter(focused->poster));
  assert(blackFill < currentFill && fillVisUlt <= currentFill);
  assert(blackRects < currentRects && rectUlt <= currentRects);
  printf("scale=%s glass=%d fill black=%.3f current=%.3f artwork=%.3f; draw assertions passed\n",
         argv[3], glass, blackFill, currentFill, fillVisUlt);
  CatItem missing = *focused;
  missing.backdrop[0] = 0; assert(!palcoArte(&missing));
  snprintf(missing.backdrop, sizeof missing.backdrop, "%s", missing.poster);
  assert(!palcoArte(&missing));
  snprintf(missing.poster, sizeof missing.poster, "https://image.tmdb.org/t/p/w500/same.jpg");
  snprintf(missing.backdrop, sizeof missing.backdrop, "https://image.tmdb.org/t/p/original/same.jpg");
  assert(!palcoArte(&missing));
  setting(0, glass, PALCO_PRETO); // 7 choices: Settings only, not the quick panel.
  ajustes_aplicar_blob("{\"palcoFundoLocal\":2}");
  assert(ajustes_palco_fundo() == PALCO_PRETO); // Account cannot override device choice.
  setting(0, glass, 2);
  // Missing backdrop must render exactly the same empty hero as Black.
  CatItem *live = (CatItem *)focused;
  char saved[sizeof live->backdrop]; memcpy(saved, live->backdrop, sizeof saved);
  snprintf(live->backdrop, sizeof live->backdrop, "%s", live->poster);
  quadros(90, NULL);
  capture(dir, phase, 2, "fallback");
  assert(heroInfo == 0 && heroBackdrops == 0 && stageArtwork == 0);
  blankHero(); memcpy(live->backdrop, saved, sizeof saved);
  // Premium concepts: backdrop art only, never the focused poster.
  GLuint posterTex = tex_obter(live->poster);
  snprintf(live->backdropTrakt, sizeof live->backdropTrakt, "%s", live->poster);
  assert(!palcoArteOutra(live, live->backdrop, NULL)); // poster is never a side panel
  snprintf(live->backdropTmdb, sizeof live->backdropTmdb, "%s", cat_item_exato(0)->backdrop);
  assert(palcoArteOutra(live, live->backdrop, NULL) == live->backdropTmdb);
  const int concepts[] = { PALCO_CORTINA, PALCO_AURA, PALCO_TRIPTICO, PALCO_MONUMENTO };
  for (int i = 0; i < 4; i++) {
    setting(0, glass, concepts[i]); quadros(90, NULL); quadros(1, NULL);
    capture(dir, phase, concepts[i], "stage");
    printf("concept %d: art=%d info=%d hero=%d\n", concepts[i], stageArtwork, heroInfo, heroBackdrops);
    assert(heroBackdrops == 0 && heroInfo > 0); // logo, no Current hero
    assert(stageArtwork == (concepts[i] == PALCO_TRIPTICO ? 3 : 1));
    for (int t = 0; t < stageArtwork; t++) assert(stageTextures[t] && stageTextures[t] != posterTex);
    if (concepts[i] == PALCO_TRIPTICO) assert(stageTextures[0] != stageTextures[1]);
    snprintf(live->backdrop, sizeof live->backdrop, "%s", live->poster);
    quadros(90, NULL);
    capture(dir, phase, concepts[i], "fallback");
    assert(stageArtwork == 0 && heroBackdrops == 0); // no backdrop -> Black
    blankHero(); memcpy(live->backdrop, saved, sizeof saved);
  }
  live->backdropTmdb[0] = live->backdropTrakt[0] = 0;
  setting(0, glass, 2); quadros(90, NULL);
  tecla(SDLK_UP); quadros(1, NULL);
  capture(dir, phase, 2, "leave");
  assert(foco.fileira != nFileiras);
  quadros(90, NULL);
  capture(dir, phase, 2, "normal-return");
  assert(heroInfo > 0 && heroBackdrops > 0);
  int otherInfo[3] = {0}, otherBackdrops[3] = {0};
  for (int mode = 0; mode <= PALCO_MONUMENTO; mode++) for (int layout = 1; layout <= 2; layout++) {
    setting(layout, glass, mode); quadros(90, NULL);
    tecla(SDLK_DOWN); quadros(90, NULL);
    capture(dir, phase, mode, layout == 1 ? "standard" : "dynamic");
    if (layout == 2) {
      assert(palcoFundoDestino() == mode);
      if (mode == PALCO_PRETO) {
        assert(palcoFundoAlpha == 0 && heroInfo == 0 && heroBackdrops == 0 && stageArtwork == 0);
        blankHero();
      } else if (mode == PALCO_ATUAL) assert(heroInfo > 0 && heroBackdrops > 0);
      else {
        assert(heroBackdrops == 0);
        assert((heroInfo > 0) == (mode != PALCO_ARTE));
        assert(stageArtwork == (mode == PALCO_TRIPTICO ? 3 : 1));
      }
    } else {
      assert(palcoFundoDestino() == 1 && palcoFundoAlpha == 1.0f);
      if (!mode) { otherInfo[layout] = heroInfo; otherBackdrops[layout] = stageArtwork; }
      else assert(heroInfo == otherInfo[layout] && stageArtwork == otherBackdrops[layout]);
    }
    tecla(SDLK_UP); quadros(90, NULL);
    capture(dir, phase, mode, layout == 1 ? "standard-off" : "dynamic-off");
  }
  tex_encerrar(); txt_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return 0;
#endif
#endif
}
