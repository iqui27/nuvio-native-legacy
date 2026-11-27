// Embedded Arabic SRT in batches (owner 25/11/2026, Tizen): mkvass sends the
// track in pieces (legenda_definir_corpo_se, then legenda_atualizar_corpo_se).
// The libass RTL document must follow every batch, not only the first one:
//  [1] first batch Latin, second brings Arabic -> the RTL document is loaded;
//  [2] both batches Arabic -> the second document has both lines.
#include "../src/legenda.h"
#include "../src/assrender.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char ultimo[8192];
static int cargas, atualizacoes;
static void guardar(const char *c, size_t n) {
  if (n >= sizeof ultimo) n = sizeof ultimo - 1;
  memcpy(ultimo, c, n); ultimo[n] = 0;
}
int assrender_carregar_texto(const char *c, size_t n, unsigned g) { (void)g; cargas++; guardar(c, n); return 1; }
int assrender_atualizar_texto(const char *c, size_t n, unsigned g) { (void)g; atualizacoes++; guardar(c, n); return 1; }
int assrender_carregar(const char *c, size_t n, unsigned g) { (void)c; (void)n; (void)g; return 1; }
int assrender_atualizar(const char *c, size_t n, unsigned g) { (void)c; (void)n; (void)g; return 1; }
const char *assrender_diagnostico(void) { return ""; }
void assrender_geracao(unsigned g) { (void)g; }
void assrender_limpar(void) {}
void assrender_limpar_fontes(void) {}
const char *i18n(const char *s) { return s; }
char *rede_baixar_bin(const char *u, int t, long *n) { (void)u; (void)t; *n = 0; return NULL; }

#define L1 "1\n00:00:01,000 --> 00:00:02,000\nHello\n\n"
#define A1 "1\n00:00:01,000 --> 00:00:02,000\n\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7\n\n"
#define A2 "2\n00:00:05,000 --> 00:00:06,000\n\xd8\xb4\xd9\x83\xd8\xb1\xd8\xa7\n\n"

int main(void) {
  unsigned g = legenda_definir_corpo_se(L1, legenda_geracao());
  assert(g && cargas == 0);
  assert(legenda_atualizar_corpo_se(L1 A2, g));
  assert(cargas + atualizacoes == 1 && strstr(ultimo, "\xd8\xb4\xd9\x83\xd8\xb1\xd8\xa7"));

  cargas = atualizacoes = 0; ultimo[0] = 0;
  g = legenda_definir_corpo_se(A1, legenda_geracao());
  assert(g && cargas == 1 && strstr(ultimo, "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7"));
  assert(legenda_atualizar_corpo_se(A1 A2, g));
  assert(atualizacoes == 1);
  assert(strstr(ultimo, "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7") && strstr(ultimo, "\xd8\xb4\xd9\x83\xd8\xb1\xd8\xa7"));
  puts("legenda_arabe_lotes: RTL document follows every embedded batch ok");
  return 0;
}
