// Matriz YCbCr das legendas ASS e fonte entregue depois (16/11/2026). O
// src/assrender.c de verdade (caminho novo do .tpk, NV_ASS_NOVO), libass do
// sistema e GL de mentira com as texturas na memoria (tests/ass_matriz_gl.h).
// Uma textura por imagem (o atlas saiu em 16/11/2026: mais lento na TV).
//  [3] fala parada: nada sobe de novo;
//  [6] matriz: script TV.601 num video BT.709 converte como o mpv/JASSUB
//      (vermelho puro -> G 0,0965); branco fica branco; "None", HDR e a cor
//      escolhida pela pessoa nao mudam;
//  [7] fonte entregue DEPOIS (fontes sob demanda: um lote novo do mkvass
//      traz um \fn): com a faixa ja desenhando, o libass passa a usa-la e o
//      evento sai igual ao de uma carga com a fonte desde o inicio.
#include "../src/assrender.h"
#include "../src/gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- GL de mentira ---- */
typedef struct { int w, h; unsigned char *px; } Tex;
static Tex texs[4096];
static GLuint ligada, prox = 1;
static int alinhamento = 4, maxTex = 2048;   // glGetIntegerv
static long subidas;   // glTexImage2D com pixels + glTexSubImage2D
void teste_glGenTextures(GLsizei n, GLuint *t) { int i; for (i = 0; i < n; i++) t[i] = prox++; }
void teste_glDeleteTextures(GLsizei n, const GLuint *t) {
  int i; for (i = 0; i < n; i++) if (t[i] < 4096) { free(texs[t[i]].px); memset(&texs[t[i]], 0, sizeof(Tex)); } }
void teste_glBindTexture(GLenum a, GLuint t) { (void)a; ligada = t; }
void teste_glPixelStorei(GLenum a, GLint v) { (void)a; alinhamento = v; }
void teste_glGetIntegerv(GLenum a, GLint *v) { (void)a; *v = maxTex; }
GLenum teste_glGetError(void) { return 0; }
void teste_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
static size_t passo(int w) { return ((size_t)w + (size_t)alinhamento - 1) / (size_t)alinhamento * (size_t)alinhamento; }
void teste_glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  Tex *t = &texs[ligada]; int y;
  (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
  free(t->px); t->w = w; t->h = h; t->px = malloc((size_t)w * (size_t)h);
  memset(t->px, 0xAA, (size_t)w * (size_t)h);   // indefinido no GL: lixo visivel
  if (p) { subidas++; for (y = 0; y < h; y++) memcpy(t->px + (size_t)y * w, (const unsigned char *)p + (size_t)y * passo(w), (size_t)w); }
}
void teste_glTexSubImage2D(GLenum a, GLint b, GLint x, GLint y0, GLsizei w, GLsizei h, GLenum e, GLenum f, const void *p) {
  Tex *t = &texs[ligada]; int y;
  (void)a; (void)b; (void)e; (void)f;
  subidas++;
  if (x < 0 || y0 < 0 || x + w > t->w || y0 + h > t->h) { printf("FALHA glTexSubImage2D fora da textura\n"); exit(1); }
  for (y = 0; y < h; y++) memcpy(t->px + (size_t)(y0 + y) * t->w + x, (const unsigned char *)p + (size_t)y * passo(w), (size_t)w);
}

/* ---- gfx de mentira: o que foi desenhado ---- */
typedef struct { float x, y, w, h, r, g, b, a; int pw, ph; unsigned char *px; } Img;
static Img cap[20000];
static int nCap, nRects;
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint tex) { (void)tex; }
int gfx_grupo_comecar(GfxGrupo *g, GfxRect caixa, float alpha) { (void)g; (void)caixa; (void)alpha; return 0; }
void gfx_grupo_terminar(GfxGrupo *g, float alpha) { (void)g; (void)alpha; }
static void capturar(float x, float y, float w, float h, float r, float g, float b, float a,
                     const Tex *t, int tx, int ty, int pw, int ph) {
  Img *m = &cap[nCap++]; int j;
  m->x = x; m->y = y; m->w = w; m->h = h; m->r = r; m->g = g; m->b = b; m->a = a; m->pw = pw; m->ph = ph;
  m->px = malloc((size_t)pw * (size_t)ph);
  for (j = 0; j < ph; j++) memcpy(m->px + (size_t)j * pw, t->px + (size_t)(ty + j) * t->w + tx, (size_t)pw);
}
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  (void)modo; (void)foco; (void)parx; (void)pary; (void)raio;
  nRects++;
  capturar(r.x, r.y, r.w, r.h, cr, cg, cb, ca, &texs[tex], 0, 0, texs[tex].w, texs[tex].h);
}
static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

static void limpar_cap(void) { int i; for (i = 0; i < nCap; i++) free(cap[i].px); nCap = nRects = 0; }

/* Pausado no mesmo instante (sem render ahead) ate o worker entregar. */
static int parado(double t) {
  int k, n = 0;
  for (k = 0; k < 40; k++) { limpar_cap(); n = assrender_desenhar(t, 0, 1.0f, 0, 0, 1920, 1080); usleep(8000); }
  limpar_cap();
  n = assrender_desenhar(t, 0, 1.0f, 0, 0, 1920, 1080);
  return n;
}

static int cmp_img(const void *a, const void *b) {
  const Img *p = a, *q = b;
  if (p->y != q->y) return p->y < q->y ? -1 : 1;
  if (p->x != q->x) return p->x < q->x ? -1 : 1;
  if (p->pw != q->pw) return p->pw - q->pw;
  if (p->ph != q->ph) return p->ph - q->ph;
  return memcmp(p->px, q->px, (size_t)p->pw * (size_t)p->ph);
}

#define CAB(matriz) "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n" matriz "\n" \
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Inter Display,56,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,3,2,2,20,20,40,1\n" \
  "Style: Sign,Inter Display,48,&H000000FF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
#define EVENTOS \
  "Dialogue: 0,0:00:01.00,0:00:05.00,Default,,0,0,0,,Uma fala com borda e sombra\\Nem duas linhas\n" \
  "Dialogue: 1,0:00:01.00,0:00:05.00,Sign,,0,0,0,,{\\pos(200,150)}LETREIRO\n" \
  "Dialogue: 1,0:00:01.00,0:00:05.00,Sign,,0,0,0,,{\\pos(1300,400)\\blur3\\c&HFFFFFF&}placa branca\n"

static char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (b && fread(b, 1, (size_t)t, f) != (size_t)t) { free(b); b = NULL; }
  fclose(f); *n = (size_t)t; return b;
}

static unsigned g = 3;
static void carregar(const char *doc) {
  assrender_geracao(++g);
  assrender_limpar();
  assrender_aplicar_invalidacao();
  if (assrender_carregar(doc, strlen(doc), g) != 1) { printf("FALHA carregar\n"); exit(1); }
}

/* A imagem vermelha (letreiro) do quadro capturado. */
static const Img *vermelha(void) {
  int i; const Img *m = NULL;
  for (i = 0; i < nCap; i++) if (cap[i].r > 0.9f && cap[i].b < 0.1f && cap[i].w > 50 && (!m || cap[i].w > m->w)) m = &cap[i];
  return m;
}
static const Img *branca(void) {
  int i;
  for (i = 0; i < nCap; i++) if (cap[i].x > 1200 && cap[i].r > 0.9f && cap[i].g > 0.9f) return &cap[i];
  return NULL;
}

int main(void) {
  size_t tf; char *fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tf);
  int n, i;
  if (!fonte) { printf("fonte ausente\n"); return 2; }
  assrender_geracao(g); assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tf);
  assrender_definir_cor_video(1, 1, 0, 1080);   // BT.709, faixa TV, SDR

  printf("[3] fala parada nao sobe de novo\n");
  carregar(CAB("YCbCr Matrix: TV.601\n") EVENTOS);
  n = parado(2.0);
  printf("    %d imagens, %d gfx_rect\n", n, nRects);
  ok(n > 3 && nRects == n && nCap == n, "cada imagem num gfx_rect");
  { long s0 = subidas; int k;
    for (k = 0; k < 30; k++) { limpar_cap(); assrender_desenhar(2.0, 0, 1.0f, 0, 0, 1920, 1080); }
    printf("    30 quadros parados: %ld subidas\n", subidas - s0);
    ok(subidas == s0 && nRects == n, "nenhuma subida, as mesmas imagens"); }

  printf("[6] matriz YCbCr\n");
  parado(2.0);
  { const Img *v = vermelha(), *b = branca();
    ok(v != NULL && b != NULL, "letreiro vermelho e placa branca no quadro");
    if (v) printf("    TV.601 -> BT.709: vermelho puro sai (%.4f, %.4f, %.4f)\n", v->r, v->g, v->b);
    /* JASSUB COLOR_CONVERSIONS['BT.601 TV']['BT.709 TV'], primeira coluna:
     * 1.0864, 0.096546, -0.014106 (saturados em 0..1). */
    ok(v && fabsf(v->r - 1.0f) < 1e-3f && fabsf(v->g - 0.096546f) < 2e-3f && v->b < 1e-3f,
       "vermelho como o JASSUB/mpv (G 0,0965)");
    ok(b && b->r > 0.999f && b->g > 0.999f && b->b > 0.999f, "branco continua branco"); }
  carregar(CAB("YCbCr Matrix: TV.709\n") EVENTOS);
  parado(2.0);
  { const Img *v = vermelha(); ok(v && v->g < 1e-6f && v->r > 0.999f, "TV.709 num video BT.709: nada muda"); }
  carregar(CAB("YCbCr Matrix: None\n") EVENTOS);
  parado(2.0);
  { const Img *v = vermelha(); ok(v && v->g < 1e-6f, "\"None\": nada muda"); }
  carregar(CAB("") EVENTOS);
  parado(2.0);
  { const Img *v = vermelha(); ok(v && fabsf(v->g - 0.096546f) < 2e-3f, "sem cabecalho: BT.601 TV, como o VSFilter/mpv"); }
  assrender_definir_cor_video(9, 1, 1, 2160);   // HDR BT.2020
  parado(2.0);
  { const Img *v = vermelha(); ok(v && v->g < 1e-6f, "video HDR: nada muda"); }
  assrender_definir_cor_video(0, 0, 0, 480);    // nao declarada, SD: BT.601 = o script
  parado(2.0);
  { const Img *v = vermelha(); ok(v && v->g < 1e-6f, "video SD sem matriz declarada (BT.601): nada muda"); }
  assrender_definir_cor_video(1, 1, 0, 1080);
  assrender_definir_cor(1, 0, 255, 0);           // a pessoa escolheu verde para o dialogo
  parado(2.0);
  { int achou = 0;
    for (i = 0; i < nCap; i++) if (cap[i].g > 0.999f && cap[i].r < 1e-6f && cap[i].b < 1e-6f) achou = 1;
    ok(achou, "a cor escolhida pela pessoa sai exata (sem conversao)"); }
  assrender_definir_cor(0, 0, 0, 0);

  printf("[7] fonte entregue com a faixa ja ativa\n");
  { size_t tr; char *rob = ler("deploy/app/fonts/Roboto-Regular.ttf", &tr);
    static Img tarde[64], cedo[64], inter[64]; int nt, nc, ni, iguais = 1, difInter = 0;
    const char *A = CAB("YCbCr Matrix: TV.709\n")
      "Dialogue: 0,0:00:01.00,0:00:05.00,Default,,0,0,0,,primeiro lote sem roboto\n";
    const char *B = CAB("YCbCr Matrix: TV.709\n")
      "Dialogue: 0,0:00:01.00,0:00:05.00,Default,,0,0,0,,primeiro lote sem roboto\n"
      "Dialogue: 0,0:00:30.00,0:00:35.00,Default,,0,0,0,,{\\fnRoboto}Placa em Roboto WMQ\n";
    const char *C = CAB("YCbCr Matrix: TV.709\n")
      "Dialogue: 0,0:00:30.00,0:00:35.00,Default,,0,0,0,,{\\fnInter Display}Placa em Roboto WMQ\n";
    if (!rob) { printf("Roboto ausente\n"); return 2; }
    carregar(C); parado(32.0); ni = nCap; for (i = 0; i < ni; i++) { inter[i] = cap[i]; inter[i].px = memcpy(malloc((size_t)cap[i].pw * cap[i].ph), cap[i].px, (size_t)cap[i].pw * cap[i].ph); }
    carregar(A); parado(2.0);                         // a faixa desenhando, sem Roboto no libass
    assrender_adicionar_fonte("Roboto-Regular.ttf", rob, tr);   // o lote novo: a fonte antes do corpo
    ok(assrender_atualizar(B, strlen(B), g) == 1, "lote novo aceito na mesma geracao");
    parado(32.0); nt = nCap; for (i = 0; i < nt; i++) { tarde[i] = cap[i]; tarde[i].px = memcpy(malloc((size_t)cap[i].pw * cap[i].ph), cap[i].px, (size_t)cap[i].pw * cap[i].ph); }
    carregar(B); parado(32.0); nc = nCap; for (i = 0; i < nc; i++) { cedo[i] = cap[i]; cedo[i].px = memcpy(malloc((size_t)cap[i].pw * cap[i].ph), cap[i].px, (size_t)cap[i].pw * cap[i].ph); }
    qsort(tarde, (size_t)nt, sizeof(Img), cmp_img); qsort(cedo, (size_t)nc, sizeof(Img), cmp_img); qsort(inter, (size_t)ni, sizeof(Img), cmp_img);
    for (i = 0; i < nt && i < nc; i++) if (cmp_img(&tarde[i], &cedo[i])) iguais = 0;
    if (nt != ni) difInter = 1; else for (i = 0; i < nt; i++) if (cmp_img(&tarde[i], &inter[i])) difInter = 1;
    printf("    %d imagens (tarde), %d (desde o inicio), %d (em Inter)\n", nt, nc, ni);
    ok(nt > 0 && nt == nc && iguais, "o evento sai igual ao da carga com a fonte desde o inicio");
    ok(difInter, "e nao e o desenho em Inter Display (a Roboto foi mesmo usada)");
    for (i = 0; i < nt; i++) free(tarde[i].px);
    for (i = 0; i < nc; i++) free(cedo[i].px);
    for (i = 0; i < ni; i++) free(inter[i].px);
    free(rob); }

  limpar_cap();
  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "ass_matriz: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
