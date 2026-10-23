// Ordem das fontes do inicio dos creditos e a leitura de capitulo do MKV
// (ChapterTimeStart em ns passa de 32 bits aos 4,29 s).
#include "../src/credfonte.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define main mkv_main_nao_usado
#include "../src/mkv.c"
#undef main
int main(void) {
  CredEntrada e; int f; double s;
  memset(&e, 0, sizeof e); e.dur = 1500;
  assert(cred_escolher(&e, &f) == 0.0 && f == CRED_FONTE_NENHUMA);
  e.aprendidoResto = 100; s = cred_escolher(&e, &f);
  assert(f == CRED_FONTE_APRENDIDO && s == 1400);
  e.vizInicio = 1300; e.vizDur = 1400;          // vizinho: faltavam 100 s... mesmo
  e.vizInicio = 1250; s = cred_escolher(&e, &f);  // faltavam 150 -> 1350
  assert(f == CRED_FONTE_VIZINHO && s == 1350);
  e.introdb = 1346; s = cred_escolher(&e, &f);
  assert(f == CRED_FONTE_INTRODB && s == 1346);
  // AniSkip: mesma prioridade do introdb, mas o rotulo e o da fonte real.
  { CredEntrada a; memset(&a, 0, sizeof a); a.dur = 1500; a.aniskip = 1340;
    s = cred_escolher(&a, &f);
    assert(f == CRED_FONTE_ANISKIP && s == 1340 && !strcmp(cred_fonte_nome(f), "aniskip"));
    assert(!strcmp(cred_fonte_nome(CRED_FONTE_INTRODB), "introdb")); }
  e.capitulo = 1380; s = cred_escolher(&e, &f);
  assert(f == CRED_FONTE_CAPITULO && s == 1380);
  // derivado fora da metade final nao vale
  memset(&e, 0, sizeof e); e.dur = 1500; e.aprendidoResto = 1000;
  assert(cred_escolher(&e, &f) == 0.0);
  // cache de 404
  assert(!cred_404_visto("tt1:1:2", 1, 2, 100));
  cred_404_marcar("tt1", 1, 2, 100);
  assert(cred_404_visto("tt1:1:2", 1, 2, 200) && !cred_404_visto("tt1", 1, 3, 200));
  assert(!cred_404_visto("tt1", 1, 2, 100 + 3600));
  { double a, b; cred_viz_guardar("tt9", 2, 2000, 2200);
    assert(cred_viz_ler("tt9:2:1", 2, &a, &b) && a == 2000 && b == 2200 && !cred_viz_ler("tt9", 3, &a, &b)); }
  // capitulo: 7405 s em ns = 7.405e12 (estoura 32 bits)
  { unsigned char c[8]; uint64_t ns = 7405ULL * 1000000000ULL; int i;
    for (i = 0; i < 8; i++) c[i] = (unsigned char)(ns >> (56 - 8 * i));
    assert((double)lerUint(c, 8) / 1e9 == 7405.0); }
  puts("credfonte: ok");
  return 0;
}
