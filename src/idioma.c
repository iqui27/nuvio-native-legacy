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

// CACHE DA BUSCA. text.c chama i18n em TODA linha desenhada, todo quadro;
// com o ingles ligado cada chamada era uma busca binaria de ~11 strcmp em
// 1500 entradas. Medido no Mac (perfil CDP, 35 s de home): 268 ms dentro de
// i18n, a segunda funcao mais cara do fio principal depois de main — e na
// Samsung o mesmo trabalho custa varias vezes mais. Tabela direta de 1024
// posicoes chaveada pelo hash FNV-1a do texto: acerto = 1 passada pelo texto
// + 1 strcmp (positivo) ou 0 strcmp (negativo, confiado pelo hash de 64 bits
// mais o tamanho). Titulo de filme e fragmento de sinopse tambem entram — sao
// justamente os negativos que antes pagavam a busca inteira.
#define I18N_CACHE 1024
static struct { unsigned long long h; unsigned n; int idx; } cache[I18N_CACHE];

const char *i18n(const char *s) {
  int lo = 0, hi = TAB_N - 1;
  unsigned long long h = 1469598103934665603ull;
  unsigned n = 0, slot;
  const unsigned char *p;
  idioma_registrar(s);
  if (!s || !*s || !ajustes_idioma_ingles()) return s;
  if (ordemOk < 0) conferirOrdem();
  if (!ordemOk) return s;
  for (p = (const unsigned char *)s; *p; p++, n++) { h ^= *p; h *= 1099511628211ull; }
  slot = (unsigned)(h % I18N_CACHE);
  if (cache[slot].h == h && cache[slot].n == n && (cache[slot].idx < 0 || !strcmp(s, TAB[cache[slot].idx].pt)))
    return cache[slot].idx < 0 ? s : TAB[cache[slot].idx].en;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    int c = strcmp(s, TAB[m].pt);
    if (c == 0) { cache[slot].h = h; cache[slot].n = n; cache[slot].idx = m; return TAB[m].en; }
    if (c < 0) hi = m - 1; else lo = m + 1;
  }
  cache[slot].h = h; cache[slot].n = n; cache[slot].idx = -1;
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
