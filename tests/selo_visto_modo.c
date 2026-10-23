// Selo de assistido em 3 modos: Desligado / Icone / Escrito (205). Modo 2 desenha
// o rotulo "Assistido"; o valor gravado antigo (V_LIGA: 0 ligado, 1 desligado)
// continua valendo: ligado vira Icone, desligado continua desligado.
#include <assert.h>
#include <string.h>
#include "../src/badges.c"

void gfx_vidro_ou_solido(GfxRect r, float radius, float density, float alpha, float red, float green, float blue, float solid) {
  (void)r; (void)radius; (void)density; (void)alpha; (void)red; (void)green; (void)blue; (void)solid;
}
static int modo, textos, discos, checks;
static char ultimo[64];
int ajustes_selo_visto_modo(void) { return modo; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1; return 0; }
const char *i18n(const char *s) { return s; }
void gfx_cor(GfxRect r, float rad, float R, float G, float B, float a) { (void)r; (void)rad; (void)R; (void)G; (void)B; (void)a; }
void gfx_rect(GfxRect r, GLuint t, GfxModo k, float f, float x, float y, float rad, float R, float G, float B, float a) { (void)r; (void)t; (void)f; (void)x; (void)y; (void)rad; (void)R; (void)G; (void)B; (void)a; if (k == GFX_DISCO) discos++; }
void gfx_icone(GfxRect r, const char *n, float R, float G, float B, float a) { (void)r; (void)R; (void)G; (void)B; (void)a; if (!strcmp(n, "check")) checks++; }
TxtLinha txt_linha(TxtEstilo est, const char *s, int r, int g, int b, int a) { TxtLinha l = { 0 }; (void)est; (void)r; (void)g; (void)b; (void)a; snprintf(ultimo, sizeof ultimo, "%s", s); l.w = 60; l.h = 12; return l; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; textos++; }

int main(void) {
  GfxRect card = { 0, 0, 200, 300 };
  modo = 1; badges_check_cartao(card, 0, 1);
  assert(discos > 0 && textos == 0);                 // icone: disco, sem palavra
  discos = checks = 0;
  modo = 2; badges_check_cartao(card, 0, 1);
  assert(textos == 1 && !strcmp(ultimo, "Assistido") && discos == 0 && checks == 1);   // escrito
  textos = 0; badges_check_cartao(card, 1, 1); assert(textos == 1);                    // cartao redondo tambem
  textos = 0; badges_check_cartao((GfxRect){ 0, 0, 120, 180 }, 0, 1); assert(textos == 0);   // estreito: so o check
  puts("selo_visto_modo: PASS");
  return 0;
}
