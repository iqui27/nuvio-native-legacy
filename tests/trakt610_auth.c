// #610: exercise the real auth state machine with deterministic HTTP responses.
#include "stub_perfisessao.h"
#define main perfil_main
#define rede_postar_st_retry perfil_postar_retry
#define sync_empurrar_credencial perfil_empurrar
#include "traktauth_perfil.c"
#undef rede_postar_st_retry
#undef sync_empurrar_credencial
#undef main
#define credfio_resultado resultado_observado
#include "../src/traktauth.c"
#undef credfio_resultado
int credfio_resultado(const char *provider, int *res);
static int pushConsumido;
int resultado_observado(const char *p, int *r) {
  int pronto = credfio_resultado(p, r);
  if (pronto) pushConsumido++;
  return pronto;
}
int sessao_identidade_local_copiar(char *u, size_t nu, char *j, size_t nj) {
  (void)nu; (void)nj; u[0] = j[0] = 0; return 0;
}
const char *sessao_usuario(void) { return ""; }

static int respostas429 = 1, retry = 90, pedidos;
char *rede_postar_st_retry(const char *u, int s, const char *const *c,
                           const char *b, int *st, int *ra) {
  pedidos++;
  if (respostas429) { respostas429--; *st = 429; *ra = retry; return strdup(""); }
  *ra = 0;
  return rede_postar_st(u, s, c, b, st);
}
static pthread_mutex_t pushTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pushCond = PTHREAD_COND_INITIALIZER;
static int pushEntrou, pushSolto;
int sync_empurrar_credencial(const char *p, const char *j) {
  (void)p; (void)j;
  pthread_mutex_lock(&pushTrava);
  pushEntrou = 1; pthread_cond_broadcast(&pushCond);
  while (!pushSolto) pthread_cond_wait(&pushCond, &pushTrava);
  pthread_mutex_unlock(&pushTrava);
  return 1;
}
static void terminar(void) {
  for (int i = 0; i < 3000 && !fioPronto; i++) usleep(1000);
  assert(fioPronto);
}
int main(void) {
  traktauth_comecar(); terminar();
  assert(traktauth_estado() == TRA_PEDINDO && traktauth_aguardando());
  assert(strstr(traktauth_erro(), "Trakt limitou"));
  traktauth_passo(1000);
  traktauth_passo(90999); assert(pedidos == 1);
  traktauth_passo(91000); terminar();
  assert(traktauth_estado() == TRA_AGUARDANDO);
  respostas429 = 1; retry = 120;
  traktauth_passo(92000); terminar();
  assert(traktauth_estado() == TRA_AGUARDANDO);
  assert(strstr(traktauth_erro(), "Trakt limitou"));
  traktauth_passo(93000);
  int antes = pedidos;
  traktauth_passo(212999); assert(pedidos == antes);
  traktauth_passo(213000); terminar();
  traktauth_passo(214000); assert(traktauth_estado() == TRA_LIGADO);
  // Repeated 429 without a header doubles the delay; Back does not bypass it.
  traktauth_esquecer();
  respostas429 = 3; retry = 0;
  traktauth_comecar(); terminar();
  traktauth_passo(300000);
  traktauth_cancelar(); traktauth_comecar();
  antes = pedidos;
  traktauth_passo(309999); assert(pedidos == antes);
  traktauth_passo(310000); terminar(); traktauth_passo(311000);
  antes = pedidos;
  traktauth_passo(330999); assert(pedidos == antes);
  traktauth_passo(331000); terminar(); traktauth_passo(332000);
  antes = pedidos;
  traktauth_passo(371999); assert(pedidos == antes);
  traktauth_passo(372000); terminar();
  // A server delay cannot keep a device flow alive past its expiry.
  expiraEm = (long)time(NULL) + 2;
  respostas429 = 1; retry = 2147483647;
  traktauth_passo(380000); terminar();
  assert(esperaMs <= 2000u);
  expiraEm = (long)time(NULL) - 1;
  traktauth_passo(381000);
  assert(traktauth_estado() == TRA_ERRO && !traktauth_aguardando());
  // Refresh 429 preserves the session and observes headers longer than 60 s.
  traktauth_esquecer();
  snprintf(token, sizeof token, "OLD"); snprintf(refresh, sizeof refresh, "R1");
  usuarioCopiar(usuarioEstado); estado = TRA_LIGADO; renovacaoPend = 1;
  respostas429 = 1; retry = 180;
  traktauth_passo(400000); terminar(); traktauth_passo(401000);
  antes = pedidos;
  traktauth_passo(580999); assert(pedidos == antes);
  traktauth_passo(581000); terminar(); traktauth_passo(582000);
  assert(traktauth_estado() == TRA_LIGADO && !strcmp(token, "RENOVADO-P1"));
  // Complete a real credential push only after disconnect writes its marker.
  assert(credfio_iniciar("trakt", "{}"));
  pushGeracao = pushGeracaoAtual;
  pthread_mutex_lock(&pushTrava);
  while (!pushEntrou) pthread_cond_wait(&pushCond, &pushTrava);
  traktauth_desconectar();
  pushSolto = 1; pthread_cond_broadcast(&pushCond);
  pthread_mutex_unlock(&pushTrava);
  for (int i = 0; i < 3000 && !pushConsumido; i++) {
    traktauth_passo(583000);
    if (!pushConsumido) usleep(1000);
  }
  assert(pushConsumido == 1);
  assert(contem("trakt-p1.txt", "!"));
  assert(traktauth_perfil() == 1 && !trakt_ativo() && traktauth_desconectado());
  assert(!traktauth_carregar_perfil(1) && traktauth_desconectado());
  // A response from the disconnected generation cannot recreate its token.
  gerFio = geracao - 1; perfilFio = 1; usuarioCopiar(usuarioFio);
  snprintf(refreshFio, sizeof refreshFio, "R1");
  fioRenovar(NULL);
  assert(contem("trakt-p1.txt", "!") && !token[0]);
  traktauth_comecar(); terminar();
  traktauth_passo(700000); terminar(); traktauth_passo(701000);
  assert(traktauth_estado() == TRA_LIGADO && !traktauth_desconectado());
  puts("#610 auth: PASS (Retry-After, backoff, profile disconnect, stale response, reconnect)");
  return 0;
}
