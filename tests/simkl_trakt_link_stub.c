// Stubs mínimos para tests/simkl_anime_cw no Linux (sem linkar trakt.c inteiro).
#include "catalogo.h"
#include <stdint.h>

int trakt_enfeitar_lote(CatItem *saida, int n) { (void)saida; return n; }
int trakt_enfeitar_lote_sessao(CatItem *saida, int n, uint64_t g) { (void)g; return n; }
