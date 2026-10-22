// SEM FIO, O DELETE REMOTO NAO RODA NO QUADRO.
//
// "Marcar como visto" e "Tirar de Continuar assistindo" mandam os DELETE do
// Trakt (ate 20 s cada pausa) e a RPC do syncprog num fio (fioTirarRemoto).
// Quando pthread_create falhava, o `else fioTirarRemoto(tr)` fazia tudo ISSO
// no fio de desenho: a tela congelava ate os servidores responderem. Aqui
// pthread_create de ctxmenu.c sempre falha e tirarremoto_executar e um duble
// que conta e demora: o caminho do menu tem de voltar rapido, sem chama-lo.
// O efeito local (registro apagado, carimbo de removido) continua.
//
//   bash tests/ctxmenu_semfio.sh
#include <pthread.h>
#include <time.h>
#include <unistd.h>
static int criarFalha(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) {
  (void)t; (void)a; (void)f; (void)arg; return 11;   // EAGAIN: sem fio
}
#define pthread_create(t, a, f, arg) criarFalha(t, a, f, arg)
#include "../src/ctxmenu.c"
#undef pthread_create

static int chamadasRemoto;
void tirarremoto_executar(const char *imdb, const char *chave, int ocultar) {
  (void)imdb; (void)chave; (void)ocultar;
  chamadasRemoto++;
  usleep(300 * 1000);   // a rede de verdade: ate 20 s
}

static unsigned long long agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}

int main(void) {
  CatItem ci;
  unsigned long long t0, dt;
  int falhas = 0;
  memset(&ci, 0, sizeof ci);
  snprintf(ci.imdb, sizeof ci.imdb, "tt0000001");
  snprintf(ci.tipo, sizeof ci.tipo, "series");
  snprintf(ci.titulo, sizeof ci.titulo, "Serie de teste");
  ci.temporada = 1; ci.episodio = 2;
  t0 = agoraMs();
  espelharAssistido(-1, &ci, 1);   // "Marcar como visto"
  dt = agoraMs() - t0;
  printf("  marcar como visto sem fio: %d chamada(s) remota(s) no quadro, %llu ms\n", chamadasRemoto, dt);
  if (chamadasRemoto != 0) { printf("  FALHOU: DELETE/RPC remoto rodou no fio de desenho\n"); falhas++; }
  if (dt >= 250) { printf("  FALHOU: o quadro esperou a rede (%llu ms)\n", dt); falhas++; }
  printf(falhas ? "ctxmenu_semfio: %d falha(s)\n" : "ctxmenu_semfio: ok\n", falhas);
  return falhas ? 1 : 0;
}
