// "Trocar arte" (#142). Ver trocaarte.h para o que a tela faz e de onde vem
// cada miniatura; aqui ficam o fio do TMDB, a grade e a previa.
#include "trocaarte.h"
#include "arteescolha.h"
#include "artehero.h"
#include "ajustes.h"
#include "descoberta.h"
#include "botoes.h"
#include "gfx.h"
#include "idioma.h"
#include "js.h"
#include "layout.h"
#include "linguas.h"
#include "detail.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include "rede.h"
#include "tex_cache.h"
#include "text.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Teto por aba. O TMDB tem titulo com 68 backdrops (Um Sonho de Liberdade,
// medido em 23/09); 36 sao sete linhas e meia de grade, mais do que alguem
// percorre no controle — e cada uma e uma textura de 300 px no cache.
// O FILTRO DE IDIOMA (relato arabe, .tpk 4/5: "so aparecem opcoes em ingles")
// pede ao TMDB TODOS os idiomas e deixa a pessoa filtrar; o teto sobe para que
// as artes de um idioma menos votado ainda entrem na lista.
#define TA_MAX 96
#define TA_TMDB_FUNDOS 72
#define TA_TMDB_LOGOS  56
#define TA_LER_FUNDOS 160
#define TA_LER_LOGOS  96

typedef struct {
  char url[ARTEESC_URL];   // o que vai para o disco (vazio = Automatico)
  char mostra[512];        // o que a miniatura pede (Automatico: a regra)
  char rotulo[40];
  char iso[8];             // idioma do TMDB: "" nao e do TMDB, "-" sem texto
} TaCand;

static TaCand cand[2][TA_MAX];
static int nCand[2];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int geracao;          // fio de uma abertura anterior nao publica aqui
static int buscando;         // o fio do TMDB ainda nao respondeu

static int aberto;
static float mola;           // 0..1, entrada e saida
static int aba;              // 0 fundos, 1 logos
static int naAba;            // foco na linha das abas (e nao na grade)
static int naFiltro;         // ...e, nela, no chip de idioma
static char filtro[8];       // idioma mostrado nas duas abas ("" = todos, "-" = sem texto)
static int foco[2];          // indice em cand[aba], nao na lista visivel
static int topo[2];          // primeira linha visivel da grade
#ifdef NV_TOUCH_UI
static ToqueRolagem toque[2];
static float toqueY[2];
static int toqueAba;
static void toqueRetomar(void);
static int toqueRolar(const PonteiroRolagem *e) {
  if (!aberto) return 0;
  if (e->fase == PONT_ROL_INICIO) toqueAba = aba;
  return toquerol_evento(&toque[toqueAba], e);
}
#endif
static int okDesceu;
static int mudou;
static Uint32 focoDesde;
static CatItem item;
static char chave[64];
static char escolhido[2][ARTEESC_URL];   // o que estava valendo ao abrir
static char previa[512];                 // fundo ja pronto na tela cheia
static char previaLogo[512];             // logo ja pronto ("" = so o nome)
static int  previaLogoTem;

// --- grade (px de layout 1920x1080) ---------------------------------------
// Cinco miniaturas por linha, duas linhas visiveis: 5x304 + 4x24 = 1616 cabe
// na margem de 96 da pagina com folga. A primeira linha comeca em 628 para a
// metade de cima da tela ficar inteira para a previa — e para o que se esta
// escolhendo, o fundo, ser o assunto.
#define TA_COLS   taColunas()
#define TA_LINHAS 2
#define TA_W      304.0f
#define TA_H      171.0f
#define TA_GAP    24.0f
#define TA_X0     96.0f
#define TA_Y0     (telefoneui_ativo() && NV_TELA_W < 1500 ? 756.0f : 628.0f)
#define TA_PASSO  (TA_H + 48.0f)

static int taColunas(void) {
  if (!telefoneui_ativo()) return 5;
  int n = (int)((NV_TELA_W - 2 * TA_X0 + TA_GAP) / (TA_W + TA_GAP));
  return n < 1 ? 1 : n > 5 ? 5 : n;
}

// A MESMA FOTO em outro tamanho conta como repetida: o catalogo manda o
// backdrop do TMDB em w1280 e a lista do /images vem com o mesmo arquivo.
static void chaveFoto(const char *u, char *k, size_t n) {
  const char *p = u ? strstr(u, "/t/p/") : NULL;
  const char *nome = p ? strchr(p + 5, '/') : NULL;
  snprintf(k, n, "%s", nome ? nome : (u ? u : ""));
}

// Chamar com a trava.
static int jaTem(int a, const char *url) {
  char k[512], k2[512];
  int i;
  chaveFoto(url, k, sizeof k);
  for (i = 0; i < nCand[a]; i++) {
    chaveFoto(cand[a][i].mostra, k2, sizeof k2);
    if (k2[0] && !strcmp(k, k2)) return 1;
  }
  return 0;
}

// Chamar com a trava. `url` e o que se grava; `mostra` o que a miniatura pede
// (NULL = a propria url).
static void porIso(int a, const char *url, const char *mostra, const char *rotulo,
                   const char *iso) {
  TaCand *c;
  if (nCand[a] >= TA_MAX) return;
  if (!mostra) mostra = url;
  if (!mostra || !mostra[0] || strlen(url ? url : "") >= ARTEESC_URL) return;
  if (nCand[a] > 0 && jaTem(a, mostra)) return;
  c = &cand[a][nCand[a]++];
  snprintf(c->url, sizeof c->url, "%s", url ? url : "");
  snprintf(c->mostra, sizeof c->mostra, "%s", mostra);
  snprintf(c->rotulo, sizeof c->rotulo, "%s", rotulo ? rotulo : "");
  snprintf(c->iso, sizeof c->iso, "%s", iso ? iso : "");
}
static void por(int a, const char *url, const char *mostra, const char *rotulo) {
  porIso(a, url, mostra, rotulo, "");
}

// --- o fio do TMDB ----------------------------------------------------------
typedef struct {
  int ger;
  char imdb[32];
  long tmdb;
  int serie;
  char lingua[4];
} TaPedido;

typedef struct { char fp[96]; char iso[8]; double nota; } TaImg;

static const char *linguaAlvo;
// Fundo: o do idioma dos metadados, o sem idioma (a foto sem letreiro, a que
// combina com o nosso logo), o ingles, os outros; dentro de cada um, a mais
// votada. A mesma ordem da escolha automatica (af_tmdb_fundo_padrao).
static int pesoFundo(const TaImg *x) {
  if (linguaAlvo && linguaAlvo[0] && !strcmp(x->iso, linguaAlvo)) return 0;
  if (!x->iso[0]) return 1;
  if (!strcmp(x->iso, "en")) return 2;
  return 3;
}
static int cmpFundo(const void *a, const void *b) {
  const TaImg *x = (const TaImg *)a, *y = (const TaImg *)b;
  int px = pesoFundo(x), py = pesoFundo(y);
  if (px != py) return px - py;
  if (px == 3) { int c = strcmp(x->iso, y->iso); if (c) return c; }   // outros agrupados
  return x->nota < y->nota ? 1 : x->nota > y->nota ? -1 : 0;
}
// Logo: o do idioma, o ingles, o sem idioma, os outros (af_tmdb_logo).
static int pesoLogo(const TaImg *x) {
  if (linguaAlvo && linguaAlvo[0] && !strcmp(x->iso, linguaAlvo)) return 0;
  if (!strcmp(x->iso, "en")) return 1;
  if (!x->iso[0]) return 2;
  return 3;
}
static int cmpLogo(const void *a, const void *b) {
  const TaImg *x = (const TaImg *)a, *y = (const TaImg *)b;
  int px = pesoLogo(x), py = pesoLogo(y);
  if (px != py) return px - py;
  if (px == 3) { int c = strcmp(x->iso, y->iso); if (c) return c; }
  return x->nota < y->nota ? 1 : x->nota > y->nota ? -1 : 0;
}

static int lerImagens(const char *corpo, const char *campo, TaImg *v, int max) {
  const char *p = js_array(corpo, NULL, campo);
  int n = 0;
  while (p && n < max) {
    const char *f = js_fim(p);
    TaImg *m = &v[n];
    memset(m, 0, sizeof *m);
    js_texto(p, f, "file_path", m->fp, sizeof m->fp);
    js_texto(p, f, "iso_639_1", m->iso, sizeof m->iso);
    m->nota = js_num(p, f, "vote_average", 0.0);
    { size_t L = strlen(m->fp);
      // SVG nao decodifica (ver artehero_url_logo): fora antes de virar
      // miniatura que falha.
      if (m->fp[0] == '/' && !(L > 4 && !strcmp(m->fp + L - 4, ".svg"))) n++; }
    p = js_prox(f);
  }
  return n;
}

// Le backdrops/logos de uma resposta do /images, ordena (ver pesoFundo e
// pesoLogo) e publica na grade se a abertura `ger` ainda e a vigente.
static void publicarTmdb(const char *resp, const char *lingua, int ger) {
  // NO HEAP, e nao estatico: fechar e reabrir depressa deixa DOIS fios no ar
  // (o velho so descarta o resultado no fim), e os dois escreveriam na mesma
  // tabela. ~28 KB por abertura.
  TaImg *fundos = (TaImg *)malloc(TA_LER_FUNDOS * sizeof *fundos);
  TaImg *logos = (TaImg *)malloc(TA_LER_LOGOS * sizeof *logos);
  int nf, nl, i;
  if (!fundos || !logos) { free(fundos); free(logos); return; }
  nf = lerImagens(resp, "backdrops", fundos, TA_LER_FUNDOS);
  nl = lerImagens(resp, "logos", logos, TA_LER_LOGOS);
  // qsort nao passa contexto: o idioma vai por estatico, sob a trava para
  // dois fios nao trocarem o idioma um do outro no meio da ordenacao.
  pthread_mutex_lock(&trava);
  linguaAlvo = lingua;
  qsort(fundos, (size_t)nf, sizeof *fundos, cmpFundo);
  qsort(logos, (size_t)nl, sizeof *logos, cmpLogo);
  linguaAlvo = NULL;
  if (ger == geracao) {
    char url[256], rot[40];
    for (i = 0; i < nf && i < TA_TMDB_FUNDOS; i++) {
      // w1280 e o tamanho do CARD; a tela cheia sobe pela qualidade
      // (artehero_url_escolha_grande) e a miniatura desce para w300.
      snprintf(url, sizeof url, "https://image.tmdb.org/t/p/w1280%s", fundos[i].fp);
      if (fundos[i].iso[0]) snprintf(rot, sizeof rot, "TMDB · %.2s", fundos[i].iso);
      else snprintf(rot, sizeof rot, "TMDB");
      { char *c; for (c = rot; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32; }
      porIso(0, url, NULL, rot, fundos[i].iso[0] ? fundos[i].iso : "-");
    }
    for (i = 0; i < nl && i < TA_TMDB_LOGOS; i++) {
      snprintf(url, sizeof url, "https://image.tmdb.org/t/p/w500%s", logos[i].fp);
      if (logos[i].iso[0]) snprintf(rot, sizeof rot, "TMDB · %.2s", logos[i].iso);
      else snprintf(rot, sizeof rot, "TMDB");
      { char *c; for (c = rot; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32; }
      porIso(1, url, NULL, rot, logos[i].iso[0] ? logos[i].iso : "-");
    }
  }
  pthread_mutex_unlock(&trava);
  free(fundos);
  free(logos);
}

static void *fioTmdb(void *arg) {
  TaPedido *q = (TaPedido *)arg;
  const char *chaveApi = desc_chave_tmdb_reserva();
  char api[512];
  char *resp;
  long id = q->tmdb;
  int serie = q->serie;
  if (!chaveApi[0]) goto fim;
  if (id <= 0 && !strncmp(q->imdb, "tt", 2)) {
    const char *v;
    snprintf(api, sizeof api,
             "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
             q->imdb, chaveApi);
    resp = rede_baixar(api, 8);
    if (!resp) goto fim;
    v = js_array(resp, NULL, serie ? "tv_results" : "movie_results");
    if (!v) { v = js_array(resp, NULL, serie ? "movie_results" : "tv_results"); if (v) serie = !serie; }
    if (v) id = (long)js_num(v, js_fim(v), "id", 0.0);
    free(resp);
  }
  if (id <= 0) goto fim;
  // TODOS OS IDIOMAS (sem include_image_language): a lista vinha so com o
  // idioma dos metadados, o sem texto e o ingles, e quem queria um logo arabe
  // com os metadados em ingles nao tinha como. A ordem (publicarTmdb) poe o
  // idioma dos metadados primeiro, e o chip de idioma filtra.
  snprintf(api, sizeof api, "https://api.themoviedb.org/3/%s/%ld/images?api_key=%s",
           serie ? "tv" : "movie", id, chaveApi);
  resp = rede_baixar(api, 8);
  if (!resp) goto fim;
  publicarTmdb(resp, q->lingua, q->ger);
  free(resp);
fim:
  pthread_mutex_lock(&trava);
  if (q->ger == geracao) buscando = 0;
  pthread_mutex_unlock(&trava);
  free(q);
  return NULL;
}

// --- abrir / fechar ----------------------------------------------------------
static int ehSerie(void) { return !strcmp(item.tipo, "series"); }

void trocaarte_abrir(const CatItem *it) {
  const char *u;
  char tt[32] = "";
  if (!it) return;
  item = *it;
  artehero_id_escolha(&item, chave, sizeof chave);
  if (!chave[0]) return;
  aberto = 1; aba = 0; naAba = 0; naFiltro = 0; okDesceu = 0; mudou = 0;
  filtro[0] = 0;
  foco[0] = foco[1] = 0; topo[0] = topo[1] = 0;
#ifdef NV_TOUCH_UI
  memset(toque, 0, sizeof toque); memset(toqueY, 0, sizeof toqueY);
#endif
  previa[0] = previaLogo[0] = 0; previaLogoTem = 0;
  focoDesde = SDL_GetTicks();
  { const char *f = arteesc_fundo(chave), *l = arteesc_logo(chave);
    snprintf(escolhido[0], sizeof escolhido[0], "%s", f ? f : "");
    snprintf(escolhido[1], sizeof escolhido[1], "%s", l ? l : ""); }
  if (!strncmp(item.imdb, "tt", 2)) {
    size_t i = 0;
    while (item.imdb[i] && item.imdb[i] != ':' && i + 1 < sizeof tt) { tt[i] = item.imdb[i]; i++; }
    tt[i] = 0;
  }

  pthread_mutex_lock(&trava);
  geracao++;
  nCand[0] = nCand[1] = 0;
  // AUTOMATICO: o que a pagina mostraria sem escolha nenhuma — a MESMA conta
  // de arteDeViva em detail.c, com a escolha suspensa.
  artehero_escolha_suspender(1);
  u = artehero_url_destaque(&item, ajustes_hero_fonte(), ajustes_hero_arte_diferente());
  if (!u && item.poster[0]) u = item.poster;
  // SEMPRE na posicao 0, mesmo sem arte nenhuma (miniatura vazia): e a unica
  // porta de volta para a regra, e escolher() conta com ela ali.
  { TaCand *c = &cand[0][nCand[0]++];
    snprintf(c->url, sizeof c->url, "%s", "");
    snprintf(c->mostra, sizeof c->mostra, "%s", u ? u : "");
    snprintf(c->rotulo, sizeof c->rotulo, "%s", "Automático"); }
  { const char *l = item.logo[0] ? artehero_url_logo(item.logo) : NULL;
    // Sem logo nenhum o Automatico continua existindo: a miniatura mostra o
    // nome escrito, que e o que a pagina desenha.
    TaCand *c = &cand[1][nCand[1]++];
    snprintf(c->url, sizeof c->url, "%s", "");
    snprintf(c->mostra, sizeof c->mostra, "%s", l ? l : "");
    snprintf(c->rotulo, sizeof c->rotulo, "%s", "Automático"); }
  artehero_escolha_suspender(0);
  // A ESCOLHA VIGENTE LOGO DEPOIS DO AUTOMATICO: ela pode ter vindo de uma
  // lista do TMDB que hoje nao responde, e reabrir a tela para conferir tem
  // de mostrar (e focar) a foto que esta valendo. A mesma foto vinda do fio
  // depois cai como repetida.
  if (escolhido[0][0]) por(0, escolhido[0], NULL, "Sua escolha");
  if (escolhido[1][0]) {
    const char *l = artehero_url_logo(escolhido[1]);
    if (l) { char m[512]; snprintf(m, sizeof m, "%s", l); por(1, escolhido[1], m, "Sua escolha"); }
  }

  // FUNDOS DE FONTE UNICA, na ordem de trocaarte.h.
  { static const struct { int fonte; const char *rot; } F[] = {
      { ARTEHERO_CATALOGO, "Catálogo" }, { ARTEHERO_METAHUB, "Metahub" },
      { ARTEHERO_APPLE, "Apple TV" }, { ARTEHERO_FANART, "fanart.tv" } };
    int i;
    for (i = 0; i < (int)(sizeof F / sizeof *F); i++) {
      u = artehero_url_fonte(&item, F[i].fonte);
      if (u && u[0]) { char copia[512]; snprintf(copia, sizeof copia, "%s", u);
                       por(0, copia, NULL, F[i].rot); }
    } }
  if (item.backdropTmdb[0]) por(0, item.backdropTmdb, NULL, "TMDB");
  // LOGOS: o do catalogo (se nao e o mesmo do Automatico) e o do metahub.
  if (item.logo[0]) por(1, item.logo, artehero_url_logo(item.logo), "Catálogo");
  if (tt[0]) {
    char m[160];
    snprintf(m, sizeof m, "https://images.metahub.space/logo/medium/%s/img", tt);
    por(1, m, NULL, "Metahub");
  }
  // O FOCO COMECA NA ESCOLHA VIGENTE, e nao no Automatico: reabrir para
  // conferir nao pode parecer que a escolha foi perdida.
  { int a, i;
    for (a = 0; a < 2; a++)
      for (i = 1; i < nCand[a]; i++)
        if (escolhido[a][0] && !strcmp(cand[a][i].url, escolhido[a])) foco[a] = i; }
  buscando = 0;
  { TaPedido *q = (TaPedido *)calloc(1, sizeof *q);
    if (q && (tt[0] || item.tmdb > 0)) {
      pthread_t f;
      q->ger = geracao;
      snprintf(q->imdb, sizeof q->imdb, "%s", tt);
      q->tmdb = item.tmdb;
      q->serie = ehSerie();
      snprintf(q->lingua, sizeof q->lingua, "%.2s", desc_tmdb_idioma());
      buscando = 1;
      if (pthread_create(&f, NULL, fioTmdb, q) == 0) pthread_detach(f);
      else { buscando = 0; free(q); }
    } else free(q);
  }
  pthread_mutex_unlock(&trava);
  printf("[arte] trocar arte de %s: %d fundo(s), %d logo(s) antes do TMDB\n",
         chave, nCand[0], nCand[1]);
  fflush(stdout);
}

void trocaarte_fechar(void) {
  if (!aberto) return;
  aberto = 0;
  pthread_mutex_lock(&trava);
  geracao++;             // o fio que ainda estiver no ar descarta o resultado
  buscando = 0;
  pthread_mutex_unlock(&trava);
}

int trocaarte_aberto(void) { return aberto; }
float trocaarte_visivel(void) { return mola; }

int trocaarte_consumir_mudanca(void) { int v = mudou; mudou = 0; return v; }

// --- lista visivel -----------------------------------------------------------
// A miniatura que falhou (Apple sem o titulo, fanart sem chave valida, 404)
// sai da grade. O Automatico nunca sai: e a porta de volta.
static int visiveis(int a, int *v) {
  int i, n = 0;
  for (i = 0; i < nCand[a]; i++) {
    if (i > 0 && tex_falhou(cand[a][i].mostra)) continue;
    // Com filtro de idioma, so o TMDB daquele idioma (e o Automatico).
    if (i > 0 && filtro[0] && strcmp(cand[a][i].iso, filtro)) continue;
    v[n++] = i;
  }
  return n;
}

// --- filtro de idioma ----------------------------------------------------------
// Os idiomas do TMDB presentes nas duas abas, na ordem da lista (a do idioma
// dos metadados primeiro). UM filtro para as duas abas: o chip fica a direita
// de "Logos", e um filtro so dos fundos nao teria como ser alcancado sem sair
// da aba. Chamar com a trava. Devolve quantos (sem o "todos").
#define TA_FILTROS 24
static int idiomasDaAba(int aba_, char out[][8]) {
  int a, i, k, n = 0;
  (void)aba_;
  for (a = 0; a < 2; a++)
    for (i = 1; i < nCand[a] && n < TA_FILTROS; i++) {
      const char *iso = cand[a][i].iso;
      if (!iso[0] || tex_falhou(cand[a][i].mostra)) continue;
      for (k = 0; k < n && strcmp(out[k], iso); k++) {}
      if (k == n) snprintf(out[n++], 8, "%s", iso);
    }
  return n;
}

// Proximo filtro: todos -> cada idioma -> todos. Chamar com a trava.
static void filtroProximo(int a) {
  char l[TA_FILTROS][8];
  int n = idiomasDaAba(a, l), k;
  if (n < 2) { filtro[0] = 0; return; }
  if (!filtro[0]) { snprintf(filtro, sizeof filtro, "%s", l[0]); }
  else {
    for (k = 0; k < n && strcmp(l[k], filtro); k++) {}
    if (k + 1 < n) snprintf(filtro, sizeof filtro, "%s", l[k + 1]);
    else filtro[0] = 0;
  }
  topo[0] = topo[1] = 0;
#ifdef NV_TOUCH_UI
  memset(toque, 0, sizeof toque); memset(toqueY, 0, sizeof toqueY);
#endif
  previa[0] = 0;
  focoDesde = SDL_GetTicks();
  printf("[arte] trocar arte: filtro de idioma %s\n", filtro[0] ? filtro : "todos");
  fflush(stdout);
}

// Rotulo do chip: "Idioma: Todos", "Idioma: Árabe", "Idioma: Sem texto".
static void rotuloFiltro(int a, char *dst, size_t n) {
  const char *f = filtro;
  (void)a;
  snprintf(dst, n, "%s: %s", i18n("Idioma"),
           !f[0] ? i18n("Todos") : !strcmp(f, "-") ? i18n("Sem texto") : i18n(ling_nome(f)));
}
static int posDe(const int *v, int n, int c) {
  int i, melhor = 0;
  for (i = 0; i < n; i++) { if (v[i] == c) return i; if (v[i] < c) melhor = i; }
  return melhor;
}

static const char *urlPrevia(const TaCand *c, int a) {
  if (a == 0) return c->url[0] ? artehero_url_escolha_grande(c->url) : c->mostra;
  return c->mostra;
}

void trocaarte_atualizar(float dt) {
  float alvo = aberto ? 1.0f : 0.0f;
  float passo = dt * 7.0f;
  if (mola < alvo) mola = mola + passo > alvo ? alvo : mola + passo;
  else if (mola > alvo) mola = mola - passo < alvo ? alvo : mola - passo;
  if (!aberto) return;
  pthread_mutex_lock(&trava);
  { int v[TA_MAX], n = visiveis(aba, v), p;
    if (n > 0) {
      p = posDe(v, n, foco[aba]);
      foco[aba] = v[p];
      // A linha focada sempre a vista.
#ifdef NV_TOUCH_UI
      if (!toque[aba].livre) {
#endif
        if (p / TA_COLS < topo[aba]) topo[aba] = p / TA_COLS;
        if (p / TA_COLS >= topo[aba] + TA_LINHAS) topo[aba] = p / TA_COLS - TA_LINHAS + 1;
#ifdef NV_TOUCH_UI
      }
#endif
      // PREVIA: so troca quando a textura grande do foco ja esta pronta — a
      // pagina nunca pisca para o vazio entre uma foto e outra. Pedida so
      // depois de o foco parar (ver trocaarte.h).
      if (SDL_GetTicks() - focoDesde >= 200) {
        const TaCand *c = &cand[aba][foco[aba]];
        const char *u = urlPrevia(c, aba);
        if (aba == 0 && u && u[0]) {
          if (tex_obter_hero(u)) snprintf(previa, sizeof previa, "%s", u);
        } else if (aba == 1) {
          if (!u || !u[0]) { previaLogo[0] = 0; previaLogoTem = 1; }
          else if (tex_obter_larg_qualquer(u, NV_DETW_LOGO_MAXW * 0.62f)) {
            snprintf(previaLogo, sizeof previaLogo, "%s", u); previaLogoTem = 1;
          }
        }
      }
    } }
  pthread_mutex_unlock(&trava);
}

const char *trocaarte_previa_fundo(void) {
  if (!aberto || !previa[0]) return NULL;
  return previa;
}

// --- teclado -----------------------------------------------------------------
static void escolher(void) {
  const TaCand *c = &cand[aba][foco[aba]];
  int m = aba == 0 ? arteesc_definir_fundo(chave, c->url)
                   : arteesc_definir_logo(chave, c->url);
  { char lb[160];
    printf("[arte] %s de %s: %s\n", aba == 0 ? "fundo" : "logo", chave,
           c->url[0] ? rede_url_log(c->url, lb, sizeof lb) : "automatico"); }
  fflush(stdout);
  if (m) mudou = 1;
  trocaarte_fechar();
}

static void mover(int dx, int dy) {
  int v[TA_MAX], n, p, alvo;
  pthread_mutex_lock(&trava);
  n = visiveis(aba, v);
  p = posDe(v, n, foco[aba]);
  if (naAba) {
    char l[TA_FILTROS][8];
    int temFiltro = idiomasDaAba(aba, l) >= 2;
    if (naFiltro) {
      if (dx < 0 || !temFiltro) naFiltro = 0;
      else if (dy > 0) { naFiltro = 0; naAba = 0; }
    } else if (dx) {
      int nova = aba + dx;
      if (nova >= 0 && nova <= 1) { aba = nova; previa[0] = 0; focoDesde = SDL_GetTicks(); }
      else if (nova == 2 && temFiltro) naFiltro = 1;
    } else if (dy > 0) naAba = 0;
    pthread_mutex_unlock(&trava);
    return;
  }
  alvo = p;
  if (dx) {
    // Esquerda/direita nao mudam de linha: e uma grade, nao uma fita.
    int col = p % TA_COLS + dx;
    if (col >= 0 && col < TA_COLS && p + dx < n) alvo = p + dx;
  } else if (dy < 0) {
    if (p < TA_COLS) naAba = 1; else alvo = p - TA_COLS;
  } else if (dy > 0) {
    if (p + TA_COLS < n) alvo = p + TA_COLS;
    else if (p / TA_COLS < (n - 1) / TA_COLS) alvo = n - 1;   // ultima linha curta
  }
  if (alvo != p && alvo >= 0 && alvo < n) { foco[aba] = v[alvo]; focoDesde = SDL_GetTicks(); }
  pthread_mutex_unlock(&trava);
}

void trocaarte_evento(const SDL_Event *e) {
  if (!aberto) return;
  if (e->type == SDL_KEYDOWN) {
    SDL_Keycode k = e->key.keysym.sym;
#ifdef NV_TOUCH_UI
    if (toquerol_navegacao(e)) toqueRetomar();
#endif
    if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
        k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
      trocaarte_fechar();
      return;
    }
    switch (k) {
      case SDLK_LEFT:  mover(-1, 0); break;
      case SDLK_RIGHT: mover(1, 0); break;
      case SDLK_UP:    mover(0, -1); break;
      case SDLK_DOWN:  mover(0, 1); break;
      case SDLK_RETURN: case SDLK_KP_ENTER: okDesceu = 1; break;
      default: break;
    }
    return;
  }
  // OK NO SOLTAR, e so se o APERTAR foi aqui: o KEYUP do OK que abriu esta
  // tela (no circular da pagina) chega depois dela aberta e nao pode escolher
  // o Automatico sozinho — o mesmo defeito que detail_evento conta.
  if (e->type == SDL_KEYUP && (e->key.keysym.sym == SDLK_RETURN ||
                               e->key.keysym.sym == SDLK_KP_ENTER)) {
    if (!okDesceu) return;
    okDesceu = 0;
    if (naAba && naFiltro) {
      pthread_mutex_lock(&trava);
      filtroProximo(aba);
      pthread_mutex_unlock(&trava);
      return;
    }
    if (naAba) { naAba = 0; return; }
    escolher();
  }
}

// --- ponteiro ----------------------------------------------------------------
static void ponteiroFoco(int a, int b) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque[aba]);
#endif
  if (a == -2) { naAba = 1; naFiltro = 1; return; }
  if (a < 0) { naAba = 1; naFiltro = 0; if (b != aba) { aba = b; previa[0] = 0; } focoDesde = SDL_GetTicks(); return; }
  naAba = 0; naFiltro = 0;
  if (b >= 0 && b < nCand[aba] && foco[aba] != b) { foco[aba] = b; focoDesde = SDL_GetTicks(); }
}
#ifdef NV_TOUCH_UI
static void toqueRetomar(void) {
  if (toque[aba].livre) {
    int v[TA_MAX], n, p;
    pthread_mutex_lock(&trava);
    n = visiveis(aba, v);
    p = (int)((toqueY[aba] + toque[aba].regiao.h * 0.35f) / TA_PASSO) * TA_COLS;
    if (p >= n) p = n - 1;
    if (p >= 0) { foco[aba] = v[p]; naAba = naFiltro = 0; topo[aba] = (int)(toqueY[aba] / TA_PASSO); }
    pthread_mutex_unlock(&trava);
  }
  toquerol_limpar(&toque[aba]);
}
#endif

// --- desenho -----------------------------------------------------------------
static void desenhaLogo(const char *u, GfxRect caixa, float a, int esq) {
  GLuint t = u && u[0] ? tex_obter_larg_qualquer(u, caixa.w) : 0;
  float asp, w, h;
  if (!t) {
    TxtLinha l = txt_linha_corta(caixa.w > 400 ? TXT_TITULO2 : TXT_DET_META2,
                                 item.titulo, 255, 255, 255, 255, caixa.w);
    txt_desenhar_alpha(l, esq ? caixa.x : caixa.x + (caixa.w - l.w) * 0.5f,
                       caixa.y + (caixa.h - l.h) * 0.5f, a);
    return;
  }
  asp = tex_aspecto(u);
  if (asp <= 0.0f) asp = 2.5f;
  w = caixa.w; h = w / asp;
  if (h > caixa.h) { h = caixa.h; w = h * asp; }
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ esq ? caixa.x : caixa.x + (caixa.w - w) * 0.5f,
                      esq ? caixa.y + caixa.h - h : caixa.y + (caixa.h - h) * 0.5f, w, h },
           t, tex_marca_escura(u) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}

static void desenhaAba(GfxRect r, const char *rot, int ativa, int focada, float a) {
  float cr = 0.14f, cg = 0.15f, cb = 0.17f, tinta = 235.0f / 255.0f;
  int c;
  if (focada) { tinta = ajustes_acento_tinta(&cr, &cg, &cb); botao_luz(r, 1.0f, a); }
  else if (ativa) { cr = cg = cb = 0.92f; tinta = 0.06f; }
  gfx_cor(r, NV_RAIO_PILL, cr, cg, cb, a);
  c = (int)(tinta * 255.0f + 0.5f);
  { TxtLinha l = telefoneui_ativo() ? txt_linha_corta(TXT_DET_META2, rot, c, c, c, 255, r.w - 40)
                                  : txt_linha(TXT_DET_META2, rot, c, c, c, 255);
    txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f, a); }
}

void trocaarte_desenhar(const char *logoPagina) {
  float a = mola;
  int v[TA_MAX], n, i, p;
  if (a <= 0.005f) return;
  ponteiro_camada();
  // A transicao ainda cobre o detalhe quando os controles ja apagaram.
  // O anteparo evita que um toque no vazio vire RETURN na pagina de baixo.
  if (telefoneui_ativo())
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  pthread_mutex_lock(&trava);
  n = visiveis(aba, v);
  p = posDe(v, n, foco[aba]);

  // O LOGO, onde a pagina o poria: e metade do "resultado" que se esta vendo.
  { const char *lg = logoPagina;
    if (aba == 1 && previaLogoTem) lg = previaLogo;
    desenhaLogo(lg, (GfxRect){ NV_DETW2_X, 120.0f, 620.0f, 230.0f }, a, 1); }

  // VEU de baixo: da metade para baixo a grade precisa de chao escuro, e a
  // metade de cima fica limpa para a previa.
  // A rampa termina OPACA logo acima do cabecalho (medido na captura: a
  // legenda em cinza sumia sobre um fundo claro com a rampa comecando em 360)
  // e o chao continua dali sem emenda.
  gfx_rect((GfxRect){ 0, 200.0f, NV_TELA_W, 340.0f }, 0, GFX_VEU_BAIXO,
           0, 0, 0, 0.0f, 0.035f, 0.038f, 0.045f, a);
  gfx_cor((GfxRect){ 0, 526.0f, NV_TELA_W, NV_TELA_H - 526.0f }, 0.0f,
          0.035f, 0.038f, 0.045f, a);

  // CABECALHO: titulo, as duas abas e a legenda do controle.
  { TxtLinha t = txt_linha(TXT_HEADLINE, "Trocar arte", 245, 248, 255, 255);
    float x = TA_X0, y = 540.0f;
    txt_desenhar_alpha(t, x, y + (52.0f - t.h) * 0.5f, a);
    if (telefoneui_ativo() && NV_TELA_W < 1500) y += 64;
    else x += t.w + 36.0f;
    { const char *rot[2] = { i18n("Fundos"), i18n("Logos") };
      int k;
      for (k = 0; k < 2; k++) {
        TxtLinha l = txt_linha(TXT_DET_META2, rot[k], 255, 255, 255, 255);
        GfxRect r = { x, y, l.w + 56.0f, 52.0f };
        if (telefoneui_ativo()) r.w = fminf(r.w, (NV_TELA_W - TA_X0 * 2 - 14) * .5f);
        desenhaAba(r, rot[k], aba == k, naAba && !naFiltro && aba == k, a);
        if (a > 0.3f) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroFoco, NULL, -1, k);
        x += r.w + 14.0f;
      }
      // CHIP DE IDIOMA: so quando o TMDB trouxe mais de um idioma nesta aba.
      // OK troca (todos -> cada idioma -> todos); um degrau a direita de Logos.
      { char l[TA_FILTROS][8];
        if (idiomasDaAba(aba, l) >= 2) {
          char rf[80];
          TxtLinha lf;
          GfxRect r;
          rotuloFiltro(aba, rf, sizeof rf);
          lf = txt_linha(TXT_DET_META2, rf, 255, 255, 255, 255);
          if (telefoneui_ativo() && NV_TELA_W < 1500) { x = TA_X0 - 22; y += 64; }
          r = (GfxRect){ x + 22.0f, y, lf.w + 56.0f, 52.0f };
          if (telefoneui_ativo()) r.w = fminf(r.w, NV_TELA_W - TA_X0 - r.x);
          desenhaAba(r, rf, filtro[0] != 0, naAba && naFiltro, a);
          if (a > 0.3f) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroFoco, NULL, -2, 0);
        } else if (naFiltro) naFiltro = 0;
      } }
    { const char *dica = buscando ? i18n("Buscando mais artes…")
                                  : i18n("OK escolhe  ·  Voltar cancela");
      TxtLinha l = txt_linha(TXT_DET_META2, dica, 170, 174, 184, 255);
      if (!telefoneui_ativo()) txt_desenhar_alpha(l, NV_TELA_W - 96.0f - l.w, y + (52.0f - l.h) * 0.5f, a * 0.9f); } }

  // GRADE.
#ifdef NV_TOUCH_UI
  if (!toque[aba].livre) toqueY[aba] = topo[aba] * TA_PASSO;
  toquerol_vincular(&toque[aba], (GfxRect){ TA_X0, TA_Y0, NV_TELA_W - 2.0f * TA_X0, TA_LINHAS * TA_PASSO }, gfx_escala(), 0.0f,
                    fmaxf(0.0f, ((n + TA_COLS - 1) / TA_COLS - TA_LINHAS) * TA_PASSO), 1, &toqueY[aba]);
  ponteiro_rolagem(toqueRolar);
  gfx_recorte(TA_X0 - 12.0f, TA_Y0, NV_TELA_W - 2.0f * TA_X0 + 24.0f, TA_LINHAS * TA_PASSO);
  for (i = 0; i < n; i++) {
    float linhaY = TA_Y0 + (i / TA_COLS) * TA_PASSO - toqueY[aba];
    if (linhaY + TA_PASSO <= TA_Y0 || linhaY >= TA_Y0 + TA_LINHAS * TA_PASSO) continue;
#else
  for (i = topo[aba] * TA_COLS; i < n && i < (topo[aba] + TA_LINHAS) * TA_COLS; i++) {
#endif
    const TaCand *c = &cand[aba][v[i]];
    int lin = i / TA_COLS - topo[aba], col = i % TA_COLS;
    int focado = !naAba && i == p;
    int vigente = c->url[0] ? !strcmp(c->url, escolhido[aba]) : !escolhido[aba][0];
    GfxRect r = { TA_X0 + col * (TA_W + TA_GAP), TA_Y0 + lin * TA_PASSO, TA_W, TA_H };
#ifdef NV_TOUCH_UI
    r.y = linhaY;
#endif
    if (focado) {
      float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
      r.w *= 1.06f; r.h *= 1.06f; r.x = cx - r.w * 0.5f; r.y = cy - r.h * 0.5f;
      { float fr, fg, fb; ajustes_acento_tinta(&fr, &fg, &fb);
        botao_luz(r, 1.0f, a);
        gfx_cor((GfxRect){ r.x - 5, r.y - 5, r.w + 10, r.h + 10 }, 17.0f / (r.h + 10.0f), fr, fg, fb, a); }
    }
    if (a > 0.3f)
#ifdef NV_TOUCH_UI
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, TA_Y0, TA_Y0 + TA_LINHAS * TA_PASSO, ponteiroFoco, NULL, aba, v[i]);
#else
      ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroFoco, NULL, aba, v[i]);
#endif
    if (aba == 0) {
      GLuint t = tex_obter_larg(c->mostra, TA_W);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(c->mostra);
        gfx_rect(r, t, GFX_CARD, 0, 0, 0, 12.0f / r.h, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else gfx_esqueleto(r, 12.0f / r.h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    } else {
      // Logo sobre um cinza medio-escuro: o preto chapado some com o logo
      // escuro (GFX_MARCA pinta branco, mas o PNG colorido nao).
      gfx_cor(r, 12.0f / r.h, 0.17f, 0.18f, 0.20f, a);
      desenhaLogo(c->mostra, (GfxRect){ r.x + 28, r.y + 26, r.w - 56, r.h - 52 }, a, 0);
    }
    if (vigente) {
      GfxRect d = { r.x + r.w - 46.0f, r.y + 10.0f, 36.0f, 36.0f };
      // Disco na cor de realce com o check na TINTA que contrasta com ela: no
      // tema padrao o realce e branco, e check branco sumia no disco.
      float fr, fg, fb, tinta = ajustes_acento_tinta(&fr, &fg, &fb);
      gfx_cor(d, NV_RAIO_PILL, fr, fg, fb, a);
      gfx_icone((GfxRect){ d.x + 8, d.y + 8, 20, 20 }, "check", tinta, tinta, tinta, a);
    }
    { TxtLinha l = txt_linha_corta(TXT_MINI, i18n(c->rotulo), focado ? 245 : 170,
                                   focado ? 248 : 174, focado ? 255 : 184, 255, TA_W);
      txt_desenhar_alpha(l, TA_X0 + col * (TA_W + TA_GAP),
#ifdef NV_TOUCH_UI
                         r.y + r.h + 12.0f, a); }
#else
                         TA_Y0 + lin * TA_PASSO + TA_H + 12.0f, a); }
#endif
  }
#ifdef NV_TOUCH_UI
  gfx_sem_recorte();
#endif
  // Mais linhas abaixo: uma seta discreta, para a grade nao parecer acabar.
  if ((topo[aba] + TA_LINHAS) * TA_COLS < n) {
    TxtLinha l = txt_linha(TXT_MINI, i18n("mais abaixo"), 150, 154, 163, 255);
    txt_desenhar_alpha(l, NV_TELA_W - 96.0f - l.w, TA_Y0 + TA_LINHAS * TA_PASSO - 30.0f, a * 0.8f);
  }
  pthread_mutex_unlock(&trava);
}

// --- teste -------------------------------------------------------------------
void trocaarte_teste_candidato(int a, const char *url, const char *rotulo) {
  if (a < 0 || a > 1) return;
  pthread_mutex_lock(&trava);
  geracao++;          // o fio de verdade, se houver, nao se mistura
  buscando = 0;
  por(a, url, a == 1 ? url : NULL, rotulo);
  pthread_mutex_unlock(&trava);
}

void trocaarte_teste_foco(int a, int pos) {
  int v[TA_MAX], n;
  if (a < 0 || a > 1) return;
  pthread_mutex_lock(&trava);
  if (aba != a) previa[0] = 0;
  aba = a; naAba = 0;
  n = visiveis(a, v);
  if (pos >= 0 && pos < n) foco[a] = v[pos];
  focoDesde = SDL_GetTicks() - 1000;   // a previa nao espera o repouso
  pthread_mutex_unlock(&trava);
}

int trocaarte_n(int a) { return a >= 0 && a <= 1 ? nCand[a] : 0; }

void trocaarte_teste_tmdb(const char *json, const char *lingua) {
  publicarTmdb(json, lingua, geracao);
}

const char *trocaarte_teste_iso(int a, int pos) {
  static char iso[8];
  int v[TA_MAX], n;
  iso[0] = 0;
  if (a < 0 || a > 1) return iso;
  pthread_mutex_lock(&trava);
  n = visiveis(a, v);
  if (pos >= 0 && pos < n) snprintf(iso, sizeof iso, "%s", cand[a][v[pos]].iso);
  pthread_mutex_unlock(&trava);
  return iso;
}

int trocaarte_teste_visiveis(int a) {
  int v[TA_MAX], n;
  if (a < 0 || a > 1) return 0;
  pthread_mutex_lock(&trava);
  n = visiveis(a, v);
  pthread_mutex_unlock(&trava);
  return n;
}

const char *trocaarte_teste_filtro(int a) {
  if (a < 0 || a > 1) return "";
  pthread_mutex_lock(&trava);
  filtroProximo(a);
  pthread_mutex_unlock(&trava);
  return filtro;
}

void trocaarte_teste_limpar(void) {
  pthread_mutex_lock(&trava);
  geracao++;
  buscando = 0;
  if (nCand[0] > 1) nCand[0] = 1;
  if (nCand[1] > 1) nCand[1] = 1;
  filtro[0] = 0;
  pthread_mutex_unlock(&trava);
}

void trocaarte_teste_candidato_iso(int a, const char *url, const char *rotulo, const char *iso) {
  if (a < 0 || a > 1) return;
  pthread_mutex_lock(&trava);
  geracao++;
  buscando = 0;
  porIso(a, url, a == 1 ? url : NULL, rotulo, iso);
  pthread_mutex_unlock(&trava);
}
