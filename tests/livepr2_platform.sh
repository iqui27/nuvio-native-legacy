#!/bin/bash
# Host regressions for TPK40 thread storage and TPK's non-SDL EGL context.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-livepr2.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
import pathlib, re, subprocess, sys
work = pathlib.Path(sys.argv[1])
failed = False
for name in ('m3u', 'stalker', 'xtream'):
    source = pathlib.Path(f'src/{name}.c')
    assembly = work / f'{name}.s'
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L',
                    '-D_DEFAULT_SOURCE', '-D_DARWIN_C_SOURCE', '-DNV_TPK40',
                    '-Isrc', '-S', str(source), '-o', str(assembly)], check=True)
    # Mach-O and ELF compiler TLS markers, including zero-initialized storage.
    tls = re.search(r'__thread_vars|__thread_bss|__tlv_bootstrap|\.tdata|\.tbss|__tls_get_addr|TLS', assembly.read_text())
    selected = 'NV_TPK40' in source.read_text()
    ok = not tls and selected
    print(f'{name}: no compiler TLS and selected for TPK40 rebuild: {"PASS" if ok else "FAIL"}', flush=True)
    failed |= not ok
source = pathlib.Path('src/gpunivel.c').read_text()
function = source[source.index('int gpun_upscale_suportado(void) {'):source.index('\nvoid gpun_ampliar_app(')]
stubs = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
typedef int GLint;
#define EGL_NO_CONTEXT NULL
#define GL_RENDERER 1
#define GL_FRAGMENT_SHADER 2
#define GL_HIGH_FLOAT 3
static void *eglContext, *sdlContext;
static const char *renderer = "Mali-T830";
static int precision = 23, queries;
static void *currentEgl(void) { return eglContext; }
static struct { void *(*GetCurrentContext)(void); } tpkEgl = {currentEgl};
#define eglGetCurrentContext tpkEgl.GetCurrentContext
static void *SDL_GL_GetCurrentContext(void) { return sdlContext; }
static const unsigned char *glGetString(int name) { (void)name; queries++; return (const unsigned char *)renderer; }
static void glGetShaderPrecisionFormat(int a, int b, int *range, int *p) { (void)a; (void)b; (void)range; *p = precision; }
'''
main = r'''
int main(void) {
  assert(!gpun_upscale_suportado()); assert(!queries);
#ifdef NV_TPK
  tpkEgl.GetCurrentContext = NULL;
  assert(!gpun_upscale_suportado()); assert(!queries);
  tpkEgl.GetCurrentContext = currentEgl;
  eglContext = (void *)1; /* SDL dummy driver still has no context. */
#else
  sdlContext = (void *)1;
#endif
  assert(gpun_upscale_suportado());
  renderer = "Mali-400"; assert(!gpun_upscale_suportado());
  renderer = "Mali-T830"; precision = 0; assert(!gpun_upscale_suportado());
  return 0;
}
'''
test = work / 'upscale.c'
test.write_text(stubs + function + main)
for platform in ('TPK', 'SDL'):
    binary = work / platform
    subprocess.run(['cc', '-DNV_GLES_NO_MAC'] + (['-DNV_TPK'] if platform == 'TPK' else []) + [str(test), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)])
    print(f'{platform} context and GPU capability guards: {"PASS" if result.returncode == 0 else "FAIL"}', flush=True)
    failed |= result.returncode != 0
sys.exit(int(failed))
PY
# Verify selection, cached pointers, filenames and retry values stay per thread.
cat > "$work/threads.c" <<'C'
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
typedef struct FonteContexto {
  unsigned sid;
  struct FonteContexto *proximo;
} FonteContexto;
#define IPTV_CTX_INICIALIZAR(c) ((void)(c))
#define IPTV_CTX_EMERGENCIA {0}
#include "iptvcontexto.inc"
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int arrived;
static void *worker(void *arg) {
  unsigned id = *(unsigned *)arg;
  assert(usarFonte(id) == UINT_MAX);
  FonteFio *f = fonteFio();
  assert(!f->retry && !f->nome[0]);
  FonteContexto *c = contexto();
  f->retry = (int)id;
  f->nome[0] = (char)id;
  pthread_mutex_lock(&lock);
  arrived++;
  pthread_cond_broadcast(&ready);
  while (arrived < 2) pthread_cond_wait(&ready, &lock);
  pthread_mutex_unlock(&lock);
  assert(contexto() == c && c->sid == id);
  assert(f->retry == (int)id && f->nome[0] == (char)id);
  assert(usarFonte(UINT_MAX) == id);
  assert(contexto()->sid == 0);
  return NULL;
}
int main(void) {
  pthread_t threads[2];
  unsigned ids[] = {1, 2};
  for (int i = 0; i < 2; i++) assert(!pthread_create(&threads[i], NULL, worker, &ids[i]));
  for (int i = 0; i < 2; i++) assert(!pthread_join(threads[i], NULL));
  assert(contexto()->sid == 0);
}
C
cc -std=c11 -DNV_TPK40 -Isrc "$work/threads.c" -lpthread -o "$work/threads"
"$work/threads"
# Exercise account persistence and source routing with the TPK40 storage path.
cc -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE \
  -DNV_TPK40 -O1 -g -Wall -Wextra -Isrc tests/iptv_accounts.c \
  src/iptvfontes.c src/xtream.c src/m3u.c src/m3u_parse.c src/stalker.c src/js.c \
  -lpthread -o "$work/accounts"
"$work/accounts" "$work/data"
