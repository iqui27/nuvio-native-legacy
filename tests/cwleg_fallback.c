// Revisao: OFF precisa valer tambem ao reaproveitar a fileira publicada.
#define CW205_MONTAGEM
#define main original_main
#include "cwordem_desc.c"
#undef main
static int proximoRepro;
int ajustes_cw_proximo(void) { return proximoRepro; }
int main(void) {
  static CatItem v[12];
  agoraMs = (long long)time(NULL) * 1000LL;
  fonteTeste = 2;
  proximoRepro = 1;
  int n = montarContinuar(v, 12);
  assert(n == 5);
  cat_trocar_continuar(v, n);
  proximoRepro = 0;
  traktFalhouTeste = 1;
  n = montarContinuar(v, 12);
  int sugestoes = 0;
  for (int i = 0; i < n; i++) sugestoes += !emAndamento(v[i].progresso);
  printf("REPRO fallback: OFF n=%d sugestoes=%d; esperado sugestoes=0\n", n, sugestoes);
  return !(n == 2 && sugestoes == 0 &&
           !strcmp(v[0].imdb, "tt1") && v[0].progresso == 40 &&
           !strcmp(v[1].imdb, "tt2") && v[1].progresso == 60);
}
