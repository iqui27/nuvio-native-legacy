// Ajuste "Legenda ASS com estilo" no src/assrender.c de verdade, com libass e GL
// de mentira (tests/ass_pisca_gl.h), desenhado a 60 Hz como o player:
//  [1] dialogo comum no Automatico: o render fica 1:1.
//  [2] placa tipografada de 40 camadas com blur em "Sempre completo": o render
//      fica 1:1, como era antes do ajuste.
//  [3] a mesma placa no Automatico: o render desce e a placa aparece.
//  [4] "Texto simples": o libass sai da frente (assrender_ativo() = 0) e volta
//      na hora ao trocar o ajuste, sem recarregar a faixa.
#include "../src/assrender.h"
#include "../src/asspeso.h"
#include "../src/gfx.h"
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
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float raio, float borda,
              float bordaA, float cr, float cg, float cb, float a) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)raio; (void)borda; (void)bordaA;
  (void)cr; (void)cg; (void)cb; (void)a; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

#define CAB "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\nScaledBorderAndShadow: yes\n\n" \
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Inter Display,64,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,3,1.5,2,60,60,50,1\n" \
  "Style: Placa,Inter Display,90,&H0030C0FF,&H000000FF,&H00101010,&H80000000,-1,0,0,0,100,100,0,0,1,5,0,5,0,0,0,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"

static char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (b && fread(b, 1, (size_t)t, f) != (size_t)t) { free(b); b = NULL; }
  fclose(f); *n = (size_t)t; return b;
}

// A mesma placa de tests/ass_peso_bench.c: 40 camadas com blur, giro e escala.
static char *placa(void) {
  char *b = malloc(1 << 16); int i, n = sprintf(b, "%s", CAB);
  for (i = 0; i < 40; i++)
    n += sprintf(b + n, "Dialogue: %d,0:00:00.00,0:00:30.00,Placa,,0,0,0,,{\\an5\\move(%d,%d,%d,%d)\\blur%d\\bord%d\\frz%d"
                 "\\t(0,30000,\\frz%d\\fscx%d\\fscy%d)\\1a&H%02X&}LOJA DE DEPARTAMENTOS\n",
                 i, 500 + i * 6, 300 + i * 4, 1100 + i * 6, 640 + i * 4, 1 + i % 4, 2 + i % 5, i % 9 - 4,
                 i % 11 - 5, 100 + i % 20, 100 + i % 20, (i * 5) % 200);
  return b;
}

// Toca `seg` segundos a 60 Hz a partir de 1 s; devolve em quantos quadros o
// libass desenhou alguma coisa.
static int tocar(double seg, int *maiorDiv) {
  double t; int com = 0, d;
  *maiorDiv = 1;
  for (t = 1.0; t < 1.0 + seg; t += 1.0 / 60.0) {
    assrender_definir_layout(0, 0, 1920, 1080, 1920, 1080, 1.0);
    if (assrender_desenhar(t, 0, 1.0f, 0, 0, 1920, 1080) > 0) com++;
    d = assrender_peso_divisor(); if (d > *maiorDiv) *maiorDiv = d;
    usleep(16667);
  }
  return com;
}

int main(void) {
  static const char *DIALOGO = CAB
    "Dialogue: 0,0:00:00.00,0:00:30.00,Default,,0,0,0,,Voce nao devia ter voltado aqui.\\NEu avisei da ultima vez.\n";
  size_t tf; char *fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tf), *doc = placa();
  unsigned g = 3; int com, div;
  if (!fonte) { printf("fonte ausente\n"); return 2; }
  assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tf);

  printf("[1] dialogo comum, Automatico\n");
  assrender_definir_peso(ASSPESO_AUTO);
  assrender_geracao(g); ok(assrender_carregar(DIALOGO, strlen(DIALOGO), g) == 1, "dialogo carregado");
  com = tocar(2.0, &div);
  printf("    %d quadros com fala, divisor maximo %d\n", com, div);
  ok(com > 60 && div == 1, "dialogo aparece e o render fica 1:1");

  printf("[2] placa de 40 camadas, Sempre completo\n");
  assrender_definir_peso(ASSPESO_COMPLETO);
  assrender_geracao(++g); ok(assrender_carregar(doc, strlen(doc), g) == 1, "placa carregada");
  com = tocar(2.0, &div);
  printf("    %d quadros com placa, divisor maximo %d\n", com, div);
  ok(div == 1, "Sempre completo nunca reduz o render");

  printf("[3] a mesma placa, Automatico\n");
  assrender_definir_peso(ASSPESO_AUTO);
  { int antes = com; com = tocar(3.0, &div);
    printf("    %d quadros com placa (antes %d em 2 s), divisor maximo %d\n", com, antes, div);
    ok(div > 1, "o render desce de resolucao na placa pesada");
    ok(com > 60, "a placa aparece com o render reduzido"); }

  printf("[4] Texto simples\n");
  ok(assrender_ativo() == 1, "libass dono da legenda antes");
  assrender_definir_peso(ASSPESO_SIMPLES);
  ok(assrender_ativo() == 0, "Texto simples: o libass sai e o app desenha o texto");
  assrender_definir_peso(ASSPESO_AUTO);
  ok(assrender_ativo() == 1, "de volta ao Automatico sem recarregar a faixa");

  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "ass_peso: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
