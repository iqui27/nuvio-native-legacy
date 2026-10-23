// "RETOMAR AGORA" COM O RELOGIO DESLIGADO (dono, 03/10: "quando nao tem o
// relogio, o continue watching fica com um card grande com o poster dentro;
// tem que colocar uma imagem cropada para preencher tudo"). Sem relogio a
// sessao que acabou de sair vira a faixa FILEIRA_RETORNO (680x178) no topo da
// home. Captura com fundo 16:9 e com SO o cartaz (addon sem fundo), e confere
// que o titulo da faixa NAO esta em "Continuar assistindo" ao mesmo tempo.
//
//   bash tests/retomar_shot.sh /Volumes/ExternalSSD/nv-ui-w20-shots
// (cabecalho herdado de homelayouts_shot.c)
// CAPTURA DA HOME NOS TRES LAYOUTS (Moderna, Padrao, Dinamica), SEM REDE.
//
// As artes sao as de deploy/app/art: fundo 16:9 (NN.jpg), cartaz 2:3
// (poster/NN.jpg) e logo (logo/NN.png). As fileiras imitam o que a descoberta
// publica de verdade: "Continuar assistindo", um catalogo de destaque, um "Em
// alta", generos e um "Top 10" — os SINAIS pelos quais o layout Dinamica
// escolhe a forma de cada fileira (home.c, dinTipoDaFileira).
//
// Uso (o .sh compila e converte para PNG):
//   bash tests/homelayouts_shot.sh /tmp/nv-home-layouts-shots [camadas]
//
// `camadas` = digitos dos layouts a capturar, "012" por padrao. Para cada layout
// roda duas passadas, com a Interface de vidro desligada e ligada, e em cada
// uma fotografa o repouso no destaque e o foco em CADA fileira. O nome do
// arquivo diz tudo: <prefixo>-L<layout>-g<vidro>-<n>-<fileira>.bmp.
//
// Ao final imprime, por captura, o preenchimento (gfx_fill, em telas cheias) e
// o numero de retangulos do quadro — o que da para medir no Mac do custo de
// desenho; o custo em ms de GPU so existe na TV.
#include "ajustes.h"
#include "nvtmp.h"
#include "artehero.h"
#include "catalogo.h"
#include "colecoes.h"
#include "corviva.h"
#include "ctxmenu.h"
#include "cwretido.h"
#include "dados.h"
#include "fileiras.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "menu.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include "posterprov.h"
#include "trakt.h"
#include <unistd.h>
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
  "Ouro Velho", "O Jardim", "Marés", "Sem Volta",
};
#define NN (int)(sizeof NOMES / sizeof *NOMES)

typedef struct { const char *chave, *titulo, *tipo; int n, catalogo; } Fil;
static const Fil FILS[] = {
  { "continue_watching", "Continuar assistindo", "movie",  6, 0 },
  { "pop_movie",  "Popular - Filme",         "movie",  10, 1 },
  { "trend_series", "Em alta - Série",       "series", 10, 1 },
  { "drama_movie", "Drama - Filme",          "movie",  10, 1 },
  { "top10_hoje",  "Top 10 · Filmes hoje",   "movie",  10, 1 },
  { "comedia_movie", "Comédia - Filme",      "movie",  10, 1 },
  { "ficcao_movie", "Ficção científica - Filme", "movie", 10, 1 },
};
#define NF (getenv("NV_COL") ? 3 : (int)(sizeof FILS / sizeof *FILS))

static SDL_Window *janela;
static const char *dirDados;
static double fillUlt, fillVisUlt, modoUlt[GFX_NMODOS]; static int rectUlt;

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
    // NV_MENU=1: a barra por cima, como app.c (no Dinamica, a pilula do topo).
    if (getenv("NV_MENU")) {
      menu_pilula_mostrar(home_topo_fracao());
      menu_atualizar(1.0f / 60.0f, agora);
      menu_desenhar(agora);
    }
    if (i == n - 1) { fillUlt = gfx_fill; fillVisUlt = gfx_fill_vis; rectUlt = gfx_n_rect;
                      memcpy(modoUlt, gfx_fill_modo, sizeof modoUlt); }
    if (bmp && i == n - 1) gravar(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(8);
  }
}


static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
}

static void ajustaSemRelogio(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma 0\ntrailerHero 1\nhomeLayoutLocal 0\nvidroLocal 0\n"
             "modernLandscapePostersEnabled 1\nrelogioTelaLocal 1\n");
  fclose(a);
  ajustes_dir(dirDados);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-retomar-shots/r");
  static CatItem itens[200];
  static CatFileira fils[8];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k, total = 0, ini = 0, passo;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-retomar-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: retomar", SDL_WINDOWPOS_CENTERED,
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
  ajustaSemRelogio();
  assert(!ajustes_relogio_ligado());
  assert(home_iniciar("deploy/app/art"));

  for (passo = 0; passo < 2; passo++) {
    const int nf = 3;
    memset(itens, 0, sizeof itens);
    memset(fils, 0, sizeof fils);
    ini = 0;
    for (k = 0; k < nf; k++) {
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
        int a = (ini + i + 3) % NA;
        snprintf(c->imdb, sizeof c->imdb, "tt90%05d", ini + i);
        snprintf(c->tipo, sizeof c->tipo, "%s", FILS[k].tipo);
        snprintf(c->titulo, sizeof c->titulo, "%s", NOMES[(ini + i) % NN]);
        snprintf(c->genero, sizeof c->genero, "Filme · Drama");
        snprintf(c->meta, sizeof c->meta, "%d · 2 h 04 min", 2018 + (ini + i) % 8);
        snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", a);
        snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
        snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", a);
        if (k == 0) { c->progresso = 30 + i * 8; c->restanteMin = 80 - i * 10; }
      }
      ini += f->n;
    }
    total = ini;
    // passo 1: o titulo retomado SO tem cartaz (addon sem fundo).
    if (passo == 1) { itens[0].backdrop[0] = 0; itens[0].backdropCatalogo[0] = 0; }
    cat_definir_tudo(itens, total, fils, nf);
    quadros(30, NULL);
    // A saida do player no meio: o mesmo registro que player.c faz.
    home_registrar_retorno(0, 1800.0, 6000.0);
    // E a escolha de app.c (cwRetidoSincronizar): sem relogio, a faixa segura.
    cw_retido_definir(cw_retido_escolher(ajustes_relogio_ligado(), "", home_retomar_imdb()));
    quadros(30, NULL);
    // Desce para a faixa (a primeira fileira abaixo do destaque).
    tecla(SDLK_DOWN); quadros(60, NULL); tecla(SDLK_UP);
    quadros(150, NULL);
    snprintf(bmp, sizeof bmp, "%s-%s.bmp", saida, passo ? "so-cartaz" : "com-fundo");
    quadros(1, bmp);
    printf("[shot] %s\n", bmp);
  }
  return 0;
}
