// CORRIDA NA TROCA DO MOTOR ASS (revisao PR734CD-r1, bloqueio 1): o leitor de
// producao — ass_novo_ligado, o que a thread de render chama a cada lote —
// contra a ESCRITA da troca (definirValorDireto, a confirmacao dos Ajustes em
// src/ajustes.c). A leitura ordinaria de valor[AJ_ASS_MOTOR] corria com a
// gravacao da confirmacao (TSan 66 em lig, src/ajustes.c). Agora a thread de
// render le um espelho publicado com store/load atomicos.
//
// Sob clang -fsanitize=thread: rc 66 em b3a9a7108, rc 0 na ponta. Sem
// sanitizer o binario so verifica que o leitor ve 0/1 e o estado final bate.
//
//   bash tests/assmotor_corrida.sh
#include "../src/ajustes.c"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_barrier_t partida;
static int parar;

static void *worker(void *arg) {
  (void)arg;
  long lidas = 0, ruins = 0;
  pthread_barrier_wait(&partida);
  while (!__atomic_load_n(&parar, __ATOMIC_RELAXED)) {
    int v = ass_novo_ligado();
    lidas++;
    if (v != 0 && v != 1) ruins++;
  }
  printf("leitor da thread de render: %ld leituras, %ld fora de {0,1}\n", lidas, ruins);
  return (void *)ruins;
}

int main(void) {
  char dir[256];
  pthread_t fio;
  void *ret = NULL;
  snprintf(dir, sizeof dir, "%s/nuvio-assmotor-corrida-XXXXXX",
           getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);
  assert(ass_novo_ligado() == 1);

  assert(!pthread_barrier_init(&partida, NULL, 2));
  assert(!pthread_create(&fio, NULL, worker, NULL));
  pthread_barrier_wait(&partida);
  for (int i = 0; i < 400; i++) {
    assert(definirValorDireto(AJ_ASS_MOTOR, i & 1));
    assert(ass_novo_ligado() == !(i & 1));   // a escrita da troca publica o espelho
  }
  __atomic_store_n(&parar, 1, __ATOMIC_RELAXED);
  pthread_join(fio, &ret);
  pthread_barrier_destroy(&partida);
  assert(ret == 0);

  /* O arquivo persistiu a ultima troca e a releitura alimenta o mesmo espelho. */
  { char *t = dados_ler("ajustes.txt");
    assert(t && strstr(t, "assMotorNovoLocal"));
    free(t); }
  ajustes_dir(dir);
  assert(ass_novo_ligado() == ajustes_ass_motor());

  printf("assmotor_corrida: ok\n");
  return 0;
}
