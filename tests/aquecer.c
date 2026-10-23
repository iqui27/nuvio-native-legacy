// AQUECER CONEXOES (2.0.2): o planejamento, sem rede. O motor e trocado por um
// falso que so anota as origens: o que vai para ele nunca leva caminho, query
// nem credencial (e por isso nao resolve link nem cria arquivo no debrid).
//
//   bash tests/aquecer.sh
#include "aquecer.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

static char vistas[8][96];
static int nVistas, bloquear, dentro;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int motor(const char *const *o, int n, unsigned *ms) {
  int i;
  pthread_mutex_lock(&m);
  dentro = 1;
  for (i = 0; i < n && nVistas < 8; i++) snprintf(vistas[nVistas++], 96, "%s", o[i]);
  pthread_mutex_unlock(&m);
  while (bloquear) usleep(2000);
  for (i = 0; i < n; i++) ms[i] = 12;
  return n;
}

int main(void) {
  char o[16][96], b[96];
  const char *u[8];
  int q, i;

  CONFERE(aquecer_origem("https://a.exemplo.com:8443/resolve/x?k=1#f", b, sizeof b) && !strcmp(b, "https://a.exemplo.com:8443"), "origem com porta (%s)", b);
  CONFERE(aquecer_origem("http://h/", b, sizeof b) && !strcmp(b, "http://h"), "http sem caminho");
  CONFERE(aquecer_origem("https://h?x=1", b, sizeof b) && !strcmp(b, "https://h"), "query sem barra");
  CONFERE(!aquecer_origem("ftp://h/x", b, sizeof b) && !b[0], "so http e https");
  CONFERE(!aquecer_origem("https:///x", b, sizeof b), "host vazio");
  CONFERE(!aquecer_origem("https://user:senha@h/x", b, sizeof b) && !b[0], "credencial embutida nao vira origem");
  CONFERE(!aquecer_origem("/relativo", b, sizeof b) && !aquecer_origem(NULL, b, sizeof b), "nao-URL");

  // Dedup no lote, e o teto.
  u[0] = "https://a/x"; u[1] = "https://a/y"; u[2] = "https://b/x"; u[3] = "https://c/x";
  u[4] = "https://d/x"; u[5] = "https://e/x"; u[6] = "https://f/x"; u[7] = "ftp://g/x";
  q = aquecer_planejar(u, 8, 1000, o);
  CONFERE(q == AQUECER_LOTE_MAX, "teto do lote (%d)", q);
  CONFERE(!strcmp(o[0], "https://a") && !strcmp(o[1], "https://b") && !strcmp(o[2], "https://c") && !strcmp(o[3], "https://d"),
          "dedup e ordem: %s %s %s %s", o[0], o[1], o[2], o[3]);

  // A mesma origem nao e aquecida de novo antes do prazo; depois, sim.
  q = aquecer_planejar(u, 4, 1000 + AQUECER_REPETIR_MS - 1, o);
  CONFERE(q == 0, "dentro do prazo nada a aquecer (%d)", q);
  q = aquecer_planejar(u, 1, 1000 + AQUECER_REPETIR_MS, o);
  CONFERE(q == 1 && !strcmp(o[0], "https://a"), "passado o prazo aquece de novo (%d)", q);
  // Origem nova entra mesmo com as outras ainda quentes.
  u[0] = "https://novo/x";
  q = aquecer_planejar(u, 1, 1000 + AQUECER_REPETIR_MS + 5, o);
  CONFERE(q == 1 && !strcmp(o[0], "https://novo"), "origem nova entra");

  // Fio: o motor so recebe origens, um pedido por vez.
  aquecer_definir(motor);
  bloquear = 1;
  u[0] = "https://zz1.exemplo/resolve/segredo?token=abc";
  u[1] = "https://zz2.exemplo/";
  CONFERE(aquecer_pedir(u, 2) == 2, "pedido aceito");
  for (i = 0; i < 200 && !dentro; i++) usleep(2000);
  CONFERE(dentro, "o fio chamou o motor");
  u[0] = "https://zz3.exemplo/x";
  CONFERE(aquecer_pedir(u, 1) == 0, "com um em curso o outro e ignorado, nao enfileira");
  bloquear = 0;
  for (i = 0; i < 200; i++) { usleep(5000); u[0] = "https://zz4.exemplo/x"; if (aquecer_pedir(u, 1) > 0) break; }
  CONFERE(i < 200, "terminado o primeiro, um novo pedido passa");
  usleep(50000);
  for (i = 0; i < nVistas; i++)
    CONFERE(!strchr(vistas[i] + 8, '/') && !strchr(vistas[i], '?') && !strstr(vistas[i], "segredo") && !strstr(vistas[i], "token"),
            "origem limpa: %s", vistas[i]);
  CONFERE(nVistas >= 3 && !strcmp(vistas[0], "https://zz1.exemplo") && !strcmp(vistas[1], "https://zz2.exemplo"), "origens na ordem");

  if (falhas) { printf("aquecer: %d falha(s)\n", falhas); return 1; }
  printf("aquecer: ok\n");
  return 0;
}
