// O MENU DO EPISODIO, O PAINEL DA TEMPORADA E O SELO DE NOTA na pagina de
// titulo (dono, 08/10/2026), provados pela pagina de verdade (detail.c
// incluido, como em tests/detail_eps_shot.c) e sem rede.
//
// O QUE SE COBRA, e por onde se observa:
//   A. segurar OK num card de episodio abre o menu de visto NA ILHA DO MENU DO
//      CARTAZ (ctxmenu.c): linhas de 60 a cada 64, ilha de 420 a 600 de
//      largura, AO LADO do card (nao por cima dele). Voltar fecha e o foco fica
//      no card. As linhas sao alvos do ponteiro: clicar numa aplica.
//   B. segurar OK numa ABA de temporada nao abre modal no meio: a aba se abre
//      num painel que nasce NELA (canto da aba, para baixo), sem veu de tela
//      cheia. Com animacoes reduzidas o painel esta pronto no quadro seguinte;
//      sem elas, ainda esta a caminho. Voltar e clicar fora fecham, o foco
//      continua na aba; clicar numa linha marca a temporada.
//   C. o selo de nota do card nao escreve mais "Trakt"/"TMDB": pede o logo
//      (art/marcas) e o numero sai em negrito.
//
// A geometria e lida da LISTA DE ALVOS DO PONTEIRO (ponteiro_teste_lista), que
// e o que a tela registra de verdade a cada quadro — o teste nao conhece as
// medidas do menu, so as do padrao que ele tem de seguir.
//
//   bash tests/menuep.sh [prefixo-das-capturas]
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- extras.h sem rede (a receita de tests/detail_eps_shot.c) ----------------
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
#define recomenda_ativo          fx_recomenda_ativo

// --- ESPIAO DO TEXTO: o que detail.c manda escrever, e em que estilo ---------
#include "text.h"
static int espRotuloFonte;        // txt_linha("Trakt" | "TMDB") vindos da pagina
static int espNotaEstilo = -1;    // estilo em que a nota de ensaio saiu
static int espImdb;
static const char *espNota;       // a nota que se procura ("7,4")
static TxtLinha fx_txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  if (s && (!strcmp(s, "Trakt") || !strcmp(s, "TMDB"))) espRotuloFonte++;
  if (s && !strcmp(s, "IMDb")) espImdb++;
  if (s && espNota && (!strcmp(s, espNota) || (!strcmp(espNota,"7,6") && !strcmp(s,"7.6")))) espNotaEstilo = (int)estilo;
  return txt_linha(estilo, s, r, g, b, a);
}
#define txt_linha fx_txt_linha
#include "../src/detail.c"
#undef txt_linha
#undef recomenda_ativo
#include "dados.h"
#include "ponteiro.h"

#ifdef NV_TEST_EPCOMENT
#undef extras_n_comentarios_ep
int extras_n_comentarios_ep(void);
#include "js.h"
#include "epcoment.h"
#include "rede.h"
static int epRequests, epRatings, epEmpty, epImdbLabels, epImdbScores, epPaged;
static int epBarsCheck, epColoredBars, epBarRows;
void epreview_color(GfxRect rect,float radius,float r,float g,float b,float a) {
  if (epBarsCheck && rect.h==12 && a>0) {
    assert(radius==.5f);
    if (a<.1f) epBarRows++;
    else if (r!=.65f || g!=.67f || b!=.71f) epColoredBars++;
  }
  gfx_cor(rect,radius,r,g,b,a);
}
static void checkBars(int selected) {
  epBarsCheck=1; epColoredBars=epBarRows=0;
  epcoment_desenhar((GfxRect){1000,350,676,624},1);
  epBarsCheck=0;
  assert(epBarRows==6 && epColoredBars==selected);
}
char *epcoment_baixar(const char *,int,const char *const *);
TxtLinha epreview_texto(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  if (s && !strncmp(s,"IMDb",4)) epImdbLabels++;
  if (s && (strstr(s,"8,7") || strstr(s,"8.7"))) epImdbScores++;
  return txt_linha(estilo, s, r, g, b, a);
}
// The rating row draws IMDb as its badge (notasui.c is built with this hook).
float epreview_badge_imdb(float x, float y, int nota, int escuro, float a) {
  if (nota == 87) epImdbScores++;
  return badge_imdb(x, y, nota, escuro, a);
}
static SDL_atomic_t epRelease;
static int epMode, epPages[8], epTmdbRequests;
static char *epFixture(const char *url) {
  if (strstr(url,"themoviedb.org")) {
    epTmdbRequests++;
    if (strstr(url,"/find/")) return strdup("{\"tv_results\":[{\"id\":42}]}");
    return strdup("{\"page\":1,\"total_pages\":1,\"results\":[{\"id\":\"tm1\",\"author\":\"Series reviewer\",\"content\":\"A review of the whole series, not this episode.\",\"author_details\":{\"rating\":8},\"created_at\":\"2026-01-01\"}]}");
  }
  if (!strstr(url,"/comments/")) return epcoment_baixar(url,12,NULL);
  const char *param=strstr(url,"page="); int page=param?atoi(param+5):0;
  if (page>=0 && page<8) epPages[page]++;
  if (epMode==2) return NULL;
  if (epMode!=1) return epcoment_baixar(url,12,NULL);
  epRequests++;
  while (!SDL_AtomicGet(&epRelease)) SDL_Delay(1);
  if (page>=3) return strdup("[]");
  char out[6000]="[";
  int n=page==1?8:2;
  for (int i=0;i<n;i++) {
    char entry[600]; int id=page==1?i:(i?8:7);
    snprintf(entry,sizeof entry,"%s{\"id\":%d,\"comment\":\"Paged review %d\",\"username\":\"Reader %d\",\"user_rating\":%d,\"created_at\":\"2026-01-01\"}",i?",":"",id,id,id,id%10+1);
    strcat(out,entry);
  }
  strcat(out,"]"); return strdup(out);
}
int epcoment_pedir(const RedePedido *p,RedeResposta *r) {
  memset(r,0,sizeof *r);
  if (rede_job_estado(p->job)!=REDE_OK) { r->erro=REDE_CANCELADO; return 0; }
  r->corpo=epFixture(p->url); r->status=r->corpo?200:503;
  if (rede_job_estado(p->job)!=REDE_OK) { free(r->corpo); r->corpo=NULL; r->erro=REDE_CANCELADO; return 0; }
  r->erro=REDE_OK; return 1;
}
int epcoment_cabecalhos(const char **cab, char *aut, size_t na, char *chave, size_t nc) {
  (void)aut; (void)na; (void)chave; (void)nc; cab[0] = NULL; return 0; // No linked Trakt account.
}
int epcoment_publicos(const char **cab, char *chave, size_t nc) {
  (void)chave; (void)nc; cab[0] = "trakt-api-version: 2"; cab[1] = NULL; return 1;
}
char *epcoment_baixar(const char *url, int segundos, const char *const *cab) {
  (void)segundos; (void)cab;
  if (!strstr(url, "/seasons/1/episodes/")) return NULL;
  if (strstr(url, "/ratings")) {
    epRatings++;
    return strdup("{\"rating\":8.4,\"votes\":1284,\"distribution\":{\"10\":600,\"9\":410,\"8\":267,\"7\":141,\"6\":95,\"5\":62}}");
  }
  if (!strstr(url, "/comments/")) return NULL;
  epRequests++;
  epPaged = strstr(url, "page=1") && strstr(url, "limit=");
  while (!SDL_AtomicGet(&epRelease)) SDL_Delay(1);
  if (epEmpty) return strdup("[]");
  return strdup("[{\"comment\":\"Episode that changes the tone of the season. The dinner scene is uncomfortable in the best sense, and the soundtrack carries everything without needing dialogue. Best direction of the series so far. I kept thinking about her last look at her brother. A long review continues here with enough text to prove that the focused card expands and scrolls all the way through the ending.\",\"user\":{\"username\":\"Marina R.\"},\"created_at\":\"2026-01-01T00:00:00Z\",\"likes\":212,\"replies\":18,\"user_rating\":9,\"spoiler\":false},{\"comment\":\"Starts slowly and takes time to get going. The final scene changes everything.\",\"user\":{\"username\":\"jpdacosta\"},\"created_at\":\"2025-12-29T00:00:00Z\",\"likes\":88,\"replies\":6,\"user_rating\":7,\"spoiler\":true}]");
}
#endif

#define IMDB_SERIE "tt0903747"
static const int TEMP_N[3] = { 10, 12, 8 };
void fx_pedir(const char *imdb, int serie, long tmdbId) { (void)imdb; (void)serie; (void)tmdbId; }
int fx_carregando(void)       { return 0; }
int fx_n_comentarios(void)    { return 0; }
int fx_n_comentarios_ep(void) { return 0; }
int fx_n_relacionados(void)   { return 0; }
int fx_n_colecao(void)        { return 0; }
int fx_n_estudios(void)       { return 0; }
int fx_n_trailers(void)       { return 0; }
int fx_nota_trakt(void)       { return 84; }
int fx_recomenda_ativo(void)  { return 0; }
int fx_n_temporadas(void)     { return 3; }
int fx_temporada_numero(int t) { return t >= 0 && t < 3 ? t + 1 : 0; }
int fx_n_eps(int t) { return t >= 0 && t < 3 ? TEMP_N[t] : 0; }
int fx_ep_numero(int t, int i) { (void)t; return i + 1; }
int fx_ep_nota(int t, int i) { return 74 + ((i * 3 + t * 5) % 16); }
int fx_ep_visto(int t, int e) { (void)t; (void)e; return 0; }
int fx_progresso_pronto(void) { return 0; }
int fx_proximo_episodio(int *t, int *e) { (void)t; (void)e; return 0; }
int fx_progresso_serie(int *v, int *x) { (void)v; (void)x; return 0; }
int fx_agenda_temporada(void) { return 0; }
int fx_agenda_episodio(void)  { return 0; }
const char *fx_agenda_data(void)   { return ""; }
const char *fx_agenda_status(void) { return "Ended"; }

static CatItem itens[1];
static CatEp   eps[30];
static const char *const NOMES[10] = {
  "Descenso", "A espada de Simón Bolívar", "Os homens de sempre", "O palácio em chamas",
  "Haverá um futuro", "Explosivos", "Você vai chorar lágrimas de sangue", "La Gran Mentira",
  "La Catedral", "Despegue" };
static void montarCatalogo(void) {
  CatFileira fil;
  int t, i, n = 0;
  memset(itens, 0, sizeof itens); memset(eps, 0, sizeof eps); memset(&fil, 0, sizeof fil);
  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Série de Ensaio");
  snprintf(itens[0].imdb, sizeof itens[0].imdb, IMDB_SERIE);
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "series");
  snprintf(itens[0].genero, sizeof itens[0].genero, "Programa de TV · Crime · Drama");
  snprintf(itens[0].meta, sizeof itens[0].meta, "2015 · 3 temporadas");
  snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
           "Sinopse de enchimento, comprida o bastante para ocupar as linhas que o "
           "heroi reserva para ela, como acontece num titulo de verdade.");
  snprintf(itens[0].backdrop, sizeof itens[0].backdrop, "deploy/app/art/03.jpg");
  itens[0].nota = 87;
  for (t = 0; t < 3; t++) itens[0].temporadas[t] = t + 1;
  itens[0].nTemporadas = 3;
  fil.ini = 0; fil.n = 1;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 1, &fil, 1);
  for (t = 0; t < 3; t++)
    for (i = 0; i < TEMP_N[t]; i++) {
      CatEp *e = &eps[n++];
      e->temporada = t + 1; e->episodio = i + 1;
      snprintf(e->nome, sizeof e->nome, "%s", NOMES[i % 10]);
      snprintf(e->duracao, sizeof e->duracao, "%d min", 47 + (i % 9));
      snprintf(e->sinopse, sizeof e->sinopse,
               "Sinopse de enchimento do episodio, comprida o bastante para ocupar "
               "as linhas que o card reserva e mostrar onde o bloco corta.");
      snprintf(e->data, sizeof e->data, "28 de agosto de %d", 2015 + t);
      snprintf(e->thumb, sizeof e->thumb, "deploy/app/art/%02d.jpg", 4 + ((t * 10 + i) % 30));
      e->nota = 72 + ((i * 5 + t * 3) % 17);   // o voto do TMDB (CatEp.nota), em decimos
    }
  cat_definir_episodios(0, eps, n);
}

// --- quadro, captura, eventos -----------------------------------------------
static SDL_Window *janela;
static GLuint fbo, fboTex;
static const char *saida;
static int falhas;
#define CONFERE(c, msg) do { if (c) printf("ok   %s\n", msg); \
  else { printf("FALHA: %s (linha %d)\n", msg, __LINE__); falhas++; } } while (0)

static Uint32 relogio = 100000;
static Uint32 agoraFn(void) { return relogio; }
static void entregar(const SDL_Event *e) { detail_evento(e); }
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents();
    tex_bombear(3);
    detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ponteiro_quadro(relogio);
    detail_desenhar(SDL_GetTicks());
    ponteiro_desenhar();
  }
}
static void gravar(const char *nome) {
  char cam[900];
  unsigned char *pix;
  SDL_Surface *s;
  if (!saida) return;
  pix = malloc(1920 * 1080 * 4); assert(pix);
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", saida, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", cam);
}
// A cor de um ponto do quadro (y de cima para baixo), 0..255 por canal.
static void pixel(int x, int y, unsigned char *rgb) {
  unsigned char p[4];
  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(x, 1079 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
  rgb[0] = p[0]; rgb[1] = p[1]; rgb[2] = p[2];
}
static void tecla(Uint32 tipo, SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo; e.key.keysym.sym = k;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
}
// SEGURAR OK ate o limiar: a pagina mede pelo relogio de verdade.
static void segurarOk(void) {
  tecla(SDL_KEYDOWN, SDLK_RETURN);
  SDL_Delay(NV_HOLD_MS + 60);
  tecla(SDL_KEYUP, SDLK_RETURN);
}
static void voltar(void) { tecla(SDL_KEYDOWN, SDLK_ESCAPE); tecla(SDL_KEYUP, SDLK_ESCAPE); }
// O cursor volta com um movimento continuo; depois, mover e clicar.
static void acordar(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  relogio += 400;
  for (int i = 0; i < 6; i++) {
    e.motion.x = 1700 + i * 30; e.motion.y = 20 + i * 4;
    relogio += 40;
    ponteiro_evento(&e, entregar);
  }
}
static void mover(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION; e.motion.x = (int)x; e.motion.y = (int)y;
  relogio += 400;
  ponteiro_evento(&e, entregar);
}
static void clicar(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
  e.button.x = (int)x; e.button.y = (int)y;
  relogio += 400;
  ponteiro_evento(&e, entregar);
  e.type = SDL_MOUSEBUTTONUP;
  relogio += 60;
  ponteiro_evento(&e, entregar);
}

// A caixa que a pagina registrou para a coluna `c` da fileira `r`.
static int alvoDaPagina(int r, int c, GfxRect *q) {
  const PonteiroAlvo *v;
  int n = ponteiro_teste_lista(&v), i;
  for (i = n - 1; i >= 0; i--)
    if (v[i].focar && v[i].a == r && v[i].b == c && v[i].w < 1900.0f) {
      *q = (GfxRect){ v[i].x, v[i].y, v[i].w, v[i].h };
      return 1;
    }
  return 0;
}
// O MENU NA LISTA DE ALVOS: depois do "fora" (tela cheia, com ativar) vem o
// anteparo da caixa (sem focar nem ativar) e as linhas (com focar). Devolve
// quantas linhas; `caixa` e a do anteparo, `lin` as das linhas.
#define LIN_MAX 8
static int alvosDoMenu(GfxRect *caixa, GfxRect *lin) {
  const PonteiroAlvo *v;
  int n = ponteiro_teste_lista(&v), i, k = 0, fora = -1;
  for (i = n - 1; i >= 0; i--)
    if (!v[i].focar && v[i].ativar && v[i].w >= 1900.0f && v[i].h >= 1000.0f) { fora = i; break; }
  if (fora < 0 || fora + 1 >= n) return -1;
  if (v[fora + 1].focar || v[fora + 1].ativar) return -1;
  *caixa = (GfxRect){ v[fora + 1].x, v[fora + 1].y, v[fora + 1].w, v[fora + 1].h };
  for (i = fora + 2; i < n && k < LIN_MAX; i++)
    if (v[i].focar && v[i].h <= 64.0f) lin[k++] = (GfxRect){ v[i].x, v[i].y, v[i].w, v[i].h };
  return k;
}
static int perto(float a, float b) { return fabsf(a - b) < 1.5f; }
static int vistosDa(int t) {
  int i, q = 0;
  for (i = 1; i <= TEMP_N[t - 1]; i++) if (vistoep_estado(IMDB_SERIE, t, i) == 1) q++;
  return q;
}
static void zerarVistos(void) {
  int t, i;
  for (t = 1; t <= 3; t++) for (i = 1; i <= TEMP_N[t - 1]; i++) vistoep_definir(IMDB_SERIE, t, i, 0);
}
static void focar(int fileira, int col) {
  nivel = 1; foco.fileira = fileira; foco.coluna = col;
  quadros(90);
}

int main(int argc, char **argv) {
  const char *quero = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  GfxRect card, aba, caixa, lin[LIN_MAX];
  int n;
  saida = argc > 1 ? argv[1] : NULL;
  if (!quero || !*quero) { printf("RECUSADO: rode por tests/menuep.sh\n"); return 2; }
  dados_iniciar(quero);
  if (strcmp(dados_dir(), quero)) { printf("RECUSADO: dados_dir() nao e a pasta do teste\n"); return 2; }
  { char cam[700]; FILE *fa;
    const char *f = getenv("NUVIO_SHOT_FONTE"), *id = getenv("NUVIO_SHOT_IDIOMA");
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    fa = fopen(cam, "w"); assert(fa);
    fprintf(fa, "idioma %d\n", id ? atoi(id) : 0);
#ifdef NV_TEST_EPCOMENT
    fprintf(fa, "notaEpisodio 1\nselected_theme 2\nbordaFocoCartaz 1\n");
#endif
    if (f && *f) fprintf(fa, "fonteInterface %d\n", atoi(f));
    fclose(fa); }
  setenv("NUVIO_TAMANHO_UI", "1", 1); // Geometry assertions use 1080p layout units.
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: menu do episodio", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela); assert(gl);
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
  tex_iniciar(128);
  gfx_icones_dir("deploy/app/art");
  extras_carregar("deploy/app/art");
  ajustes_iniciar();
#ifdef NV_TEST_EPCOMENT
  dados_gravar("aparencia-20.txt","1\n");
#endif
  ajustes_dir(dados_dir());
  gfx_escala_txt_definir(1.0f);
  ponteiro_teste_relogio(agoraFn);
  ponteiro_teste_janela(1920, 1080);
  ponteiro_teste_toque(0);

  montarCatalogo();
  zerarVistos();
  { HomeItem hi; memset(&hi, 0, sizeof hi);
    hi.indice = 0; hi.rect = (GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H };
    hi.titulo = itens[0].titulo; hi.genero = itens[0].genero; hi.meta = itens[0].meta;
    detail_abrir(&hi); }
  focar(SEC_EPISODIOS, 5);
  quadros(120);
  acordar(); quadros(2);

#ifdef NV_TEST_EPCOMENT
  CONFERE(epRequests == 0 && epRatings == 0, "plain focus makes no episode requests");
  segurarOk(); quadros(10);
  CONFERE(extras_comentarios_ep_carregando(), "opening menu starts lazy fetch");
  gravar("loading");
  SDL_AtomicSet(&epRelease, 1);
  for (int wait = 0; extras_comentarios_ep_carregando() && wait < 500; wait++) SDL_Delay(2);
  quadros(90); acordar(); quadros(2);
  n = alvosDoMenu(&caixa, lin);
  CONFERE(n == 6 && epcoment_quantidade() == 2, "Comments row added with episode count 2");
  ExEpNotas ratings; extras_notas_ep(&ratings);
  CONFERE(ratings.nota == 84 && ratings.votos == 1284 && ratings.distribuicao[10] == 600,
          "episode ratings and distribution parsed from Trakt");
  CONFERE(extras_comentario_ep_respostas(0) == 18 && extras_comentario_ep_spoiler(1),
          "reply count and spoiler metadata retained");
  tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT); quadros(90);
  gravar("focused");
  epSheen=-.35f; quadros(1); epSheen=-.35f;
  gfx_novo_quadro(); txt_novo_quadro(); detail_desenhar(SDL_GetTicks());
  gravar("focused-sheen");
  CONFERE(episodios_menu_aberto() && epcoment_foco() == 0, "Right enters first comment");
  tecla(SDL_KEYDOWN, SDLK_DOWN); tecla(SDL_KEYUP, SDLK_DOWN); quadros(90);
  gravar("next");
  CONFERE(epcoment_foco() == 1, "Down focuses next comment");
  tecla(SDL_KEYDOWN, SDLK_RETURN); tecla(SDL_KEYUP, SDLK_RETURN); quadros(2);
  CONFERE(!sinopse_aberta(), "first OK reveals spoiler in card");
  tecla(SDL_KEYDOWN, SDLK_RETURN); tecla(SDL_KEYUP, SDLK_RETURN); quadros(2);
  CONFERE(sinopse_comentario_aberto(), "second OK opens shared full comment reader");
  gravar("expanded");
  tecla(SDL_KEYDOWN, SDLK_LEFT); tecla(SDL_KEYUP, SDLK_LEFT); quadros(2);
  CONFERE(sinopse_comentario_aberto() && epcoment_foco() == 1,
          "reader does not navigate unrelated detail comments");
  voltar(); quadros(2);
  CONFERE(!sinopse_aberta() && episodios_menu_aberto() && epcoment_foco() == 1,
          "Back closes reader and preserves episode menu focus");
  tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT); quadros(2);
  CONFERE(epcoment_foco() == 0 && epcoment_quantidade() == 1 && episodios_menu_aberto(), "Right skips empty 10 and selects rating 9");
  gravar("filter-active");
  checkBars(1); quadros(1);
  CONFERE(epPaged, "episode comments request includes page and limit");
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(1);
  CONFERE(epcoment_quantidade()==1,"filter skips empty 8 and selects 7");
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(1);
  CONFERE(epcoment_quantidade()==2,"filter skips empty 6..1 and wraps to All");
  gravar("all");
  checkBars(0); quadros(1);
  tecla(SDL_KEYDOWN, SDLK_LEFT); tecla(SDL_KEYUP, SDLK_LEFT);
  CONFERE(epcoment_foco()==-1,"Left returns to menu on its left");
  voltar(); quadros(60);
  CONFERE(!episodios_menu_aberto(), "Back closes entire menu from comment list");
  epEmpty = 1;
  focar(SEC_EPISODIOS, 6); segurarOk();
  for (int wait = 0; extras_comentarios_ep_carregando() && wait < 500; wait++) SDL_Delay(2);
  quadros(90); acordar(); quadros(2);
  gravar("no-comments");
  CONFERE(alvosDoMenu(&caixa, lin) == 5 && epcoment_quantidade() == 0, "empty episode omits Comments row");
  CONFERE(epRequests == 2 && epRatings == 2, "one comments/ratings fetch per opened episode");
  voltar(); quadros(60);
  CONFERE(ajustes_nota_episodio(EX_IMDB) && !ajustes_nota_episodio(EX_TRAKT) &&
          !ajustes_nota_episodio(EX_TMDB), "notaEpisodio=1 selects only IMDb badges");
  CatEp scored = *cat_episodio(0, 5);
  scored.notaImdb = 87; scored.nota=76;
  cat_definir_episodios(0, &scored, 1);
  espImdb = 0; espNota = "8,7"; espNotaEstilo = -1;
  desenhaEpisodio((GfxRect){96, 350, 560, 580}, 0, 1, 1, SDL_GetTicks());
  CONFERE(espImdb && espNotaEstilo >= 0, "selected IMDb episode badge appears on card");
  espNota = NULL;
  epcoment_abrir(IMDB_SERIE, scored.temporada, scored.episodio);
  epImdbLabels = epImdbScores = 0;
  epcoment_desenhar((GfxRect){1000, 350, 676, 624}, 1);
  CONFERE(epImdbScores, /* the IMDb logo replaced the text label */ "selected IMDb episode score appears in review panel");
  char settingsPath[700]; snprintf(settingsPath,sizeof settingsPath,"%s/ajustes.txt",dados_dir());
  FILE *settings=fopen(settingsPath,"w"); assert(settings);
  fputs("notaEpisodio 3\n",settings); fclose(settings); ajustes_dir(dados_dir());
  CONFERE(ajustes_nota_episodio(EX_IMDB) && ajustes_nota_episodio(EX_TMDB),"both episode sources selected");
  espNota="7,6"; espNotaEstilo=-1;
  desenhaEpisodio((GfxRect){96,350,560,580},0,1,1,SDL_GetTicks());
  CONFERE(espNotaEstilo>=0,"selected TMDB episode rating appears alongside IMDb");
  epcoment_abrir(IMDB_SERIE, scored.temporada, scored.episodio);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glClearColor(.04f,.04f,.05f,1); glClear(GL_COLOR_BUFFER_BIT);
  epcoment_desenhar((GfxRect){1000, 350, 676, 624}, 1);
  gravar("ratings");
  espNota=NULL;
  scored.notaImdb = 0;
  cat_definir_episodios(0, &scored, 1);
  epcoment_abrir(IMDB_SERIE, scored.temporada, scored.episodio);
  epImdbLabels = epImdbScores = 0;
  epcoment_desenhar((GfxRect){1000, 350, 676, 624}, 1);
  CONFERE(!epImdbLabels && !epImdbScores, "missing IMDb episode score never borrows show rating");
  voltar(); quadros(60);
  montarCatalogo(); epEmpty=0; epMode=1; memset(epPages,0,sizeof epPages);
  focar(SEC_EPISODIOS,3); episodios_menu_visto(0,1,4,"Paged episode");
  for (int wait=0;extras_comentarios_ep_carregando() && wait<1000;wait++) SDL_Delay(2);
  quadros(60); gravar("header-bars-rest");
  CONFERE(extras_n_comentarios_ep()==8 && !extras_comentarios_ep_tmdb(),"Trakt preferred when it has comments");
  SDL_AtomicSet(&epRelease,0);
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT);
  for (int i=0;i<7;i++) { tecla(SDL_KEYDOWN,SDLK_DOWN); tecla(SDL_KEYUP,SDLK_DOWN); }
  quadros(60); gravar("loading-more");
  for (int i=0;i<20;i++) extras_comentarios_ep_mais();
  SDL_AtomicSet(&epRelease,1);
  for (int wait=0;extras_comentarios_ep_carregando() && wait<1000;wait++) SDL_Delay(2);
  quadros(5);
  CONFERE(extras_n_comentarios_ep()==9,"append removes duplicate review across pages");
  CONFERE(epPages[1]==1 && epPages[2]==1,"near-end requests coalesce to one request per page");
  CONFERE(!extras_comentarios_ep_tem_mais(),"short last page stops infinite loading");
  for (int i=0;i<20;i++) extras_comentarios_ep_mais();
  CONFERE(epPages[3]==0,"no request after last page");
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(2);
  int countBefore=epcoment_quantidade();
  SDL_Event held={0}; held.type=SDL_KEYDOWN; held.key.keysym.sym=SDLK_RIGHT; held.key.repeat=1;
  epcoment_evento(&held);
  CONFERE(epcoment_quantidade()==countBefore,"held Right does not skip rating filters");
  for (int i=0;i<9;i++) { tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); }
  CONFERE(epcoment_quantidade()==9,"All includes every appended review");
  tecla(SDL_KEYDOWN,SDLK_LEFT); tecla(SDL_KEYUP,SDLK_LEFT);
  for (int i=0;i<12;i++) { tecla(SDL_KEYDOWN,SDLK_UP); tecla(SDL_KEYUP,SDLK_UP); }
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(20);
  int t1,e1; episodios_menu_fontes(&t1,&e1);
  CONFERE(t1==1 && e1==5 && episodios_menu_aberto(),"header Right switches episode without closing modal");
  GfxRect fixedPanel=caixa;
  episodios_menu_caixa(&fixedPanel);
  tecla(SDL_KEYDOWN,SDLK_LEFT); tecla(SDL_KEYUP,SDLK_LEFT); quadros(20);
  episodios_menu_fontes(&t1,&e1);
  CONFERE(t1==1 && e1==4 && episodios_menu_aberto(),"header Left replaces removed Previous action");
  GfxRect afterPanel; episodios_menu_caixa(&afterPanel);
  CONFERE(fabsf(fixedPanel.x-afterPanel.x)<.01f && fabsf(fixedPanel.y-afterPanel.y)<.01f,"context panel stays fixed while row moves");
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(20);
  CONFERE(foco.coluna==4,"row behind follows selected episode");
  episodios_menu_visto(0,1,10,"Season end");
  tecla(SDL_KEYDOWN,SDLK_UP); tecla(SDL_KEYUP,SDLK_UP);
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(20);
  episodios_menu_fontes(&t1,&e1);
  CONFERE(t1==2 && e1==1,"navigation crosses loaded season boundary");
  tecla(SDL_KEYDOWN,SDLK_LEFT); tecla(SDL_KEYUP,SDLK_LEFT); quadros(20);
  episodios_menu_fontes(&t1,&e1);
  CONFERE(t1==1 && e1==10,"reverse navigation crosses season boundary");
  anim_politica_reduzida=1;
  tecla(SDL_KEYDOWN,SDLK_RIGHT); tecla(SDL_KEYUP,SDLK_RIGHT); quadros(1);
  int direction; float phase=episodios_menu_transicao(&direction);
  CONFERE(phase==1,"reduced motion skips episode transition");
  anim_politica_reduzida=0;
  tecla(SDL_KEYDOWN,SDLK_LEFT); tecla(SDL_KEYUP,SDLK_LEFT);
  phase=episodios_menu_transicao(&direction);
  CONFERE(phase==0 && direction==-1,"episode transition starts in travel direction");
  SDL_Event repeat={0}; repeat.type=SDL_KEYDOWN; repeat.key.keysym.sym=SDLK_LEFT; repeat.key.repeat=1;
  float previousScroll=scrollSec[SEC_EPISODIOS];
  for (int i=0;i<5;i++) { episodios_menu_evento(&repeat); quadros(1); }
  phase=episodios_menu_transicao(&direction);
  gravar("transition-out");
  CONFERE(phase>0 && phase<1,"episode crossfade progresses before 180 ms");
  quadros(3); gravar("transition-in");
  CONFERE(fabsf(previousScroll-scrollSec[SEC_EPISODIOS])>.01f,"episode row keeps moving during content transition");
  quadros(20); phase=episodios_menu_transicao(&direction);
  CONFERE(phase==1,"episode crossfade ends after 180 ms under repeated keys");
  voltar(); quadros(20);
  focar(SEC_EPISODIOS,0);
  episodios_menu_visto(0,1,1,"The One Where Everybody Discovers What Happened After the Very Long Dinner");
  quadros(60); gravar("long-title");
  tecla(SDL_KEYDOWN,SDLK_UP); tecla(SDL_KEYUP,SDLK_UP);
  tecla(SDL_KEYDOWN,SDLK_LEFT); tecla(SDL_KEYUP,SDLK_LEFT);
  episodios_menu_fontes(&t1,&e1);
  CONFERE(t1==1 && e1==1,"first episode boundary does not wrap");
  voltar(); quadros(20);
  epMode=2; desc_tmdb_definir("0123456789abcdef0123456789abcdef");
  focar(SEC_EPISODIOS,1); episodios_menu_visto(0,1,2,"Fallback episode");
  for (int wait=0;extras_comentarios_ep_carregando() && wait<1000;wait++) SDL_Delay(2);
  quadros(60); gravar("tmdb-fallback");
  CONFERE(extras_comentarios_ep_tmdb() && extras_n_comentarios_ep()==1,"TMDB fallback on Trakt unavailable");
  CONFERE(strstr(extras_comentario_ep_texto(0),"whole series")!=NULL,"fallback preserves show review content");
  voltar(); quadros(20);
  epMode=0; epEmpty=1;
  episodios_menu_visto(0,1,3,"Empty Trakt");
  for (int wait=0;extras_comentarios_ep_carregando() && wait<1000;wait++) SDL_Delay(2);
  CONFERE(extras_comentarios_ep_tmdb(),"TMDB fallback on empty Trakt response");
  voltar(); quadros(20);
  desc_tmdb_definir(""); epMode=1; epEmpty=0;
  SDL_AtomicSet(&epRelease,0);
  episodios_menu_visto(0,1,8,"Cancelled episode"); SDL_Delay(10);
  episodios_menu_visto(0,1,9,"Replacement episode");
  SDL_AtomicSet(&epRelease,1);
  for (int wait=0;extras_comentarios_ep_carregando() && wait<1000;wait++) SDL_Delay(2);
  quadros(20);
  CONFERE(extras_comentarios_ep_do(IMDB_SERIE,1,9) && extras_n_comentarios_ep()==8,"cancelled worker cannot replace new episode feed");
  char grouped[32]; idioma_inteiro(IDIOMA_PT,3948,grouped,sizeof grouped);
  CONFERE(!strcmp(grouped,"3.948"),"Portuguese thousands grouping");
  idioma_inteiro(IDIOMA_EN,3948,grouped,sizeof grouped);
  CONFERE(!strcmp(grouped,"3,948"),"English thousands grouping");
  voltar(); quadros(20);
  printf("epcoment: %d failures\n", falhas);
  return falhas ? 1 : 0;
#endif

  // --- C. O SELO DE NOTA ---------------------------------------------------
  // T1E6 no ensaio: Trakt 74 + (5*3 % 16) = 8,9. Portugues: virgula.
  espNota = "8,9"; espRotuloFonte = 0; espNotaEstilo = -1;
  quadros(3);
  gravar("1-cards-notas");
  CONFERE(espRotuloFonte == 0, "nota do episodio: a pagina nao escreve mais \"Trakt\"/\"TMDB\" por extenso");
  CONFERE(espNotaEstilo == (int)TXT_G21B, "nota do episodio: o numero sai em negrito (TXT_G21B, 21/700)");
  for (int wait = 0; wait < 200; wait++) {
    tex_obter(extras_caminho_marca_nome("trakt"));
    tex_obter(extras_caminho_marca_nome("tmdb"));
    quadros(1); SDL_Delay(2);
    if (tex_aspecto(extras_caminho_marca_nome("trakt")) > 0 && tex_aspecto(extras_caminho_marca_nome("tmdb")) > 0) break;
  }
  CONFERE(tex_aspecto(extras_caminho_marca_nome("trakt")) > 0.0f &&
          tex_aspecto(extras_caminho_marca_nome("tmdb")) > 0.0f,
          "nota do episodio: os logos do Trakt e do TMDB (art/marcas) foram carregados");
  espNota = NULL;

  // --- A. O MENU DO EPISODIO -----------------------------------------------
  CONFERE(alvoDaPagina(SEC_EPISODIOS, 5, &card), "episodio: o card em foco e alvo do ponteiro");
  segurarOk();
  CONFERE(episodios_menu_aberto() && !episodios_menu_modo_temporada(), "episodio: segurar OK abre o menu de visto");
  quadros(4);  gravar("2-menu-episodio-abrindo");
  quadros(90); gravar("2-menu-episodio");
  acordar(); quadros(2);
  n = alvosDoMenu(&caixa, lin);
  CONFERE(n == 5 || n == 6, "episode: five actions plus optional loading Comments row");
  if (n == 5 || n == 6) {
    CONFERE(perto(lin[0].h, 60.0f) && perto(lin[1].y - lin[0].y, 64.0f) && perto(lin[3].y - lin[2].y, 64.0f),
            "episodio: linhas de 60 a cada 64, as do menu do cartaz");
    CONFERE(caixa.w >= 419.0f && caixa.w <= 601.0f, "episodio: a ilha tem de 420 a 600 de largura");
    CONFERE(perto(lin[0].x - caixa.x, 14.0f) && perto(caixa.x + caixa.w - lin[0].x - lin[0].w, 14.0f),
            "episodio: 14 de ar da ilha as linhas");
    CONFERE(caixa.x >= card.x + card.w || caixa.x + caixa.w <= card.x,
            "episodio: a ilha fica AO LADO do card, nao por cima dele");
    // O ponteiro: passar pela terceira linha e clicar = "Temporada inteira".
    mover(lin[2].x + lin[2].w * 0.5f, lin[2].y + lin[2].h * 0.5f);
    quadros(12); gravar("2-menu-episodio-ponteiro");
    clicar(lin[2].x + lin[2].w * 0.5f, lin[2].y + lin[2].h * 0.5f);
    quadros(6); gravar("2-menu-episodio-feito");
    CONFERE(vistosDa(1) == TEMP_N[0], "episodio: clicar em \"Temporada inteira\" marca a temporada");
    SDL_Delay(1000); quadros(40);
    CONFERE(!episodios_menu_aberto(), "episodio: o menu sai sozinho depois da confirmacao");
  }
  zerarVistos();
  quadros(30);
  tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT);   // o cursor dorme com a seta
  tecla(SDL_KEYDOWN, SDLK_LEFT);  tecla(SDL_KEYUP, SDLK_LEFT);
  quadros(60);
  segurarOk(); quadros(60);
  CONFERE(episodios_menu_aberto(), "episodio: abre de novo pelo controle");
  tecla(SDL_KEYDOWN, SDLK_DOWN); tecla(SDL_KEYUP, SDLK_DOWN);
  quadros(20); gravar("2-menu-episodio-foco2");
  voltar(); quadros(60);
  CONFERE(!episodios_menu_aberto() && nivel == 1 && foco.fileira == SEC_EPISODIOS && foco.coluna == 5,
          "episodio: Voltar fecha e o foco continua no card");
  CONFERE(vistosDa(1) == 0, "episodio: Voltar nao marca nada");

  // --- B. O PAINEL DA TEMPORADA --------------------------------------------
  focar(SEC_TEMPORADAS, 2);
  acordar(); quadros(2);
  CONFERE(alvoDaPagina(SEC_TEMPORADAS, 2, &aba), "temporada: a aba em foco e alvo do ponteiro");
  tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT);
  tecla(SDL_KEYDOWN, SDLK_LEFT);  tecla(SDL_KEYUP, SDLK_LEFT);
  quadros(60);
  gravar("3-temporada-fechado");
  acordar(); quadros(2);
  CONFERE(alvoDaPagina(SEC_TEMPORADAS, 2, &aba), "season anchor after navigation settles");
  { unsigned char antes[3], depois[3];
    pixel(1700, 1000, antes);
    segurarOk();
    CONFERE(episodios_menu_aberto() && episodios_menu_modo_temporada(), "temporada: segurar OK abre as acoes da temporada");
    acordar();
    // COM ANIMACAO o painel ainda esta a caminho dois quadros depois: as linhas
    // so viram alvo quando ele passou da metade.
    quadros(2);
    CONFERE(alvosDoMenu(&caixa, lin) < 1, "temporada: o painel ABRE animado (a 2 quadros ainda nao chegou)");
    quadros(3);  gravar("3-temporada-abrindo-1");
    quadros(4);  gravar("3-temporada-abrindo-2");
    quadros(90); gravar("3-temporada-aberto");
    pixel(1700, 1000, depois);
    CONFERE(abs(antes[0] - depois[0]) < 3 && abs(antes[1] - depois[1]) < 3 && abs(antes[2] - depois[2]) < 3,
            "temporada: sem veu de tela cheia (o canto da pagina nao mudou de cor)"); }
  n = alvosDoMenu(&caixa, lin);
  CONFERE(n == 2, "temporada: duas linhas (marcar e desmarcar), alvos do ponteiro");
  if (n == 2) {
    CONFERE(perto(caixa.x, aba.x - 10.0f) && perto(caixa.y, aba.y - 10.0f),
            "temporada: o painel nasce NA ABA (canto dela, com 10 de ar)");
    CONFERE(caixa.y + caixa.h > aba.y + aba.h + 100.0f && caixa.w >= aba.w + 19.0f,
            "temporada: e cresce para baixo e para o lado");
    CONFERE(lin[0].y >= aba.y + aba.h && perto(lin[0].h, 60.0f) && perto(lin[1].y - lin[0].y, 64.0f),
            "temporada: as acoes ficam abaixo da aba, nas linhas do menu do cartaz");
    // O PAINEL E O PREENCHIMENTO DA ABA CRESCIDO (dono, 08/10): a borda
    // direita do painel, fora das linhas, tem a cor do miolo da aba em foco.
    { unsigned char pa[3], pp[3];
      pixel((int)(aba.x + 12.0f), (int)(aba.y + aba.h * 0.5f), pa);
      pixel((int)(caixa.x + caixa.w - 6.0f), (int)(caixa.y + caixa.h * 0.5f), pp);
      CONFERE(abs(pa[0] - pp[0]) < 8 && abs(pa[1] - pp[1]) < 8 && abs(pa[2] - pp[2]) < 8,
              "temporada: o painel e o preenchimento da aba crescido (mesma cor do miolo da aba)"); }
    mover(lin[1].x + lin[1].w * 0.5f, lin[1].y + lin[1].h * 0.5f);
    quadros(12); gravar("3-temporada-ponteiro");
    mover(lin[0].x + lin[0].w * 0.5f, lin[0].y + lin[0].h * 0.5f);
    clicar(lin[0].x + lin[0].w * 0.5f, lin[0].y + lin[0].h * 0.5f);
    quadros(8); gravar("3-temporada-feito");
    CONFERE(vistosDa(2) == TEMP_N[1], "temporada: clicar em \"Marcar temporada\" marca a temporada inteira");
    SDL_Delay(1000); quadros(4); gravar("3-temporada-fechando");
    quadros(60);
    CONFERE(!episodios_menu_aberto() && nivel == 1 && foco.fileira == SEC_TEMPORADAS && foco.coluna == 2,
            "temporada: fecha depois da confirmacao e o foco continua na aba");
  }
  // Voltar fecha, o foco fica na aba.
  tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT);
  tecla(SDL_KEYDOWN, SDLK_LEFT);  tecla(SDL_KEYUP, SDLK_LEFT);
  quadros(60);
  segurarOk(); quadros(60);
  CONFERE(episodios_menu_aberto(), "temporada: abre de novo pelo controle");
  voltar(); quadros(60);
  CONFERE(!episodios_menu_aberto() && nivel == 1 && foco.fileira == SEC_TEMPORADAS && foco.coluna == 2,
          "temporada: Voltar fecha e o foco continua na aba");
  // Clicar fora fecha.
  segurarOk(); acordar(); quadros(60);
  clicar(1700.0f, 1000.0f); quadros(30);
  CONFERE(!episodios_menu_aberto(), "temporada: clicar fora do painel fecha");
  // ANIMACOES REDUZIDAS: pronto no quadro seguinte.
  tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT);
  tecla(SDL_KEYDOWN, SDLK_LEFT);  tecla(SDL_KEYUP, SDLK_LEFT);
  quadros(60);
  anim_politica_reduzida = 1;
  segurarOk(); acordar(); quadros(2);
  CONFERE(alvosDoMenu(&caixa, lin) == 2, "temporada: com animacoes reduzidas o painel esta pronto em 2 quadros");
  gravar("3-temporada-reduzidas");
  voltar(); quadros(2);
  anim_politica_reduzida = 0;

  printf(falhas ? "menuep: %d FALHA(S)\n" : "menuep: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
