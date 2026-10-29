// Title-line rating order: Letterboxd first and it must survive a late IMDb/RT.
#include <assert.h>
#include "../src/notasfontes.c"

int main(void) {
  int pos, n, i;
  unsigned char manter[EX_NFONTES];
  float larg[3] = { 100, 100, 100 };
  int prio[3];
  int fonte[3];
  // Default order untouched.
  for (pos = 0; pos < EX_NFONTES; pos++) assert(nf_na_posicao_ord(pos, 0) == nf_na_posicao(pos));
  // Letterboxd first, the rest in the usual order, no repeats, none lost.
  assert(nf_na_posicao_ord(0, 1) == EX_LETTERBOXD);
  assert(nf_na_posicao_ord(1, 1) == nf_na_posicao(0));
  assert(nf_na_posicao_ord(7, 1) == nf_na_posicao(6));
  assert(nf_na_posicao_ord(8, 1) == nf_na_posicao(8));
  { int vis[EX_NFONTES] = {0};
    for (pos = 0; pos < EX_NFONTES; pos++) vis[nf_na_posicao_ord(pos, 1)]++;
    for (i = 0; i < EX_NFONTES; i++) assert(vis[i] == 1); }
  // Late load: Letterboxd is there, then IMDb + RT arrive and only 2 fit.
  for (n = 0; n < 2; n++) {
    int ord = n;
    fonte[0] = EX_LETTERBOXD; fonte[1] = EX_IMDB; fonte[2] = EX_TOMATOES;
    for (i = 0; i < 3; i++) prio[i] = nf_prioridade_ord(fonte[i], ord);
    nf_encaixar(larg, prio, 3, 250.0f, manter);
    assert(manter[0]);               // Letterboxd survives in either order
  }
  return 0;
}
