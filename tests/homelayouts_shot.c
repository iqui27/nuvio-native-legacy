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
#include "artehero.h"
#include "catalogo.h"
#include "colecoes.h"
#include "corviva.h"
#include "ctxmenu.h"
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
#include "socialvis.h"
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
  // NV_TRAILER_CARTAZ: o PLANO DE VIDEO simulado sob a UI, como a TV compoe o
  // framebuffer pre-multiplicado (final = ui + video * (1 - alfa da UI)). O
  // "video" e uma grade colorida de tela cheia: onde ela aparece e o furo.
  if (getenv("NV_TRAILER_CARTAZ")) {
    int x;
    for (y = 0; y < 1080; y++) for (x = 0; x < 1920; x++) {
      unsigned char *q = pix + ((size_t)y * 1920 + x) * 4;
      int ty = 1079 - y, grade = (x % 96) < 3 || (ty % 96) < 3, k;
      float a = q[3] / 255.0f, u = x / 1920.0f, v = ty / 1080.0f;
      float vc[3] = { grade ? 255.0f : 70.0f + 170.0f * u,
                      grade ? 255.0f : 190.0f - 110.0f * v,
                      grade ? 255.0f : 230.0f - 150.0f * u };
      for (k = 0; k < 3; k++) {
        float c = q[k] + vc[k] * (1.0f - a);
        q[k] = (unsigned char)(c > 255.0f ? 255.0f : c + 0.5f);
      }
      q[3] = 255;
    }
  }
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
    // O passo do trailer e do app.c (app_atualizar); aqui so na captura dele.
    if (getenv("NV_TRAILER_CARTAZ")) home_trailer_passo(1, 1.0f / 60.0f, agora);
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

// SEGURAR OK de verdade: KEYDOWN, quadros ate passar NV_HOLD_MS (o relogio e o
// SDL_GetTicks real), KEYUP. E o caminho da home que abre o menu do cartaz.
static char meioBmp[512];   // se preenchido: captura no meio do segurar
static void segurarOk(void) {
  SDL_Event e;
  Uint32 ini = SDL_GetTicks();
  int meioFeito = 0;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  home_evento(&e);
  while (SDL_GetTicks() - ini < NV_HOLD_MS + 150) {
    quadros(1, NULL);
    if (meioBmp[0] && !meioFeito && SDL_GetTicks() - ini > NV_HOLD_MS / 2) {
      meioFeito = 1; quadros(1, meioBmp); meioBmp[0] = 0;
    }
  }
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
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
}

// Ajustes pelo mesmo arquivo que a TV le. Idioma 0 = pt; animacoes NORMAIS
// (as molas assentam em ~90 quadros); trailer desligado (sem rede).
static void ajusta(int layout, int vidro) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(a, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fprintf(a, "idioma 0\ntrailerHero 1\nhomeLayoutLocal %d\nvidroLocal %d\n"
             "modernLandscapePostersEnabled 1\nselected_theme %d\n",
          layout, vidro ? 0 : 1, getenv("NV_TEMA") ? atoi(getenv("NV_TEMA")) : 0);
  if (getenv("NV_AJ")) fprintf(a, "%s\n", getenv("NV_AJ"));   // ex.: "heroSectionEnabled 1"
  fclose(a);
  ajustes_dir(dirDados);
  // O roteamento de app.c (app_atualizar), que este teste nao roda.
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nv-home-layouts-shots/h";
  const char *camadas = argc > 2 ? argv[2] : "012";
  static CatItem itens[200];
  static CatFileira fils[8];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k, total = 0, ini = 0;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-homelayouts-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  (void)argc;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  // O furo do trailer so existe no ALFA do quadro: sem canal alfa ele some.
  if (getenv("NV_TRAILER_CARTAZ")) SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  janela = SDL_CreateWindow("Nuvio: layouts da home", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  // NV_EFEITOS=1 leves, 2 minimos (os niveis de gpunivel.h sem o 720p).
  if (getenv("NV_EFEITOS")) {
    int e = atoi(getenv("NV_EFEITOS"));
    gfx_definir_efeitos_leves(e >= 1);
    gfx_definir_efeitos_minimos(e >= 2);
  }
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_borrao_iniciar(480, 270);
  artehero_definir_falhou(tex_falhou);
  snprintf(cache, sizeof cache, "%s/cache", dirDados);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajusta(0, 0);
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
    // NV_FIL_N=<n>: cada CATALOGO com n itens (o "Itens por fileira" 12/18/24
    // da issue #201); sem ele, o n da tabela.
    { int nk = FILS[k].n;
      if (FILS[k].catalogo && getenv("NV_FIL_N")) nk = atoi(getenv("NV_FIL_N"));
      if (nk > 24) nk = 24;
      f->ini = ini; f->n = nk; }
    for (i = 0; i < f->n; i++) {
      CatItem *c = &itens[ini + i];
      int a = (ini + i) % NA;
      snprintf(c->imdb, sizeof c->imdb, "tt90%05d", ini + i);
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
      // NV_SEM_LOGO=1: nenhum titulo com logo (o cabecalho em texto do menu).
      if (a < 10 && !getenv("NV_SEM_LOGO")) snprintf(c->logo, sizeof c->logo, "deploy/app/art/logo/%02d.png", a);
      // NV_ARTE_ADDON=1: todo item vem de um addon (origem; inclusive o da
      // primeira fileira, que abre o destaque) e tambem traz um fundo "do
      // TMDB" — outra foto, local —, para a captura separar "Background do
      // hero" = TMDB de "Fundo do destaque do addon".
      if (getenv("NV_ARTE_ADDON")) {
        snprintf(c->origem, sizeof c->origem, "%s", "xperience");
        snprintf(c->backdropTmdb, sizeof c->backdropTmdb, "deploy/app/art/%02d.jpg", (a + 17) % NA);
      }
      c->nota = 68 + (ini + i) % 25;
      if (k == 0) { c->progresso = 20 + i * 12; c->restanteMin = 90 - i * 10; }
    }
    ini += f->n;
    total = ini;
  }
  if (getenv("NV_COL")) {   // NV_COL=1: um grupo de colecao no fim, com as quatro cadeias de arte
    // NV_COL_FORMA=POSTER|LANDSCAPE|SQUARE: o tileShape das quatro pastas
    // (ausente = sem o campo, que o web le como quadrado).
    char fonte[400];
    char js[4600];
    snprintf(fonte, sizeof fonte, "%s%s%s\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]",
             getenv("NV_COL_FORMA") ? "\"tileShape\":\"" : "",
             getenv("NV_COL_FORMA") ? getenv("NV_COL_FORMA") : "",
             getenv("NV_COL_FORMA") ? "\"," : "");
    snprintf(js, sizeof js,
      "{\"collections\":[{\"id\":\"cs\",\"title\":\"Streaming\",\"backdropImageUrl\":\"deploy/app/art/07.jpg\",\"folders\":["
      "{\"id\":\"a\",\"title\":\"Com hero e capa\",\"heroBackdropUrl\":\"deploy/app/art/03.jpg\",\"coverImageUrl\":\"deploy/app/art/poster/12.jpg\",%s},"
      "{\"id\":\"b\",\"title\":\"So capa\",\"coverImageUrl\":\"deploy/app/art/05.jpg\",%s},"
      "{\"id\":\"c\",\"title\":\"So fundo da colecao\",%s},"
      "{\"id\":\"d\",\"title\":\"Com hero sem capa\",\"heroBackdropUrl\":\"deploy/app/art/09.jpg\",%s}]}]}",
      fonte, fonte, fonte, fonte);
    // NV_COL_PACOTE=1: o pacote traz as pastas "a" e "b" com arte PROPRIA
    // (caminho absoluto: localiza nao mexe), para a captura de "Arte das
    // pastas da conta". Desligado vence o pacote; ligado, a conta.
    if (getenv("NV_COL_PACOTE")) {
      char cam[800], cwd[500], pk[2400];
      FILE *f;
      assert(getcwd(cwd, sizeof cwd));
      snprintf(pk, sizeof pk,
        "{\"groups\":[{\"id\":\"cs\",\"title\":\"Streaming\",\"folders\":["
        "{\"id\":\"a\",\"title\":\"Com hero e capa\",\"cover\":\"%s/deploy/app/art/poster/30.jpg\",\"hero\":\"%s/deploy/app/art/21.jpg\","
          "\"sources\":[{\"title\":\"M\",\"base\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catId\":\"k\"}]},"
        "{\"id\":\"b\",\"title\":\"So capa\",\"cover\":\"%s/deploy/app/art/25.jpg\","
          "\"sources\":[{\"title\":\"M\",\"base\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catId\":\"k\"}]}]}]}",
        cwd, cwd, cwd);
      snprintf(cam, sizeof cam, "%s/collections.json", dirDados);
      f = fopen(cam, "w"); assert(f); fputs(pk, f); fclose(f);
      assert(col_carregar(dirDados) == 2);
    }
    assert(col_definir_json(js) == 4);
  }
  cat_definir_tudo(itens, total, fils, NF);
  // NV_AMIGOS=1: amigos falsos nos tres primeiros titulos de "Popular" (feed externo).
  if (getenv("NV_AMIGOS")) {
    SvEvento e[6]; int k = 0, i;
    memset(e, 0, sizeof e);
    for (i = 0; i < 6; i++) { e[i].pct = -1; e[i].restanteMin = -1; e[i].quando = 1700000000LL - i;
      snprintf(e[i].imdb, sizeof e[i].imdb, "%s", itens[fils[1].ini + (i < 3 ? 0 : i - 2)].imdb); }
    snprintf(e[k].pessoaId, 96, "nuvio:fabi"); snprintf(e[k].pessoaNome, 64, "Fabi"); e[k].acao = SV_REACAO; e[k++].reacao = SV_REAC_GOSTOU;
    snprintf(e[k].pessoaId, 96, "nuvio:rafa"); snprintf(e[k].pessoaNome, 64, "Rafa"); e[k++].acao = SV_FIM;
    snprintf(e[k].pessoaId, 96, "nuvio:mari"); snprintf(e[k].pessoaNome, 64, "Mari"); e[k++].acao = SV_FIM;
    snprintf(e[k].pessoaId, 96, "nuvio:mari"); snprintf(e[k].pessoaNome, 64, "Mari"); e[k++].acao = SV_FIM;
    snprintf(e[k].pessoaId, 96, "nuvio:rafa"); snprintf(e[k].pessoaNome, 64, "Rafa"); e[k++].acao = SV_FIM;
    snprintf(e[k].pessoaId, 96, "nuvio:leo"); snprintf(e[k].pessoaNome, 64, "Leo"); e[k++].acao = SV_FIM;
    socialvis_definir_feed(e, 6);
  }
  quadros(60, NULL);
  // NV_VISTOS=1 (#212): o selo de visto pelo HISTORICO, sem progresso. Dois
  // filmes de "Popular" pelo leitor de /sync/watched/movies (o corpo do Trakt)
  // e uma serie de "Em alta" pelo historico de titulo — nenhum deles tem
  // progresso, que era a unica coisa que o selo lia.
  if (getenv("NV_VISTOS")) {
    char corpo[600];
    extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);
    snprintf(corpo, sizeof corpo,
             "[{\"plays\":1,\"last_watched_at\":\"2026-01-01T00:00:00.000Z\",\"movie\":{\"title\":\"A\",\"year\":2020,"
             "\"ids\":{\"trakt\":1,\"slug\":\"a\",\"imdb\":\"%s\",\"tmdb\":1}}},"
             "{\"plays\":2,\"movie\":{\"title\":\"B\",\"ids\":{\"trakt\":2,\"imdb\":\"%s\"}}}]",
             itens[fils[1].ini].imdb, itens[fils[1].ini + 2].imdb);
    printf("[shot] filmes vistos lidos: %d\n", trakt_ler_filmes_vistos(corpo));
    cat_historico_definir_id(itens[fils[2].ini + 1].imdb, "series", 1);
    printf("[shot] cat_visto pop0=%d pop1=%d pop2=%d serie1=%d\n",
           cat_visto(&itens[fils[1].ini]), cat_visto(&itens[fils[1].ini + 1]),
           cat_visto(&itens[fils[1].ini + 2]), cat_visto(&itens[fils[2].ini + 1]));
    quadros(10, NULL);
  }
  // NV_TIPOS="chave=tipo,chave=tipo": a forma escolhida em "Estilo da fileira"
  // (o numero FilTipo de fileiras.h), como se a pessoa tivesse escolhido.
  if (getenv("NV_TIPOS")) {
    char buf[400], *p, *sv = NULL;
    snprintf(buf, sizeof buf, "%s", getenv("NV_TIPOS"));
    for (p = strtok_r(buf, ",", &sv); p; p = strtok_r(NULL, ",", &sv)) {
      char *ig = strchr(p, '=');
      if (!ig) continue;
      *ig = 0;
      printf("[shot] tipo %s=%s ok=%d\n", p, ig + 1, fil_definir_tipo(p, atoi(ig + 1)));
    }
    quadros(60, NULL);
  }

  // NV_SO_CTX=1: pula a volta pelos layouts e vai direto ao menu do cartaz
  // (NV_CTX), no layout do primeiro digito de `camadas`.
  if (!getenv("NV_SO_CTX")) { const char *L;
    for (L = camadas; *L; L++) {
      int layout = *L - '0', vidro;
      for (vidro = 0; vidro < 2; vidro++) {
        int r;
        ajusta(layout, vidro);
        assert(ajustes_home_layout() == layout);
        for (r = 0; r < 14; r++) tecla(SDLK_UP);
        quadros(120, NULL);
        snprintf(bmp, sizeof bmp, "%s-L%d-g%d-0-destaque.bmp", saida, layout, vidro);
        quadros(1, bmp);
        printf("[shot] L%d g%d destaque: fill=%.2f vis=%.2f rects=%d\n", layout, vidro, fillUlt, fillVisUlt, rectUlt);
        // NV_MENU_ABRIR=1 (com NV_MENU): a barra aberta sobre o destaque e,
        // descendo ate o fim, a secao de Streaming (NV_COL) rolada.
        if (getenv("NV_MENU_ABRIR") && getenv("NV_MENU")) {
          SDL_Event me;
          int d;
          menu_abrir();
          snprintf(bmp, sizeof bmp, "%s-L%d-g%d-0-menu-aberto.bmp", saida, layout, vidro);
          quadros(60, bmp);
          memset(&me, 0, sizeof me);
          me.type = SDL_KEYDOWN; me.key.keysym.sym = SDLK_DOWN;
          for (d = 0; d < 10; d++) menu_evento(&me);
          snprintf(bmp, sizeof bmp, "%s-L%d-g%d-0-menu-streaming.bmp", saida, layout, vidro);
          quadros(60, bmp);
          for (d = 0; d < 10; d++) menu_evento(&me);
          snprintf(bmp, sizeof bmp, "%s-L%d-g%d-0-menu-fim.bmp", saida, layout, vidro);
          quadros(60, bmp);
          menu_fechar();
          quadros(40, NULL);
        }
        if (getenv("NV_FILL_MODOS")) {   // quem preenche: modo=telas cheias
          int m; printf("[shot]   modos:");
          for (m = 0; m < GFX_NMODOS; m++) if (modoUlt[m] > 0.05) printf(" %d=%.2f", m, modoUlt[m]);
          printf("\n"); }
        // NV_TROCA=1: a troca do destaque pela seta (direita e depois esquerda),
        // fotografada a cada 4 quadros para pegar o MEIO do deslize/esmaecer.
        if (getenv("NV_TROCA") && !vidro) {
          int q, lado;
          for (lado = 0; lado < 2; lado++) {
            tecla(lado ? SDLK_LEFT : SDLK_RIGHT);
            for (q = 0; q < 10; q++) {
              quadros(3, NULL);
              snprintf(bmp, sizeof bmp, "%s-L%d-troca-%c-%02d.bmp", saida, layout,
                       lado ? 'e' : 'd', q);
              quadros(1, bmp);
            }
            quadros(60, NULL);
          }
        }
        // NV_SO_DESTAQUE=1: so o destaque (e o menu, com NV_MENU_ABRIR), sem
        // descer pelas fileiras — a conferencia do menu e do recuo do conteudo.
        if (getenv("NV_SO_DESTAQUE")) continue;
        for (r = 0; r < 9; r++) {
          tecla(SDLK_DOWN);
          quadros(110, NULL);
          snprintf(bmp, sizeof bmp, "%s-L%d-g%d-%d-fileira.bmp", saida, layout, vidro, r + 1);
          quadros(1, bmp);
          printf("[shot] L%d g%d fileira %d: fill=%.2f vis=%.2f rects=%d\n", layout, vidro, r + 1, fillUlt, fillVisUlt, rectUlt);
          if (getenv("NV_FILL_MODOS")) {
            int m; printf("[shot]   modos:");
            for (m = 0; m < GFX_NMODOS; m++) if (modoUlt[m] > 0.02) printf(" %d=%.2f", m, modoUlt[m]);
            printf("\n"); }
          // NV_OK_FIL=<r>: OK na fileira r (abre a pilha do Top 10) antes do
          // passo para o lado.
          if (getenv("NV_OK_FIL") && r == atoi(getenv("NV_OK_FIL"))) {
            tecla(SDLK_RETURN); quadros(60, NULL); }
          // Um passo para o lado: rolagem horizontal + foco. NV_LADO_FIL=<r>
          // escolhe a fileira (0 = a primeira abaixo do destaque).
          if (r == (getenv("NV_LADO_FIL") ? atoi(getenv("NV_LADO_FIL")) : 2)) {
            // NV_LADO=<n>: n passos em vez de dois (o "11", "24" do ranking).
            int d = getenv("NV_LADO") ? atoi(getenv("NV_LADO")) : 2, q;
            for (q = 0; q < d; q++) { tecla(SDLK_RIGHT); quadros(8, NULL); }
            quadros(110, NULL);
            snprintf(bmp, sizeof bmp, "%s-L%d-g%d-%d-fileira-lado.bmp", saida, layout, vidro, r + 1);
            quadros(1, bmp);
          }
        }
        /* MEIO DA ROLAGEM: 12 quadros depois de subir uma fileira. */
        tecla(SDLK_UP);
        quadros(10, NULL);
        snprintf(bmp, sizeof bmp, "%s-L%d-g%d-x-meio-da-rolagem.bmp", saida, layout, vidro);
        quadros(1, bmp);
      }
    } }

  // NV_CTX=<n>: menu do cartaz SEGURANDO OK na fileira n (contada do destaque
  // para baixo), a pagina de estilos, a escolha e a home depois dela.
  if (getenv("NV_CTX")) {
    int r, alvo = atoi(getenv("NV_CTX"));
    // NV_CTX_VIDRO=1: o menu sobre a Interface de vidro (o padrao e o solido).
    ajusta(camadas[0] - '0', getenv("NV_CTX_VIDRO") ? 1 : 0);
    for (r = 0; r < 14; r++) tecla(SDLK_UP);
    quadros(60, NULL);
    for (r = 0; r < alvo; r++) { tecla(SDLK_DOWN); quadros(40, NULL); }
    // NV_CTX_DIR=<n>: n cartoes a direita antes de segurar (um com logo).
    { int d = getenv("NV_CTX_DIR") ? atoi(getenv("NV_CTX_DIR")) : 0;
      for (r = 0; r < d; r++) { tecla(SDLK_RIGHT); quadros(8, NULL); } }
    quadros(80, NULL);
    snprintf(bmp, sizeof bmp, "%s-ctx-0-antes.bmp", saida); quadros(1, bmp);
    segurarOk();
    // NV_CTX_FOCO=<n>: o foco n linhas abaixo da primeira (o mockup acende a
    // segunda).
    { int d = getenv("NV_CTX_FOCO") ? atoi(getenv("NV_CTX_FOCO")) : 0;
      for (r = 0; r < d; r++) teclaCtx(SDLK_DOWN); }
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-ctx-1-menu.bmp", saida); quadros(1, bmp);
    printf("[shot] ctx aberto=%d\n", ctx_aberto());
    // NV_CTX_CONF=<n>: desce n linhas ate "Tirar de Continuar assistindo", OK
    // abre a confirmacao (capturada), e Voltar cancela de volta ao menu.
    if (getenv("NV_CTX_CONF")) {
      int d = atoi(getenv("NV_CTX_CONF"));
      for (r = 0; r < d; r++) teclaCtx(SDLK_DOWN);
      teclaCtx(SDLK_RETURN);
      quadros(60, NULL);
      snprintf(bmp, sizeof bmp, "%s-ctx-1b-confirma.bmp", saida); quadros(1, bmp);
      printf("[shot] confirmacao: menu aberto=%d\n", ctx_aberto());
      teclaCtx(SDLK_ESCAPE);
      quadros(20, NULL);
    }
    // Ja na pagina de estilos (colecao) o OK escolhe; no titulo, desce ate
    // "Estilo da fileira" (a ultima) e entra.
    if (!getenv("NV_CTX_COL")) {
      for (r = 0; r < 8; r++) teclaCtx(SDLK_DOWN);
      teclaCtx(SDLK_RETURN);
      quadros(40, NULL);
      snprintf(bmp, sizeof bmp, "%s-ctx-2-estilos.bmp", saida); quadros(1, bmp);
    }
    // Uma forma abaixo da atual e OK (NV_CTX_DESCE=<n>: n formas abaixo).
    { int d = getenv("NV_CTX_DESCE") ? atoi(getenv("NV_CTX_DESCE")) : 1;
      for (r = 0; r < d; r++) teclaCtx(SDLK_DOWN); }
    // NV_CTX_LADO=<n>: n vezes a direita na linha (o tamanho P M G).
    { int d = getenv("NV_CTX_LADO") ? atoi(getenv("NV_CTX_LADO")) : 0;
      for (r = 0; r < d; r++) teclaCtx(SDLK_RIGHT); }
    quadros(20, NULL);
    snprintf(bmp, sizeof bmp, "%s-ctx-3-escolha.bmp", saida); quadros(1, bmp);
    teclaCtx(SDLK_RETURN);
    quadros(120, NULL);
    printf("[shot] ctx depois da escolha aberto=%d\n", ctx_aberto());
    snprintf(bmp, sizeof bmp, "%s-ctx-4-depois.bmp", saida); quadros(1, bmp);
  }

  // NV_CTX_EXP=<n>: o menu do cartaz com a EXPANSAO do cartaz ligada (atraso de
  // 4 s). Captura: foco antes do menu, menu logo ao abrir, menu depois de o
  // atraso passar, logo apos fechar e fechado com o atraso cumprido.
  // NV_CTX_DIR=<n> cartoes a direita. Quadros no relogio real (SDL_GetTicks).
  if (getenv("NV_CTX_EXP")) {
    int r, alvo = atoi(getenv("NV_CTX_EXP"));
    Uint32 t0;
    setenv("NV_AJ", "focusedPosterBackdropExpandEnabled 1\nfocusedPosterBackdropExpandDelaySeconds 4", 1);
    ajusta(camadas[0] - '0', 0);
    for (r = 0; r < 14; r++) tecla(SDLK_UP);
    quadros(60, NULL);
    for (r = 0; r < alvo; r++) { tecla(SDLK_DOWN); quadros(40, NULL); }
    { int d = getenv("NV_CTX_DIR") ? atoi(getenv("NV_CTX_DIR")) : 0;
      for (r = 0; r < d; r++) { tecla(SDLK_RIGHT); quadros(8, NULL); } }
    quadros(30, NULL);
    snprintf(bmp, sizeof bmp, "%s-exp-a-foco.bmp", saida); quadros(1, bmp);
    snprintf(meioBmp, sizeof meioBmp, "%s-exp-a2-meio-hold.bmp", saida);
    segurarOk();
    quadros(30, NULL);
    snprintf(bmp, sizeof bmp, "%s-exp-b-menu-logo.bmp", saida); quadros(1, bmp);
    printf("[shot] exp aberto=%d\n", ctx_aberto());
    t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 6500u) quadros(1, NULL);
    snprintf(bmp, sizeof bmp, "%s-exp-c-menu-apos-atraso.bmp", saida); quadros(1, bmp);
    teclaCtx(SDLK_ESCAPE); teclaCtx(SDLK_BACKSPACE);
    quadros(20, NULL);
    printf("[shot] exp fechado=%d\n", !ctx_aberto());
    snprintf(bmp, sizeof bmp, "%s-exp-d-fechado.bmp", saida); quadros(1, bmp);
    t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 6500u) quadros(1, NULL);
    snprintf(bmp, sizeof bmp, "%s-exp-e-fechado-expandido.bmp", saida); quadros(1, bmp);
  }

  // NV_VISTO_CTX=<n> (#212): segurar OK no PRIMEIRO cartaz da fileira n (um
  // passo a direita: o segundo), descer NV_VISTO_DESCE linhas ate "Marcar como
  // assistido" e OK. Sem Trakt o espelho e na hora: a home depois tem de
  // mostrar o selo naquele cartaz.
  if (getenv("NV_VISTO_CTX")) {
    int r, alvo = atoi(getenv("NV_VISTO_CTX"));
    int d = getenv("NV_VISTO_DESCE") ? atoi(getenv("NV_VISTO_DESCE")) : 1;
    ajusta(camadas[0] - '0', 0);
    for (r = 0; r < 14; r++) tecla(SDLK_UP);
    quadros(60, NULL);
    for (r = 0; r < alvo; r++) { tecla(SDLK_DOWN); quadros(40, NULL); }
    tecla(SDLK_RIGHT); quadros(80, NULL);
    snprintf(bmp, sizeof bmp, "%s-visto-0-antes.bmp", saida); quadros(1, bmp);
    segurarOk();
    for (r = 0; r < d; r++) teclaCtx(SDLK_DOWN);
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-visto-1-menu.bmp", saida); quadros(1, bmp);
    teclaCtx(SDLK_RETURN);
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-visto-2-confirmado.bmp", saida); quadros(1, bmp);
    if (ctx_aberto()) { teclaCtx(SDLK_ESCAPE); teclaCtx(SDLK_BACKSPACE); }
    quadros(90, NULL);
    printf("[shot] visto ctx: menu aberto=%d\n", ctx_aberto());
    snprintf(bmp, sizeof bmp, "%s-visto-3-home.bmp", saida); quadros(1, bmp);
  }

#ifdef NV_SHOT_HOOKS
  // NV_TRAILER_CARTAZ=1 (com NV_CFLAGS=-DNV_SHOT_HOOKS e NV_SO_CTX=1): o
  // "Trailer do cartaz em foco" nos tres modos, mais o cartaz em pe com Expandir
  // ligado, na Moderna. O trailer e fingido (home_shot_trailer) e o video e a
  // grade composta em gravar(): o que aparece dela e o furo.
  if (getenv("NV_TRAILER_CARTAZ")) {
    static const struct { const char *nome, *aj; const char *tipo; int desce, lado; } C[] = {
      { "tc-1-desligado",        "trailerCartazModo 0", NULL, 3, 2 },
      { "tc-2-tela-cheia",       "trailerCartazModo 1", NULL, 3, 2 },
      { "tc-2b-tela-cheia-sem-logo", "trailerCartazModo 1\nlogoTrailerLocal 0", NULL, 3, 2 },
      { "tc-3-no-card-deitado",  "trailerCartazModo 2", "2", 3, 2 },
      { "tc-3b-no-card-4x3",     "trailerCartazModo 2", "6", 3, 1 },
      // V_LIGA: 0 = Ligado. Expandir vem ligado de fabrica.
      { "tc-3c-no-card-retrato-sem-expandir", "trailerCartazModo 2\nfocusedPosterBackdropExpandEnabled 1", NULL, 3, 2 },
      { "tc-4-no-card-retrato-expandido",
        "trailerCartazModo 2\nfocusedPosterBackdropExpandEnabled 0\nfocusedPosterBackdropExpandDelaySeconds 2",
        NULL, 3, 2 },
    };
    int q, r;
    home_shot_trailer(1);
    for (q = 0; q < (int)(sizeof C / sizeof *C); q++) {
      Uint32 t0;
      setenv("NV_AJ", C[q].aj, 1);
      ajusta(0, 0);
      fil_definir_tipo("pop_movie", C[q].tipo ? atoi(C[q].tipo) : 0);
      for (r = 0; r < 14; r++) tecla(SDLK_UP);
      quadros(60, NULL);
      for (r = 0; r < C[q].desce; r++) { tecla(SDLK_DOWN); quadros(30, NULL); }
      for (r = 0; r < 12; r++) { tecla(SDLK_LEFT); quadros(2, NULL); }
      for (r = 0; r < C[q].lado; r++) { tecla(SDLK_RIGHT); quadros(8, NULL); }
      t0 = SDL_GetTicks();
      while (SDL_GetTicks() - t0 < 4500u) quadros(1, NULL);
      snprintf(bmp, sizeof bmp, "%s-%s.bmp", saida, C[q].nome);
      quadros(1, bmp);
      printf("[shot] %s: trailer cartaz modo %d\n", C[q].nome, ajustes_trailer_cartaz());
    }
    home_shot_trailer(0);
    fil_definir_tipo("pop_movie", 0);
  }
#endif

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
