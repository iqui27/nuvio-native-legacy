#!/usr/bin/env python3
"""Exercise production Trakt list GET headers with and without a session."""
import pathlib
import subprocess
import tempfile
src = pathlib.Path('src/descoberta.c').read_text()
a = src.index('      const char *cli=nuvem_trakt_cliente();', src.index('static void *fioVerTudo'))
b = src.index('        if (corpo) {', a)
code = r'''
#include "colecoes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
static int authenticated, gotAuth;
static const char *nuvem_trakt_cliente(void) { return "fixture-key"; }
int trakt_cabecalhos(const char **cab, char *a, size_t na, char *k, size_t nk) {
  if (!authenticated) return 0;
  snprintf(a, na, "Authorization: Bearer fixture"); snprintf(k, nk, "trakt-api-key: fixture-key");
  cab[0]=a; cab[1]="trakt-api-version: 2"; cab[2]=k; cab[3]=NULL; return 1;
}
static char *rede_baixar_com(const char *url, int t, const char *const *cab) {
  assert(strstr(url, "/lists/77/items/movie")); assert(t == 12);
  gotAuth = 0;
  for (int i=0; cab[i]; i++) if (!strncmp(cab[i], "Authorization: Bearer ", 22)) gotAuth++;
  return NULL;
}
static void readList(void) {
  ColSource fonte = {0}; fonte.traktLista=77; strcpy(fonte.midia,"MOVIE");
  int skip=1; char url[1024]; char *corpo;
'''
code += src[a:b] + '\n      }\n}\n'
code += 'int main(void) { authenticated=1; readList(); assert(gotAuth==1); authenticated=0; readList(); assert(!gotAuth); }\n'
with tempfile.TemporaryDirectory(prefix='listas-auth-') as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-Isrc', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('listas authenticated GET: PASS')
