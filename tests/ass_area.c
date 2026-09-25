// ASS na AREA DO VIDEO, nao na tela (25/09/2026). O src/assrender.c de
// verdade, libass do Homebrew e GL de mentira (tests/ass_pisca_gl.h); o
// gfx_rect do teste guarda a caixa de tudo que foi desenhado.
//  [1] 4:3 pillarbox: placa \an7\pos(64,48) cai DENTRO da imagem (x >= 240).
//      Antes, com o quadro 1920x1080 fixo, caia em x=195, na barra preta.
//  [2] 2.39:1 letterbox: a mesma placa cai abaixo do topo da imagem (y >= 140).
//  [3] troca de layout com o video PARADO (mesmo instante) move a legenda.
//  [4] escala de fonte 1,5 deixa a fala ~1,5x mais larga, mesmo centro.
#include "../src/assrender.h"
#include "../src/gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void teste_glGenTextures(GLsizei n, GLuint *t) { static GLuint prox = 1; int i; for (i = 0; i < n; i++) t[i] = prox++; }
void teste_glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
void teste_glBindTexture(GLenum a, GLuint t) { (void)a; (void)t; }
void teste_glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  (void)a; (void)b; (void)c; (void)w; (void)h; (void)d; (void)e; (void)f; (void)p; }
void teste_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint tex) { (void)tex; }

static float cx0, cy0, cx1, cy1;
static int desenhos;
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float raio, float borda,
              float bordaA, float cr, float cg, float cb, float a) {
  (void)tex; (void)modo; (void)foco; (void)raio; (void)borda; (void)bordaA;
  (void)cr; (void)cg; (void)cb; (void)a;
  if (!desenhos++) { cx0 = r.x; cy0 = r.y; cx1 = r.x + r.w; cy1 = r.y + r.h; return; }
  if (r.x < cx0) cx0 = r.x;
  if (r.y < cy0) cy0 = r.y;
  if (r.x + r.w > cx1) cx1 = r.x + r.w;
  if (r.y + r.h > cy1) cy1 = r.y + r.h;
}

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

#define CAB(rx, ry) "[Script Info]\nScriptType: v4.00+\nPlayResX: " #rx "\nPlayResY: " #ry "\nScaledBorderAndShadow: yes\n\n" \
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Inter Display,36,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,2,10,10,20,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
static const char *DOC43 = CAB(640, 480)
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\an7\\pos(64,48)}PLACA\n"
  "Dialogue: 0,0:00:05.00,0:00:07.00,Default,,0,0,0,,Fala comum de teste\n";
static const char *DOC239 = CAB(1920, 800)
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\an7\\pos(192,80)}PLACA\n";

static char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (b && fread(b, 1, (size_t)t, f) != (size_t)t) { free(b); b = NULL; }
  fclose(f); *n = (size_t)t; return b;
}

/* Desenha a 60 Hz ate a caixa estabilizar em `t` (o worker e assincrono). */
static int caixa(double t) {
  int k, n = 0;
  float px0 = -1, py0 = -1;
  for (k = 0; k < 60; k++) {
    desenhos = 0;
    n = assrender_desenhar(t, 0, 1.0f, 0, 0, 1920, 1080);
    if (n > 0 && k > 3 && fabsf(cx0 - px0) < 0.5f && fabsf(cy0 - py0) < 0.5f) break;
    if (n > 0) { px0 = cx0; py0 = cy0; }
    usleep(16667);
  }
  return n;
}

int main(void) {
  size_t tf; char *fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tf);
  unsigned g = 11;
  float larg1, centro1;
  if (!fonte) { printf("fonte ausente\n"); return 2; }
  assrender_geracao(g); assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tf);

  printf("[1] 4:3 na tela 16:9: video em x 240..1680\n");
  ok(assrender_carregar(DOC43, strlen(DOC43), g) == 1, "documento 4:3 carregado");
  assrender_definir_layout(240, 0, 1440, 1080, 640, 480, 1.0);
  ok(caixa(2.0) > 0, "placa desenhada");
  printf("    placa em x %.0f..%.0f y %.0f..%.0f\n", cx0, cx1, cy0, cy1);
  ok(cx0 >= 240.0f, "placa dentro da imagem (antes: x=195, na barra preta)");
  ok(fabsf(cx0 - (240.0f + 1440.0f * 0.1f)) < 24.0f, "placa a ~10% da largura DO VIDEO");

  printf("[3] video parado em 2,0 s, layout muda para tela inteira\n");
  { float antes = cx0;
    assrender_definir_layout(0, 0, 1920, 1080, 1920, 1080, 1.0);
    caixa(2.0);
    printf("    placa de x %.0f para x %.0f\n", antes, cx0);
    ok(fabsf(cx0 - antes) > 50.0f, "legenda seguiu o layout sem o relogio andar"); }

  printf("[4] escala de fonte\n");
  assrender_definir_layout(240, 0, 1440, 1080, 640, 480, 1.0);
  ok(caixa(6.0) > 0, "fala desenhada em escala 1,0");
  larg1 = cx1 - cx0; centro1 = (cx0 + cx1) * 0.5f;
  assrender_definir_layout(240, 0, 1440, 1080, 640, 480, 1.5);
  caixa(6.0);
  printf("    largura %.0f -> %.0f (x%.2f), centro %.0f -> %.0f\n",
         larg1, cx1 - cx0, (cx1 - cx0) / larg1, centro1, (cx0 + cx1) * 0.5f);
  ok((cx1 - cx0) / larg1 > 1.35f && (cx1 - cx0) / larg1 < 1.65f, "escala 1,5 deixa a fala ~1,5x mais larga");
  ok(fabsf((cx0 + cx1) * 0.5f - centro1) < 8.0f, "a fala continua centrada no video");

  printf("[2] 2.39:1: video em y 140..940\n");
  assrender_geracao(++g);
  ok(assrender_carregar(DOC239, strlen(DOC239), g) == 1, "documento 2.39 carregado");
  assrender_definir_layout(0, 140, 1920, 800, 1920, 800, 1.0);
  ok(caixa(2.0) > 0, "placa desenhada");
  printf("    placa em x %.0f..%.0f y %.0f..%.0f\n", cx0, cx1, cy0, cy1);
  ok(cy0 >= 140.0f, "placa abaixo do topo da imagem (antes: y=120, na barra preta)");

  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "ass_area: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
