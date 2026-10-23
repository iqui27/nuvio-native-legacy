// Real Home entry/rendering plus cast OK routing across all saved home layouts.
#include <assert.h>
#include "dados.h"
#include "addons.h"
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#define pessoa_pedir capture_person
#define txt_linha capture_line
#define extras_n_colecao fixture_collection_count
#include "../src/detail.c"
#undef pessoa_pedir
#undef txt_linha
#undef extras_n_colecao

TxtLinha txt_linha(TxtEstilo style, const char *text, int r, int g, int b, int a);
static int layout, collection_count, requests, filmography_draws;
TxtLinha capture_line(TxtEstilo style, const char *text, int r, int g, int b, int a) {
  if (!strcmp(text, "Filmografia")) filmography_draws++;
  return txt_linha(style, text, r, g, b, a);
}
static long requested_id;
static char requested_name[64], requested_photo[512];
int fixture_collection_count(void) { return collection_count; }
void capture_person(long id, const char *name, const char *photo) {
  requests++;
  requested_id = id;
  snprintf(requested_name, sizeof requested_name, "%s", name);
  snprintf(requested_photo, sizeof requested_photo, "%s", photo);
}

int main(void) {
  CatItem title = {0};
  int cases = 0;
  const char *dir = getenv("NUVIO_DADOS");
  char settings[700];
  assert(dir && strstr(dir, "nuvio-detail-cast-ok."));
  dados_iniciar("deploy/app/art");
  snprintf(settings, sizeof settings, "%s/ajustes.txt", dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Cast OK", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext gl = SDL_GL_CreateContext(window);
  assert(gl);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(32);
  gfx_borrao_iniciar(480, 270);
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&addon, 1));
  SDL_Delay(2); // Zero ticks is the handler's sentinel for no OK press.
  snprintf(title.titulo, sizeof title.titulo, "Cast test");
  snprintf(title.poster, sizeof title.poster, "deploy/app/art/poster/00.jpg");
  snprintf(title.backdrop, sizeof title.backdrop, "deploy/app/art/00.jpg");
  title.nElenco = 3;
  for (int actor = 0; actor < title.nElenco; actor++) {
    title.elenco[actor].tmdb = 5150 + actor;
    snprintf(title.elenco[actor].nome, sizeof title.elenco[actor].nome, "Actor %d", actor);
    snprintf(title.elenco[actor].foto, sizeof title.elenco[actor].foto, "photo-%d.jpg", actor);
  }
  for (layout = 0; layout < HOME_LAYOUT_N; layout++) {
    FILE *f = fopen(settings, "w");
    assert(f);
    fprintf(f, "homeLayoutLocal %d\ntrailerHero 1\n", layout);
    fclose(f);
    ajustes_dir(dir);
    assert(ajustes_home_layout() == layout);
    snprintf(title.tipo, sizeof title.tipo, "movie");
    CatFileira row = { .chave = "cast_test", .titulo = "Cast test", .tipo = "movie", .n = 1 };
    cat_definir_tudo(&title, 1, &row, 1);
    aberto = 0;
    home_atualizar(1.0f / 60, SDL_GetTicks());
    home_ir_topo();
    SDL_Event nav = { .type = SDL_KEYDOWN };
    nav.key.keysym.sym = SDLK_DOWN;
    home_evento(&nav);
    for (int frame = 0; frame < 90; frame++) home_atualizar(1.0f / 60, SDL_GetTicks());
    txt_novo_quadro(); gfx_novo_quadro();
    home_desenhar(SDL_GetTicks());
    HomeItem selected;
    assert(home_item_focado(&selected));
    nav.key.keysym.sym = SDLK_RETURN;
    home_evento(&nav);
    nav.type = SDL_KEYUP; home_evento(&nav);
    assert(home_pediu_abrir());
    detail_abrir(&selected);
    nav.type = SDL_KEYDOWN; nav.key.keysym.sym = SDLK_DOWN;
    assert(carro == (layout == HOME_LAYOUT_DINAMICA));
    for (int frame = 0; frame < 90; frame++) detail_atualizar(1.0f / 60, SDL_GetTicks());
    for (int step = 0; step < N_SECOES + 2 && (nivel == 0 || foco.fileira != SEC_ELENCO); step++)
      detail_evento(&nav);
    assert(nivel == 1 && foco.fileira == SEC_ELENCO);
    requests = filmography_draws = 0;
    nav.key.keysym.sym = SDLK_RETURN;
    detail_evento(&nav);
    detail_atualizar(1.0f / 60, SDL_GetTicks());
    nav.type = SDL_KEYUP;
    detail_evento(&nav);
    detail_atualizar(1.0f / 60, SDL_GetTicks());
    txt_novo_quadro(); gfx_novo_quadro();
    detail_desenhar(SDL_GetTicks());
    if (requests != 1 || !pessoaAberta || !filmography_draws) {
      fprintf(stderr, "FAIL: Home entry layout=%d requests=%d open=%d drawn=%d\n", layout, requests, pessoaAberta, filmography_draws);
      return 1;
    }
    for (int series = 0; series < 2; series++) {
      snprintf(title.tipo, sizeof title.tipo, "%s", series ? "series" : "movie");
      cat_definir_tudo(&title, 1, NULL, 0);
      idx = 0;
      for (int tabs = 0; tabs < 2; tabs++) {
        collection_count = tabs ? 2 : 0;
        abaInfo = 0;
        assert((secaoN(SEC_ABAS_INFO) > 0) == (series && tabs));
        assert(secaoN(SEC_ELENCO) == title.nElenco);
        for (int carousel = 0; carousel <= (layout == HOME_LAYOUT_DINAMICA); carousel++) {
          carro = carousel;
          carCheia = 1;
          carPos = carAplicado = 0;
          for (int keypad = 0; keypad < 2; keypad++) {
            for (int actor = 0; actor < title.nElenco; actor++) {
              SDL_Event event = {0};
              aberto = 1; saindo = 0; nivel = 1;
              pessoaAberta = colListaAberta = 0;
              foco.fileira = SEC_ELENCO; foco.coluna = actor;
              okDesceEm = 0;
              requests = 0; requested_id = 0;
              event.key.keysym.sym = keypad ? SDLK_KP_ENTER : SDLK_RETURN;
              event.type = SDL_KEYDOWN;
              detail_evento(&event);
              detail_atualizar(1.0f / 60, SDL_GetTicks());
              event.type = SDL_KEYUP;
              detail_evento(&event);
              detail_atualizar(1.0f / 60, SDL_GetTicks());
              filmography_draws = 0;
              txt_novo_quadro(); gfx_novo_quadro();
              detail_desenhar(SDL_GetTicks());
              assert(filmography_draws == 1);
              if (requests != 1 || requested_id != title.elenco[actor].tmdb || !pessoaAberta) {
                fprintf(stderr, "FAIL: layout=%d series=%d tabs=%d carousel=%d keypad=%d actor=%d requests=%d id=%ld open=%d\n",
                        layout, series, tabs, carousel, keypad, actor, requests, requested_id, pessoaAberta);
                return 1;
              }
              assert(!strcmp(requested_name, title.elenco[actor].nome));
              assert(!strcmp(requested_photo, title.elenco[actor].foto));
              cases++;
            }
          }
        }
      }
    }
  }
  // A collection in the shared slot must not request the actor at column zero.
  snprintf(title.tipo, sizeof title.tipo, "series");
  cat_definir_tudo(&title, 1, NULL, 0);
  idx = 0; collection_count = 2; abaInfo = 1;
  assert(abaIdDe(abaInfo) == ABA_COLECAO);
  for (layout = 0; layout < HOME_LAYOUT_N; layout++) {
    FILE *f = fopen(settings, "w");
    assert(f);
    fprintf(f, "homeLayoutLocal %d\ntrailerHero 1\n", layout);
    fclose(f); ajustes_dir(dir);
    assert(ajustes_home_layout() == layout);
    SDL_Event event = {0};
    carro = 0; aberto = 1; saindo = 0; nivel = 1;
    pessoaAberta = colListaAberta = 0;
    foco.fileira = SEC_ELENCO; foco.coluna = 0;
    okDesceEm = 0; requests = 0;
    event.key.keysym.sym = SDLK_RETURN;
    event.type = SDL_KEYDOWN; detail_evento(&event);
    event.type = SDL_KEYUP; detail_evento(&event);
    assert(!requests && !pessoaAberta && colListaAberta);
  }
  printf("PASS: real Home entry in all 3 saved layouts, including Dinamica carousel; %d cast OK/update/draw cases; collection routing preserved\n", cases);
  tex_encerrar(); txt_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
