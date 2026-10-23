// Search: gap under the top menu pill at every text scale, and the ?!# page
// typing accents and punctuation (UTF-8) into the live query.
#include <assert.h>
#include <stdio.h>
// The pill sits at fixed REAL px (what the TCL shows at 80%), the Search screen
// measures in virtual px (real / text scale).
static int pilula_falsa(float *x, float *y, float *w, float *h) {
  *x = 34; *y = 35; *w = 150; *h = 45;
  return 1;
}
#define menu_pilula_rect pilula_falsa
#ifndef BUSCA_C
#define BUSCA_C "../src/busca.c"
#endif
#include BUSCA_C

#define GAP_MIN 20.0f
static int busca_teclado_gap(void) {
  static const float escalas[] = { 0.8f, 1.0f, 1.3f };
  int bad = 0;
  for (int i = 0; i < 3; i++) {
    float x, y, w, h, gap;
    gfx_escala_txt_definir(escalas[i]);
    TXT_INI();
    pilula_falsa(&x, &y, &w, &h);
    gap = buTopoEsq() - (y + h) / gfx_escala_txt();
    printf("%s gap pilula->campo %.0f%%: %.1f px (min %.0f)\n", gap >= GAP_MIN ? "PASS" : "FAIL",
           escalas[i] * 100, gap, GAP_MIN);
    bad |= gap < GAP_MIN;
    TXT_FIM();
  }
  gfx_escala_txt_definir(1.0f);
  return bad;
}

#ifndef SO_GAP
static void tocar(const char *tecla) {
  for (int f = 0; f < kbFil; f++)
    for (int c = 0; c < KB_COLUNAS[f]; c++)
      if (!strcmp(kbTeclas[f * BU_KB_COLS + c], tecla)) {
        focoKb.fileira = f; focoKb.coluna = c; aplicarTecla(); return;
      }
  printf("FAIL tecla %s ausente\n", tecla); assert(0);
}
static int busca_teclado_simbolos(void) {
  static const char *esperadas[] = { "\xc3\xa1", "\xc3\xa0", "\xc3\xa2", "\xc3\xa3", "\xc3\xa9", "\xc3\xaa",
    "\xc3\xad", "\xc3\xb3", "\xc3\xb4", "\xc3\xb5", "\xc3\xba", "\xc3\xa7", "\xc3\xb1", "\xc3\xbc",
    ".", ",", ":", ";", "'", "\"", "!", "?", "-", "_", "&", "(", ")", "/", "@", "#", "+", "=", "*" };
  layoutCir = pagSim = 0; nConsulta = 0; consulta[0] = 0;
  kbMontar();
  focus_iniciar(&focoKb, kbFil + 1, KB_COLUNAS);
  focoKb.fileira = kbFil; focoKb.coluna = 3; aplicarTecla();     // ?!#
  assert(pagSim == 1);
  for (unsigned i = 0; i < sizeof esperadas / sizeof *esperadas; i++) {
    int ach = 0;
    for (int k = 0; k < kbN; k++) ach |= !strcmp(kbTeclas[k], esperadas[i]);
    if (!ach) { printf("FAIL falta a tecla %s\n", esperadas[i]); return 1; }
  }
  tocar("\xc3\xa9"); tocar(":");
  assert(!strcmp(busca_consulta(), "\xc3\xa9:"));
  { char n[16]; busca_normalizar("Am\xc3\xa9lie", n, sizeof n); assert(!strcmp(n, "amelie")); }
  focoKb.fileira = kbFil; focoKb.coluna = 3; aplicarTecla();     // abc
  assert(pagSim == 0 && kbN == 36 && !strcmp(kbTeclas[0], "a"));
  puts("PASS pagina ?!# digita \"é\" e \":\" (UTF-8) e volta para abc");
  return 0;
}
#endif

int main(void) {
  int bad = busca_teclado_gap();
#ifndef SO_GAP
  bad |= busca_teclado_simbolos();
#endif
  return bad;
}
