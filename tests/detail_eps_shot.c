// CAPTURA DA FILEIRA DE TEMPORADAS E DE EPISODIOS da pagina de titulo, sem
// rede nenhuma.
//
// POR QUE ESTA CAPTURA EXISTE, e nao basta tests/detail_secoes_shot.c: aquela
// olha as secoes de BAIXO (audiencia e frases) e fixa uma serie so, toda
// exibida e sem historico. O que decide a qualidade das duas fileiras de cima
// sao justamente os estados que ela nao tem:
//
//   uma temporada com episodios que AINDA NAO FORAM AO AR — o card os desenha
//     igual a um episodio que voce so nao viu, e a lista convida a abrir o que
//     nao existe;
//   uma temporada INTEIRA vista, e outra com NADA visto — os dois extremos da
//     linha de progresso da temporada;
//   o historico que NAO CHEGOU, que e diferente de "nada visto" e nao pode ser
//     desenhado como zero (vistoep_estado devolve -1, ver vistoep.h);
//   uma temporada de 24 episodios, onde a fileira rola de verdade;
//   um FILME, que nao tem nem temporada nem episodio e onde as duas fileiras
//     precisam sumir por inteiro.
//
// O CHAO E A ARTE DA OBRA A 15%, e nao um preto chapado. Os itens de ensaio
// carregam `backdrop`, entao detail.c segue o caminho de verdade
// (desenhaArteDetalhe com `1 - 0.85 * pg`). Uma captura em fundo chapado
// aprova veu, selo translucido e barra de progresso que na TV aparecem por
// cima de um rosto — ja aconteceu neste repositorio.
//
// SEM REDE: extras.h e interceptado por #define antes do include de detail.c
// (mesma receita de tests/detail_secoes_shot.c). O mapa de vistos vem do
// modulo REAL (vistoep.c), semeado aqui por vistoep_definir — e o mesmo caminho
// que a leitura do Trakt usa, so que sem o Trakt.
//
//   bash tests/detail_eps_shot.sh /tmp/nuvio-deteps
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- INTERCEPTACAO DE extras.h ----------------------------------------------
// So o que as fileiras de temporada e episodio consultam. O resto do modulo
// continua ligado; estes nomes sao trocados ANTES do include, entao a troca
// vale para detail.c e para mais nada.
#define extras_pedir             fx_pedir
#define extras_carregando        fx_carregando
#define extras_n_temporadas      fx_n_temporadas
#define extras_temporada_numero  fx_temporada_numero
#define extras_n_eps             fx_n_eps
#define extras_ep_numero         fx_ep_numero
#define extras_ep_nota           fx_ep_nota
#define extras_n_comentarios     fx_n_comentarios
#define extras_n_comentarios_ep  fx_n_comentarios_ep
#define extras_n_relacionados    fx_n_relacionados
#define extras_n_colecao         fx_n_colecao
#define extras_n_estudios        fx_n_estudios
#define extras_n_trailers        fx_n_trailers
#define extras_trailer_yt        fx_trailer_yt
#define extras_trailer_nome      fx_trailer_nome
#define extras_trailer_miniatura fx_trailer_miniatura
#define extras_nota_trakt        fx_nota_trakt
#define extras_ep_visto          fx_ep_visto
#define extras_progresso_pronto  fx_progresso_pronto
#define extras_progresso_serie   fx_progresso_serie
#define extras_proximo_episodio  fx_proximo_episodio
#define extras_agenda_temporada  fx_agenda_temporada
#define extras_agenda_episodio   fx_agenda_episodio
#define extras_agenda_data       fx_agenda_data
#define extras_agenda_status     fx_agenda_status

// --- ESPIAO DO DESFOQUE (#133) ---------------------------------------------
// gfx_desfocado e trocado so dentro de detail.c: o espiao anota QUAL arte a
// pagina pediu desfocada e repassa ao modulo real (gfx.c), que gera a copia de
// verdade no GL — a captura mostra o resultado, o espiao prova a regra.
#define gfx_desfocado fx_desfocado
#define recomenda_ativo fx_recomenda_ativo
#define recomenda_n fx_recomenda_n
#define recomenda_item fx_recomenda_item

#include "text.h"
#include "plrui.h"
static const char *fxTituloEsperado;
static int fxTituloDesenhado;
static const char *fxSinopseEsperada;
static int fxSinopseDesenhada;
static float fxTituloY, fxTituloH, fxPlayY, fxInfoY;
static void fx_icone(GfxRect r, const char *nome, float cr, float cg, float cb, float ca);
static float fx_kicker(const char *s, float x, float y, int r, int g, int b, float a);
static float fx_titulo_bloco(TxtEstilo estilo, const char *s, int r, int g, int b,
                            float x, float y, float larg, float leading,
                            float alpha, int maxLinhas);
#define txt_bloco fx_titulo_bloco
#define gfx_icone fx_icone
#define plrui_kicker fx_kicker
#include "../src/detail.c"
#include "menu.h"
#include "intro.h"
#include "ilha.h"
#undef txt_bloco
#undef gfx_icone
#undef plrui_kicker
#undef recomenda_ativo
#undef recomenda_n
#undef recomenda_item
static void fx_icone(GfxRect r, const char *nome, float cr, float cg, float cb, float ca) {
  if (ca > .3f && !strcmp(nome,"pl_play-f")) fxPlayY=r.y;
  gfx_icone(r,nome,cr,cg,cb,ca);
}
static float fx_kicker(const char *s, float x, float y, int r, int g, int b, float a) {
  if (a > .1f) fxInfoY=y;
  return plrui_kicker(s,x,y,r,g,b,a);
}
static float fx_titulo_bloco(TxtEstilo estilo, const char *s, int r, int g, int b,
                            float x, float y, float larg, float leading,
                            float alpha, int maxLinhas) {
  float height = txt_bloco(estilo,s,r,g,b,x,y,larg,leading,alpha,maxLinhas);
  if (fxTituloEsperado && s && !strcmp(s,fxTituloEsperado) && alpha > 0.3f) {
    assert(maxLinhas == 0); // The complete name is wrapped, never ellipsized.
    assert(y >= 109.0f && y + height < NV_TELA_H);
    fxTituloY = y;
    fxTituloH = height;
    fxTituloDesenhado++;
  }
  if (fxSinopseEsperada && s && !strcmp(s,fxSinopseEsperada) && alpha > 0.3f) {
    fxSinopseDesenhada++;
  }
  return height;
}

#undef gfx_desfocado
GLuint gfx_desfocado(GLuint src, const char *chave);
#define FX_DESF_MAX 64
static char fxDesf[FX_DESF_MAX][160];
static int  fxDesfN, fxDesfOk;
GLuint fx_desfocado(GLuint src, const char *chave) {
  GLuint t = gfx_desfocado(src, chave);
  int i;
  if (t) fxDesfOk++;
  for (i = 0; i < fxDesfN; i++) if (!strcmp(fxDesf[i], chave ? chave : "")) break;
  if (i == fxDesfN && fxDesfN < FX_DESF_MAX)
    snprintf(fxDesf[fxDesfN++], sizeof fxDesf[0], "%s", chave ? chave : "");
  return t;
}
static int fxDesfPediu(const char *chave) {
  for (int i = 0; i < fxDesfN; i++) if (!strcmp(fxDesf[i], chave)) return 1;
  return 0;
}

#include "dados.h"

// --- A SERIE DE ENSAIO -------------------------------------------------------
//
// T1: 10 episodios de 2024, todos exibidos.
// T2: 12 episodios de 2025, todos exibidos.
// T3: 24 episodios SEMANAIS de 2026 — a temporada comprida, em exibicao: do E12
//     em diante ainda nao foi ao ar, e e o que a agenda do TMDB publica
//     (next_episode_to_air = T3E12).
//
// A CONSISTENCIA DA MONTAGEM E PARTE DO TESTE. A primeira versao deste arquivo
// punha o proximo episodio na T2 E dava a T3 datas passadas — uma serie que
// ainda vai exibir a T2 nao pode ter a T3 no ar, e a captura mostrou o
// resultado: 24 cards com "ESTREIA" e o selo de visto no mesmo card. O defeito
// era da montagem, nao do desenho, mas so apareceu porque a foto foi olhada.
#define IMDB_SERIE "tt0903747"
#define IMDB_FILME "tt0133093"

static int fxRecomendacaoLigada;
int fx_recomenda_ativo(void) { return fxRecomendacaoLigada; }
int fx_recomenda_n(void) { return fxRecomendacaoLigada; }
int fx_recomenda_item(int i, RecItem *item) {
  if (!fxRecomendacaoLigada || i != 0) return 0;
  memset(item, 0, sizeof *item);
  snprintf(item->imdb, sizeof item->imdb, "%s", IMDB_FILME);
  snprintf(item->deNome, sizeof item->deNome, "Marina");
  snprintf(item->de, sizeof item->de, "nuvio:fixture");
  return 1;
}

#define T1_N 10
#define T2_N 12
#define T3_N 24
#define T3_PROX_EP 12         // primeiro NAO exibido da T3

static const int TEMP_NUM[3] = { 1, 2, 3 };
static const int TEMP_N[3]   = { T1_N, T2_N, T3_N };

// Estado do duble, trocado entre capturas.
static int agendaLigada = 1;   // o TMDB respondeu com next_episode_to_air?
static int progVis, progExib;  // extras_progresso_serie

void fx_pedir(const char *imdb, int serie, long tmdbId) {
  (void)imdb; (void)serie; (void)tmdbId;
}
int fxCarregandoLigado;
int fx_carregando(void)       { return fxCarregandoLigado; }
int fx_n_comentarios(void)    { return 0; }
int fx_n_comentarios_ep(void) { return 0; }
int fx_n_relacionados(void)   { return 0; }
int fx_n_colecao(void)        { return 0; }
int fx_n_estudios(void)       { return 0; }
int fx_nota_trakt(void)       { return 82; }

static int ehSerieDeEnsaio(void) {
  const CatItem *ci = cat_item(idx);
  return ci && !strcmp(ci->imdb, IMDB_SERIE);
}
// TRAILERS DA SERIE (#123): a fileira que o layout de serie nao tinha. O
// primeiro e o da temporada mais recente, como extras.c ordena.
static int trailersLigados;
static const char *const TR_NOME[] = {
  "Season 3 Official Trailer", "Season 2 Teaser", "Season 1 Official Trailer" };
static const char *const TR_MINI[] = {
  "deploy/app/art/27.jpg", "deploy/app/art/07.jpg", "deploy/app/art/03.jpg" };
// Filme tambem: a fileira e a mesma nos dois layouts, e o foco nela tambem.
int fx_n_trailers(void) { return trailersLigados ? 3 : 0; }
const char *fx_trailer_yt(int i) { return (i >= 0 && i < fx_n_trailers()) ? "s3TrailerEN" : ""; }
const char *fx_trailer_nome(int i) { return (i >= 0 && i < fx_n_trailers()) ? TR_NOME[i] : ""; }
const char *fx_trailer_miniatura(int i) { return (i >= 0 && i < fx_n_trailers()) ? TR_MINI[i] : ""; }

int fx_n_temporadas(void) { return ehSerieDeEnsaio() ? 3 : 0; }
int fx_temporada_numero(int t) {
  return (t >= 0 && t < 3) ? TEMP_NUM[t] : 0;
}
int fx_n_eps(int t) {
  if (!ehSerieDeEnsaio() || t < 0 || t >= 3) return 0;
  return TEMP_N[t];
}
int fx_ep_numero(int t, int i) { (void)t; return i + 1; }
// Notas plausiveis e VARIADAS — o card mostra o selo "Trakt 8.1" so quando ha
// nota. Episodio nao exibido nao tem nota, e e o duble que precisa dizer isso:
// inventar 7.8 para um episodio que nao foi ao ar seria o defeito que a
// captura existe para pegar.
int fx_ep_nota(int t, int i) {
  if (t == 2 && i + 1 >= T3_PROX_EP && agendaLigada) return 0;
  return 74 + ((i * 3 + t * 5) % 16);
}

// O mapa do Trakt vive em vistoep.c neste teste; extras_ep_visto e a fonte
// ANTIGA e continua respondendo "nao sei" para nao competir com ele.
int fx_ep_visto(int temporada, int episodio) { (void)temporada; (void)episodio; return 0; }
int fx_progresso_pronto(void) { return 0; }
int fx_proximo_episodio(int *t, int *e) { (void)t; (void)e; return 0; }
int fx_progresso_serie(int *vistosEp, int *exibidos) {
  if (progExib <= 0) return 0;
  if (vistosEp) *vistosEp = progVis;
  if (exibidos) *exibidos = progExib;
  return 1;
}

// A AGENDA: o TMDB diz qual e o PROXIMO episodio a estrear. E o unico sinal de
// "ainda nao foi ao ar" que a pagina tem sem gastar pedido nenhum — ver a nota
// em desenhaEpisodio.
int fx_agenda_temporada(void) { return agendaLigada && ehSerieDeEnsaio() ? 3 : 0; }
int fx_agenda_episodio(void)  { return agendaLigada && ehSerieDeEnsaio() ? T3_PROX_EP : 0; }
const char *fx_agenda_data(void) {
  return agendaLigada && ehSerieDeEnsaio() ? "2026-09-23" : "";
}
const char *fx_agenda_status(void) {
  return ehSerieDeEnsaio() ? "returning series" : "Released";
}

// --- CATALOGO DE ENSAIO ------------------------------------------------------

static CatItem itens[2];
static CatEp   episodios[T1_N + T2_N + T3_N];

// Datas por extenso, como o Cinemeta entrega depois do desc_data_extenso. A T3
// sai as tercas: os onze primeiros ja passaram (hoje e 17/09/2026) e do E12 em
// diante estao no futuro — a mesma fronteira que a agenda publica.
static const char *DATA_T3[T3_N] = {
  "7 de julho de 2026",       "14 de julho de 2026",
  "21 de julho de 2026",      "28 de julho de 2026",
  "4 de agosto de 2026",      "11 de agosto de 2026",
  "18 de agosto de 2026",     "25 de agosto de 2026",
  "1 de setembro de 2026",    "8 de setembro de 2026",
  "15 de setembro de 2026",   "23 de setembro de 2026",
  "30 de setembro de 2026",   "6 de outubro de 2026",
  "13 de outubro de 2026",    "20 de outubro de 2026",
  "27 de outubro de 2026",    "3 de novembro de 2026",
  "10 de novembro de 2026",   "17 de novembro de 2026",
  "24 de novembro de 2026",   "1 de dezembro de 2026",
  "8 de dezembro de 2026",    "15 de dezembro de 2026"
};

static const char *SINOPSE_ENSAIO =
  "Sinopse de enchimento do episodio, comprida o bastante para ocupar as tres "
  "linhas que o card reserva e mostrar onde o bloco corta, que e o unico jeito "
  "de julgar se o rodape ainda respira.";

static CatFileira filEnsaio;
static void montarCatalogo(void) {
  CatFileira fil;
  int i, t, n = 0;
  memset(itens, 0, sizeof itens);
  memset(episodios, 0, sizeof episodios);
  memset(&fil, 0, sizeof fil);

  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Série de Ensaio");
  snprintf(itens[0].imdb, sizeof itens[0].imdb, IMDB_SERIE);
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "series");
  snprintf(itens[0].genero, sizeof itens[0].genero,
           "Programa de TV · Drama · Suspense");
  snprintf(itens[0].meta, sizeof itens[0].meta, "2024 · 3 temporadas");
  snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
           "Sinopse de enchimento, comprida o bastante para ocupar as linhas "
           "que o heroi reserva para ela e empurrar a pilha de meta para a "
           "base da tela, como acontece num titulo de verdade.");
  snprintf(itens[0].classificacao, sizeof itens[0].classificacao, "16");
  snprintf(itens[0].pais, sizeof itens[0].pais, "Brasil");
  // O CHAO DE VERDADE: a arte da obra, que detail.c apaga a 15%. Ver o topo.
  snprintf(itens[0].backdrop, sizeof itens[0].backdrop, "deploy/app/art/03.jpg");
  itens[0].nota = 89;
  for (t = 0; t < 3; t++) itens[0].temporadas[t] = TEMP_NUM[t];
  itens[0].nTemporadas = 3;
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
           "Sinopse de enchimento do filme, comprida o bastante para o bloco de "
           "texto do heroi ficar com a altura que tem num titulo de verdade.");
  snprintf(itens[1].classificacao, sizeof itens[1].classificacao, "14");
  snprintf(itens[1].pais, sizeof itens[1].pais, "United States, Australia");   // Cinemeta: ingles
  snprintf(itens[1].backdrop, sizeof itens[1].backdrop, "deploy/app/art/07.jpg");
  itens[1].nota = 87;
  for (i = 0; i < 6; i++) {
    snprintf(itens[1].elenco[i].nome, sizeof itens[1].elenco[i].nome,
             "Elenco de Ensaio %d", i + 1);
    snprintf(itens[1].elenco[i].papel, sizeof itens[1].elenco[i].papel,
             "Papel %d", i + 1);
  }
  itens[1].nElenco = 6;

  // NUVIO_SHOT_EN=1: a captura do heroi vai para as notas da release em
  // ingles. O titulo passa a ser o da ARTE do pacote (27.jpg e a de Locke &
  // Key), com sinopse curta escrita aqui, em vez de um nome de ensaio em
  // portugues por cima da arte de outra obra.
  { const char *en = getenv("NUVIO_SHOT_EN");
    if (en && *en == '1') {
      snprintf(itens[0].titulo, sizeof itens[0].titulo, "Locke & Key");
      snprintf(itens[0].genero, sizeof itens[0].genero, "TV Show · Drama · Fantasy");
      snprintf(itens[0].meta, sizeof itens[0].meta, "2020 · 3 seasons");
      snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
               "After a family tragedy, three siblings move into their "
               "ancestral home and find magical keys hidden in its walls, "
               "each one unlocking a different power and a darker secret.");
      snprintf(itens[0].classificacao, sizeof itens[0].classificacao, "14");
      snprintf(itens[0].pais, sizeof itens[0].pais, "United States");
      snprintf(itens[0].backdrop, sizeof itens[0].backdrop, "deploy/app/art/27.jpg");
      itens[0].nota = 73;
    } }

  fil.ini = 0; fil.n = 2;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 2, &fil, 1);
  filEnsaio = fil;

  for (t = 0; t < 3; t++) {
    for (i = 0; i < TEMP_N[t]; i++) {
      CatEp *e = &episodios[n++];
      e->temporada = TEMP_NUM[t];
      e->episodio  = i + 1;
      snprintf(e->nome, sizeof e->nome, "Título do episódio %d", i + 1);
      snprintf(e->duracao, sizeof e->duracao, "%d min", 42 + (i % 7));
      snprintf(e->sinopse, sizeof e->sinopse, "%s", SINOPSE_ENSAIO);
      if (t == 2) snprintf(e->data, sizeof e->data, "%s", DATA_T3[i]);
      else snprintf(e->data, sizeof e->data, "%d de março de %d",
                    1 + (i % 28), 2024 + t);
    }
  }
  cat_definir_episodios(0, episodios, n);
}

// --- MAPA DE VISTOS ----------------------------------------------------------
// Pelo modulo REAL. `ate` = quantos episodios da temporada estao marcados como
// vistos; os outros entram como 0 EXPLICITO, que e o que a leitura do Trakt
// faz (vistoep_ler_progresso escreve os dois estados). Nao chamar esta funcao
// para uma serie deixa o mapa em "nao sei", que e o quarto estado e tem
// captura propria.
static void semear(int temporada, int nEps, int ate) {
  int i;
  for (i = 1; i <= nEps; i++)
    vistoep_definir(IMDB_SERIE, temporada, i, i <= ate);
}

// --- CAPTURA -----------------------------------------------------------------

static SDL_Window *janela;
// Janela escondida e desenho num FBO (padrao de explorar_shot): nada aparece
// na tela de quem roda.
static GLuint fbo, fboTex;

static void gravar(const char *nome) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
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
// quadro, e tex.c ainda esta decodificando a arte de fundo. Repetir e o que o
// aparelho faz nos primeiros quadros da tela.
static int parado, comMenu;
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
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    detail_desenhar(SDL_GetTicks());
    // MENU OVER THE PAGE (NV_MENU_SOBRE): the same order as app.c — page,
    // side menu on top, clock island last (always top-right).
    if (comMenu) {
      menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
      if (menu_sobre()) { menu_pilula_mostrar(-1.0f); menu_desenhar(SDL_GetTicks()); }
      ilha_relogio_visivel(1);
      ilha_posicionar(1);
      ilha_desenhar(SDL_GetTicks());
    }
  }
}

// Abre a pagina do item `i` e deixa o foco na TEMPORADA `tc` (coluna da fileira
// de temporadas). detail_atualizar reescreve `temporada` a partir da coluna a
// cada quadro, entao cravar a variavel nao adianta — quem escolhe e o foco.
static void abrir(int i, int tc) {
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
  foco.fileira = SEC_TEMPORADAS;
  foco.coluna = tc;
  quadros(120);
}

// Desce para a fileira de episodios, na coluna pedida.
static void nosEpisodios(int col) {
  foco.fileira = SEC_EPISODIOS;
  foco.coluna = col;
  quadros(120);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-deteps";
  char nome[600];
  SDL_GLContext gl;

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: temporadas e episodios", 0, 0, 64, 64,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
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
  tex_iniciar(16);
  gfx_icones_dir("deploy/app/art");   // o check do card visto e icone (#74)
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  // EM PORTUGUES, e nao no padrao de fabrica. `valor[AJ_IDIOMA]` nasce em 1
  // (ingles) e a captura saia com "Season 2 / EPISODE 9 / Cast & Crew" — o que
  // se precisa julgar aqui e a largura das frases que o dono ve na TV dele, e
  // sao as portuguesas, que sao as compridas ("ainda não exibidos" contra "not
  // aired yet"). Escrever o arquivo e chamar ajustes_dir e o caminho de verdade
  // do modulo; nao ha API para cravar uma opcao de fora.
  { char cam[600]; FILE *f;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w");
    assert(f);
    // NUVIO_SHOT_IDIOMA=N troca o idioma das capturas que nao sao as das notas
    // da release (0 pt ... 7 es; ver idiomacod.h): serve a conferir traducao.
    fprintf(f, "idioma %d\n", getenv("NUVIO_SHOT_IDIOMA") ? atoi(getenv("NUVIO_SHOT_IDIOMA")) : 0);
    // Pular ligado: sem isto nenhuma marca A/R/C aparece (2.1, "Vai tocar").
    fprintf(f, "pularAberturaLocal 0\npularCreditosLocal 0\n");
    if (getenv("NV_APPLE_EP")) fprintf(f,"homeLayoutLocal %d\n",HOME_LAYOUT_DINAMICA);
    fclose(f);
    ajustes_dir(dados_dir()); }

  montarCatalogo();
  if (getenv("NV_APPLE_EP")) {
    semear(1,2,T1_N);
    itens[0].temporada=1; itens[0].episodio=3;
    itens[0].progresso=45; itens[0].restanteMin=24;
    cat_definir_tudo(itens,2,&filEnsaio,1);
    cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
    abrir(0,0); nosEpisodios(0);
    snprintf(nome,sizeof nome,"%s-apple-episodes.png",saida); gravar(nome);
    foco.fileira=SEC_ELENCO; foco.coluna=0; quadros(120);
    snprintf(nome,sizeof nome,"%s-apple-cast.png",saida); gravar(nome);
    HomeItem replacement={0}; replacement.indice=0;
    replacement.rect=(GfxRect){0,0,NV_TELA_W,NV_TELA_H};
    replacement.titulo=itens[0].titulo;
    assert(aberto && !saindo);
    detail_abrir(&replacement); assert(t==1.0f && pg==1.0f);
    carro=0; nivel=0; botao=0; maisAcoes=0;
    assert(acoesAgrupadas() && nBotoes()==2+(temInicio()?1:0));
    assert(acaoEm(1+(temInicio()?1:0))==ACAO_LISTA);
    quadros(120);
    // TextGate fades by wallclock time, unlike the frame-stepped springs.
    for (int step=0;step<20;step++) {SDL_Delay(16);quadros(1);}
    snprintf(nome,sizeof nome,"%s-hero-actions.png",saida); gravar(nome);
    carro=1; carCheia=0; nivel=0; botao=0; maisAcoes=0;
    carN=2; carIdx[0]=0; carIdx[1]=1; carPos=carAplicado=0; carOff=0;
    carEsperaRect=0; cartao=carTxt=1; cartaoVel=carTxtVel=0;
    assert(epTempY() == 1160 && epRowY() == 1286);
    assert(epCardW()==420 && epCardH()==500 && epThumbH()==236);
    for (int step=0;step<25;step++) {SDL_Delay(16);quadros(1);}
    snprintf(nome,sizeof nome,"%s-actions-closed.png",saida); gravar(nome);
    botao=1+(temInicio()?1:0);
    for (int step=0;step<28;step++) {
      SDL_Delay(16);quadros(1);
      if (step==3 || step==10 || step==27) {
        snprintf(nome,sizeof nome,"%s-actions-open-%d.png",saida,step);gravar(nome);
      }
    }
    SDL_Event down = {0}; down.type=SDL_KEYDOWN; down.key.keysym.sym=SDLK_DOWN;
    botao=0; maisAcoes=0; detail_evento(&down); assert(carCheia);
    // CAR_MOLA_PAG=9 settles below2% after0.8s;32frames still leave4.8%.
    for (int step=0;step<48;step++) {SDL_Delay(16);quadros(1);}
    assert(carTxt < .02f && cartao < .02f);
    down.key.keysym.sym=SDLK_LEFT; detail_evento(&down); assert(carPos==0 && !saindo);
    snprintf(nome,sizeof nome,"%s-carousel-expanded-series.png",saida); gravar(nome);
    replacement.indice=1; replacement.titulo=itens[1].titulo;
    detail_abrir(&replacement);
    carro=1; carCheia=1; nivel=0; botao=0; maisAcoes=0;
    carN=2; carIdx[0]=0; carIdx[1]=1; carPos=carAplicado=1; carOff=1;
    carEsperaRect=0; cartao=carTxt=0; cartaoVel=carTxtVel=0;
    quadros(120);
    for (int step=0;step<20;step++) {SDL_Delay(16);quadros(1);}
    snprintf(nome,sizeof nome,"%s-carousel-expanded-film.png",saida); gravar(nome);
    down.key.keysym.sym=SDLK_RIGHT; botao=nBotoes()-1;
    detail_evento(&down); assert(carPos==1 && !saindo);
    // A compact artwork fixture makes the first-screen order reviewable.
    // Unknown logo language keeps the artwork without a duplicate caption.
    snprintf(itens[1].titulo,sizeof itens[1].titulo,"The Devil Wears Prada 2");
    snprintf(itens[1].logo,sizeof itens[1].logo,"deploy/app/art/logo/07.png");
    itens[1].logoIdiomaUrl[0]=itens[1].logoIdioma[0]=0;
    snprintf(itens[1].pais,sizeof itens[1].pais,"Brazil");
    snprintf(itens[1].direcao,sizeof itens[1].direcao,"Diretor de Ensaio");
    cat_definir_tudo(itens,2,&filEnsaio,1);
    detail_abrir(&replacement);
    carro=1; carCheia=1; nivel=0; botao=0; maisAcoes=0;
    cartao=carTxt=0; cartaoVel=carTxtVel=0;
    for (int step=0;step<24;step++) {SDL_Delay(16);quadros(1);}
    snprintf(nome,sizeof nome,"%s-carousel-expanded-film-logo.png",saida); gravar(nome);
    // Stress the measured bottom limit with actual fonts and real wrapping:
    // foreign artwork, a complete long title, hidden large synopsis, resume,
    // country/direction and the received recommendation chip.
    snprintf(itens[1].titulo,sizeof itens[1].titulo,
      "Uma história extraordinariamente longa sobre a viagem de volta para casa e os amigos que encontramos pelo caminho através de mundos desconhecidos");
    snprintf(itens[1].sinopse,sizeof itens[1].sinopse,
      "Durante uma longa viagem de volta para casa, uma família precisa atravessar "
      "cidades desconhecidas e enfrentar escolhas difíceis. Quando antigos amigos "
      "reaparecem com notícias inesperadas, cada encontro revela uma parte esquecida "
      "de sua história. Entre lembranças, descobertas e caminhos que parecem não ter "
      "fim, eles aprendem a encontrar esperança nos pequenos gestos e decidem juntos "
      "qual futuro desejam construir. A jornada transforma o modo como enxergam o "
      "mundo e as pessoas que sempre estiveram ao seu lado.");
    snprintf(itens[1].logo,sizeof itens[1].logo,"deploy/app/art/logo/07.png");
    snprintf(itens[1].logoIdiomaUrl,sizeof itens[1].logoIdiomaUrl,"%s",itens[1].logo);
    snprintf(itens[1].logoIdioma,sizeof itens[1].logoIdioma,"en");
    snprintf(itens[1].pais,sizeof itens[1].pais,"Brazil");
    snprintf(itens[1].direcao,sizeof itens[1].direcao,"Diretor de Ensaio");
    itens[1].progresso=45; itens[1].restanteMin=92;
    cat_definir_tudo(itens,2,&filEnsaio,1);
    fxRecomendacaoLigada=1;
    detail_abrir(&replacement);
    carro=1; carCheia=1; nivel=0; botao=0; maisAcoes=0;
    cartao=carTxt=0; cartaoVel=carTxtVel=0;
    quadros(120);
    for (int step=0;step<20;step++) {SDL_Delay(16);quadros(1);}
    fxTituloEsperado=itens[1].titulo; fxTituloDesenhado=0;
    fxSinopseEsperada=itens[1].sinopse; fxSinopseDesenhada=0;
    quadros(3);
    assert(fxTituloDesenhado>0 && fxSinopseDesenhada>0);
    assert(fxTituloY >= 109);
    assert(fxTituloY > fxPlayY + 24);

    printf("release1.7.4 localized caption after actions at %.0f; synopsis visible\n",fxTituloY);
    snprintf(nome,sizeof nome,"%s-carousel-expanded-film-long.png",saida); gravar(nome);
    puts("PASS: Apple episode/cast, Add reveal, expanded carousel and measured long movie captures"); return 0;
  }
  if (getenv("NUVIO_SHOT_TITULO")) {
    CatFileira fil=filEnsaio;
    snprintf(itens[1].titulo,sizeof itens[1].titulo,"O Senhor dos Anéis: A Sociedade do Anel");
    snprintf(itens[1].logo,sizeof itens[1].logo,"deploy/app/art/logo/01.png");
    snprintf(itens[1].logoIdiomaUrl,sizeof itens[1].logoIdiomaUrl,"%s",itens[1].logo);
    snprintf(itens[1].logoIdioma,sizeof itens[1].logoIdioma,"en");
    cat_definir_tudo(itens,2,&fil,1);
    fxTituloEsperado=NULL;abrir(1,0);nivel=0;SDL_Delay(400);quadros(120);
    fxTituloEsperado=itens[1].titulo;fxTituloDesenhado=0;quadros(3);
    assert(fxTituloDesenhado>0);assert(tex_obter_larg_qualquer(logoDe(idx),NV_DETW_LOGO_MAXW));
    snprintf(nome,sizeof nome,"%s-localizado.png",saida);gravar(nome);
    // Metadata can localize after opening; read the live catalog every frame.
    snprintf(itens[1].titulo,sizeof itens[1].titulo,"Uma história extraordinariamente longa sobre a viagem de volta para casa e os amigos que encontramos pelo caminho através de mundos desconhecidos");
    cat_definir_tudo(itens,2,&fil,1);fxTituloDesenhado=0;quadros(120);
    assert(fxTituloDesenhado>0);
    snprintf(nome,sizeof nome,"%s-longo.png",saida);gravar(nome);
    // A new opening resets the frozen artwork session, like a real title.
    itens[1].logo[0]=0;
    snprintf(itens[1].imdb,sizeof itens[1].imdb,"tt99887766");
    cat_definir_tudo(itens,2,&fil,1);fxTituloEsperado=NULL;
    abrir(1,0);nivel=0;SDL_Delay(400);quadros(120);
    fxTituloEsperado=itens[1].titulo;fxTituloDesenhado=0;quadros(3);
    assert(!logoDe(idx));
    assert(fxTituloDesenhado>0);
    snprintf(nome,sizeof nome,"%s-sem-logo.png",saida);gravar(nome);
    puts("detail title: live localized caption, complete wrapping and no-logo rendering passed");
    return 0;
  }

  // --- MENU OVER THE TITLE PAGE (owner 03/10, "pode abrir por cima"): Left
  //     on the first episode of S1 asks for the side menu; it opens ON TOP and
  //     the page keeps its focus/scroll; Right hands the focus back.
  //     NV_MENU_SOBRE=1 bash tests/detail_eps_shot.sh /Volumes/ExternalSSD/nv-ui-w14-shots/det
  if (getenv("NV_MENU_SOBRE")) {
    SDL_Event ev = {0};
    int f0, c0; float s0;
    semear(1, T1_N, 3);
    progVis = 3; progExib = T1_N + T2_N + (T3_PROX_EP - 1);
    abrir(0, 0); nosEpisodios(0);
    for (int step = 0; step < 20; step++) { SDL_Delay(16); quadros(1); }
    f0 = foco.fileira; c0 = foco.coluna; s0 = scrollY;
    ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_LEFT;
    detail_evento(&ev);
    assert(!saindo && detail_pediu_menu());
    comMenu = 1;
    menu_abrir_sobre(1);
    detail_sob_menu(1);
    for (int step = 0; step < 6; step++) { SDL_Delay(16); quadros(1); }
    snprintf(nome, sizeof nome, "%s-menu-sobre-abrindo.png", saida); gravar(nome);
    for (int step = 0; step < 40; step++) { SDL_Delay(16); quadros(1); }
    snprintf(nome, sizeof nome, "%s-menu-sobre-aberto.png", saida); gravar(nome);
    assert(aberto && !saindo && foco.fileira == f0 && foco.coluna == c0 && scrollY == s0);
    ev.key.keysym.sym = SDLK_RIGHT; menu_evento(&ev);
    assert(!menu_aberto() && !menu_escolheu());
    detail_sob_menu(0);
    for (int step = 0; step < 50; step++) { SDL_Delay(16); quadros(1); }
    assert(!menu_sobre());
    snprintf(nome, sizeof nome, "%s-menu-sobre-voltou.png", saida); gravar(nome);
    assert(aberto && !saindo && foco.fileira == f0 && foco.coluna == c0 && scrollY == s0);
    puts("menu over detail: page kept alive under the menu, Right returns");
    return 0;
  }

  // --- 1. HISTORICO NAO CHEGOU. Nada foi semeado: vistoep_estado devolve -1
  //        em tudo. Nenhum check, e a temporada nao pode afirmar "0 vistos".
  progVis = progExib = 0;
  abrir(0, 0);
  snprintf(nome, sizeof nome, "%s-1-t1-sem-historico.png", saida);
  gravar(nome);

  // --- 2. NADA VISTO, mas SABIDO: o mapa existe e diz 0. E outro estado.
  semear(1, T1_N, 0);
  semear(2, T2_N, 0);
  semear(3, T3_N, 0);
  progVis = 0; progExib = T1_N + T2_N + (T3_PROX_EP - 1);
  abrir(0, 0);
  snprintf(nome, sizeof nome, "%s-2-t1-nada-visto.png", saida);
  gravar(nome);

  // --- 3. TEMPORADA INTEIRA VISTA.
  semear(1, T1_N, T1_N);
  progVis = T1_N; progExib = T1_N + T2_N + (T3_PROX_EP - 1);
  abrir(0, 0);
  snprintf(nome, sizeof nome, "%s-3-t1-tudo-visto.png", saida);
  gravar(nome);

  // --- 4. A TEMPORADA COMPRIDA E EM EXIBICAO, com o foco NA TEMPORADA: a
  //        linha acima das pilulas tem de dar as tres contagens de uma vez.
  semear(3, T3_N, T3_PROX_EP - 1);      // tudo o que ja foi ao ar, visto
  progVis = T1_N + (T3_PROX_EP - 1); progExib = T1_N + T2_N + (T3_PROX_EP - 1);
  abrir(0, 2);
  snprintf(nome, sizeof nome, "%s-4-t3-temporada-em-exibicao.png", saida);
  gravar(nome);

  // --- 5. A MESMA T3 com o foco NA FRONTEIRA: o ultimo exibido e o primeiro
  //        que ainda nao foi, lado a lado. E a comparacao que decide se a
  //        marca funciona — um card so nunca prova diferenca.
  nosEpisodios(T3_PROX_EP - 2);
  snprintf(nome, sizeof nome, "%s-5-t3-fronteira-exibido-e-nao.png", saida);
  gravar(nome);

  // --- 6. E o primeiro NAO EXIBIDO em FOCO: o anel branco por cima de um card
  //        que a pessoa nao pode abrir e o caso mais perigoso do lote.
  nosEpisodios(T3_PROX_EP - 1);
  snprintf(nome, sizeof nome, "%s-6-t3-nao-exibido-focado.png", saida);
  gravar(nome);

  // --- 7. AGENDA AUSENTE (o TMDB nao respondeu, ou a serie nao tem proximo
  //        episodio publicado). A pagina NAO pode adivinhar: os mesmos cards
  //        voltam a ser episodios comuns, e e o certo — afirmar "nao exibido"
  //        sem fonte e o defeito que PRODUCT.md proibe.
  agendaLigada = 0;
  abrir(0, 2);
  nosEpisodios(T3_PROX_EP - 2);
  snprintf(nome, sizeof nome, "%s-7-t3-sem-agenda.png", saida);
  gravar(nome);
  agendaLigada = 1;

  // --- 8. T2, temporada INTEIRA no ar e vista pela metade: o caso comum, sem
  //        nenhuma clausula de "nao exibido".
  semear(2, T2_N, 5);
  progVis = T1_N + 5 + (T3_PROX_EP - 1);
  abrir(0, 1);
  snprintf(nome, sizeof nome, "%s-8-t2-metade-vista.png", saida);
  gravar(nome);

  // --- 9. A T3 no MEIO da fileira: com 24 cards ela rola de verdade, e o
  //        check de visto e a marca de estreia tem de acompanhar a rolagem.
  abrir(0, 2);
  nosEpisodios(15);
  snprintf(nome, sizeof nome, "%s-9-t3-meio-da-fileira.png", saida);
  gravar(nome);

  // --- 10. FILME: nem temporada nem episodio. As duas fileiras somem e a
  //         pagina empilha as secoes proprias do filme.
  abrir(1, 0);
  foco.fileira = SEC_ELENCO; foco.coluna = 0;
  quadros(120);
  snprintf(nome, sizeof nome, "%s-10-filme.png", saida);
  gravar(nome);

  // --- 13. SERIE COM TRAILERS (#123): a fileira empilhada abaixo do elenco,
  //         com o cabecalho "Trailers", e o foco no primeiro card.
  trailersLigados = 1;
  abrir(0, 2);
  foco.fileira = SEC_TRAILERS; foco.coluna = 0;
  quadros(150);
  snprintf(nome, sizeof nome, "%s-13-serie-trailers.png", saida);
  gravar(nome);
  // --- 14. A MESMA serie de volta nos episodios: temporadas e episodios no
  //         lugar de sempre com a fileira nova la embaixo.
  nosEpisodios(3);
  snprintf(nome, sizeof nome, "%s-14-serie-trailers-episodios.png", saida);
  gravar(nome);
  // --- 15. FILME COM TRAILERS, foco no SEGUNDO card: o anel de realce tem de
  //         aparecer so nele, e o primeiro fica em repouso.
  abrir(1, 0);
  foco.fileira = SEC_TRAILERS; foco.coluna = 1;
  quadros(150);
  snprintf(nome, sizeof nome, "%s-15-filme-trailers.png", saida);
  gravar(nome);
  trailersLigados = 0;

  // --- 11. BOTOES DO HERO NA COR DE REALCE (20/09/2026: "nenhum botao ta
  //         ficando com a cor do accent"). Violeta e ESCURO: a tinta sobre ele
  //         tem de sair BRANCA; no 12, carmesim claro, tinta escura.
  { char cam[600]; FILE *f;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "idioma 1\nselected_theme 3\n"); fclose(f);   // em ingles: e a captura das notas da release
    ajustes_dir(dados_dir()); }
  abrir(0, 0);
  nivel = 0; botao = 0; quadros(120);
  snprintf(nome, sizeof nome, "%s-11-hero-violeta-primario.png", saida);
  gravar(nome);
  botao = 1; quadros(120);
  snprintf(nome, sizeof nome, "%s-11-hero-violeta-circular.png", saida);
  gravar(nome);
  { char cam[600]; FILE *f;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    f = fopen(cam, "w"); assert(f);
    fprintf(f, "idioma 1\nselected_theme 7\n"); fclose(f);
    ajustes_dir(dados_dir()); }
  botao = 0; quadros(60);
  snprintf(nome, sizeof nome, "%s-12-hero-dourado-primario.png", saida);
  gravar(nome);

  // --- 20. DADOS A CAMINHO (#172): sinopse e meta ainda nao chegaram. Esperado:
  //         20a = quadro 1, hero escondido pelo portao do texto; 20b = esqueleto
  //         das barras (sinopse em 3 linhas e o meta) reservando a altura; 20c =
  //         o texto chegou ha ~90 ms (meio da troca); 20d = assentado.
  { char sin0[900], gen0[160], meta0[160];
    snprintf(sin0, sizeof sin0, "%s", itens[1].sinopse);
    snprintf(gen0, sizeof gen0, "%s", itens[1].genero);
    snprintf(meta0, sizeof meta0, "%s", itens[1].meta);
    itens[1].sinopse[0] = itens[1].genero[0] = itens[1].meta[0] = 0;
    itens[1].nota = 0;
    cat_definir_tudo(itens, 2, &filEnsaio, 1);
    fxCarregandoLigado = 1;
    abrir(1, 0);
    nivel = 0; botao = 0;
    quadros(1);
    snprintf(nome, sizeof nome, "%s-20a-dados-portao.png", saida);
    gravar(nome);
    quadros(90); SDL_Delay(400); quadros(30);
    snprintf(nome, sizeof nome, "%s-20b-dados-esqueleto.png", saida);
    gravar(nome);
    snprintf(itens[1].sinopse, sizeof itens[1].sinopse, "%s", sin0);
    snprintf(itens[1].genero, sizeof itens[1].genero, "%s", gen0);
    snprintf(itens[1].meta, sizeof itens[1].meta, "%s", meta0);
    itens[1].nota = 87;
    cat_definir_tudo(itens, 2, &filEnsaio, 1);
    quadros(1); SDL_Delay(90); quadros(1);
    snprintf(nome, sizeof nome, "%s-20c-dados-troca.png", saida);
    gravar(nome);
    fxCarregandoLigado = 0;
    SDL_Delay(400); quadros(30);
    snprintf(nome, sizeof nome, "%s-20d-dados-assentado.png", saida);
    gravar(nome); }

  // --- 16/17. TRAILERS EM INGLES com realce carmesim (tema 7): o anel segue a
  //            cor escolhida, nao o branco de fabrica.
  trailersLigados = 1;
  abrir(0, 2);
  foco.fileira = SEC_TRAILERS; foco.coluna = 0;
  quadros(150);
  snprintf(nome, sizeof nome, "%s-16-serie-trailers-en.png", saida);
  gravar(nome);
  abrir(1, 0);
  foco.fileira = SEC_TRAILERS; foco.coluna = 1;
  quadros(150);
  snprintf(nome, sizeof nome, "%s-17-filme-trailers-en.png", saida);
  gravar(nome);
  trailersLigados = 0;

  // --- 18/19. DESFOCAR NAO ASSISTIDOS (#133). O ajuste existia na tela e na
  //            conta e NADA o lia: o card saia nitido em toda plataforma. A T2
  //            ganha um still diferente por episodio (o espiao identifica o
  //            card pela arte) e fica vista ate o E5, como no passo 8.
  //            18 = ajuste desligado: nada pedido desfocado, cards nitidos.
  //            19 = ligado: E1..E5 (vistos, com check) nitidos; do E6 em
  //                 diante desfocados.
  { int t2ini = T1_N, k;
    static char stills[T2_N][64];
    for (k = 0; k < T2_N; k++) {
      snprintf(stills[k], sizeof stills[k], "deploy/app/art/ep/%s_1_%02d.jpg",
               k < 8 ? "00" : "01", k < 8 ? k + 1 : k - 7);
      snprintf(episodios[t2ini + k].thumb, sizeof episodios[0].thumb, "%s", stills[k]);
    }
    cat_definir_episodios(0, episodios, T1_N + T2_N + T3_N);
    semear(2, T2_N, 5);
    { char cam[600]; FILE *f;
      snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
      f = fopen(cam, "w"); assert(f);
      fprintf(f, "idioma %d\nblurUnwatchedEpisodes 1\n", getenv("NUVIO_SHOT_IDIOMA") ? atoi(getenv("NUVIO_SHOT_IDIOMA")) : 0); fclose(f);
      ajustes_dir(dados_dir()); }
    assert(!ajustes_desfocar_nao_assistidos());
    fxDesfN = fxDesfOk = 0;
    abrir(0, 1);
    nosEpisodios(4);
    snprintf(nome, sizeof nome, "%s-18-t2-desfoque-desligado.png", saida);
    gravar(nome);
    assert(fxDesfN == 0);

    { char cam[600]; FILE *f;
      snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
      f = fopen(cam, "w"); assert(f);
      fprintf(f, "idioma %d\nblurUnwatchedEpisodes 0\n", getenv("NUVIO_SHOT_IDIOMA") ? atoi(getenv("NUVIO_SHOT_IDIOMA")) : 0); fclose(f);
      ajustes_dir(dados_dir()); }
    assert(ajustes_desfocar_nao_assistidos());
    fxDesfN = fxDesfOk = 0;
    abrir(0, 1);
    nosEpisodios(4);
    snprintf(nome, sizeof nome, "%s-19-t2-desfoque-nao-assistidos.png", saida);
    gravar(nome);
    // Vistos: nunca pedidos desfocados.
    for (k = 0; k < 5; k++)
      if (fxDesfPediu(stills[k])) {
        printf("FALHA: E%d visto foi desfocado\n", k + 1);
        return 1;
      }
    // Nao vistos na tela (o foco no E5 deixa E6 e E7 a direita): desfocados.
    if (!fxDesfPediu(stills[5]) || !fxDesfPediu(stills[6])) {
      printf("FALHA: E6/E7 nao vistos sairam nitidos (%d artes pedidas)\n", fxDesfN);
      return 1;
    }
    // E a copia existe de fato no GL, nao so o pedido.
    if (fxDesfOk == 0) { printf("FALHA: gfx_desfocado nunca devolveu textura\n"); return 1; }
    printf("desfoque: %d artes desfocadas, %d desenhos com a copia pronta\n",
           fxDesfN, fxDesfOk);
  }

#ifdef NV_SHOT_HOOKS
  // --- 21. 2.1 "Vai tocar": a faixa embaixo dos botoes (fonte que o automatico
  //         tocaria + o que da para pular e de onde vem) e os chips A/R/C nos
  //         cards dos episodios cujas marcas ja estao em memoria. Sem lista de
  //         fontes a faixa nao existe; com ela, a altura reservada acompanha.
  {
    static Stream lv[3];
    IntroTrecho tr[2];
    int t, e, k;
    char alvo[64];
    const char *ARQ = "Breaking.Bad.S01E01.2160p.WEB-DL.DV.HDR.EAC3.Atmos.mkv";
    badges_carregar("deploy/app/art");
    parado = 0;
    abrir(0, 0);
    quadros(20);
    assert(!vaiTocar.tem && hVaiVis < 0.5f);   // sem lista de fontes: sem faixa
    t = vaiTocarT; e = vaiTocarE;
    for (k = 0; k < 3; k++) {
      memset(&lv[k], 0, sizeof lv[k]);
      snprintf(lv[k].provedor, sizeof lv[k].provedor, k == 1 ? "AIOStreams" : "Torrentio");
      snprintf(lv[k].rotulo, sizeof lv[k].rotulo, "%s", k == 2 ? "1080p WEB-DL" : "4K DV HDR");
      snprintf(lv[k].arquivo, sizeof lv[k].arquivo, "%s", k == 2 ? "Breaking.Bad.S01E01.1080p.WEB-DL.mkv" : ARQ);
      snprintf(lv[k].descricao, sizeof lv[k].descricao, "%s", lv[k].rotulo);
      snprintf(lv[k].url, sizeof lv[k].url, "https://exemplo.invalido/%d.mkv", k);
      lv[k].altura = k == 2 ? 1080 : 2160; lv[k].fileIdx = -1;
      lv[k].tamanhoMB = k == 2 ? 4096 : 11264; lv[k].tamanhoBytes = (uint64_t)lv[k].tamanhoMB << 20;
      lv[k].dolbyVision = k != 2;
      { char tx[800]; snprintf(tx, sizeof tx, "%s %s", lv[k].rotulo, lv[k].arquivo);
        lv[k].badges = badges_detectar(tx); }
    }
    if (t > 0 && e > 0) snprintf(alvo, sizeof alvo, "%s:%d:%d", IMDB_SERIE, t, e);
    else snprintf(alvo, sizeof alvo, "%s", IMDB_SERIE);
    stream_definir_alvo(alvo);
    stream_definir_lista(lv, 3);
    memset(tr, 0, sizeof tr);
    tr[0].tipo = INTRO_ABERTURA; tr[0].inicio = 5; tr[0].fim = 95;
    tr[1].tipo = INTRO_CREDITOS; tr[1].inicio = 2700; tr[1].fim = 2820;
    intro_shot_marcas(IMDB_SERIE, t, e, tr, 2, 0);
    intro_shot_marcas(IMDB_SERIE, t, e + 1, tr, 1, 0);   // so a abertura
    abrir(0, 0);
    quadros(120);
    { int k; for (k = 0; k < 40; k++) { SDL_Delay(25); quadros(1); } }  // a faixa anima em tempo real   // a altura reservada anda por relogio real (90 ms)
    assert(vaiTocar.tem && vaiTocar.pularOn);
    assert(fabsf(hVaiVis - (vtui_altura(&vaiTocar) + DET_VAI_GAP)) < 1.0f);
    assert(vaiTocar.marcas == (VT_T_A | VT_T_C) && vaiTocar.origem == VT_O_THEINTRODB);
    assert(!strcmp(vaiTocar.res, "4K"));
    snprintf(nome, sizeof nome, "%s-21-vai-tocar.png", saida);
    gravar(nome);
    nosEpisodios(0);
    snprintf(nome, sizeof nome, "%s-21-vai-tocar-episodios.png", saida);
    gravar(nome);
    printf("vai tocar: %s %s, altura da faixa %.0f\n", vaiTocar.res, vaiTocar.hdr, hVaiVis);
  }
#endif

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
