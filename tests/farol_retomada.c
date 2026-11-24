// Retomada sem spoiler (lote sense, issue 57).
//
// Liga a P6 real (src/faroldetalhe.c + farolcena/farolinfo) ao compositor real
// (src/farolcompor.c + rede.c) contra um servidor local (tests/farol_retomada.sh)
// que grava cada corpo HTTP recebido. Os dubles do app sao os de
// tests/farol_detalhe.c (incluido com FD_SEM_MAIN).
//
// Prova:
//   1. a sinopse que sai e SO a do episodio que o Diario deu como visto; o
//      canario do proximo episodio (E+1 e T+1) nunca aparece no corpo;
//   2. sem Diario, infantil, sem consentimento, player aberto, offline, sem
//      sinopse do visto ou lista ambigua -> zero POST e so a linha de tempo;
//   3. 500 / 429 / regras / "\n" / ALMA / (t, e) trocado / prazo 6 s -> queda
//      silenciosa na linha deterministica;
//   4. resposta valida de 1 linha -> entra na P6 logo abaixo da retomada.
//
//   uso: farol_retomada <base-url> <arquivo-cenario> <arquivo-captura>
#define FD_SEM_MAIN 1
#include "farol_detalhe.c"
#include "farolcompor.h"
#include "farolinfo.h"
#include <unistd.h>

static _Atomic int g_player = 0;
static _Atomic int g_offline = 0;
int player_aberto(void)      { return atomic_load(&g_player); }
int rede_saude_offline(void) { return atomic_load(&g_offline); }

static const char *g_cenArq, *g_capArq;
static int falhas;

#define CANARIO_E5 "CanarioProximoEpisodio57"
#define CANARIO_T3 "CanarioProximaTemporada57"
#define SINOPSE_VISTA "SinopseVistaT2E4: Walt e Jesse ficam presos no deserto."
#define RESUMO_OK "Walt e Jesse ficaram presos no deserto."

#define CHECA(c, ...) do { if (!(c)) { falhas++; \
  fprintf(stderr, "FALHA %s:%d: ", __FILE__, __LINE__); \
  fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static CatEp g_lista[6];

static void montarEpisodiosEp(int t, int e, int comSinopseVista, int duplicado) {
  memset(g_lista, 0, sizeof g_lista);
  g_lista[0].temporada = t; g_lista[0].episodio = e - 1;
  snprintf(g_lista[0].sinopse, sizeof g_lista[0].sinopse, "SinopseAnteriorT2E3 sem interesse.");
  g_lista[1].temporada = t; g_lista[1].episodio = e;
  if (comSinopseVista)
    snprintf(g_lista[1].sinopse, sizeof g_lista[1].sinopse,
             "SinopseVistaT%dE%d: Walt e Jesse ficam presos no deserto.", t, e);
  g_lista[2].temporada = t; g_lista[2].episodio = e + 1;
  snprintf(g_lista[2].sinopse, sizeof g_lista[2].sinopse, "%s: Walt foge.", CANARIO_E5);
  g_lista[3].temporada = t + 1; g_lista[3].episodio = e;
  snprintf(g_lista[3].sinopse, sizeof g_lista[3].sinopse, "%s: outra fase.", CANARIO_T3);
  g_lista[4].temporada = t + 1; g_lista[4].episodio = 1;
  snprintf(g_lista[4].sinopse, sizeof g_lista[4].sinopse, "%s: recomeco.", CANARIO_T3);
  g_nEps = 5;
  if (duplicado) {
    g_lista[5] = g_lista[1];
    snprintf(g_lista[5].sinopse, sizeof g_lista[5].sinopse, "%s duplicada", CANARIO_E5);
    g_nEps = 6;
  }
  g_eps = g_lista;
}

static void montarEpisodios(int comSinopseVista, int duplicado) {
  montarEpisodiosEp(2, 4, comSinopseVista, duplicado);
}

static void montarSerie(void) {
  FarolFicha f;
  montarCatalogoBase();
  pthread_mutex_lock(&g_catMu);
  memset(&g_itens[3], 0, sizeof g_itens[3]);
  snprintf(g_itens[3].imdb, sizeof g_itens[3].imdb, "tt0903747");
  snprintf(g_itens[3].titulo, sizeof g_itens[3].titulo, "Breaking Bad");
  snprintf(g_itens[3].tipo, sizeof g_itens[3].tipo, "series");
  g_itens[3].tmdb = 1396;
  g_nItens = 4;
  pthread_mutex_unlock(&g_catMu);
  preencherFicha(&f, 1396, FF_SERIE, 2008, 47, 90, 0, 0, 18);
  faroldetalhe_teste_ficha("tt0903747", &f);
}

static void escreverCenario(const char *c) {
  FILE *fp = fopen(g_cenArq, "w");
  if (!fp) { perror("cenario"); exit(2); }
  fputs(c, fp);
  fclose(fp);
}

// Le a captura inteira (cada corpo HTTP numa linha). n = quantos POSTs.
static char *lerCaptura(int *n) {
  FILE *fp = fopen(g_capArq, "rb");
  char *buf;
  long tam;
  int k = 0;
  *n = 0;
  if (!fp) { buf = calloc(1, 1); return buf; }
  fseek(fp, 0, SEEK_END);
  tam = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  buf = calloc(1, (size_t)tam + 1);
  if (tam > 0 && fread(buf, 1, (size_t)tam, fp) != (size_t)tam) { tam = 0; buf[0] = 0; }
  fclose(fp);
  for (long i = 0; i < tam; i++) if (buf[i] == '\n') k++;
  *n = k;
  return buf;
}

static int nCapturas(void) {
  int n;
  free(lerCaptura(&n));
  return n;
}

// Linhas da P6 de agora: todas as linhas literais e a posicao da linha de tempo (T2E4).
typedef struct { int n, temTempo, posTempo; char linhas[4][192]; } P6;

static P6 lerP6Ep(int t, int e) {
  P6 p;
  char tag[32];
  int i;
  snprintf(tag, sizeof tag, "T%dE%d", t, e);
  memset(&p, 0, sizeof p);
  p.posTempo = -1;
  for (i = 0; i < 4; i++) {
    if (!faroldetalhe_linha_info(i, p.linhas[i], sizeof p.linhas[i])) break;
    if (strstr(p.linhas[i], tag)) { p.temTempo = 1; p.posTempo = i; }
    p.n++;
  }
  return p;
}

static P6 lerP6(void) {
  return lerP6Ep(2, 4);
}

static int p6Igual(const P6 *a, const P6 *b) {
  int i;
  if (a->n != b->n || a->temTempo != b->temTempo || a->posTempo != b->posTempo) return 0;
  for (i = 0; i < a->n; i++) {
    if (strcmp(a->linhas[i], b->linhas[i]) != 0) return 0;
  }
  return 1;
}

static Uint32 g_t = 1000u;

// Um quadro como o detail.c faz: atualizar, retomada, e o passo do app.
static void quadro(void) {
  g_t += 16u;
  atomic_store(&g_ms, g_t);
  faroldetalhe_atualizar(3, 0, 0, 0.016f, g_t);
  faroldetalhe_retomada(3);
  (void)farolcompor_passo();
}

// Roda ate entrar linha nova na P6 (ou o pedido terminar e ainda mais ~1,5 s de quadros).
static P6 rodarEp(int t, int e, int maxMs, const P6 *base) {
  long long fim = (long long)maxMs;
  long long dec = 0;
  int calmos = 0;
  P6 p = lerP6Ep(t, e);
  while (dec < fim) {
    quadro();
    p = lerP6Ep(t, e);
    if (p.n > base->n) { quadro(); return lerP6Ep(t, e); }
    if (!farolcompor_em_voo()) { if (++calmos > 90) break; }
    else calmos = 0;
    usleep(16000);
    dec += 16;
  }
  return lerP6Ep(t, e);
}

static P6 rodar(int maxMs, const P6 *base) {
  return rodarEp(2, 4, maxMs, base);
}

// Cada caso comeca do zero: perfil "trocado" (zera o dedupe da sessao), P6
// reaberta, diario com T{t}E{e} visto ha 7 semanas e consentimento dado.
// Devolve a P6 deterministica inicial (sem resumo) para comparacao literal.
static P6 abrirCasoEp(int t, int e) {
  atomic_store(&g_player, 0);
  atomic_store(&g_offline, 0);
  atomic_store(&g_infantil, 0);
  atomic_store(&g_seguro, 0);
  atomic_store(&g_recomendar, 1);
  farolcompor_trocar_perfil();
  farolcompor_definir_consentimento(1);
  atomic_store(&g_retT, t);
  atomic_store(&g_retE, e);
  atomic_store(&g_retQ, atomic_load(&g_epoch) - 49LL * 86400LL);
  atomic_fetch_add(&g_diaGer, 1);
  montarEpisodiosEp(t, e, 1, 0);
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarSerie();
  g_t += 5000u;
  atomic_store(&g_ms, g_t);
  faroldetalhe_abrir(3, g_t);
  g_t += 16u;
  atomic_store(&g_ms, g_t);
  faroldetalhe_atualizar(3, 0, 0, 0.016f, g_t);
  return lerP6Ep(t, e);
}

static P6 abrirCaso(void) {
  return abrirCasoEp(2, 4);
}

static void auditarCorpos(const char *caso) {
  int n;
  char *c = lerCaptura(&n);
  CHECA(!strstr(c, CANARIO_E5), "%s: canario do proximo episodio no corpo HTTP", caso);
  CHECA(!strstr(c, CANARIO_T3), "%s: canario da proxima temporada no corpo HTTP", caso);
  CHECA(!strstr(c, "SinopseAnteriorT2E3"), "%s: sinopse de outro episodio no corpo", caso);
  free(c);
}

// --- 0. unidades puras -------------------------------------------------------
static void teste_unidades(void) {
  FarolInfoEp eps[4] = {
    { 2, 3, "anterior" }, { 2, 4, "vista" }, { 2, 5, "proxima" }, { 3, 1, "outra" },
  };
  FarolInfoEp dup[3] = { { 2, 4, "a" }, { 2, 4, "b" }, { 2, 5, "c" } };
  FarolInfoEp vazia[2] = { { 2, 4, "" }, { 2, 5, "proxima" } };
  char longa[80];

  CHECA(farolinfo_retomada_sinopse(2, 4, eps, 4) == eps[1].sinopse, "casamento exato");
  CHECA(farolinfo_retomada_sinopse(2, 6, eps, 4) == NULL, "sem casamento nao chuta");
  CHECA(farolinfo_retomada_sinopse(0, 4, eps, 4) == NULL, "sem diario (t=0)");
  CHECA(farolinfo_retomada_sinopse(2, 0, eps, 4) == NULL, "sem diario (e=0)");
  CHECA(farolinfo_retomada_sinopse(2, 4, dup, 3) == NULL, "duplicado e ambiguo");
  CHECA(farolinfo_retomada_sinopse(2, 4, vazia, 2) == NULL, "sinopse vazia nao cai na proxima");
  CHECA(farolinfo_retomada_sinopse(2, 4, NULL, 4) == NULL, "lista nula");

  CHECA(farolinfo_retomada_linha_ok(RESUMO_OK), "linha boa");
  CHECA(farolinfo_retomada_linha_ok("Ação no posto de gasolina à noite."), "acentos");
  CHECA(!farolinfo_retomada_linha_ok("um\ndois"), "\\n");
  CHECA(!farolinfo_retomada_linha_ok("um\rdois"), "\\r");
  CHECA(!farolinfo_retomada_linha_ok("um\tdois"), "\\t");
  CHECA(!farolinfo_retomada_linha_ok("um\xe2\x80\xa8" "dois"), "LS");
  CHECA(!farolinfo_retomada_linha_ok("um\xc2\x85" "dois"), "NEL");
  CHECA(!farolinfo_retomada_linha_ok(" comeca com espaco"), "espaco inicial");
  CHECA(!farolinfo_retomada_linha_ok("termina com espaco "), "espaco final");
  CHECA(!farolinfo_retomada_linha_ok(""), "vazia");
  CHECA(!farolinfo_retomada_linha_ok("\xc0\xaf"), "sobrelongo");
  CHECA(!farolinfo_retomada_linha_ok("\xed\xa0\x80"), "surrogate");
  memset(longa, 'a', 62); longa[62] = 0;
  CHECA(farolinfo_retomada_linha_ok(longa), "62 cabe");
  memset(longa, 'a', 63); longa[63] = 0;
  CHECA(!farolinfo_retomada_linha_ok(longa), "63 nao cabe");
  puts("ok  unidades: sinopse por (t, e) exato e linha unica <= 62");
}

static void checaResumoAceitoEp(const char *caso, int t, int e, const P6 *base, const P6 *p,
                                const char *esperado) {
  char res[192];
  int ok = farolcompor_retomada_resumo("tt0903747", t, e, res, sizeof res);
  CHECA(ok == 1, "%s: farolcompor_retomada_resumo devia devolver 1, veio %d", caso, ok);
  CHECA(!strcmp(res, esperado), "%s: resumo retornado [%s] != [%s]", caso, res, esperado);
  CHECA(p->temTempo, "%s: linha de tempo T%dE%d sumiu", caso, t, e);
  CHECA(p->n == base->n + 1, "%s: esperava %d linhas na P6, veio %d", caso, base->n + 1, p->n);
  CHECA(p->posTempo == base->posTempo, "%s: posTempo mudou (%d vs %d)", caso, p->posTempo, base->posTempo);
  if (p->posTempo >= 0 && p->posTempo + 1 < p->n) {
    CHECA(!strcmp(p->linhas[p->posTempo], base->linhas[base->posTempo]),
          "%s: linha de tempo alterada [%s]", caso, p->linhas[p->posTempo]);
    CHECA(!strcmp(p->linhas[p->posTempo + 1], esperado),
          "%s: linha P6 abaixo da retomada [%s] != [%s]", caso, p->linhas[p->posTempo + 1], esperado);
  }
}

static void checaResumoAceito(const char *caso, const P6 *base, const P6 *p,
                              const char *esperado) {
  checaResumoAceitoEp(caso, 2, 4, base, p, esperado);
}

static void checaResumoRecusadoEp(const char *caso, int t, int e, const P6 *base, const P6 *p) {
  char res[192];
  int ok = farolcompor_retomada_resumo("tt0903747", t, e, res, sizeof res);
  CHECA(ok == 0 && res[0] == '\0', "%s: farolcompor_retomada_resumo vazou [%s]", caso, res);
  CHECA(p6Igual(p, base), "%s: P6 mudou (%d linhas vs %d, linha1=[%s])",
        caso, p->n, base->n, p->n > 1 ? p->linhas[1] : "");
}

static void checaResumoRecusado(const char *caso, const P6 *base, const P6 *p) {
  checaResumoRecusadoEp(caso, 2, 4, base, p);
}

// --- 1. caminho bom + canario anti-spoiler ----------------------------------
static void teste_resumo_valido(void) {
  int n0, n;
  char *c;
  P6 base, p;

  base = abrirCaso();
  escreverCenario("ok");
  n0 = nCapturas();
  p = rodar(4000, &base);
  n = nCapturas();
  CHECA(n == n0 + 1, "esperava 1 POST, veio %d", n - n0);
  checaResumoAceito("valido", &base, &p, RESUMO_OK);
  CHECA(!strchr(p.linhas[p.posTempo + 1], '\n'), "resumo com quebra");
  CHECA(faroldetalhe_altura(1040.0f) > 0.0f, "bloco sem altura");

  c = lerCaptura(&n);
  CHECA(strstr(c, "\"modo\":\"retomada\"") != NULL, "modo retomada ausente");
  CHECA(strstr(c, "\"retomada\":{\"temporada\":2,\"episodio\":4,\"sinopse\":\"" SINOPSE_VISTA "\"}") != NULL,
        "corpo sem a sinopse do episodio visto: %s", c);
  CHECA(strstr(c, "\"id\":\"tt0903747\"") != NULL, "serie ausente");
  free(c);
  auditarCorpos("valido");

  // Mesmo (perfil, serie, T2E4): nao sai de novo na sessao.
  {
    int k;
    farolcompor_cancelar();
    faroldetalhe_abrir(3, g_t);
    for (k = 0; k < 120; k++) { quadro(); usleep(2000); }
    CHECA(nCapturas() == n, "pedido repetido para o mesmo episodio");
  }
  puts("ok  resumo valido: 1 POST com a sinopse de T2E4, canarios E5/T3 ausentes, linha abaixo da retomada");
}

// --- 2. zero POST ------------------------------------------------------------
typedef void (*Prep)(void);
static void prepSemDiario(void)  { atomic_store(&g_retT, 0); atomic_store(&g_retE, 0); atomic_fetch_add(&g_diaGer, 1); }
static void prepInfantil(void)   { atomic_store(&g_infantil, 1); }
static void prepSemConsent(void) { farolcompor_definir_consentimento(0); }
static void prepPlayer(void)     { atomic_store(&g_player, 1); }
static void prepOffline(void)    { atomic_store(&g_offline, 1); }
static void prepSemSinopse(void) { montarEpisodios(0, 0); }
static void prepDuplicado(void)  { montarEpisodios(1, 1); }
static void prepSeguro(void)     { atomic_store(&g_seguro, 1); }

static void teste_zero_post(void) {
  static const struct { const char *n; Prep f; int temTempo; } casos[] = {
    { "sem-diario",      prepSemDiario,  0 },
    { "infantil",        prepInfantil,   1 },
    { "sem-consentimento", prepSemConsent, 1 },
    { "player-aberto",   prepPlayer,     1 },
    { "offline",         prepOffline,    1 },
    { "sem-sinopse-do-visto", prepSemSinopse, 1 },
    { "lista-ambigua",   prepDuplicado,  1 },
    { "modo-seguro",     prepSeguro,     0 },
  };
  int i;
  escreverCenario("ok");
  for (i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
    int n0, k;
    char res[192];
    P6 base, p;
    base = abrirCaso();
    casos[i].f();
    n0 = nCapturas();
    for (k = 0; k < 150; k++) { quadro(); usleep(4000); }
    p = lerP6();
    CHECA(nCapturas() == n0, "%s: houve POST", casos[i].n);
    CHECA(farolcompor_retomada_resumo("tt0903747", 2, 4, res, sizeof res) == 0 && res[0] == '\0',
          "%s: resumo vazou [%s]", casos[i].n, res);
    if (casos[i].temTempo)
      CHECA(p6Igual(&p, &base), "%s: P6 mudou (%d vs %d)", casos[i].n, p.n, base.n);
    else
      CHECA(!p.temTempo && p.n == 0, "%s: P6 devia ficar vazia, veio %d", casos[i].n, p.n);
    auditarCorpos(casos[i].n);
  }
  puts("ok  zero POST: sem Diario, infantil, sem consentimento, player, offline, sem sinopse, ambigua, seguro");
}

// --- 3. falhas do servidor caem na linha de tempo ----------------------------
static void teste_quedas(void) {
  static const char *cenarios[] = {
    "s500", "s429", "regras", "nl", "proibida", "t2e5", "alvo_outro", "torto", "longa",
    "ep5", "ep5_en", "temp3", "futuro",
    "p1_ep_cinco_pt", "p1_ep_five_en", "p1_ep_cinco_es",
    "p1_eleventh_en", "p1_3ponto_a", "p1_chegara",
    "will_die", "he_will", "ep_21_pt", "ep_21_en", "ep_21_es",
    "p1_r3_ep_no5", "p1_r3_ep_n5", "p1_r3_seas_no3", "p1_r3_ep_ponto5",
    "p1_r3_cap_ponto5", "p1_r3_temp_ponto3", "p1_r3_ep_nro5",
  };
  int i;
  for (i = 0; i < (int)(sizeof cenarios / sizeof cenarios[0]); i++) {
    int n0;
    P6 base, p;
    base = abrirCaso();
    escreverCenario(cenarios[i]);
    n0 = nCapturas();
    p = rodar(2500, &base);
    CHECA(nCapturas() == n0 + 1, "%s: esperava 1 POST", cenarios[i]);
    checaResumoRecusado(cenarios[i], &base, &p);
    auditarCorpos(cenarios[i]);
  }
  puts("ok  quedas: 500, 429, regras, \\n, ALMA, T2E5, P1-1..6, r3 abrev., will/21, JSON torto e longa -> linha de tempo");
}

// --- 3b. positivos P2-1/P2-2 e controles do proprio episodio -----------------
static void teste_positivos_p2_e_controles(void) {
  static const struct { const char *cen; const char *esp; } casos[] = {
    { "pos_revelara",     "Jesse soube que Walt revelara o segredo no deserto." },
    { "pos_escapara",     "Jesse negó que Walt escapara del desierto." },
    { "pos_will_found",   "Will found a trail in the desert." },
    { "pos_proximo_ao",   "Walt encontrou Jesse próximo ao deserto." },
    { "ctrl_ep4",         "No episódio 4, Walter confronta Jesse no deserto." },
    { "ctrl_temp2",       "Na 2ª temporada, Walter encontrou Jesse no deserto." },
    { "ctrl_ep_no4",      "In episode no. 4, Walt confronts Jesse." },
    { "ctrl_ep_n4",       "No episódio n. 4, Walt confronta Jesse." },
    { "pos_alvara",       "Walt perde o alvará do laboratório." },
    { "pos_guarana_para", "Walt toma guaraná com maracujá no Pará." },
    { "pos_ep_ponto_fim", "Walt watched the episode. 5 days later he ran." },
    { "pos_r5_ep_later",  "The episode later shows Walt confronting Jesse." },
    { "pos_r5_ep_depois", "O episódio depois mostra Walt confrontando Jesse." },
    { "pos_r5_ep_tarde",  "El episodio más tarde muestra a Walt y a Jesse." },
    { "pos_r5_ep_after",  "Walt ends the episode after confronting Jesse." },
    { "pos_r5_cap_after", "Walt grabs his cap after confronting Jesse." },
  };
  int i;
  for (i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
    int n0;
    P6 base, p;
    base = abrirCaso();
    escreverCenario(casos[i].cen);
    n0 = nCapturas();
    p = rodar(4000, &base);
    CHECA(nCapturas() == n0 + 1, "%s: esperava 1 POST", casos[i].cen);
    checaResumoAceito(casos[i].cen, &base, &p, casos[i].esp);
    auditarCorpos(casos[i].cen);
  }
  puts("ok  positivos P2-1/P2-2 e controles: revelara, escapara, Will found, alvara, no. 4, n. 4, ep 4, 2a temporada");
}

// --- 3c. P2-1 ordinais compostos (twenty-first T2E21, decimo primeiro T2E11) --
static void teste_p2_ordinais_compostos(void) {
  static const struct { int t; int e; const char *cen; const char *esp; } casos[] = {
    { 2, 21, "ep_21_en_ok", "In the twenty-first episode, Walt flees." },
    { 2, 11, "ep_11_pt_ok", "No décimo primeiro episódio, Walt foge." },
  };
  int i;
  for (i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
    int n0;
    P6 base, p;
    base = abrirCasoEp(casos[i].t, casos[i].e);
    escreverCenario(casos[i].cen);
    n0 = nCapturas();
    p = rodarEp(casos[i].t, casos[i].e, 4000, &base);
    CHECA(nCapturas() == n0 + 1, "%s: esperava 1 POST", casos[i].cen);
    checaResumoAceitoEp(casos[i].cen, casos[i].t, casos[i].e, &base, &p, casos[i].esp);
    auditarCorpos(casos[i].cen);
  }
  puts("ok  P2-1 ordinais compostos: twenty-first (T2E21) e decimo primeiro (T2E11)");
}

// --- 3d. P1-r4 / P1-r5: plural com numero ou singular+marcador temporal -> recusado ---
static void teste_p1_r4_marcador_temporal(void) {
  static const struct { int t; int e; const char *cen; } casos[] = {
    { 2, 21, "p1_r4_21_later" },
    { 2, 22, "p1_r4_22_depois" },
    { 2,  4, "p1_r4_4_later" },
    { 3,  4, "p1_r4_3temp_despues" },
    { 2,  4, "p1_r5_4_from_now" },
    { 2, 21, "p1_r5_21_from_now" },
    { 2, 21, "p1_r5_21_pass" },
    { 2, 22, "p1_r5_22_se_passam" },
    { 2,  4, "p1_r5_eps_4_5" },
    { 2,  1, "p1_r5_one_ep_later" },
    { 2,  1, "p1_r5_1_ep_from_now" },
    { 2,  1, "p1_r5_an_ep_later" },
    { 2,  1, "p1_r5_um_ep_depois" },
    { 1,  4, "p1_r5_one_seas_later" },
    { 2,  4, "p1_r6_several_later" },
    { 2,  4, "p1_r6_alguns_depois" },
    { 2,  4, "p1_r6_varias_despues" },
    { 2,  4, "p1_r6_ep_after_this" },
    { 2,  4, "p1_r6_ep_depois_deste" },
    { 2,  4, "p1_r6_ep_despues_este" },
    { 2,  4, "p1_r6_several_pass" },
    { 2,  4, "p1_r6_episodes_later" },
    { 2,  4, "p1_r6_seasons_from_now" },
  };
  int i;
  for (i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
    int n0;
    P6 base, p;
    base = abrirCasoEp(casos[i].t, casos[i].e);
    escreverCenario(casos[i].cen);
    n0 = nCapturas();
    p = rodarEp(casos[i].t, casos[i].e, 2500, &base);
    CHECA(nCapturas() == n0 + 1, "%s: esperava 1 POST", casos[i].cen);
    checaResumoRecusadoEp(casos[i].cen, casos[i].t, casos[i].e, &base, &p);
    auditarCorpos(casos[i].cen);
  }
  puts("ok  P1-r4/r5 plural+numero e singular+marcador temporal -> recusados");
}

// --- 4. prazo de 6 s ----------------------------------------------------------
static void teste_prazo(void) {
  P6 base, p;
  base = abrirCaso();
  escreverCenario("lento");
  p = rodar(9000, &base);
  checaResumoRecusado("prazo", &base, &p);
  CHECA(!farolcompor_em_voo(), "prazo: pedido ainda em voo");
  puts("ok  prazo 6 s: resposta lenta descartada, fica a linha de tempo");
}

// --- 5. player abre com o resumo na tela -> some ------------------------------
static void teste_player_abre_depois(void) {
  P6 base, p;
  int k;
  base = abrirCaso();
  escreverCenario("ok");
  p = rodar(4000, &base);
  checaResumoAceito("player-depois-antes", &base, &p, RESUMO_OK);
  atomic_store(&g_player, 1);
  for (k = 0; k < 4; k++) quadro();
  p = lerP6();
  CHECA(p6Igual(&p, &base), "player-depois: resumo ficou com o player aberto");
  atomic_store(&g_player, 0);
  puts("ok  player aberto depois: o resumo sai da P6");
}

// --- 6. abertura via Continuar assistindo com IMDb composto ("tt...:2:5") ---
static void teste_imdb_composto(void) {
  int n0, n;
  P6 base, p;

  base = abrirCaso();
  pthread_mutex_lock(&g_catMu);
  snprintf(g_itens[3].imdb, sizeof g_itens[3].imdb, "tt0903747:2:5");
  pthread_mutex_unlock(&g_catMu);
  g_t += 5000u;
  atomic_store(&g_ms, g_t);
  faroldetalhe_abrir(3, g_t);

  escreverCenario("ok");
  n0 = nCapturas();
  p = rodar(4000, &base);
  n = nCapturas();
  CHECA(n == n0 + 1, "composto: esperava 1 POST, veio %d", n - n0);
  checaResumoAceito("imdb-composto", &base, &p, RESUMO_OK);
  auditarCorpos("imdb-composto");
  puts("ok  IMDb composto (tt0903747:2:5): 1 POST com sinopse T2E4 + linha de tempo + resumo");
}

// --- 7. retorno apos offline (P2-3): nao marca como tentado na sessao ---------
static void teste_offline_volta_online(void) {
  int n0, k;
  P6 base, p;

  base = abrirCaso();
  escreverCenario("ok");
  atomic_store(&g_offline, 1);
  n0 = nCapturas();
  for (k = 0; k < 20; k++) { quadro(); usleep(4000); }
  CHECA(nCapturas() == n0, "offline-volta: houve POST offline");
  p = lerP6();
  checaResumoRecusado("offline-volta-offline", &base, &p);

  atomic_store(&g_offline, 0);
  g_t += 1500u;
  atomic_store(&g_ms, g_t);
  p = rodar(4000, &base);
  CHECA(nCapturas() == n0 + 1, "offline-volta: nao buscou quando a rede voltou");
  checaResumoAceito("offline-volta-online", &base, &p, RESUMO_OK);
  auditarCorpos("offline-volta");
  puts("ok  offline volta online: zero POST offline, busca e exibe resumo ao reconectar");
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "uso: %s <base> <cenario> <captura>\n", argv[0]); return 2; }
  g_cenArq = argv[2];
  g_capArq = argv[3];
  farolcompor_definir_url(argv[1]);
  farolcompor_definir_auth("Authorization: Bearer tok-retomada-57", "X-Nuvio-Auth: nuvio",
                           "X-Nuvio-Perfil: 1");

  teste_unidades();
  teste_resumo_valido();
  teste_zero_post();
  teste_quedas();
  teste_positivos_p2_e_controles();
  teste_p2_ordinais_compostos();
  teste_p1_r4_marcador_temporal();
  teste_prazo();
  teste_player_abre_depois();
  teste_imdb_composto();
  teste_offline_volta_online();

  farolcompor_encerrar();
  if (falhas) { fprintf(stderr, "farol_retomada: %d falha(s)\n", falhas); return 1; }
  puts("farol_retomada: ok");
  return 0;
}
