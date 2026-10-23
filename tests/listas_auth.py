#!/usr/bin/env python3
"""Exercise production Trakt list GET headers, including persisted own pins."""
import pathlib
import subprocess
import tempfile
src = pathlib.Path('src/descoberta.c').read_text()
a = src.index('      const char *cli=nuvem_trakt_cliente();', src.index('static void *fioVerTudo'))
b = src.index('        if (corpo) {', a)
code = r'''
#include "listas.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static int authenticated, gotAuth;
static ColSource selected;
static char *pins;
static const char *linkedSlug = "own-slug";
int perfis_ativo(void) { return 0; }
char *dados_ler(const char *name) { assert(!strcmp(name, "listas.txt")); return pins ? strdup(pins) : NULL; }
int dados_gravar(const char *name, const char *text) {
  assert(!strcmp(name, "listas.txt")); free(pins); pins = strdup(text); return 1;
}
int col_extra_definir(const ColFolder *v, int n) { (void)v; (void)n; return 0; }
const char *recomenda_identidade_trakt(void) { return linkedSlug; }
void desc_vertudo_fonte(const ColSource *s) { selected = *s; }
static const char *nuvem_trakt_cliente(void) { return "fixture-key"; }
int trakt_cabecalhos(const char **cab, char *a, size_t na, char *k, size_t nk) {
  if (!authenticated) return 0;
  snprintf(a, na, "Authorization: Bearer fixture"); snprintf(k, nk, "trakt-api-key: fixture-key");
  cab[0]=a; cab[1]="trakt-api-version: 2"; cab[2]=k; cab[3]=NULL; return 1;
}
static char *rede_baixar_com(const char *url, int t, const char *const *cab) {
  assert(!strncmp(url, "https://api.trakt.tv/", 21));
  assert(strstr(url, "/lists/77/items/movie")); assert(t == 12);
  gotAuth = 0;
  for (int i=0; cab[i]; i++) if (!strncmp(cab[i], "Authorization: Bearer ", 22)) gotAuth++;
  return NULL;
}
static void getSelected(void) {
  ColSource fonte = selected;
  int skip=1; char url[1024]; char *corpo;
'''
code += src[a:b] + '\n      }\n}\n'
code += r'''
static void readList(int envelope) {
  const char *own = "[{\"name\":\"Own\",\"ids\":{\"trakt\":77},\"privacy\":\"private\"}]";
  const char *other = "[{\"list\":{\"name\":\"Other\",\"ids\":{\"trakt\":77},\"user\":{\"ids\":{\"slug\":\"other\"}}}}]";
  assert(lst_ler_trakt(envelope ? other : own, envelope) == 1);
  lst_abrir(lst_lista(0), "MOVIE"); getSelected();
}
static void reopenPin(int own) {
  lst_esquecer(); lst_pedir_fixadas();
  assert(lst_n() == 1);
  assert(lst_lista(0)->propria == own);
  lst_abrir(lst_lista(0), "MOVIE"); getSelected(); assert(gotAuth == own);
}
int main(void) {
  authenticated=1; readList(0); assert(gotAuth==1); readList(1); assert(!gotAuth);
  authenticated=0; readList(0); assert(!gotAuth); readList(1); assert(!gotAuth);
  authenticated=1;
  readList(0); assert(lst_alternar_fixada(lst_lista(0))); reopenPin(1);
  assert(lst_alternar_fixada(lst_lista(0)) == 0);
  readList(1); assert(lst_alternar_fixada(lst_lista(0))); reopenPin(0);
  free(pins); pins = strdup("trakt\t77\tMOVIE\t0\tLegacy own\town-slug\n");
  reopenPin(1); linkedSlug = ""; reopenPin(0);
  linkedSlug = "own-slug";
  free(pins); pins = strdup("trakt\t77\tMOVIE\t0\tLegacy other\tother-slug\n"); reopenPin(0);
  free(pins); pins = strdup("trakt\t77\tMOVIE\t0\tUnknown owner\t\t0\n"); reopenPin(0);
  free(pins);
}
'''
with tempfile.TemporaryDirectory(prefix='listas-auth-') as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    '-ffunction-sections', '-fdata-sections', 'src/listas.c', 'src/js.c',
                    str(p / 'test.c'), '-Wl,-dead_strip', '-Wl,-undefined,dynamic_lookup',
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('listas authenticated GET and persisted pins: PASS')
