// Pure Home navigation fixtures keep scale and remote library services inert.
#include "gfx.h"
static float scale = 1.0f;
__attribute__((weak)) float gfx_escala(void) { return scale; }
__attribute__((weak)) void gfx_escala_sair(float value) { scale = value; }
__attribute__((weak)) float gfx_escala_txt(void) { return 1.0f; }
__attribute__((weak)) float gfx_escala_tela_txt(void) { return 1.0f; }
__attribute__((weak)) float gfx_escala_txt_entrar(void) { return scale; }
__attribute__((weak)) float gfx_poster_k(void) { return 1.0f; }
__attribute__((weak)) unsigned addons_versao(void) { return 0; }
__attribute__((weak)) int contalib_tem_conta(void) { return 0; }
__attribute__((weak)) int trakt_ativo(void) { return 0; }
__attribute__((weak)) int simkl_ativo(void) { return 0; }
__attribute__((weak)) int gpun_efeito_ativo(int effect) { (void)effect; return 0; }

#include "glance.h"
#include "proximo.h"
__attribute__((weak)) void glance_abrir(const GlanceFonte *f, int i) { (void)f; (void)i; }
__attribute__((weak)) int prox_seguinte(const CatItem *ci, const CatEp *eps, int n, int pct, long long agora, ProxSugestao *out) {
  (void)ci; (void)eps; (void)n; (void)pct; (void)agora; (void)out; return 0;
}
