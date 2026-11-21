// Dubles dos modulos que torboxbib_ui.c referencia: o teste dirige o parser e
// o ciclo assincrono de producao, sem a pilha de debrid/partes inteira.
#include "debrid.h"
#include "partes.h"
#include <string.h>

int debrid_tb_disponivel(void) { return 1; }

void partes_link_pedir(int usenet, int item, int arquivo, int comLista) {
  (void)usenet; (void)item; (void)arquivo; (void)comLista;
}
void partes_link_cancelar(void) {}
int partes_link_estado(char *url, unsigned n, PartesLista *l) {
  (void)url; (void)n; (void)l; return -2;
}
int partes_lista_copiar(PartesLista *dst, const PartesLista *src) {
  if (dst && src) *dst = *src;
  return dst != NULL;
}
void partes_lista_limpar(PartesLista *l) { if (l) memset(l, 0, sizeof *l); }
void partes_arquivo_limpar(void) {}
int partes_arquivo_de(const char *alvo, PartesPonto *p) { (void)alvo; (void)p; return 0; }
int debrid_tb_disponivel_real(void);
