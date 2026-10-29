#include "../src/seekr.c"
#include <assert.h>
// Ajustes nao entram nesta bancada: o relogio de 12 h fica desligado, como em
// tests/guia_logos.c. seekr_horario_local formata com hora_tela() (horafmt.h).
int ajustes_relogio_12h(void) { return 0; }
int main(void) {
  const int estados[] = {SEEKR_DESLIGADO, SEEKR_BUSCANDO, SEEKR_PRONTO, SEEKR_SEM_PREVIA,
    SEEKR_CHAVE_RECUSADA, SEEKR_LIMITE_LOCAL, SEEKR_LIMITE_PROVEDOR, SEEKR_REDE_INDISPONIVEL,
    SEEKR_ARMAZENAMENTO_INDISPONIVEL, SEEKR_RELOGIO_INDISPONIVEL};
  for (size_t i = 0; i < sizeof estados / sizeof estados[0]; i++) {
    const char *texto = seekr_estado_rotulo(estados[i]);
    assert(texto && texto[0] && !strstr(texto, "http") && !strstr(texto, "API-Key"));
    for (size_t j = 0; j < i; j++) assert(strcmp(texto, seekr_estado_rotulo(estados[j])));
  }
  char hora[48];
  setenv("TZ", "America/Sao_Paulo", 1); tzset();
  assert(seekr_horario_local(1735776000LL, hora, sizeof hora)); // 2025-01-02 00:00 UTC
  assert(!strcmp(hora, "01/01 21:00"));
  setenv("TZ", "Asia/Kolkata", 1); tzset();
  assert(seekr_horario_local(1735776000LL, hora, sizeof hora));
  assert(!strcmp(hora, "02/01 05:30"));
  assert(!seekr_horario_local(0, hora, sizeof hora) && !hora[0]);
  assert(!seekr_horario_local(1735776000LL, hora, 1));
  assert(!seekr_horario_local(1735776000LL, NULL, 10));
  puts("seekr states: PASS distinct states, secret-free labels, timezone reset dates and invalid timestamps");
  return 0;
}
