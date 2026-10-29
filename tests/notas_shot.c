// CAPTURAS DA LINHA DE NOTAS DO TITULO E DA SECAO "NOTAS" (heatmap por fonte,
// resumo critica x publico e grade temporadas x episodios), sem rede.
//
//   bash tests/notas_shot.sh /tmp/nv-notas-shots/n
//
// Mesma receita de tests/detail_secoes_shot.c: extras.h e interceptado por
// #define e os DADOS de nota vem das tabelas deste arquivo. As escolhas da
// pessoa (Ajustes > Notas no titulo) sao escritas no ajustes.txt da pasta
// temporaria, que e o caminho de verdade que o app le.
//
// NUVIO_DADOS TEM DE APONTAR PARA UMA PASTA TEMPORARIA e o programa se recusa a
// rodar de outro jeito.

#include <SDL2/SDL.h>
#include "nvtmp.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
#define extras_comentario_lingua fx_com_lingua
// Ficha do TMDB do filme de ensaio: os valores CRUS (ingles) que a rede manda,
// para a captura provar a traducao de status/pais/duracao (fix/detalhe-traducao).
#define extras_ficha_status       fx_ficha_status
#define extras_ficha_lancamento   fx_ficha_lancamento
#define extras_ficha_duracao      fx_ficha_duracao
#define extras_ficha_paises       fx_ficha_paises
#define extras_ficha_classificacao fx_ficha_classificacao

#define extras_nota             fx_nota

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
// O 2o comentario esta em outro idioma que o da interface: leva a etiqueta.
const char *fx_com_lingua(int i)   { return i == 1 ? "en" : ""; }
const char *fx_ficha_status(void)        { return "Released"; }
const char *fx_ficha_lancamento(void)    { return "1999-03-31"; }
int         fx_ficha_duracao(void)       { return 136; }
const char *fx_ficha_paises(void)        { return "United States of America, Australia"; }
const char *fx_ficha_classificacao(void) { return "R"; }
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

// --- NOTAS DE ENSAIO ---------------------------------------------------------
// Valor CRU x 10, o mesmo de extras_nota(): imdb 7.8 -> 78, tomatoes 87% -> 870.
static int crus[EX_NFONTES];
// Como o extras real: a fonte escondida em Ajustes (mdblist_show_*) chega zerada.
int fx_nota(int f) { return ajustes_mdblist_fonte(f) ? crus[f] : 0; }

static void notas(int imdb, int trakt, int tmdb, int tomates, int audiencia,
                  int meta, int letter, int metauser, int mal, int ebert, int score) {
  crus[EX_IMDB] = imdb; crus[EX_TRAKT] = trakt; crus[EX_TMDB] = tmdb;
  crus[EX_TOMATOES] = tomates; crus[EX_AUDIENCE] = audiencia;
  crus[EX_METACRITIC] = meta; crus[EX_LETTERBOXD] = letter;
  crus[EX_METAUSER] = metauser; crus[EX_MAL] = mal; crus[EX_EBERT] = ebert;
  crus[EX_MDBSCORE] = score;
}

static int serieGrande;
static int ehSerieDeEnsaio(void) {
  const CatItem *ci = cat_item(idx);
  return ci && !strcmp(ci->imdb, IMDB_SERIE);
}
int fx_n_temporadas(void) { return ehSerieDeEnsaio() ? (serieGrande ? 27 : 2) : 0; }
int fx_temporada_numero(int t) { return t + 1; }
int fx_n_eps(int t) {
  if (!ehSerieDeEnsaio()) return 0;
  if (serieGrande) return 8 + (t * 7) % 17;
  return t == 1 ? N_T2 : 8;
}
int fx_ep_numero(int t, int i) { (void)t; return i + 1; }
// Serie grande: um arco de qualidade com ruido deterministico e uns poucos
// episodios fracos e excelentes, como a de verdade (nada de curva bonita).
int fx_ep_nota(int t, int i) {
  if (!serieGrande) {
    if (t != 1 || i < 0 || i >= N_T2) return 70 + (i % 9);
    return SERIE_T2[i].nota;
  }
  { unsigned h = (unsigned)(t * 131 + i * 31 + 7) * 2654435761u;
    int ruido = (int)((h >> 16) % 15) - 7;
    int arco = 80 - (t * t) / 40 + (t > 20 ? 4 : 0);
    int v = arco + ruido;
    if (((h >> 8) % 23) == 0) v -= 14;
    if (((h >> 12) % 29) == 0) v += 9;
    if (t == 3 && i == 4) return 0;          // um episodio sem nota
    return v < 32 ? 32 : v > 98 ? 98 : v; }
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
  snprintf(itens[1].genero, sizeof itens[1].genero, "Filme  \xc2\xb7  Ficção científica  \xc2\xb7  Ação");
  snprintf(itens[1].meta, sizeof itens[1].meta, "1999  \xc2\xb7  136 min");   // Cinemeta: minutos em ingles
  snprintf(itens[1].sinopse, sizeof itens[1].sinopse,
           "Sinopse de enchimento do filme, tambem comprida o bastante para o "
           "bloco de texto do heroi ficar com a altura que tem num titulo de "
           "verdade.");
  snprintf(itens[1].classificacao, sizeof itens[1].classificacao, "14");
  snprintf(itens[1].pais, sizeof itens[1].pais, "United States, Australia");   // Cinemeta: ingles
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
    episodios[n].notaImdb = 92;
    episodios[n].nota = 76;
    snprintf(episodios[n].nome, sizeof episodios[n].nome,
             "Episódio de ensaio %d", i + 1);
    snprintf(episodios[n].duracao, sizeof episodios[n].duracao, "47 min");
    n++;
  }
  for (i = 0; i < N_T2; i++) {
    episodios[n].temporada = 2; episodios[n].episodio = i + 1;
    episodios[n].notaImdb = 92;
    episodios[n].nota = 76;
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

// Escolhas da pessoa: escritas no ajustes.txt que o app le. `linhas` e o texto
// cru ("notaTituloAudiencia 0\n..."); V_LIGA: 0 = Ligado, 1 = Desligado.
static const char *dd;
static void escolher(const char *linhas) {
  char caminho[600]; FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dd);
  f = fopen(caminho, "w"); assert(f);
  // O ajustes_dir NAO volta ao padrao o que o arquivo nao cita: sem as linhas
  // de fabrica aqui, a escolha de uma captura vazaria para a seguinte.
  fputs("notaTituloImdb 0\nnotaTituloTomates 0\nnotaTituloTrakt 0\n"
        "notaTituloAudiencia 1\nnotaTituloMeta 1\nnotaTituloMetaUser 1\n"
        "notaTituloTmdb 1\nnotaTituloLetter 1\nnotaTituloMal 1\n"
        "notaTituloEbert 1\nnotaTituloScore 1\n"
        "mdblist_show_tomatoes 0\n", f);
  fputs(linhas, f);
  { const char *lg = getenv("NUVIO_SHOT_IDIOMA");
    if (lg && *lg) fprintf(f, "idioma %d\n", atoi(lg)); }
  // NUVIO_SHOT_VIDRO=1: Interface de vidro ligada (V_LIGA: 0 = Ligado).
  if (getenv("NUVIO_SHOT_VIDRO")) fputs("vidroLocal 0\n", f);
  fclose(f);
  ajustes_dir(dd);
}
#define TODAS \
  "notaTituloImdb 0\nnotaTituloTomates 0\nnotaTituloAudiencia 0\nnotaTituloMeta 0\n" \
  "notaTituloMetaUser 0\nnotaTituloTrakt 0\nnotaTituloTmdb 0\nnotaTituloLetter 0\n" \
  "notaTituloMal 0\nnotaTituloEbert 0\nnotaTituloScore 0\n"

static void abrirItem(int i) {
  HomeItem hi;
  memset(&hi, 0, sizeof hi);
  hi.indice = i;
  hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
  hi.titulo = itens[i].titulo; hi.genero = itens[i].genero; hi.meta = itens[i].meta;
  parado = 0;
  detail_abrir(&hi);
}
// O HERO, sem foco em secao nenhuma: a linha de notas fica no alto.
static void hero(int i) {
  abrirItem(i);
  nivel = 0;
  quadros(150);
}
// A SECAO "Notas" focada (a pagina rola ate ela).
static void secaoFoco(int i, int sec) {
  abrirItem(i);
  nivel = 1;
  if (!strcmp(itens[i].imdb, IMDB_SERIE)) { foco.fileira = SEC_TEMPORADAS; foco.coluna = 2; quadros(3); }
  else quadros(1);
  foco.fileira = sec; foco.coluna = 0;
  quadros(220);
}
#define secaoNotas(i) secaoFoco((i), SEC_NOTAS)

// Cache do /stats da serie de ensaio (formato de serieaud.c): o bloco
// "Numeros da temporada" carrega do disco, sem rede. `ate` = episodios com dado.
static void cacheAudiencia(int temp, int ate) {
  char nome[80], buf[4096];
  size_t k = 0;
  int i;
  snprintf(nome, sizeof nome, "serieaud-%s-t%d.txt", IMDB_SERIE, temp);
  k += (size_t)snprintf(buf + k, sizeof buf - k, "# nuvio serieaud v1\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\t%d\t%lld\t%ld\t%ld\n",
                        IMDB_SERIE, temp, (long long)time(NULL), 24126034L, 404730L);
  for (i = 0; i < N_T2 && i < ate; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%d\t%ld\t%ld\t%d\t%d\n",
                          SERIE_T2[i].ep, SERIE_T2[i].w, SERIE_T2[i].p,
                          SERIE_T2[i].com, SERIE_T2[i].vot);
  assert(dados_gravar_leve(nome, buf));
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-notas-shots/n");
  char nome[600];
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: notas do titulo", SDL_WINDOWPOS_CENTERED,
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
  tex_iniciar(16);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  dd = dados_dir();
  if (!dd || !strstr(dd, "nuvio-notas-dados")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de apontar para a pasta temporaria "
                    "do teste (dados_dir = \"%s\")\n", dd ? dd : "");
    return 1;
  }
  // As marcas moram em art/marcas: sem isto o caminho absoluto sai vazio.
  extras_carregar("deploy/app/art");
  { char cwd[512]; if (getcwd(cwd, sizeof cwd)) {
      static char art[560]; snprintf(art, sizeof art, "%s/deploy/app/art", cwd);
      extras_carregar(art); } }
  escolher("");
  montarCatalogo();

  if (getenv("NUVIO_NOTAORDEM_TEST")) {
    extern int ajustes_teste_focar_opcao(int);
    extern int ajustes_teste_ordem_notas(void);
    SDL_Event e = {0};
    NotasPlano p;
    int valores[EX_NFONTES] = {0};
    unsigned char querer[EX_NFONTES] = {0};
    e.type = SDL_KEYDOWN;
    escolher("notaTituloOrdem 1\n");
    assert(ajustes_nota_fonte(0) == EX_LETTERBOXD);
    escolher("notaTituloLetter 0\n");
    assert(ajustes_nota_fonte(0) == EX_IMDB);
    valores[EX_LETTERBOXD] = 40; querer[EX_LETTERBOXD] = 1;
    notasui_planejar(&p, valores, 250, 0, querer);
    assert(p.n == 1 && p.fonte[0] == EX_LETTERBOXD);
    valores[EX_IMDB] = 87; valores[EX_TOMATOES] = 830;
    querer[EX_IMDB] = querer[EX_TOMATOES] = 1;
    notasui_planejar(&p, valores, 250, 0, querer);
    { int found = 0; for (int i = 0; i < p.n; i++) found |= p.fonte[i] == EX_LETTERBOXD; assert(found); }
    notas(87, 840, 790, 830, 850, 730, 40, 82, 86, 35, 800);
    escolher(TODAS); hero(1);
    snprintf(nome, sizeof nome, "%s-title.png", saida); gravar(nome);
    assert(ajustes_teste_focar_opcao(ajustes_teste_ordem_notas()));
    e.key.keysym.sym = SDLK_RETURN; ajustes_evento(&e);
    e.key.keysym.sym = SDLK_DOWN;
    for (int i = 0; i < 7; i++) ajustes_evento(&e);
    e.key.keysym.sym = SDLK_RETURN;
    for (int i = 0; i < 6; i++) ajustes_evento(&e);
    assert(ajustes_nota_fonte(0) == EX_TMDB);
    { int seen[EX_NFONTES] = {0};
      for (int i = 0; i < EX_NFONTES; i++) seen[ajustes_nota_fonte(i)]++;
      for (int i = 0; i < EX_NFONTES; i++) assert(seen[i] == 1); }
    ajustes_dir(dd);
    assert(ajustes_nota_fonte(0) == EX_TMDB);
    valores[EX_TMDB] = 790; querer[EX_TMDB] = 1;
    notasui_planejar(&p, valores, 2000, 0, querer);
    assert(p.n == 4 && p.fonte[0] == EX_TMDB && p.fonte[1] == EX_IMDB);
    for (int i = 0; i < 60; i++) {
      txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
      glClearColor(0.02f, 0.02f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
      ajustes_atualizar(0.016f, SDL_GetTicks()); ajustes_desenhar(SDL_GetTicks());
      if (i < 59) SDL_GL_SwapWindow(janela);
    }
    snprintf(nome, sizeof nome, "%s-order.png", saida); gravar(nome);
    e.key.keysym.sym = SDLK_ESCAPE; ajustes_evento(&e);
    escolher("notaTituloImdb 0\nnotaTituloTrakt 1\nnotaTituloTmdb 1\n");
    assert(ajustes_nota_titulo(EX_IMDB) && !ajustes_nota_titulo(EX_TRAKT));
    secaoFoco(0, SEC_EPISODIOS);
    snprintf(nome, sizeof nome, "%s-episode.png", saida); gravar(nome);
    escolher("notaTituloFontes 0 0 0 0 0 0 0 0 0 0 0\n");
    assert(ajustes_nota_fonte(0) == EX_IMDB);
    // Episode card source: 0 follows the title line, 1-4 = IMDb / TMDB / both / off.
    escolher("notaTituloImdb 1\nnotaTituloTrakt 0\nnotaTituloTmdb 1\n");
    assert(!ajustes_nota_episodio(EX_IMDB) && ajustes_nota_episodio(EX_TRAKT) && !ajustes_nota_episodio(EX_TMDB));
    { static const int quer[5][3] = { {0,1,0}, {1,0,0}, {0,0,1}, {1,0,1}, {0,0,0} };  // IMDb, Trakt, TMDB
      for (int m = 1; m < 5; m++) {
        char l[40]; snprintf(l, sizeof l, "notaEpisodio %d\n", m); escolher(l);
        assert(ajustes_nota_episodio(EX_IMDB) == quer[m][0] && ajustes_nota_episodio(EX_TRAKT) == quer[m][1]
               && ajustes_nota_episodio(EX_TMDB) == quer[m][2]);
      } }
    puts("notaordem: PASS (settings events, persistence, planner, late loads)");
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
    return 0;
  }

  // --- 1. LINHA DO TITULO DE FABRICA: IMDb, Rotten Tomatoes, Trakt.
  notas(87, 840, 790, 830, 850, 730, 40, 82, 86, 35, 800);
  hero(1);
  snprintf(nome, sizeof nome, "%s-1-titulo-padrao.png", saida); gravar(nome);

  // --- 2. TODAS LIGADAS: a linha nao cabe, saem as de menor prioridade.
  escolher(TODAS);
  hero(1);
  snprintf(nome, sizeof nome, "%s-2-titulo-todas.png", saida); gravar(nome);

  // --- 3. Uma nota so.
  escolher("notaTituloTomates 1\nnotaTituloTrakt 1\n");
  hero(1);
  snprintf(nome, sizeof nome, "%s-3-titulo-so-imdb.png", saida); gravar(nome);

  // --- 4. Escolha do dono: Metacritic, Popcornmeter, Letterboxd, MAL (sem o padrao).
  escolher("notaTituloImdb 0\nnotaTituloTomates 1\nnotaTituloTrakt 1\n"
           "notaTituloMeta 0\nnotaTituloAudiencia 0\nnotaTituloLetter 0\nnotaTituloMal 0\n");
  hero(1);
  snprintf(nome, sizeof nome, "%s-4-titulo-escolha.png", saida); gravar(nome);

  // --- 5. Notas ruins: tomate podre, quadrado vermelho.
  notas(53, 480, 510, 190, 320, 380, 22, 41, 0, 15, 0);
  escolher("notaTituloMeta 0\nnotaTituloMetaUser 0\nnotaTituloAudiencia 0\nnotaTituloLetter 0\n");
  hero(1);
  snprintf(nome, sizeof nome, "%s-5-titulo-ruins.png", saida); gravar(nome);

  // --- 6. Uma fonte escondida na conta (mdblist_show_tomatoes) some da linha.
  notas(87, 840, 790, 830, 850, 730, 40, 82, 86, 35, 800);
  escolher("mdblist_show_tomatoes 1\n");
  hero(1);
  snprintf(nome, sizeof nome, "%s-6-titulo-conta-esconde.png", saida); gravar(nome);

  // --- 7. SERIE: a linha com as notas e o "% assistido".
  escolher("");
  hero(0);
  snprintf(nome, sizeof nome, "%s-7-titulo-serie.png", saida); gravar(nome);

  // --- 8. FILME: a secao Notas, todas as fontes (onze blocos, seis linhas).
  escolher(TODAS);
  secaoNotas(1);
  assert(secaoN(SEC_NOTAS) == notasui_fontes_n(notasDados()));
  snprintf(nome, sizeof nome, "%s-8-filme-secao.png", saida); gravar(nome);
  // 8b. D-pad dentro da grade: baixo desce uma linha, direita anda na linha,
  //     esquerda na coluna da esquerda nao muda de bloco.
  { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_DOWN;  detail_evento(&ev); assert(foco.fileira == SEC_NOTAS && foco.coluna == 2);
    ev.key.keysym.sym = SDLK_RIGHT; detail_evento(&ev); assert(foco.coluna == 3);
    ev.key.keysym.sym = SDLK_RIGHT; detail_evento(&ev); assert(foco.coluna == 3);
    ev.key.keysym.sym = SDLK_UP;    detail_evento(&ev); assert(foco.coluna == 1);
    ev.key.keysym.sym = SDLK_DOWN;  detail_evento(&ev); detail_evento(&ev); assert(foco.coluna == 5);
    quadros(60); }
  snprintf(nome, sizeof nome, "%s-8b-filme-foco-bloco.png", saida); gravar(nome);

  // --- 9. FILME: so critica (sem comparativo) e uma nota so.
  notas(78, 0, 0, 870, 0, 0, 0, 0, 0, 0, 0);
  secaoNotas(1);
  snprintf(nome, sizeof nome, "%s-9-filme-poucas.png", saida); gravar(nome);

  // --- 10. SERIE: a secao Notas logo abaixo dos episodios.
  notas(89, 830, 810, 940, 880, 810, 43, 79, 0, 0, 850);
  secaoNotas(0);
  snprintf(nome, sizeof nome, "%s-10-serie-secao.png", saida); gravar(nome);
  // 10b. NUMEROS DA TEMPORADA antes de entrar (sem cache: nada pedido ainda),
  //      com o foco nas Notas e a pagina rolada a mao ate o bloco.
  assert(secaoN(SEC_NUMEROS) == N_T2);
  assert(topoSec[SEC_NUMEROS] >= conteudoSec[SEC_NOTAS] + alturaSecao(SEC_NOTAS));
  assert(topoSec[SEC_ABAS_INFO] >= topoSec[SEC_NUMEROS] + alturaSecao(SEC_NUMEROS));
  parado = 1; scrollY = topoSec[SEC_NUMEROS] - 120.0f; quadros(40); parado = 0;
  snprintf(nome, sizeof nome, "%s-10b-serie-numeros-chamada.png", saida); gravar(nome);
  // 10c. Dentro do bloco, com o /stats da T2 no cache: os tres cartoes, E1.
  cacheAudiencia(2, N_T2);
  secaoFoco(0, SEC_NUMEROS);
  assert(audAberta);
  snprintf(nome, sizeof nome, "%s-10c-serie-numeros-e1.png", saida); gravar(nome);
  // 10d. Direita x7: o E8 em foco nos tres cartoes.
  { SDL_Event ev; int q; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_RIGHT;
    for (q = 0; q < 7; q++) detail_evento(&ev);
    assert(foco.fileira == SEC_NUMEROS && foco.coluna == 7);
    quadros(60); }
  snprintf(nome, sizeof nome, "%s-10d-serie-numeros-e8.png", saida); gravar(nome);
  // 10e. Ordem do D-pad: Notas -> Numeros -> abas/elenco.
  { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_UP; detail_evento(&ev);
    assert(foco.fileira == SEC_NOTAS);
    foco.coluna = notasui_fontes_n(notasDados()) - 1;
    ev.key.keysym.sym = SDLK_DOWN; detail_evento(&ev);
    assert(foco.fileira == SEC_NUMEROS);
    detail_evento(&ev);
    assert(foco.fileira == SEC_ABAS_INFO || foco.fileira == SEC_ELENCO); }

  // --- 11. SERIE com 27 temporadas: o mapa encolhe, rotulos de 5 em 5; a T2
  //         (a T2 continua com o /stats que o modulo ja tem em memoria).
  serieGrande = 1;
  secaoFoco(0, SEC_NUMEROS);
  snprintf(nome, sizeof nome, "%s-11-serie-27-temporadas.png", saida); gravar(nome);
  serieGrande = 0;

  // --- 12. FILME notas ruins com desacordo grande critica x publico.
  notas(62, 590, 610, 240, 880, 300, 32, 85, 0, 15, 0);
  secaoNotas(1);
  snprintf(nome, sizeof nome, "%s-12-filme-desacordo.png", saida); gravar(nome);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
