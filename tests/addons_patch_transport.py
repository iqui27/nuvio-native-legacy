"""Exercise nuvem against the production WGT request adapter, without a TV SDK."""
import pathlib
import subprocess
import sys
import tempfile

source = pathlib.Path('src/rede.c').read_text()

def function(name):
    start = source.index(name)
    start = source.rfind('\n', 0, start) + 1
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

adapter = function('int rede_pedir(')
if 'char *rede_patch_st(' in source:
    adapter += '\n' + function('char *rede_patch_st(')

harness = r'''
#include "rede.h"
#include "nuvem.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int calls, response = 200;
static const char *expected;
static int nv_http_fluxo(const char *u, unsigned p, unsigned t, int cb,
                         void *v, int *s, unsigned *n) { abort(); }
static char *pedir2(const char *method, const char *url, int seconds, const char *const *headers,
                   const char *extra, const char *body, long *size, int *status,
                   char *etag, unsigned etagSize, int *retryAfter) {
  // rede_patch_st now adds Content-Type as the extra line; the JS bridge
  // collapses it with headers[2] (same name and value).
  assert(!extra || !strcmp(extra, "Content-Type: application/json"));
  assert(!strcmp(method, "PATCH"));
  assert(!strcmp(url, "https://account.example/rest/v1/addons?user_id=eq.owner&profile_id=eq.1&url=eq.addon"));
  assert(!strcmp(headers[0], "apikey: fixture-anon"));
  assert(!strcmp(headers[1], "Authorization: Bearer fixture-token"));
  assert(!strcmp(headers[2], "Content-Type: application/json"));
  assert(!strcmp(headers[3], "Prefer: return=representation"));
  assert(!strcmp(body, expected));
  calls++;
  *status = response;
  return response ? strdup(response == 200 ? "[{\"enabled\":false}]" : "error") : NULL;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); }
'''
if 'rede_pedido_wgt_sonda(' in adapter:
    harness += r'''
#include "redepedido_wgt.h"
static int redeCancelouLocal;
static char *juntarCabs(const char *const *cab, const char *extra) {
  (void)cab; (void)extra; abort();
}
static unsigned long redeAgoraMs(void) { abort(); }
static int nv_http_iniciar_assinc(const char *m, const char *u, const char *c,
                                  const char *b, int p, int l, int mode) {
  (void)m; (void)u; (void)c; (void)b; (void)p; (void)l; (void)mode; abort();
}
static void nv_http_esperar_pedido(const RedePedido *p, int id) {
  (void)p; (void)id; abort();
}
static int nv_http_ttfb(int id) { (void)id; abort(); }
static char *nv_http_colher_assinc(int id, int *tam, int *status,
                                   char *uf, int uft, char *et, int ett,
                                   int *ra, int *lim, char *m, int mt,
                                   long *t, int modo) {
  (void)id; (void)tam; (void)status; (void)uf; (void)uft; (void)et; (void)ett;
  (void)ra; (void)lim; (void)m; (void)mt; (void)t; (void)modo; abort();
}
'''
harness += adapter
harness += r'''
int main(void) {
  int st;
  for (int enabled = 0; enabled <= 1; enabled++) {
    expected = enabled ? "{\"enabled\":true}" : "{\"enabled\":false}";
    char *r = nuvem_addon_alternar("user_id=eq.owner&profile_id=eq.1&url=eq.addon",
                                  enabled, "fixture-token", 2000, &st);
    if (!r || st != 200 || calls != enabled + 1) {
      fprintf(stderr, "FAIL: authenticated WGT PATCH unavailable (HTTP %d)\n", st);
      return 1;
    }
    free(r);
  }
  response = 503;
  char *r = nuvem_addon_alternar("user_id=eq.owner&profile_id=eq.1&url=eq.addon",
                                1, "fixture-token", 2000, &st);
  assert(r && st == 503); free(r);
  response = 0;
  r = nuvem_addon_alternar("user_id=eq.owner&profile_id=eq.1&url=eq.addon",
                          1, "fixture-token", 2000, &st);
  assert(!r && st == 0);
  puts("PASS: production WGT adapter sends authenticated PATCH and preserves failures");
}
'''
with tempfile.TemporaryDirectory(prefix='addon-patch-') as directory:
    c = pathlib.Path(directory) / 'transport.c'
    exe = pathlib.Path(directory) / 'transport'
    c.write_text(harness)
    subprocess.run(['cc', '-w', '-Isrc', '-ffunction-sections', '-fdata-sections',
                    '-Wl,-dead_strip', '-D__EMSCRIPTEN__', '-DNV_SUPABASE_URL="https://account.example"',
                    '-DNV_SUPABASE_ANON_KEY="fixture-anon"', str(c), 'src/nuvem.c',
                    '-o', str(exe)], check=True)
    sys.exit(subprocess.run([str(exe)]).returncode)
