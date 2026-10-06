// CONTRATO TV <-> SERVIDOR do cofre do diario: o corpo que a TV monta
// (diarionuvem_json_lote) e aceito pelo Worker de verdade, e a restauracao que
// o Worker devolve volta a TV igual. Roda por tests/diarionuvem_servidor.sh,
// contra `wrangler dev --local` (D1 local, sem rede externa).
//   modo "lote": imprime o corpo de POST /v1/diario/lote
//   modo "ler":  le o corpo de GET /v1/diario da entrada e confere
#include "diarionuvem.h"
#include "ilha.h"
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int perfis_ativo(void) { return 1; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
const char *i18n(const char *s) { return s; }
int sessao_logada(void) { return 0; }
const char *sessao_token(void) { return ""; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; *st = 0; return NULL; }
void ilha_avisar_ex(const IlhaAvisoEx *a) { (void)a; }
void ilha_avisar(const char *c, int t, const char *i, const char *x, unsigned ms, int k) { (void)c; (void)t; (void)i; (void)x; (void)ms; (void)k; }
int ilha_tem(const char *c) { (void)c; return 0; }

static void montar(DiarioEntrada *v) {
  memset(v, 0, sizeof *v * 3);
  snprintf(v[0].id, sizeof v[0].id, "tt0111161"); v[0].origem = 'p'; v[0].quando = 1790000000; v[0].nota10 = 9;
  v[0].flags = DIARIO_F_REVER | DIARIO_F_CORACAO; v[0].hora = 118; v[0].seg = 8000; v[0].cor = 0x112233;
  v[0].tags = 1u << DIARIO_TAG_FINAL; v[0].tmdb = 278; v[0].durSeg = 8520;
  snprintf(v[1].id, sizeof v[1].id, "tt0900"); v[1].origem = 'c'; v[1].temporada = 2; v[1].episodio = 5;
  v[1].quando = 1790001000; v[1].hora = -1; v[1].flags = DIARIO_F_DATA_APROX;
  snprintf(v[2].id, sizeof v[2].id, "tt0002"); v[2].origem = 'p'; v[2].quando = 1790002000; v[2].hora = -1;
  v[2].flags = DIARIO_F_DORMIU_SUS | DIARIO_F_PARCIAL; v[2].dormiuSeg = 4320; v[2].durSeg = 9000;
  diario_nome_definir("tt0111161", "Um \"Sonho\" de Liberdade", 1994, "");
}

int main(int argc, char **argv) {
  DiarioEntrada v[3], w[8];
  montar(v);
  if (argc > 1 && !strcmp(argv[1], "lote")) {
    static char corpo[16384];
    assert(diarionuvem_json_lote(v, 3, corpo, sizeof corpo));
    fputs(corpo, stdout);
    return 0;
  }
  { static char r[65536]; size_t n = fread(r, 1, sizeof r - 1, stdin); int mais; long long ate; int i, k;
    r[n] = 0;
    k = diarionuvem_ler_itens(r, w, 8, &mais, &ate);
    assert(k == 3 && !mais && ate > 0);
    for (i = 0; i < 3; i++) {
      int j;
      for (j = 0; j < 3 && strcmp(w[j].id, v[i].id); j++) {}
      assert(j < 3);
      assert(w[j].temporada == v[i].temporada && w[j].episodio == v[i].episodio);
      assert(w[j].quando == v[i].quando && w[j].nota10 == v[i].nota10 && w[j].flags == v[i].flags);
      assert(w[j].tags == v[i].tags && w[j].cor == v[i].cor && w[j].seg == v[i].seg && w[j].hora == v[i].hora);
      assert(w[j].origem == v[i].origem && w[j].tmdb == v[i].tmdb && w[j].dormiuSeg == v[i].dormiuSeg);
      assert(w[j].durSeg == v[i].durSeg);
    }
    { char t[64]; int ano; assert(diario_nome("tt0111161", t, sizeof t, &ano, NULL, 0) && ano == 1994); }
    puts("diarionuvem_servidor: ok (o que a TV manda volta igual)"); }
  return 0;
}
