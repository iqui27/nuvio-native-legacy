// Regression: material buckets use degrees, and failed loads preserve artwork tint.
// bash tests/textura_fraca.sh
#define tex_obter_larg_qualquer test_texture_load
#define tex_obter_hero test_texture_hero
#define tex_aspecto test_texture_aspect
#define gfx_textura_definir test_texture_define
#include "../src/ajustes.c"
#undef tex_obter_larg_qualquer
#undef tex_obter_hero
#undef tex_aspecto
#undef gfx_textura_definir
#include <assert.h>

static char requested[700];
static int load_ok = 1, texture_enabled;
static float aspect = 1.0f;

GLuint test_texture_load(const char *path, float width) {
  FILE *file;
  (void)width;
  snprintf(requested, sizeof requested, "%s", path);
  file = fopen(path, "rb");
  if (!file) return 0;
  fclose(file);
  return load_ok ? 1 : 0;
}
GLuint test_texture_hero(const char *path) { return test_texture_load(path, 0); }
float test_texture_aspect(const char *path) { (void)path; return aspect; }
void test_texture_define(GLuint tex, const float *window, float asp,
                         float strength, float veil, int dark) {
  (void)window; (void)asp; (void)strength; (void)veil; (void)dark;
  texture_enabled = tex != 0;
}

static void reset_texture(float strength) {
  CorvivaTextura *T = &nv_textura_viva;
  memset(T, 0, sizeof *T);
  T->ok = T->fraca = T->tintaBranca = 1;
  T->forca = strength;
  T->base[0] = T->base[1] = T->base[2] = 0.25f;
  strcpy(T->url, "artwork");
}

int main(void) {
  CorvivaTextura *T = &nv_textura_viva;
  char expected[700];
  int m, failure;
  ajustes_texturas_dir("deploy/app/art");
  valor[AJ_TEMA] = AJ_TEMA_TEXTURA;
  nv_acento_viva[0] = 1.0f;
  nv_acento_viva[1] = nv_acento_viva[2] = 0.0f;
  // Pure red maps to bucket 1, whose bundled asset is material-030.jpg.
  reset_texture(1.0f);
  ajustes_textura_quadro();
  assert(!strcmp(requested, "deploy/app/art/texturas/material-030.jpg"));
  assert(texture_enabled && !T->tintaBranca);
  assert(ajustes_acento_tinta(NULL, NULL, NULL) == AJ_TINTA_ESCURA);

  for (m = 0; m < 12; m++) {
    float angle = m * 30.0f / 57.29578f;
    float lab[3] = { 0.65f, 0.08f * cosf(angle), 0.08f * sinf(angle) };
    corviva_oklab_para_srgb(lab, nv_acento_viva);
    reset_texture(1.0f);
    ajustes_textura_quadro();
    snprintf(expected, sizeof expected, "deploy/app/art/texturas/material-%03d.jpg", m * 30);
    assert(!strcmp(requested, expected));
    assert(texture_enabled && !T->tintaBranca);
  }
  // A pending/missing texture or invalid aspect must leave tint and veil intact.
  for (failure = 0; failure < 3; failure++) {
    load_ok = failure != 0;
    aspect = failure == 1 ? 0.0f : 1.0f;
    ajustes_texturas_dir(failure == 2 ? "/tmp/nao-existe-nvio-textura" : "deploy/app/art");
    reset_texture(1.0f);
    T->veu = 0.17f;
    ajustes_textura_quadro();
    assert(!texture_enabled && T->tintaBranca && T->veu == 0.17f);
    assert(ajustes_acento_tinta(NULL, NULL, NULL) == 1.0f);
  }
  ajustes_texturas_dir("deploy/app/art");
  aspect = 1.0f;
  valor[AJ_TEMA] = AJ_TEMA_TEXTURA_SUTIL;
  nv_acento_viva[0] = nv_acento_viva[1] = nv_acento_viva[2] = 1.0f;
  reset_texture(0.35f);
  ajustes_textura_quadro();
  assert(texture_enabled && T->tintaBranca && T->veu >= 0.05f);
  nv_acento_viva[0] = 1.0f;
  nv_acento_viva[1] = nv_acento_viva[2] = 0.0f;
  reset_texture(0.35f);
  ajustes_textura_quadro();
  assert(texture_enabled && T->tintaBranca && T->veu < 0.05f);
  puts("ok: 12 material filenames, load failures preserve tint, per-asset veil");
  return 0;
}
