// ORGANIZAR OS SALVOS (dono, 01/10/2026): categorias da pessoa, ordenar,
// agrupar e estilo de exibicao no painel da tecla AZUL, e a aba Social por
// pessoa. Capturas PNG de cada estilo, de cada agrupamento e do fluxo inteiro
// de categoria (criar pelo teclado do app, mover segurando OK, renomear,
// excluir) — para serem OLHADAS — e as conferencias do que da para medir:
//
//   1. a ordem e o agrupamento PADRAO sao os de sempre (a lista de quem
//      atualiza nao muda de lugar);
//   2. cada escolha da barra muda a lista e fica gravada no arquivo do perfil,
//      versionado; salvos.txt continua no formato v1, intocado;
//   3. criar categoria pelo teclado, mover segurando OK, renomear e excluir
//      (os titulos continuam salvos);
//   4. Social por pessoa agrupa as recomendacoes de cada um;
//   5. navegar a grade parado nao reconstroi a lista.
//
//   bash tests/salvosorg_shot.sh [pasta-das-capturas]
#define NV_REC_URL "http://127.0.0.1:8799"
#include "../src/recomenda.c"
#include "ajustes.h"
#include "catalogo.h"
#include "ctxmenu.h"
#include "dados.h"
#include "gfx.h"
#include "vidro_fundo.h"
#include "home.h"
#include "layout.h"
#include "salvos.h"
#include "salvosorg.h"
#include "salvospainel.h"
#include "teclado.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-66s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

static const char *saida;
static SDL_Window *win;

static void rotear(const SDL_Event *e) {
  if (spainel_aberto()) {
    if (ctx_aberto()) ctx_evento(e); else spainel_evento(e);
    return;
  }
  if (ctx_aberto()) ctx_evento(e);
}
static void homeFundo(void *ctx) { (void)ctx; home_desenhar(SDL_GetTicks()); }
static void quadro(void) {
  SDL_Event e;
  Uint32 agora = SDL_GetTicks();
  while (SDL_PollEvent(&e)) rotear(&e);
  tex_bombear(4);
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  txt_novo_quadro();
  home_atualizar(1.0f / 60.0f, agora);
  ctx_atualizar(1.0f / 60.0f, agora);
  spainel_atualizar(1.0f / 60.0f, agora);
  spainel_fundo(!ctx_aberto() || ctx_do_painel(), cat_revisao(), homeFundo, NULL);
  vidroFundoDesenhar();   // so com NUVIO_SHOT_VIDRO_*: arte de verdade atras do vidro
  if (spainel_visivel()) { spainel_desenhar(agora); ctx_desenhar(agora); }
  else ctx_desenhar(agora);
}
static void quadros(int n) { int i; for (i = 0; i < n; i++) { quadro(); SDL_GL_SwapWindow(win); } }
static void durante(Uint32 ms) {
  Uint32 t0 = SDL_GetTicks();
  while (SDL_GetTicks() - t0 < ms) { quadro(); SDL_GL_SwapWindow(win); SDL_Delay(8); }
}
static void empurrar(Uint32 tipo, SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = tipo;
  e.key.keysym.sym = k;
  e.key.state = tipo == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  SDL_PushEvent(&e);
}
static void toque(SDL_Keycode k) {
  empurrar(SDL_KEYDOWN, k); quadros(1);
  empurrar(SDL_KEYUP, k);   quadros(1);
}
static void toques(SDL_Keycode k, int n) { while (n-- > 0) toque(k); }

static void captura(const char *nome) {
  char caminho[700];
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(caminho, sizeof caminho, "%s/%s", saida, nome);
  assert(IMG_SavePNG(s, caminho) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("  captura: %s\n", caminho);
}
// Desenha ate assentar (arte subindo, molas paradas) e captura o ultimo quadro.
static void foto(const char *nome) {
  int i;
  for (i = 0; i < 70; i++) {
    quadro();
    if (i == 69) captura(nome);
    SDL_GL_SwapWindow(win);
    if (i < 20) SDL_Delay(6);
  }
}

// O TECLADO DO APP, pelo D-pad: a grade e "a-z0-9 -" em 6 colunas, e a ultima
// fileira e apagar / limpar / pronto. A posicao e acompanhada aqui.
static const char *ALFA = "abcdefghijklmnopqrstuvwxyz0123456789 -";
static int tf, tc;
// Sempre pela primeira fileira: a de "espaco e hifen" tem duas colunas, e o
// teclado prende a coluna nela — andar na coluna por la desencontraria a conta.
static void tecladoIr(int f, int c) {
  while (tf > 0) { toque(SDLK_UP); tf--; }
  while (tc < c) { toque(SDLK_RIGHT); tc++; }
  while (tc > c) { toque(SDLK_LEFT); tc--; }
  while (tf < f) { toque(SDLK_DOWN); tf++; }
}
static void digitar(const char *s) {
  for (; *s; s++) {
    int i = (int)(strchr(ALFA, *s) - ALFA);
    tecladoIr(i / 6, i % 6);
    toque(SDLK_RETURN);
  }
}
// A fileira de acoes: entrar nela guarda a coluna, sair dela a devolve — por
// isso o caminho e sempre "da coluna 0 da primeira fileira".
static void tecladoAcao(int c) {
  tecladoIr(0, 0);
  toques(SDLK_DOWN, 7); tf = 7; tc = 0;
  toques(SDLK_RIGHT, c); tc = c;
  toque(SDLK_RETURN);
}
static void tecladoAbriu(void) { tf = 0; tc = 0; }
// ESQUERDA alem da primeira coluna entra na coluna do campo (o modo do
// celular): encosta a esquerda e, se entrou, DIREITA volta a tecla de onde saiu.
static void tecladoVoltarTopo(void) {
  toques(SDLK_UP, 7);
  toques(SDLK_LEFT, 6);
  if (teclado_foco_campo()) toque(SDLK_RIGHT);
  tf = 0; tc = 0;
}

// Segura OK na linha focada ate o menu do cartaz abrir e solta.
static void segurarOk(void) {
  empurrar(SDL_KEYDOWN, SDLK_RETURN);
  durante(NV_HOLD_MS + 150);
  empurrar(SDL_KEYUP, SDLK_RETURN);
  quadros(4);
}

#define NCAT 12
static CatItem catItens[NCAT];
static CatFileira fil;

static void semear(const char *dir) {
  char caminho[700];
  long long agora = (long long)time(NULL);
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/recomendacoes.txt", dir);
  f = fopen(caminho, "wb");
  assert(f);
  fprintf(f, "# nuvio recomendacoes v2\n");
  fprintf(f, "7\t%lld\t0\t2\tmovie\t1994\ttrakt:gustavo\tGustavo\ttt0111161\t"
             "deploy/app/art/00.jpg\t\t93\tdeploy/app/art/elenco/00_0.jpg\t"
             "Um Sonho de Liberdade\n", agora - 900);
  fprintf(f, "6\t%lld\t0\t-1\tseries\t2008\tnuvio:9a1c\tMarina\ttt0903747\t"
             "deploy/app/art/01.jpg\tisso e melhor que tudo\t95\t\t"
             "Breaking Bad\n", agora - 9000);
  fprintf(f, "5\t%lld\t1\t4\tmovie\t2014\ttrakt:gustavo\tGustavo\ttt2582802\t"
             "deploy/app/art/02.jpg\t\t85\tdeploy/app/art/elenco/00_0.jpg\t"
             "Whiplash: Em Busca da Perfeição\n", agora - 200000);
  fprintf(f, "4\t%lld\t1\t0\tseries\t2016\tnuvio:3b2d\tCarolina Menezes\t"
             "tt4574334\tdeploy/app/art/03.jpg\t\tStranger Things\n", agora - 400000);
  fclose(f);
  // A LISTA LOCAL, na ordem em que foi salva (a mais antiga primeiro), com
  // "quando" de verdade para a ordem "Recentes".
  snprintf(caminho, sizeof caminho, "%s/salvos.txt", dir);
  f = fopen(caminho, "wb");
  assert(f);
  fprintf(f, "# nuvio salvos v1\n");
  fprintf(f, "tt0110912\tmovie\t%lld\t89\t1994\tdeploy/app/art/04.jpg\tPulp Fiction\n", agora - 900000);
  fprintf(f, "tt0068646\tmovie\t%lld\t92\t1972\tdeploy/app/art/05.jpg\tO Poderoso Chefão\n", agora - 800000);
  fprintf(f, "tt0944947\tseries\t%lld\t92\t2011 · 8 temporadas\tdeploy/app/art/06.jpg\tGame of Thrones\n", agora - 700000);
  fprintf(f, "tt1375666\tmovie\t%lld\t88\t2010\tdeploy/app/art/07.jpg\tA Origem\n", agora - 600000);
  fprintf(f, "tt0245429\tmovie\t%lld\t86\t2001\tdeploy/app/art/08.jpg\tA Viagem de Chihiro\n", agora - 500000);
  fprintf(f, "tt0386676\tseries\t%lld\t90\t2005 · 9 temporadas\tdeploy/app/art/09.jpg\tThe Office\n", agora - 400000);
  fprintf(f, "tt6751668\tmovie\t%lld\t85\t2019\tdeploy/app/art/10.jpg\tParasita\n", agora - 300000);
  fprintf(f, "tt2380307\tmovie\t%lld\t84\t2017\tdeploy/app/art/11.jpg\tViva: A Vida É uma Festa\n", agora - 200000);
  fprintf(f, "tt5180504\tseries\t%lld\t80\t2019 · 3 temporadas\tdeploy/app/art/12.jpg\tThe Witcher\n", agora - 100000);
  fclose(f);
}

// O catalogo: copias com progresso de dois salvos, um canal e uma colecao que
// so o catalogo tem (marcados naLista, como a watchlist/conta marcariam).
static void montarCatalogo(void) {
  int i, n = 0;
  memset(catItens, 0, sizeof catItens);
#define IT(id_, tipo_, tit_, art_, meta_, prog_, rest_, t_, e_) do { CatItem *c = &catItens[n++]; \
    snprintf(c->imdb, sizeof c->imdb, "%s", id_); snprintf(c->tipo, sizeof c->tipo, "%s", tipo_); \
    snprintf(c->titulo, sizeof c->titulo, "%s", tit_); \
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/%02d.jpg", art_); \
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", (art_ + 13) % 40); \
    snprintf(c->meta, sizeof c->meta, "%s", meta_); c->progresso = prog_; c->restanteMin = rest_; \
    c->temporada = t_; c->episodio = e_; } while (0)
  IT("tt0110912", "movie", "Pulp Fiction", 4, "1994", 35, 65, 0, 0);
  IT("tt0944947:2:3", "series", "Game of Thrones", 6, "2011 · 8 temporadas", 60, 22, 2, 3);
  IT("cs:channel:futebol", "tv", "Canal Futebol Ao Vivo", 14, "", 0, 0, 0, 0);
  IT("ctmdb:10", "collect", "Coleção O Senhor dos Anéis", 15, "2001", 0, 0, 0, 0);
  IT("tt1375666", "movie", "A Origem", 7, "2010", 0, 0, 0, 0);
#undef IT
  snprintf(fil.chave, sizeof fil.chave, "teste_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Em alta");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = n;
  cat_definir_tudo(catItens, n, &fil, 1);
  for (i = 0; i < cat_n(); i++)
    if (!strncmp(cat_item(i)->imdb, "cs:", 3) || !strncmp(cat_item(i)->imdb, "ctmdb", 5))
      cat_definir_na_lista(i, 1);
  salvos_reconciliar();
}

// O titulo que o toque curto abriria agora: o da linha focada.
static char focado[32], movido[32];
static const char *tituloFocado(void) {
  const char *p;
  empurrar(SDL_KEYDOWN, SDLK_RETURN); quadros(1);
  empurrar(SDL_KEYUP, SDLK_RETURN); quadros(2);
  p = spainel_pediu_abrir();
  snprintf(focado, sizeof focado, "%s", p ? p : "");
  spainel_abrir();
  durante(300);
  return focado;
}

static int arquivoTem(const char *nome, const char *agulha) {
  char *b = dados_ler(nome);
  int r = b && strstr(b, agulha) != NULL;
  free(b);
  return r;
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  GLuint fbo, fboTex;
  int kids, r0;
  saida = argc > 1 ? argv[1] : "/tmp";
  if (!dir || !dir[0]) { printf("NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("dados_dir() != NUVIO_DADOS; recusando\n"); return 2; }
  semear(dir);
  { char c[700]; FILE *f;
    snprintf(c, sizeof c, "%s/ajustes.txt", dir);
    f = fopen(c, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\n");   // portugues, acento oceano
    fclose(f); }
  ajustes_dir(dir);
  ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_OPAC / _FOSCO

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: organizar Salvos", 0, 0, 64, 64,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_icones_dir("deploy/app/art");
  gfx_snap_iniciar(1920, 1080);
  vidroFundoPreparar();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  assert(home_iniciar("deploy/app/art"));
  salvos_iniciar();
  recomenda_iniciar();
  aparecer = REC_APARECER_SIM;
  montarCatalogo();
  ajustes_definir_vidro(0);
  quadros(20);

  printf("\no padrao e o de sempre:\n");
  confere("ordem padrao = ordem em que salvou", sorg_ordem() == SORG_ORDEM_SALVOU);
  confere("grupo padrao = progresso", sorg_grupo() == SORG_GRUPO_PROGRESSO);
  confere("estilo padrao = lista", sorg_estilo() == SORG_ESTILO_LISTA);
  spainel_abrir();
  durante(400);
  confere("primeira linha: o primeiro com progresso (Pulp Fiction)",
          !strcmp(tituloFocado(), "tt0110912"));
  foto("01-lista-padrao.png");
  // #203: titulo aberto no cartao (OK longo) com a lista agrupada por
  // progresso: o rotulo da secao de baixo ("Nao comecados") tem de descer junto.
  segurarOk();
  durante(700);
  foto("27c-cartao-aberto-secao.png");
  empurrar(SDL_KEYDOWN, SDLK_ESCAPE); quadros(1);
  empurrar(SDL_KEYUP, SDLK_ESCAPE); quadros(10);
  toque(SDLK_UP);
  toque(SDLK_DOWN);
  toque(SDLK_UP);
  foto("02-barra-foco-ordenar.png");

  printf("\nordenar:\n");
  toque(SDLK_RETURN);
  foto("03-ordenar-escolha.png");
  toques(SDLK_DOWN, 2);
  toque(SDLK_RETURN);   // Nome (A-Z)
  confere("escolher \"Nome\" grava a ordem", sorg_ordem() == SORG_ORDEM_NOME);
  confere("e fica no arquivo do perfil", arquivoTem("salvos-org-p1.txt", "ordem\t2"));
  confere("o arquivo e versionado", arquivoTem("salvos-org-p1.txt", "# nuvio salvos-org v1"));
  foto("04-ordem-nome.png");
  toque(SDLK_DOWN);
  confere("Continuar por nome: Game of Thrones antes de Pulp Fiction",
          !strcmp(tituloFocado(), "tt0944947"));

  printf("\nagrupar:\n");
  toque(SDLK_UP);                     // barra (Ordenar)
  toque(SDLK_RIGHT);                  // Agrupar
  toque(SDLK_RETURN);
  foto("05-agrupar-escolha.png");
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Tipo
  confere("agrupar por tipo", sorg_grupo() == SORG_GRUPO_TIPO);
  foto("06-agrupar-tipo.png");
  toque(SDLK_DOWN);
  confere("por tipo + nome: o primeiro filme e A Origem", !strcmp(tituloFocado(), "tt1375666"));
  { int k; for (k = 0; k < 14; k++) toque(SDLK_DOWN); }
  foto("07-agrupar-tipo-fim.png");

  printf("\nestilo:\n");
  toques(SDLK_UP, 20);                // abas
  toque(SDLK_DOWN);                   // barra: o painel reabriu em Ordenar
  toques(SDLK_RIGHT, 2);              // Estilo
  toque(SDLK_RETURN);
  foto("08-estilo-escolha.png");
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Grade
  confere("estilo grade", sorg_estilo() == SORG_ESTILO_GRADE);
  foto("09-grade.png");
  toque(SDLK_DOWN); toque(SDLK_RIGHT);
  r0 = spainel_n_reconstrucoes();
  foto("10-grade-foco.png");
  toque(SDLK_DOWN); toque(SDLK_LEFT); toque(SDLK_DOWN);
  quadros(10);
  confere("navegar a grade nao reconstroi a lista", spainel_n_reconstrucoes() == r0);
  foto("11-grade-rolada.png");
  toques(SDLK_UP, 8);
  toque(SDLK_DOWN);                   // barra
  toque(SDLK_RETURN);                 // Estilo
  toques(SDLK_DOWN, 2); toque(SDLK_RETURN);   // Paisagem
  confere("estilo paisagem", sorg_estilo() == SORG_ESTILO_PAISAGEM);
  foto("12-paisagem.png");
  toque(SDLK_DOWN); toque(SDLK_RIGHT);
  foto("13-paisagem-foco.png");
  ajustes_definir_vidro(1);
  foto("14-paisagem-vidro.png");
  toques(SDLK_UP, 6);
  toque(SDLK_DOWN);
  toque(SDLK_RETURN);
  toque(SDLK_UP); toque(SDLK_RETURN);     // de volta a Grade (vidro)
  foto("15-grade-vidro.png");
  toque(SDLK_RETURN);
  toque(SDLK_UP); toque(SDLK_UP); toque(SDLK_RETURN);   // Lista
  confere("de volta a lista", sorg_estilo() == SORG_ESTILO_LISTA);
  foto("16-lista-vidro-barra.png");
  ajustes_definir_vidro(0);

  printf("\nnova categoria:\n");
  toque(SDLK_RIGHT);                  // "Nova categoria"
  toque(SDLK_RETURN);
  tecladoAbriu();
  confere("sem categorias, a pilula abre o teclado direto", teclado_aberto());
  digitar("kids");
  foto("17-nova-categoria-teclado.png");
  tecladoAcao(2);                     // pronto
  quadros(4);
  confere("uma categoria criada", sorg_n_categorias() == 1);
  kids = sorg_categoria_id(0);
  confere("com a primeira letra em maiuscula: \"Kids\"",
          sorg_categoria_nome_id(kids) && !strcmp(sorg_categoria_nome_id(kids), "Kids"));
  confere("criar pela barra agrupa por categoria", sorg_grupo() == SORG_GRUPO_CATEGORIA);
  confere("gravada no arquivo", arquivoTem("salvos-org-p1.txt", "\tKids\n"));
  foto("18-categoria-vazia.png");

  printf("\nmover para categoria (segurar OK):\n");
  toque(SDLK_DOWN); toque(SDLK_DOWN);   // segunda linha da lista
  { const char *alvo = tituloFocado();
    char id[32];
    snprintf(id, sizeof id, "%s", alvo);
    snprintf(movido, sizeof movido, "%s", alvo);
    toque(SDLK_DOWN);                    // tituloFocado reabriu o painel no topo
  segurarOk();
    confere("segurar OK abre o menu do cartaz", ctx_do_painel());
    toque(SDLK_DOWN);                    // Remover, Mover
    foto("19-menu-mover.png");
    toque(SDLK_RETURN);
    quadros(4);
    confere("\"Mover para categoria\" abre a escolha no painel", !ctx_aberto() && spainel_aberto());
    foto("20-mover-escolha.png");
    toque(SDLK_UP);                      // o foco nasce em "Sem categoria"
    toque(SDLK_RETURN);                  // Kids
    quadros(4);
    confere("o titulo foi para Kids", sorg_categoria_de(id) == kids);
    confere("e continua salvo", salvos_tem(id));
    foto("21-categoria-com-titulo.png"); }

  printf("\nmover criando categoria:\n");
  toque(SDLK_DOWN);
  segurarOk();
  toques(SDLK_DOWN, 2); toque(SDLK_RETURN); quadros(3);
  toque(SDLK_DOWN);                    // de "Sem categoria" para "Nova categoria"
  toque(SDLK_RETURN);
  tecladoAbriu();
  digitar("fim de semana");
  tecladoAcao(2);
  quadros(4);
  confere("duas categorias", sorg_n_categorias() == 2);
  confere("\"Fim de semana\"", !strcmp(sorg_categoria_nome_id(sorg_categoria_id(1)), "Fim de semana"));
  foto("22-duas-categorias.png");

  printf("\nrenomear e excluir:\n");
  toques(SDLK_UP, 20); toque(SDLK_DOWN);   // barra
  toques(SDLK_RIGHT, 3);
  foto("23-barra-categorias.png");
  toque(SDLK_RETURN);
  foto("24-categorias-escolha.png");
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Kids
  foto("25-categoria-acoes.png");
  toque(SDLK_RETURN);                     // Renomear
  tecladoAbriu();
  confere("renomear abre o teclado com o nome atual",
          teclado_aberto() && !strcmp(teclado_texto(), "Kids"));
  tecladoAcao(1);                         // limpar
  tecladoVoltarTopo();
  digitar("infantil");
  tecladoAcao(2);
  quadros(4);
  confere("renomeada para \"Infantil\"", !strcmp(sorg_categoria_nome_id(kids), "Infantil"));
  confere("o titulo continua nela", sorg_categoria_de(movido) == kids);
  toque(SDLK_RETURN);                     // categorias de novo
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Infantil
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Excluir categoria
  foto("26-excluir-confirma.png");
  toque(SDLK_RETURN);                     // o foco nasce em Cancelar
  confere("Cancelar nao exclui", sorg_n_categorias() == 2);
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Infantil
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Excluir categoria
  toque(SDLK_DOWN); toque(SDLK_RETURN);   // Excluir
  quadros(4);
  confere("excluida", sorg_n_categorias() == 1 && sorg_categoria_indice(kids) < 0);
  confere("os titulos continuam salvos", salvos_n() == 9);
  foto("27-depois-de-excluir.png");
  confere("salvos.txt continua no formato v1", arquivoTem("salvos.txt", "# nuvio salvos v1\n"));
  // AGRUPADO POR PROGRESSO, a categoria vira selo na linha.
  toques(SDLK_UP, 3); toque(SDLK_DOWN);   // barra (Categorias)
  toques(SDLK_LEFT, 2); toque(SDLK_RETURN);
  toque(SDLK_UP); toque(SDLK_UP); toque(SDLK_RETURN);   // Progresso
  confere("de volta ao agrupamento por progresso", sorg_grupo() == SORG_GRUPO_PROGRESSO);
  toques(SDLK_DOWN, 4);
  foto("27b-selo-da-categoria.png");

  printf("\nsocial por pessoa:\n");
  toques(SDLK_UP, 8);
  toque(SDLK_RIGHT);                      // Social
  toque(SDLK_DOWN);                       // barra da Social
  if (vidroFundoAtivo()) ajustes_definir_vidro(1);   // o teste compara VIDRO
  foto("28-social-barra.png");
  toque(SDLK_RETURN);
  foto("29-social-escolha.png");
  toque(SDLK_DOWN); toque(SDLK_RETURN);
  confere("social por pessoa", sorg_social() == SORG_SOCIAL_PESSOA);
  toque(SDLK_DOWN);
  foto("30-social-por-pessoa.png");

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  SDL_Quit();
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
