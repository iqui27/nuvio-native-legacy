/* "Texto simples" (asspeso, fora do .tpk) x faixa/lote novo: motorTrocou
 * (legenda.c) reextrai o documento SEM a trava. Uma extracao atrasada nao
 * pode:
 *  [1] reinstalar os cues de um corpo velho por cima do lote novo da mesma
 *      faixa (mesma geracao, outra revisao do corpo);
 *  [2] tirar do libass a faixa que entrou depois (assrender_limpar_geracao).
 * A extracao lenta e um documento ASS grande; a troca de modo corre num fio
 * enquanto o principal entrega o lote/faixa novo. */
#include "../src/legenda.h"
#include "../src/assrender.h"
#include "../src/asspeso.h"
#include "../src/gfx.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

char *rede_baixar(const char *url, int segundos) { (void)url; (void)segundos; return NULL; }
char *rede_baixar_bin(const char *url, int segundos, long *n) { (void)url; (void)segundos; (void)n; return NULL; }
const char *i18n(const char *s) { return s; }
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

#define CAB "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n\n" \
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Arial,64,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,3,1.5,2,60,60,50,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

/* Documento com `n` falas, todas vivas em 1 s (a contagem de cues ali diz qual
 * documento esta valendo). */
static char *doc(int n, const char *rotulo) {
  size_t cap = strlen(CAB) + (size_t)n * 96 + 1; char *b = malloc(cap); size_t o; int i;
  o = (size_t)snprintf(b, cap, "%s", CAB);
  for (i = 0; i < n; i++)
    o += (size_t)snprintf(b + o, cap - o, "Dialogue: 0,0:00:00.%02d,0:00:05.00,Default,,0,0,0,,%s %d\n", i % 100, rotulo, i);
  return b;
}
static int vivos(void) { LegendaCue v[64]; return legenda_cues(1.0, 0, v, 64); }

static void *simples(void *u) { (void)u; assrender_definir_peso(ASSPESO_SIMPLES); return NULL; }
static void *automatico(void *u) { (void)u; assrender_definir_peso(ASSPESO_AUTO); return NULL; }

static double agora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(void) {
  char *grande = doc(1500000, "velho"), *lote = doc(3, "lote"), *outra = doc(5, "outra");
  pthread_t f; double t0;

  /* Quanto demora a extracao do documento grande (a janela da corrida). */
  t0 = agora(); { LegendaCue *v = NULL; int n = legenda_extrair(grande, &v); free(v); (void)n; }
  printf("extracao do documento grande: %.0f ms\n", (agora() - t0) * 1000);

  printf("[1] lote novo da mesma faixa durante a reextracao\n");
  assrender_definir_peso(ASSPESO_AUTO);
  legenda_definir_corpo(grande);
  pthread_create(&f, NULL, simples, NULL);
  usleep(10000);                       /* o fio ja copiou o corpo grande e esta extraindo */
  legenda_atualizar_corpo(lote);       /* mesma geracao, corpo novo */
  pthread_join(f, NULL);
  ok(vivos() == 3, "os cues do lote novo ficam (a reextracao velha nao reinstala o corpo antigo)");

  printf("[2] faixa nova durante a reextracao (volta ao Automatico)\n");
  assrender_definir_peso(ASSPESO_SIMPLES);
  legenda_definir_corpo(grande);
  pthread_create(&f, NULL, automatico, NULL);   /* reextrai o grande e o devolve ao libass */
  usleep(10000);
  legenda_definir_corpo(outra);                 /* geracao nova, ja no Automatico */
  pthread_join(f, NULL);
  ok(vivos() == 5, "os cues da faixa nova ficam");
  ok(assrender_ativo() == 1, "a faixa nova continua no libass");
  { unsigned g = legenda_geracao();
    ok(assrender_limpar_geracao(g - 1) == 0 && assrender_ativo() == 1, "limpar da geracao velha nao apaga a faixa nova");
    ok(assrender_limpar_geracao(g) == 1 && assrender_ativo() == 0, "limpar da geracao certa apaga"); }

  printf("[3] SRT em arabe (texto simples no libass) durante a reextracao do Texto simples\n");
  { static const char *SRT = "1\n00:00:00,500 --> 00:00:05,000\n\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 \xd8\xa8\xd9\x83\xd9\x85\n\n";
    int antes;
    assrender_definir_peso(ASSPESO_AUTO);
    legenda_definir_corpo(SRT);
    antes = assrender_texto_simples();
    printf("    SRT arabe no libass sozinho: %d\n", antes);
    legenda_definir_corpo(grande);
    pthread_create(&f, NULL, simples, NULL);   /* reextrai o grande e tira do libass */
    usleep(10000);
    legenda_definir_corpo(SRT);                /* faixa nova: texto RTL pelo libass */
    pthread_join(f, NULL);
    ok(!antes || assrender_texto_simples() == 1, "a faixa nova (SRT RTL no libass) nao e apagada pela reextracao velha"); }

  legenda_desligar();
  free(grande); free(lote); free(outra);
  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "legenda_peso_corrida: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
