// Objeto 3D e tinta de cada conquista: familia/chave -> arquivo e acento.
#include <stdio.h>
#include <string.h>
#include "conquistasarte.h"
#include "conquistasregra.h"
static int falhas;
#define EQ(a, b) do { if (strcmp((a), (b))) { printf("FAIL %s:%d '%s' != '%s'\n", __FILE__, __LINE__, (a), (b)); falhas++; } } while (0)
#define OK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)
static void arq(const char *k, int f, int n, const char *esp) { char b[64]; OK(cqarte_arquivo(k, f, n, b, sizeof b)); EQ(b, esp); }
int main(void) {
  arq("dir:nolan", CQ_FAM_DIRETOR, 4, "director-n-gold.png");
  arq("dir:kubrick", CQ_FAM_DIRETOR, 2, "director-k-silver.png");
  arq("dir:varda", CQ_FAM_DIRETOR, 1, "director-bronze.png");
  arq("trilha:ghibli", CQ_FAM_TRILHA, 3, "studio-silver.png");
  arq("trilha:cult-80s", CQ_FAM_TRILHA, 0, "decade-locked.png");
  arq("trilha:noir", CQ_FAM_TRILHA, 5, "noir-gold.png");
  arq("trilha:oscar-filme-internacional", CQ_FAM_TRILHA, 2, "international-silver.png");
  arq("trilha:outra", CQ_FAM_TRILHA, 1, "trail-bronze.png");
  arq("top:250", CQ_FAM_EXPLORADOR, 4, "top250-gold.png");
  arq("ator:tilda", CQ_FAM_PREMIO, 2, "actor-silver.png");
  arq("x", CQ_FAM_PREMIO, 1, "award-bronze.png");
  arq("x", CQ_FAM_COMUNIDADE, 1, "community-bronze.png");
  arq("x", CQ_FAM_TEMPORADA, 1, "season-bronze.png");
  arq("x", CQ_FAM_EXPLORADOR, 1, "explorer-bronze.png");
  CqTinta g = cqarte_tinta("trilha:noir", CQ_FAM_TRILHA, 0), a = cqarte_tinta("trilha:ghibli", CQ_FAM_TRILHA, 1);
  CqTinta b = cqarte_tinta("trilha:noir", CQ_FAM_TRILHA, 1), h = cqarte_tinta("trilha:cult-80s", CQ_FAM_TRILHA, 1);
  OK(g.r == 150 && g.g == 156 && g.b == 168);                 // locked = cinza neutro
  OK(a.r == 86 && a.g == 190 && a.b == 170);                  // studio
  OK(a.r != b.r || a.g != b.g || a.b != b.b);                 // acento por objeto
  OK(h.r != a.r || h.g != a.g);
  printf(falhas ? "conquistas_arte: %d falhas\n" : "conquistas_arte ok\n", falhas);
  return falhas != 0;
}
