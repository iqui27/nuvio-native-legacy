#define desc_vertudo_n shot_vt_n
#define desc_vertudo_item shot_vt_item
#include "../src/vertudo.c"
#undef desc_vertudo_n
#undef desc_vertudo_item
#include "nvtmp.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

extern int ajustes_teste_focar_opcao(int op);
extern int ajustes_teste_op_por_chave(const char *chave);
extern void ajustes_teste_escala(int percentual);

static CatItem semente[14];
static int nSemente;
int shot_vt_n(void) { return nSemente; }
int shot_vt_item(int i, CatItem *dst) {
  if (i < 0 || i >= nSemente) return 0;
  if (dst) *dst = semente[i];
  return 1;
}

static SDL_Window *janela;
static void captura(const char *nome, int quadros) {
  for (int i = 0; i < quadros; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_atualizar(1.0f / 60.0f, agora);
    ajustes_desenhar(agora);
    vertudo_atualizar(1.0f / 60.0f, agora);
    vertudo_desenhar(agora);
    if (nome && i == quadros - 1) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(pix && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
      printf("captura: %s\n", nome);
    }
    SDL_GL_SwapWindow(janela);
    SDL_Delay(4);
  }
}

static ColFolder pasta;
static ColSource fontes[2];

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-colecao-padrao-png");
  char nome[700], cam[700];
  FILE *a;

  snprintf(cam, sizeof cam, "%s/nv-colexib.XXXXXX", nvt_dir());
  assert(mkdtemp(cam));
  setenv("NUVIO_DADOS", cam, 1);
  dados_iniciar("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
  a = fopen(cam, "w"); assert(a);
  fprintf(a, "idioma 0\n");
  fprintf(a, "colecaoExibicaoPadraoLocal %d\n", VT_LAY_MURAL);
  fclose(a);
  ajustes_dir(dados_dir());
  ajustes_teste_escala(100);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: colecao padrao", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  assert(SDL_GL_CreateContext(janela));
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(160);
  gfx_borrao_iniciar(480, 270);
  gfx_icones_dir("deploy/app/art");

  nSemente = 14;
  for (int i = 0; i < nSemente; i++) {
    snprintf(semente[i].imdb, sizeof semente[i].imdb, "tt80%05d", i);
    snprintf(semente[i].tipo, sizeof semente[i].tipo, "movie");
    snprintf(semente[i].titulo, sizeof semente[i].titulo, "Titulo %d", i);
    snprintf(semente[i].meta, sizeof semente[i].meta, "%d", 1970 + (i % 5) * 10);
    snprintf(semente[i].backdrop, sizeof semente[i].backdrop, "deploy/app/art/%02d.jpg", (i * 3) % 40);
    snprintf(semente[i].poster, sizeof semente[i].poster, "deploy/app/art/poster/%02d.jpg", (i * 3) % 40);
  }
  pasta.sources = fontes;
  pasta.nSources = 1;
  snprintf(pasta.id, sizeof pasta.id, "starwars");
  snprintf(pasta.title, sizeof pasta.title, "Star Wars");
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  snprintf(fontes[0].title, sizeof fontes[0].title, "Filmes");
  snprintf(fontes[0].type, sizeof fontes[0].type, "movie");
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0; tabFocus = 0;

  assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("colecaoExibicaoPadraoLocal")));
  snprintf(nome, sizeof nome, "%s/ajuste-exibicao-padrao-colecoes.bmp", saida);
  captura(nome, 90);

  layEscolha = VT_LAY_AUTO;
  resolverLayout();
  assert(lay == VT_LAY_MURAL);
  foco = 1;
  snprintf(nome, sizeof nome, "%s/colecao-seguindo-padrao-mural.bmp", saida);
  captura(nome, 90);

  return 0;
}
