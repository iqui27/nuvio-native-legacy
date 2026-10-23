// CAPTURAS DA TELA DE PLUGINS, sem rede: o estado entra pelo disco
// (estado por conta/perfil e manifesto em cache, como plugins.c grava), e a foto
// prova o formato dos dois arquivos alem do desenho.
//   -lista     ligado, um repositorio JS lido e um DEX do Android
//   -remover   direita abre o primeiro repositorio; OK arma a remocao
//   -teclado   o OK em "Adicionar repositorio" abre o teclado de URL
//   -desligado o mesmo estado com a chave geral desligada
#include "plugins.h"
#include "pluginsui.h"
#include "teclado.h"
#include "layout.h"
#include "menu.h"
#include "dados.h"
#include "ajustes.h"
#include "rail_shot.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned long long fnv(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(32); gfx_novo_quadro();
    pluginsui_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    pluginsui_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_AJUSTES);
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s; int y;
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0);
      SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; pluginsui_evento(&e);
  e.type = SDL_KEYUP; pluginsui_evento(&e);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-pluginsui";
  const char *manifesto = getenv("PLUG_MANIFESTO");
  const char *url = "http://127.0.0.1:9/manifest.json";
  const char *fixture = "{\"name\":\"Repo Teste\",\"scrapers\":[{\"id\":\"bom\",\"name\":\"Bom\",\"filename\":\"bom.js\"}]}";
  char nome[600], c[700];
  SDL_Window *w; FILE *f;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: plugins", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  assert(SDL_GL_CreateContext(w));
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(160);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(".");
  snprintf(c, sizeof c, "%s/ajustes.txt", dados_dir());
  f = fopen(c, "w"); if (f) { fprintf(f, "idioma 0\nanimacoes 1\n"); fclose(f); }
  ajustes_dir(dados_dir());
  // o estado, pelo disco
  snprintf(c, sizeof c, "%s/plugins-00000000-p1.txt", dados_dir());
  f = fopen(c, "w"); assert(f);
  fprintf(f, "ligado\t1\nrepo\t1\tNUVIO_JS\tYoru's Repo\t%s\n"
             "repo\t1\tEXTERNAL_DEX\tCloudStream do celular\thttps://exemplo.org/repo.cs3\n"
             "repo\t0\tNUVIO_JS\t\thttp://127.0.0.1:9/sem-resposta/manifest.json\n", url);
  fclose(f);
  snprintf(c, sizeof c, "%s/plugins-c-%016llx.txt", dados_dir(), fnv(url));
  f = fopen(c, "w"); assert(f);
  fprintf(f, "//nvcache %ld\n%s", (long)time(NULL), fixture); fclose(f);
  snprintf(c, sizeof c, "%s/plugins-c-%016llx.txt", dados_dir(),
           fnv("http://127.0.0.1:9/sem-resposta/manifest.json"));
  f = fopen(c, "w"); assert(f);
  fprintf(f, "//nvcache %ld\n{}", (long)time(NULL)); fclose(f);
  if (manifesto) {
    FILE *m = fopen(manifesto, "rb"); long n; char *b;
    assert(m); fseek(m, 0, SEEK_END); n = ftell(m); fseek(m, 0, SEEK_SET);
    b = malloc((size_t)n + 1); assert(fread(b, 1, (size_t)n, m) == (size_t)n); b[n] = 0; fclose(m);
    snprintf(c, sizeof c, "%s/plugins-c-%016llx.txt", dados_dir(), fnv(url));
    f = fopen(c, "w"); assert(f); fprintf(f, "//nvcache %ld\n%s", (long)time(NULL), b); fclose(f); free(b);
  }
  plugins_iniciar();
  assert(plugins_ligado() && plugins_n_repos() == 3);
  assert(plugins_repo_scrapers(0, NULL) > 0);
  pluginsui_abrir();
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  assert(pluginsui_foco() == 2);
  snprintf(nome, sizeof nome, "%s-lista.png", saida); captura(nome, w);
  tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  assert(pluginsui_foco() == 0 && plugins_n_repos() == 3);
  snprintf(nome, sizeof nome, "%s-remover.png", saida); captura(nome, w);
  tecla(SDLK_LEFT); tecla(SDLK_UP); tecla(SDLK_RETURN);
  assert(teclado_aberto());
  snprintf(nome, sizeof nome, "%s-teclado.png", saida); captura(nome, w);
  tecla(SDLK_ESCAPE);
  assert(!teclado_aberto());
  plugins_definir_ligado(0);
  pluginsui_abrir();
  snprintf(nome, sizeof nome, "%s-desligado.png", saida); captura(nome, w);
  return 0;
}
