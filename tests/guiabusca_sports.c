/* Offline published sports feed for the guide search regression. */
#include "../src/esportes.c"
#include <assert.h>

void guiabusca_sports_fixture(void) {
  free(eventos);
  eventos = calloc(1, sizeof *eventos);
  assert(eventos);
  nEventos = 1;
  snprintf(eventos[0].id, sizeof eventos[0].id, "fixture-match");
  snprintf(eventos[0].nome, sizeof eventos[0].nome, "Flamengo vs Palmeiras");
  snprintf(eventos[0].liga, sizeof eventos[0].liga, "Brasileirao");
  eventos[0].nParticipantes = 1;
  snprintf(eventos[0].participantes[0].nome, sizeof eventos[0].participantes[0].nome, "Mengao");
  eventos[0].inicio = time(NULL);
  revisao++;
}
