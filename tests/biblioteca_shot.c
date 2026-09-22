// CAPTURA DA BIBLIOTECA RENOVADA, sem interacao e sem rede.
//
// Existe pelo motivo que tests/player_regression.c ja registra: interface de TV
// julgada so por codigo sai ilegivel a 3 m. Duas armadilhas deste app foram
// pegas SO olhando — o raio do gfx_cor e uma FRACAO DA ALTURA, nao pixels, e
// texto claro sobre superficie clara some. As duas so aparecem em BMP.
//
// O QUE ELE CAPTURA: as duas exibicoes (cartaz e lista) das duas grades novas,
// a lista aberta com a barra de acoes, o teclado da busca publica por cima, e a
// degradacao da fonte Simkl sem vinculo.
//
// ESCREVE EM NUVIO_DADOS, e recusa rodar se dados_dir() nao for essa pasta:
// fixar uma lista GRAVA, e um teste deste repositorio ja sobrescreveu os dados
// reais do dono.
#include "biblioteca.h"
#include "listas.h"
#include "ajustes.h"
#include "extras.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  biblioteca_evento(&e);
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    biblioteca_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    biblioteca_desenhar(SDL_GetTicks());
    SDL_GL_SwapWindow(win);
  }
}

// QUANTO A ARTE DESTA TELA ESTA OCUPANDO AGORA. `bytesQuentes` e o conjunto
// desenhado neste quadro ou no anterior — exatamente o que a exibicao escolhida
// obriga o cache a manter vivo. E o numero que separa "a lista e mais leve" de
// uma suposicao.
static void medir(const char *rotulo) {
  int itens = 0, pend = 0, quentes = 0;
  long bytes = 0, bytesQuentes = 0;
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQuentes);
  // O NUMERO QUE IMPORTA E O KB POR ITEM, e nao o total: quantos decodes
  // terminaram dentro dos 50 quadros da captura varia entre execucoes (o decode
  // e assincrono), mas o custo de CADA arte e estavel e e ele que a exibicao
  // escolhe.
  printf("  [arte] %-18s %d itens, %ld KB (%ld KB por arte) | quentes=%d, %ld KB\n",
         rotulo, itens, bytes / 1024, itens ? bytes / 1024 / itens : 0,
         quentes, bytesQuentes / 1024);
  fflush(stdout);
}

static void captura(const char *nome) {
  unsigned char *pix;
  SDL_Surface *s;
  int y;
  quadros(50);
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(6);
  biblioteca_atualizar(1.0f / 60.0f, SDL_GetTicks());
  glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  biblioteca_desenhar(SDL_GetTicks());
  pix = malloc(1920 * 1080 * 4);
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(win);
  printf("captura: %s\n", nome);
}

// TITULOS DE MENTIRA, escolhidos para cobrir OS CASOS DA LINHA, e nao para
// encher a grade. Cada um existe por um motivo:
//
//   0  metadado completo + nota + progresso  -> a linha cheia, com as duas
//                                               celulas da direita ocupadas
//   1  TITULO MUITO LONGO                    -> prova o corte com reticencias
//                                               sem invadir a coluna da nota
//   2  SEM meta e SEM generos, so "Filme"    -> o caso do item que veio da
//                                               watchlist do Trakt e nunca foi
//                                               enriquecido (ver linhaMeta)
//   3  metadado completo, SEM NOTA           -> o travessao na coluna
//   4  serie com temporadas                  -> "Série" derivado, nao do campo
//   resto  variacao normal
static const struct {
  const char *titulo, *meta, *genero, *sinopse;
  int serie, nota, progresso, restante;
} AMOSTRA[] = {
  { "CODA", "2021 · 1 season", "TV Show · Drama · Music",
    "Uma familia encontra musica, coragem e um novo caminho.", 1, 84, 42, 61 },
  { "Killers of the Flower Moon and the Rest of a Name Far Too Long to Fit on One Single Line No Matter What",
    "2023 · 3 h 26 min", "Movie · Crime · Drama · History · Western",
    "Uma investigacao sobre poder, dinheiro e os crimes de uma cidade inteira.",
    0, 79, 0, 0 },
  { "Dune: Part Two", "", "Movie", "", 0, 0, 0, 0 },
  { "Aftersun", "2022", "Movie · Drama", "Memorias de uma viagem que muda de sentido com o tempo.",
    0, 0, 68, 24 },
  { "Everything Everywhere All at Once", "2022 · 3 seasons",
    "TV Show · Science Fiction · Adventure", "", 1, 79, 0, 0 },
  { "The Brutalist", "2024", "Movie · Drama", "", 0, 82, 0, 0 },
  { "Poor Things", "2023", "Movie · Comedy · Romance", "", 0, 78, 12, 0 },
  { "The Substance", "2024", "Movie · Horror", "", 0, 71, 0, 0 },
  { "Anatomy of a Fall", "2023", "Movie · Crime · Drama", "", 0, 79, 0, 0 },
  { "Isle of Dogs", "2018", "Movie · Animation", "", 0, 78, 0, 0 },
  { "Civil War", "2024", "Movie · Action · Drama", "", 0, 71, 0, 0 },
  { "Bacurau", "2019", "Movie · Mystery · Thriller", "", 0, 74, 0, 0 },
  { "Cidade de Deus", "2002", "Movie · Crime · Drama", "", 0, 86, 0, 0 },
  { "O Agente Secreto", "", "Programa de TV", "", 1, 0, 0, 0 },
};

static void povoar(void) {
  CatItem v[sizeof AMOSTRA / sizeof AMOSTRA[0]];
  size_t i;
  for (i = 0; i < sizeof AMOSTRA / sizeof AMOSTRA[0]; i++) {
    CatItem it;
    memset(&it, 0, sizeof it);
    snprintf(it.titulo, sizeof it.titulo, "%s", AMOSTRA[i].titulo);
    snprintf(it.imdb, sizeof it.imdb, "tt900%02d", (int)i);
    snprintf(it.tipo, sizeof it.tipo, "%s", AMOSTRA[i].serie ? "series" : "movie");
    snprintf(it.meta, sizeof it.meta, "%s", AMOSTRA[i].meta);
    snprintf(it.genero, sizeof it.genero, "%s", AMOSTRA[i].genero);
    snprintf(it.sinopse, sizeof it.sinopse, "%s", AMOSTRA[i].sinopse);
    snprintf(it.poster, sizeof it.poster, "deploy/app/art/%02d.jpg", (int)i);
    it.nota = AMOSTRA[i].nota;
    it.progresso = AMOSTRA[i].progresso;
    it.restanteMin = AMOSTRA[i].restante;
    if (AMOSTRA[i].serie) it.nTemporadas = 1;
    it.naLista = 1;
    it.naColecao = (i % 2) == 0;
    v[i] = it;
  }
  // cat_definir e nao cat_acrescentar em laco: aquele RECUSA quando o catalogo
  // esta vazio (`n < 1`), que e justamente o estado de um teste sem rede.
  cat_definir(v, (int)(sizeof AMOSTRA / sizeof AMOSTRA[0]));
}

static char *lerArquivo(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  long n;
  char *b;
  if (!f) { printf("nao abri %s\n", caminho); exit(2); }
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = malloc((size_t)n + 1);
  assert(b && fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0;
  fclose(f);
  return b;
}

// As listas entram POR FIXTURE, no lugar do que a rede traria. Tem de ser
// DEPOIS de o pedido da fonte terminar: lst_pedir dispara um fio que, sem rede,
// volta zerando a tabela — injetar antes seria apagado por ele.
// UMA GRADE CHEIA, gerada. O fixture de busca tem duas listas — o bastante para
// conferir o CARTAO, e insuficiente para julgar DENSIDADE, que foi o pedido do
// dono ("podia mostrar mais listas"). Aqui saem dezoito, com nomes de tamanhos
// reais: curtos, um que ocupa duas linhas e um que nem em duas cabe.
static void injetarMuitasListas(void) {
  static const char *NOMES[] = {
    // NOMES EM INGLES porque isto e DADO, e nao interface: numa lista publica
    // do Trakt o nome vem de quem a criou. Em ingles a mesma captura serve a
    // conferencia daqui e ao album do post, sem a mistura de linguas que so
    // parece defeito para quem le o album. O que importa para o layout e o
    // comprimento, e ele foi preservado: um curto, um que ocupa duas linhas e
    // um que nem em duas cabe.
    "Everything A24", "80s sci-fi",
    "Harry Potter and the Prisoner of Azkaban and All the Others",
    "Cult", "Oscars 2025", "Japanese horror", "Noir",
    "Documentaries worth a whole evening", "Kurosawa",
    "British comedy", "Adult animation", "Westerns",
    "Last year's best releases according to the critics",
    "Studio Ghibli", "Neo-noir", "Slow cinema", "Musicals", "Giallo",
  };
  char json[8000];
  size_t k = 0, i;
  k += (size_t)snprintf(json + k, sizeof json - k, "[");
  for (i = 0; i < sizeof NOMES / sizeof NOMES[0]; i++)
    k += (size_t)snprintf(json + k, sizeof json - k,
        "%s{\"type\":\"list\",\"list\":{\"name\":\"%s\",\"item_count\":%d,"
        "\"likes\":%d,\"ids\":{\"trakt\":%d},"
        "\"user\":{\"username\":\"curator%d\",\"ids\":{\"slug\":\"c%d\"}}}}",
        i ? "," : "", NOMES[i], 12 + (int)i * 7, (int)i, 900000 + (int)i,
        (int)i, (int)i);
  snprintf(json + k, sizeof json - k, "]");
  quadros(40);
  lst_ler_trakt(json, 1);
  quadros(4);
}

static void injetarListas(void) {
  char *j = lerArquivo("tests/fixtures/trakt_busca_listas.json");
  quadros(40);
  lst_ler_trakt(j, 1);
  free(j);
  j = lerArquivo("tests/fixtures/trakt_minhas_listas.json");
  /* As duas fontes juntas dao quatro cartoes, o bastante para a segunda linha
     da grade aparecer. lst_ler_trakt substitui a tabela, entao a segunda
     leitura sozinha nao serve — a de busca fica e esta so confere o caminho. */
  free(j);
  quadros(4);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-biblioteca";
  // "lista" no segundo argumento COMECA em exibicao de lista, sem ter desenhado
  // um quadro antes. E a unica forma de medir a arte das duas exibicoes com o
  // cache FRIO nas duas: o tex_cache e indexado pelo CAMINHO, nao pela largura
  // pedida, entao uma textura ja decodificada a 268 continua valendo para a
  // miniatura de 64 — alternar a exibicao com o cache quente nao encolhe nada
  // (quem encolhe e o LRU, quando a grande esfria). Os eventos de tecla nao
  // desenham, entao a troca aqui nao aquece nada.
  int comecaLista = argc > 2 && !strcmp(argv[2], "lista");
  const char *quero = getenv("NUVIO_DADOS");
  char nome[600];
  SDL_GLContext gl;

  if (!quero || !*quero) {
    printf("RECUSADO: rode por tests/biblioteca_shot.sh — NUVIO_DADOS nao definido.\n");
    return 2;
  }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: revisao da Biblioteca", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1920, 1080,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  // O wordmark do Trakt mora em art/marcas/, e quem sabe esse caminho e o
  // extras.c. Sem isto o selo cai no texto e a captura nao prova nada sobre a
  // marca.
  extras_carregar("deploy/app/art");

  dados_iniciar(quero);
  if (strcmp(dados_dir(), quero)) {
    printf("RECUSADO: dados_dir()=\"%s\" nao e a pasta do teste \"%s\".\n",
           dados_dir(), quero);
    return 2;
  }
  printf("gravando em %s\n", dados_dir());

  // A interface em PORTUGUES, que e a lingua em que estas telas sao escritas e
  // revisadas. Sem ajustes_iniciar, `valor[AJ_IDIOMA]` fica no default estatico
  // e a captura sai em ingles com as chaves novas cruas no meio.
  ajustes_iniciar();
  povoar();
  biblioteca_iniciar();
  if (comecaLista) {
    int k;
    tecla(SDLK_DOWN);
    for (k = 0; k < 3; k++) tecla(SDLK_LEFT);
    for (k = 0; k < 2; k++) tecla(SDLK_RIGHT);
    tecla(SDLK_RETURN);
  }

  // SUBIR ATE A BARRA DE MODOS com um numero fixo de UPs seria adivinhacao: a
  // altura da grade muda com a exibicao. Oito e mais do que a tela tem de
  // fileiras visiveis, e subir alem do topo nao faz nada.
  #define AO_TOPO() do { int _k; for (_k = 0; _k < 8; _k++) tecla(SDLK_UP); } while (0)
  // Anda ate o seletor `n` da faixa, vindo da barra de modos.
  #define SELETOR(n) do { int _k; tecla(SDLK_DOWN); \
    for (_k = 0; _k < 3; _k++) tecla(SDLK_LEFT); \
    for (_k = 0; _k < (n); _k++) tecla(SDLK_RIGHT); } while (0)
  // Escolhe o modo `n` da barra (0 = Salvos, 1 = Coleção, 2 = Listas).
  #define MODO(n) do { int _k; AO_TOPO(); \
    for (_k = 0; _k < 3; _k++) tecla(SDLK_LEFT); \
    for (_k = 0; _k < (n); _k++) tecla(SDLK_RIGHT); } while (0)

  // 1. Salvos em cartazes, com o foco NA GRADE: e onde se ve a pilula escolhida
  // continuando marcada com o foco longe dela, e o contorno do cartaz.
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-salvos-cartaz.bmp", saida);
  captura(nome);
  medir(comecaLista ? "lista (frio)" : "cartazes (frio)");

  // 2. A BARRA DE MODOS COM O FOCO NELA: escolhido + em foco, realce cheio.
  AO_TOPO();
  snprintf(nome, sizeof nome, "%s-modos-foco.bmp", saida);
  captura(nome);

  // 3. A faixa de TRES seletores, com "Exibição" (o novo) em foco. Aqui a
  // pilula escolhida da barra de cima aparece no OUTRO estado: escolhida, sem
  // foco — realce a 60%, e sem contorno nenhum.
  SELETOR(2);
  snprintf(nome, sizeof nome, "%s-seletores.bmp", saida);
  captura(nome);

  // 3. A mesma biblioteca em lista.
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-salvos-lista.bmp", saida);
  captura(nome);
  medir(comecaLista ? "cartazes (quente)" : "lista (quente)");

  // 4. Modo LISTAS, em lista.
  MODO(2);
  injetarListas();
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-listas-lista.bmp", saida);
  captura(nome);

  // 5. A mesma grade em CARTOES de lista.
  AO_TOPO();
  SELETOR(1);
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-listas-cartao.bmp", saida);
  captura(nome);

  // A MESMA grade com dezoito listas: e aqui que a densidade se julga.
  injetarMuitasListas();
  snprintf(nome, sizeof nome, "%s-listas-densidade.bmp", saida);
  captura(nome);

  // 6, 7 e 8. Uma lista ABERTA: breadcrumb e barra de acoes. Sem rede nao ha
  // itens, e o estado vazio e o certo. Depois, fixada e na Home.
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-lista-aberta.bmp", saida);
  captura(nome);
  tecla(SDLK_RETURN);      // acao 0: fixar
  snprintf(nome, sizeof nome, "%s-lista-fixada.bmp", saida);
  captura(nome);
  // OS TRES ESTADOS NUM QUADRO SO. Na barra de MODOS eles nao cabem juntos: ali
  // mover o foco TROCA o modo, entao "nao escolhido + em foco" nao existe por
  // construcao. Na barra de acoes existe — "Fixada" fica escolhida com o foco
  // na vizinha.
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-acoes-tres-estados.bmp", saida);
  captura(nome);
  tecla(SDLK_RETURN);      // acao 1: adicionar a Home
  snprintf(nome, sizeof nome, "%s-lista-home.bmp", saida);
  captura(nome);
  tecla(SDLK_ESCAPE);      // volta para a lista de listas

  // 9. A fonte SIMKL sem vinculo: a tela diz o que falta, e nao mostra aba
  // vazia. O seletor de fonte cicla Trakt -> Simkl.
  AO_TOPO();
  SELETOR(0);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-simkl-sem-vinculo.bmp", saida);
  captura(nome);

  // A aba FIXADAS e a unica grade MISTA: e nela, e so nela, que a marca da
  // fonte volta ao cartao. Duas voltas a mais no seletor: Simkl -> Nuvio ->
  // Fixadas.
  tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-fixadas-grade-mista.bmp", saida);
  captura(nome);
  AO_TOPO();

  // 10. O teclado da busca publica, por cima da tela.
  SELETOR(2);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-busca-teclado.bmp", saida);
  captura(nome);

  printf("pronto\n");
  return 0;
}
