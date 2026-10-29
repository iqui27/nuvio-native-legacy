// Ver telalag.h.
#include "telalag.h"

#if defined(NV_TPK)
#include "assperf.h"
#include "gl_compat.h"
#include "tpk_egl.h"
#include <string.h>

/* OpenGL ES 3.0 sync objects, by hand: the link only declares ES 2. */
#define TL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define TL_SYNC_FLUSH_COMMANDS_BIT    0x00000001
#define TL_ALREADY_SIGNALED           0x911A
#define TL_CONDITION_SATISFIED        0x911C
#define TL_WAIT_FAILED                0x911D
typedef void *TlSync;
typedef TlSync (*FnFence)(unsigned, unsigned);
typedef unsigned (*FnWait)(TlSync, unsigned, unsigned long long);
typedef void (*FnDel)(TlSync);
static FnFence pFence;
static FnWait pWait;
static FnDel pDel;
static int estado;   /* 0 not tried, 1 ready, -1 no ES 3 / no functions */

#define TL_FILA 24   /* frames in flight; at 60 fps that is 400 ms of GPU backlog */
static TlSync fila[TL_FILA];
static int nFila;

static int pronto(void) {
  if (estado) return estado > 0;
  estado = -1;
  {
    const char *v = (const char *)glGetString(GL_VERSION);
    if (!v || !strstr(v, "OpenGL ES ") || v[10] < '3') return 0;
  }
  if (!tpkEgl.GetProcAddress) return 0;
  *(void **)&pFence = tpkEgl.GetProcAddress("glFenceSync");
  *(void **)&pWait = tpkEgl.GetProcAddress("glClientWaitSync");
  *(void **)&pDel = tpkEgl.GetProcAddress("glDeleteSync");
  if (!pFence || !pWait || !pDel) return 0;
  estado = 1;
  return 1;
}

static void colher(void) {
  int i = 0;
  while (i < nFila) {
    unsigned r = pWait(fila[i], 0, 0);
    if (r != TL_ALREADY_SIGNALED && r != TL_CONDITION_SATISFIED && r != TL_WAIT_FAILED) break;
    pDel(fila[i]);
    i++;
  }
  if (i) { memmove(fila, fila + i, (size_t)(nFila - i) * sizeof fila[0]); nFila -= i; }
}

/* GPU QUEUE LIMIT, by situation (owner 25/10, logs 5 and 7): 1 frame helped
 * only while Render resolution Auto was at 75 % on heavy signs (lag peaks
 * 47-56 ms against 67-117 without); at a fixed 100 % on heavy signs it made
 * them worse (169-217 ms against ~118): there the GL thread itself is the
 * bottleneck and the wait only adds a stall. Light scenes: no difference. */
static int limite(void) {
  return assperf_get(AP_ESCALA) == 5 && assperf_escala() < 1.0 ? 1 : 0;
}

void telalag_quadro_inicio(void) {
  int lim;
  if (!pronto()) return;
  colher();
  lim = limite();
  if (lim > 0 && nFila >= lim) {
    /* wait for the frame that leaves only lim-1 still running (100 ms cap) */
    pWait(fila[nFila - lim], TL_SYNC_FLUSH_COMMANDS_BIT, 100000000ull);
    colher();
  }
}

void telalag_quadro_fim(void) {
  TlSync s;
  if (estado <= 0 || !limite()) return;   /* fences only while the limit is on */
  if (nFila == TL_FILA) { pDel(fila[0]); memmove(fila, fila + 1, (TL_FILA - 1) * sizeof fila[0]); nFila--; }
  s = pFence(TL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (!s) return;
  fila[nFila++] = s;
}

#else
void telalag_quadro_inicio(void) {}
void telalag_quadro_fim(void) {}
#endif
