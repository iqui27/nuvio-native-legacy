// INICIO DE VOLTA DA DESCOBERTA: ATOMICO, E O AGENDAMENTO LEVA O QUE PRECISA.
//
// 1. desc_iniciar testava `buscando` SEM trava e so depois tomava listaTrava
//    para marcar: dois chamadores (o fio adiado acordando + o quadro, ou a
//    volta condenada recomecando + o quadro) passavam juntos pelo teste e
//    largavam DOIS montar() sobre o mesmo `fio` estatico. O gancho em
//    pthread_mutex_lock segura cada um dos dois corredores na entrada da trava
//    ate o outro chegar tambem: antes do conserto os dois ja passaram pelo
//    teste (2 montar); depois, o teste e feito sob a trava (1 montar).
// 2. adiadoFio dormia e depois lia descVoltasAoAgendar GLOBAL: um segundo
//    agendamento no meio sobrescrevia a marca, e o primeiro fio acordava
//    achando que nenhuma volta tinha comecado — largava uma volta antes do
//    minimo entre voltas. Agora a marca viaja no argumento do fio.
//
// Reaproveita os dubles de tests/montagem_cedo.c (main dele renomeado).
// montar() nao roda: o gancho de pthread_create conta e troca por um fio vazio.
//
//   bash tests/desc_iniciar_corrida.sh
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
static void *montar(void *u);
static int testeTravar(pthread_mutex_t *m);
static int testeCriar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg);
#define pthread_mutex_lock(m) testeTravar(m)
#define pthread_create(t, a, f, arg) testeCriar(t, a, f, arg)
#define main montagem_cedo_main
#include "montagem_cedo.c"
#undef main
#undef pthread_mutex_lock
#undef pthread_create

static atomic_int corridaLigada, chegaram, montaresCriados;
static _Thread_local int ehCorredor;
static unsigned long long montarEm[16];

static int testeTravar(pthread_mutex_t *m) {
  if (ehCorredor && atomic_load(&corridaLigada) && m == &listaTrava) {
    int i;
    ehCorredor = 0;
    atomic_fetch_add(&chegaram, 1);
    // os dois corredores se encontram aqui (teto de 2 s se um nao vier)
    for (i = 0; i < 2000 && atomic_load(&chegaram) < 2; i++) usleep(1000);
  }
  return pthread_mutex_lock(m);
}
static void *fioVazio(void *u) { (void)u; return NULL; }
static int testeCriar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) {
  if (f == montar) {
    int k = atomic_fetch_add(&montaresCriados, 1);
    if (k < 16) __atomic_store_n(&montarEm[k], agoraMs(), __ATOMIC_SEQ_CST);
    return pthread_create(t, a, fioVazio, NULL);
  }
  return pthread_create(t, a, f, arg);
}
static void volta_terminou(void) {   // o que montar() faz no fim, sem montar
  pthread_mutex_lock(&listaTrava); buscando = 0; pthread_mutex_unlock(&listaTrava);
}

static void *corredor(void *u) {
  (void)u;
  ehCorredor = 1;
  desc_iniciar();
  return NULL;
}

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHOU: "); falhas++; } else printf("ok: "); \
                             printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void) {
  pthread_t a, b;
  int n;
  long folga;
  setvbuf(stdout, NULL, _IOLBF, 0);

  // 1. dois desc_iniciar ao mesmo tempo
  atomic_store(&corridaLigada, 1);
  pthread_create(&a, NULL, corredor, NULL);
  pthread_create(&b, NULL, corredor, NULL);
  pthread_join(a, NULL); pthread_join(b, NULL);
  atomic_store(&corridaLigada, 0);
  n = atomic_load(&montaresCriados);
  CONFERE(n == 1, "dois desc_iniciar juntos largam UM montar (largaram %d)", n);
  volta_terminou();

  // 2. agendamento sobrescrito no meio do sono do fio adiado
  usleep((useconds_t)(NV_DESC_MIN_MS + 100) * 1000);   // a volta acima ja nao conta
  atomic_store(&montaresCriados, 0);
  desc_iniciar();                     // volta 1 (t=0)
  volta_terminou();
  usleep(100 * 1000);
  desc_repetir_silencioso();          // agenda A para t=MIN (marca: volta 1)
  usleep(200 * 1000);
  desc_repetir();                     // pedido da pessoa: volta 2 ja (t=300)
  volta_terminou();
  usleep(200 * 1000);
  desc_repetir_silencioso();          // agenda B para t=300+MIN (marca: volta 2)
  usleep((useconds_t)(NV_DESC_MIN_MS + 600) * 1000);
  n = atomic_load(&montaresCriados);
  CONFERE(n == 3, "tres voltas: a 1, a da pessoa e UMA agendada (foram %d)", n);
  if (n >= 3) {
    folga = (long)(__atomic_load_n(&montarEm[2], __ATOMIC_SEQ_CST) - __atomic_load_n(&montarEm[1], __ATOMIC_SEQ_CST));
    CONFERE(folga >= (long)NV_DESC_MIN_MS - 50,
            "a agendada respeita o minimo depois da volta da pessoa (%ld ms, minimo %llu)",
            folga, NV_DESC_MIN_MS);
  }
  printf(falhas ? "desc_iniciar_corrida: %d falha(s)\n" : "desc_iniciar_corrida: ok\n", falhas);
  return falhas ? 1 : 0;
}
