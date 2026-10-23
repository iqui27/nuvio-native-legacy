#include "intro.h"
#include "creditosjson.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t netTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t netCond = PTHREAD_COND_INITIALIZER;
static pthread_key_t fimFio;
static int entrou[2], soltar[2], acabou[2];
static const char *resposta = "{\"credits\":[{\"start_ms\":900000,\"end_ms\":1000000}]}";

// O destrutor roda depois da publicacao, ao sair do fio destacado de intro.c.
static void terminou(void *p) {
  int i = (int)(size_t)p - 1;
  pthread_mutex_lock(&netTrava); acabou[i] = 1;
  pthread_cond_broadcast(&netCond); pthread_mutex_unlock(&netTrava);
}
char *rede_baixar(const char *url, int segundos) {
  int i = strstr(url, "tt000001") ? 0 : strstr(url, "tt000003") ? 1 : -1;
  (void)segundos;
  if (i >= 0) {
    assert(!pthread_setspecific(fimFio, (void *)(size_t)(i + 1)));
    pthread_mutex_lock(&netTrava); entrou[i] = 1; pthread_cond_broadcast(&netCond);
    while (!soltar[i]) pthread_cond_wait(&netCond, &netTrava);
    pthread_mutex_unlock(&netTrava);
    return strdup("{\"credits\":[{\"start_ms\":100000,\"end_ms\":200000}]}");
  }
  return strdup(resposta);
}
const char *dados_dir(void) { return ""; }
char *dados_caminho(char *dst, unsigned size, const char *name) {
  (void)dst; (void)size; (void)name; return NULL;
}
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}
void dados_marcar_sujo(int leve) { (void)leve; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; (void)r; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
static int fixtureBuscar(const char *url, char **corpo, int *status) {
  *corpo = rede_baixar(url, 12); *status = 200; return 1;
}
static const char *respostaAni = "{\"results\":[{\"skipType\":\"op\",\"interval\":{\"startTime\":10,\"endTime\":100},\"episodeLength\":1440},{\"skipType\":\"ed\",\"interval\":{\"startTime\":1350,\"endTime\":1400},\"episodeLength\":1440}]}";
static int statusDb = 200;
static int fontesBuscar(const char *url, char **corpo, int *status) {
  int ani = strstr(url, "api.aniskip.com") != NULL;
  assert(ani || strstr(url, "api.theintrodb.org"));
  assert(!pthread_setspecific(fimFio, (void *)(size_t)1));
  *status = ani ? 200 : statusDb;
  *corpo = *status == 200 ? strdup(ani ? respostaAni : resposta) : NULL;
  return *status;
}
static void esperar(int *flag) {
  struct timespec limite; assert(!clock_gettime(CLOCK_REALTIME, &limite)); limite.tv_sec += 5;
  pthread_mutex_lock(&netTrava);
  while (!*flag) assert(!pthread_cond_timedwait(&netCond, &netTrava, &limite));
  pthread_mutex_unlock(&netTrava);
}
static void liberar(int i) {
  pthread_mutex_lock(&netTrava); soltar[i] = 1; pthread_cond_broadcast(&netCond);
  pthread_mutex_unlock(&netTrava); esperar(&acabou[i]);
}
static void verificar(const char *j, int esperado) {
  IntroTrecho v[8]; int n = creditosjson_extrair(j, v, 8);
  assert(n == esperado);
  for (int i = 0; i < n; i++)
    assert(v[i].tipo == INTRO_CREDITOS && v[i].inicio >= 0.0 && v[i].fim > v[i].inicio);
}
#undef R_OK
#include "../src/intro.c"

int main(void) {
  IntroTrecho v[8];
  verificar("{\"type\":\"movie\",\"tmdb_id\":1,\"credits\":[{\"start_ms\":900000,\"end_ms\":1000000}]}", 1);
  verificar("{\"credits\":[{\"end_ms\":2000.5,\"note\":{\"x\":[null,true,false,\"ok\"]},\"start_ms\":1e3}]}", 1);
  verificar("{\"intro\":[{\"start_ms\":0,\"end_ms\":5000}],\"credits\":[{\"start_ms\":0,\"end_ms\":1000},{\"start_ms\":2000,\"end_ms\":3000}]}", 2);
  verificar("{\"credits\":[{\"start_ms\":1,\"end_ms\":null},{\"start_ms\":2,\"end_ms\":3}]}", 1);
  const char *invalidos[] = {
    "{}", "", "nao e json", "[]", "{\"credits\":{\"start_ms\":0,\"end_ms\":1}}",
    "{\"nested\":{\"credits\":[{\"start_ms\":0,\"end_ms\":1}]}}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1}],\"credits\":[]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1}],\"\\u0063redits\":[]}",
    "{\"credits\":[{\"end_ms\":1000}]}", "{\"credits\":[{\"start_ms\":0}]}",
    "{\"credits\":[{\"start_ms\":null,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":null}]}",
    "{\"credits\":[{\"start_ms\":\"0\",\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":\"1000\"}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":true}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1e999}]}",
    "{\"credits\":[{\"start_ms\":NaN,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":Infinity}]}",
    "{\"credits\":[{\"start_ms\":-1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":-1e-999,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":1000,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":1000,\"end_ms\":1}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":30garbage}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":01}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":+1}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":.1}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1.}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1,}]}",
    "{\"credits\":[{\"start_ms\":0,\"start_ms\":1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"\\u0073tart_ms\":1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1,\"end_ms\":1000}]}",
    "{\"credits\":[{\"nested\":{\"start_ms\":0},\"end_ms\":1000}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":{\"end_ms\":1000}}]}",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1000}]", // documento cortado
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1000}]}junk",
    "{\"credits\":[{\"start_ms\":0,\"end_ms\":1000}],\"bad\":undefined}"
  };
  for (size_t i = 0; i < sizeof invalidos / sizeof *invalidos; i++) verificar(invalidos[i], 0);
  assert(!creditosjson_extrair(NULL, v, 8));
  assert(!creditosjson_extrair(resposta, NULL, 8));
  assert(!creditosjson_extrair(resposta, v, 0));
  assert(creditosjson_extrair("{\"credits\":[{\"start_ms\":0,\"end_ms\":1},{\"start_ms\":2,\"end_ms\":3}]}", v, 1) == 1);
  // O marcador aberto continua aparecendo no caminho manual.
  assert(intro_extrair("{\"credits\":[{\"start_ms\":900000,\"end_ms\":null}]}", v, 8) == 1 && v[0].fim == 0.0);

  // A leitura explicita deve obedecer a mesma janela do botao manual.
  creditosLimitados[0] = (IntroTrecho){900, 1000, INTRO_CREDITOS}; nCreditosLimitados = 1;
  durMidia = 0; ehFilme = 0;
  assert(!intro_creditos_limitados(v, 8));
  durMidia = 1500;
  assert(!intro_creditos_limitados(v, 8)); // outro corte: meio do episodio
  durMidia = 1000;
  assert(intro_creditos_limitados(v, 8) == 1);
  durMidia = 3000; ehFilme = 1;
  assert(!intro_creditos_limitados(v, 8)); // abertura do filme nao e final
  durMidia = 1000; ehFilme = 0;
  // O fluxo unico exige limites explicitos e nunca pula uma previa/cena junto.
  trechos[0] = (IntroTrecho){900, 950, INTRO_CREDITOS};
  trechos[1] = (IntroTrecho){950, 980, INTRO_PREVIA}; nTrechos = 2;
  creditosLimitados[0] = trechos[0];
  double fim; int tipo;
  assert(intro_ativo(910, &fim, &tipo) && fim == 980);
  assert(intro_auto_ativo(910, &fim, &tipo) && fim == 950 && tipo == INTRO_CREDITOS);
  assert(intro_auto_pular(tipo, fim, 1000, 0, 0, 1));
  intro_auto_marcar(tipo, fim);
  assert(intro_auto_ja(tipo, fim)); // recuo nao repete o pulo
  intro_auto_recuar(960, 890);
  assert(!intro_auto_ativo(910, &fim, &tipo)); // protege resposta tardia tambem
  geracao++; // outra reproducao permite o marcador novamente
  assert(intro_auto_ativo(910, &fim, &tipo));
  nCreditosLimitados = 0;
  assert(intro_ativo(910, &fim, &tipo) && !intro_auto_ativo(910, &fim, &tipo));
  capTr[0] = trechos[0]; nCapTr = 1;
  assert(intro_auto_ativo(910, &fim, &tipo) && fim == 950); // capitulo real tem prioridade
  capTr[0].fim = 0;
  assert(!intro_auto_ativo(910, &fim, &tipo)); // capitulo aberto nao e limite explicito
  nCapTr = 0;
  creditosLimitados[0] = (IntroTrecho){900, 1000, INTRO_CREDITOS}; nCreditosLimitados = 1;
  trechos[0] = creditosLimitados[0]; nTrechos = 1;
  assert(intro_auto_ativo(910, &fim, &tipo));
  assert(!intro_auto_pular(tipo, fim, 1000, 0, 0, 1)); // ate o fim: fica para Next
  intro_definir_buscador(fixtureBuscar);
  assert(!pthread_key_create(&fimFio, terminou));
  intro_pedir("tt000001", 0, 0); esperar(&entrou[0]);
  intro_pedir("tt000002", 0, 0);
  for (int i = 0; i < 2000 && !intro_creditos_limitados(v, 8); i++) usleep(1000);
  assert(intro_creditos_limitados(v, 8) == 1 && v[0].inicio == 900.0 && v[0].fim == 1000.0);
  liberar(0); // A acaba depois de B: a resposta antiga nao sobrescreve B.
  assert(intro_creditos_limitados(v, 8) == 1 && v[0].inicio == 900.0);
  intro_pedir("tt000003", 0, 0); esperar(&entrou[1]);
  assert(!intro_creditos_limitados(v, 8)); // pedir outro titulo limpa imediatamente
  intro_desligar(); liberar(1);
  assert(!intro_creditos_limitados(v, 8)); // sair invalida tambem a resposta em voo
  assert(!intro_creditos_limitados(NULL, 8) && !intro_creditos_limitados(v, 0));
  // Oito limites descartados do TheIntroDB nao podem bloquear o ED escolhido.
  resposta = "{\"credits\":[{\"start_ms\":1200000,\"end_ms\":1210000},"
    "{\"start_ms\":1220000,\"end_ms\":1230000},{\"start_ms\":1240000,\"end_ms\":1250000},"
    "{\"start_ms\":1260000,\"end_ms\":1270000},{\"start_ms\":1280000,\"end_ms\":1290000},"
    "{\"start_ms\":1300000,\"end_ms\":1310000},{\"start_ms\":1320000,\"end_ms\":1330000},"
    "{\"start_ms\":1340000,\"end_ms\":1350000}]}";
  intro_definir_buscador(fontesBuscar);
  acabou[0] = 0;
  intro_pedir_ids("mal:21", 21, 1, 1, 0, 0); esperar(&acabou[0]);
  acabou[0] = 0;
  intro_definir_duracao(1440, 0); esperar(&acabou[0]);
  assert(intro_trechos(v, 8) == 2);
  assert(intro_ativo(1360, &fim, &tipo) && fim == 1400 && tipo == INTRO_CREDITOS);
  assert(intro_auto_ativo(1360, &fim, &tipo) && fim == 1400 && tipo == INTRO_CREDITOS);
  assert(intro_creditos_limitados(v, 8) == 1 && v[0].inicio == 1350 && v[0].fim == 1400 && v[0].ani);
  intro_desligar();
  puts("ok  limites descartados do TheIntroDB nao saturam o ED do AniSkip");
  // OP parcial continua publicado; 503 complementar ainda tenta apos 30 s.
  respostaAni = "{\"results\":[{\"skipType\":\"op\",\"interval\":{\"startTime\":10,\"endTime\":100},\"episodeLength\":1440}]}";
  resposta = "{\"recap\":[{\"start_ms\":0,\"end_ms\":10000}],\"credits\":[{\"start_ms\":1350000,\"end_ms\":1400000}]}";
  statusDb = 503; acabou[0] = 0;
  intro_pedir_ids("mal:21", 21, 1, 1, 0, 0); esperar(&acabou[0]);
  assert(intro_trechos(v, 8) == 1 && v[0].tipo == INTRO_ABERTURA && v[0].ani);
  acabou[0] = 0;
  intro_definir_duracao(1440, 0); esperar(&acabou[0]);
  assert(intro_trechos(v, 8) == 1 && v[0].inicio == 10);
  assert(intro_auto_ativo(20, &fim, &tipo) && fim == 100 && tipo == INTRO_ABERTURA);
  // Avanca somente o prazo da falha, sem mudar a duracao nem esperar 30 s.
  pthread_mutex_lock(&trava); falhouEm = (long)time(NULL); pthread_mutex_unlock(&trava);
  statusDb = 200;
  intro_definir_duracao(1440, 0);
  assert(intro_trechos(v, 8) == 1); // antes do prazo, segue parcial
  pthread_mutex_lock(&trava); falhouEm -= 30; pthread_mutex_unlock(&trava);
  acabou[0] = 0;
  intro_definir_duracao(1440, 0);
  for (int i = 0; i < 2000 && intro_trechos(v, 8) != 3; i++) usleep(1000);
  assert(intro_trechos(v, 8) == 3); // o retry recupera recap/creditos ausentes
  esperar(&acabou[0]);
  assert(v[0].tipo == INTRO_ABERTURA && v[0].inicio == 10 && v[0].ani);
  assert(v[1].tipo == INTRO_RESUMO && v[1].fim == 10 && !v[1].ani);
  assert(v[2].tipo == INTRO_CREDITOS && v[2].inicio == 1350 && !v[2].ani);
  assert(intro_auto_ativo(1360, &fim, &tipo) && fim == 1400 && tipo == INTRO_CREDITOS);
  intro_desligar();
  puts("ok  publicacao parcial mantem retry de 30 s da fonte complementar");
  pthread_key_delete(fimFio);
  puts("creditos_limitados: limites explicitos, leitura manual e publicacao por titulo OK");
  return 0;
}
