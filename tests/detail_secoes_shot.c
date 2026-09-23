// CAPTURA DA PAGINA DE TITULO COM AS DUAS SECOES NOVAS — audiencia (serieaud)
// e frases + ficha de producao (seriefrases) —, sem rede nenhuma.
//
// POR QUE ESTA CAPTURA EXISTE, e nao bastam tests/serieaud_shot.c e
// tests/seriefrases_shot.c: aqueles desenham os paineis SOZINHOS, num retangulo
// escolhido a mao. O que este responde e outra coisa, e e o que decide se a
// fiacao presta:
//
//   a secao aparece no lugar certo DENTRO do documento rolavel?
//   ela empurra o que vem depois (comentarios, estudios) em vez de cair por
//     cima, como a secao do Trakt ja caiu sobre os avatares do elenco?
//   o que se ve ANTES de alguem entrar nela nao mente?
//   o destaque anda com o D-pad?
//   o caso VAZIO — que e o comum em serie, ver a cobertura medida em
//     seriefrases.h — le como informacao ou como defeito?
//
// SEM REDE E SEM CREDENCIAL, de proposito, e por dois caminhos:
//
//   extras.h e INTERCEPTADO por #define antes do include de detail.c (a mesma
//     receita de tests/serieaud.c). Quem responde pelas temporadas e pelas
//     notas por episodio e a tabela deste arquivo. Assim a captura nao depende
//     do Trakt estar de pe, do token do dono estar vivo, nem traz para dentro
//     do quadro o conteudo escrito por terceiros que a secao de comentarios
//     mostraria.
//
//   serieaud e seriefrases leem o CACHE DE DISCO, e este arquivo escreve o
//     cache antes de abrir a pagina. E o caminho de verdade dos dois modulos
//     (aplicarCache), com zero pedidos — o mesmo que uma segunda visita faz.
//
// OS TEXTOS DAS FRASES SAO DE ENCHIMENTO E ISSO E DELIBERADO. Eles nao estao
// aqui para ser lidos, e sim para EXERCITAR OS LIMITES do painel: uma fala que
// quebra em tres linhas, uma de uma linha so, uma SEM autor (metade das falas
// do Wikiquote nao diz quem falou) e uma que estoura o teto de SF_LINHAS e tem
// de terminar em reticencias. Os numeros de audiencia, esses sim, sao os
// medidos na api em 16/09/2026 e copiados de tests/serieaud_shot.c — curva
// inventada e sempre bonita.
//
//   bash tests/detail_secoes_shot.sh /tmp/nuvio-detsec
//
// NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA e o proprio arquivo se recusa a
// rodar se dados_dir() nao for ela: ele ESCREVE cache, e sem a trava escreveria
// dentro do ~/.nuvio de quem executa.
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- INTERCEPTACAO DE extras.h ----------------------------------------------
// So o que detail.c consulta nas secoes que interessam aqui. O resto do modulo
// continua ligado normalmente; estes nomes sao trocados ANTES do include, entao
// a troca vale para detail.c e para mais nada.
#define extras_pedir            fx_pedir
#define extras_carregando       fx_carregando
#define extras_n_temporadas     fx_n_temporadas
#define extras_temporada_numero fx_temporada_numero
#define extras_n_eps            fx_n_eps
#define extras_ep_numero        fx_ep_numero
#define extras_ep_nota          fx_ep_nota
#define extras_n_comentarios    fx_n_comentarios
#define extras_n_comentarios_ep fx_n_comentarios_ep
#define extras_comentario_usuario  fx_com_usuario
#define extras_comentario_texto    fx_com_texto
#define extras_comentario_curtidas fx_com_curtidas
#define extras_comentario_nota     fx_com_nota
#define extras_n_relacionados   fx_n_relacionados
#define extras_relacionado_titulo fx_relacionado_titulo
#define extras_relacionado_ano    fx_relacionado_ano
#define extras_relacionado_poster fx_relacionado_poster
#define extras_n_colecao        fx_n_colecao
#define extras_n_estudios       fx_n_estudios
#define extras_estudio_nome     fx_estudio_nome
#define extras_estudio_logo     fx_estudio_logo
#define extras_n_trailers       fx_n_trailers
#define extras_nota_trakt       fx_nota_trakt

#include "../src/detail.c"

#include "dados.h"

// --- A SERIE DE ENSAIO -------------------------------------------------------
// 13 episodios numa temporada 2, com as notas que o Trakt publica. As notas
// entram pela tabela abaixo (extras_ep_nota), e os watchers/plays pelo cache de
// disco — que e exatamente a divisao do modulo real: nota vem de graca do
// `seasons?extended=episodes,full`, audiencia vem do /stats.
typedef struct { int ep, nota; long w, p; int com, vot; } Fix;

static const Fix SERIE_T2[] = {
  {  1, 81, 364060, 423111, 10, 3445 },
  {  2, 84, 361321, 418989, 13, 3467 },
  {  3, 79, 360700, 417529,  8, 3278 },
  {  4, 78, 358789, 416703, 20, 3236 },
  {  5, 79, 358206, 415361, 14, 3224 },
  {  6, 81, 356977, 413054, 23, 3232 },
  {  7, 80, 357109, 413444, 15, 3203 },
  {  8, 84, 355875, 412309, 22, 3254 },
  {  9, 83, 354997, 411444, 21, 3201 },
  { 10, 80, 354231, 409979, 22, 3110 },
  { 11, 83, 353772, 408180, 13, 3131 },
  { 12, 85, 353026, 406882, 28, 3147 },
  { 13, 85, 353462, 407430, 20, 3171 }
};
#define N_T2 ((int)(sizeof SERIE_T2 / sizeof SERIE_T2[0]))

#define IMDB_SERIE "tt0903747"
#define IMDB_FILME "tt0133093"
#define IMDB_VAZIO "tt0000009"

// --- DUBLE DE extras ---------------------------------------------------------
void fx_pedir(const char *imdb, int serie, long tmdbId) {
  (void)imdb; (void)serie; (void)tmdbId;
}
int fx_carregando(void)       { return 0; }
// COMENTARIOS DE ENSAIO, so no filme: a captura 12 e o cartao de comentario
// no tamanho novo (600x340, 19/09/2026), com um texto que estoura as seis
// linhas e outro curto — o rodape tem de ficar no mesmo lugar nos dois.
static int comentariosLigados;
static const char *const COM_USU[] = { "robertaajr", "demarisp", "kfilms" };
static const char *const COM_TXT[] = {
  "And the award for worst lighting in a movie goes to ... 'Do Not Enter'. This might not "
  "have been so awful if you could actually see anything that's happening. No, you know "
  "what, this still would've sucked either way. It starts off so darn slow and stupid. The "
  "movie essentially has no point and it's completely boring. We hardly get to know anyone.",
  "this movie should have been so good- the story line just wasn't there. Acting and camera "
  "work are amazing but there is no substance in the movie.",
  "Fine for a rainy afternoon." };
static const int COM_NOTA[] = { 2, 5, 7 }, COM_CUR[] = { 6, 3, 1 };
int fx_n_comentarios(void)    { return comentariosLigados ? 3 : 0; }
int fx_n_comentarios_ep(void) { return 0; }
const char *fx_com_usuario(int i)  { return COM_USU[i]; }
const char *fx_com_texto(int i)    { return COM_TXT[i]; }
int fx_com_curtidas(int i)         { return COM_CUR[i]; }
int fx_com_nota(int i)             { return COM_NOTA[i]; }
static int extrasCardsLigados;
static const char *const REL_TIT[] = { "The Second Chapter", "Night Archive", "The Glass Shore" };
static const char *const REL_ANO[] = { "2024", "2025", "2026" };
static const char *const REL_PO[] = {
  "deploy/app/art/poster/00.jpg", "deploy/app/art/poster/07.jpg",
  "deploy/app/art/poster/19.jpg" };
static const char *const EST_NOME[] = { "Northlight Pictures", "A24 Television", "Nuvio Studios" };
int fx_n_relacionados(void)   { return extrasCardsLigados ? 3 : 0; }
const char *fx_relacionado_titulo(int i) { return REL_TIT[i]; }
const char *fx_relacionado_ano(int i) { return REL_ANO[i]; }
const char *fx_relacionado_poster(int i) { return REL_PO[i]; }
int fx_n_colecao(void)        { return 0; }
int fx_n_estudios(void)       { return extrasCardsLigados ? 3 : 0; }
const char *fx_estudio_nome(int i) { return EST_NOME[i]; }
const char *fx_estudio_logo(int i) { (void)i; return ""; }
int fx_n_trailers(void)       { return 0; }
int fx_nota_trakt(void)       { return 82; }

// SO a serie de ensaio tem temporadas. O filme devolve zero, que e o estado
// real de um filme — e e o que faz a secao de audiencia nao existir nele.
static int ehSerieDeEnsaio(void) {
  const CatItem *ci = cat_item(idx);
  return ci && !strcmp(ci->imdb, IMDB_SERIE);
}
int fx_n_temporadas(void) { return ehSerieDeEnsaio() ? 2 : 0; }
// Temporadas 1 e 2; a pagina abre na 2, que e onde o cache esta.
int fx_temporada_numero(int t) { return t + 1; }
int fx_n_eps(int t) { return ehSerieDeEnsaio() ? (t == 1 ? N_T2 : 8) : 0; }
int fx_ep_numero(int t, int i) { (void)t; return i + 1; }
int fx_ep_nota(int t, int i) {
  if (t != 1 || i < 0 || i >= N_T2) return 70 + (i % 9);
  return SERIE_T2[i].nota;
}

// --- SEMEADURA DO CACHE ------------------------------------------------------
// O formato e o que serieaud.c/seriefrases.c gravam e leem. Escrever por aqui
// (e nao chamar o modulo) e o que mantem a captura sem rede: aplicarCache e o
// primeiro passo do fio dos dois, e com cobertura total ele encerra ali mesmo.

static void cacheAudiencia(const char *imdb, int temp, int ate) {
  char nome[80], buf[4096];
  size_t k = 0;
  int i;
  snprintf(nome, sizeof nome, "serieaud-%s-t%d.txt", imdb, temp);
  k += (size_t)snprintf(buf + k, sizeof buf - k, "# nuvio serieaud v1\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\t%d\t%lld\t%ld\t%ld\n",
                        imdb, temp, (long long)time(NULL), 24126034L, 404730L);
  for (i = 0; i < N_T2 && i < ate; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%d\t%ld\t%ld\t%d\t%d\n",
                          SERIE_T2[i].ep, SERIE_T2[i].w, SERIE_T2[i].p,
                          SERIE_T2[i].com, SERIE_T2[i].vot);
  assert(dados_gravar_leve(nome, buf));
}

// UMA fala por linha: "Q<TAB>quem<TAB>texto". Os quatro casos que o painel tem
// de aguentar, nesta ordem, e nenhum deles e uma fala de verdade — ver a nota
// do topo.
static void cacheFrases(const char *imdb, int comConteudo) {
  char nome[80];
  char buf[4096];
  size_t k = 0;
  snprintf(nome, sizeof nome, "frases-%s.txt", imdb);
  k += (size_t)snprintf(buf + k, sizeof buf - k, "# nuvio seriefrases v1\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k, "%lld\t%s\t0\n",
                        (long long)time(NULL),
                        comConteudo ? "Pagina de exemplo" : "");
  if (!comConteudo) { assert(dados_gravar_leve(nome, buf)); return; }
  // FICHA: um valor curto, um que quebra em duas linhas e um rotulo comprido.
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "F\tOrçamento\t63 milhões de dólares\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "F\tBilheteria\t467 milhões de dólares\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "F\tLocal de filmagem\tPrimeira cidade, segunda cidade, terceira cidade, "
      "quarta cidade e uma quinta que obriga o valor a quebrar em mais de uma "
      "linha dentro da coluna estreita da direita\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "F\tPrêmio recebido\tQuatro prêmios técnicos\n");
  // FALA 1 — tres linhas cheias, com autor de nome curto.
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "Q\tPersonagem Um\tEsta linha de enchimento existe para ocupar tres "
      "linhas inteiras da coluna larga da esquerda e mostrar como fica a "
      "superficie de foco quando a fala e das compridas.\n");
  // FALA 2 — uma linha so.
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "Q\tPersonagem Dois\tUma linha curta, e nada mais.\n");
  // FALA 3 — SEM autor: nao pode deixar buraco onde a atribuicao estaria.
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "Q\t\tFala de enchimento sem nenhum autor, que e o caso de metade das "
      "falas que a fonte publica.\n");
  // FALA 4 — estoura o teto de linhas e tem de terminar em reticencias.
  k += (size_t)snprintf(buf + k, sizeof buf - k,
      "Q\tPersonagem de Nome Bem Mais Comprido\tTexto de enchimento escrito de "
      "proposito para passar bem do teto de tres linhas que o painel desenha, "
      "de modo que a ultima linha visivel precise terminar em reticencias em "
      "vez de ser cortada no seco no meio de uma palavra qualquer, e para isso "
      "ele segue por mais uma frase inteira alem do que caberia.\n");
  assert(dados_gravar_leve(nome, buf));
}

// --- CATALOGO DE ENSAIO ------------------------------------------------------

static CatItem itens[2];
static CatEp   episodios[21];

static void montarCatalogo(void) {
  CatFileira fil;
  int i, n = 0;
  (void)n;
  memset(itens, 0, sizeof itens);
  memset(&fil, 0, sizeof fil);

  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Série de Ensaio");
  snprintf(itens[0].imdb, sizeof itens[0].imdb, IMDB_SERIE);
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "series");
  snprintf(itens[0].genero, sizeof itens[0].genero,
           "Programa de TV · Drama · Suspense");
  snprintf(itens[0].meta, sizeof itens[0].meta, "2008 · 2 temporadas");
  snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
           "Sinopse de enchimento, comprida o bastante para ocupar as linhas "
           "que o heroi reserva para ela e empurrar a pilha de meta para a "
           "base da tela, como acontece num titulo de verdade.");
  snprintf(itens[0].classificacao, sizeof itens[0].classificacao, "16");
  snprintf(itens[0].pais, sizeof itens[0].pais, "Brasil");
  // O CHAO DE VERDADE DESTA PAGINA, e nao um preto chapado.
  //
  // Sem `backdrop` o arteDe() devolve NULL, desenhaArteDetalhe pinta #0D0D0D
  // chapado e a captura valida os cards contra um fundo que a TV NUNCA mostra:
  // la eles caem sobre a ARTE DA OBRA apagada a 15% (detail_desenhar, o
  // `1 - 0.85 * pg`). A diferenca nao e sutil — sobre arte, um veu fraco deixa
  // passar rosto e lettering justo onde o texto do card fica, e um selo
  // translucido que parecia opaco no preto vira uma janela para a imagem.
  //
  // Esta e a mesma correcao que tests/serieaud_shot.c ja tinha feito por conta
  // propria (o chaoDaPagina de la); aqui sai mais barato e mais fiel: em vez de
  // pintar a arte por fora, DA a arte ao item e deixa detail.c seguir o caminho
  // de verdade — mesmo GFX_DETALHE, mesma vinheta, mesmo 0,15.
  snprintf(itens[0].backdrop, sizeof itens[0].backdrop,
           "deploy/app/art/03.jpg");
  itens[0].nota = 89;
  itens[0].temporadas[0] = 1; itens[0].temporadas[1] = 2;
  itens[0].nTemporadas = 2;
  // ELENCO DE ENCHIMENTO, e nao e decoracao: sem ele a faixa da aba fica vazia e
  // a captura nao prova a coisa que mais importa aqui — que a banda de
  // audiencia comeca ABAIXO do conteudo da aba ativa, em vez de cair por cima
  // dele como a secao do Trakt ja caiu sobre os avatares.
  for (i = 0; i < 6; i++) {
    snprintf(itens[0].elenco[i].nome, sizeof itens[0].elenco[i].nome,
             "Elenco de Ensaio %d", i + 1);
    snprintf(itens[0].elenco[i].papel, sizeof itens[0].elenco[i].papel,
             "Papel %d", i + 1);
  }
  itens[0].nElenco = 6;

  snprintf(itens[1].titulo, sizeof itens[1].titulo, "Filme de Ensaio");
  snprintf(itens[1].imdb, sizeof itens[1].imdb, IMDB_FILME);
  snprintf(itens[1].tipo, sizeof itens[1].tipo, "movie");
  snprintf(itens[1].genero, sizeof itens[1].genero, "Filme · Ficção científica");
  snprintf(itens[1].meta, sizeof itens[1].meta, "1999 · 2 h 16 min");
  snprintf(itens[1].sinopse, sizeof itens[1].sinopse,
           "Sinopse de enchimento do filme, tambem comprida o bastante para o "
           "bloco de texto do heroi ficar com a altura que tem num titulo de "
           "verdade.");
  snprintf(itens[1].classificacao, sizeof itens[1].classificacao, "14");
  snprintf(itens[1].pais, sizeof itens[1].pais, "Estados Unidos");
  // Arte DIFERENTE da serie, de proposito: se as duas fossem a mesma, uma
  // captura trocada passaria despercebida.
  snprintf(itens[1].backdrop, sizeof itens[1].backdrop,
           "deploy/app/art/07.jpg");
  itens[1].nota = 87;
  for (i = 0; i < 6; i++) {
    snprintf(itens[1].elenco[i].nome, sizeof itens[1].elenco[i].nome,
             "Elenco de Ensaio %d", i + 1);
    snprintf(itens[1].elenco[i].papel, sizeof itens[1].elenco[i].papel,
             "Papel %d", i + 1);
  }
  itens[1].nElenco = 6;

  fil.ini = 0; fil.n = 2;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 2, &fil, 1);

  // 8 episodios na T1 e 13 na T2 — a mesma contagem que o duble de extras diz.
  for (i = 0; i < 8; i++) {
    episodios[n].temporada = 1; episodios[n].episodio = i + 1;
    snprintf(episodios[n].nome, sizeof episodios[n].nome,
             "Episódio de ensaio %d", i + 1);
    snprintf(episodios[n].duracao, sizeof episodios[n].duracao, "47 min");
    n++;
  }
  for (i = 0; i < N_T2; i++) {
    episodios[n].temporada = 2; episodios[n].episodio = i + 1;
    snprintf(episodios[n].nome, sizeof episodios[n].nome,
             "Episódio de ensaio %d", i + 1);
    snprintf(episodios[n].duracao, sizeof episodios[n].duracao, "47 min");
    n++;
  }
  cat_definir_episodios(0, episodios, n);
}

// --- CAPTURA -----------------------------------------------------------------

static SDL_Window *janela;

static void gravar(const char *nome) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
           1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s  (%d desenhos gfx no quadro)\n", nome, gfx_n_rect);
}

// Um quadro so nao basta: text.c rasteriza no maximo TXT_POR_QUADRO linhas por
// quadro, entao a primeira passada sai com quase todo o texto faltando. Repetir
// e o que o aparelho faz nos primeiros quadros da secao.
//
// `parado` desliga o detail_atualizar: serve as capturas que empurram scrollY a
// mao para olhar uma parte do documento que o foco nao alcanca. Com a
// atualizacao ligada a mola puxaria a rolagem de volta no quadro seguinte.
static int parado;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    // BOMBEAR ANTES DE TUDO, como main.c faz. Sem isto nada decodifica: a fila
    // de texturas so anda dentro do tex_bombear, e a captura sairia com a arte
    // do backdrop e as miniaturas dos episodios em cinza — de volta ao chao
    // chapado que este arquivo existe para nao ter.
    tex_bombear(3);
    if (!parado) detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    detail_desenhar(SDL_GetTicks());
    if (i < n - 1) SDL_GL_SwapWindow(janela);
  }
}

// Abre a pagina do item `i`, poe o foco na secao pedida e deixa as molas
// assentarem. `col` e a coluna dentro da secao.
static void abrir(int i, int secao, int col) {
  HomeItem hi;
  memset(&hi, 0, sizeof hi);
  hi.indice = i;
  hi.rect.x = 0; hi.rect.y = 0; hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
  hi.titulo = itens[i].titulo;
  hi.genero = itens[i].genero;
  hi.meta = itens[i].meta;
  parado = 0;
  detail_abrir(&hi);
  nivel = 1;
  // A T2 E A QUE TEM CACHE, e escolher a temporada aqui NAO e cravar
  // `temporada`: detail_atualizar reescreve essa variavel a partir da COLUNA da
  // fileira de temporadas a cada quadro (o seletor troca no movimento do foco).
  // Cravar e deixar o foco em SEC_TEMPORADAS coluna 0 punha a pagina de volta
  // na T1 no primeiro quadro — e a captura saia com a temporada errada, que e
  // justamente o defeito que este arquivo existe para pegar.
  if (!strcmp(itens[i].imdb, IMDB_SERIE)) {
    foco.fileira = SEC_TEMPORADAS;
    foco.coluna = 1;
    quadros(3);
  } else {
    quadros(1);
  }
  foco.fileira = secao;
  foco.coluna = col;
  quadros(150);
}

// A ABA ESCOLHIDA. `abaInfo` so muda dentro do tratamento do OK, e a captura nao
// manda tecla nenhuma — sem isto a pagina fica sempre na aba de elenco e a
// grade de notas por episodio, que e a vizinha de cima da audiencia, nunca
// aparece na foto.
static void escolherAba(int visivel) { abaInfo = visivel; quadros(60); }

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-detsec";
  char nome[600];
  SDL_GLContext gl;
  const char *dd;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: secoes novas da pagina de titulo",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  ajustes_iniciar();

  // A TRAVA. Este arquivo ESCREVE cache; sem ela escreveria no ~/.nuvio de quem
  // executa. Mesma guarda de tests/serieaud.sh, pelo mesmo motivo.
  dados_iniciar("deploy/app/art");
  dd = dados_dir();
  if (!dd || !strstr(dd, "nuvio-detsec-dados")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de apontar para a pasta temporaria "
                    "do teste (dados_dir = \"%s\")\n", dd ? dd : "");
    return 1;
  }

  montarCatalogo();

  // --- 1. SERIE, foco nas ABAS: a banda de audiencia aparece logo abaixo e
  //        mostra a CHAMADA, porque ninguem entrou nela ainda.
  cacheAudiencia(IMDB_SERIE, 2, N_T2);
  cacheFrases(IMDB_SERIE, 0);           // serie SEM pagina de frases: o comum
  abrir(0, SEC_ABAS_INFO, 1);           // aba "Avaliações"
  escolherAba(1);
  snprintf(nome, sizeof nome, "%s-1-serie-chamada.png", saida);
  gravar(nome);

  // --- 2. SERIE, foco na primeira banda: o arco de qualidade.
  abrir(0, SEC_AUD_ARCO, 0);
  snprintf(nome, sizeof nome, "%s-2-serie-arco.png", saida);
  gravar(nome);

  // --- 3. Uma descida: o radar entra nos mesmos 33% da tela.
  foco.fileira = SEC_AUD_RADAR; foco.coluna = 0;
  quadros(90);
  snprintf(nome, sizeof nome, "%s-3-serie-radar.png", saida);
  gravar(nome);

  // --- 4. Outra descida: a impressao digital, com o primeiro episodio escolhido.
  foco.fileira = SEC_AUD_DIGITAL; foco.coluna = 0;
  quadros(90);
  snprintf(nome, sizeof nome, "%s-4-serie-digital-e1.png", saida);
  gravar(nome);

  // --- 5. D-pad andado ate o episodio 8: o destaque e o rodape de numeros
  //        crus tem de acompanhar.
  foco.coluna = 7;
  quadros(60);
  snprintf(nome, sizeof nome, "%s-5-serie-digital-e8.png", saida);
  gravar(nome);

  // --- 6. SERIE, foco nas FRASES, caso VAZIO — o que acontece em 10 de 12
  //        series. As duas colunas dizem o que a fonte nao tem.
  abrir(0, SEC_FRASES, 0);
  snprintf(nome, sizeof nome, "%s-6-serie-frases-vazio.png", saida);
  gravar(nome);

  // --- 7. SERIE com cache PARCIAL (3 de 13): buracos na curva, o estado de uma
  //        visita interrompida. Passa pela T1 primeiro para o modulo largar a
  //        temporada que ja tem em memoria — repetir a MESMA nao refaz nada, e
  //        e assim que ele deve se comportar.
  cacheAudiencia(IMDB_SERIE, 2, 3);
  abrir(0, SEC_TEMPORADAS, 0);          // T1: sem cache, fica vazia
  foco.fileira = SEC_AUD_ARCO; foco.coluna = 0;
  quadros(30);
  foco.fileira = SEC_TEMPORADAS; foco.coluna = 1;   // volta para a T2
  quadros(30);
  foco.fileira = SEC_AUD_ARCO; foco.coluna = 0;
  quadros(120);
  snprintf(nome, sizeof nome, "%s-7-serie-arco-parcial.png", saida);
  gravar(nome);

  // --- 11. SEM DADOS DE AUDIENCIA: a T1 nao tem cache e esta captura nao tem
  //         chave do Trakt, entao as bandas dizem que nao ha o que mostrar. E o
  //         estado de quem abre a secao numa instalacao sem credencial, e e
  //         tambem o que prova o PISO DE ALTURA: o cabecalho do radar tem de
  //         ficar abaixo do arco mesmo com o arco vazio. Sem o piso, a captura
  //         anterior mostrava "Drop-off radar" escrito por cima da curva.
  foco.fileira = SEC_TEMPORADAS; foco.coluna = 0;   // T1: sem cache
  quadros(3);
  foco.fileira = SEC_AUD_ARCO; foco.coluna = 0;
  quadros(120);
  snprintf(nome, sizeof nome, "%s-11-serie-aud-sem-dados.png", saida);
  gravar(nome);

  // O ESTADO "Carregando…" NAO TEM CAPTURA PROPRIA AQUI, e vale dizer por que
  // em vez de deixar o buraco sem explicacao.
  //
  // Ele e desenhado pelo MESMO ramo que a captura 11 mostra — o `vazio()` de
  // serieaud.c, que escolhe entre "Carregando…" e "Sem dados desta temporada"
  // conforme serieaud_carregando(). O que a fiacao precisa provar e que essa
  // linha cai DENTRO da banda e dentro da altura reservada, e e isso que a 11
  // prova; a palavra em si e do modulo e ja esta em tests/serieaud_shot.sh.
  //
  // Fabricar o estado aqui exigiria segurar o fio, e segurar o fio exige rede:
  // sem cache e sem chave ele nasce e morre no mesmo milissegundo. Tentou-se com
  // a credencial de art/trakt.txt e NAO FUNCIONOU — trakt_cabecalhos() ganha de
  // trakt_cabecalhos_publicos() quando existe token, e o token daquele arquivo
  // esta vencido: as chamadas voltam 401 depressa e o estado que aparece e o de
  // "sem dados", nao o de carregando. Um teste que depende de um token vivo para
  // capturar um estado de meio segundo nao paga o que custa.

  // --- 8. FILME, foco nas FRASES, populado — a cobertura alta e no filme
  //        (11 de 14). Sem banda de audiencia: filme nao tem temporada.
  cacheFrases(IMDB_FILME, 1);
  abrir(1, SEC_FRASES, 0);
  snprintf(nome, sizeof nome, "%s-8-filme-frases.png", saida);
  gravar(nome);

  // --- 9. FILME, a MESMA secao com a ultima citacao escolhida: a superficie
  //        clara anda, o filete ao redor dela some dos dois lados e a fileira
  //        PANORAMIZA para a citacao escolhida nao ficar abaixo da dobra.
  seriefrases_selecionar(3);
  quadros(90);
  snprintf(nome, sizeof nome, "%s-9-filme-frases-fim.png", saida);
  gravar(nome);

  // --- 10. FILME, foco no ELENCO e a pagina empurrada a mao ate a secao de
  //         frases: e o que se ve ANTES de descer ate ela — a chamada.
  abrir(1, SEC_ELENCO, 0);
  parado = 1;
  scrollY = conteudoSec[SEC_FRASES] - 260.0f;
  quadros(40);
  snprintf(nome, sizeof nome, "%s-10-filme-chamada.png", saida);
  gravar(nome);

  // --- 12. FILME, foco nos COMENTARIOS: o cartao no tamanho novo, o segundo
  //         em foco (superficie clara), texto longo cortado antes do rodape.
  comentariosLigados = 1;
  abrir(1, SEC_COMENTARIOS, 1);
  snprintf(nome, sizeof nome, "%s-12-filme-comentarios.png", saida);
  gravar(nome);

  // --- 13. FILME, Relacionados em foco: cartao de vidro com accent dinamico.
  // Usa OCEANO em vez do branco padrao para comparar com a fileira de Studios.
  { char caminho[600]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dd);
    f = fopen(caminho, "w"); assert(f);
    fputs("selected_theme 2\n", f); fclose(f);
    ajustes_dir(dd); }
  extrasCardsLigados = 1;
  abrir(1, SEC_RELACIONADOS, 1);
  snprintf(nome, sizeof nome, "%s-13-filme-relacionados-accent.png", saida);
  gravar(nome);

  // --- 14. SERIE, Studios em foco: surface de vidro no accent ativo.
  abrir(0, SEC_ESTUDIOS, 1);
  snprintf(nome, sizeof nome, "%s-14-serie-studios-accent.png", saida);
  gravar(nome);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
