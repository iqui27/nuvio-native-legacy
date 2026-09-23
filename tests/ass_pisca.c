// "Aparece piscando" e "atrasada" (#92, C9 em 23/09/2026). O src/assrender.c
// de verdade, com o libass do Homebrew e GL de mentira (tests/ass_pisca_gl.h),
// desenhado a 60 Hz como o player:
//  [1] relogio aos degraus de ~200 ms (o currentTime cru da C9: 197..247 ms,
//      77 % acima de 200): nenhum quadro vazio no meio de uma fala. Antes o
//      salto > 200 ms apagava o quadro — piscava a cada evento.
//  [2] um lote novo do mkvass (legenda_atualizar_corpo -> assrender_atualizar)
//      no meio da fala nao apaga a tela. Antes cada lote recarregava tudo.
//  [3] a fala entra no maximo ~2 quadros depois do Start com relogio fino.
//  [4] textura so e reenviada quando o quadro muda, nao a cada quadro.
//  [5] seek (salto > 1,5 s) ainda limpa.
#include "../src/assrender.h"
#include "../src/gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static long uploads, gerados;
void teste_glGenTextures(GLsizei n, GLuint *t) { static GLuint prox = 1; int i; for (i = 0; i < n; i++) t[i] = prox++; gerados += n; }
void teste_glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
void teste_glBindTexture(GLenum a, GLuint t) { (void)a; (void)t; }
void teste_glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  (void)a; (void)b; (void)c; (void)w; (void)h; (void)d; (void)e; (void)f; (void)p; uploads++; }
void teste_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint tex) { (void)tex; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float raio, float borda,
              float bordaA, float cr, float cg, float cb, float a) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)raio; (void)borda; (void)bordaA;
  (void)cr; (void)cg; (void)cb; (void)a; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

#define CAB "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 360\n\n" \
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Inter Display,28,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
// Duas falas coladas (como fansub faz): 1,000-3,000 e 3,000-6,000.
static const char *DOC1 = CAB
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Primeira fala\n"
  "Dialogue: 0,0:00:03.00,0:00:06.00,Default,,0,0,0,,Segunda fala colada\n";
// O lote seguinte: as mesmas e mais uma la na frente.
static const char *DOC2 = CAB
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Primeira fala\n"
  "Dialogue: 0,0:00:03.00,0:00:06.00,Default,,0,0,0,,Segunda fala colada\n"
  "Dialogue: 0,0:00:20.00,0:00:22.00,Default,,0,0,0,,Terceira, do lote novo\n";

static char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (b && fread(b, 1, (size_t)t, f) != (size_t)t) { free(b); b = NULL; }
  fclose(f); *n = (size_t)t; return b;
}

static void quadro(void) { usleep(16667); }

int main(void) {
  size_t tf; char *fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tf);
  double t; int vazios = 0, comTexto = 0, primeiro = -1, k, n;
  unsigned g = 7;
  long quadros = 0;
  if (!fonte) { printf("fonte ausente\n"); return 2; }
  assrender_geracao(g); assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tf);
  ok(assrender_carregar(DOC1, strlen(DOC1), g) == 1, "documento carregado");

  printf("[1] relogio aos degraus de 200-247 ms, 60 Hz, de 0,8 s a 5,8 s\n");
  printf("[2] lote novo (assrender_atualizar) em 2,0 s, no meio da primeira fala\n");
  { double degrau = 0.8, prox = 0.8; int atualizou = 0; srand(92);
    for (t = 0.8; t < 5.8; t += 1.0 / 60.0) {
      if (t >= prox) { degrau = prox; prox += 0.197 + (rand() % 51) / 1000.0; }
      if (!atualizou && t >= 2.0) {
        ok(assrender_atualizar(DOC2, strlen(DOC2), g) == 1, "lote novo aceito na mesma geracao");
        atualizou = 1;
      }
      n = assrender_desenhar(degrau, 0, 1.0f, 0, 0, 1920, 1080);
      quadros++;
      if (degrau >= 1.25 && degrau < 5.75) { if (n > 0) comTexto++; else vazios++; }
      quadro();
    } }
  printf("    %d quadros com fala, %d vazios no meio das falas\n", comTexto, vazios);
  ok(vazios == 0, "nenhum quadro vazio entre 1,25 s e 5,75 s (antes: pisca a cada degrau)");

  printf("[3] relogio fino: a fala entra perto do Start\n");
  assrender_geracao(++g); assrender_carregar(DOC2, strlen(DOC2), g);
  for (k = 0, t = 19.5; t < 20.5; t += 1.0 / 60.0, k++) {
    n = assrender_desenhar(t, 0, 1.0f, 0, 0, 1920, 1080);
    if (n > 0 && primeiro < 0) primeiro = k;
    quadro();
  }
  { double entrou = 19.5 + primeiro / 60.0;
    printf("    Start 20,000 s; primeira imagem no quadro de %.3f s (%+.0f ms)\n", entrou, (entrou - 20.0) * 1000);
    ok(primeiro >= 0 && entrou - 20.0 <= 0.050, "entrou ate 50 ms (3 quadros) depois do Start"); }

  printf("[4] textura so muda quando o quadro muda\n");
  { long u0 = uploads, q = 0;
    for (t = 20.6; t < 21.6; t += 1.0 / 60.0) { assrender_desenhar(t, 0, 1.0f, 0, 0, 1920, 1080); quadro(); q++; }
    printf("    %ld quadros parados na mesma fala: %ld envios de textura\n", q, uploads - u0);
    ok(uploads - u0 <= 4, "fala parada: no maximo 4 envios em 1 s (antes: 1 por imagem por quadro)"); }

  printf("[5] seek de 21,6 s para 2,0 s limpa o quadro velho\n");
  n = assrender_desenhar(2.0, 0, 1.0f, 0, 0, 1920, 1080);
  ok(n == 0, "no quadro do salto, nada da fala de 21 s");
  for (k = 0; k < 30 && n <= 0; k++) { quadro(); n = assrender_desenhar(2.0 + k / 60.0, 0, 1.0f, 0, 0, 1920, 1080); }
  ok(n > 0, "a fala de 2,0 s aparece logo depois do salto");
  (void)quadros;
  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "ass_pisca: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
