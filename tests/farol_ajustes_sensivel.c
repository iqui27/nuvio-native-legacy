// Conferidor da lista branca do Farol (farol-26). Roda ANTES do teste, com o
// ajlog.c de verdade, e emite uma linha "#error ..." para cada violacao da
// politica: chave que ajlog_chave_sensivel marca, nome de pacote ou token fora
// do alfabeto [a-z0-9_.:+-] de ate 39 caracteres. A saida vira um header que
// tests/farol_ajustes.c inclui — qualquer violacao e erro de COMPILACAO do
// teste, nao so uma assertiva em execucao (requisito do PLANO §5).
#include <stdio.h>
#include <string.h>
#include "ajlog.h"
#include "farolajustes.h"

static int nomeSeguro(const char *s) {
  const char *p;
  for (p = s; *p; p++)
    if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') &&
        *p != '_' && *p != '.' && *p != ':' && *p != '+' && *p != '-')
      return 0;
  return p - s >= 1 && p - s <= 39;
}

int main(void) {
  size_t i;
  int j;
  for (i = 0; i < sizeof FAROL_AJ_LISTA / sizeof *FAROL_AJ_LISTA; i++) {
    const FarolAjItem *it = &FAROL_AJ_LISTA[i];
    if (ajlog_chave_sensivel(it->chave))
      printf("#error \"farolajustes.h: chave sensivel na lista branca: %s\"\n", it->chave);
    if (ajlog_chave_sensivel(it->nome) || !nomeSeguro(it->nome))
      printf("#error \"farolajustes.h: nome de pacote fora do alfabeto: %s\"\n", it->nome);
    if (it->rot)
      for (j = 0; j < it->n; j++)
        if (!nomeSeguro(it->rot[j]))
          printf("#error \"farolajustes.h: token fora do alfabeto: %s (%s)\"\n", it->rot[j], it->nome);
  }
  return 0;
}
