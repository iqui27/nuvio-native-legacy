#include "idioma.h"
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- tabela
//
// ORDENADA POR CHAVE, e a ordem e conferida em tempo de execucao no primeiro
// uso (ver conferirOrdem). Uma tabela desordenada faria a busca binaria falhar
// em SILENCIO: alguns textos traduziriam, outros nao, e o padrao pareceria
// aleatorio. Nao ha teste que pegue isso melhor que a propria busca.
typedef struct { const char *pt, *en; } Par;

static const Par TAB[] = {
#include "idioma_tab.h"
};
#define TAB_N ((int)(sizeof TAB / sizeof *TAB))

// ---------------------------------------------------------------- registro
//
// Levantamento das strings vivas. Guarda o que ja viu para nao reescrever a
// mesma linha a cada quadro — sao ~200 linhas por quadro a 60fps.
#define REG_MAX 1024
static char *vistos[REG_MAX];
static int   nVistos;
static FILE *arqReg;
static int   regTentado;

void idioma_registrar(const char *s) {
  int i;
  if (!s || !*s) return;
  if (!regTentado) {
    const char *caminho = getenv("NUVIO_TEXTO_DUMP");
    regTentado = 1;
    if (caminho && *caminho) arqReg = fopen(caminho, "w");
  }
  if (!arqReg || nVistos >= REG_MAX) return;
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], s)) return;
  vistos[nVistos] = strdup(s);
  if (!vistos[nVistos]) return;
  nVistos++;
  fprintf(arqReg, "%s\n", s);
  fflush(arqReg);
}

// ---------------------------------------------------------------- traducao

static int ordemOk = -1;
static void conferirOrdem(void) {
  int i;
  ordemOk = 1;
  for (i = 1; i < TAB_N; i++) {
    if (strcmp(TAB[i - 1].pt, TAB[i].pt) >= 0) {
      // Falar alto uma vez. Uma tabela fora de ordem nao quebra o app, ela
      // traduz PELA METADE — o pior defeito possivel, porque parece escolha.
      printf("[idioma] TABELA FORA DE ORDEM em %d: \"%s\" antes de \"%s\"\n",
             i, TAB[i - 1].pt, TAB[i].pt);
      fflush(stdout);
      ordemOk = 0;
      return;
    }
  }
}

const char *i18n(const char *s) {
  int lo = 0, hi = TAB_N - 1;
  idioma_registrar(s);
  if (!s || !*s || !ajustes_idioma_ingles()) return s;
  if (ordemOk < 0) conferirOrdem();
  if (!ordemOk) return s;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    int c = strcmp(s, TAB[m].pt);
    if (c == 0) return TAB[m].en;
    if (c < 0) hi = m - 1; else lo = m + 1;
  }
  // NAO ADIANTA RECLAMAR AQUI, e eu tentei: text.c chama i18n em cada linha
  // DESENHADA, entao esta funcao ve tambem titulo de filme, sinopse e cada
  // fragmento de quebra de linha. Uma medicao de 25 s no Mac produziu 96
  // avisos — sinopse do Fallout palavra a palavra, letras soltas do relogio —
  // e encheu o teto antes de qualquer texto de interface aparecer. O sinal
  // real ficaria enterrado no log de quem relata. Separar interface de
  // conteudo aqui exigiria a mesma heuristica de portugues que ja falhou tres
  // vezes na varredura estatica, e ela erra igual em "Detalhes" e "Cartazes".
  return s;
}
