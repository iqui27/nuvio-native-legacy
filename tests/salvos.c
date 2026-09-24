// Lista LOCAL de salvos: cresce alem do antigo teto de 300, vai inteira para o
// disco e, cheia, perde o MAIS ANTIGO — nunca o que acabou de ser salvo.
//
// O defeito (issue do Owlphibia29, "o contador nunca passou de 205; o que eu
// adiciono agora nao aparece nem substitui os antigos"): o 301o titulo era
// recusado com uma linha no log e o "+" acendia do mesmo jeito.
//
//   bash tests/salvos.sh
#include "salvos.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-58s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok   " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

// ---------------------------------------------------------------- dubles
// O disco e uma string: o que dados_gravar escreve, dados_ler devolve.
static char *disco;
int dados_gravar(const char *nome, const char *conteudo) {
  (void)nome; free(disco); disco = strdup(conteudo); return 1;
}
char *dados_ler(const char *nome) { (void)nome; return disco ? strdup(disco) : NULL; }
int dados_apagar(const char *nome) { (void)nome; free(disco); disco = NULL; return 1; }

// Catalogo vazio: salvos so MARCA quem esta nele, e aqui o que se prova e a
// lista local.
int cat_n(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return -1; }
void cat_definir_na_lista(int i, int naLista) { (void)i; (void)naLista; }

static CatItem ci;
static const CatItem *titulo(int i) {
  memset(&ci, 0, sizeof ci);
  snprintf(ci.imdb, sizeof ci.imdb, "tt%07d", 2000000 + i);
  snprintf(ci.tipo, sizeof ci.tipo, "movie");
  snprintf(ci.titulo, sizeof ci.titulo, "Filme %d", i);
  return &ci;
}

int main(void) {
  int i, entraram = 0, linhas = 0;
  const char *p;
  char id[24];

  printf("a lista local passa do antigo teto de 300:\n");
  salvos_iniciar();
  for (i = 0; i < 450; i++) entraram += salvos_definir(titulo(i), 1);
  confere("os 450 entraram", entraram, 450);
  confere("e estao na lista", salvos_n(), 450);
  confere("o ultimo salvo esta la", salvos_tem("tt2000449"), 1);
  for (p = disco; p && (p = strchr(p, '\n')); p++) linhas++;
  confere("o arquivo tem cabecalho + 450 linhas", linhas, 451);

  printf("\ncheia, sai o MAIS ANTIGO e o novo entra:\n");
  for (i = 450; i < SALVOS_MAX; i++) salvos_definir(titulo(i), 1);
  confere("lista no teto", salvos_n(), SALVOS_MAX);
  confere("o novo entrou mesmo com a lista cheia",
          salvos_definir(titulo(SALVOS_MAX), 1), 1);
  snprintf(id, sizeof id, "tt%07d", 2000000 + SALVOS_MAX);
  confere("e esta la", salvos_tem(id), 1);
  confere("o mais antigo saiu", salvos_tem("tt2000000"), 0);
  confere("o segundo mais antigo ficou", salvos_tem("tt2000001"), 1);
  confere("o tamanho nao passou do teto", salvos_n(), SALVOS_MAX);
  confere("e o mais novo e o ultimo da lista",
          strcmp(salvos_item(salvos_n() - 1)->id, id) == 0, 1);

  printf("\nremover continua valendo:\n");
  confere("tira o mais novo", salvos_definir(titulo(SALVOS_MAX), 0), 1);
  confere("e ele sumiu", salvos_tem(id), 0);

  salvos_esquecer();
  confere("esquecer zera a lista", salvos_n(), 0);
  confere("e apaga o arquivo", disco == NULL, 1);
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
