// Focused OSD text contract; real player helper and Seekr labels/local time.
#include "../src/player.c"
#include <assert.h>
const char *i18n(const char *key) { return key; }
// Ajustes nao entra nesta bancada (so seekr.c + player.c): relogio de 12 h
// desligado, como em tests/guia_logos.c. seekr_horario_local usa hora_tela().
int ajustes_relogio_12h(void) { return 0; }
int main(void) {
  SeekrUso u = {.usadas=50, .limite=50, .persistente=1,
                .reinicioUtc=1735776000LL, .retryUtc=1735779600LL};
  char l[3][192];
  setenv("TZ", "America/Sao_Paulo", 1); tzset();
  assert(!seekrStatusLinhas(SEEKR_PRONTO, &u, 1, l));
  assert(!seekrStatusLinhas(SEEKR_BUSCANDO, &u, 1, l));
  assert(!seekrStatusLinhas(SEEKR_DESLIGADO, &u, 1, l));
  assert(seekrStatusLinhas(SEEKR_DESLIGADO, &u, 0, l) == 1 && strstr(l[0], "sem chave"));
  assert(seekrStatusLinhas(SEEKR_LIMITE_LOCAL, &u, 1, l) == 3);
  assert(strstr(l[0], "TV") && !strstr(l[0], "temporário"));
  assert(!strcmp(l[1], "50 de 50 consultas hoje (UTC)"));
  assert(strstr(l[2], "01/01 21:00") && strstr(l[2], "hora local"));
  assert(seekrStatusLinhas(SEEKR_LIMITE_PROVEDOR, &u, 1, l) == 2);
  assert(strstr(l[0], "temporário") && strstr(l[1], "01/01 22:00"));
  u.retryUtc = 0;
  assert(seekrStatusLinhas(SEEKR_LIMITE_PROVEDOR, &u, 1, l) == 1);
  u.relogioAtrasado = 1;
  u.retryUtc = 1735779600LL;
  assert(seekrStatusLinhas(SEEKR_LIMITE_PROVEDOR, &u, 1, l) == 2);
  assert(strstr(l[1], "data e a hora") && !strstr(l[1], "Tente após"));
  assert(seekrStatusLinhas(SEEKR_LIMITE_LOCAL, &u, 1, l) == 3);
  assert(strstr(l[2], "data e a hora") && !strstr(l[2], "Renova"));
  u.persistente = 0;
  assert(seekrStatusLinhas(SEEKR_LIMITE_LOCAL, &u, 1, l) == 1);
  assert(strstr(l[0], "guardar o uso") && !strstr(l[0], "atingido"));
  const int states[] = {SEEKR_SEM_PREVIA, SEEKR_CHAVE_RECUSADA,
    SEEKR_REDE_INDISPONIVEL, SEEKR_ARMAZENAMENTO_INDISPONIVEL, SEEKR_RELOGIO_INDISPONIVEL};
  for (size_t i=0; i<sizeof states/sizeof states[0]; i++) {
    assert(seekrStatusLinhas(states[i], &u, 1, l) == 1);
    assert(!strcmp(l[0], seekr_estado_rotulo(states[i])));
    assert(!strstr(l[0], "http") && !strstr(l[0], "API-Key") && !strstr(l[0], "Bearer"));
  }
  assert(!seekrStatusLinhas(99, &u, 1, l));
  puts("player Seekr status: PASS causes, UTC usage, local retry/reset, clock/storage and safe labels");
}
