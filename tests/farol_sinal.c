// Sinais do Farol (lote sense, issue 38): a tabela 2.2 inteira linha a
// linha, as exclusoes (sono, falha de fonte, perfil, "Recomendar nesta TV"),
// o desfazer do desmarcar, as tres ressalvas do lote (buffer no teto, chave
// catalogo<->TMDB sem peso em dobro, ficha-cache sem corrida) — tudo contra o
// farolsinal.c DE PRODUCAO. Por baixo: diario, catalogo, dormiu, perfis,
// dados, salvos, visto e progresso FALSOS; o anel, a ficha, o gosto e o mapa
// sao os de verdade.
#include "farolsinal.h"
#include "farolanel.h"
#include "progresso.h"
#include "visto.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FALHA(msg) do { fprintf(stderr, "farol_sinal: FALHA %s:%d %s\n", \
                              __FILE__, __LINE__, msg); exit(1); } while (0)
#define OK(cond, msg) do { if (!(cond)) FALHA(msg); } while (0)

// ------------------------------------------------------------------- tempo --
static long long T_now;          // epoch s (o relogio injetado)
static uint32_t  T_ms;           // ms para o anel
static long long fakeS(void) { return T_now; }
static uint32_t  fakeMs(void) { return T_ms; }
static int diaNow(void) { return (int)((T_now - 1767225600LL) / 86400); }
static void avanca(long long s) { T_now += s; T_ms += (uint32_t)(s * 1000); }

// ------------------------------------------------------------------ perfil --
static int T_perfil = 1;
int perfis_ativo(void) { return T_perfil; }

// ------------------------------------------------------------------- dados --
static char T_dir[512];
const char *dados_dir(void) { return T_dir; }

// ------------------------------------------------------------------ dormiu --
static int T_sono;
int dormiu_suspeita(const char *id, DiarioEntrada *saida) {
  if (!T_sono) return 0;
  if (saida) {
    memset(saida, 0, sizeof *saida);
    snprintf(saida->id, sizeof saida->id, "%s", id);
    saida->quando = T_now - 60;
    saida->flags = DIARIO_F_DORMIU_SUS;
  }
  return 1;
}

// ------------------------------------------------------------------ diario --
static DiarioEntrada D[512]; static int D_n, D_pf[512];
static DiarioOuvinte D_fn; static void *D_ctx;
int diario_ouvir(DiarioOuvinte fn, void *ctx) { D_fn = fn; D_ctx = ctx; return 1; }
void diario_parar_de_ouvir(DiarioOuvinte fn, void *ctx) {
  if (D_fn == fn && D_ctx == ctx) D_fn = NULL;
}
static void dEv(int ev, const DiarioEntrada *e) {
  if (D_fn) D_fn(T_perfil, ev, e, D_ctx);
}
static void dAdd(const DiarioEntrada *e) {
  OK(D_n < 512, "diario fake cheio");
  D[D_n] = *e; D_pf[D_n] = T_perfil; D_n++;
  dEv(DIARIO_EV_NOVA, &D[D_n - 1]);
}
static void dRecarrega(void) { dEv(DIARIO_EV_RECARREGADO, NULL); }
int diario_percorrer(int perfil, const DiarioFiltro *f, DiarioVisita cb, void *ctx) {
  int ord[512], n = 0, i, j, k = 0;
  for (i = 0; i < D_n; i++) if (D_pf[i] == perfil) ord[n++] = i;
  if (f && f->decrescente)
    for (i = 0; i < n; i++)
      for (j = i + 1; j < n; j++)
        if (D[ord[j]].quando > D[ord[i]].quando)
          { int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (i = 0; i < n; i++) {
    const DiarioEntrada *e = &D[ord[i]];
    if (f) {
      if (f->id && strcmp(f->id, e->id)) continue;
      if (f->tipo && f->tipo != e->tipo) continue;
      if (f->de && e->quando < f->de) continue;
      if (f->ate && e->quando >= f->ate) continue;
      if ((e->flags & f->comFlags) != f->comFlags) continue;
      if (e->flags & f->semFlags) continue;
      if (f->origem && f->origem != e->origem) continue;
    }
    k++;
    if (!cb(e, ctx)) break;
  }
  return k;
}
int diario_por_titulo(int perfil, const char *id, DiarioEntrada *saida, int max) {
  int i, n = 0;
  for (i = 0; i < D_n; i++)
    if (D_pf[i] == perfil && !strcmp(D[i].id, id)) { n++; if (saida && n <= max) saida[n - 1] = D[i]; }
  return n;
}
int diario_apagar(int perfil, const char *id, int t, int e, long long quando) {
  int i, ach = -1;
  for (i = 0; i < D_n; i++)
    if (D_pf[i] == perfil && !strcmp(D[i].id, id) && D[i].temporada == t && D[i].episodio == e &&
        (quando ? D[i].quando == quando : (ach < 0 || D[i].quando > D[ach].quando)))
      ach = i;
  if (ach < 0) return 0;
  {
    DiarioEntrada copia = D[ach];
    memmove(&D[ach], &D[ach + 1], sizeof *D * (size_t)(D_n - ach - 1));
    D_n--;
    dEv(DIARIO_EV_APAGADA, &copia);
  }
  return 1;
}
int diario_episodio(int perfil, const char *id, int t, int e, long long *quando) {
  int i, n = 0;
  for (i = 0; i < D_n; i++)
    if (D_pf[i] == perfil && !strcmp(D[i].id, id) && D[i].temporada == t && D[i].episodio == e) {
      n++;
      if (quando && D[i].quando > *quando) *quando = D[i].quando;
    }
  return n;
}
int diario_ultimo_episodio(int perfil, const char *id, int *t, int *e,
                           long long *quando) {
  int i, ach = -1;
  for (i = 0; i < D_n; i++)
    if (D_pf[i] == perfil && !strcmp(D[i].id, id) && D[i].temporada > 0 &&
        (ach < 0 || D[i].temporada > D[ach].temporada ||
         (D[i].temporada == D[ach].temporada && D[i].episodio > D[ach].episodio)))
      ach = i;
  if (ach < 0) return 0;
  if (t) *t = D[ach].temporada;
  if (e) *e = D[ach].episodio;
  if (quando) *quando = D[ach].quando;
  return 1;
}

// ---------------------------------------------------------------- catalogo --
static CatItem C_it[32]; static int C_n;
static CatEp   C_eps[32][24]; static int C_en[32];
static int cat_add(const char *imdb, const char *tipo, long tmdb,
                   const char *genero, const char *titulo) {
  int i = C_n++;
  memset(&C_it[i], 0, sizeof C_it[i]);
  snprintf(C_it[i].imdb, sizeof C_it[i].imdb, "%s", imdb);
  snprintf(C_it[i].tipo, sizeof C_it[i].tipo, "%s", tipo);
  snprintf(C_it[i].titulo, sizeof C_it[i].titulo, "%s", titulo);
  snprintf(C_it[i].genero, sizeof C_it[i].genero, "%s", genero);
  snprintf(C_it[i].meta, sizeof C_it[i].meta, "2010 · 2h");
  C_it[i].tmdb = tmdb;
  return i;
}
static void cat_ep(int i, int t, int e) {
  C_eps[i][C_en[i]].temporada = t; C_eps[i][C_en[i]].episodio = e; C_en[i]++;
}
const CatItem *cat_item(int i) { return i >= 0 && i < C_n ? &C_it[i] : NULL; }
int cat_indice_por_imdb(const char *imdb) {
  int i;
  for (i = 0; i < C_n; i++) if (!strcmp(C_it[i].imdb, imdb)) return i;
  return -1;
}
int cat_n_episodios(int i) { return i >= 0 && i < C_n ? C_en[i] : 0; }
const CatEp *cat_episodio(int i, int k) {
  return i >= 0 && i < C_n && k >= 0 && k < C_en[i] ? &C_eps[i][k] : NULL;
}

// ------------------------------------------------------- salvos/visto/prog --
static void (*Salvos_fn)(const CatItem *, int);
void salvos_ao_definir(void (*f)(const CatItem *, int)) { Salvos_fn = f; }
static VistoOuvinte V_fn;
void visto_ao_marcar(VistoOuvinte fn) { V_fn = fn; }
static void (*Prog_fn)(const char *);
void prog_ao_retirar(void (*fn)(const char *)) { Prog_fn = fn; }
static ProgRegistro P_regs[32]; static int P_n;
int prog_ler(ProgRegistro *saida, int max) {
  int n = P_n < max ? P_n : max;
  memcpy(saida, P_regs, sizeof *P_regs * (size_t)n);
  return n;
}

// ------------------------------------------------------------------ busca --
// Transporte da fila lenta: um detalhe TMDB minimo de resposta fixa.
static int fakeBusca(const char *id, long tmdb, int serie, char *dst, size_t cap) {
  static const char json[] =
    "{\"id\":901,\"title\":\"x\",\"vote_average\":7.0,\"vote_count\":10,"
    "\"release_date\":\"2010-01-01\",\"runtime\":100,"
    "\"original_language\":\"pt\",\"imdb_id\":\"tt_wrk\","
    "\"genres\":[{\"id\":28,\"name\":\"Acao\"}],"
    "\"keywords\":{\"keywords\":[{\"id\":9,\"name\":\"k\"}]},"
    "\"credits\":{\"cast\":[{\"id\":3,\"name\":\"n\"}],\"crew\":[]}}";
  size_t n = strlen(json);
  (void)id; (void)tmdb; (void)serie;
  if (n + 1 > cap) return 0;
  memcpy(dst, json, n + 1);
  return (int)n;
}

// Fio de estresse: sinais e fichas de outros fios, tudo que toca a trava.
static void *fioTeste(void *u) {
  long k = (long)u;
  int i;
  FarolFicha f;
  memset(&f, 0, sizeof f);
  f.tmdb = 7000 + (int)k; f.tipo = FF_FILME; f.genero[0] = 28;
  f.nGenero = 1; f.minutos = 90; f.ano = 2000; f.fonte = FF_DE_TMDB;
  for (i = 0; i < 200; i++) {
    char id[24];
    snprintf(id, sizeof id, "tt_fio%ld_%d", k, i % 8);
    farolsinal_saida(id, DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
    farolsinal_ficha_ok(((int64_t)FF_FILME << 32) | (uint32_t)(7000 + (i & 7)), &f);
    farolsinal_pendencias();
  }
  return NULL;
}

// ------------------------------------------------------------------ medida --
static int peso(int dim, int32_t id) {
  FarolGosto g;
  OK(farolsinal_modelo(&g), "modelo");
  return farolgosto_peso(&g, dim, id, (uint16_t)diaNow());
}
// O modelo satura cada slot por n (afinToca: peso += delta*8/(n+8)) — entao a
// soma esperada de uma sequencia de sinais nao e linear. esp() repete a conta
// no mesmo dia-modelo (sem decaimento) para a assertiva ser exata.
static int esp(int peso, int n, int delta) {
  int num = delta * 8, den = n + 8;
  peso += (num + (num >= 0 ? den / 2 : -den / 2)) / den;
  if (peso > FAROL_GOSTO_PESOMAX) peso = FAROL_GOSTO_PESOMAX;
  if (peso < -FAROL_GOSTO_PESOMAX) peso = -FAROL_GOSTO_PESOMAX;
  return peso;
}
static uint32_t titulosModelo(void) {
  FarolGosto g;
  OK(farolsinal_modelo(&g), "modelo");
  return g.titulos;
}
static int32_t genAcao;   // id do genero "Acao" via mapa_hash_nome (CatItem)
static void entrada(DiarioEntrada *e, const char *id, char tipo, int t, int ep,
                    unsigned flags, int nota) {
  memset(e, 0, sizeof *e);
  snprintf(e->id, sizeof e->id, "%s", id);
  e->tipo = tipo; e->origem = DIARIO_ORIG_PLAYER;
  e->temporada = (short)t; e->episodio = (short)ep;
  e->quando = T_now; e->hora = 34; e->flags = (unsigned short)flags;
  e->nota10 = (unsigned char)nota; e->seg = 3600; e->durSeg = 3600;
}
static void zeraTudo(void) {
  char cam[640];
  int p;
  farolsinal_teste_zera();
  for (p = 1; p <= 3; p++) {
    snprintf(cam, sizeof cam, "%s/farol-gosto-p%d.txt", T_dir, p); unlink(cam);
    snprintf(cam, sizeof cam, "%s/farol-sinal-p%d.txt", T_dir, p); unlink(cam);
  }
  snprintf(cam, sizeof cam, "%s/farol-fichas.bin", T_dir); unlink(cam);
  D_n = 0; T_sono = 0; P_n = 0; T_perfil = 1;
  farolanel_zerar();
  farolsinal_definir_recomendar(1);
}

int main(void) {
  DiarioEntrada e;
  int i;
  strcpy(T_dir, "/tmp/nuvio-farol-sinal-XXXXXX");
  OK(mkdtemp(T_dir) != NULL, "mkdtemp");
  T_now = 1767225600LL + 40LL * 86400LL;   // dia 40 do modelo
  T_ms = 10u * 1000u * 1000u;
  genAcao = (int32_t)mapa_hash_nome("Acao");
  farolsinal_relogio(fakeS, fakeMs);
  farolanel_relogio(fakeMs);
  farolsinal_pasta(T_dir);
  farolsinal_iniciar();

  // ---------- tabela 2.2, linha a linha ----------
  // terminou >= 90%: +1,0
  zeraTudo();
  cat_add("tt_aaa", "movie", 0, "Acao", "Filme Secreto X");
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TERMINOU, "terminou +1.0");
  OK(titulosModelo() == 1, "titulos==1");

  // terminou e gostou: +2,0 (coracao ou nota >= 7)
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_CORACAO, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_GOSTOU, "coracao +2.0");
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 8);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_GOSTOU, "nota 8 +2.0");

  // nota <= 3: -2,0
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 2);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_REPROVOU, "nota 2 -2.0");

  // nota 4-6 ("mais ou menos"): 0
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 5);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "nota 5 pesa 0");

  // reviu: +1,5 (a segunda vez soma em cima da primeira)
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  avanca(3600);
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_REVER, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_REVIU),
     "reviu +1.5 saturado por n");
  OK(titulosModelo() == 1, "reviu: ainda 1 titulo");

  // visto junto: delta x 0,5
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_JUNTOS, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == 128, "juntos x0.5");

  // serie: +0,1 por episodio
  zeraTudo();
  cat_add("tt_ser", "series", 0, "Acao", "Serie S");
  cat_ep(1, 1, 1); cat_ep(1, 1, 2); cat_ep(1, 2, 1);
  entrada(&e, "tt_ser", DIARIO_SERIE, 1, 1, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == 26, "episodio +0.1");

  // temporada completa: +delta cheio uma vez por temporada
  entrada(&e, "tt_ser", DIARIO_SERIE, 1, 2, 0, 0);
  avanca(3600); e.quando = T_now;
  dAdd(&e);
  farolsinal_passo();
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(esp(0, 0, 26), 1, 26), 2, FG_D_TERMINOU),
     "temporada completa +1.0 unica");
  farolsinal_passo();
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(esp(0, 0, 26), 1, 26), 2, FG_D_TERMINOU),
     "temporada nao credita de novo");

  // salvou: +0,6; tirou do salvos devolve
  zeraTudo();
  OK(Salvos_fn != NULL, "ouvinte de salvos registrado");
  Salvos_fn(&C_it[0], 1);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_SALVOU, "salvou +0.6");
  Salvos_fn(&C_it[0], 0);
  // a devolucao e o -delta pelo mesmo modelo: a saturacao por n deixa um
  // resíduo de ~11%% — e o preco honesto de um modelo com memoria de toque
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(0, 0, FG_D_SALVOU), 1, -FG_D_SALVOU), "tirou do salvos devolve");

  // tirou do Continuar: -0,6
  zeraTudo();
  OK(Prog_fn != NULL, "ouvinte de progresso registrado");
  Prog_fn("tt_aaa");
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TIROU, "tirou do continuar -0.6");

  // "mais assim": +1,5, uma vez
  zeraTudo();
  farolsinal_mais_assim(&C_it[0]);
  farolsinal_mais_assim(&C_it[0]);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_MAIS_ASSIM, "mais assim +1.5 unico");

  // interesse (detalhe > 20 s / trailer inteiro): +0,2, uma vez por dia
  zeraTudo();
  farolsinal_interesse(&C_it[0]);
  farolsinal_interesse(&C_it[0]);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_INTERESSE, "interesse +0.2 unico/dia");

  // "nao e para mim": -2,0 + anel de recusados; desfazer devolve o delta
  zeraTudo();
  {
    FarolGosto g;
    farolsinal_recusar(&C_it[0]);
    farolsinal_recusar(&C_it[0]);
    OK(peso(FG_DIM_GENERO, genAcao) == FG_D_REPROVOU, "recusa -2.0 unica");
    OK(farolsinal_modelo(&g), "modelo");
    OK(farolgosto_recusado(&g, mapa_hash_titulo("tt_aaa")), "anel recusado");
    farolsinal_desfazer_recusa(&C_it[0]);
    OK(peso(FG_DIM_GENERO, genAcao) ==
       esp(esp(0, 0, FG_D_REPROVOU), 1, -FG_D_REPROVOU),
       "desfazer recusa devolve");
  }

  // mostrado: recusa passiva anotada, sem delta
  zeraTudo();
  {
    FarolGosto g;
    farolsinal_mostrado(&C_it[0]);
    OK(farolsinal_modelo(&g), "modelo");
    OK(farolgosto_mostrado_dias(&g, mapa_hash_titulo("tt_aaa")) == 1,
       "mostrado anotado");
    OK(peso(FG_DIM_GENERO, genAcao) == 0, "mostrado sem delta");
  }

  // origem do Play (medida): anel com hash + origem + dia
  zeraTudo();
  {
    uint32_t h[4]; uint8_t o[4], d[4];
    farolsinal_origem(&C_it[0], FS_ORIGEM_FAROL);
    OK(farolsinal_origens(h, o, d, 4) == 1, "origem anotada");
    OK(h[0] == mapa_hash_titulo("tt_aaa") && o[0] == FS_ORIGEM_FAROL,
       "origem certa");
  }

  // ---------- abandono / sono / falha (secao 1.3) ----------
  zeraTudo();
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_ABANDONOU, "abandono -0.8");

  // parou antes de 1 min ou depois de 20%: nem sinal
  zeraTudo();
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 30.0, 3600.0, 0);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "parou < 1 min sem sinal");
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 1500.0, 3600.0, 0);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "parou > 20% sem sinal");

  // aceite do brief: saida aos 6 min com FE_ERRO_FONTE nos ultimos 5 min NAO pesa
  zeraTudo();
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 1, 500, "x");
  avanca(60);
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "abandono com falha nao pesa");
  farolanel_zerar();
  avanca(6 * 60 * 1000 / 1000 + 100);       // fora da janela de 5 min
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_ABANDONOU,
     "falha velha nao protege");

  // sono: a saida com suspeita recente nao pesa, nem a entrada de sono
  zeraTudo();
  T_sono = 1;
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "saida com sono nao pesa");
  T_sono = 0;
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_PARCIAL | DIARIO_F_DORMIU, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "entrada de sono nao pesa");

  // serie largada: progresso parado ha 21+ dias com episodio adiante no
  // catalogo => -1,0 uma vez por titulo
  zeraTudo();
  {
    ProgRegistro *r = &P_regs[P_n++];
    memset(r, 0, sizeof *r);
    snprintf(r->contentId, sizeof r->contentId, "tt_ser");
    snprintf(r->tipo, sizeof r->tipo, "series");
    r->lastWatchedMs = (T_now - 30LL * 86400) * 1000;
    r->perfil = 1;
  }
  entrada(&e, "tt_ser", DIARIO_SERIE, 1, 1, 0, 0);
  e.quando = T_now - 3600; dAdd(&e);
  e.episodio = 2; e.quando += 3600; dAdd(&e);
  farolsinal_passo();   // fecha a temporada 1 E marca a largada
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(esp(esp(0, 0, 26), 1, 26), 2, FG_D_TERMINOU), 3, FG_D_LARGOU),
     "largada -1.0");
  {
    int antes = peso(FG_DIM_GENERO, genAcao);
    farolsinal_passo();
    OK(peso(FG_DIM_GENERO, genAcao) == antes, "largada nao repete");
  }

  // ---------- desmarcar visto desfaz ----------
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TERMINOU, "pre-desmarcar");
  OK(V_fn != NULL, "ouvinte de visto registrado");
  V_fn("tt_aaa", "movie", NULL, 0, 0);          // titulo inteiro desmarcado
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(0, 0, FG_D_TERMINOU), 1, -FG_D_TERMINOU),
     "desmarcar desfaz o credito");
  OK(D_n == 0, "entrada apagada do diario");

  // marcar assistido na mao: +1,0; o eco do diario nao dobra
  zeraTudo();
  V_fn("tt_aaa", "movie", NULL, 0, 1);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TERMINOU, "gesto +1.0");
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  e.origem = DIARIO_ORIG_CONTA;
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TERMINOU, "eco do gesto nao dobra");

  // o eco com informacao a mais (coracao) completa so a diferenca
  zeraTudo();
  V_fn("tt_aaa", "movie", NULL, 0, 1);
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_CORACAO, 0);
  e.origem = DIARIO_ORIG_CONTA;
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_GOSTOU - FG_D_TERMINOU),
     "eco enriquecido completa");

  // apagar a entrada direto no diario tambem desfaz
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  diario_apagar(T_perfil, "tt_aaa", 0, 0, e.quando);
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(0, 0, FG_D_TERMINOU), 1, -FG_D_TERMINOU), "APAGADA desfaz");

  // ALTERADA (nota chegou depois): so a diferenca anda
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  e.nota10 = 8;
  dEv(DIARIO_EV_ALTERADA, &e);
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_GOSTOU - FG_D_TERMINOU),
     "ALTERADA sobe a diferenca");
  e.nota10 = 0;
  dEv(DIARIO_EV_ALTERADA, &e);
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_GOSTOU - FG_D_TERMINOU),
         2, FG_D_TERMINOU - FG_D_GOSTOU),
     "ALTERADA desce a diferenca");

  // ---------- pendencias / chaves / fila lenta (ressalvas 2 e 3) ----------
  // titulo FORA do catalogo: a entrada espera a ficha; quando o detalhe do
  // TMDB chega (chave real, positiva) o delta esperado aplica — e a reserva
  // vira alias, entao o MESMO titulo nao pesa duas vezes.
  zeraTudo();
  entrada(&e, "tt_off", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(farolsinal_pendencias() == 1, "ficha ausente -> pendencia");
  OK(peso(FG_DIM_GENERO, 28) == 0, "sem ficha nao pesa ainda");
  {
    FarolFicha f;
    int64_t chavePed = -(((int64_t)FF_FILME << 32) |
        (int64_t)(0x40000000u | (mapa_hash_titulo("tt_off") & 0x3fffffffu)));
    memset(&f, 0, sizeof f);
    f.tmdb = 55; f.tipo = FF_FILME; f.genero[0] = 28; f.nGenero = 1;
    f.minutos = 100; f.ano = 2010; f.fonte = FF_DE_TMDB;
    farolsinal_ficha_ok(chavePed, &f);
    OK(farolsinal_pendencias() == 0, "pendencia drenada");
    OK(peso(FG_DIM_GENERO, 28) == FG_D_TERMINOU, "delta esperado aplicado");
    OK(titulosModelo() == 1, "um titulo, uma contagem");
    // segunda vez, ainda com tmdb=0: a chave pedida (reserva) resolve pelo
    // alias — mesma ficha, mesmo titulo, sem segunda contagem.
    avanca(3600);
    e.quando = T_now; e.flags = DIARIO_F_REVER; e.tmdb = 0;
    dAdd(&e);
    OK(peso(FG_DIM_GENERO, 28) ==
       esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_REVIU),
       "reserva->tmdb: mesmo titulo sem dobro");
    OK(titulosModelo() == 1, "alias: ainda 1 titulo");
    // e a via direta (a entrada ja vem com tmdb) cai na mesma chave real;
    // passa da janela de fusao de 6 h para contar como outra vez
    avanca(21601);
    e.tmdb = 55; e.flags = 0; e.quando = T_now;
    dAdd(&e);
    OK(peso(FG_DIM_GENERO, 28) ==
       esp(esp(esp(0, 0, FG_D_TERMINOU), 1, FG_D_REVIU), 2, FG_D_TERMINOU),
       "tmdb direto soma na mesma obra");
    OK(titulosModelo() == 1, "tmdb direto: ainda 1 titulo");
  }

  // a busca que falha duas vezes solta a pendencia (nunca fica para sempre)
  zeraTudo();
  entrada(&e, "tt_off2", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(farolsinal_pendencias() == 1, "pendencia criada");
  {
    int64_t chavePed = -(((int64_t)FF_FILME << 32) |
        (int64_t)(0x40000000u | (mapa_hash_titulo("tt_off2") & 0x3fffffffu)));
    farolsinal_ficha_falhou(chavePed);
    OK(farolsinal_pendencias() == 1, "1a falha: espera a retentativa");
    farolsinal_ficha_falhou(chavePed);
    OK(farolsinal_pendencias() == 0, "2a falha: solta a pendencia");
  }

  // a pendencia sobrevive ao APAGADA: se a vez sumiu antes da ficha, nada aplica
  zeraTudo();
  entrada(&e, "tt_off3", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(farolsinal_pendencias() == 1, "pendencia para apagar");
  diario_apagar(T_perfil, "tt_off3", 0, 0, e.quando);
  OK(farolsinal_pendencias() == 0, "pendencia cancelada com a vez");

  // a fila lenta de verdade (fio + busca injetada): a ficha vem do TMDB
  zeraTudo();
  farolsinal_busca(fakeBusca);
  entrada(&e, "tt_wrk", DIARIO_FILME, 0, 0, 0, 0);
  e.tmdb = 901;
  dAdd(&e);
  for (i = 0; i < 400 && farolsinal_pendencias() > 0; i++) {
    farolsinal_passo();
    usleep(2000);
  }
  OK(farolsinal_pendencias() == 0, "fila lenta drenou");
  OK(peso(FG_DIM_GENERO, 28) == FG_D_TERMINOU, "ficha do TMDB aplicou");
  farolsinal_busca(NULL);

  // ---------- perfil / ajuste (fronteiras da secao 1.3) ----------
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TERMINOU, "perfil 1 creditou");
  T_perfil = 2;
  farolsinal_passo();
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "perfil 2 comeca zerado");
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_CORACAO, 0);
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_GOSTOU, "perfil 2 creditou o seu");
  T_perfil = 1;
  farolsinal_passo();
  OK(peso(FG_DIM_GENERO, genAcao) == FG_D_TERMINOU,
     "perfil 1 intacto (sem vazar delta)");

  // "Recomendar nesta TV" desligado: nao coleta; de volta, continua
  zeraTudo();
  farolsinal_definir_recomendar(0);
  OK(!farolsinal_recomendar(), "ajuste desligado");
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);
  V_fn("tt_aaa", "movie", NULL, 0, 1);
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
  Salvos_fn(&C_it[0], 1);
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "recomendar off: nada coleta");
  OK(D_n >= 1, "diario continua recebendo");
  farolsinal_definir_recomendar(1);
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  e.quando = T_now + 5;
  dAdd(&e);
  OK(peso(FG_DIM_GENERO, genAcao) > 0, "recomendar on: volta a coletar");

  // ---------- RECARREGADO / varredura ----------
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, 0, 0);
  D[D_n] = e; D_pf[D_n] = T_perfil; D_n++;  // carga antiga, sem evento
  e.nota10 = 8; e.quando += 10; D[D_n] = e; D_pf[D_n] = T_perfil; D_n++;
  dRecarrega();
  farolsinal_passo();
  // a varredura desce do mais novo para o mais velho: a versao rica (nota 8)
  // vence e a mais velha so traria diff negativo, que nao aplica
  OK(peso(FG_DIM_GENERO, genAcao) == esp(0, 0, FG_D_GOSTOU),
     "varredura credita o historico");

  // ---------- persistencia ----------
  zeraTudo();
  entrada(&e, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_CORACAO, 0);
  dAdd(&e);
  farolsinal_saida("tt_aaa", DIARIO_FILME, 0, 0, 0, 360.0, 3600.0, 0);
  farolsinal_sair();            // grava modelo + estado + fichas
  {
    char cam[640]; struct stat st;
    snprintf(cam, sizeof cam, "%s/farol-gosto-p1.txt", T_dir);
    OK(stat(cam, &st) == 0 && st.st_size > 0, "modelo gravado");
    snprintf(cam, sizeof cam, "%s/farol-sinal-p1.txt", T_dir);
    OK(stat(cam, &st) == 0 && st.st_size > 0, "estado gravado");
    snprintf(cam, sizeof cam, "%s/farol-fichas.bin", T_dir);
    OK(stat(cam, &st) == 0 && st.st_size > 0, "fichas gravadas");
    // privacidade: nenhum TITULO vai para os arquivos — so ids e numeros
    {
      FILE *fp = fopen(cam, "rb"); char *buf = malloc((size_t)st.st_size + 1);
      OK(fp && buf, "le fichas");
      if (fp && buf) {
        fread(buf, 1, (size_t)st.st_size, fp); buf[st.st_size] = 0; fclose(fp);
        OK(!strstr(buf, "Filme Secreto") && !strstr(buf, "Serie S"),
           "sem titulo no bin de fichas");
        free(buf);
      }
    }
    snprintf(cam, sizeof cam, "%s/farol-sinal-p1.txt", T_dir);
    {
      FILE *fp = fopen(cam, "rb"); char l[4096]; int okTit = 1;
      while (fp && fgets(l, sizeof l, fp))
        if (strstr(l, "Filme Secreto") || strstr(l, "Serie S")) okTit = 0;
      if (fp) fclose(fp);
      OK(okTit, "sem titulo no estado");
    }
  }
  // recarga: o modelo volta igual
  farolsinal_teste_zera();
  farolsinal_iniciar();
  OK(peso(FG_DIM_GENERO, genAcao) ==
     esp(esp(0, 0, FG_D_GOSTOU), 1, FG_D_ABANDONOU),
     "modelo recarregado identico");
  // e a entrada re-entregue depois do reinicio nao dobra (registro em disco)
  {
    DiarioEntrada e2;
    entrada(&e2, "tt_aaa", DIARIO_FILME, 0, 0, DIARIO_F_CORACAO, 0);
    e2.quando = e.quando;
    dEv(DIARIO_EV_NOVA, &e2);   // mesma vez, redeliver
    OK(peso(FG_DIM_GENERO, genAcao) ==
       esp(esp(0, 0, FG_D_GOSTOU), 1, FG_D_ABANDONOU),
       "redelivery pos-reinicio nao dobra");
  }

  // ---------- esquecer ----------
  farolsinal_esquecer();
  {
    char cam[640]; struct stat st;
    snprintf(cam, sizeof cam, "%s/farol-gosto-p1.txt", T_dir);
    OK(stat(cam, &st) != 0, "esquecer apaga o modelo");
    snprintf(cam, sizeof cam, "%s/farol-fichas.bin", T_dir);
    OK(stat(cam, &st) != 0, "esquecer apaga as fichas");
  }
  OK(peso(FG_DIM_GENERO, genAcao) == 0, "modelo zerado no esquecer");

  // ---------- ressalva 1: o modelo no TETO escreve e rele ----------
  {
    static char buf[FAROL_GOSTO_TXT_MAX + 8], buf2[FAROL_GOSTO_TXT_MAX + 8];
    FarolGosto g, g2;
    size_t n, n2;
    // piores valores LEGAIS: o leitor saneia peso em +-8192 e o resto em
    // < meia-vida, entao o teto real e este — e ele ainda tem que caber.
    farolgosto_iniciar(&g, 1);
    g.diaBase = 65535; g.titulos = 0xffffffffu;
    for (i = 0; i < FG_GENERO_N; i++) g.genero[i] =
        (FarolAfin){ INT32_MIN + i, -8192, 65535, 65535 };
    for (i = 0; i < FG_PESSOA_N; i++) g.pessoa[i] =
        (FarolAfin){ INT32_MIN + i, -8192, 65535, 65535 };
    for (i = 0; i < FG_TEMA_N; i++) g.tema[i] =
        (FarolAfin){ INT32_MIN + i, -8192, 65535, 65535 };
    for (i = 0; i < FG_DECADA_N; i++) g.decada[i] =
        (FarolAfin){ INT32_MIN + i, -8192, 65535, 65535 };
    for (i = 0; i < FG_IDIOMA_N; i++) g.idioma[i] =
        (FarolAfin){ INT32_MIN + i, -8192, 65535, 65535 };
    for (i = 0; i < FG_DURACAO_N; i++) g.duracao[i] =
        (FarolAfin){ INT32_MIN + i, -8192, 65535, 65535 };
    {
      int a, b;
      for (a = 0; a < FG_HORA_F; a++)
        for (b = 0; b < FG_HORA_D; b++) g.hora[a][b] = -32768;
    }
    for (i = 0; i < FG_RECUS_N; i++) g.recusado[i] = 0xffffffffu;
    g.recProx = FG_RECUS_N;
    for (i = 0; i < FG_MOSTR_N; i++)
      { g.mostrado[i].h = 0xffffffffu; g.mostrado[i].vezes = 255;
        g.mostrado[i].dia = 255; }
    g.mostProx = FG_MOSTR_N;
    memset(g.restoGenero, FG_MEIA_AFIN - 1, sizeof g.restoGenero);
    memset(g.restoPessoa, FG_MEIA_AFIN - 1, sizeof g.restoPessoa);
    memset(g.restoTema, FG_MEIA_AFIN - 1, sizeof g.restoTema);
    memset(g.restoDecada, FG_MEIA_AFIN - 1, sizeof g.restoDecada);
    memset(g.restoIdioma, FG_MEIA_AFIN - 1, sizeof g.restoIdioma);
    memset(g.restoDuracao, FG_MEIA_HORA - 1, sizeof g.restoDuracao);
    g.horaResto = FG_MEIA_HORA - 1;
    n = farolgosto_escrever(&g, buf, sizeof buf);
    OK(n > 0, "modelo no teto ESCREVE (buffer cobre o pior caso)");
    OK(n <= FAROL_GOSTO_TXT_MAX, "escrita dentro do teto");
    farolgosto_iniciar(&g2, 1);
    OK(farolgosto_ler(&g2, buf, n) == 0, "modelo no teto rele");
    n2 = farolgosto_escrever(&g2, buf2, sizeof buf2);
    if (n2 != n || memcmp(buf, buf2, n)) {
      size_t i;
      fprintf(stderr, "teto n=%zu n2=%zu\n", n, n2);
      for (i = 0; i < n && i < n2; i++)
        if (buf[i] != buf2[i]) {
          fprintf(stderr, "dif @%zu\nA: %.80s\nB: %.80s\n", i,
                  buf + i, buf2 + i);
          break;
        }
    }
    OK(n2 == n && !memcmp(buf, buf2, n), "teto: ida e volta identicas");
  }

  // ---------- fios (TSan exercita isto): ficha_ok + saida + passo juntos --
  {
    // drena o que sobrou dos testes anteriores e mede corrida de verdade
    pthread_t ts[4];
    int k;
    farolsinal_teste_zera();
    zeraTudo();
    // sem busca: sinais empilham pendencias, ficha_ok de fora resolve
    for (k = 0; k < 4; k++)
      OK(!pthread_create(&ts[k], NULL,
          (void *(*)(void *))fioTeste, (void *)(long)k), "thread");
    for (k = 0; k < 4; k++) pthread_join(ts[k], NULL);
    // nada travou, nada corrompeu: o modelo le e o estado esta inteiro
    {
      FarolGosto g;
      OK(farolsinal_modelo(&g), "modelo pos-fios");
    }
  }

  // nada em /tmp: apaga os arquivos do perfil fake e a pasta temporaria
  {
    char cam[640];
    const char *nomes[] = { "farol-gosto-p1.txt", "farol-gosto-p2.txt",
                            "farol-gosto-p3.txt", "farol-sinal-p1.txt",
                            "farol-sinal-p2.txt", "farol-sinal-p3.txt",
                            "farol-fichas.bin", NULL };
    for (i = 0; nomes[i]; i++) {
      snprintf(cam, sizeof cam, "%s/%s", T_dir, nomes[i]);
      unlink(cam);
      snprintf(cam, sizeof cam, "%s/%s.tmp", T_dir, nomes[i]);
      unlink(cam);
    }
    rmdir(T_dir);
  }
  printf("farol_sinal: tudo ok\n");
  return 0;
}
