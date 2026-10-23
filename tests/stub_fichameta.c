// Sem disco nos testes que compilam trakt.c sozinhos.
#include "../src/fichameta.h"
#include <stddef.h>
#include <stdint.h>
// Default session for isolated Trakt fixtures; race tests provide strong hooks.
__attribute__((weak)) uint64_t perfis_sessao_geracao(void) { return 0; }
__attribute__((weak)) int perfis_sessao_atual(uint64_t g) { return g == 0; }
__attribute__((weak)) int perfis_sessao_travar(uint64_t g) { return g == 0; }
__attribute__((weak)) void perfis_sessao_destravar(void) {}
char *fichameta_ler(const char *c, long t) { (void)c; (void)t; return NULL; }
void fichameta_gravar(const char *c, const char *b) { (void)c; (void)b; }
// Preferencias padrao para leitores Trakt isolados; o diagnostico injeta OFF.
__attribute__((weak)) int ajustes_cw_proximo(void) { return 1; }
__attribute__((weak)) int ajustes_cw_minimo(void) { return 1; }
__attribute__((weak)) int ajustes_cw_concluido(void) { return 90; }
