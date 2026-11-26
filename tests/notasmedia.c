// Media por temporada do cartao "Notas por episodio" (src/notasmedia.c).
#include "../src/notasmedia.h"
#include <stdio.h>

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %s %s\n", ok ? "ok    " : "FALHOU", o_que);
  if (!ok) falhas++;
}

int main(void) {
  int k = -1, m, p;
  { int a[] = { 80, 82, 85 };
    confere("media simples: 8,2", nm_media(a, 3, &k) == 82 && k == 3); }
  { int a[] = { 80, 81 };
    confere("meio decimo arredonda para cima: 8,05 -> 8,1", nm_media(a, 2, NULL) == 81); }
  { int a[] = { 90, 0, 70, 0 };
    confere("episodio sem nota nao entra", nm_media(a, 4, &k) == 80 && k == 2); }
  { int a[] = { 0, 91, 0 };
    confere("um episodio com nota: sem media", nm_media(a, 3, &k) == 0 && k == 1); }
  { int a[] = { 0, 0 };
    confere("temporada sem nota: sem media", nm_media(a, 2, &k) == 0 && k == 0); }
  confere("lista vazia ou nula", nm_media(NULL, 5, &k) == 0 && k == 0 && nm_media(NULL, 0, NULL) == 0);
  { static int a[2000]; int i;
    for (i = 0; i < 2000; i++) a[i] = 100;
    confere("temporada enorme nao estoura", nm_media(a, 2000, NULL) == 100); }

  { int md[] = { 78, 0, 86, 71, 86 };
    nm_extremos(md, 5, &m, &p);
    confere("melhor e pior; empate fica com a primeira; sem media e pulada", m == 2 && p == 3); }
  { int md[] = { 80, 0, 0 };
    nm_extremos(md, 3, &m, &p);
    confere("uma temporada so com media: nada a comparar", m == -1 && p == -1); }
  { int md[] = { 80, 80, 80 };
    nm_extremos(md, 3, &m, &p);
    confere("todas iguais: nada a comparar", m == -1 && p == -1); }
  nm_extremos(NULL, 0, &m, &p);
  confere("sem temporadas", m == -1 && p == -1);
  return falhas != 0;
}
