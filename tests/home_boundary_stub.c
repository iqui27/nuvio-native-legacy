// Pure Home navigation fixtures keep scale and remote library services inert.
#include "gfx.h"
static float scale = 1.0f;
float gfx_escala(void) { return scale; }
void gfx_escala_sair(float value) { scale = value; }
float gfx_escala_txt(void) { return 1.0f; }
float gfx_escala_tela_txt(void) { return 1.0f; }
float gfx_escala_txt_entrar(void) { return scale; }
float gfx_poster_k(void) { return 1.0f; }
unsigned addons_versao(void) { return 0; }
int contalib_tem_conta(void) { return 0; }
int trakt_ativo(void) { return 0; }
int simkl_ativo(void) { return 0; }
int gpun_efeito_ativo(int effect) { (void)effect; return 0; }
