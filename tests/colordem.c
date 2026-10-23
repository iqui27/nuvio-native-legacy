// A ORDEM DOS ITENS DA FILEIRA DE COLECAO (2.0.5: "alem do estilo da fileira,
// ordenar elas por ano, ordem alfabetica, ordem da lista ou aleatorio").
//
// As quatro ordens da escolha por fileira (FilOrdem, fileiras.c) sobre as
// pastas de um grupo, a gravacao por fileira e a semente do embaralhado — que
// tem de ser ESTAVEL de uma chamada para a outra (a montagem da home nao pode
// trocar o baralho de um quadro para o outro) e diferente da ordem da lista.
#include "colecoes.h"
#include "fileiras.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *addons_base_por_id(const char *id) { (void)id; return ""; }

// A conta nao faz parte deste teste da ordem. Sem o retrato de addons
// (tests/fileiras.c tem o mesmo par de dubles fracos).
unsigned addons_versao(void) { return 0; }
int addons_perfil_da_lista(void) { return -1; }
int addons_travar_lista(unsigned versao, int perfil) { (void)versao; (void)perfil; return 0; }
void addons_destravar_lista(void) {}

// Dubles de dados.c como tests/fileiras.c: sem arquivo ate o round-trip pedir.
static int usaArquivo;
int dados_gravar(const char *nome, const char *conteudo) {
  if (!usaArquivo) { (void)nome; (void)conteudo; return 1; }
  char caminho[600];
  snprintf(caminho, sizeof caminho, "%s/%s", nvt_dir(), nome);
  FILE *f = fopen(caminho, "w");
  if (!f) return 0;
  fputs(conteudo, f);
  fclose(f);
  return 1;
}
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  if (!usaArquivo) { (void)dst; (void)tam; (void)nome; return NULL; }
  snprintf(dst, tam, "%s/%s", nvt_dir(), nome);
  return dst;
}
void fil_teste_recarregar(void);

// Cinco pastas no grupo "Anos e listas": duas com ano no titulo, duas sem
// (Directors, Themes...) e uma com minuscula para o strcasecmp.
static const char *PASTA_FMT =
  "{\"id\":\"f%d\",\"title\":\"%s\",\"sources\":"
  "[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}";
static const char *TITULOS[5] = {
  "Cinema 2005", "Anos 90", "aardvark", "Boom 2020", "Melhores de 1999"
};
#define NP 5

static void carregarGrupo(void) {
  char js[2048];
  int k = snprintf(js, sizeof js, "{\"collections\":[{\"id\":\"c1\",\"title\":\"Anos e listas\",\"folders\":[");
  for (int i = 0; i < NP; i++) {
    char pasta[512];
    snprintf(pasta, sizeof pasta, PASTA_FMT, i, TITULOS[i]);
    k += snprintf(js + k, sizeof js - (size_t)k, "%s%s", i ? "," : "", pasta);
  }
  snprintf(js + k, sizeof js - (size_t)k, "]}]}");
  assert(col_definir_json(js) == NP);
}

// Os indices do grupo, na ordem que col_grupo_chave devolve (a da colecao).
static void pastas(int *v) {
  char chave[192];
  col_chave_grupo("Anos e listas", chave, sizeof chave);
  assert(!strcmp(chave, "collection_c1"));
  int n = col_grupo_chave(chave, v, NP);
  assert(n == NP);
}

static void conferir(const char *nome, const int *v, const int *esperado) {
  for (int i = 0; i < NP; i++)
    if (v[i] != esperado[i]) {
      printf("FALHOU (%s): posicao %d e %d, esperava %d\n", nome, i, v[i], esperado[i]);
      assert(0);
    }
  printf("ok  %s\n", nome);
}

int main(void) {
  int v[NP], v2[NP];
  // HERMETICO: a rodada anterior pode ter deixado o fileirasui.txt dela no
  // TMPDIR (o duplo de dados grava la), e a ordem que sobrou valeria aqui.
  { char caminho[600];
    snprintf(caminho, sizeof caminho, "%s/fileirasui.txt", nvt_dir());
    remove(caminho); }
  carregarGrupo();
  pastas(v);
  conferir("ordem da lista: a da propria colecao, nada mexido", v, (int[]){ 0, 1, 2, 3, 4 });

  // ANO: mais novo primeiro; sem ano no fim, na ordem que ja estavam.
  pastas(v);
  col_ordenar_pastas(v, NP, FIL_ORD_ANO, 0);
  conferir("ano: 2020, 2005, 1999 e as sem ano no fim", v, (int[]){ 3, 0, 4, 1, 2 });

  // ALFABETICA: strcasecmp (aardvark < Anos 90 < Boom 2020 < Cinema < Melhores).
  pastas(v);
  col_ordenar_pastas(v, NP, FIL_ORD_AZ, 0);
  conferir("alfabetica: sem case, como os titulos sao comparados no app", v,
           (int[]){ 2, 1, 3, 0, 4 });

  // ALEATORIA: permutacao, diferente da ordem da lista e ESTAVEL — a mesma
  // semente devolve o mesmo baralho na chamada seguinte (a montagem nao pode
  // trocar as pastas de um quadro para o outro enquanto a pessoa rola).
  pastas(v);
  col_ordenar_pastas(v, NP, FIL_ORD_ALEATORIA, 20261031u);
  { int visto[NP] = { 0 };
    for (int i = 0; i < NP; i++) {
      assert(v[i] >= 0 && v[i] < NP);
      assert(!visto[v[i]]);   // cada pasta uma vez so
      visto[v[i]] = 1;
    }
    assert(memcmp(v, (int[]){ 0, 1, 2, 3, 4 }, sizeof v) != 0);   // embaralhou mesmo
  }
  pastas(v2);
  col_ordenar_pastas(v2, NP, FIL_ORD_ALEATORIA, 20261031u);
  conferir("aleatoria: a mesma semente, o mesmo baralho (estavel entre quadros)", v2, v);

  // A ESCOLHA E DA FILEIRA (fileiras.c): so colecao aceita, gravada no arquivo
  // do perfil como as outras opcoes e de volta na releitura.
  usaArquivo = 1;
  fil_registrar("collection_c1", "Anos e listas", "", "", NP);
  assert(fil_ordem("collection_c1") == FIL_ORD_LISTA);   // o padrao e a lista
  assert(fil_definir_ordem("continue_watching", FIL_ORD_ANO) == 0);   // fileira do app: nao
  assert(fil_definir_ordem("collection_nao_existe", FIL_ORD_ANO) == 0);
  assert(fil_definir_ordem("collection_c1", FIL_ORD_N) == 0);         // fora da faixa
  assert(fil_definir_ordem("collection_c1", FIL_ORD_AZ) == 1);
  assert(fil_ordem("collection_c1") == FIL_ORD_AZ);
  assert(fil_definir_ordem("collection_c1", FIL_ORD_AZ) == 1);   // de novo: nada mudou
  { // O caminho da ESCRITA: a linha opcional vai para o arquivo do perfil.
    char linha[256] = "", caminho[600];
    FILE *f;
    snprintf(caminho, sizeof caminho, "%s/fileirasui.txt", nvt_dir());
    f = fopen(caminho, "r");
    assert(f);
    while (fgets(linha, sizeof linha, f))
      if (!strncmp(linha, "ordemfileira collection_c1 ", 27)) break;
    assert(!strncmp(linha, "ordemfileira collection_c1 2\n", 27));
    fclose(f); }
  // O caminho da LEITURA: arquivo com a linha escolhida recarrega a ordem.
  // (fil_esquecer regrava o arquivo vazio — dai o gravar no-op e o arquivo
  // escrito a mao, como tests/fileiras.c faz com as linhas "mortas".)
  { FILE *f = fopen(nvt("/fileirasui.txt"), "w");
    assert(f);
    fputs("linha collection_c1\t0\t0\t0\t0\tAnos e listas\n"
          "ordemfileira collection_c1 1\n", f);
    fclose(f); }
  usaArquivo = 0;
  fil_esquecer();
  usaArquivo = 1;
  fil_teste_recarregar();
  assert(fil_ordem("collection_c1") == FIL_ORD_ANO);   // sobreviveu ao arquivo
  puts("ok  ordem gravada por fileira, so em colecao, e recarregada do arquivo");
  return 0;
}
