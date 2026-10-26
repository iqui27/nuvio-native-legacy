// Offline source navigation regression; includes private state for bounds assertions.
#ifndef BIB_FONTE
#define BIB_FONTE "../src/biblioteca.c"
#endif
#define perfis_ativo biblioteca_perfil_teste
#define tex_obter_larg biblioteca_texture_teste
#include BIB_FONTE
#undef tex_obter_larg
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>

static int pedidos[2000], falhas;
GLuint biblioteca_texture_teste(const char *url, float width) {
  int i;
  (void)width;
  if (sscanf(url, "reflow-%d.jpg", &i) == 1 && i >= 0 && i < 2000) {
    pedidos[i]++;
  }
  return 0;
}

static int perfilTeste = 1;
int biblioteca_perfil_teste(void) { return perfilTeste; }

static void key(SDL_Keycode code) {
  SDL_Event event = {0};
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = code;
  biblioteca_evento(&event);
}

static void reflow(void) {
  CatItem *items = calloc(2000, sizeof *items);
  assert(items);
  for (int i = 0; i < 2000; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%07d", i);
    snprintf(items[i].poster, sizeof items[i].poster, "reflow-%d.jpg", i);
    strcpy(items[i].tipo, "movie");
    items[i].naLista = 1;
    items[i].listaFontes = 1u << AJ_SALVOS_LOCAL;
  }
  cat_definir(items, 2000); free(items);
  modo = MODO_PARAVER; origem = ORIG_TODAS; exibicao = VIS_CARTAZ;
  origensAbertas = 1; animOrig = flipDe = 1;
  reconstruir();
  assert(nCelulas == 2000);
  animTela = animVista = 1; ondaEm = ondaArmada = 0;
  // The shared poster renderer also serves outgoing view snapshots.
  memset(pedidos, 0, sizeof pedidos);
  desenhaCartaz(cat_item(0), (GfxRect){100, NV_TELA_H + 10, 220, 330}, 0, 1, NULL, 1000);
  if (pedidos[0]) { fprintf(stderr, "offscreen snapshot requested art\n"); falhas++; }
  memset(pedidos, 0, sizeof pedidos);
  // A label still in the fade must not request its fully clipped poster.
  desenhaCartaz(cat_item(0), (GfxRect){100, gradeY() - 18 * bibRel() - 331, 220, 330},
                 0, 1, NULL, 1000);
  if (pedidos[0]) { fprintf(stderr, "fully clipped artwork requested\n"); falhas++; }
  foco.fileira = BIB_FIL_PICK; focoBotaoOrig = 1;
  const char *names[] = {"closing", "opening", "reversing"};
  int maximum = 0;
  for (int direction = 0; direction < 3; direction++) {
    alternarOrigens();
    const float steps[] = {0, .25f, .5f, .75f, 1};
    float start = animOrig;
    for (unsigned step = 0; step < sizeof steps / sizeof *steps; step++) {
      animOrig = start + ((float)origensAbertas - start) * steps[step];
      // Reversal starts halfway through closing.
      if (direction == 2 && step > 2) break;
      const float scrolls[] = {0, 12000};
      for (unsigned s = 0; s < sizeof scrolls / sizeof *scrolls; s++) {
        scrollY = scrolls[s]; velY = 0;
        memset(pedidos, 0, sizeof pedidos);
        txt_novo_quadro(); tex_novo_quadro();
        biblioteca_desenhar(1000);
        int total = 0, offscreen = 0, missing = 0;
        float remaining = origensFlipRestante();
        for (int i = 0; i < nCelulas; i++) {
          float y = gradeY() + (i / colunas()) * passoLinha() - scrollY
                  + flipOffset[i][1] * remaining;
          float clip = gradeY() - 18.0f * bibRel();
          total += pedidos[i];
          if (pedidos[i] && (y >= BIB_GRADE_BASE || y + alturaLinha() <= clip)) offscreen++;
          if (y >= clip && y + alturaLinha() <= BIB_GRADE_BASE && !pedidos[i]) missing++;
        }
        if (total > maximum) maximum = total;
        if (total > 40 || offscreen || missing) {
          fprintf(stderr, "%s %.2f scroll %.0f: requests=%d offscreen=%d missing=%d\n",
                  names[direction], steps[step], scrollY, total, offscreen, missing);
          falhas++;
        }
      }
    }
    scrollY = 0;
  }
  // Reverse the half-finished close and verify the same bounded work.
  alternarOrigens();
  memset(pedidos, 0, sizeof pedidos);
  txt_novo_quadro(); tex_novo_quadro();
  biblioteca_desenhar(1000);
  int total = 0;
  for (int i = 0; i < 2000; i++) total += pedidos[i];
  if (total > 40) { fprintf(stderr, "mid-flight reversal requests=%d\n", total); falhas++; }
  printf("2000-title reflow: maximum %d poster requests/frame\n", maximum);
}

int main(void) {
  CatItem items[2] = {0};
  for (int i = 0; i < 2; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt900%02d", i);
    strcpy(items[i].tipo, "movie");
    snprintf(items[i].origem, sizeof items[i].origem, "addon.%d", i);
    items[i].naLista = 1;
    items[i].listaFontes = 1u << AJ_SALVOS_LOCAL;
  }
  cat_definir(items, 2);
  mkdir(getenv("NUVIO_DADOS"), 0700);
  dados_iniciar(getenv("NUVIO_DADOS"));
  biblioteca_iniciar();
  // Header and source column retain the mock's physical geometry below 100%.
  const float scales[] = {.85f, 1.0f, 1.3f};
  for (unsigned i = 0; i < sizeof scales / sizeof scales[0]; i++) {
    gfx_escala_txt_definir(scales[i]);
    TXT_INI();
    assert(fabsf(bibHS() - fmaxf(1.0f, scales[i])) < .001f);
    assert(fabsf(BIB_TOPO * bibHS() - 124.0f) < .001f);
    // Compact header (owner 25/10): tabs on the title row, the rest rises 152.
    assert(fabsf(BIB_FAIXA_Y * bibHS() - BIB_LINHA1_Y) < .001f);
    assert(fabsf(pickerY() * bibHS() - 160.0f) < .001f);
    assert(fabsf(BIB_GRADE_Y_TITULOS - 236.0f) < .001f);
    assert(fabsf(hdrX() * bibHS() - paginaX() * scales[i]) < .001f);
    assert(fabsf((bibX() - paginaX()) * scales[i] - 364.0f) < .001f);
    TXT_FIM();
  }
  gfx_escala_txt_definir(1.0f);
  // "Origens" chip: Left of Type; OK closes the panel and the grid takes its
  // column; Down/Up reach the grid and come back; the choice is per profile.
  key(SDLK_DOWN); key(SDLK_LEFT);
  assert(focoBotaoOrig && foco.fileira == BIB_FIL_PICK);
  key(SDLK_LEFT);
  assert(sair && focoBotaoOrig && foco.fileira == BIB_FIL_PICK);
  sair = 0;
  int colsAberto = colunas();
  key(SDLK_RETURN);
  assert(!origensAbertas && !origensNaGrade() && bibX() == paginaX() && colunas() > colsAberto);
  assert(focoBotaoOrig && !focoOrigem);
  prefPerfil = -2; lerPref();
  assert(!origensAbertas);
  perfilTeste = 2; lerPref();
  assert(origensAbertas);   // a new profile defaults open
  perfilTeste = 1; lerPref();
  assert(!origensAbertas);  // switching back restores this person's choice
  key(SDLK_DOWN);
  assert(!focoBotaoOrig && !focoOrigem && foco.fileira == BIB_FIL_GRADE);
  key(SDLK_LEFT);   // closed panel: Left from the first poster does not land in it
  assert(!focoOrigem);
  sair = 0;
  key(SDLK_UP);
  assert(focoBotaoOrig);
  key(SDLK_RETURN);
  assert(origensAbertas && colunas() == colsAberto);
  key(SDLK_DOWN); assert(focoOrigem);
  alternarOrigens();
  assert(!focoOrigem && focoBotaoOrig && foco.fileira == BIB_FIL_PICK);
  // Reversing mid-flight keeps each poster at its current position.
  animOrig = .6f;
  float before = bibX() + flipOffset[0][0] * origensFlipRestante();
  alternarOrigens();
  assert(fabsf(before - bibX() - flipOffset[0][0] * origensFlipRestante()) < .001f);
  anim_politica_reduzida = 1;
  biblioteca_atualizar_(.016f, 100);
  assert(animOrig == 1.0f && velOrig == 0.0f);
  anim_politica_reduzida = 0;
  key(SDLK_DOWN); assert(focoOrigem);
  key(SDLK_UP); assert(!focoOrigem && focoBotaoOrig);
  key(SDLK_RIGHT); assert(!focoBotaoOrig && pickSel == 0);
  key(SDLK_UP); assert(foco.fileira == BIB_FIL_MODO);
  key(SDLK_DOWN); key(SDLK_DOWN);
  for (int i = 0; i < BIB_N_ORIG + 1; i++) key(SDLK_DOWN);
  key(SDLK_RETURN);
  assert(origem == BIB_N_ORIG + 1);
  for (int i = 0; i < BIB_N_ORIG + 3; i++) key(SDLK_UP);
  for (int i = 0; i < MODO_LISTAS; i++) key(SDLK_RIGHT);
  key(SDLK_RETURN);
  assert(modo == MODO_LISTAS && fonte == FONTE_TRAKT);
  key(SDLK_DOWN); key(SDLK_DOWN); key(SDLK_RETURN);
  assert(fonte == FONTE_TRAKT);
  listaTab = 1;   // 6D "Minhas listas" has no Source chip; Seguindo keeps it
  const char *rot, *val;
  pickerTexto(0, &rot, &val);
  assert(!strcmp(val, "Trakt"));
  // Reject stale pointer/state values before they become a list source.
  const int invalid[] = {-1, BIB_N_FONTES, INT_MAX, INT_MIN};
  for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
    origemSel = invalid[i];
    key(SDLK_RETURN);
    assert(fonte == FONTE_TRAKT);
    fonte = invalid[i];
    pickerTexto(0, &rot, &val);
    assert(!strcmp(val, "Trakt"));
    fonte = FONTE_TRAKT;
    origem = invalid[i];
    modo = MODO_ASSISTIDOS;
    pickerTexto(0, &rot, &val);
    assert(val && *val);
    modo = MODO_LISTAS;
  }
  for (fonte = 0; fonte < BIB_N_FONTES; fonte++) {
    pickerTexto(0, &rot, &val);
    assert(!strcmp(val, ROT_FONTE[fonte]));
  }
  modo = MODO_ASSISTIDOS;
  for (origem = 0; origem < BIB_N_ORIG; origem++) {
    pickerTexto(0, &rot, &val);
    assert(!strcmp(val, ROT_ORIG[origem]));
  }
  origem = BIB_N_ORIG + 1;
  pickerTexto(0, &rot, &val);
  assert(!strcmp(val, "addon.1"));
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Sources regression", 0, 0, 1920, 1080,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context && gfx_iniciar() && txt_iniciar("deploy/app", 1));
  gfx_tamanho_alvo(1920, 1080);
  reflow();
  // List header, rows and width follow the same spring, including reversal.
  exibicao = VIS_LISTA; origensAbertas = 1; animOrig = flipDe = 1;
  float rowX = bibX(), rowW = bibW();
  alternarOrigens();
  assert(fabsf(bibX() - rowX) < .001f && fabsf(bibW() - rowW) < .001f);
  animOrig = .5f;
  assert(fabsf(bibX() - (paginaX() + rowX) * .5f) < .001f);
  assert(fabsf(bibX() + bibW() - bibDireita()) < .001f);
  rowX = bibX(); rowW = bibW();
  alternarOrigens();
  assert(fabsf(bibX() - rowX) < .001f && fabsf(bibW() - rowW) < .001f);
  txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  if (falhas) return 1;
  puts("biblioteca source navigation and 2000-title reflow: PASS");
  return 0;
}
