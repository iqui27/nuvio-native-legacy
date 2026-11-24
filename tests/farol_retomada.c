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

static void montarEpisodios(int comSinopseVista, int duplicado) {
  memset(g_lista, 0, sizeof g_lista);
  g_lista[0].temporada = 2; g_lista[0].episodio = 3;
  snprintf(g_lista[0].sinopse, sizeof g_lista[0].sinopse, "SinopseAnteriorT2E3 sem interesse.");
  g_lista[1].temporada = 2; g_lista[1].episodio = 4;
  if (comSinopseVista)
    snprintf(g_lista[1].sinopse, sizeof g_lista[1].sinopse, "%s", SINOPSE_VISTA);
  g_lista[2].temporada = 2; g_lista[2].episodio = 5;
  snprintf(g_lista[2].sinopse, sizeof g_lista[2].sinopse, "%s: Walt foge.", CANARIO_E5);
  g_lista[3].temporada = 3; g_lista[3].episodio = 4;
  snprintf(g_lista[3].sinopse, sizeof g_lista[3].sinopse, "%s: outra fase.", CANARIO_T3);
  g_lista[4].temporada = 3; g_lista[4].episodio = 1;
  snprintf(g_lista[4].sinopse, sizeof g_lista[4].sinopse, "%s: recomeco.", CANARIO_T3);
  g_nEps = 5;
  if (duplicado) {
    g_lista[5] = g_lista[1];
    snprintf(g_lista[5].sinopse, sizeof g_lista[5].sinopse, "%s duplicada", CANARIO_E5);
    g_nEps = 6;
  }
  g_eps = g_lista;
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

// Linhas da P6 de agora: quantas, a de tempo (T2E4) e se o resumo esta.
typedef struct { int n, temTempo, temResumo, posTempo, posResumo; char resumo[192]; } P6;

static P6 lerP6(void) {
  P6 p;
  char l[192];
  int i;
  memset(&p, 0, sizeof p);
  p.posTempo = p.posResumo = -1;
  for (i = 0; i < 4; i++) {
    if (!faroldetalhe_linha_info(i, l, sizeof l)) break;
    p.n++;
    if (strstr(l, "T2E4")) { p.temTempo = 1; p.posTempo = i; }
    if (strstr(l, "deserto")) {
      p.temResumo = 1; p.posResumo = i;
      snprintf(p.resumo, sizeof p.resumo, "%s", l);
    }
  }
  return p;
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

// Roda ate ver o resumo (ou o pedido terminar e ainda mais ~1,5 s de quadros).
static P6 rodar(int maxMs) {
  long long fim = (long long)maxMs;
  long long dec = 0;
  int calmos = 0;
  P6 p = lerP6();
  while (dec < fim) {
    quadro();
    p = lerP6();
    if (p.temResumo) { quadro(); return lerP6(); }
    if (!farolcompor_em_voo()) { if (++calmos > 90) break; }
    else calmos = 0;
    usleep(16000);
    dec += 16;
  }
  return lerP6();
}

// Cada caso comeca do zero: perfil "trocado" (zera o dedupe da sessao), P6
// reaberta, diario com T2E4 visto ha 7 semanas e consentimento dado.
static void abrirCaso(void) {
  atomic_store(&g_player, 0);
  atomic_store(&g_offline, 0);
  atomic_store(&g_infantil, 0);
  atomic_store(&g_seguro, 0);
  atomic_store(&g_recomendar, 1);
  farolcompor_trocar_perfil();
  farolcompor_definir_consentimento(1);
  atomic_store(&g_retT, 2);
  atomic_store(&g_retE, 4);
  atomic_store(&g_retQ, atomic_load(&g_epoch) - 49LL * 86400LL);
  atomic_fetch_add(&g_diaGer, 1);
  montarEpisodios(1, 0);
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarSerie();
  g_t += 5000u;
  atomic_store(&g_ms, g_t);
  faroldetalhe_abrir(3, g_t);
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

// --- 1. caminho bom + canario anti-spoiler ----------------------------------
static void teste_resumo_valido(void) {
  int n0, n;
  char *c;
  P6 p;

  abrirCaso();
  escreverCenario("ok");
  n0 = nCapturas();
  p = rodar(4000);
  n = nCapturas();
  CHECA(n == n0 + 1, "esperava 1 POST, veio %d", n - n0);
  CHECA(p.temTempo, "linha de tempo T2E4 sumiu");
  CHECA(p.temResumo, "resumo nao entrou na P6");
  CHECA(p.posResumo == p.posTempo + 1, "resumo fora do lugar (%d vs %d)", p.posResumo, p.posTempo);
  CHECA(!strcmp(p.resumo, RESUMO_OK), "resumo diferente: [%s]", p.resumo);
  CHECA(!strchr(p.resumo, '\n'), "resumo com quebra");
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
    P6 p;
    abrirCaso();
    casos[i].f();
    n0 = nCapturas();
    for (k = 0; k < 150; k++) { quadro(); usleep(4000); }
    p = lerP6();
    CHECA(nCapturas() == n0, "%s: houve POST", casos[i].n);
    CHECA(!p.temResumo, "%s: resumo apareceu", casos[i].n);
    if (casos[i].temTempo)
      CHECA(p.temTempo, "%s: linha de tempo sumiu", casos[i].n);
    auditarCorpos(casos[i].n);
  }
  puts("ok  zero POST: sem Diario, infantil, sem consentimento, player, offline, sem sinopse, ambigua, seguro");
}

// --- 3. falhas do servidor caem na linha de tempo ----------------------------
static void teste_quedas(void) {
  static const char *cenarios[] = {
    "s500", "s429", "regras", "nl", "proibida", "t2e5", "alvo_outro", "torto", "longa",
  };
  int i;
  for (i = 0; i < (int)(sizeof cenarios / sizeof cenarios[0]); i++) {
    int n0;
    P6 p;
    abrirCaso();
    escreverCenario(cenarios[i]);
    n0 = nCapturas();
    p = rodar(2500);
    CHECA(nCapturas() == n0 + 1, "%s: esperava 1 POST", cenarios[i]);
    CHECA(p.temTempo, "%s: linha de tempo sumiu", cenarios[i]);
    CHECA(!p.temResumo, "%s: resumo indevido [%s]", cenarios[i], p.resumo);
    auditarCorpos(cenarios[i]);
  }
  puts("ok  quedas: 500, 429, regras, \\n, ALMA, T2E5, alvo trocado, JSON torto e longa -> so a linha de tempo");
}

// --- 4. prazo de 6 s ----------------------------------------------------------
static void teste_prazo(void) {
  P6 p;
  abrirCaso();
  escreverCenario("lento");
  p = rodar(9000);
  CHECA(p.temTempo, "prazo: linha de tempo sumiu");
  CHECA(!p.temResumo, "prazo: resumo chegou depois do prazo");
  CHECA(!farolcompor_em_voo(), "prazo: pedido ainda em voo");
  puts("ok  prazo 6 s: resposta lenta descartada, fica a linha de tempo");
}

// --- 5. player abre com o resumo na tela -> some ------------------------------
static void teste_player_abre_depois(void) {
  P6 p;
  int k;
  abrirCaso();
  escreverCenario("ok");
  p = rodar(4000);
  CHECA(p.temResumo, "player-depois: resumo nao entrou");
  atomic_store(&g_player, 1);
  for (k = 0; k < 4; k++) quadro();
  p = lerP6();
  CHECA(!p.temResumo, "player-depois: resumo ficou com o player aberto");
  atomic_store(&g_player, 0);
  puts("ok  player aberto depois: o resumo sai da P6");
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
  teste_prazo();
  teste_player_abre_depois();

  farolcompor_encerrar();
  if (falhas) { fprintf(stderr, "farol_retomada: %d falha(s)\n", falhas); return 1; }
  puts("farol_retomada: ok");
  return 0;
}
