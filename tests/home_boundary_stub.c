// Pure Home navigation fixtures keep scale and remote library services inert.
#include "gfx.h"
#include "menu.h"
#include "homeassistiu.h"
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
// home.c asks whether the top menu is mid-transition (22e53b627); idle here.
__attribute__((weak)) int menu_ocupado(Uint32 now) { (void)now; return 0; }
__attribute__((weak)) unsigned home_assistiu_passo(uint32_t agora) { (void)agora; return 0; }
__attribute__((weak)) int home_assistiu_itens(int saida[HOME_ASSISTIU_MAX]) { (void)saida; return 0; }
__attribute__((weak)) void home_assistiu_rotulo(char *saida, size_t tam) { if (tam) *saida = 0; }

#include "addons.h"
#include <stdio.h>
__attribute__((weak)) void addons_catalogo_chave(const char *id, const char *base,
    const char *type, const char *catalogue, char *dst, unsigned size) {
  (void)base; snprintf(dst, size, "%s_%s_%s", id, type, catalogue);
}

#include "proximo.h"
__attribute__((weak)) int prox_seguinte(const CatItem *ci, const CatEp *eps, int n, int pct, long long agora, ProxSugestao *out) {
  (void)ci; (void)eps; (void)n; (void)pct; (void)agora; (void)out; return 0;
}
