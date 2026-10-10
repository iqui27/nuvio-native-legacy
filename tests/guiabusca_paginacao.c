// #344: a busca do guia (buscaFazer, REAL — este teste inclui src/guia.c)
// pagina a grade de um canal em lotes de 24. O cursor e a POSICAO na sequencia,
// nao o fim do ultimo programa: a XMLTV nao rejeita sobreposicao.
//
//  PULO:      #1-#23 em fatias de 1 min, #24 em [23,100), #25 [24,25), #26 [25,26),
//             #27 em [100,101). Cursor por horario recomecava em 100 e perdia #25-#26.
//  DUPLICADO: 22 fatias de 1 min, depois [22,100) [23,25) [24,26) [25,27).
//             Cursor por horario recomecava em 25 e repetia #23-#26.
// Cada programa tem de aparecer em buscaProg exatamente uma vez.
#include "badges.h"
#include "../src/guia.c"
#include <assert.h>

static char *xml_canal(const char *id, const char *pref, const int (*iv)[2], int n, time_t base) {
  char *x = malloc((size_t)n * 220 + 400), *p; int k;
  p = x + sprintf(x, "<?xml version=\"1.0\"?><tv><channel id=\"%s\"><display-name>%s</display-name></channel>", id, id);
  for (k = 0; k < n; k++) {
    char a[24], b[24]; time_t i0 = base + iv[k][0] * 60, i1 = base + iv[k][1] * 60;
    strftime(a, sizeof a, "%Y%m%d%H%M%S +0000", gmtime(&i0));
    strftime(b, sizeof b, "%Y%m%d%H%M%S +0000", gmtime(&i1));
    p += sprintf(p, "<programme channel=\"%s\" start=\"%s\" stop=\"%s\"><title>Show %s%02d</title></programme>", id, a, b, pref, k + 1);
  }
  strcpy(p, "</tv>");
  return x;
}

// Carrega, busca "show" e confere: n programas, cada titulo uma vez.
static int falhas;
static void caso(const char *nome, const char *id, const int (*iv)[2], int n) {
  time_t base = time(NULL) + 30;           // tudo no futuro, dentro das 6 h
  char *xml = xml_canal(id, "X", iv, n, base), t[32];
  int k, j, cont;
  epg_teste_limpar();
  assert(epg_xml_processar(xml) > 0);
  free(xml);
  memset(canais, 0, sizeof canais);
  snprintf(canais[0].nome, sizeof canais[0].nome, "Canal Qualquer");
  canais[0].epg = epg_match_id(id);
  assert(canais[0].epg >= 0);
  nCanais = 1;
  buscaFazer("show");
  printf("%s: %d programa(s)\n", nome, buscaNP);
  if (buscaNP != n) { printf("  FALHOU: esperava %d\n", n); falhas++; }
  for (k = 1; k <= n; k++) {
    snprintf(t, sizeof t, "Show X%02d", k);
    for (cont = 0, j = 0; j < buscaNP; j++) if (!strcmp(buscaProg[j].titulo, t)) cont++;
    if (cont != 1) printf("  %s apareceu %d vez(es)\n", t, cont);
    if (cont != 1) falhas++;
  }
}

int main(void) {
  int pulo[27][2], dup[26][2], k;
  for (k = 0; k < 23; k++) { pulo[k][0] = k; pulo[k][1] = k + 1; }
  pulo[23][0] = 23; pulo[23][1] = 100;
  pulo[24][0] = 24; pulo[24][1] = 25;
  pulo[25][0] = 25; pulo[25][1] = 26;
  pulo[26][0] = 100; pulo[26][1] = 101;
  for (k = 0; k < 22; k++) { dup[k][0] = k; dup[k][1] = k + 1; }
  dup[22][0] = 22; dup[22][1] = 100;
  dup[23][0] = 23; dup[23][1] = 25;
  dup[24][0] = 24; dup[24][1] = 26;
  dup[25][0] = 25; dup[25][1] = 27;
  caso("pulo (sobreposicao)", "Pulo.br", (const int (*)[2])pulo, 27);
  caso("duplicado (sobreposicao)", "Dup.br", (const int (*)[2])dup, 26);
  if (falhas) { printf("FALHOU: %d checagem(ns)\n", falhas); return 1; }
  puts("PASS: guiabusca_paginacao");
  return 0;
}
