#!/bin/bash
# Painel lateral da colecao (dono, 13/10/2026: "o poster tinha antes, eu so pedi
# para tirar o poster quando a view e de poster"): o cartaz some na Grade, no
# Cover Flow, no Ranking E nas Decadas (dono, 14/10: "o ranking ainda ta
# mostrando o poster sendo que ele ja e poster"; "no modo ranking dos streaming
# ainda ta mostrando o poster, tem que tirar, decades tb") — nessas o logo
# segue valendo. As paginas de streaming sao a mesma tela e o mesmo painel
# (vertudo.c, grupo "Streaming"), entao vale o mesmo contrato. Com o cartaz na
# ficha (as outras exibicoes), a LOGO nao entra: so o nome escrito (dono, 14/10:
# "quando tem poster nas infos nao colocar a logo"). A Vitrine nao usa painel():
# o destaque dela e logo+meta+sinopse e o cartaz vem na fileira.
# O mesmo arquivo prova o TETO DO SCROLL do foco (dono, 14/10: "quando ta
# focado, fica cortado no top"): o topo do cartaz focado, com o crescimento e o
# anel, nunca entra por baixo do recorte do cabecalho.
set -eu
cd "$(dirname "$0")/.."
python3 -I - <<'PY'
from pathlib import Path
import re, subprocess, tempfile
s = Path('src/vertudo.c').read_text()
start = s.index('static int painelComCartaz(void)')
end = s.index('\nstatic void fundoColecao', start)
defs = '\n'.join(re.findall(r'^#define (?:VT_TOPO|VT_PAN_W|VT_PAN_X|VT_PAN_Y|VT_PAN_ART_H|VT_PAN_ART_W)\b.*$', s, re.M))
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned int GLuint;
typedef struct { float x,y,w,h; } GfxRect;
typedef struct { float w,h; } TxtLinha;
typedef struct { char nome[48]; } Pessoa;
typedef struct { char poster[64], backdrop[64], logo[64], titulo[64], genero[32], meta[32], sinopse[64],
  imdb[16], tipo[16], provNome[16], classificacao[8], direcao[48]; int nota; int nElenco; Pessoa elenco[3]; } CatItem;
typedef enum { GFX_CARD, GFX_MARCA, GFX_TEXTO } GfxModo;
typedef enum { TXT_HEADLINE, TXT_CAPTION, TXT_HERO_META, TXT_DET_META2, TXT_MINI, TXT_TITULO2, TXT_CALLOUT } TxtEstilo;
#define BADGE_H 28.0f
#define BADGE_NEUTRO 0
#define NV_TELA_W 1920.0f
#define NV_TELA_H 1080.0f
#define NV_ILHA_MARGEM_D 64.0f
float ajustes_raio_poster_px(void) { return 12.0f; }
static const void *collection = (void *)1;
static int foco;
enum { VT_LAY_AUTO, VT_LAY_GRADE, VT_LAY_LINHA, VT_LAY_RANKING, VT_LAY_SAGA, VT_LAY_DECADAS, VT_LAY_MURAL, VT_LAY_VITRINE, VT_LAY_FLUXO };
static int lay;
float gfx_tex_aspect_atual;
'''+defs+r'''
static CatItem item; static int hasLogo, hasPoster;
static int posterDraws, logoDraws; static float posterY, logoY;
static int viewItem(int i, CatItem *o) { (void)i; *o = item; return 1; }
GLuint tex_obter_logo_larg(const char *u, float w) { (void)u; (void)w; return hasLogo ? 9 : 0; }
GLuint tex_obter_larg(const char *u, float w) { (void)u; (void)w; return hasPoster ? 7 : 0; }
float tex_aspecto(const char *u) { return u[0] ? 2.0f : 0.0f; }
int tex_marca_escura(const char *u) { (void)u; return 0; }
void gfx_cor(GfxRect r, float rd, float a, float b, float c, float d) { (void)r;(void)rd;(void)a;(void)b;(void)c;(void)d; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float a, float b, float c, float rd, float e, float f, float g, float h) {
  (void)m;(void)a;(void)b;(void)c;(void)rd;(void)e;(void)f;(void)g;(void)h;
  if (t == 7) { posterDraws++; posterY = r.y; }
  if (t == 9) { logoDraws++; logoY = r.y; }
}
TxtLinha txt_linha_corta(int f, const char *t, int r, int g, int b, int a, float w) { (void)f;(void)t;(void)r;(void)g;(void)b;(void)a;(void)w; return (TxtLinha){10,10}; }
TxtLinha txt_linha(int f, const char *t, int r, int g, int b, int a) { (void)f;(void)t;(void)r;(void)g;(void)b;(void)a; return (TxtLinha){10,10}; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l;(void)x;(void)y;(void)a; }
float txt_bloco(int f, const char *t, int r, int g, int b, float x, float y, float w, float lh, float a, int n) { (void)f;(void)t;(void)r;(void)g;(void)b;(void)x;(void)y;(void)w;(void)lh;(void)a;(void)n; return lh; }
const char *badges_provedor(const char *n) { return n; }
float badges_desenhar(const char *p, float x, float y, float w, float h, float a) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)a; return 0; }
int imdbnota_obter(const char *i, int n, int s) { (void)i;(void)n;(void)s; return 0; }
void idioma_decimal_texto(char *n, int l) { (void)n;(void)l; }
int ajustes_idioma(void) { return 0; }
float txt_bloco_corta(int f, const char *t, int r, int g, int b, float x, float y, float w, float lh, float a, int n) { (void)f;(void)t;(void)r;(void)g;(void)b;(void)x;(void)y;(void)w;(void)lh;(void)a;(void)n; return 10.0f; }
TxtEstilo sinopse_estilo(TxtEstilo f) { return f; }
float sinopse_entrelinha(TxtEstilo f, float lh) { (void)f; return lh; }
float badge_imdb(float x, float y, int n, int k, float a) { (void)x;(void)y;(void)n;(void)k;(void)a; return 0; }
void badge_desenhar(float x, float y, const char *t, int c, float a) { (void)x;(void)y;(void)t;(void)c;(void)a; }
const char *i18n(const char *s) { return s; }
'''+s[start:end]+r'''
static void run(int logo, int poster) {
  memset(&item, 0, sizeof item);
  strcpy(item.titulo, "Deadpool"); strcpy(item.poster, "p.jpg");
  if (logo) strcpy(item.logo, "l.png");
  hasLogo = logo; hasPoster = poster; posterDraws = logoDraws = 0;
  painel(1.0f);
}
int main(void) {
  for (lay = VT_LAY_GRADE; lay <= VT_LAY_MURAL; lay++) {
    if (lay == VT_LAY_GRADE || lay == VT_LAY_RANKING || lay == VT_LAY_DECADAS) {
      // Exibicao de cartazes: o cartaz ja esta na celula, o painel so da as
      // infos — e a logo vale. Decadas e poster na celula tambem (streaming
      // e colecoes, a mesma tela).
      run(1, 1); assert(posterDraws == 0 && logoDraws == 1);
      run(0, 1); assert(posterDraws == 0 && logoDraws == 0);
      continue;
    }
    // Com o cartaz na ficha NAO ha logo: so o nome escrito (dono, 14/10).
    run(1, 1);
    assert(posterDraws == 1);
    assert(logoDraws == 0);
    run(0, 1);   // sem logo: cartaz + titulo em texto
    assert(posterDraws == 1 && logoDraws == 0);
  }
  return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    src = Path(d)/'painel.c'; binf = Path(d)/'painel'
    src.write_text(code)
    subprocess.run(['cc', str(src), '-o', str(binf), '-lm'], check=True)
    subprocess.run([str(binf)], check=True)

# TETO DO SCROLL DO FOCO: a linha em foco nunca sobe alem do recorte do
# cabecalho com o cartaz crescido/anelado inteiro visivel.
m = re.search(r'static float tetoScrollFoco\([^)]*\)\s*\{[^}]*\}', s)
assert m, 'missing tetoScrollFoco'
assert 'tetoScrollFoco(celY[foco], celH[foco])' in s, 'focus scroll ignores the ceiling'
code2 = r'''
#include <assert.h>
#include <math.h>
#define FOCO_ZOOM 0.06f
#define NV_ANEL_FOCO 4.0f
static float perto(float a, float b) { float d = a - b; return d < 0 ? -d : d; }
''' + m.group(0) + r'''
int main(void) {
  // 1a linha da Grade (cartaz de 372): teto negativo — scroll fica em zero e o
  // topo do foco (zoom de 11.16 + anel de 4) cabe nos 12 px acima do VT_TOPO.
  float t0 = tetoScrollFoco(0.0f, 372.0f);
  assert(perto(t0, 12.0f - (372.0f * FOCO_ZOOM * 0.5f + NV_ANEL_FOCO)) < 0.01f);
  assert(t0 < 0.0f);
  // Linha de baixo: mesmo teto relativo, deslocado da linha.
  float t1 = tetoScrollFoco(416.0f, 372.0f);
  assert(perto(t1, t0 + 416.0f) < 0.01f);
  // Cartaz maior (Ranking e mural deitados nao chegam; o teto cobre qualquer h).
  assert(tetoScrollFoco(0.0f, 1000.0f) < 0.0f);
  return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    src = Path(d)/'teto.c'; binf = Path(d)/'teto'
    src.write_text(code2)
    subprocess.run(['cc', str(src), '-o', str(binf), '-lm'], check=True)
    subprocess.run([str(binf)], check=True)
print('vertudo_painel_cartaz: PASS')
PY
