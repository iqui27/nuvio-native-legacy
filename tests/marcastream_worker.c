// Worker real, rede e relogio controlados: recuperacao sem reiniciar o app.
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static atomic_long agora = 100;
static time_t relogio(time_t *t) {
  time_t v = atomic_load(&agora);
  if (t) *t = v;
  return v;
}
#define time relogio
#include "../src/marcastream_logo.c"
#undef time
static const char *chave = "";
static atomic_int pedidos;
static int rede_ok;
const char *desc_chave_tmdb(void) { return chave; }
char *rede_baixar(const char *url, int segundos) {
  assert(strstr(url, "/network/213/images?api_key=key"));
  assert(segundos == 20);
  atomic_fetch_add(&pedidos, 1);
  return rede_ok ? strdup("{\"logos\":[{\"file_path\":\"/logo.svg\"},{\"file_path\":\"/logo.png\"}]}") : NULL;
}
static void esperar(void) {
  struct timespec pausa = {0, 1000000};
  for (int i = 0; i < 2000; i++) {
    if (atomic_load(&slot[0].estado) != 1) return;
    nanosleep(&pausa, NULL);
  }
  assert(!"worker nao terminou");
}
int main(int argc, char **argv) {
  const MarcaStream *m = marca_stream("Netflix");
  assert(argc == 2);
  if (!strcmp(argv[1], "config")) {
    assert(!marca_stream_logo(m)[0]); esperar();
    assert(atomic_load(&pedidos) == 0);
    chave = "key"; rede_ok = 1;
    marca_stream_logo(m); esperar();
  } else {
    chave = "key";
    marca_stream_logo(m); esperar();
    assert(atomic_load(&pedidos) == 1);
    if (!strcmp(argv[1], "key")) {
      chave = "key-new"; rede_ok = 1;
      marca_stream_logo(m); esperar();
      assert(atomic_load(&pedidos) == 2);
    } else {
    rede_ok = 1;
    for (int i = 0; i < 100; i++) assert(!marca_stream_logo(m)[0]);
    assert(atomic_load(&pedidos) == 1);
    atomic_store(&agora, 100 + MS_RETRY_S - 1);
    assert(!marca_stream_logo(m)[0]);
    assert(atomic_load(&pedidos) == 1);
    atomic_store(&agora, 100 + MS_RETRY_S);
    marca_stream_logo(m); esperar();
    }
  }
  assert(!strcmp(marca_stream_logo(m), "https://image.tmdb.org/t/p/w500/logo.png"));
  int n = atomic_load(&pedidos);
  marca_stream_logo(m); assert(atomic_load(&pedidos) == n);
  printf("ok  marcastream worker %s\n", argv[1]);
}
