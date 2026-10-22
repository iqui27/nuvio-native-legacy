// PONTEIRO NAS TELAS (relato do dono: "LG pointer is not working in all
// components and page"). tests/ponteiro.c prova o modulo sozinho; este prova
// que as TELAS registram alvos e que passar por cima foca e clicar ativa —
// com eventos SDL de mouse de verdade passando por ponteiro_evento, como o
// main.c faz no aparelho.
//
// Telas cobertas aqui (as que tem fixture sem rede): Biblioteca, Ajustes
// (indice, lista, editor e a pergunta de restaurar), Addons, Plugins, o painel
// de Salvos (abas, chips, linhas das outras abas), o Guia (barra de cima),
// Diagnostico, Live TV diag, o cartao 1.4.2 e as telas da segunda leva (ver
// main). Explorar e "Ver tudo" dependem de mapa/TMDB e da descoberta em rede;
// ficam fora.
//
//   bash tests/ponteiro_telas.sh [pasta-das-capturas]
//
// Com uma pasta, salva PNGs dos estados com o cursor em cima (NUVIO_SHOT_FONTE
// escolhe a fonte; 3 = Montserrat, a da TV do dono).
#include "biblioteca.h"
#include "ajustes.h"
#include "ponteiro.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "extras.h"
#include "addons.h"
#include "addonsui.h"
#include "pluginsui.h"
#include "salvospainel.h"
#include "guia.h"
#include "diagnostico.h"
#include "livetvdiag.h"
#include "novidades142.h"
#include "faixas.h"
#include "legendasui.h"
#include "plrilha.h"
#include "cacheboost.h"
#include "video.h"
#include "avisos.h"
#include "agenda.h"
#include "agendaui.h"
#include "socialvis.h"
#include "amigoperfil.h"
#include "dvtela.h"
#include "posplay.h"
#include "pipintro.h"
#include "salvosintro.h"
#include "guialembrete.h"
#include "layout.h"
#include "escala.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SDL_Window *janela;
static const char *pasta;
static int falhas;
#define CONFERE(c, msg) do { if (c) printf("ok   %s\n", msg); \
  else { printf("FALHA: %s (linha %d)\n", msg, __LINE__); falhas++; } } while (0)

static Uint32 relogio = 100000;
static Uint32 agora(void) { return relogio; }

// A tela sob teste: as tres entradas que o app.c chama.
typedef struct {
  void (*evento)(const SDL_Event *);
  void (*atualizar)(float, Uint32);
  void (*desenhar)(Uint32);
} Tela;
static const Tela BIB = { biblioteca_evento, biblioteca_atualizar, biblioteca_desenhar };
static const Tela AJ  = { ajustes_evento, ajustes_atualizar, ajustes_desenhar };
static const Tela *tela = &BIB;
static void entregar(const SDL_Event *e) { tela->evento(e); }
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    tela->atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ponteiro_quadro(relogio);
    tela->desenhar(SDL_GetTicks());
    ponteiro_desenhar();
    SDL_GL_SwapWindow(janela);
  }
}
static void captura(const char *nome) {
  char cam[800];
  unsigned char *pix;
  SDL_Surface *s;
  if (!pasta) return;
  quadros(1);
  // O quadro de cima ja foi trocado; redesenha sem trocar para ler.
  glClear(GL_COLOR_BUFFER_BIT);
  ponteiro_quadro(relogio);
  tela->desenhar(SDL_GetTicks());
  ponteiro_desenhar();
  pix = malloc(1920 * 1080 * 4);
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s/%s", pasta, nome);
  IMG_SavePNG(s, cam);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static void mover(float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION; e.motion.x = (int)x; e.motion.y = (int)y;
  relogio += 400;   // depois do freio do conteudo que anda (PONT_ASSENTA_MS)
  ponteiro_evento(&e, entregar);
}
// O gesto que devolve o cursor depois de uma seta: movimento continuo (menos
// de PONT_SETA_PAUSA_MS entre eventos) que vence o limiar de distancia.
static void acordar(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  relogio += 400;
  for (int i = 0; i < 6; i++) {
    e.motion.x = 5 + i * 40; e.motion.y = 5 + i * 40;
    relogio += 40;
    ponteiro_evento(&e, entregar);
  }
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
static void okTecla(void) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; relogio += 400;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
  e.type = SDL_KEYUP;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
}
static void seta(SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; relogio += 400;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
  e.type = SDL_KEYUP;
  if (!ponteiro_evento(&e, entregar)) entregar(&e);
}
// O centro do alvo i da lista que o hit-test le.
static int alvos(const PonteiroAlvo **v) { return ponteiro_teste_lista(v); }
static float cx(const PonteiroAlvo *a) { return a->x + a->w * 0.5f; }
static float cy(const PonteiroAlvo *a) { return a->y + a->h * 0.5f; }

// PASSAR POR CIMA DE CADA ALVO e ler o foco da tela depois de cada um:
// quantos valores DIFERENTES o foco assumiu. >= 2 e "o hover mexe no foco"
// (um so valor seria o foco de partida). Os centros sao lidos antes, porque a
// lista e refeita a cada quadro (o foco pode rolar a tela).
#define MAXC 160
static int distintos(int (*ler)(void), int *nAlvos) {
  const PonteiroAlvo *v;
  float xs[MAXC], ys[MAXC];
  int vals[MAXC], nv = 0, n = alvos(&v);
  if (n > MAXC) n = MAXC;
  for (int i = 0; i < n; i++) { xs[i] = cx(&v[i]); ys[i] = cy(&v[i]); }
  if (nAlvos) *nAlvos = n;
  for (int i = 0; i < n; i++) {
    int f, j;
    mover(xs[i], ys[i]); quadros(3);
    f = ler();
    for (j = 0; j < nv && vals[j] != f; j++) {}
    if (j == nv && nv < MAXC) vals[nv++] = f;
  }
  return nv;
}
// Hover numa tela + o D-pad: a seta esconde o cursor e, sem cursor, nenhuma
// tela registra alvo (custo zero) e o foco que a seta pos fica.
static void confereDpad(const char *nome, int (*ler)(void), SDL_Keycode k) {
  const PonteiroAlvo *v;
  char msg[160];
  int antes, depois;
  quadros(2);
  antes = ler();
  seta(k); quadros(1);
  depois = ler();
  quadros(4);
  snprintf(msg, sizeof msg, "%s: com a seta o cursor some e nao ha alvo por quadro", nome);
  CONFERE(alvos(&v) == 0, msg);
  snprintf(msg, sizeof msg, "%s: o foco da seta fica (o ponteiro nao briga com o D-pad)", nome);
  CONFERE(ler() == depois && depois != antes, msg);
  // O cursor volta com um gesto de verdade (fora da janela e do limiar da seta).
  acordar(); quadros(3);
}

static const char *NOMES[] = {
  "CODA", "Aftersun", "Dune: Part Two", "The Brutalist", "Poor Things", "The Substance",
  "Anatomy of a Fall", "Isle of Dogs", "Civil War", "Bacurau", "Cidade de Deus", "Noir",
};
#define NN (int)(sizeof NOMES / sizeof *NOMES)
static void povoar(void) {
  static CatItem v[NN];
  for (int i = 0; i < NN; i++) {
    CatItem *it = &v[i];
    memset(it, 0, sizeof *it);
    snprintf(it->titulo, sizeof it->titulo, "%s", NOMES[i]);
    snprintf(it->imdb, sizeof it->imdb, "tt900%02d", i);
    snprintf(it->tipo, sizeof it->tipo, "%s", i % 3 ? "movie" : "series");
    snprintf(it->genero, sizeof it->genero, "Movie · Drama");
    snprintf(it->poster, sizeof it->poster, "deploy/app/art/%02d.jpg", i);
    it->naLista = 1;
    it->naColecao = 1;
  }
  cat_definir(v, NN);
}

static void testeBiblioteca(void) {
  const PonteiroAlvo *v;
  int n, a = -1, b = -1, idxA = -1, idxB = -1, idx = -1;
  printf("\n== Biblioteca\n");
  tela = &BIB;
  povoar();
  biblioteca_iniciar();
  mover(5, 5);           // o cursor aparece
  quadros(70);
  n = alvos(&v);
  CONFERE(n > 0, "biblioteca registra alvos");
  // Duas celulas da grade: alvos abaixo da faixa de filtros, em colunas
  // diferentes.
  for (int i = 0; i < n; i++) {
    if (v[i].y < 330.0f) continue;
    if (a < 0) a = i;
    else if (b < 0 && v[i].y == v[a].y && v[i].x > v[a].x + 50.0f) b = i;
  }
  CONFERE(a >= 0 && b >= 0, "biblioteca: celulas da grade sao alvos");
  if (a < 0 || b < 0) return;
  { float ax = cx(&v[a]), ay = cy(&v[a]), bx = cx(&v[b]), by = cy(&v[b]);
    clicar(ax, ay); quadros(2);
    CONFERE(biblioteca_pediu_abrir(&idxA) && idxA >= 0, "biblioteca: clique na celula A abre o titulo");
    clicar(bx, by); quadros(2);
    CONFERE(biblioteca_pediu_abrir(&idxB) && idxB >= 0 && idxB != idxA,
            "biblioteca: clique na celula B abre OUTRO titulo");
    // Hover sozinho poe o foco: o OK do controle depois abre a celula de baixo
    // do cursor.
    mover(ax, ay); quadros(2);
    captura("biblioteca-hover-celula.png");
    okTecla(); quadros(2);
    CONFERE(biblioteca_pediu_abrir(&idx) && idx == idxA, "biblioteca: passar por cima foca (OK abre a celula A)"); }
  // A faixa de cima (abas/seletores) tambem e alvo.
  n = alvos(&v);
  { int topo = 0;
    for (int i = 0; i < n; i++) if (v[i].y < 330.0f) topo++;
    CONFERE(topo >= 3, "biblioteca: abas e seletores sao alvos"); }
  biblioteca_encerrar();
}

static void testeAjustes(void) {
  const PonteiroAlvo *v;
  int n, k = -1, antes, depois;
  printf("\n== Ajustes\n");
  tela = &AJ;
  ajustes_iniciar();
  mover(5, 5);
  quadros(90);
  n = alvos(&v);
  CONFERE(n > 0, "ajustes (indice) registra alvos");
  CONFERE(ajustes_foco_no_indice(), "ajustes abre no indice");
  // A categoria mais baixa do indice (os cartoes moram embaixo das pilulas).
  for (int i = 0; i < n; i++) if (k < 0 || v[i].y > v[k].y) k = i;
  if (k < 0) return;
  { float x = cx(&v[k]), y = cy(&v[k]);
    mover(x, y); quadros(4);
    captura("ajustes-hover-categoria.png");
    clicar(x, y); quadros(90); }
  CONFERE(!ajustes_foco_no_indice(), "ajustes: clique na categoria entra nela");
  n = alvos(&v);
  CONFERE(n > 1, "ajustes (lista da categoria) registra alvos");
  antes = ajustes_opcao_em_foco();
  // A linha mais baixa da lista: nao e a que abriu com o foco.
  k = -1;
  for (int i = 0; i < n; i++) if (k < 0 || v[i].y > v[k].y) k = i;
  if (k < 0) return;
  mover(cx(&v[k]), cy(&v[k])); quadros(20);
  depois = ajustes_opcao_em_foco();
  CONFERE(depois != antes && !ajustes_foco_no_indice(), "ajustes: passar por cima da linha foca ela");
  captura("ajustes-hover-linha.png");
}

// --- segunda leva (#99): as telas que ainda nao registravam alvos ----------
static int lerAddon(void) { return addonsui_foco(); }
static void testeAddons(void) {
  static const Tela T = { addonsui_evento, addonsui_atualizar, addonsui_desenhar };
  AddonRemoto l[4];
  const PonteiroAlvo *v;
  int n, d, k = -1, antes;
  printf("\n== Addons\n");
  memset(l, 0, sizeof l);
  for (int i = 0; i < 4; i++) {
    snprintf(l[i].nome, sizeof l[i].nome, "Addon %c", 'A' + i);
    snprintf(l[i].url, sizeof l[i].url, "http://127.0.0.1:9/a%d/manifest.json", i);
    l[i].ativo = 1;
  }
  addons_definir_lista(l, 4);
  tela = &T;
  mover(5, 5); quadros(30);
  d = distintos(lerAddon, &n);
  CONFERE(n >= 4, "addons: as linhas sao alvos");
  CONFERE(d >= 3, "addons: passar por cima foca linhas diferentes");
  captura("addons-hover-linha.png");
  // Clique na linha mais baixa: o OK liga/desliga AQUELE addon.
  n = alvos(&v);
  for (int i = 0; i < n; i++) if (k < 0 || v[i].y > v[k].y) k = i;
  if (k >= 0) {
    clicar(cx(&v[k]), cy(&v[k])); quadros(2);
    antes = addonsui_foco();
    CONFERE(antes == 3 && !addons_ativo(3), "addons: clique na linha desliga aquele addon");
  }
  mover(cx(&v[0]), cy(&v[0])); quadros(2);
  confereDpad("addons", lerAddon, SDLK_DOWN);
}

static int lerPlugin(void) { return pluginsui_foco(); }
static void testePlugins(void) {
  static const Tela T = { pluginsui_evento, pluginsui_atualizar, pluginsui_desenhar };
  int n, d;
  printf("\n== Plugins\n");
  pluginsui_abrir();
  tela = &T;
  mover(5, 5); quadros(30);
  d = distintos(lerPlugin, &n);
  CONFERE(n >= 1, "plugins: as linhas sao alvos");
  CONFERE(d >= 2 || n == 1, "plugins: passar por cima foca linhas diferentes");
}

static int lerEdPend(void) { int p; ajustes_teste_editor(&p, NULL, NULL, NULL); return p; }
static int lerEdRod(void) { int r; ajustes_teste_editor(NULL, &r, NULL, NULL); return r; }
static int lerEdConf(void) { int c; ajustes_teste_editor(NULL, NULL, NULL, &c); return c; }
static void testeAjustesEditor(void) {
  const PonteiroAlvo *v;
  int n, ed, d, rest = 0, k = -1;
  printf("\n== Ajustes (editor e pergunta)\n");
  tela = &AJ;
  ajustes_abrir_na_fonte();
  ajustes_iniciar();
  mover(5, 5); quadros(90);
  okTecla(); quadros(90);
  ed = ajustes_teste_editor(NULL, NULL, NULL, NULL);
  CONFERE(ed == 1, "ajustes: OK abre o editor da fonte");
  if (ed != 1) return;
  n = alvos(&v);
  CONFERE(n >= 2, "ajustes (editor) registra alvos");
  d = distintos(lerEdPend, NULL);
  CONFERE(d >= 2, "ajustes (editor): passar por cima escolhe valores diferentes");
  captura("ajustes-hover-editor.png");
  // O rodape "Restaurar padrao": algum alvo poe o foco nele; o clique pergunta.
  n = alvos(&v);
  for (int i = 0; i < n && k < 0; i++) {
    mover(cx(&v[i]), cy(&v[i])); quadros(2);
    if (lerEdRod()) k = i;
  }
  if (k >= 0) {
    float x = cx(&v[k]), y = cy(&v[k]);
    CONFERE(1, "ajustes (editor): o chip Restaurar padrao ganha o foco");
    clicar(x, y); quadros(10);
    ajustes_teste_editor(NULL, NULL, &rest, NULL);
    CONFERE(rest, "ajustes (editor): clique no Restaurar abre a pergunta");
    if (rest) {
      d = distintos(lerEdConf, &n);
      CONFERE(n == 2 && d == 2, "ajustes (pergunta): os dois botoes sao alvos e ganham o foco");
      captura("ajustes-hover-pergunta.png");
    }
    seta(SDLK_ESCAPE); quadros(10);
  } else CONFERE(0, "ajustes (editor): o chip Restaurar padrao ganha o foco");
  seta(SDLK_ESCAPE); quadros(30);
  acordar(); quadros(3);
}

static int lerPainel(void) { return spainel_aba_atual() * 1000 + spainel_foco_indice(); }
static void testePainel(void) {
  static const Tela T = { spainel_evento, spainel_atualizar, spainel_desenhar };
  const PonteiroAlvo *v;
  int n, abas = 0, barra = 0, trocou = 0, aba0;
  printf("\n== Salvos (painel)\n");
  povoar();
  tela = &T;
  spainel_abrir();
  mover(5, 5); quadros(90);
  n = alvos(&v);
  CONFERE(n > 0, "salvos: o painel registra alvos");
  // Cada alvo de cima (y < 330): hover e ver se o foco foi para as abas (-1)
  // ou para a barra de chips (-2).
  { float xs[MAXC], ys[MAXC]; int m = n < MAXC ? n : MAXC;
    for (int i = 0; i < m; i++) { xs[i] = cx(&v[i]); ys[i] = cy(&v[i]); }
    for (int i = 0; i < m; i++) {
      int f;
      mover(xs[i], ys[i]); quadros(3);
      f = spainel_foco_indice();
      if (f == -1) abas = 1;
      if (f == -2) { barra = 1; if (pasta && barra == 1) captura("salvos-hover-chip.png"); barra = 2; }
    } }
  CONFERE(abas, "salvos: passar por cima da faixa de abas foca a faixa");
  CONFERE(barra, "salvos: passar por cima de um chip foca a barra");
  // Clique numa aba que nao e a aberta: troca para ela.
  aba0 = spainel_aba_atual();
  n = alvos(&v);
  for (int i = 0; i < n && !trocou; i++) {
    if (!v[i].ativar || !v[i].focar) continue;
    clicar(cx(&v[i]), cy(&v[i])); quadros(40);
    if (spainel_aba_atual() != aba0) trocou = 1;
    n = alvos(&v);
  }
  CONFERE(trocou, "salvos: clique numa aba troca para ela");
  if (trocou) {
    int d;
    quadros(30);
    d = distintos(lerPainel, &n);
    printf("     (aba %d, %d alvos, %d focos)\n", spainel_aba_atual(), n, d);
    captura("salvos-hover-outra-aba.png");
    CONFERE(d >= 1 && n >= 1, "salvos: a outra aba tambem registra alvos");
  }
  spainel_fechar(); quadros(40);
}

static int lerGuia(void) { return guia_foco_topo(); }
static void testeGuia(void) {
  static const Tela T = { guia_evento, guia_atualizar, guia_desenhar };
  int n, d;
  printf("\n== Guia (Live TV)\n");
  tela = &T;
  guia_abrir();
  mover(5, 5); quadros(60);
  d = distintos(lerGuia, &n);
  CONFERE(n >= 2, "guia: os chips da barra de cima sao alvos");
  CONFERE(d >= 2, "guia: passar por cima foca chips diferentes");
  captura("guia-hover-chip.png");
}

static int lerDiag(void) { int m, l; diagnostico_teste_foco(&m, &l, NULL, NULL); return m * 10 + l; }
static void testeDiagnostico(void) {
  static const Tela T = { diagnostico_evento, diagnostico_atualizar, diagnostico_desenhar };
  int n, d;
  printf("\n== Diagnostico\n");
  dados_gravar("diagnostico-otimizacao-intro.cfg", "versao=1\n");
  tela = &T;
  diagnostico_iniciar();
  mover(5, 5); quadros(40);
  d = distintos(lerDiag, &n);
  CONFERE(n >= 2, "diagnostico: objetivos e botoes sao alvos");
  CONFERE(d >= 2, "diagnostico: passar por cima foca itens diferentes");
  captura("diagnostico-hover.png");
}

static int lerLtv(void) { return livetvdiag_teste_foco(); }
static void testeLiveTvDiag(void) {
  static const Tela T = { livetvdiag_evento, livetvdiag_atualizar, livetvdiag_desenhar };
  int n, d;
  printf("\n== Live TV diag\n");
  tela = &T;
  livetvdiag_iniciar();
  mover(5, 5); quadros(60);
  d = distintos(lerLtv, &n);
  CONFERE(n >= 2, "livetv diag: os botoes do rodape sao alvos");
  CONFERE(d >= 2, "livetv diag: passar por cima foca botoes diferentes");
  livetvdiag_encerrar();
}

static int lerN142(void) { return novidades142_teste_foco(); }
static void testeNovidades142(void) {
  static const Tela T = { novidades142_evento, novidades142_atualizar, novidades142_desenhar };
  int n, d;
  printf("\n== Novidades 1.4.2\n");
  tela = &T;
  novidades142_abrir();
  mover(5, 5); quadros(20);
  d = distintos(lerN142, &n);
  CONFERE(n >= 2 && d >= 2, "novidades 1.4.2: passar por cima foca os botoes");
}

// --- player: folhas de Audio e Legendas, hospedadas pela ilha -------------
static void plrEvento(const SDL_Event *e) { if (!faixas_pilula_tecla(e) && faixas_aberta()) faixas_evento(e); }
// A ilha anima pelo `agora` que recebe: o relogio do teste (o mesmo do
// ponteiro), senao 120 quadros rapidos nao assentam a mola.
static void plrAtualizar(float dt, Uint32 t) { (void)t; faixas_atualizar(dt, relogio); }
static void plrDesenhar(Uint32 t) {
  (void)t; t = relogio;
  // A ordem do app.c (desenharTelas): camada da folha, a folha, a ilha.
  if (faixas_aberta()) ponteiro_camada();
  faixas_desenhar(t);
  plrilha_desenhar(t);
}
static int lerFaixa(void) { int c, vo, ve, f = faixas_teste_foco(&c, &vo, &ve); return c * 100 + f * 4 + vo * 2 + ve; }
static int lerLeg(void) { return legendasui_teste_foco(NULL, NULL); }
static void testePlayer(void) {
  static const Tela T = { plrEvento, plrAtualizar, plrDesenhar };
  static VideoSimulacao sim;
  int n, d, vol = 0, vel = 0;
  printf("\n== Player: Audio, volume, velocidade e Legendas\n");
  memset(&sim, 0, sizeof sim);
  sim.nAudio = 3; sim.nLeg = 3; sim.pronto = 1;
  for (int i = 0; i < 3; i++) {
    snprintf(sim.audio[i].rotulo, sizeof sim.audio[i].rotulo, "Audio %d", i + 1);
    snprintf(sim.audio[i].idioma, sizeof sim.audio[i].idioma, "%s", i ? "eng" : "por");
    snprintf(sim.leg[i].rotulo, sizeof sim.leg[i].rotulo, "Legenda %d", i + 1);
    snprintf(sim.leg[i].idioma, sizeof sim.leg[i].idioma, "%s", i ? "eng" : "por");
  }
  video_simular(&sim);
  cacheboost_simular_suporte(1);
  tela = &T;
  faixas_abrir_em(0);
  mover(5, 5); quadros(120);
  { const PonteiroAlvo *v; printf("     (folha aberta %d, ativo %d, %d alvos)\n", faixas_aberta(), ponteiro_ativo(), alvos(&v)); }
  d = distintos(lerFaixa, &n);
  CONFERE(n >= 5, "audio: volume, velocidade e faixas sao alvos");
  CONFERE(d >= 4, "audio: passar por cima foca linhas diferentes");
  { const PonteiroAlvo *v; int m = alvos(&v);
    float xs[MAXC], ys[MAXC]; if (m > MAXC) m = MAXC;
    for (int i = 0; i < m; i++) { xs[i] = cx(&v[i]); ys[i] = cy(&v[i]); }
    for (int i = 0; i < m; i++) {
      int c, vo, ve;
      mover(xs[i], ys[i]); quadros(2);
      faixas_teste_foco(&c, &vo, &ve);
      if (vo && !vol) { vol = 1; captura("player-hover-volume.png"); }
      if (ve) vel = 1;
    } }
  CONFERE(vol, "audio: passar por cima do volume foca o volume");
  CONFERE(vel, "audio: passar por cima da velocidade foca a velocidade");
  faixas_abrir_em(1); quadros(120);
  d = distintos(lerLeg, &n);
  CONFERE(n >= 3, "legendas: as linhas sao alvos");
  CONFERE(d >= 2, "legendas: passar por cima foca linhas diferentes");
  captura("player-hover-legenda.png");
  { SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    for (int i = 0; i < 3 && faixas_aberta(); i++) { entregar(&e); quadros(30); } }
}

// --- Social: o perfil de um amigo, com o que ele viu (feed local) ---------
static int lerAmigo(void) { int c, f = amigoperfil_teste_foco(&c); return f * 100 + c; }
static void testeAmigo(void) {
  static const Tela T = { amigoperfil_evento, amigoperfil_atualizar, amigoperfil_desenhar };
  static SvEvento v[4];
  int n, d;
  printf("\n== Social (perfil do amigo)\n");
  memset(v, 0, sizeof v);
  for (int i = 0; i < 4; i++) {
    SvEvento *e = &v[i];
    snprintf(e->pessoaId, sizeof e->pessoaId, "nuvio:lia");
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "Lia");
    e->fonte = SV_FONTE_NUVIO; e->acao = i < 2 ? SV_INICIO : SV_REACAO;
    e->reacao = i < 2 ? SV_REAC_NADA : SV_REAC_GOSTOU;
    snprintf(e->imdb, sizeof e->imdb, "tt900%02d", i);
    snprintf(e->tipo, sizeof e->tipo, "movie");
    snprintf(e->titulo, sizeof e->titulo, "%s", NOMES[i]);
    snprintf(e->poster, sizeof e->poster, "deploy/app/art/%02d.jpg", i);
    e->pct = 100; e->quando = (long long)time(NULL) - 600 * (i + 1);
  }
  socialvis_definir_feed(v, 4);
  tela = &T;
  amigoperfil_abrir("nuvio:lia");
  mover(5, 5); quadros(60);
  d = distintos(lerAmigo, &n);
  printf("     (amigo: %d alvos, %d focos)\n", n, d);
  CONFERE(n >= 2, "social: os cartazes do perfil sao alvos");
  CONFERE(d >= 2, "social: passar por cima foca cartazes diferentes");
  captura("social-hover-cartaz.png");
}

// --- terceira leva: telas de toque unico e cartoes ------------------------
// A TELA DO DOLBY VISION: o unico botao e "Assistir agora em HDR10". So um
// clique NELE pode dispara-lo; no resto da tela o clique nao faz nada.
static int dvtUltimo;
static void dvtEvento(const SDL_Event *e) { int r = dvtela_evento(e, relogio); if (r != DVT_EV_NADA) dvtUltimo = r; }
static void dvtAtualizar(float dt, Uint32 t) { DvtelaSinais s; (void)t; memset(&s, 0, sizeof s); dvtela_atualizar(&s, dt, relogio); }
static void dvtDesenhar(Uint32 t) { (void)t; dvtela_desenhar(relogio); }
static void testeDvtela(void) {
  static const Tela T = { dvtEvento, dvtAtualizar, dvtDesenhar };
  const PonteiroAlvo *v;
  int n, b = -1;
  printf("\n== Tela do Dolby Vision\n");
  tela = &T;
  dvtUltimo = DVT_EV_NADA;
  dvtela_entrar(relogio);
  mover(5, 5); quadros(90);
  // Hover pela tela toda (cantos e centro): nada muda, nada dispara.
  mover(960, 540); quadros(2); mover(1800, 100); quadros(2);
  CONFERE(dvtela_ativa() && dvtUltimo != DVT_EV_HDR10, "dv: passar o cursor nao escolhe HDR10");
  clicar(40, 40); quadros(3);
  CONFERE(dvtela_ativa() && dvtUltimo != DVT_EV_HDR10, "dv: clique fora do botao nao escolhe HDR10");
  n = alvos(&v);
  for (int i = 0; i < n; i++) if (v[i].focar && v[i].w < 900.0f) b = i;
  CONFERE(b >= 0, "dv: o botao HDR10 e alvo");
  if (b >= 0) {
    float x = cx(&v[b]), y = cy(&v[b]);
    mover(x, y); quadros(2);
    captura("dv-hover-botao.png");
    clicar(x, y); quadros(3);
    CONFERE(!dvtela_ativa() && dvtUltimo == DVT_EV_HDR10, "dv: clique no botao escolhe HDR10");
  }
  if (dvtela_ativa()) dvtela_sair(DVT_SAIDA_VOLTAR, relogio);
  quadros(60);
}

// O POS-REPRODUCAO DA SERIE: o proximo episodio so comeca com um clique no
// botao "Comecar agora"; hover e clique no vazio nao comecam nada.
static int ppEv(const SDL_Event *e) { return posplay_evento(e); }
static void ppEvento(const SDL_Event *e) { (void)ppEv(e); }
static void ppAtualizar(float dt, Uint32 t) { (void)dt; (void)t; }
static void ppDesenhar(Uint32 t) { (void)t; posplay_desenhar(relogio, NV_VTELA_H - 60.0f); }
static void testePosplay(void) {
  static const Tela T = { ppEvento, ppAtualizar, ppDesenhar };
  const PonteiroAlvo *v;
  int n, b = -1, te, ep;
  printf("\n== Pos-reproducao (serie)\n");
  povoar();
  tela = &T;
  posplay_shot(0, 1, 1, 2, 0);
  mover(5, 5); quadros(10);
  for (int i = 0; i < 8; i++) { mover(100 + i * 230, 200 + (i % 4) * 220); quadros(2); }
  CONFERE(!posplay_pediu_episodio(&te, &ep), "posplay: passar o cursor nao comeca o episodio");
  clicar(60, 60); quadros(3);
  CONFERE(!posplay_pediu_episodio(&te, &ep) && posplay_visivel(), "posplay: clique no vazio nao comeca o episodio");
  n = alvos(&v);
  for (int i = 0; i < n; i++) if (v[i].focar) b = i;
  CONFERE(b >= 0, "posplay: o botao Comecar agora e alvo");
  if (b >= 0) {
    float x = cx(&v[b]), y = cy(&v[b]);
    mover(x, y); quadros(2);
    captura("posplay-hover-botao.png");
    CONFERE(!posplay_pediu_episodio(&te, &ep), "posplay: hover no botao nao comeca");
    clicar(x, y); quadros(2);
    CONFERE(posplay_pediu_episodio(&te, &ep) && te == 1 && ep == 2, "posplay: clique no botao comeca o episodio");
  }
  posplay_fechar();
}

// Cartoes de duas escolhas: o hover troca o foco entre os botoes.
static void (*cartaoDes)(Uint32);
static void cartaoDesenhar(Uint32 t) { (void)t; cartaoDes(relogio); }
static void cartaoAtualizarPip(float dt, Uint32 t) { (void)t; pipintro_atualizar(dt, relogio); }
static void cartaoAtualizarSi(float dt, Uint32 t) { (void)t; sintro_atualizar(dt, relogio); }
// O cartao do lembrete conta o prazo pelo SDL_GetTicks (desde), nao pelo relogio do teste.
static void cartaoAtualizarGl(float dt, Uint32 t) { glem_passo(dt, t); }
static int glemEv(const SDL_Event *e) { return glem_evento(e); }
static void glemEvento(const SDL_Event *e) { (void)glemEv(e); }
static int lerPip(void) { return pipintro_teste_foco(); }
static int lerSi(void) { return sintro_teste_foco(); }
static int lerGl(void) { return glem_teste_foco(); }
static void testeCartoes(void) {
  static const Tela PIP = { pipintro_evento, cartaoAtualizarPip, cartaoDesenhar };
  static const Tela SI  = { sintro_evento, cartaoAtualizarSi, cartaoDesenhar };
  static const Tela GL  = { glemEvento, cartaoAtualizarGl, cartaoDesenhar };
  int n, d;
  printf("\n== Cartoes (PiP, Salvos, lembrete do guia)\n");
  tela = &PIP; cartaoDes = pipintro_desenhar;
  pipintro_abrir();
  mover(5, 5); quadros(60);
  d = distintos(lerPip, &n);
  CONFERE(pipintro_aberto() && n == 2 && d == 2, "pip: os dois botoes sao alvos e ganham o foco");
  tela = &SI; cartaoDes = sintro_desenhar;
  sintro_primeira_vez();
  mover(5, 5); quadros(60);
  d = distintos(lerSi, &n);
  CONFERE(sintro_aberto() && n == 2 && d == 2, "salvos (intro): as duas opcoes sao alvos e ganham o foco");
  tela = &GL; cartaoDes = glem_desenhar;
  glem_teste_cartao("Jornal", "Canal 5", 0);
  mover(5, 5); quadros(60);
  d = distintos(lerGl, &n);
  { char id[64], nome[64], base[256];
    CONFERE(glem_cartao_aberto() && n == 2 && d == 2 &&
            !glem_pediu_assistir(id, sizeof id, nome, sizeof nome, base, sizeof base),
            "lembrete do guia: Assistir e Dispensar ganham o foco sem assistir"); }
}

// --- central de avisos (painel proprio; NUVIO_AVISOS_DEMO tem linhas) -----
static int lerAviso(void) { return avisos_teste_foco(); }
static int avisosEv(const SDL_Event *e) { return avisos_evento(e); }
static void avisosEvento(const SDL_Event *e) { (void)avisosEv(e); }
static void testeAvisos(void) {
  static const Tela T = { avisosEvento, avisos_atualizar, avisos_desenhar };
  int n, d;
  printf("\n== Central de avisos\n");
  tela = &T;
  avisos_abrir();
  mover(5, 5); quadros(90);
  d = distintos(lerAviso, &n);
  CONFERE(n >= 2, "avisos: as linhas sao alvos");
  CONFERE(d >= 2, "avisos: passar por cima foca linhas diferentes");
  captura("avisos-hover-linha.png");
  { SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; entregar(&e); quadros(40); }
}

// --- Agenda: tres episodios futuros, sem rede -----------------------------
static char *semRede(const char *url, int seg, const char *const *cab, int *st) {
  (void)url; (void)seg; (void)cab; if (st) *st = 0; return NULL;
}
static int lerAgenda(void) { int l, c; agendaui_teste_foco(&l, &c, NULL, NULL, NULL, NULL); return c ? -100 - c : l; }
static void testeAgenda(void) {
  static const Tela T = { agendaui_evento, agendaui_atualizar, agendaui_desenhar };
  int n, d;
  printf("\n== Agenda\n");
  agenda_rede_teste(semRede);
  agenda_definir_hoje("2026-09-16");
  agenda_iniciar();
  agenda_registrar("tt5550001", "Slow Horses", "deploy/app/art/07.jpg", "Returning Series", 5, 3,
                   "Uncle Sam", "2026-09-18", "2026-09-10");
  agenda_registrar("tt5550002", "The Diplomat", "deploy/app/art/08.jpg", "Returning Series", 3, 2,
                   "Episode 2", "2026-09-20", "2026-09-12");
  agenda_registrar("tt5550003", "Andor", "deploy/app/art/09.jpg", "Returning Series", 2, 4,
                   "Episode 4", "2026-09-25", "2026-09-11");
  agenda_montar();
  tela = &T;
  agendaui_iniciar();
  mover(5, 5); quadros(60);
  d = distintos(lerAgenda, &n);
  printf("     (agenda: %d itens, %d alvos, %d focos)\n", agenda_n(), n, d);
  CONFERE(n >= 3, "agenda: botoes e linhas sao alvos");
  CONFERE(d >= 3, "agenda: passar por cima foca itens diferentes");
  captura("agenda-hover-linha.png");
}

int main(int argc, char **argv) {
  const char *quero = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  pasta = argc > 1 ? argv[1] : NULL;
  if (!quero || !*quero) { printf("RECUSADO: rode por tests/ponteiro_telas.sh\n"); return 2; }
  dados_iniciar(quero);
  if (strcmp(dados_dir(), quero)) { printf("RECUSADO: dados_dir() nao e a pasta do teste\n"); return 2; }
  { const char *f = getenv("NUVIO_SHOT_FONTE");
    char cam[700]; FILE *fa;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
    fa = fopen(cam, "w");
    if (fa) { fprintf(fa, "idioma 0\n"); if (f && *f) fprintf(fa, "fonteInterface %d\n", atoi(f)); fclose(fa); } }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: ponteiro nas telas", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  extras_carregar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  // Relogio e janela injetados: o hit-test converte 1920x1080 -> logico 1:1 e
  // os freios de tempo andam pelo relogio do teste.
  ponteiro_teste_relogio(agora);
  ponteiro_teste_janela(1920, 1080);
  ponteiro_teste_toque(0);

  testeBiblioteca();
  testeAjustes();
  testeAjustesEditor();
  testeAddons();
  testePlugins();
  testePainel();
  testeGuia();
  testeDiagnostico();
  testeLiveTvDiag();
  testeNovidades142();
  testeAgenda();
  setenv("NUVIO_AVISOS_DEMO", "1", 1);
  avisos_iniciar();
  testeAvisos();
  testeAmigo();
  testePlayer();
  testeDvtela();
  testePosplay();
  testeCartoes();

  printf("\n%s: %d falha(s)\n", falhas ? "FALHOU" : "PASSOU", falhas);
  return falhas ? 1 : 0;
}
