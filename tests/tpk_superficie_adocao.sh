#!/bin/bash
# #754: ao voltar do 2o plano o .tpk remede a superficie EGL SEM zerar o
# contexto. Stubs de EGL mudam o tamanho entre nv_tpk_ciclo(0) e a retomada;
# o teste exige telaW/telaH novos no tpk_tamanho, handle de superficie
# readotado, SIZE_CHANGED + viewport novos, nenhuma releitura sem ciclo e o
# init do contexto uma unica vez. Roda o codigo REAL de src/tpk.c, extraido
# como em tests/tpk_ciclo_fila.sh.
#   SANITIZE=1 (padrao) ASan+UBSan, SANITIZE=thread TSan, SANITIZE=0 nenhum.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-sup.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PYTHON'
from pathlib import Path
import re, sys
out = Path(sys.argv[1])
src = Path("src/tpk.c").read_text()
tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{};]')
def funcao(assinatura):
    for inicio in re.finditer(r'(?m)^' + re.escape(assinatura), src):
        aberta = None; nivel = 0
        for token in tokens.finditer(src, inicio.start()):
            v = token[0]
            if aberta is None:
                if v == ';': break
                if v != '{': continue
                aberta = token.start()
            if v == '{': nivel += 1
            elif v == '}':
                nivel -= 1
                if nivel == 0:
                    return src[inicio.start():token.end()]
        if aberta is not None:
            return src[inicio.start():]
    raise SystemExit("funcao nao achada: " + assinatura)
# helpers e estado da fila FIFO (#799) + a flag revalidarSuperficie (#754):
# o bloco entre o marcador e appEsperaVez
ini = src.index("#define CICLO_FILA")
fim = src.index("static void appEsperaVez")
fila = src[ini:fim]
pedacos = [funcao("static void appEsperaVez(int entregou) {"),
           funcao("void nv_tpk_ciclo(int memoria) {"),
           funcao("void nv_tpk_retomou(void) {"),
           funcao("int nv_tpk_quadro(void) {"),
           funcao("void tpk_tamanho(int *w, int *h) {")]
extraido = fila + "\n" + "\n".join(
    re.sub(r'__attribute__\(\(visibility\("default"\)\)\)\s*', '', p) for p in pedacos)
harness = r'''
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef unsigned Uint32;
typedef unsigned char Uint8;
// EGL/GLES/ SDL minimos para compilar o trecho real de tpk.c.
typedef void *EGLDisplay, *EGLSurface, *EGLContext, *EGLConfig;
typedef void *EGLNativeDisplayType, *EGLNativeWindowType;
typedef int EGLint, EGLBoolean, EGLenum;
#define EGL_NO_DISPLAY  ((EGLDisplay)0)
#define EGL_NO_SURFACE  ((EGLSurface)0)
#define EGL_NO_CONTEXT  ((EGLContext)0)
#define EGL_DRAW        0x3059
#define EGL_WIDTH       0x3057
#define EGL_HEIGHT      0x3056
#define EGL_CONFIG_ID   0x3028
#define EGL_NONE        0x3038
#define EGL_RED_SIZE    0x3024
#define EGL_GREEN_SIZE  0x3023
#define EGL_BLUE_SIZE   0x3022
#define EGL_ALPHA_SIZE  0x3021
#define EGL_SURFACE_TYPE 0x3033
typedef int GLint, GLsizei;
typedef unsigned GLenum, GLbitfield;
typedef float GLfloat;
#define GL_ALPHA_BITS       0x0D55
#define GL_COLOR_BUFFER_BIT 0x4000
#define SDL_APP_WILLENTERBACKGROUND 0x102
#define SDL_APP_LOWMEMORY           0x104
#define SDL_APP_DIDENTERFOREGROUND  0x106
#define SDL_WINDOWEVENT             0x200
#define SDL_WINDOWEVENT_SIZE_CHANGED 5
typedef union SDL_Event {
  Uint32 type;
  struct { Uint32 type; Uint32 timestamp; Uint32 windowID;
           Uint8 event, p1, p2, p3; int data1, data2; } window;
} SDL_Event;
enum { VEZ_NUI = 0, VEZ_APP = 1 };

// ---- EGL do teste: superficie "recriada" pelo host muda de tamanho entre
// nv_tpk_ciclo(0) e a volta. Contadores provam o que o codigo pergunta ao EGL.
static EGLDisplay stubDpy = (EGLDisplay)0xD1;
static EGLSurface stubSup = (EGLSurface)0x51;
static EGLContext stubCtx = (EGLContext)0xC1;
static int stubW = 1920, stubH = 1080, stubCorrente = 1, stubConsultaOk = 1;
static int nQuerySurface, nQueryContext, nChooseConfig, nMakeSemCtx;
static int nPushSizeChanged, nViewport, vpW, vpH;
static EGLDisplay qGetCurrentDisplay(void) { return stubDpy; }
static EGLSurface qGetCurrentSurface(EGLint r) { (void)r; return stubCorrente ? stubSup : EGL_NO_SURFACE; }
static EGLContext qGetCurrentContext(void) { return stubCorrente ? stubCtx : EGL_NO_CONTEXT; }
static EGLBoolean qMakeCurrent(EGLDisplay d, EGLSurface dr, EGLSurface rd, EGLContext c) {
  (void)d; (void)rd; (void)dr;
  if (dr == EGL_NO_SURFACE && c == EGL_NO_CONTEXT) nMakeSemCtx++;
  stubCorrente = c != EGL_NO_CONTEXT;
  return 1;
}
static EGLint qGetError(void) { return 0x3000; }
static EGLBoolean qQueryContext(EGLDisplay d, EGLContext c, EGLint a, EGLint *v) {
  (void)d; (void)c; (void)a; nQueryContext++; *v = 7; return 1;
}
static EGLBoolean qQuerySurface(EGLDisplay d, EGLSurface s, EGLint a, EGLint *v) {
  (void)d; (void)s; nQuerySurface++;
  if (!stubConsultaOk) return 0;
  *v = a == EGL_WIDTH ? stubW : stubH;
  return 1;
}
static EGLBoolean qChooseConfig(EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint n, EGLint *num) {
  (void)d; (void)a; (void)n; nChooseConfig++; *c = (EGLConfig)0xCF; *num = 1; return 1;
}
static EGLBoolean qGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint *v) {
  (void)d; (void)c; (void)a; *v = 8; return 1;
}
static EGLBoolean qSwapInterval(EGLDisplay d, EGLint i) { (void)d; (void)i; return 1; }
// Mesma tabela de ponteiros de src/tpk_egl.h: o trecho real chama via macro.
typedef struct {
  EGLDisplay (*GetCurrentDisplay)(void);
  EGLSurface (*GetCurrentSurface)(EGLint);
  EGLContext (*GetCurrentContext)(void);
  EGLBoolean (*MakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
  EGLint     (*GetError)(void);
  EGLBoolean (*QueryContext)(EGLDisplay, EGLContext, EGLint, EGLint *);
  EGLBoolean (*QuerySurface)(EGLDisplay, EGLSurface, EGLint, EGLint *);
  EGLBoolean (*ChooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
  EGLBoolean (*GetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint *);
  EGLDisplay (*GetDisplay)(EGLNativeDisplayType);
  EGLBoolean (*Initialize)(EGLDisplay, EGLint *, EGLint *);
  EGLBoolean (*BindAPI)(EGLenum);
  EGLSurface (*CreateWindowSurface)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
  EGLContext (*CreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
  EGLBoolean (*SwapInterval)(EGLDisplay, EGLint);
  EGLBoolean (*SwapBuffers)(EGLDisplay, EGLSurface);
  void      *(*GetProcAddress)(const char *);
} TpkEgl;
static TpkEgl tpkEgl;
#define eglGetCurrentDisplay  tpkEgl.GetCurrentDisplay
#define eglGetCurrentSurface  tpkEgl.GetCurrentSurface
#define eglGetCurrentContext  tpkEgl.GetCurrentContext
#define eglMakeCurrent        tpkEgl.MakeCurrent
#define eglGetError           tpkEgl.GetError
#define eglQueryContext       tpkEgl.QueryContext
#define eglQuerySurface       tpkEgl.QuerySurface
#define eglChooseConfig       tpkEgl.ChooseConfig
#define eglGetConfigAttrib    tpkEgl.GetConfigAttrib
#define eglSwapInterval       tpkEgl.SwapInterval
static void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
  (void)x; (void)y; nViewport++; vpW = (int)w; vpH = (int)h;
}
static void glGetIntegerv(GLenum p, GLint *v) { (void)p; *v = 8; }
static void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
static void glClear(GLbitfield m) { (void)m; }
static int SDL_PushEvent(SDL_Event *e) {
  if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) nPushSizeChanged++;
  return 0;
}
static void player_ciclo_evento(Uint32 t) { (void)t; }
void nv_tpk40_etapa(const char *linha) { (void)linha; }

// Estado compartilhado, espelho das estaticas de src/tpk.c.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  mudou = PTHREAD_COND_INITIALIZER;
static int vez = VEZ_NUI, terminou, pendente, appEsperando, nuiCorrente = 1;
static EGLDisplay dpy = EGL_NO_DISPLAY;
static EGLSurface sup = EGL_NO_SURFACE;
static EGLContext ctx = EGL_NO_CONTEXT;
static int telaW = 1920, telaH = 1080;
static int esperaMs = 50, swapZero;
static unsigned nQuadros, nPulados;
static int contextoFalhou;
static char erro[512];
'''
main = r'''
static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }
int main(void) {
  int w = 0, h = 0, r, q0;
  tpkEgl.GetCurrentDisplay = qGetCurrentDisplay;
  tpkEgl.GetCurrentSurface = qGetCurrentSurface;
  tpkEgl.GetCurrentContext = qGetCurrentContext;
  tpkEgl.MakeCurrent = qMakeCurrent;
  tpkEgl.GetError = qGetError;
  tpkEgl.QueryContext = qQueryContext;
  tpkEgl.QuerySurface = qQuerySurface;
  tpkEgl.ChooseConfig = qChooseConfig;
  tpkEgl.GetConfigAttrib = qGetConfigAttrib;
  tpkEgl.SwapInterval = qSwapInterval;

  // Arranque: o 1o quadro mede a superficie do host e NAO zera o contexto.
  r = nv_tpk_quadro();
  tpk_tamanho(&w, &h);
  ok(r == 0, "primeiro quadro devolve 0 (app ainda nao desenha)");
  ok(w == 1920 && h == 1080, "arranque: tamanho do host");
  ok(nQueryContext == 1 && nChooseConfig == 1, "init do contexto uma unica vez");
  ok(nMakeSemCtx == 0, "arranque nao solta o contexto (sem zerar)");
  q0 = nQuerySurface;

  // Quadros sem ciclo algum: a superficie nao pode ser relida.
  nv_tpk_quadro(); nv_tpk_quadro();
  ok(nQuerySurface == q0, "sem ciclo: nenhuma releitura da superficie");

  // O host recria a superficie menor durante o 2o plano (a 2a abertura da #754).
  stubSup = (EGLSurface)0x52; stubW = 1280; stubH = 720;
  nv_tpk_ciclo(0);
  nv_tpk_retomou();
  r = nv_tpk_quadro();
  tpk_tamanho(&w, &h);
  ok(w == 1280 && h == 720, "retomada: telaW/telaH readotados da superficie nova (#754)");
  ok(nQuerySurface > q0, "eglQuerySurface rodou de novo na volta");
  ok(nQueryContext == 1 && nChooseConfig == 1, "remedida nao refaz o init do contexto");
  ok(sup == stubSup && ctx == stubCtx, "handles vivos readotados, contexto preservado");
  ok(nPushSizeChanged == 1, "app avisado do tamanho novo (viewport/projecao)");
  ok(vpW == 1280 && vpH == 720, "viewport refeita no tamanho novo");
  q0 = nQuerySurface;

  // Consulta que falha mantem o tamanho e tenta de novo no proximo quadro.
  stubW = 3840; stubH = 2160;
  nv_tpk_ciclo(0);
  stubConsultaOk = 0;
  nv_tpk_quadro();
  tpk_tamanho(&w, &h);
  ok(w == 1280 && h == 720, "consulta falhou: tamanho anterior mantido");
  stubConsultaOk = 1;
  nv_tpk_quadro();
  tpk_tamanho(&w, &h);
  ok(w == 3840 && h == 2160, "flag armada: remede no quadro seguinte");
  ok(nPushSizeChanged == 2, "segunda mudanca tambem avisa o app");

  // Sem ciclo novo: nada de releitura.
  q0 = nQuerySurface;
  nv_tpk_quadro();
  ok(nQuerySurface == q0, "depois de remediar, nenhuma releitura sem ciclo");

  printf("\n%s\n", falhas ? "FALHOU" : "tpk_superficie_adocao: ok");
  return falhas != 0;
}
'''
(out/"test.c").write_text(harness + extraido + "\n" + main)
PYTHON
cc ${flags[@]+"${flags[@]}"} -Wall -Wno-unused-function -O1 -pthread "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
