// #762: as listas do MDBList chegam por /lists/user e entram como uma fonte
// propria da Biblioteca (LST_MDB). Testa o parser, a chave da lista (colId),
// a fixada do perfil e o caminho para a Home: a pasta extra que a Home recebe
// carrega prov "mdblist" com o catId numerico — exatamente o ColSource que
// lst_abrir passa a desc_vertudo_fonte.
#include "listas.h"
#include "extras.h"
#include "descoberta.h"
#include "colecoes.h"
#include "dados.h"
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int falhas;
#define CONFERE(cond, ...) do { \
  if (!(cond)) { falhas++; printf("  FALHOU: "); printf(__VA_ARGS__); printf("\n"); } \
  else { printf("  ok: "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static char *ler(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  long n;
  char *b;
  if (!f) { printf("nao abri %s\n", caminho); exit(2); }
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = malloc((size_t)n + 1);
  assert(b);
  assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0;
  fclose(f);
  return b;
}

static LstLista listaMdb(const char *id, const char *nome, const char *midia) {
  LstLista l;
  memset(&l, 0, sizeof l);
  l.fonte = LST_MDB;
  l.itens = -1;
  l.curtidas = -1;
  l.propria = 1;
  snprintf(l.colId, sizeof l.colId, "%s", id);
  snprintf(l.titulo, sizeof l.titulo, "%s", nome);
  snprintf(l.midia, sizeof l.midia, "%s", midia);
  return l;
}

int main(int argc, char **argv) {
  char *j, dir[600];
  int n;
  const LstLista *a, *b;
  assert(argc == 2);

  // NUVIO_DADOS aponta para a pasta temporaria do script (padrao de
  // tests/listas.c: nada escreve no home de quem roda).
  snprintf(dir, sizeof dir, "%s", argv[1]);
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(".");
  perfis_definir_ativo(1);
  lst_iniciar();

  printf("\n[#762] listas do /lists/user\n");
  j = ler("tests/fixtures/mdblist_user_lists.json");
  n = lst_teste_mdblist_listas(j);
  CONFERE(n == 2, "duas listas (a terceira nao tem id; veio %d)", n);
  a = lst_lista(0); b = lst_lista(1);
  CONFERE(a && a->fonte == LST_MDB, "fonte LST_MDB");
  CONFERE(a && !strcmp(a->titulo, "Oscar Winners"), "titulo lido");
  CONFERE(a && !strcmp(a->colId, "13914"), "colId 13914 (veio %s)", a ? a->colId : "");
  CONFERE(a && !strcmp(a->midia, "MOVIE"), "mediatype movie -> MOVIE");
  CONFERE(b && !strcmp(b->colId, "20416"), "id da segunda");
  CONFERE(b && !strcmp(b->midia, "TV"), "mediatype show -> TV");
  free(j);

  printf("\n[#762] fixar e levar para a Home\n");
  { LstLista l = listaMdb("13914", "Oscar Winners", "MOVIE");
    const char *porque = "";
    char chave[96];
    int antes;
    // Sem chave do MDBList o caminho para a Home recusa com o motivo.
    CONFERE(!lst_aceita_home(&l, &porque) && porque[0], "sem chave recusa (%s)", porque);
    extras_definir_chave("chave-de-teste");
    CONFERE(extras_mdblist_tem_chave(), "chave presente");
    CONFERE(lst_aceita_home(&l, &porque), "com chave aceita");
    antes = lst_fixada(&l);
    CONFERE(lst_alternar_fixada(&l) != antes, "fixada alternada");
    CONFERE(lst_na_home(&l) == 0, "na home comeca desligada");
    CONFERE(lst_alternar_home(&l) == 1, "levar para a Home liga");
    // A pasta extra que a Home recebe tem a fonte "mdblist".
    { int i, achou = -1;
      const ColFolder *pf = NULL;
      snprintf(chave, sizeof chave, "lista_mdblist_13914");
      for (i = 0; i < col_n(); i++) {
        const ColFolder *f = col_folder(i);
        if (f && !strcmp(f->id, chave)) { achou = i; pf = f; break; }
      }
      CONFERE(achou >= 0, "pasta extra na Home (id %s)", chave);
      CONFERE(pf && pf->nSources == 1 && !strcmp(pf->sources[0].prov, "mdblist"),
              "fonte da pasta e prov mdblist");
      CONFERE(pf && !strcmp(pf->sources[0].catId, "13914"), "catId da pasta 13914");
      CONFERE(pf && !strcmp(pf->sources[0].midia, "MOVIE"), "midia MOVIE"); }
    // Desligar devolve o estado.
    CONFERE(lst_alternar_home(&l) == 0, "desligar da Home");
    CONFERE(lst_alternar_fixada(&l) == 0, "desfixar"); }

  printf("\n[#762] fixada sobrevive a um reinicio\n");
  { LstLista l = listaMdb("13914", "Oscar Winners", "MOVIE");
    lst_alternar_fixada(&l);
    lst_esquecer();
    lst_iniciar();
    CONFERE(lst_fixada(&l), "continua fixada depois do reinicio");
    lst_pedir_fixadas();
    CONFERE(lst_n() == 1, "a aba Fixadas mostra uma (veio %d)", lst_n());
    CONFERE(lst_lista(0) && lst_lista(0)->fonte == LST_MDB &&
            !strcmp(lst_lista(0)->colId, "13914"), "e a lista do MDBList");
    lst_alternar_fixada(&l); }

  lst_esquecer_conta();
  puts(falhas ? "mdblist_listas: FALHOU" : "mdblist_listas: ok");
  return falhas ? 1 : 0;
}
