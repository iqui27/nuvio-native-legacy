/* GUIA TV v5 — captura da TELA PRINCIPAL do guia (grade, cartoes, esportes,
 * folhas e estados de erro/vazio), em pt-BR, lado a lado com os PNGs do mock
 * aprovado (tests/guiav5_compare.py monta o <estado>-compare.png).
 *
 * Inclui src/guiav5ui.c E src/guia.c: os canais e a grade EPG sao estaticos
 * dos dois modulos, e semear por dentro e o unico jeito de fotografar o guia
 * sem addon no ar. Os canais e programas sao inventados; nenhum dado de conta
 * passa por aqui. Nao entra na suite (tools/testa-tudo.sh pula *_shot.sh).
 */
#include "../src/guia.c"
#include "../src/guiav5ui.c"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <sys/stat.h>
#include <assert.h>

static int shot_zap;   /* zapping band (guia.c overlay) instead of the v5 screen */
static void capture(SDL_Window *window, const char *dir, const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s-app.png", dir, name);
  for (int frame = 0; frame < 90; frame++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(.045f, .047f, .051f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (shot_zap) desenharBanda(1, SDL_GetTicks());
    else g5ui_draw();
    if (frame == 89) {
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      assert(pixels);
      SDL_Surface *surface =
          SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(surface);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)surface->pixels + y * surface->pitch,
               pixels + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(surface, path) == 0);
      SDL_FreeSurface(surface);
      free(pixels);
    }
    SDL_GL_SwapWindow(window);
    SDL_Delay(8);   /* real time for the springs and entrances to settle */
  }
}

static void xtv(char *dst, time_t t) {
  struct tm *m = gmtime(&t);
  strftime(dst, 24, "%Y%m%d%H%M%S +0000", m);
}

/* Grade de mentira, relativa a meia hora corrente: a linha "agora" cai sempre
 * no comeco da janela, como na TV. Titulos no molde do mock (times e placar)
 * exercitam o heroi, o placar da previa e os cartoes de esporte. */
static void semear(void) {
  static const struct { const char *canal, *titulo; int ini, fim; } P[] = {
    { "SporTV", "Brasileirão: Flamengo × Palmeiras", -0, 118 },
    { "SporTV", "Resenha esportiva", 118, 178 },
    { "SporTV", "Melhores momentos", -62, -2 },
    { "GloboNews", "Jornal da tarde", -22, 18 },
    { "GloboNews", "Economia em pauta", 18, 78 },
    { "GloboNews", "Em foco", 78, 138 },
    { "Discovery", "Caçadores de mitos", -22, 88 },
    { "Discovery", "Expedição selvagem", 88, 148 },
    { "Telecine Pipoca", "Filme: O resgate", -42, 170 },
    { "Telecine Pipoca", "A última estação", 170, 230 },
    { "Gloob", "Mundo Bita", -2, 8 },
    { "Band", "Flamengo hoje", 98, 158 },
    { "Canal Brasil", "Fla × Flu: Retrospectiva", 200, 260 },
    { "Gloob", "Peppa Pig", 8, 28 },
  };
  char *xml = malloc(24000), ini[24], fim[24];
  size_t n = 0;
  assert(xml);
  n += (size_t)snprintf(xml + n, 24000 - n, "%s", "<?xml version=\"1.0\"?><tv>");
  static const char *CH[] = { "SporTV", "GloboNews", "Discovery", "Telecine Pipoca", "Gloob" };
  for (unsigned k = 0; k < sizeof CH / sizeof *CH; k++)
    n += (size_t)snprintf(xml + n, 24000 - n,
      "<channel id=\"%s\"><display-name>%s</display-name></channel>", CH[k], CH[k]);
  for (unsigned k = 0; k < sizeof P / sizeof *P; k++) {
    xtv(ini, time(NULL) + (time_t)P[k].ini * 60);
    xtv(fim, time(NULL) + (time_t)P[k].fim * 60);
    n += (size_t)snprintf(xml + n, 24000 - n,
      "<programme channel=\"%s\" start=\"%s\" stop=\"%s\"><title>%s</title></programme>",
      P[k].canal, ini, fim, P[k].titulo);
  }
  snprintf(xml + n, 24000 - n, "</tv>");
  assert(epg_xml_processar(xml) > 0);
  free(xml);

  static const struct { const char *nome; int cat; int fav; } C[] = {
    { "SporTV", 0, 1 }, { "GloboNews", 1, 1 }, { "Discovery", 2, 1 },
    { "Telecine Pipoca", 3, 0 }, { "Record", 4, 1 }, { "BBC News", 1, 0 },
    { "CNN Int'l", 1, 0 }, { "Gloob", 5, 0 }, { "Canal Brasil", 3, 0 },
    { "Band", 1, 0 }, { "Flash News 24", 1, 0 }, { "Canal Flamengo TV", 0, 0 },
  };
  static const char *CATS[] = { "Esportes", "Notícias", "Documentários", "Filmes", "Abertos", "Infantil" };
  static const char *IDS[] = { "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "c10", "c11", "c12" };
  static const int INI[] = { 0, 1, 2, 3, 4, 7 }, NUM[] = { 1, 4, 1, 2, 1, 3 };
  nCats = 6;
  nCanais = (int)(sizeof C / sizeof *C);
  if (!cats) cats = calloc(8, sizeof *cats);
  if (!canais) canais = calloc(16, sizeof *canais);
  if (!catIni) catIni = calloc(8, sizeof *catIni);
  if (!catN) catN = calloc(8, sizeof *catN);
  assert(cats && canais && catIni && catN);
  for (int i = 0; i < nCats; i++) {
    cats[i] = (GCat){ CATS[i], 0, i };
    catIni[i] = INI[i]; catN[i] = NUM[i];
  }
  for (int i = 0; i < nCanais; i++) {
    GCanal *c = &canais[i];
    *c = (GCanal){0};
    c->id = IDS[i];
    c->nome = C[i].nome;
    c->base = "FrostView";
    c->cat = C[i].cat;
    c->epg = -1;
    c->fav = C[i].fav;
    c->numero = 101 + i;
  }
  estado = G_PRONTO;
  dados_gravar("guia-fav.txt", "c1\nc2\nc3\nc5\n");
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/rev/guiav5-png/principal";
  mkdir(dir, 0755);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Guia v5 principal", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(window);
  assert(SDL_GL_CreateContext(window));
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  /* NUVIO_SHOT_ESCALA=85|100 and NUVIO_SHOT_VIDRO=0|1 pick the text scale and
   * material; default stays 100% glass. */
  {
    char aj[160];
    const char *esc = getenv("NUVIO_SHOT_ESCALA");
    snprintf(aj, sizeof aj, "idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal %d\nescalaTextoVersaoLocal 1\n",
             esc && atoi(esc) == 85 ? 0 : 2);
    dados_gravar("ajustes.txt", aj);
  }
  ajustes_dir(getenv("NUVIO_DADOS"));
  gfx_escala_txt_definir(ajustes_escala_texto());
  ajustes_definir_vidro(!getenv("NUVIO_SHOT_VIDRO") || atoi(getenv("NUVIO_SHOT_VIDRO")));
  assert(ajustes_idioma() == 0);
  context();
  semear();
  const char *names[] = {
    "grade", "cartoes", "preview", "preview-grade", "preview-foco", "preview-hero-cheio",
    "preview-troca", "dock", "catchup", "categorias", "pastas", "busca", "canal-opcoes",
    "numero", "opcoes", "ajustes", "epg-saude", "epg-verificando", "epg-adicionado",
    "epg-falha", "erros", "fontes-erro", "vazio", "esportes-ligas", "esportes-liga",
    "esportes-favoritos", "esportes-ajustes", "esportes-onde-assistir", "zap",
    /* navigation states without a mock: base page @ focus */
    "grade@rolada", "grade@futuro", "grade@heroi", "grade@chips", "grade@topo", "cartoes@lateral",
  };
  printf("shot: glass=%d text=%.2f\n", ajustes_vidro(), gfx_escala_txt());
  const char *only = getenv("NUVIO_SHOT_SO");   /* iterate on one state */
  for (unsigned n = 0; n < sizeof names / sizeof *names; n++) {
    if (only && *only && !strstr(only, names[n])) continue;
    if (!strcmp(names[n], "vazio")) {
      nCanais = 0; nCats = 0; gv_n = 0; gv_total_n = 0;  /* estado vazio real */
    }
    if (!strcmp(names[n], "zap")) {
      nLtvF = 1; snprintf(ltvF[0].nome, sizeof ltvF[0].nome, "FrostView"); ltvF[0].tipo = LTV_M3U;
      g5ui_close(); vistaMontar(); focoValido(); overlay = 1; shot_zap = 1;
      capture(window, dir, names[n]);
      overlay = 0; shot_zap = 0;
      continue;
    }
    char page[48];
    snprintf(page, sizeof page, "%s", names[n]);
    if (strchr(page, '@')) *strchr(page, '@') = 0;
    g5ui_open(page);
    gv_zone = 0; gv_lin = 0; gv_pin = 0; gv_sb = 3; gv_chipi = 0; gv_card = 0; gv_hbt = 0;
    gv_snap_force = 1; gv_snapshot();
    if (!strcmp(names[n], "grade@rolada")) gv_lin = 6;
    if (!strcmp(names[n], "grade@futuro")) for (int k = 0; k < 3; k++) gv_tempo_passo(1);
    if (!strcmp(names[n], "grade@heroi")) { gv_zone = 2; gv_hbt = 1; }
    if (!strcmp(names[n], "grade@chips")) { gv_zone = 4; gv_chipi = 1; }
    if (!strcmp(names[n], "grade@topo")) { gv_zone = 5; focus = 1101; }
    if (!strcmp(names[n], "cartoes@lateral")) { gv_zone = 3; gv_sb = 2; }
    if (!strcmp(names[n], "preview-foco")) { gv_zone = 2; gv_hbt = 2; }
    if (!strcmp(names[n], "dock")) gv_zone = 1;
    if (!strcmp(names[n], "categorias")) { gv_zone = 3; gv_sb = 6; }
    if (!strcmp(page, "cartoes") && gv_zone == 0) gv_zone = 6;
    if (!strcmp(names[n], "numero")) snprintf(gv_numero, sizeof gv_numero, "1");
    if (!strcmp(names[n], "busca")) snprintf(query, sizeof query, "fla");
    if (!strcmp(names[n], "fontes-erro")) { falhas = 0; }
    capture(window, dir, names[n]);
    if (!strcmp(names[n], "vazio")) semear();
  }
  puts("PASS: native v5 guide states captured");
  return 0;
}
