// Neste episodio: interpreta guest_stars do TMDB e o estado sem chave/rede.
#include "../src/neste.h"
#include <stdio.h>
#include <string.h>

char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "en-US"; }
static int falhas;
#define CK(c) do { if (!(c)) { printf("FAIL %s\n", #c); falhas++; } } while (0)

int main(void) {
  NesteElenco el;
  const char *ok = "{\"cast\":[],\"guest_stars\":[{\"name\":\"Ana Souza\",\"character\":\"Dr. Lima\",\"order\":0},"
    "{\"name\":\"Bob Roe\",\"character\":\"\"},{\"character\":\"x\"}]}";
  CK(neste_interpretar(ok, &el) == 2);
  CK(!strcmp(el.p[0].nome, "Ana Souza") && !strcmp(el.p[0].personagem, "Dr. Lima"));
  CK(!strcmp(el.p[1].nome, "Bob Roe") && !el.p[1].personagem[0]);
  CK(neste_interpretar("{\"guest_stars\":[]}", &el) == 0);
  CK(neste_interpretar("lixo", &el) == 0);
  CK(neste_interpretar(NULL, &el) == 0);
  CK(neste_estado("tt1", 1, 1, &el) == NESTE_NADA);
  neste_pedir("bad", 1, 1);
  CK(neste_estado("bad", 1, 1, &el) == NESTE_NADA);
  if (falhas) return 1;
  printf("neste ok\n");
  return 0;
}
