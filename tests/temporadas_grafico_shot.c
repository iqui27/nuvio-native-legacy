// CAPTURA DO GRAFICO DE TEMPORADAS ("Seu progresso") NA PAGINA DE SERIE, sem
// rede: a pagina de verdade (detail.c incluido aqui), com o catalogo, o mapa
// do vistoep e os amigos semeados por este arquivo.
//
//   bash tests/temporadas_grafico_shot.sh /tmp/nuvio-tgraf
//   NUVIO_SHOT_FONTE=3 bash tests/temporadas_grafico_shot.sh /tmp/nuvio-tgraf   # fonte da TV
//
// Tres quadros:
//   -parcial-foco.png   serie pela metade, foco na T3: a lista dos amigos;
//   -parcial.png        a mesma, sem o foco no grafico (rostos em fila);
//   -completa.png       serie vista inteira, amigo atras.
// E confere o gesto: OK numa coluna leva a fileira de episodios para aquela
// temporada, no primeiro episodio nao visto.
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Sem rede: o pedido de extras e o indice social real nao rodam. A agenda (o
// proximo episodio a ir ao ar) vem daqui, para o tracejado do que nao estreou.
#define extras_pedir            fx_pedir
#define extras_carregando       fx_carregando
#define extras_agenda_temporada fx_agenda_t
#define extras_agenda_episodio  fx_agenda_e
#define amigostitulo_atualizar  fx_amg_atualizar
void fx_pedir(const char *imdb, int serie, long tmdbId) { (void)imdb; (void)serie; (void)tmdbId; }
int  fx_carregando(void) { return 0; }
static int agT, agE;
int  fx_agenda_t(void) { return agT; }
int  fx_agenda_e(void) { return agE; }
void fx_amg_atualizar(void) {}

#include "../src/detail.c"
#include "../src/temporadas_grafico.c"
#include "dados.h"
#include "vistoep.h"

#define IMDB_PARCIAL  "tt7000001"
#define IMDB_COMPLETA "tt7000002"

static CatItem itens[2];
static CatEp   episodios[64];
static const int EPS_PARCIAL[] = { 10, 10, 10, 8, 8 };
static const int EPS_COMPLETA[] = { 6, 8, 8 };

static void serie(int i, const char *imdb, const char *titulo, const char *arte,
                  const int *eps, int nt) {
  int t;
  snprintf(itens[i].titulo, sizeof itens[i].titulo, "%s", titulo);
  snprintf(itens[i].imdb, sizeof itens[i].imdb, "%s", imdb);
  snprintf(itens[i].tipo, sizeof itens[i].tipo, "series");
  snprintf(itens[i].genero, sizeof itens[i].genero, "Programa de TV · Drama");
  snprintf(itens[i].meta, sizeof itens[i].meta, "2019 · %d temporadas", nt);
  snprintf(itens[i].sinopse, sizeof itens[i].sinopse,
           "Sinopse de enchimento, comprida o bastante para ocupar as linhas do "
           "heroi como num titulo de verdade.");
  snprintf(itens[i].backdrop, sizeof itens[i].backdrop, "%s", arte);
  itens[i].nota = 84;
  for (t = 0; t < nt; t++) itens[i].temporadas[t] = t + 1;
  itens[i].nTemporadas = nt;
  (void)eps;
}

static int montarEpisodios(const int *eps, int nt) {
  int t, e, n = 0;
  for (t = 0; t < nt; t++)
    for (e = 1; e <= eps[t]; e++) {
      memset(&episodios[n], 0, sizeof episodios[n]);
      episodios[n].temporada = t + 1; episodios[n].episodio = e;
      snprintf(episodios[n].nome, sizeof episodios[n].nome, "Episódio de ensaio %d", e);
      snprintf(episodios[n].duracao, sizeof episodios[n].duracao, "47 min");
      n++;
    }
  return n;
}

static void montarCatalogo(void) {
  CatFileira fil;
  memset(itens, 0, sizeof itens);
  memset(&fil, 0, sizeof fil);
  serie(0, IMDB_PARCIAL, "Série pela Metade", "deploy/app/art/03.jpg", EPS_PARCIAL, 5);
  serie(1, IMDB_COMPLETA, "Série Completa", "deploy/app/art/07.jpg", EPS_COMPLETA, 3);
  fil.ini = 0; fil.n = 2;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 2, &fil, 1);
}

static SvEvento evento(const char *pid, const char *nome, int acao, int reacao,
                       const char *imdb, int t, int e) {
  SvEvento v;
  memset(&v, 0, sizeof v);
  snprintf(v.pessoaId, sizeof v.pessoaId, "%s", pid);
  snprintf(v.pessoaNome, sizeof v.pessoaNome, "%s", nome);
  v.acao = acao; v.reacao = reacao;
  snprintf(v.imdb, sizeof v.imdb, "%s", imdb);
  snprintf(v.tipo, sizeof v.tipo, "series");
  v.temporada = t; v.episodio = e; v.pct = -1; v.restanteMin = -1;
  v.quando = (long long)time(NULL) - 3600;
  return v;
}

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
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

static int parado;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    tex_bombear(3);
    if (!parado) detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    detail_desenhar(SDL_GetTicks());
    if (i < n - 1) SDL_GL_SwapWindow(janela);
    SDL_Delay(8);
  }
}

static void abrir(int i, const int *eps, int nt) {
  HomeItem hi;
  memset(&hi, 0, sizeof hi);
  cat_definir_episodios(i, episodios, montarEpisodios(eps, nt));
  hi.indice = i;
  hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
  hi.titulo = itens[i].titulo;
  hi.genero = itens[i].genero;
  hi.meta = itens[i].meta;
  parado = 0;
  detail_abrir(&hi);
  nivel = 1;
  foco.fileira = SEC_EPISODIOS; foco.coluna = 0;
  quadros(20);
}

static void tecla(SDL_Keycode k, int up) {
  SDL_Event ev;
  memset(&ev, 0, sizeof ev);
  ev.type = up ? SDL_KEYUP : SDL_KEYDOWN;
  ev.key.keysym.sym = k;
  detail_evento(&ev);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-tgraf";
  char nome[600];
  SDL_GLContext gl;
  const char *dd;
  SvEvento ev[16];
  int n = 0, t, e;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: grafico de temporadas", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  { char ic[1024]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  dd = dados_dir();
  if (!dd || !strstr(dd, "nuvio-tgraf-dados")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste (\"%s\")\n",
            dd ? dd : "");
    return 1;
  }
  { char caminho[600]; FILE *f;
    const char *fonte = getenv("NUVIO_SHOT_FONTE");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dd);
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\n");
    if (fonte) fprintf(f, "fonteInterface %d\n", atoi(fonte));
    fclose(f);
    ajustes_dir(dd); }

  montarCatalogo();

  // VOCE: T1 e T2 inteiras, T3 ate o E6. A T5 estreia o E5 na semana que vem.
  for (t = 1; t <= 2; t++) for (e = 1; e <= 10; e++) vistoep_definir(IMDB_PARCIAL, t, e, 1);
  for (e = 1; e <= 6; e++) vistoep_definir(IMDB_PARCIAL, 3, e, 1);
  vistoep_definir(IMDB_PARCIAL, 3, 7, 0);
  // A completa: tudo visto.
  for (t = 0; t < 3; t++) for (e = 1; e <= EPS_COMPLETA[t]; e++)
    vistoep_definir(IMDB_COMPLETA, t + 1, e, 1);

  // OS AMIGOS: tres na frente (um gostou, um vendo agora), dois atras.
  ev[n++] = evento("nuvio:ana", "Ana Lima", SV_FIM, SV_REAC_NADA, IMDB_PARCIAL, 4, 2);
  ev[n++] = evento("nuvio:rafa", "Rafa Cine", SV_AGORA, SV_REAC_NADA, IMDB_PARCIAL, 3, 9);
  ev[n++] = evento("nuvio:mari", "Mari", SV_FIM, SV_REAC_NADA, IMDB_PARCIAL, 5, 3);
  ev[n++] = evento("nuvio:mari", "Mari", SV_REACAO, SV_REAC_GOSTOU, IMDB_PARCIAL, 0, 0);
  ev[n++] = evento("nuvio:bia", "Bia Souza", SV_FIM, SV_REAC_NADA, IMDB_PARCIAL, 2, 4);
  ev[n++] = evento("nuvio:caio", "Caio", SV_FIM, SV_REAC_NADA, IMDB_PARCIAL, 3, 3);
  ev[n++] = evento("nuvio:caio", "Caio", SV_REACAO, SV_REAC_MEIO, IMDB_PARCIAL, 0, 0);
  ev[n++] = evento("nuvio:bia", "Bia Souza", SV_FIM, SV_REAC_NADA, IMDB_COMPLETA, 2, 5);
  amigostitulo_montar(ev, n);

  // --- PARCIAL, foco na T3 --------------------------------------------------
  agT = 5; agE = 5;
  abrir(0, EPS_PARCIAL, 5);
  { const TgDados *d = tgraf_dados(idx);
    assert(tgraf_existe(d) && d->n == 5 && d->nFrente == 3 && d->nAtras == 2);
    assert(d->t[4].exibidos == 4 && d->t[4].total == 8);
    assert(secaoN(SEC_PROGTEMP) == 5);
    assert(topoSec[SEC_PROGTEMP] > conteudoSec[SEC_EPISODIOS]); }
  tecla(SDLK_DOWN, 0);
  assert(foco.fileira == SEC_PROGTEMP);
  // Entra pela temporada escolhida na pagina (a T1 aqui); anda ate a T3.
  tecla(SDLK_RIGHT, 0); tecla(SDLK_RIGHT, 0);
  assert(foco.coluna == 2);
  quadros(90);
  snprintf(nome, sizeof nome, "%s-parcial-foco.png", saida);
  gravar(nome);
  // Sem o foco no grafico, mesma rolagem.
  parado = 1;
  foco.fileira = SEC_EPISODIOS;
  quadros(6);
  snprintf(nome, sizeof nome, "%s-parcial.png", saida);
  gravar(nome);
  parado = 0;
  foco.fileira = SEC_PROGTEMP; foco.coluna = 2;
  quadros(4);
  // OK na T3: a fileira de episodios vai para a T3, no E7 (o primeiro nao visto).
  tecla(SDLK_RETURN, 0); tecla(SDLK_RETURN, 1);
  assert(foco.fileira == SEC_EPISODIOS && temporadaEm(temporada) == 3);
  { const CatEp *ep = cat_episodio(idx, epAbsoluto(foco.coluna));
    assert(ep && ep->temporada == 3 && ep->episodio == 7); }
  quadros(10);

  // --- COMPLETA -------------------------------------------------------------
  agT = 0; agE = 0;
  abrir(1, EPS_COMPLETA, 3);
  { const TgDados *d = tgraf_dados(idx);
    assert(d->completas == 3 && d->nAmg == 1 && d->nFrente == 0); }
  tecla(SDLK_DOWN, 0);
  assert(foco.fileira == SEC_PROGTEMP);
  parado = 0;
  quadros(60);
  parado = 1;
  foco.fileira = SEC_EPISODIOS;
  quadros(6);
  snprintf(nome, sizeof nome, "%s-completa.png", saida);
  gravar(nome);

  puts("PASS: temporadas_grafico_shot");
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return 0;
}
