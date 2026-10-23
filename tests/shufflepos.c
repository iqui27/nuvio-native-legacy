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
static const char *espNota;       // a nota que se procura ("7,4")
static TxtLinha fx_txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  if (s && (!strcmp(s, "Trakt") || !strcmp(s, "TMDB"))) espRotuloFonte++;
  if (s && espNota && !strcmp(s, espNota)) espNotaEstilo = (int)estilo;
  return txt_linha(estilo, s, r, g, b, a);
}
#define txt_linha fx_txt_linha
#include "../src/detail.c"
#undef txt_linha
#undef recomenda_ativo
#include "dados.h"
#include "ponteiro.h"


// A POSICAO DO BOTAO DE EPISODIO ALEATORIO (dono, #205): "acho que o botao do
// shuffle tem que ser no inicio pq se pegar uma serie que tem muitas
// temporadas ela fica fora de tela". Serie de 24 temporadas, pagina de titulo
// de verdade (detail.c incluido), teclas de verdade, sem rede.
//
// O QUE SE COBRA:
//   A. o botao e a PRIMEIRA coluna da fileira, antes da "Temporada 1";
//   B. com 24 temporadas o botao esta DENTRO da tela (era o defeito: no fim da
//      fileira ele caia fora);
//   C. o foco inicial da fileira continua na temporada atual;
//   D. esquerda na primeira aba chega ao botao, que e o comeco da fileira e
//      nao troca a temporada.
//
//   bash tests/shufflepos.sh [prefixo-das-capturas]

#define IMDB_SERIE "tt0903747"
#define N_TEMPS 24                      // o caso do dono: serie de muitas temporadas
#define EPS_TEMP 2
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
int fx_n_temporadas(void)     { return N_TEMPS; }
int fx_temporada_numero(int t) { return t >= 0 && t < N_TEMPS ? t + 1 : 0; }
int fx_n_eps(int t) { return t >= 0 && t < N_TEMPS ? EPS_TEMP : 0; }
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

static CatItem itens[2];
static CatEp   eps[N_TEMPS * EPS_TEMP];
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
  snprintf(itens[0].meta, sizeof itens[0].meta, "2015 · %d temporadas", N_TEMPS);
  snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
           "Sinopse de enchimento, comprida o bastante para ocupar as linhas que o "
           "heroi reserva para ela, como acontece num titulo de verdade.");
  snprintf(itens[0].backdrop, sizeof itens[0].backdrop, "deploy/app/art/03.jpg");
  itens[0].nota = 87;
  for (t = 0; t < N_TEMPS; t++) itens[0].temporadas[t] = t + 1;
  itens[0].nTemporadas = N_TEMPS;
  snprintf(itens[1].titulo, sizeof itens[1].titulo, "Filme de Ensaio");
  snprintf(itens[1].imdb, sizeof itens[1].imdb, "tt0000001");
  snprintf(itens[1].tipo, sizeof itens[1].tipo, "movie");
  snprintf(itens[1].backdrop, sizeof itens[1].backdrop, "deploy/app/art/03.jpg");
  fil.ini = 0; fil.n = 2;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 2, &fil, 1);
  for (t = 0; t < N_TEMPS; t++)
    for (i = 0; i < EPS_TEMP; i++) {
      CatEp *e = &eps[n++];
      e->temporada = t + 1; e->episodio = i + 1;
      snprintf(e->nome, sizeof e->nome, "%s", NOMES[i % 10]);
      snprintf(e->duracao, sizeof e->duracao, "%d min", 47 + (i % 9));
      snprintf(e->sinopse, sizeof e->sinopse,
               "Sinopse de enchimento do episodio, comprida o bastante para ocupar "
               "as linhas que o card reserva e mostrar onde o bloco corta.");
      snprintf(e->data, sizeof e->data, "28 de agosto de %d", 2014 + t % 12);
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
static void tecla(Uint32 tipo, SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo; e.key.keysym.sym = k;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
}
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
static void abrir(int indice) {
  HomeItem hi; memset(&hi, 0, sizeof hi);
  hi.indice = indice; hi.rect = (GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H };
  hi.titulo = itens[indice].titulo; hi.genero = itens[indice].genero; hi.meta = itens[indice].meta;
  detail_abrir(&hi);
}
static void focar(int fileira, int col) {
  nivel = 1; foco.fileira = fileira; foco.coluna = col;
  quadros(90);
}
static void direita(void) { tecla(SDL_KEYDOWN, SDLK_RIGHT); tecla(SDL_KEYUP, SDLK_RIGHT); quadros(30); }
static void esquerda(void) { tecla(SDL_KEYDOWN, SDLK_LEFT); tecla(SDL_KEYUP, SDLK_LEFT); quadros(30); }

int main(int argc, char **argv) {
  const char *quero = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  saida = argc > 1 ? argv[1] : NULL;
  if (!quero || !*quero) { printf("RECUSADO: rode por tests/shufflepos.sh\n"); return 2; }
  dados_iniciar(quero);
  if (strcmp(dados_dir(), quero)) { printf("RECUSADO: dados_dir() nao e a pasta do teste\n"); return 2; }
  { char cam[700]; FILE *fa;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    fa = fopen(cam, "w"); assert(fa);
    fprintf(fa, "idioma 0\n");
    fclose(fa); }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: posicao do shuffle", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  tex_iniciar(32);
  gfx_icones_dir("deploy/app/art");
  extras_carregar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  ponteiro_teste_relogio(agoraFn);
  ponteiro_teste_janela(1920, 1080);
  ponteiro_teste_toque(0);
  srand(12345);

  montarCatalogo();

  // --- A/B. a fileira da serie de 24 temporadas ------------------------------
  abrir(1);
  quadros(60);
  CONFERE(secaoN(SEC_TEMPORADAS) == 0, "filme: sem fileira de temporadas");
  detail_fechar();
  quadros(5);

  abrir(0);
  quadros(60);
  CONFERE(secaoN(SEC_TEMPORADAS) == N_TEMPS + 1,
          "serie: 24 abas e UMA coluna a mais, o botao de aleatorio");
  CONFERE(ehShuffle(0) && !ehShuffle(1) && !ehShuffle(N_TEMPS),
          "o botao e a PRIMEIRA coluna da fileira, antes da Temporada 1");
  { float sx = xItem(SEC_TEMPORADAS, 0) - scrollSec[SEC_TEMPORADAS];
    float sw = larguraItem(SEC_TEMPORADAS, 0);
    CONFERE(sx >= 0.0f && sx + sw <= NV_TELA_W,
            "com 24 temporadas o botao esta DENTRO da tela (o defeito era ele cair fora)"); }
  CONFERE(xItem(SEC_TEMPORADAS, 0) < xItem(SEC_TEMPORADAS, 1),
          "o botao vem antes da primeira temporada na fileira");
  // --- C. o foco inicial continua na temporada atual ------------------------
  CONFERE(temporada == 0, "a pagina abre na temporada atual (T1)");
  CONFERE(foco.colunaLembrada[SEC_TEMPORADAS] == (temporada + 1),
          "o foco inicial da fileira continua na temporada atual, uma coluna alem do botao");

  // --- D. navegacao: esquerda/direita consistentes --------------------------
  focar(SEC_TEMPORADAS, 1);            // Temporada 1
  { int t0 = temporada;
    esquerda();
    CONFERE(nivel == 1 && foco.fileira == SEC_TEMPORADAS && foco.coluna == 0,
            "a seta esquerda na primeira aba chega ao botao");
    CONFERE(temporada == t0, "focar o botao nao troca a temporada");
    esquerda();
    CONFERE(foco.coluna == 0, "o botao e o comeco da fileira");
    direita();
    CONFERE(foco.coluna == 1, "a seta direita do botao volta a Temporada 1"); }

  // --- E. o alvo do ponteiro do botao, com a fileira na tela -----------------
  focar(SEC_TEMPORADAS, 0);
  acordar(); quadros(2);
  { GfxRect q;
    CONFERE(alvoDaPagina(SEC_TEMPORADAS, 0, &q) && q.x >= 0.0f && q.x + q.w <= NV_TELA_W,
            "o alvo do ponteiro do botao esta dentro da tela"); }
  gravar("shuffle-pos");

  printf(falhas ? "shufflepos: %d FALHA(S)\n" : "shufflepos: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
