// CAPTURA DO CARROSSEL DA DINAMICA (detail.c), SEM REDE: abrir um titulo da
// fileira, andar dois titulos, descer para a pagina, voltar ao cartao e voltar
// a fileira. Base: tests/homelayouts_shot.c (mesmas artes e fileiras).
//
//   bash tests/carrossel_shot.sh /tmp/nv-carrossel
//
// NV_REDUZ=1 liga Animacoes reduzidas (tudo vira corte).
//
// (cabecalho original abaixo)
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
#include "agenda.h"
#include "artehero.h"
#include "catalogo.h"
#include "addons.h"
#include "colecoes.h"
#include "corviva.h"
#include "ctxmenu.h"
#include "dados.h"
#include "fileiras.h"
#include "gfx.h"
#include "home.h"
#include "detail.h"
#include "anim.h"
#include "layout.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include "posterprov.h"
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/detail.c" // Capture exact background endpoints without changing the public app API.

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
static float fundoShotCartao = -1.0f;

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
    anim_politica_reduzida = ajustes_animacoes_reduzidas();
    home_atualizar(1.0f / 60.0f, agora);
    if (!detail_aberto() && home_pediu_abrir()) {
      HomeItem it;
      if (home_item_focado(&it)) detail_abrir(&it);
    }
    detail_atualizar(1.0f / 60.0f, agora);
    if (fundoShotCartao >= 0.0f) {
      cartao = fundoShotCartao; cartaoVel = 0;
      pg = getenv("NV_CAR_PG") ? atof(getenv("NV_CAR_PG")) : 0; t = 1;
    }
    corviva_quadro(1.0f / 60.0f, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    gfx_ambiente_preparar();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_ambiente(1.0f);
    if (!detail_cobre_tela()) home_desenhar(agora);
    if (fundoShotCartao >= 0.0f) detalheFundo(1.0f);
    else detail_desenhar(agora);
    ctx_atualizar(1.0f / 60.0f, agora);
    ctx_desenhar(agora);
    if (i == n - 1) { fillUlt = gfx_fill; fillVisUlt = gfx_fill_vis; rectUlt = gfx_n_rect;
                      memcpy(modoUlt, gfx_fill_modo, sizeof modoUlt); }
    if (bmp && i == n - 1) gravar(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(8);
  }
}

// SEGURAR OK de verdade: KEYDOWN, quadros ate passar NV_HOLD_MS (o relogio e o
// SDL_GetTicks real), KEYUP. E o caminho da home que abre o menu do cartaz.
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
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  if (detail_aberto()) detail_evento(&e); else home_evento(&e);
  quadros(1, NULL);
  e.type = SDL_KEYUP;
  if (detail_aberto()) detail_evento(&e); else home_evento(&e);
}

// Ajustes pelo mesmo arquivo que a TV le. Idioma 0 = pt; animacoes NORMAIS
// (as molas assentam em ~90 quadros); trailer desligado (sem rede).
static void ajusta(int layout, int vidro) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma 0\ntrailerHero 0\nhomeLayoutLocal %d\nvidroLocal %d\n"
             "modernLandscapePostersEnabled 1\nselected_theme %d\n",
          layout, vidro ? 0 : 1, getenv("NV_TEMA") ? atoi(getenv("NV_TEMA")) : 0);
  if (getenv("NV_AJ")) fprintf(a, "%s\n", getenv("NV_AJ"));   // ex.: "heroSectionEnabled 1"
  if (getenv("NV_REDUZ")) fprintf(a, "animacoes 1\n");
  fclose(a);
  ajustes_dir(dirDados);
  // O roteamento de app.c (app_atualizar), que este teste nao roda.
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-carrossel/c");
  const char *camadas = argc > 2 ? argv[2] : "012";
  static CatItem itens[200];
  static CatFileira fils[8];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k, total = 0, ini = 0;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-carrossel-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  (void)argc;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
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
  // NUVIO_SHOT_VIDRO=1: Interface de vidro ligada (o padrao desta captura e o solido).
  ajusta(2, getenv("NUVIO_SHOT_VIDRO") && !strcmp(getenv("NUVIO_SHOT_VIDRO"), "1"));
  assert(home_iniciar("deploy/app/art"));

  // A Home exige um addon ligado mesmo quando o catalogo e um fixture local
  // (homelayouts_shot.c): sem ele o homevazio toma a tela e as capturas saem
  // todas com o QR de "Sua Home está vazia".
  { AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
    assert(addons_definir_lista(&addon, 1)); }
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
      if (a < 10) snprintf(c->logo, sizeof c->logo, "deploy/app/art/logo/%02d.png", a);
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
      "{\"collections\":[{\"id\":\"cs\",\"title\":\"%s\",\"backdropImageUrl\":\"deploy/app/art/07.jpg\",\"folders\":["
      "{\"id\":\"a\",\"title\":\"Com hero e capa\",\"heroBackdropUrl\":\"deploy/app/art/03.jpg\",\"coverImageUrl\":\"deploy/app/art/poster/12.jpg\",%s},"
      "{\"id\":\"b\",\"title\":\"So capa\",\"coverImageUrl\":\"deploy/app/art/05.jpg\",%s},"
      "{\"id\":\"c\",\"title\":\"So fundo da colecao\",%s},"
      "{\"id\":\"d\",\"title\":\"Com hero sem capa\",\"heroBackdropUrl\":\"deploy/app/art/09.jpg\",%s}]}]}",
      getenv("NV_COL_ORDEM") ? "Coleção" : "Streaming",
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
  if (getenv("NV_AGENDA")) {
    agenda_iniciar(); agenda_definir_hoje("2026-10-03");
    for (int j = 0; j < total; j++) if (!strcmp(itens[j].tipo, "series"))
      agenda_registrar(itens[j].imdb, itens[j].titulo, itens[j].poster,
                        "Returning Series", 2, 5, "Episódio de ensaio",
                        "2026-10-07", "2026-09-30");
  }
  cat_definir_tudo(itens, total, fils, NF);
  if (getenv("NV_EP_LIST")) for (int j=0;j<total;j++) if (!strcmp(itens[j].tipo,"series")) {
    CatEp eps[6] = {0};
    for (int e=0;e<6;e++) {
      eps[e].temporada=1; eps[e].episodio=e+1;
      snprintf(eps[e].nome,sizeof eps[e].nome,"Episódio de ensaio %d",e+1);
      snprintf(eps[e].thumb,sizeof eps[e].thumb,"deploy/app/art/%02d.jpg",e%NA);
      snprintf(eps[e].sinopse,sizeof eps[e].sinopse,"Uma sinopse de ensaio para conferir a leitura, o foco e a separação entre a imagem e as informações do episódio.");
      snprintf(eps[e].duracao,sizeof eps[e].duracao,"50 min");
      snprintf(eps[e].data,sizeof eps[e].data,"01/04/2022");
    }
    cat_definir_episodios(j,eps,6);
  }
  quadros(90, NULL);
  {
    int q, r, n = 0;
    assert(ajustes_home_layout() == HOME_LAYOUT_DINAMICA);
    for (r = 0; r < 14; r++) tecla(SDLK_UP);
    quadros(60, NULL);
    // Desce ate a fileira NV_FIL (padrao 2: "Em alta - Serie") e anda um card.
    { int alvo = getenv("NV_FIL") ? atoi(getenv("NV_FIL")) : 3;
      for (r = 0; r < alvo; r++) { tecla(SDLK_DOWN); quadros(40, NULL); } }
    tecla(SDLK_RIGHT); quadros(90, NULL);
#define FOTO(nome) do { snprintf(bmp, sizeof bmp, "%s-%02d-%s.bmp", saida, n++, nome); quadros(1, bmp); \
    printf("[shot] %s: fill=%.2f vis=%.2f rects=%d\n", nome, fillUlt, fillVisUlt, rectUlt); } while (0)
    FOTO("home");
    // NV_CTX=1: so o MENU DO CARTAZ (segurar OK), a segunda opcao em foco e a
    // folha de estilo da fileira — as ilhas modais do Glass UI — e para.
    if (getenv("NV_CTX")) {
      int alvo = getenv("NV_FIL") ? atoi(getenv("NV_FIL")) : 3;
      segurarOk();
      quadros(60, NULL); FOTO("ctx");
      teclaCtx(SDLK_DOWN);
      quadros(40, NULL); FOTO("ctx-foco2");
      teclaCtx(SDLK_ESCAPE);
      quadros(40, NULL);
      ctx_abrir_fileira(fils[alvo].chave, fils[alvo].titulo);
      quadros(60, NULL); FOTO("estilos");
      goto fim;
    }
    // NV_COL_ORDEM=1 (com NV_COL=1): a fileira de colecao em foco, segurando
    // OK abre o modal de estilo dela — e a linha "Ordem" das pastas (2.0.5)
    // em foco, com o ciclo de uma troca e o OK que aplica.
    if (getenv("NV_COL_ORDEM")) {
      for (r = 0; r < 14; r++) { tecla(SDLK_DOWN); quadros(6, NULL); }
      FOTO("colecao-foco");
      segurarOk();
      quadros(60, NULL); FOTO("colecao-estilo");
      teclaCtx(SDLK_DOWN); teclaCtx(SDLK_DOWN); teclaCtx(SDLK_DOWN); teclaCtx(SDLK_DOWN);
      quadros(50, NULL); FOTO("colecao-ordem");
      teclaCtx(SDLK_RIGHT);
      quadros(40, NULL); FOTO("colecao-ordem-ano");
      teclaCtx(SDLK_RIGHT);   // Alfabética: as pastas do fixture (Com hero e capa,
      teclaCtx(SDLK_RETURN); // So capa...) trocam de lugar na fileira ao aplicar
      quadros(50, NULL); FOTO("colecao-aplicada");
      goto fim;
    }
    // ABRIR: OK (keydown + keyup na home), quadros a cada 3.
    tecla(SDLK_RETURN);
    for (q = 0; q < 8; q++) { quadros(2, NULL); FOTO("abrindo"); }
    quadros(90, NULL); FOTO("cartao");
    if (getenv("NV_CAR_LIMIAR")) {
      static const float passos[] = {1.0f, .5f, .1f, .02f, .0021f, .0019f, 0.0f};
      carOff = (float)carAplicado; carCheia = 1;
      if (getenv("NV_CAR_SEM_ARTE")) {
        CatItem *c = (CatItem *)cat_item(idx);
        c->backdrop[0] = c->poster[0] = c->imdb[0] = arteFixa[0] = 0;
        // O fixture remove a identidade junto com a arte: nao deixar o
        // reconciliador reencontrar uma copia do titulo com imagem no catalogo.
        idxImdb[0] = 0;
        arteFixaPoster = 0;
      } else if (getenv("NV_CAR_POSTER")) {
        const CatItem *c = cat_item(idx);
        snprintf(arteFixa, sizeof arteFixa, "%s", c->poster);
        arteFixaPoster = 1;
      }
      for (size_t j=0; j<sizeof passos/sizeof passos[0]; j++) {
        fundoShotCartao = passos[j];
        snprintf(bmp, sizeof bmp, "%s-limiar-%zu.bmp", saida, j);
        quadros(1, bmp);
        printf("[carousel-background] card=%.5f branch=%s fill=%.3f rects=%d\n",
               cartao, carDesenhaFundo() ? "window" : "detail", fillUlt, rectUlt);
      }
      goto fim;
    }
    // ANDAR: direita ate a ponta dos botoes e mais uma = proximo titulo.
    for (r = 0; r < 8; r++) { tecla(SDLK_RIGHT); quadros(2, NULL); }
    for (q = 0; q < 8; q++) { quadros(3, NULL); FOTO("andando1"); }
    quadros(90, NULL); FOTO("titulo2");
    tecla(SDLK_RIGHT);
    for (q = 0; q < 5; q++) { quadros(4, NULL); FOTO("andando2"); }
    quadros(90, NULL); FOTO("titulo3");
    // TELA CHEIA: a primeira seta para baixo so estica o cartao (o texto
    // vai para a margem); a segunda desce para a pagina.
    tecla(SDLK_DOWN);
    for (q = 0; q < 6; q++) { quadros(3, NULL); FOTO("esticando"); }
    quadros(90, NULL); FOTO("tela-cheia");
    tecla(SDLK_DOWN);
    for (q = 0; q < 3; q++) { quadros(4, NULL); FOTO("descendo"); }
    quadros(90, NULL); FOTO("pagina");
    if (getenv("NV_EP_LIST")) { tecla(SDLK_DOWN); quadros(90,NULL); FOTO("episodios"); tecla(SDLK_UP); quadros(90,NULL); }
    // VOLTAR: pagina -> tela cheia no topo -> cartao -> fileira.
    tecla(SDLK_AC_BACK);
    quadros(90, NULL); FOTO("tela-cheia-de-novo");
    tecla(SDLK_AC_BACK);
    for (q = 0; q < 3; q++) { quadros(5, NULL); FOTO("voltando-cartao"); }
    quadros(90, NULL); FOTO("cartao-de-novo");
    tecla(SDLK_AC_BACK);
    for (q = 0; q < 6; q++) { quadros(3, NULL); FOTO("fechando"); }
    quadros(90, NULL); FOTO("fileira");
    for (int back=0; back<4 && detail_aberto(); back++) { tecla(SDLK_AC_BACK); quadros(90,NULL); }
    printf("[shot] detalhe aberto no fim: %d\n", detail_aberto());
  }
fim:

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
