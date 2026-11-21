#!/bin/bash
# #754: ao voltar do 2o plano o .tpk remede a superficie EGL SEM zerar o
# contexto. Stubs de EGL mudam o tamanho entre nv_tpk_ciclo(0) e a retomada;
# o teste exige telaW/telaH novos no tpk_tamanho, handle de superficie
# readotado, SIZE_CHANGED + viewport novos, nenhuma releitura sem ciclo e o
# init do contexto uma unica vez. Roda o codigo REAL de src/tpk.c, extraido
# como em tests/tpk_ciclo_fila.sh.
# F62: um fio faz nv_tpk_retomou() DURANTE a consulta EGL de um quadro em
# voo — o rearme novo nao pode ser consumido pela medida da superficie
# anterior. Na variante -DNV_TPK40 o host nao pode receber pedido de swap
# (r>0) quando a consulta ou o bind falham ou nao ha superficie corrente.
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
static int nBindsToDeadSurface, recusarBind;
static int sincronizarConsulta;
static pthread_barrier_t inicioConsulta, fimConsulta;
static int nPushSizeChanged, nViewport, vpW, vpH;
static EGLDisplay qGetCurrentDisplay(void) { return stubDpy; }
static EGLSurface qGetCurrentSurface(EGLint r) { (void)r; return stubCorrente ? stubSup : EGL_NO_SURFACE; }
static EGLContext qGetCurrentContext(void) { return stubCorrente ? stubCtx : EGL_NO_CONTEXT; }
static EGLBoolean qMakeCurrent(EGLDisplay d, EGLSurface dr, EGLSurface rd, EGLContext c) {
  (void)d; (void)rd; (void)dr;
  if (dr == EGL_NO_SURFACE && c == EGL_NO_CONTEXT) nMakeSemCtx++;
  if (c != EGL_NO_CONTEXT && dr != stubSup) {
    nBindsToDeadSurface++;
    return 0;
  }
  if (c != EGL_NO_CONTEXT && recusarBind) return 0;
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
  if (sincronizarConsulta && a == EGL_HEIGHT) {
    pthread_barrier_wait(&inicioConsulta);
    pthread_barrier_wait(&fimConsulta);
  }
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
#ifndef NV_TPK40
// F62: enfia a retomada exatamente dentro da consulta EGL de um quadro em
// voo — o rearme nao pode ser consumido pela medida da superficie anterior.
static void *retomarDuranteConsulta(void *arg) {
  (void)arg;
  pthread_barrier_wait(&inicioConsulta);
  nv_tpk_retomou();
  pthread_barrier_wait(&fimConsulta);
  return NULL;
}
#else
// Fio do app do protocolo de handoff: devolve a vez com quadro entregue.
static void *entregarQuadros(void *arg) {
  const int total = *(int *)arg;
  appEsperaVez(0);
  for (int quadro = 0; quadro < total; quadro++)
    appEsperaVez(1);
  return NULL;
}
#endif
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

  // A new window surface can remain the same size as the old one.
  nv_tpk_ciclo(0);
  nv_tpk_quadro(); // last render callback of the old surface after OnPause
  stubSup = (EGLSurface)0x53;
  nv_tpk_retomou();
  nv_tpk_quadro();
  ok(sup == stubSup, "retomada readota apos ultimo quadro da pausa");

  // Previous frame timed out and the app has since handed the turn back.
  sup = stubSup; // establish a valid old cache independently of the previous assertion
  nuiCorrente = 0;
  pendente = 1;
  nv_tpk_ciclo(0);
  nv_tpk_retomou();
  stubSup = (EGLSurface)0x54;
  stubConsultaOk = 0;
  r = nv_tpk_quadro();
  ok(nBindsToDeadSurface == 0, "consulta falha: sem bind da superficie destruida");
  ok(r == 0, "consulta falha: nao sinaliza swap de quadro da superficie destruida");
  stubConsultaOk = 1;
  nv_tpk_quadro();
  ok(sup == stubSup, "quadro seguinte adota superficie nova");

  // O host 4/5 nao chama nv_tpk_retomou: a troca do handle sozinha consome.
  nv_tpk_ciclo(0);
  nv_tpk_quadro(); // quadro da pausa na superficie antiga: flag segue armada
  stubSup = (EGLSurface)0x55;
  nv_tpk_quadro();
  ok(sup == stubSup, "sem retomou: troca do handle readota (TPK40)");

  // Sem ciclo novo: nada de releitura.
  q0 = nQuerySurface;
  nv_tpk_quadro();
  ok(nQuerySurface == q0, "depois de remediar, nenhuma releitura sem ciclo");

#ifndef NV_TPK40
  // F62: retomada DURANTE a consulta EGL de um quadro em voo. O quadro mediu
  // a superficie antiga e nao pode consumir o rearme novo (F59-r2, achado 1).
  {
    pthread_t outro;
    nv_tpk_ciclo(0);
    sincronizarConsulta = 1;
    pthread_barrier_init(&inicioConsulta, NULL, 2);
    pthread_barrier_init(&fimConsulta, NULL, 2);
    if (pthread_create(&outro, NULL, retomarDuranteConsulta, NULL)) return 88;
    nv_tpk_quadro();
    pthread_join(outro, NULL);
    sincronizarConsulta = 0;
    pthread_barrier_destroy(&inicioConsulta);
    pthread_barrier_destroy(&fimConsulta);
    ok(revalidarSuperficie != 0, "retomada posterior a leitura EGL nao perde o rearme");
    stubSup = (EGLSurface)0x99;
    nv_tpk_quadro();
    ok(sup == stubSup, "primeiro callback com handle novo readota");

    // Sem eglQuerySurface o fluxo cai no handoff de sempre.
    nv_tpk_ciclo(0);
    nv_tpk_retomou();
    tpkEgl.QuerySurface = NULL;
    stubSup = sup;
    pendente = 1;
    ok(nv_tpk_quadro() == 1 && !pendente, "sem simbolo QuerySurface: handoff antigo funciona");
    tpkEgl.QuerySurface = qQuerySurface;
  }
#else
  // F62 (TPK40): o host recebe pedido de swap quando r>0. Sem superficie
  // corrente, com a consulta falhando ou com o bind recusado o quadro nao
  // pode pedir swap (F59-r2, achado 2). esperaMs<0 = modo TVGLApplication.
  {
    pthread_t outro;
    int total, trocas = 0, semCorrente = 0;
    int quadrosAntes, consultasAntes, puladosAntes;
    esperaMs = -1;
    nv_tpk_ciclo(0);
    nuiCorrente = 0;
    pendente = 1;
    stubCorrente = 0;
    stubSup = (EGLSurface)0x91;
    r = nv_tpk_quadro();
    ok(r <= 0, "sem superficie corrente: host nao deve pedir swap");
    ok(nBindsToDeadSurface == 0 && pendente && revalidarSuperficie,
       "sem superficie corrente preserva quadro e flag sem bind velho");
    stubCorrente = 1;
    stubConsultaOk = 0;
    r = nv_tpk_quadro();
    ok(r <= 0, "consulta falha: host nao deve pedir swap");
    stubConsultaOk = 1;
    ok(nv_tpk_quadro() == 1 && sup == stubSup && !pendente && !revalidarSuperficie,
       "callback com superficie valida recupera handoff");

    nuiCorrente = 0;
    stubCorrente = 0;
    pendente = 1;
    recusarBind = 1;
    r = nv_tpk_quadro();
    ok(r <= 0, "bind recusado: host nao deve pedir swap");
    recusarBind = 0;
    stubCorrente = 1;
    nv_tpk_quadro();

    // TPK40 sem nv_tpk_retomou: a flag 2 segue armada ate o handle trocar; o
    // handoff real entrega quadro a quadro com duas consultas de dimensao.
    total = 120;
    quadrosAntes = nQuadros; consultasAntes = nQuerySurface;
    puladosAntes = nPulados;
    nv_tpk_ciclo(0);
    if (pthread_create(&outro, NULL, entregarQuadros, &total)) return 88;
    pthread_mutex_lock(&trava);
    while (!appEsperando) pthread_cond_wait(&mudou, &trava);
    pthread_mutex_unlock(&trava);
    for (int quadro = 0; quadro < total; quadro++) {
      r = nv_tpk_quadro();
      trocas += r > 0;
      semCorrente += !stubCorrente;
    }
    pthread_mutex_lock(&trava);
    vez = VEZ_APP;
    pthread_cond_broadcast(&mudou);
    pthread_mutex_unlock(&trava);
    pthread_join(outro, NULL);
    ok(nQuerySurface - consultasAntes == total * 2 && revalidarSuperficie == 2,
       "handle estavel consulta largura e altura a cada quadro");
    ok(nQuadros - quadrosAntes == total && trocas == total &&
       nPulados == puladosAntes && semCorrente == 0,
       "quadros reais seguem o handoff com superficie valida");

    pthread_mutex_lock(&trava);
    vez = VEZ_NUI;
    pthread_mutex_unlock(&trava);
    total = 1;
    recusarBind = 1;
    if (pthread_create(&outro, NULL, entregarQuadros, &total)) return 88;
    pthread_mutex_lock(&trava);
    while (!appEsperando) pthread_cond_wait(&mudou, &trava);
    pthread_mutex_unlock(&trava);
    r = nv_tpk_quadro();
    ok(r <= 0, "bind recusado no retorno do app: host nao deve pedir swap");
    pthread_mutex_lock(&trava);
    vez = VEZ_APP;
    pthread_cond_broadcast(&mudou);
    pthread_mutex_unlock(&trava);
    pthread_join(outro, NULL);
  }
#endif

  printf("\n%s\n", falhas ? "FALHOU" : "tpk_superficie_adocao: ok");
  return falhas != 0;
}
'''
(out/"test.c").write_text(harness + extraido + "\n" + main)
PYTHON
cc ${flags[@]+"${flags[@]}"} -Wall -Wno-unused-function -O1 -pthread "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
cc ${flags[@]+"${flags[@]}"} -DNV_TPK40 -Wall -Wno-unused-function -O1 -pthread "$tmp/test.c" -o "$tmp/test40"
"$tmp/test40"
