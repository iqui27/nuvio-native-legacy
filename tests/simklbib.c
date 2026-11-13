// A BIBLIOTECA DO SIMKL NO DISCO (src/simklbib.c): uma copia por perfil, de um
// vinculo so. Volta inteira; outro vinculo, outra versao ou arquivo cortado
// nunca sao lidos — sao apagados e quem chama faz a leitura cheia.
// Disco em memoria; o texto gravado e o que dados.c gravaria.
#include "simklbib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_MAX 8
static struct { char nome[48]; char *conteudo; } arqs[ARQ_MAX];
static int leves;
static int achar(const char *nome) {
  for (int i = 0; i < ARQ_MAX; i++) if (arqs[i].conteudo && !strcmp(arqs[i].nome, nome)) return i;
  return -1;
}
char *dados_ler(const char *nome) {
  int i = achar(nome);
  return i >= 0 ? strdup(arqs[i].conteudo) : NULL;
}
static int gravar(const char *nome, const char *c) {
  int i = achar(nome);
  if (i < 0) for (i = 0; i < ARQ_MAX && arqs[i].conteudo; i++) { }
  assert(i < ARQ_MAX);
  free(arqs[i].conteudo);
  snprintf(arqs[i].nome, sizeof arqs[i].nome, "%s", nome);
  arqs[i].conteudo = strdup(c);
  return 1;
}
int dados_gravar(const char *nome, const char *c) { return gravar(nome, c); }
int dados_gravar_leve(const char *nome, const char *c) { leves++; return gravar(nome, c); }
int dados_apagar(const char *nome) {
  int i = achar(nome);
  if (i >= 0) { free(arqs[i].conteudo); arqs[i].conteudo = NULL; }
  return i >= 0;
}
static int existe(const char *n) { return achar(n) >= 0; }

static const char *ATIV = "{\"all\":\"2026-11-08T10:00:00Z\",\n\"tv_shows\":{\"all\":\"2026-11-08T10:00:00Z\"}}";
static const char *BIB =
  "{\"shows\":[{\"show\":{\"title\":\"Linha\\nquebrada\",\"ids\":{\"simkl\":1}}}],\n"
  "\"movies\":[],\"anime\":[{\"show\":{\"ids\":{\"simkl\":2}}}]}";

// Grava a copia valida do perfil e devolve o texto dela (para estragar).
static char *gravarValida(int perfil, unsigned long long conta) {
  SimklBib b = { (char *)ATIV, (char *)BIB };
  char n[40];
  assert(simklbib_gravar(perfil, conta, &b));
  snprintf(n, sizeof n, SIMKLBIB_ARQ_FMT, perfil);
  char *r = dados_ler(n);
  assert(r);
  return r;
}
// Poe `txt` como arquivo do perfil 1 e confere que a leitura recusa e apaga.
static void recusa(const char *txt, unsigned long long conta, const char *caso) {
  SimklBib b = { (char *)"lixo", (char *)"lixo" };
  gravar("simkl-biblioteca-p1.txt", txt);
  assert(!simklbib_ler(1, conta, &b));
  assert(!b.atividades && !b.biblioteca);
  assert(!existe("simkl-biblioteca-p1.txt"));
  printf("ok  recusa e apaga: %s\n", caso);
}

int main(void) {
  unsigned long long a = simklbib_conta("TOKEN-A"), c = simklbib_conta("TOKEN-C");
  SimklBib b = {0};
  char *txt, *p;

  assert(a && c && a != c && simklbib_conta("TOKEN-A") == a);
  assert(!simklbib_conta("") && !simklbib_conta(NULL));
  printf("ok  assinatura estavel por token, 0 sem token\n");

  assert(!simklbib_ler(1, a, &b) && !b.atividades);
  printf("ok  sem arquivo = leitura cheia\n");

  txt = gravarValida(1, a);
  assert(leves == 1);
  assert(!strstr(txt, "TOKEN-A"));
  assert(simklbib_ler(1, a, &b));
  assert(!strcmp(b.atividades, ATIV) && !strcmp(b.biblioteca, BIB));
  simklbib_liberar(&b);
  assert(!b.atividades && !b.biblioteca);
  printf("ok  volta inteira (quebras de linha nos corpos), gravacao leve, sem token\n");

  free(gravarValida(2, c));
  assert(simklbib_ler(2, c, &b)); simklbib_liberar(&b);
  assert(simklbib_ler(1, a, &b)); simklbib_liberar(&b);
  simklbib_apagar(2);
  assert(!existe("simkl-biblioteca-p2.txt") && existe("simkl-biblioteca-p1.txt"));
  printf("ok  um arquivo por perfil; apagar um nao toca o outro\n");

  assert(!simklbib_ler(1, c, &b) && !b.atividades);
  assert(!existe("simkl-biblioteca-p1.txt"));
  printf("ok  outro vinculo no mesmo perfil: descartada e apagada\n");

  free(txt);
  txt = gravarValida(1, a);
  assert(!simklbib_ler(1, 0, &b) && existe("simkl-biblioteca-p1.txt"));
  printf("ok  sem vinculo nao le nem apaga\n");

  { SimklBib vazia = { (char *)"", (char *)BIB }, nula = { NULL, (char *)BIB };
    assert(!simklbib_gravar(3, a, &vazia) && !simklbib_gravar(3, a, &nula));
    assert(!simklbib_gravar(3, 0, &(SimklBib){ (char *)ATIV, (char *)BIB }));
    assert(!existe("simkl-biblioteca-p3.txt")); }
  printf("ok  nao grava corpo vazio nem sem vinculo\n");

  // Estragos: cada um cai na leitura cheia.
  { char *v = strdup(txt);
    v[strlen("nuvio-simkl-biblioteca ")] = '9';
    recusa(v, a, "outra versao"); free(v); }
  recusa("outra coisa\n", a, "outro formato");
  { char *v = strdup(txt);
    v[strlen(v) - 20] = 0;
    recusa(v, a, "arquivo cortado"); free(v); }
  { char *v = malloc(strlen(txt) + 8);
    assert(v);
    strcpy(v, txt);
    strcat(v, "extra\n");
    recusa(v, a, "sobra depois do fim"); free(v); }
  { char *v = strdup(txt);
    p = strstr(v, "\nbiblioteca ") + 1;
    p[strlen("biblioteca ")] = '9';
    recusa(v, a, "tamanho maior que o arquivo"); free(v); }
  { char *v = strdup(txt);
    p = strstr(v, "atividades ");
    memcpy(p, "atividadez", 10);
    recusa(v, a, "rotulo errado"); free(v); }
  recusa("nuvio-simkl-biblioteca 1\nconta 00000000000000ff\natividades 99999999999999999999999\nx\n",
         0xff, "tamanho absurdo");

  free(txt);
  printf("simklbib: tudo ok\n");
  return 0;
}
