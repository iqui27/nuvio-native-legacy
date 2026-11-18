// TorBox burn-in (tbburn.h) for the fake-host tests that link src/video_tpk.c
// without the network: burn-in is never possible, so video_tpk.c behaves as
// before it existed.
#include "tbburn.h"
#include <stdint.h>
#include <string.h>
void tbburn_fonte(const char *u, const char *h, const char *a, uint64_t b) { (void)u; (void)h; (void)a; (void)b; }
void tbburn_ids(const char *u, int s, int i, int f) { (void)u; (void)s; (void)i; (void)f; }
int  tbburn_pode(void) { return 0; }
void tbburn_arquivo_tem_ass(int s) { (void)s; }
void tbburn_pedir(const char *u, int s, int a) { (void)u; (void)s; (void)a; }
int  tbburn_resultado(char *m, unsigned n, char *mo, unsigned mn) {
  if (m && n) m[0] = 0;
  if (mo && mn) mo[0] = 0;
  return TBB_NADA;
}
void tbburn_cancelar(void) {}
int  rede_torbox_em_voo(int *p) { if (p) *p = 0; return 0; }
int  ajustes_leg_queimar(void) { return 0; }
void tbburn_definir_escolha(int v) { (void)v; }
int  tbburn_memoria(TbFaixa *m) { (void)m; return 0; }
void tbburn_lembrar(const TbFaixa *f) { (void)f; }
int  tbburn_escolher_auto(const TbFaixa *f, int n, int mem, const TbFaixa *m, const char *p) {
  (void)f; (void)n; (void)mem; (void)m; (void)p; return -1;
}
// Weak: some of these tests link the real ones.
__attribute__((weak)) void mkvass_teto_conexoes(int n) { (void)n; }
__attribute__((weak)) const char *ling_legenda(void) { return ""; }
// HDR rule and original audio with burn-in (owner 13/11): never active here.
void tbburn_hdr_fonte(int s) { (void)s; }
int  tbburn_hdr_da_fonte(void) { return 0; }
int  tbburn_hdr_permitido(void) { return 0; }
void tbburn_hdr_lembrar(int s) { (void)s; }
__attribute__((weak)) int  tbburn_linha_visivel(void) { return 0; }
__attribute__((weak)) int  ajustes_tb_audio_modo(void) { return 0; }
__attribute__((weak)) void ajustes_definir_tb_audio_modo(int r) { (void)r; }
__attribute__((weak)) int  legref_em_rede(void) { return 0; }
#include "dts/tbaudio_tpk.h"
__attribute__((weak)) int  tba_disponivel(void) { return 0; }
__attribute__((weak)) int  tba_abrir(const char *t, const char *o, const char *c, int a, int m, char *out, size_t n) {
  (void)t; (void)o; (void)c; (void)a; (void)m; if (out && n) out[0] = 0; return 0;
}
__attribute__((weak)) void tba_fechar(void) {}
__attribute__((weak)) int  tba_modo(void) { return -1; }
__attribute__((weak)) int  tba_falhou(char *m, size_t n) { if (m && n) m[0] = 0; return 0; }
__attribute__((weak)) int  tba_n_rendicoes(void) { return 0; }
__attribute__((weak)) int  tba_rendicao_de(int o) { (void)o; return -1; }
__attribute__((weak)) void tba_definir_audio(int o) { (void)o; }
__attribute__((weak)) int  tba_audio(void) { return -1; }
__attribute__((weak)) void tba_log(void) {}
__attribute__((weak)) void tba_sockets_vigiar(const char *a, const char *b, const char *c, const char *d) { (void)a; (void)b; (void)c; (void)d; }
__attribute__((weak)) int  tba_sockets_contar(int *n) { if (n) *n = -1; return -1; }
__attribute__((weak)) int  tba_sockets_contar_por(int *n, int p[4]) { if (n) *n = -1; if (p) p[0] = p[1] = p[2] = p[3] = 0; return -1; }
// The burn-in subtitle clock (tbrelogio.h) and its head reader in video_tpk.c
// (tbcFio: TorBox's playlist + segment heads): never active here.
#include "tbrelogio.h"
#include "rede.h"
__attribute__((weak)) unsigned tbr_comecar(void) { return 1; }
__attribute__((weak)) unsigned tbr_geracao(void) { return 0; }
__attribute__((weak)) void tbr_encerrar(void) {}
__attribute__((weak)) int  tbr_ativo(void) { return 0; }
__attribute__((weak)) void tbr_segmentos(unsigned g, const double *d, int n) { (void)g; (void)d; (void)n; }
__attribute__((weak)) void tbr_pts(unsigned g, int s, double p) { (void)g; (void)s; (void)p; }
__attribute__((weak)) void tbr_ancorar(double a) { (void)a; }
__attribute__((weak)) int  tbr_falta(unsigned *g, int *s) { (void)g; (void)s; return 0; }
__attribute__((weak)) double tbr_correcao(void) { return 0; }
__attribute__((weak)) int  tba_pl_ler(const char *t, const char *b, TbaPl *pl) { (void)t; (void)b; (void)pl; return 0; }
__attribute__((weak)) void tba_pl_liberar(TbaPl *pl) { (void)pl; }
__attribute__((weak)) int  tba_ts_primeiro_pts(const unsigned char *b, size_t n, double *s) { (void)b; (void)n; (void)s; return 0; }
__attribute__((weak)) int  rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; if (r) memset(r, 0, sizeof *r); return 0; }
__attribute__((weak)) void rede_resposta_limpar(RedeResposta *r) { (void)r; }
__attribute__((weak)) void rede_fio_fechar(void) {}
// The Tizen 4-6 slow-TV rules (assperf.h, merged from feat/libass-tizen4):
// video_tpk.c sets them from the TV's version; no libass engine here.
__attribute__((weak)) void assperf_tv_lenta_definir(int sim) { (void)sim; }
