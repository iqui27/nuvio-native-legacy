#!/bin/bash
# Offline regression: execute production EGL loading and first-frame code.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-i612.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import re, sys, xml.etree.ElementTree as ET
w = Path(sys.argv[1])
ns = {'t': 'http://tizen.org/ns/packages'}
m = ET.parse('tizen-tpk/NuvioTpk65/tizen-manifest.xml').getroot()
assert m.attrib['api-version'] == '6.5'
assert m.find('t:profile', ns).attrib['name'] == 'tv'
assert m.find('t:ui-application', ns).attrib['exec'] == 'NuvioTpk65.dll'
p = ET.parse('tizen-tpk/NuvioTpk65/NuvioTpk65.csproj').getroot()
assert p.find('.//TargetFramework').text == 'tizen90'
assert 'NV_API9' in p.find('.//DefineConstants').text
assert '../Program.cs' in [x.attrib['Include'] for x in p.findall('.//Compile')]
print('PASS: Tizen 6.5 uses API9 host and TV manifest')
host = Path('tizen-tpk/Program.cs').read_text()
entry = host[host.index('static void Main(string[] args)'):]
assert entry.index('app_get_data_path()') < entry.index('RodarEtapas(dados)') < entry.index('new Program()')
assert entry.index('Etapa("begin nui-construct")') < entry.index('new Program()')
oncreate = host[host.index('protected override void OnCreate()'):host.index('void Arranca(string dados)')]
assert 'if (string.IsNullOrEmpty(etapasArq)) etapaAnterior = RodarEtapas(dados);' in oncreate, 'OnCreate must preserve entry breadcrumbs'
for stage in ['nui-oncreate', 'dlopen-so', 'video-init', 'voz-init', 'nv_tpk_iniciar',
              'apps-init', 'gl-create', 'gl-config', 'gl-show', 'first-frame']:
    assert 'Etapa("begin '+stage in host and 'Etapa("ok '+stage in host, stage
assert 'RuntimeInformation.ProcessArchitecture' in entry
print('PASS: startup trace precedes NUI construction and covers native/GL boundaries')
# Release packaging retains ARM softfp and an in-package native library.
package = Path('tools/tpk.sh').read_text()
assert 'cp "$SAIDA/libnuvio.so" "$H/lib/"' in package
assert 'cmp -s "$D/$(basename "$T" .tpk)/lib/libnuvio.so" "$SAIDA/libnuvio.so"' in package
assert '-lGLESv2 -ldl -lpthread -lm -lrt' in package
flags = Path('tools/tpk/Dockerfile').read_text()
assert '-march=armv7-a -mfloat-abi=softfp -mfpu=neon' in flags
s = Path('src/tpk.c').read_text()
for stage in ['egl-load', 'app-thread', 'app-context']:
    assert 'nv_tpk40_etapa("begin '+stage in s and 'nv_tpk40_etapa("ok '+stage in s
assert s.index('snprintf(etapasArq,') < s.index('if (tpk_egl_carregar()')
print('PASS: ARMv7 softfp packaging and persisted native startup steps')
def function(marker):
    a = s.index(marker)
    b = s.index('{', a)
    depth, end = 1, b + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[a:end]
# Use real EGL dispatch layout, with host-only scalar types/constants.
(w/'EGL').mkdir()
constants = set(re.findall(r'\bEGL_[A-Z0-9_]+', function('int nv_tpk_quadro(void)')))
constants.update(['EGL_FALSE', 'EGL_TRUE', 'EGL_SUCCESS', 'EGL_NO_DISPLAY'])
header = '''#include <stdint.h>
typedef void *EGLDisplay, *EGLSurface, *EGLContext, *EGLConfig, *EGLNativeDisplayType, *EGLNativeWindowType;
typedef int EGLint, EGLBoolean; typedef unsigned EGLenum;
'''
for i, name in enumerate(sorted(constants), 1):
    value = '0' if name in {'EGL_NO_CONTEXT','EGL_NO_SURFACE','EGL_NO_DISPLAY','EGL_FALSE'} else str(i)
    header += '#define '+name+' '+value+'\n'
(w/'EGL/egl.h').write_text(header)
globals_start = s.index('enum { VEZ_NUI')
globals_end = s.index('// Fio do app: devolve', globals_start)
globals_text = s[globals_start:globals_end]
# Error storage may move ahead of the loader; declare it once in this harness.
globals_text = globals_text.replace('static char erro[512];', '')
(w/'egl-test.c').write_text('''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include "tpk_egl.h"
typedef int GLint;
#define GL_ALPHA_BITS 1
#define GL_COLOR_BUFFER_BIT 2
static void glGetIntegerv(int p, int *v) { *v = 8; }
static void glClearColor(float r, float g, float b, float a) {}
static void glClear(int mask) {}
static char erro[512];
'''+s[s.index('static char etapasArq'):s.index('static void etapasAnteriorParaLog')]+'''
TpkEgl tpkEgl;
static const char *missing;
static int no_library, opens;
static EGLDisplay display(void) { return (void *)1; }
static EGLSurface surface(EGLint which) { return (void *)2; }
static EGLContext context(void) { return (void *)3; }
static EGLint error(void) { return EGL_SUCCESS; }
static EGLBoolean query(EGLDisplay d, EGLContext c, EGLint key, EGLint *out) { *out=1; return 1; }
static EGLBoolean choose(EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint size, EGLint *n) { *n=0; return 1; }
static EGLBoolean make_current(EGLDisplay d, EGLSurface a, EGLSurface b, EGLContext c) { return 1; }
static void *fake_dlopen(const char *name, int flags) { opens++; return no_library ? NULL : (void *)1; }
static char *fake_dlerror(void) { return "test loader refusal"; }
static void *fake_dlsym(void *h, const char *name) {
  if (missing && !strcmp(name, missing)) return NULL;
  if (!strcmp(name,"eglGetCurrentDisplay")) return (void *)display;
  if (!strcmp(name,"eglGetCurrentSurface")) return (void *)surface;
  if (!strcmp(name,"eglGetCurrentContext")) return (void *)context;
  if (!strcmp(name,"eglGetError")) return (void *)error;
  if (!strcmp(name,"eglMakeCurrent")) return (void *)make_current;
  if (!strcmp(name,"eglChooseConfig")) return (void *)choose;
  return (void *)query;
}
#define dlopen fake_dlopen
#define dlsym fake_dlsym
#define dlerror fake_dlerror
'''+function('int tpk_egl_carregar(void)')+'\n'+globals_text+'\n'+function('int nv_tpk_quadro(void)')+'''
int main(int argc, char **argv) {
  assert(argc == 2);
  snprintf(etapasArq, sizeof etapasArq, "%s/steps.txt", argv[1]);
  const char *required[] = { "eglGetCurrentDisplay", "eglGetCurrentSurface", "eglGetCurrentContext",
    "eglMakeCurrent", "eglGetError", "eglQueryContext", "eglChooseConfig", "eglGetConfigAttrib" };
  for (unsigned i=0; i<sizeof required/sizeof *required; i++) {
    memset(&tpkEgl, 0, sizeof tpkEgl); erro[0]=0; missing=required[i];
    if (tpk_egl_carregar() == 0) { fprintf(stderr, "FAIL: loader accepted missing %s\\n", missing); return 1; }
    assert(strstr(erro, missing) && "diagnostic must name missing EGL entry point");
    missing=NULL;
    assert(tpk_egl_carregar() == 0 && "retry after incomplete library must reload symbols");
  }
  puts("PASS: every required EGL symbol checked; incomplete load can retry");
  memset(&tpkEgl, 0, sizeof tpkEgl); no_library=1; opens=0;
  assert(tpk_egl_carregar() != 0 && opens == 3);
  no_library=0; assert(tpk_egl_carregar() == 0);
  /* Optional EGL calls must not gate the app. */
  tpkEgl.SwapInterval=NULL; tpkEgl.QuerySurface=NULL;
  /* Missing current context/display/surface must fail before querying GL. */
  for (int i=0; i<3; i++) {
    ctx=EGL_NO_CONTEXT; contextoFalhou=0; erro[0]=0;
    tpkEgl.GetCurrentDisplay=display; tpkEgl.GetCurrentSurface=surface; tpkEgl.GetCurrentContext=context;
    if (i==0) tpkEgl.GetCurrentDisplay=(void *)null_current;
    if (i==1) tpkEgl.GetCurrentSurface=(void *)null_surface;
    if (i==2) tpkEgl.GetCurrentContext=null_current;
    assert(nv_tpk_quadro() == -1 && "missing EGL state must not reach handoff");
    assert(erro[0]);
  }
  ctx=EGL_NO_CONTEXT; contextoFalhou=0; erro[0]=0; tpkEgl.GetCurrentDisplay=display; tpkEgl.GetCurrentSurface=surface; tpkEgl.GetCurrentContext=context;
  assert(nv_tpk_quadro() == 0 && "valid API9 startup waits for app frame");
  puts("PASS: absent EGL state rejected; valid API9 startup preserved");
  char lines[4096] = {0};
  FILE *f = fopen(etapasArq, "r"); assert(f);
  assert(fread(lines, 1, sizeof lines-1, f) > 0); fclose(f);
  assert(strstr(lines, "native fail egl-context") && strstr(lines, "native ok egl-context"));
  puts("PASS: EGL failure/success breadcrumbs persisted before first app frame");
  return 0;
}
'''.replace('int main(int argc, char **argv) {', 'static EGLContext null_current(void) { return NULL; }\nstatic EGLSurface null_surface(EGLint which) { return NULL; }\nint main(int argc, char **argv) {'))
PY
cc -std=c11 -D_POSIX_C_SOURCE=200809L -I"$work" -Isrc "$work/egl-test.c" -pthread -o "$work/egl-test"
"$work/egl-test" "$work"
