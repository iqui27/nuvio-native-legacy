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
#define extras_nota_trakt        fx_nota_trakt
#define extras_ep_visto          fx_ep_visto
#define extras_progresso_pronto  fx_progresso_pronto
#define extras_progresso_serie   fx_progresso_serie
#define extras_proximo_episodio  fx_proximo_episodio
#define extras_agenda_temporada  fx_agenda_temporada
#define extras_agenda_episodio   fx_agenda_episodio
#define extras_agenda_data       fx_agenda_data
#define extras_agenda_status     fx_agenda_status

#include "../src/detail.c"

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
int fx_carregando(void)       { return 0; }
int fx_n_comentarios(void)    { return 0; }
int fx_n_comentarios_ep(void) { return 0; }
int fx_n_relacionados(void)   { return 0; }
int fx_n_colecao(void)        { return 0; }
int fx_n_estudios(void)       { return 0; }
int fx_n_trailers(void)       { return 0; }
int fx_nota_trakt(void)       { return 82; }

static int ehSerieDeEnsaio(void) {
  const CatItem *ci = cat_item(idx);
  return ci && !strcmp(ci->imdb, IMDB_SERIE);
}

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
  snprintf(itens[1].genero, sizeof itens[1].genero, "Filme · Ficção científica");
  snprintf(itens[1].meta, sizeof itens[1].meta, "1999 · 2 h 16 min");
  snprintf(itens[1].sinopse, sizeof itens[1].sinopse,
           "Sinopse de enchimento do filme, comprida o bastante para o bloco de "
           "texto do heroi ficar com a altura que tem num titulo de verdade.");
  snprintf(itens[1].classificacao, sizeof itens[1].classificacao, "14");
  snprintf(itens[1].pais, sizeof itens[1].pais, "Estados Unidos");
  snprintf(itens[1].backdrop, sizeof itens[1].backdrop, "deploy/app/art/07.jpg");
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
// quadro, e tex.c ainda esta decodificando a arte de fundo. Repetir e o que o
// aparelho faz nos primeiros quadros da tela.
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

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: temporadas e episodios",
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
    fprintf(f, "idioma 0\n");
    fclose(f);
    ajustes_dir(dados_dir()); }

  montarCatalogo();

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

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
