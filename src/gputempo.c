// GPU time per frame via GL_EXT_disjoint_timer_query. See gputempo.h.
#include "gputempo.h"

#ifdef NV_ANDROID
#include <SDL.h>
#include <stdio.h>
#include "gl_compat.h"
#include <sys/system_properties.h>

#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT           0x88BF
#endif
#ifndef GL_QUERY_RESULT_EXT
#define GL_QUERY_RESULT_EXT           0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE_EXT
#define GL_QUERY_RESULT_AVAILABLE_EXT 0x8867
#endif
#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT           0x8FBB
#endif

typedef unsigned long long NvGLuint64;
typedef void (*PfnGenQueries)(GLsizei, GLuint *);
typedef void (*PfnBeginQuery)(GLenum, GLuint);
typedef void (*PfnEndQuery)(GLenum);
typedef void (*PfnGetQueryObjectuiv)(GLuint, GLenum, GLuint *);
typedef void (*PfnGetQueryObjectui64v)(GLuint, GLenum, NvGLuint64 *);

static PfnGenQueries          pGen;
static PfnBeginQuery          pBegin;
static PfnEndQuery            pEnd;
static PfnGetQueryObjectuiv   pGetUiv;
static PfnGetQueryObjectui64v pGetUi64v;

// Ring of queries in flight. A result is normally ready two frames later; with
// six slots the ring never fills on a healthy pipeline, and when it does the
// frame simply goes unmeasured instead of blocking on glGetQueryObject.
#define NQ 6
static GLuint q[NQ];
static int cab, cauda, nFila;   // FIFO: cauda = oldest in flight, cab = next free
static int ligado, aberto;
static double soma, pior, ult;
static int n;
#define NAMOSTRAS 256
static float amostras[NAMOSTRAS];   // frames of the current window, for the p90

void gputempo_iniciar(void) {
  // SO DIAGNOSTICO, DESLIGADO POR PADRAO (#318). Ligado em todo quadro na
  // 2.0.1, ele nao acelera nada (so mede) e no Xiaomi MiTV-AFKR0 (Mali-G31)
  // devolveu lixo (med=217020518528 ms) junto com a tela piscando; a
  // teste-318.1 sem ele consertou. Para medir numa TV de bancada:
  //   adb shell setprop debug.nuvio.gputempo 1   (lido so na abertura)
  { char pv[PROP_VALUE_MAX] = "";
    if (__system_property_get("debug.nuvio.gputempo", pv) <= 0 || !pv[0] || pv[0] == '0') {
      printf("[gpu-tempo] desligado (diagnostico: debug.nuvio.gputempo 1)\n");
      return;
    } }
  if (!SDL_GL_ExtensionSupported("GL_EXT_disjoint_timer_query")) {
    printf("[gpu-tempo] sem GL_EXT_disjoint_timer_query: GPU nao medida\n");
    return;
  }
  *(void **)&pGen      = SDL_GL_GetProcAddress("glGenQueriesEXT");
  *(void **)&pBegin    = SDL_GL_GetProcAddress("glBeginQueryEXT");
  *(void **)&pEnd      = SDL_GL_GetProcAddress("glEndQueryEXT");
  *(void **)&pGetUiv   = SDL_GL_GetProcAddress("glGetQueryObjectuivEXT");
  *(void **)&pGetUi64v = SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT");
  if (!pGen || !pBegin || !pEnd || !pGetUiv || !pGetUi64v) {
    printf("[gpu-tempo] extensao sem funcoes: GPU nao medida\n");
    return;
  }
  pGen(NQ, q);
  ligado = 1;
  printf("[gpu-tempo] GL_EXT_disjoint_timer_query ligado\n");
}

// Harvest every finished query, oldest first; stop at the first one still
// running (results come back in order).
// The spec says a result is only trustworthy if GL_GPU_DISJOINT_EXT reads 0
// AFTER it was fetched; a disjoint drops everything in flight and the window.
// A value over 1 s is a broken counter, not a frame (the Xiaomi's
// 217020518528 ms): dropped too.
static void colherProntas(void) {
  GLint disj = 0;
  while (nFila > 0) {
    GLuint pronto = 0;
    NvGLuint64 ns = 0;
    pGetUiv(q[cauda], GL_QUERY_RESULT_AVAILABLE_EXT, &pronto);
    if (!pronto) break;
    pGetUi64v(q[cauda], GL_QUERY_RESULT_EXT, &ns);
    cauda = (cauda + 1) % NQ; nFila--;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disj);
    if (disj) { soma = 0; pior = 0; n = 0; cauda = cab; nFila = 0; break; }
    if (ns == 0 || ns > 1000000000ULL) continue;
    { double ms = (double)ns / 1e6;
      ult = ms; soma += ms; if (n < NAMOSTRAS) amostras[n] = (float)ms; n++;
      if (ms > pior) pior = ms; }
  }
}

void gputempo_quadro_inicio(void) {
  GLint disj = 0;
  if (!ligado || aberto) return;
  glGetIntegerv(GL_GPU_DISJOINT_EXT, &disj);   // also clears the flag
  if (disj) { soma = 0; pior = 0; n = 0; }   // clock jumped: the window is garbage
  colherProntas();
  if (nFila >= NQ) return;                   // ring full: skip this frame, never block
  pBegin(GL_TIME_ELAPSED_EXT, q[cab]);
  aberto = 1;
}

void gputempo_quadro_fim(void) {
  if (!ligado || !aberto) return;
  pEnd(GL_TIME_ELAPSED_EXT);
  aberto = 0;
  cab = (cab + 1) % NQ; nFila++;
}

int gputempo_colher(double *med, double *piorOut, double *ultOut) {
  int r = n;
  if (med) *med = n ? soma / n : 0.0;
  if (piorOut) *piorOut = pior;
  if (ultOut) *ultOut = ult;
  soma = 0; pior = 0; n = 0;
  return r;
}

// 90th percentile of the window so far (call BEFORE gputempo_colher, which
// resets it). 0 = no samples.
double gputempo_p90(void) {
  float a[NAMOSTRAS]; int m = n < NAMOSTRAS ? n : NAMOSTRAS, i, j;
  if (m <= 0) return 0.0;
  for (i = 0; i < m; i++) a[i] = amostras[i];
  for (i = 1; i < m; i++) { float v = a[i]; for (j = i - 1; j >= 0 && a[j] > v; j--) a[j + 1] = a[j]; a[j + 1] = v; }
  return (double)a[(m * 9) / 10 >= m ? m - 1 : (m * 9) / 10];
}

double gputempo_ultimo(void) { return ult; }

#else
void gputempo_iniciar(void) {}
void gputempo_quadro_inicio(void) {}
void gputempo_quadro_fim(void) {}
int gputempo_colher(double *med, double *pior, double *ult) {
  if (med) *med = 0.0;
  if (pior) *pior = 0.0;
  if (ult) *ult = 0.0;
  return 0;
}
double gputempo_p90(void) { return 0.0; }
double gputempo_ultimo(void) { return 0.0; }
#endif
