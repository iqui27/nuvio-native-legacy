// Local 1080p Home fixture, both synopsis sizes in pt/de/ru; no network.
// The row mirrors herologo_shot.c (chave/tipo que a Home reconhece), the art
// is deploy/app/art (NN.jpg, poster/NN.jpg) as in homelayouts_shot.c.
#define main unused_herologo_main
#include "herologo_shot.c"
#undef main
#include "idiomacod.h"
#include "addons.h"

// Um addon de mentira: homevazio_ativo esconde a Home sem nenhum instalado.
static void addonFake(void) {
  AddonRemoto a;
  memset(&a, 0, sizeof a);
  snprintf(a.nome, sizeof a.nome, "Fixture");
  snprintf(a.url, sizeof a.url, "https://addon.invalid/x");
  a.ativo = 1;
  assert(addons_definir_lista(&a, 1));
}

#define NT 3
int main(int argc, char **argv) {
  assert(argc == 3);
  const char *dir = argv[1], *out = argv[2];
  const int idiomas[] = {IDIOMA_PT, IDIOMA_DE, IDIOMA_RU};
  const char *cod[] = {"pt", "de", "ru"};
  const char *syn[] = {
    "Durante uma viagem por uma cidade desconhecida, uma jovem descobre cartas que revelam a história de sua família. Com a ajuda de um velho amigo, ela precisa reconstruir o passado e decidir em quem confiar antes que os segredos mudem para sempre o seu futuro.",
    "Während einer Reise durch eine unbekannte Stadt entdeckt eine junge Frau Briefe, die die Geschichte ihrer Familie enthüllen. Gemeinsam mit einem alten Freund muss sie die Vergangenheit rekonstruieren und entscheiden, wem sie vertrauen kann, bevor die Geheimnisse ihre Zukunft für immer verändern.",
    "Во время путешествия по незнакомому городу молодая женщина находит письма, раскрывающие историю её семьи. Вместе со старым другом она должна восстановить прошлое и решить, кому можно доверять, прежде чем тайны навсегда изменят её будущее."
  };
  static CatItem itens[NT];
  CatFileira fil;
  SDL_Window *w;
  SDL_GLContext gl;
  char cache[600], cam[700];
  int i, lang, large;
  FILE *a;
  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Synopsis", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192); artehero_definir_falhou(tex_falhou);
  snprintf(cache, sizeof cache, "%s/cache", dir);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  a = fopen(cam, "w"); assert(a);
  fprintf(a, "trailerHero 1\nidioma 0\nidiomaAutoLocal 0\n");
  fclose(a);
  ajustes_dir(dir);
  addonFake();
  assert(home_iniciar("deploy/app/art"));
  memset(itens, 0, sizeof itens);
  for (i = 0; i < NT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt000000%d", i + 1);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Uma viagem inesperada");
    snprintf(c->genero, sizeof c->genero, "Drama");
    snprintf(c->meta, sizeof c->meta, "2024 · 2 h 04 min");
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", i);
  }
  memset(&fil, 0, sizeof fil);
  snprintf(fil.chave, sizeof fil.chave, "cinemeta_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Populares - Filme");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = NT;
  for (lang = 0; lang < 3; lang++) {
    assert(ajustes_shot_valor("idioma", idiomas[lang] + 1));
    for (i = 0; i < NT; i++)
      snprintf(itens[i].sinopse, sizeof itens[i].sinopse, "%s", syn[lang]);
    cat_definir_tudo(itens, NT, &fil, 1);
    for (large = 0; large < 2; large++) {
      char path[700];
      assert(ajustes_shot_valor("heroSynopsisSizeLocal", large));
      snprintf(path, sizeof path, "%s/%s-%s.bmp", out, cod[lang], large ? "grande" : "normal");
      quadros(w, 90, path);
    }
  }
  tex_encerrar(); txt_encerrar(); SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w); SDL_Quit(); return 0;
}
