#ifndef F69_PRODUCTION_SOURCE
#define F69_PRODUCTION_SOURCE "../src/simkl.c"
#endif
#include F69_PRODUCTION_SOURCE
#define main simkl_cw_main
#define atividades fixture_activities
#include "simkl_cw.c"
#undef atividades
#undef main

int main(int argc, char **argv) {
  assert(argc == 2);
  tokenAtual = TOKEN;
  simkl_esquecer();
  if (!strcmp(argv[1], "backoff")) {
    for (int falha = 1; falha <= 3; falha++) {
      char *r = simkl_lista_corpo("shows", "watching");
      assert(r); free(r);
      pthread_mutex_lock(&trava);
      bibConferida = 0;
      pthread_mutex_unlock(&trava);
      atividadesFalham = falha;
      r = simkl_lista_corpo("shows", "watching");
      assert(r); free(r);
      int n = batidas[0];
      r = simkl_lista_corpo("shows", "watching");
      assert(r); free(r);
      assert(batidas[0] == n);
      bibEsperar();
      atividadesFalham = 0;
      __atomic_store_n(&provedorAte, 0, __ATOMIC_RELEASE);
      simkl_esquecer();
      r = simkl_lista_corpo("shows", "watching");
      assert(r); free(r);
      assert(batidas[0] == n + 1);
    }
  } else if (!strcmp(argv[1], "disconnect")) {
    desconectarNoHistorico = 1;
    char *r = simkl_lista_corpo("shows", "watching");
    free(r);
    bibEsperar();
    assert(!bibTexto);
  } else if (!strcmp(argv[1], "memory")) {
    SmkTexto tipos[3] = {{0}};
    for (int i = 0; i < 20000; i++) assert(textoPor(&tipos[0], i ? ",{}" : "{}", i ? 3 : 2));
    char *r = bibliotecaMontar(tipos);
    assert(r);
    free(r);
    assert(textoPor(&tipos[1], "{}", 2));
    assert(!bibliotecaMontar(tipos));
    for (int i = 0; i < 3; i++) free(tipos[i].p);
    char *grande = malloc(8u * 1024u * 1024u + 64);
    assert(grande);
    strcpy(grande, "{\"shows\":[{\"padding\":\"");
    size_t n = strlen(grande);
    memset(grande + n, 'x', 8u * 1024u * 1024u);
    strcpy(grande + n + 8u * 1024u * 1024u, "\"}]}");
    bibPublicar(pedidoCapturar(), grande, strdup("{\"all\":\"1\"}"), 1);
    assert(!bibCorpo);
  } else assert(0);
  bibEsperar();
  simkl_esquecer();
  free(bibTexto);
  bibTexto = NULL;
  puts("simkl_f69: PASS");
  return 0;
}
