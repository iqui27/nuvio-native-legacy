// Mac + rede real: mesmos quadros para primeira/segunda abertura e reinicio.
// Sem conta; cache isolado. Os marcos sao do desenho de producao.
#include "app.h"
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "descoberta.h"
#include "detail.h"
#include "gfx.h"
#include "home.h"
#include "js.h"
#include "marco.h"
#include "metaprov.h"
#include "rede.h"
#include "tex_cache.h"
#include "text.h"
#include "gl_compat.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void quadros(SDL_Window *w, unsigned ms, int detalhe) {
  Uint32 inicio = SDL_GetTicks();
  while (SDL_GetTicks() - inicio < ms) {
    SDL_PumpEvents(); cat_quadro(); txt_novo_quadro(); tex_novo_quadro();
    tex_bombear(4); gfx_novo_quadro();
    glClearColor(.05f,.05f,.05f,1); glClear(GL_COLOR_BUFFER_BIT);
    if (detalhe) { detail_atualizar(.016f, SDL_GetTicks()); detail_desenhar(SDL_GetTicks()); }
    else { home_atualizar(.016f, SDL_GetTicks()); home_desenhar(SDL_GetTicks()); }
    SDL_GL_SwapWindow(w); SDL_Delay(16);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; home_evento(&e);
  e.type = SDL_KEYUP; home_evento(&e);
}
int main(int argc, char **argv) {
  assert(argc >= 2);
  setenv("NUVIO_DADOS", argv[1], 1);
  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio logo/info: rede real",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  assert(w); assert(SDL_GL_CreateContext(w)); SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080); gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1));
  assert(tex_iniciar(96)); artehero_definir_falhou(tex_falhou);
  char cache[700]; snprintf(cache,sizeof cache,"%s/cache",argv[1]); tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  assert(dados_gravar_leve("ajustes.txt", "trailerHero 0\ntrailerAuto 0\npreferExternalMetaAddonDetail 0\ntmdb_enabled 0\nheroDifferentFromCard 0\n"));
  ajustes_dir(dados_dir()); desc_tmdb("deploy/app/art");
  assert(home_iniciar("deploy/app/art"));
  // Um snapshot real compartilhado pelo A/B evita trocar os titulos entre runs.
  const char *catalogo = getenv("NUVIO_BENCH_CATALOGO");
  char *json = catalogo ? dados_ler(catalogo) : NULL;
  if (!json) {
    json = rede_baixar("https://v3-cinemeta.strem.io/catalog/movie/top.json",20);
    if (json && catalogo) assert(dados_gravar_leve(catalogo, json));
  }
  assert(json);
  const char *p = js_array(json,NULL,"metas");
  CatItem v[3] = {0};
  for (int i=0; i<3; i++) {
    assert(p); const char *fim=js_fim(p);
    js_texto(p,fim,"id",v[i].imdb,sizeof v[i].imdb);
    js_texto(p,fim,"name",v[i].titulo,sizeof v[i].titulo);
    js_texto(p,fim,"poster",v[i].poster,sizeof v[i].poster);
    js_texto(p,fim,"background",v[i].backdrop,sizeof v[i].backdrop);
    js_texto(p,fim,"logo",v[i].logo,sizeof v[i].logo);
    js_texto(p,fim,"releaseInfo",v[i].meta,sizeof v[i].meta);
    js_texto(p,fim,"description",v[i].sinopse,sizeof v[i].sinopse);
    v[i].nota = (int)(js_num(p,fim,"imdbRating",0) * 10 + .5);
    snprintf(v[i].tipo,sizeof v[i].tipo,"movie");
    printf("[bench] catalogo id=%s logo=%d sinopse=%d\n",v[i].imdb,!!v[i].logo[0],!!v[i].sinopse[0]);
    p=js_prox(fim);
  }
  free(json);
  // Opcional: catalogo sem logo, caso comum de Salvos/busca.
  if (argc>2 && !strcmp(argv[2],"sem-logo")) {
    v[1].logo[0]=v[2].logo[0]=0;
    // Salvos com texto, mas sem logo: mesma regra do catalogo de producao.
    strcpy(v[1].sinopse,"Sinopse ja presente no catalogo.");
    strcpy(v[2].sinopse,"Sinopse ja presente no catalogo.");
  }
  CatFileira f={0}; strcpy(f.chave,"cinemeta_movie_top"); strcpy(f.titulo,"Filmes");
  strcpy(f.tipo,"movie"); f.n=2;
  cat_definir_tudo(v,3,&f,1);
  marco_iniciar();
  for (int rodada=1; rodada<=2; rodada++) {
    printf("[bench] detalhe rodada=%d\n",rodada); fflush(stdout);
    const CatItem *c=cat_item(2);
    HomeItem it={0}; it.indice=2; it.rect=(GfxRect){96,600,200,300};
    it.arte=c->backdrop; it.titulo=c->titulo; it.genero=c->genero; it.meta=c->meta;
    detail_abrir(&it); quadros(w,6000,1);
    detail_fechar_seco(); quadros(w,500,1);
  }
  quadros(w,500,0); tecla(SDLK_DOWN); quadros(w,100,0); tecla(SDLK_RIGHT);
  quadros(w,6000,0);
  tecla(SDLK_LEFT); quadros(w,700,0); tecla(SDLK_RIGHT); quadros(w,3000,0);
  puts("[bench] fim"); fflush(stdout);
  // Workers de enriquecimento sao destacados; o processo encerra todos juntos.
  return 0;
}
