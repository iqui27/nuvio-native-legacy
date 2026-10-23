// HOME 2.1 · "Home com Colecoes e fileiras" (design/2.1/home.html), 1920x1080,
// SEM REDE. Confere layout e foco e grava os quadros em BMP (o .sh converte em
// PNG sob tests/out/).
//
//   bash tests/home21_shot.sh [pasta]        (padrao: tests/out)
//
// Cena: Continuar assistindo (com dois episodios futuros -> "Proximos
// episodios" logo abaixo), um catalogo de filmes, uma fileira de cartoes de
// colecao (AIOMetadata/add-on) e um catalogo de series. A ordem "separar
// futuros" liga a fileira de Proximos episodios.
//
// O que ele prova (assert; sai com 1 no primeiro defeito):
//   1. Continuar assistindo e Proximos episodios existem, nessa ordem, antes
//      de qualquer catalogo;
//   2. a fileira de colecoes existe e SEGURAR OK nela abre o menu da fileira
//      (fixar, mover, ocultar);
//   3. fixar vira uma fileira propria (colpin_<id>) e desafixar a tira;
//   4. mover para baixo/cima troca de lugar com a vizinha;
//   5. "vistos ocultos" (#366) tira os vistos de um catalogo, sem esvaziar;
//   6. D-pad: toda fileira e alcancavel descendo e todo cartao da fileira
//      pela direita; Voltar fecha o menu; ocultar tira a fileira.
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "colecoes.h"
#include "corviva.h"
#include "ctxmenu.h"
#include "cwordem.h"
#include "dados.h"
#include "fileiras.h"
#include "gfx.h"
#include "homefil.h"
#include "home.h"
#include "layout.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include "posterprov.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NA 40
static const char *NOMES[] = {
  "O Diabo Veste Vermelho", "A Ilha do Farol", "Mata Fechada", "Sofá no Deserto",
  "Preto e Branco", "Ensaio Seis", "Noite de Verão", "O Último Trem",
  "Cidade Cinza", "Rio Acima", "Sal e Luz", "A Casa do Lago",
  "Vento Norte", "Fronteira", "Depois da Chuva", "Cartas de Inverno",
};
#define NN (int)(sizeof NOMES / sizeof *NOMES)

typedef struct { const char *chave, *titulo, *tipo; int n, catalogo; } Fil;
static const Fil FILS[] = {
  { "continue_watching", "Continuar assistindo", "series", 6, 0 },
  { "pop_movie",   "Popular - Filme",   "movie",  10, 1 },
  { "trend_series", "Em alta - Série",  "series", 10, 1 },
};
#define NF (int)(sizeof FILS / sizeof *FILS)

extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

static SDL_Window *janela;
static const char *dirDados;

static void gravar(const char *bmp) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, bmp) == 0);
  SDL_FreeSurface(s);
  free(pix);
}

static void quadros(int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    tex_bombear(8);
    home_atualizar(1.0f / 60.0f, agora);
    corviva_quadro(1.0f / 60.0f, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    gfx_ambiente_preparar();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_ambiente(1.0f);
    home_desenhar(agora);
    ctx_atualizar(1.0f / 60.0f, agora);
    ctx_desenhar(agora);
    if (bmp && i == n - 1) gravar(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(8);
  }
}

// SEGURAR OK de verdade: KEYDOWN, quadros ate passar NV_HOLD_MS, KEYUP.
static void segurarOk(void) {
  SDL_Event e;
  Uint32 ini = SDL_GetTicks();
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  home_evento(&e);
  while (SDL_GetTicks() - ini < NV_HOLD_MS + 150) quadros(1, NULL);
  e.type = SDL_KEYUP;
  if (ctx_aberto()) ctx_evento(&e); else home_evento(&e);
}
static void teclaCtx(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ctx_evento(&e);
  e.type = SDL_KEYUP;
  ctx_evento(&e);
  quadros(6, NULL);
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
  quadros(k == SDLK_UP || k == SDLK_DOWN ? 30 : 8, NULL);
}

static int indiceDa(const char *chave) {
  int i, n = home_fileiras_n();
  for (i = 0; i < n; i++) if (!strcmp(home_fileira_chave(i), chave)) return i;
  return -1;
}
static int indicePrefixo(const char *pre) {
  int i, n = home_fileiras_n();
  for (i = 0; i < n; i++) if (!strncmp(home_fileira_chave(i), pre, strlen(pre))) return i;
  return -1;
}
static void listar(const char *rotulo) {
  int i, n = home_fileiras_n();
  printf("[home21] %s:", rotulo);
  for (i = 0; i < n; i++) printf(" %s", home_fileira_chave(i));
  printf("\n");
}

static void ajusta(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma 0\ntrailerHero 1\nhomeLayoutLocal 0\nvidroLocal 1\n"
             "modernLandscapePostersEnabled 1\nselected_theme 0\n");
  fclose(a);
  ajustes_dir(dirDados);
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
}

// Desce do topo ate a fileira `chave` ficar sob o foco. 0 se o D-pad nao chega.
static int focarFileira(const char *chave) {
  int r;
  for (r = 0; r < 16; r++) tecla(SDLK_UP);
  for (r = 0; r < 16; r++) {
    if (!strcmp(home_fileira_focada(), chave)) { quadros(40, NULL); return 1; }
    tecla(SDLK_DOWN);
  }
  return !strcmp(home_fileira_focada(), chave);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "tests/out/home21";
  static CatItem itens[200];
  static CatFileira fils[8];
  const char *futuros[2] = { "tt9000100:1:2", "tt9000101:1:3" };
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k, total = 0, ini = 0, r, iCol;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-home21-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: home 2.1", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_borrao_iniciar(480, 270);
  artehero_definir_falhou(tex_falhou);
  snprintf(cache, sizeof cache, "%s/cache", dirDados);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajusta();
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true,\"continueWatchingSortMode\":\"split_upcoming\"}");
  assert(ajustes_cw_ordem() == CWO_SEPARAR);
  assert(home_iniciar("deploy/app/art"));

  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (k = 0; k < NF; k++) {
    CatFileira *f = &fils[k];
    snprintf(f->chave, sizeof f->chave, "%s", FILS[k].chave);
    snprintf(f->titulo, sizeof f->titulo, "%s", FILS[k].titulo);
    snprintf(f->tipo, sizeof f->tipo, "%s", FILS[k].tipo);
    if (FILS[k].catalogo) {
      snprintf(f->base, sizeof f->base, "https://addon.invalid/x");
      snprintf(f->catId, sizeof f->catId, "%s", FILS[k].chave);
    }
    f->ini = ini; f->n = FILS[k].n;
    for (i = 0; i < f->n; i++) {
      CatItem *c = &itens[ini + i];
      int a = (ini + i) % NA;
      if (k == 0) snprintf(c->imdb, sizeof c->imdb, "tt90001%02d:1:%d", i, i + 1);
      else        snprintf(c->imdb, sizeof c->imdb, "tt90%05d", ini + i);
      snprintf(c->tipo, sizeof c->tipo, "%s", FILS[k].tipo);
      snprintf(c->titulo, sizeof c->titulo, "%s", NOMES[(ini + i) % NN]);
      snprintf(c->genero, sizeof c->genero, "%s · Drama", strcmp(FILS[k].tipo, "series") ? "Filme" : "Série");
      snprintf(c->meta, sizeof c->meta, "%d · 2 h 04 min", 2018 + (ini + i) % 8);
      snprintf(c->classificacao, sizeof c->classificacao, "%s", (i % 3) ? "14" : "16");
      snprintf(c->sinopse, sizeof c->sinopse,
               "Sinopse de enchimento, comprida o bastante para ocupar as linhas "
               "que o destaque reserva para ela, como num titulo de verdade.");
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", a);
      snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", a);
      if (a < 10) snprintf(c->logo, sizeof c->logo, "deploy/app/art/logo/%02d.png", a);
      c->nota = 68 + (ini + i) % 25;
      if (k == 0) { c->progresso = 20 + i * 12; c->restanteMin = 90 - i * 10; }
    }
    ini += f->n;
    total = ini;
  }
  // Os dois ultimos de Continuar sao episodios que ainda nao estreiam: vao para
  // "Proximos episodios".
  snprintf(itens[4].imdb, sizeof itens[4].imdb, "%s", futuros[0]);
  snprintf(itens[5].imdb, sizeof itens[5].imdb, "%s", futuros[1]);
  cwo_publicar_futuros(futuros, 2);

  // Colecoes do add-on (AIOMetadata): um grupo com quatro pastas.
  {
    char js[1800];
    snprintf(js, sizeof js,
      "{\"collections\":[{\"id\":\"cs\",\"title\":\"Coleções\",\"folders\":["
      "{\"id\":\"a\",\"title\":\"Saga Estelar\",\"coverImageUrl\":\"deploy/app/art/poster/12.jpg\",\"tileShape\":\"POSTER\","
        "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
      "{\"id\":\"b\",\"title\":\"Clássicos\",\"coverImageUrl\":\"deploy/app/art/poster/05.jpg\",\"tileShape\":\"POSTER\","
        "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
      "{\"id\":\"c\",\"title\":\"Premiados\",\"coverImageUrl\":\"deploy/app/art/poster/21.jpg\",\"tileShape\":\"POSTER\","
        "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
      "{\"id\":\"d\",\"title\":\"Noites de chuva\",\"coverImageUrl\":\"deploy/app/art/poster/09.jpg\",\"tileShape\":\"POSTER\","
        "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}");
    assert(col_definir_json(js) == 4);
  }
  cat_definir_tudo(itens, total, fils, NF);
  quadros(120, NULL);
  listar("inicial");

  // --- 1. Continuar assistindo e Proximos episodios, antes dos catalogos -----
  {
    int cw = indiceDa("continue_watching"), up = indiceDa("upcoming_section");
    iCol = indicePrefixo("collection_");
    assert(cw >= 0 && up == cw + 1);
    assert(indiceDa("pop_movie") > up && indiceDa("trend_series") > up);
    assert(iCol > up);
    assert(home_fileira_n("upcoming_section") == 2 && home_fileira_n("continue_watching") == 4);
    printf("[home21] ok  continuar=%d proximos=%d colecoes=%d\n", cw, up, iCol);
  }
  for (r = 0; r < 16; r++) tecla(SDLK_UP);
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-0-topo.bmp", saida); quadros(1, bmp);

  // --- 2. D-pad: cada fileira e alcancavel; cada cartao, pela direita --------
  {
    int n = home_fileiras_n(), q;
    for (q = 0; q < n; q++) {
      char chave[96];
      snprintf(chave, sizeof chave, "%s", home_fileira_chave(q));
      assert(focarFileira(chave));
      for (r = 0; r < home_fileira_n(chave) + 1; r++) tecla(SDLK_RIGHT);
      for (r = 0; r < home_fileira_n(chave) + 1; r++) tecla(SDLK_LEFT);
      assert(!strcmp(home_fileira_focada(), chave));
    }
    printf("[home21] ok  d-pad alcanca as %d fileiras e percorre os cartoes\n", n);
    assert(focarFileira("upcoming_section"));
    snprintf(bmp, sizeof bmp, "%s-1-proximos.bmp", saida); quadros(1, bmp);
  }

  // --- 3. colecoes: segurar OK abre o menu da fileira ------------------------
  {
    char colChave[96];
    snprintf(colChave, sizeof colChave, "%s", home_fileira_chave(iCol));
    assert(focarFileira(colChave));
    snprintf(bmp, sizeof bmp, "%s-2-colecoes.bmp", saida); quadros(1, bmp);
    segurarOk();
    quadros(40, NULL);
    assert(ctx_aberto());
    snprintf(bmp, sizeof bmp, "%s-3-menu-fileira.bmp", saida); quadros(1, bmp);

    // Voltar fecha o menu e devolve o foco a mesma fileira.
    teclaCtx(SDLK_ESCAPE);
    quadros(20, NULL);
    assert(!ctx_aberto() && !strcmp(home_fileira_focada(), colChave));

    // --- 4. fixar: vira fileira propria ---------------------------------------
    assert(cf_n() == 0 && indicePrefixo(CF_PREFIXO) < 0);
    segurarOk(); quadros(30, NULL);
    assert(ctx_aberto());
    teclaCtx(SDLK_RETURN);                  // 1a linha: Fixar na Home
    quadros(60, NULL);
    assert(cf_n() == 1);
    if (ctx_aberto()) teclaCtx(SDLK_ESCAPE);
    quadros(90, NULL);
    listar("fixada");
    {
      int ip = indicePrefixo(CF_PREFIXO);
      assert(ip >= 0 && !ctx_aberto());
      printf("[home21] ok  fixada: fileira %s no lugar %d\n", home_fileira_chave(ip), ip);
    }
    {
      char pin[96];
      snprintf(pin, sizeof pin, "%s", home_fileira_chave(indicePrefixo(CF_PREFIXO)));
      assert(focarFileira(pin));
      snprintf(bmp, sizeof bmp, "%s-4-fixada.bmp", saida); quadros(1, bmp);

      // --- 5. mover: para baixo e para cima trocam com a vizinha ---------------
      {
        int a0 = indiceDa(pin);
        assert(a0 > 0);
        segurarOk(); quadros(30, NULL);
        assert(ctx_aberto());
        teclaCtx(SDLK_DOWN);                // Mover para cima
        teclaCtx(SDLK_RETURN);
        quadros(60, NULL);
        listar("movida");
        assert(indiceDa(pin) == a0 - 1);
        printf("[home21] ok  mover para cima: %d -> %d\n", a0, indiceDa(pin));
        if (ctx_aberto()) teclaCtx(SDLK_ESCAPE);
        quadros(60, NULL);
        assert(focarFileira(pin));
        segurarOk(); quadros(30, NULL);
        assert(ctx_aberto());
        teclaCtx(SDLK_DOWN);
        teclaCtx(SDLK_DOWN);                // Mover para baixo
        teclaCtx(SDLK_RETURN);
        quadros(60, NULL);
        assert(indiceDa(pin) == a0);
        printf("[home21] ok  mover para baixo: volta a %d\n", a0);
        // --- 6. desafixar pelo mesmo menu: a fileira some ---------------------
        if (ctx_aberto()) { teclaCtx(SDLK_ESCAPE); quadros(30, NULL); }
        assert(focarFileira(pin));
        segurarOk(); quadros(30, NULL);
        assert(ctx_aberto());
        teclaCtx(SDLK_RETURN);              // Tirar da Home
        quadros(90, NULL);
        assert(cf_n() == 0 && indiceDa(pin) < 0 && !ctx_aberto());
        printf("[home21] ok  desafixar tira a fileira e fecha o menu\n");
      }
    }
  }

  // --- 7. vistos ocultos (#366) em Popular -----------------------------------
  {
    int n0 = home_fileira_n("pop_movie"), p, vistos = 0;
    cat_historico_definir_id(itens[fils[1].ini].imdb, "movie", 1);
    cat_historico_definir_id(itens[fils[1].ini + 2].imdb, "movie", 1);
    for (p = 0; p < fils[1].n; p++) if (cat_visto(&itens[fils[1].ini + p])) vistos++;
    assert(vistos == 2 && n0 == 10 && !ov_ativa("pop_movie"));
    assert(ov_alternar("pop_movie") == 1 && ov_ativa("pop_movie"));
    quadros(90, NULL);
    assert(home_fileira_n("pop_movie") == n0 - 2);
    assert(focarFileira("pop_movie"));
    snprintf(bmp, sizeof bmp, "%s-5-vistos-ocultos.bmp", saida); quadros(1, bmp);
    // O D-pad ainda percorre os que sobraram.
    for (r = 0; r < home_fileira_n("pop_movie") + 1; r++) tecla(SDLK_RIGHT);
    assert(!strcmp(home_fileira_focada(), "pop_movie"));
    assert(ov_alternar("pop_movie") == 0);
    quadros(90, NULL);
    assert(home_fileira_n("pop_movie") == n0);
    printf("[home21] ok  vistos ocultos: %d -> %d -> %d cartoes\n", n0, n0 - 2, n0);
    // Vistos em tudo: a fileira nao esvazia (fica inteira).
    for (p = 0; p < fils[1].n; p++) cat_historico_definir_id(itens[fils[1].ini + p].imdb, "movie", 1);
    assert(ov_alternar("pop_movie") == 1);
    quadros(90, NULL);
    assert(home_fileira_n("pop_movie") == n0);
    assert(ov_alternar("pop_movie") == 0);
    printf("[home21] ok  tudo visto: a fileira nao esvazia\n");
  }

  // --- 8. ocultar a fileira de colecoes pelo menu ----------------------------
  {
    char colChave[96];
    snprintf(colChave, sizeof colChave, "%s", home_fileira_chave(indicePrefixo("collection_")));
    assert(focarFileira(colChave));
    segurarOk(); quadros(30, NULL);
    assert(ctx_aberto());
    for (r = 0; r < 6; r++) teclaCtx(SDLK_DOWN);   // para na ultima linha: Ocultar fileira
    snprintf(bmp, sizeof bmp, "%s-6-ocultar.bmp", saida); quadros(1, bmp);
    teclaCtx(SDLK_RETURN);
    quadros(90, NULL);
    listar("oculta");
    assert(indiceDa(colChave) < 0 && !ctx_aberto());
    printf("[home21] ok  ocultar fileira de colecoes\n");
    snprintf(bmp, sizeof bmp, "%s-7-depois.bmp", saida); quadros(1, bmp);
  }

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  puts("home21: PASS");
  return 0;
}
