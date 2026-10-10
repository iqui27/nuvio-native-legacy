/* Fio residente da biblioteca falsa da LG: no webOS 3 a libplayerAPIs puxa
 * GObject/GStreamer e outras, que podem subir fios proprios ao carregar. Este
 * imita isso: um fio que roda codigo DESTA .so para sempre. Se o dlclose da
 * sonda descarregar a .so, o fio executa memoria sem mapa e o processo cai.
 * O controle negativo so vale se o fio de fato rodou: sem fio, aborta; na
 * 1a volta ele escreve "[fio] rodou" (antes de o construtor voltar) e o .sh
 * exige essa linha. */
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>
static atomic_int rodou;  /* 1 so DEPOIS de "[fio] rodou" estar escrito */
static void *girar(void *a) {
  static const char m[] = "[fio] rodou\n";
  (void)a;
  if (write(2, m, sizeof m - 1) < 0) abort();
  atomic_store(&rodou, 1);
  for (;;) usleep(1000);
  return 0;
}
__attribute__((constructor)) static void subir(void) {
  pthread_t t;
  if (pthread_create(&t, 0, girar, 0)) abort();
  pthread_detach(t);
  while (!atomic_load(&rodou)) usleep(100);  /* o construtor so volta com o fio vivo */
}
