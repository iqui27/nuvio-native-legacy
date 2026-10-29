// "Arquivos desta fonte" e o filme em partes (src/partes.h), so a parte pura:
// achar o item da fonte no mylist do TorBox, a ordem, os extras, a parte
// seguinte e a posicao no filme inteiro.
#include "partes.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Rede, disco e TorBox ficam fora deste teste.
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 0; }
char *dados_ler(const char *n) { (void)n; return NULL; }
unsigned debrid_tb_geracao(void) { return 1; }
int debrid_tb_disponivel(void) { return 0; }
char *debrid_tb_mylist(int u, int id, int p, int f, int *st) { (void)u; (void)id; (void)p; (void)f; if (st) *st = 0; return NULL; }
int debrid_tb_link(int u, int id, int a, char *url, unsigned n) { (void)u; (void)id; (void)a; (void)url; (void)n; return 0; }
int debrid_tb_adicionar(const char *h) { (void)h; return 0; }
int debrid_tb_airlock(int u, int id, int l, char *det, unsigned n) { (void)u; (void)id; (void)l; (void)det; (void)n; return 0; }
char *rede_baixar_trecho64_final(const char *url, const char *h, int64_t a, int64_t b, long *s,
                                 int64_t *t, int *st, volatile int *c, char *f, int *x) {
  (void)url; (void)h; (void)a; (void)b; (void)s; (void)t; (void)st; (void)c; (void)f; (void)x;
  return NULL;
}

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); falhas += !c; }

#define H1 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1"
#define H2 "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb2"
static const char *corpo =
  "{\"success\":true,\"data\":["
  "{\"id\":7,\"hash\":\"" H1 "\",\"name\":\"Kaguya-sama First Kiss [Grp]\",\"files\":["
  "{\"id\":10,\"name\":\"K/Kaguya-sama First Kiss - 03.mkv\",\"short_name\":\"Kaguya-sama First Kiss - 03.mkv\",\"size\":300000000},"
  "{\"id\":11,\"name\":\"K/Kaguya-sama First Kiss - 01.mkv\",\"short_name\":\"Kaguya-sama First Kiss - 01.mkv\",\"size\":300000000},"
  "{\"id\":12,\"name\":\"K/Kaguya-sama First Kiss - NCOP.mkv\",\"short_name\":\"Kaguya-sama First Kiss - NCOP.mkv\",\"size\":20000000},"
  "{\"id\":13,\"name\":\"K/Kaguya-sama First Kiss - 02.mkv\",\"short_name\":\"Kaguya-sama First Kiss - 02.mkv\",\"size\":300000000}]},"
  "{\"id\":8,\"hash\":\"" H2 "\",\"name\":\"Other Show [Other]\",\"files\":["
  "{\"id\":20,\"name\":\"O/Other Show - 01.mkv\",\"short_name\":\"Other Show - 01.mkv\",\"size\":500000000}]}"
  "]}";

int main(void) {
  static PartesLista l;
  char hs[2][41];
  int n = partes_hashes_texto("magnet:?xt=urn:btih:" H1 "&dn=x", hs, 2);
  ok(n == 1 && !strcmp(hs[0], H1), "40-hex hash read from the add-on link");

  partes_lista_limpar(&l);
  ok(partes_achar(corpo, 0, hs, 1, 1, "", 0, &l) == 1 && l.item == 7, "hash finds the source's item");
  ok(l.n == 4, "all four videos listed");
  ok(l.airlock == 0, "item without \"airlocked\" is not in the Airlock");
  { static const char *air = "{\"id\":7,\"airlocked\":true,\"files\":[{\"id\":10,\"name\":\"K/Kaguya-sama First Kiss - 03.mkv\",\"size\":300000000}]}";
    static const char *nao = "{\"id\":7,\"airlocked\":false,\"files\":[{\"id\":10,\"name\":\"K/Kaguya-sama First Kiss - 03.mkv\",\"size\":300000000}]}";
    PartesLista a = {0};
    ok(partes_ler_item(air, NULL, 0, &a) && a.airlock == 1, "\"airlocked\":true reads as Airlock on");
    ok(partes_ler_item(nao, NULL, 0, &a) && a.airlock == 0, "\"airlocked\":false reads as Airlock off");
    const char *flags[] = {"1", "\"true\"", "0", "\"false\"", "10"};
    for (int i = 0; i < 5; i++) {
      char item[160];
      snprintf(item, sizeof item, "{\"id\":7,\"airlocked\":%s,\"files\":[{\"id\":10,\"name\":\"part.mkv\",\"size\":300000000}]}", flags[i]);
      ok(partes_ler_item(item, NULL, 0, &a) && a.airlock == (i < 2), "Airlock accepts numeric/string flags without treating 10 as true");
    }
    partes_lista_limpar(&a); }
  ok(l.n == 4 && strstr(l.arq[0].nome, "01") && strstr(l.arq[1].nome, "02") && strstr(l.arq[2].nome, "03"),
     "parts in natural order (01, 02, 03)");
  ok(l.n == 4 && l.arq[3].extra && strstr(l.arq[3].nome, "NCOP"), "NCOP is an extra, after the parts");
  ok(partes_principais(&l) == 3, "three real parts");
  ok(partes_seguinte(&l, 0) == 1 && partes_seguinte(&l, 2) == -1, "next part after 1 is 2; none after 3");
  ok(partes_numero(&l, 1) == 2 && partes_numero(&l, 3) == 0, "part numbers skip the extra");

  { double vp = 0, vd = 0;
    ok(partes_conta_virtual(&l, 1, 600, 1200, 5760, &vp, &vd) && fabs(vd - 5760) < 1 && fabs(vp - 2880) < 1,
       "half of part 2 is the movie's middle");
    ok(!partes_conta_virtual(&l, 3, 10, 60, 5760, &vp, &vd), "an extra has no movie position"); }

  partes_lista_limpar(&l);
  ok(partes_achar(corpo, 0, hs, 0, 0, "Kaguya-sama First Kiss - 02.mkv", 300000000, &l) == 1 && l.item == 7 &&
     l.atual >= 0 && strstr(l.arq[l.atual].nome, "02"), "file name finds the item and the playing file");

  { char outro[1][41] = { "cccccccccccccccccccccccccccccccccccccc3" "c" };
    partes_lista_limpar(&l);
    ok(partes_achar(corpo, 0, outro, 1, 1, "Kaguya-sama First Kiss - 02.mkv", 300000000, &l) != 1,
       "an unknown explicit hash never falls back to another release's filename/size"); }

  { char pack[65536];
    size_t used = (size_t)snprintf(pack, sizeof pack, "{\"data\":[{\"id\":99,\"files\":[");
    for (int i = 240; i > 0; i--)
      used += (size_t)snprintf(pack + used, sizeof pack - used,
        "{\"id\":%d,\"name\":\"Pack/Show - %03d.mkv\",\"size\":300000000},", i, i);
    snprintf(pack + used, sizeof pack - used,
      "{\"id\":241,\"name\":\"Pack/OVA 1.mkv\",\"size\":300000000},"
      "{\"id\":242,\"name\":\"Pack/NCOP.mkv\",\"size\":10000000}]}]}");
    ok(partes_achar(pack, 0, hs, 0, 0, "Show - 001.mkv", 0, &l), "large pack found");
    ok(l.n == 242, "all 240 episodes and trailing special/extra survive parsing");
    ok(l.atual >= 0 && l.arq[l.atual].id == 1, "current file beyond input entry 160 identified");
    ok(l.n == 242 && l.arq[0].id == 1 && l.arq[239].id == 240 &&
       l.arq[240].tipo == PARTE_ESPECIAL && l.arq[241].tipo == PARTE_EXTRA,
       "full pack naturally sorted and trailing tabs retained");
  }

  { PartesLista copy = {0};
    ok(partes_lista_copiar(&copy, &l), "large list copied");
    partes_lista_limpar(&l);
    ok(copy.n == 242 && copy.arq[241].id == 242, "copy survives source replacement");
    partes_lista_limpar(&copy);
  }

  printf(falhas ? "partes: %d FAIL\n" : "partes: ok\n", falhas);
  return falhas != 0;
}
