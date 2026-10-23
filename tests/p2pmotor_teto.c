// #334: conta do teto duro do motor P2P (p2pmotor_teto_duro), com os tetos
// COMPILADOS de verdade (P2PM_DURO_MAX_MB 1536, reserva 512 MB). So conta: nao
// sobe motor. Rodado por tests/p2pmotor.sh.
#include "p2pmotor.h"
#include "p2p.h"
#include <string.h>
#include "rede.h"
#include <assert.h>
#include <stdio.h>

// cotos do app (os mesmos de tests/p2pmotor.c)
int ajustes_p2p_ligado(void) { return 1; }
const char *ajustes_p2p_url(void) { return ""; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }
const char *dados_dir(void) { return "/nao/usado"; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) { (void)u; (void)s; (void)c; (void)b; if (st) *st = 0; return NULL; }
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *t, int *st, int *e, char *f, unsigned n) {
  (void)u; (void)s; (void)a; (void)b; (void)t; (void)e; (void)f; (void)n; if (st) *st = 0; return NULL; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

#define MB(x) ((uint64_t)(x) << 20)
#define GB(x) ((uint64_t)(x) << 30)

int main(void) {
  // No NV_P2P_MOTOR: even a saved enabled setting cannot open a torrent.
  assert(!p2pmotor_disponivel());
  assert(strstr(p2pmotor_motivo_indisponivel(), "package built without the P2P engine"));
  assert(!p2p_ativo());
  assert(!p2p_usa_motor());
  char url[512] = "stale";
  assert(p2p_resolver("0123456789012345678901234567890123456789", 0, "", url, sizeof url) == P2P_ERR_DESLIGADO);
  assert(!url[0]);
#ifdef NV_ANDROID
  uint64_t ceiling = p2pmotor_teto_duro(GB(8), 0);
  assert(p2pmotor_cabe(MB(700), ceiling));
  assert(p2pmotor_cabe(GB(15), ceiling));
  assert(p2pmotor_janela(ceiling) >= MB(P2PM_JANELA_MIN_MB));
  assert(ceiling <= GB(8) / 2);
  assert(!p2pmotor_cabe(GB(4), p2pmotor_teto_duro(MB(300), 0)));
#else
  // Automatico = o de sempre: metade do livre, no maximo 1536 MB.
  assert(p2pmotor_teto_duro(MB(1379), 0) == MB(1379) / 2);   // a TV do #334: 689 MB
  assert(p2pmotor_teto_duro(GB(8), 0) == MB(1536));
  assert(p2pmotor_teto_duro(0, 0) == 0);
  // Fixo: ate o pedido, deixando 512 MB livres.
  assert(p2pmotor_teto_duro(GB(30), 2048) == GB(2));
  assert(p2pmotor_teto_duro(GB(30), 16384) == GB(16));
  assert(p2pmotor_teto_duro(GB(10), 16384) == GB(10) - MB(512));
  assert(p2pmotor_teto_duro(MB(1379), 4096) == MB(1379 - 512));  // #334: 867 MB, nao 4 GB
  // Nunca abaixo do Automatico (pouco livre: a reserva comeria tudo).
  assert(p2pmotor_teto_duro(MB(900), 2048) == MB(450));
  assert(p2pmotor_teto_duro(MB(400), 8192) == MB(200));
  assert(p2pmotor_teto_duro(MB(512), 8192) == MB(256));
  // Monotono na escolha: mais nunca da menos.
  { uint64_t l; unsigned e[] = { 0, 2048, 4096, 8192, 16384 }; int i;
    for (l = 0; l <= GB(40); l += MB(97))
      for (i = 1; i < 5; i++) assert(p2pmotor_teto_duro(l, e[i]) >= p2pmotor_teto_duro(l, e[i - 1]));
  }
  // #334, janela: teto duro menos 32 MB; arquivo maior que o teto toca se a
  // janela tiver ao menos 160 MB, senao e recusado antes de tocar.
  assert(p2pmotor_janela(MB(689)) == MB(657));               // a TV do #334
  assert(p2pmotor_janela(MB(20)) == 0);
  assert(p2pmotor_cabe(GB(15), p2pmotor_teto_duro(MB(1379), 0)));   // 4K de 15 GB na TV do #334: toca
  assert(p2pmotor_cabe(MB(600), MB(689)));                   // cabe inteiro: sem janela
  assert(p2pmotor_cabe(GB(4), p2pmotor_teto_duro(MB(461), 0)));     // LG 43AU801 (461 MB livres): toca
  assert(!p2pmotor_cabe(GB(4), p2pmotor_teto_duro(MB(300), 0)));     // 150 MB de teto: recusa
  assert(p2pmotor_cabe(MB(200), p2pmotor_teto_duro(MB(500), 0)));    // ... mas o que cabe toca
  assert(p2pmotor_cabe(GB(4), MB(192)) && !p2pmotor_cabe(GB(4), MB(191)));   // borda: 160 MB de janela
#endif
  puts("p2pmotor_teto: ok");
  return 0;
}
