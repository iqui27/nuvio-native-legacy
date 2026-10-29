// Filtro da Live TV: qualidade (Todas / So 4K / Preferir 4K), categorias e
// canais escondidos, por perfil. Ver src/guiafiltro.h.
#include "guiafiltro.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)

typedef struct { const char *grupo, *id, *nome; } Ent;
typedef struct { char id[32]; char nome[48]; int grupo; } Ret;

// O mesmo contrato de sCanalAdd (guia.c): avalia ANTES do teto, so conta vaga
// quem entra de verdade, e quem perde para uma variante melhor vira reserva.
static int carrega(const GuiaFiltro *f, const Ent *e, int n, int teto, Ret *out, int *nAlt) {
  GuiaFiltroDup d = { 0 };
  int nr = 0, i;
  *nAlt = 0;
  for (i = 0; i < n; i++) {
    int velho = -1, r = guiafiltro_avaliar(f, &d, 0, e[i].grupo, e[i].id, e[i].nome, "", &velho);
    if (r == GF_FORA_DUP) { (*nAlt)++; continue; }
    if (r == GF_FORA) continue;
    if (r == GF_TROCA) {
      snprintf(out[velho].id, sizeof out[velho].id, "%s", e[i].id);
      snprintf(out[velho].nome, sizeof out[velho].nome, "%s", e[i].nome);
      (*nAlt)++;
      continue;
    }
    if (nr >= teto) continue;
    snprintf(out[nr].id, sizeof out[nr].id, "%s", e[i].id);
    snprintf(out[nr].nome, sizeof out[nr].nome, "%s", e[i].nome);
    guiafiltro_dup_registrar(f, &d, 0, e[i].grupo, e[i].nome, "", nr);
    nr++;
  }
  guiafiltro_dup_zerar(&d);
  return nr;
}
static int tem(const Ret *r, int n, const char *nome) {
  for (int i = 0; i < n; i++) if (!strcmp(r[i].nome, nome)) return 1;
  return 0;
}

int main(void) {
  static GuiaFiltro f, g;
  Ret r[16];
  char buf[4096], nome[48];
  int n, alt;
  const Ent lista[] = {
    { "Esportes", "a1", "X 4K" }, { "Esportes", "a2", "X FHD" }, { "Esportes", "a3", "X HD" },
    { "Esportes", "b1", "Y FHD" }, { "Esportes", "b2", "Y SD" },
  };
  memset(&f, 0, sizeof f);
  // Todas as versoes: como era.
  n = carrega(&f, lista, 5, 99, r, &alt);
  CHECA(n == 5 && alt == 0);
  // So 4K: so quem declara 4K.
  guiafiltro_modo_definir(&f, GF_SO_4K);
  n = carrega(&f, lista, 5, 99, r, &alt);
  CHECA(n == 1 && !strcmp(r[0].nome, "X 4K"));
  // Preferir 4K: um por canal, a maior resolucao quando nao ha 4K.
  guiafiltro_modo_definir(&f, GF_PREFERIR_4K);
  n = carrega(&f, lista, 5, 99, r, &alt);
  CHECA(n == 2 && !strcmp(r[0].nome, "X 4K") && !strcmp(r[1].nome, "Y FHD") && alt == 3);
  // Only 4K preserves declared variants; source quality also counts.
  { const Ent uhd[] = { { "G", "1", "X UHD" }, { "G", "2", "X 4K HEVC" } };
    guiafiltro_modo_definir(&f, GF_SO_4K);
    CHECA(carrega(&f, uhd, 2, 99, r, &alt) == 2);
    GuiaFiltroDup d = {0}; int idx = -1;
    CHECA(guiafiltro_avaliar(&f, &d, 0, "G", "1", "X", "Source UHD", &idx) == GF_ENTRA);
    guiafiltro_modo_definir(&f, GF_PREFERIR_4K);
  }
  // A melhor chega DEPOIS da pior: troca no lugar, sem gastar vaga.
  { const Ent inv[] = { { "G", "1", "Z SD" }, { "G", "2", "Z HD" }, { "G", "3", "Z 4K" } };
    n = carrega(&f, inv, 3, 99, r, &alt);
    CHECA(n == 1 && !strcmp(r[0].nome, "Z 4K") && alt == 2); }
  // Resolution categories share one channel within a source.
  { const Ent dois[] = { { "A", "1", "Q HD" }, { "B", "2", "Q FHD" } };
    n = carrega(&f, dois, 2, 99, r, &alt);
    CHECA(n == 1 && alt == 1 && !strcmp(r[0].nome, "Q FHD")); }
  // Categoria escondida sai ANTES do teto: nao gasta vaga.
  guiafiltro_modo_definir(&f, GF_TODAS);
  { const Ent c[] = { { "Adulto", "1", "P1" }, { "Adulto", "2", "P2" }, { "Adulto", "3", "P3" },
                      { "Filmes", "4", "F1" }, { "Filmes", "5", "F2" } };
    n = carrega(&f, c, 5, 2, r, &alt);
    CHECA(n == 2 && tem(r, n, "P1") && !tem(r, n, "F1"));   // sem filtro, o teto de 2 e todo de Adulto
    CHECA(guiafiltro_cat_alternar(&f, "Adulto") == 1);
    n = carrega(&f, c, 5, 2, r, &alt);
    CHECA(n == 2 && tem(r, n, "F1") && tem(r, n, "F2") && !tem(r, n, "P1"));
    CHECA(guiafiltro_cat_oculta(&f, "Adulto") && !guiafiltro_cat_oculta(&f, "Filmes")); }
  // Canal escondido: tambem antes do teto.
  CHECA(guiafiltro_canal_alternar(&f, "4", "F1", "Filmes") == 1);
  { const Ent c[] = { { "Filmes", "4", "F1" }, { "Filmes", "5", "F2" }, { "Filmes", "6", "F3" } };
    n = carrega(&f, c, 3, 2, r, &alt);
    CHECA(n == 2 && !tem(r, n, "F1") && tem(r, n, "F2") && tem(r, n, "F3")); }
  // Ida e volta pelo texto do arquivo.
  guiafiltro_modo_definir(&f, GF_PREFERIR_4K);
  CHECA(guiafiltro_texto(&f, buf, sizeof buf) > 0);
  memset(&g, 0, sizeof g);
  guiafiltro_ler(&g, buf);
  CHECA(g.modo == GF_PREFERIR_4K && guiafiltro_cat_oculta(&g, "Adulto") && guiafiltro_canal_oculto(&g, "4"));
  CHECA(g.nCan == 1 && !strcmp(g.can[0].nome, "F1") && !strcmp(g.can[0].cat, "Filmes"));
  // Mostrar de novo; lixo no arquivo nao quebra; modo invalido volta a Todas.
  CHECA(guiafiltro_cat_alternar(&g, "Adulto") == 0 && !guiafiltro_cat_oculta(&g, "Adulto"));
  guiafiltro_ler(&g, "Q9\n\n??\nCFilmes\nCFilmes\n");
  CHECA(g.modo == GF_TODAS && g.nCat == 1);
  // Arquivo por perfil.
  CHECA(!strcmp(guiafiltro_arquivo(2, nome, sizeof nome), "guia-filtro-p2.txt"));
  // Legacy IDs apply to every source until an explicit source rule exists.
  guiafiltro_ler(&g, "Htv:news\tNews 4K\tNews\n");
  CHECA(guiafiltro_canal_oculto_fonte(&g, "a", "tv:news"));
  CHECA(guiafiltro_canal_oculto_fonte(&g, "b", "tv:news"));
  CHECA(!guiafiltro_canal_alternar_fonte(&g, "a", "tv:news", "News 4K", "News"));
  CHECA(!guiafiltro_canal_oculto_fonte(&g, "a", "tv:news"));
  CHECA(!guiafiltro_canal_oculto_fonte(&g, "b", "tv:news"));
  CHECA(guiafiltro_canal_alternar_fonte(&g, "a", "tv:news", "News 4K", "News"));
  CHECA(guiafiltro_texto(&g, buf, sizeof buf) > 0 && !strstr(buf, "Htv:news") && !strstr(buf, "S*\t"));
  guiafiltro_ler(&g, buf);
  CHECA(g.nCan == 1 && guiafiltro_canal_oculto_fonte(&g, "a", "tv:news"));
  CHECA(!guiafiltro_canal_oculto_fonte(&g, "b", "tv:news"));
  CHECA(!guiafiltro_canal_alternar_fonte(&g, "a", "tv:news", "News 4K", "News"));
  CHECA(guiafiltro_texto(&g, buf, sizeof buf) > 0);
  guiafiltro_ler(&g, buf);
  CHECA(!guiafiltro_canal_oculto_fonte(&g, "a", "tv:news"));
  guiafiltro_ler(&g, "Htv:news\tNews 4K\tNews\n");
  CHECA(guiafiltro_texto(&g, buf, sizeof buf) > 0);
  guiafiltro_ler(&g, buf);
  CHECA(guiafiltro_canal_oculto_fonte(&g, "b", "tv:news"));
  // Teto das listas: nao estoura.
  memset(&g, 0, sizeof g);
  for (int i = 0; i < GF_CATS_MAX + 5; i++) { char c[32]; snprintf(c, sizeof c, "C%d", i); guiafiltro_cat_alternar(&g, c); }
  CHECA(g.nCat == GF_CATS_MAX);
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("guiafiltro: tudo ok\n");
  return 0;
}
