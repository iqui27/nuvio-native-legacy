#!/usr/bin/env python3
"""Run production cache functions and list writes against a tiny GET fixture."""
import pathlib
import subprocess
import tempfile

def function(src, name):
    start = src.index('void ' + name + '(')
    end = src.index('\n}', start) + 2
    return src[start:end]

src = pathlib.Path('src/descoberta.c').read_text()
functions = function(src, 'desc_vertudo_fonte')
if 'void desc_vertudo_invalidar_lista(' in src:
    functions += '\n' + function(src, 'desc_vertudo_invalidar_lista')
else:
    functions += '\nvoid desc_vertudo_invalidar_lista(long id) { (void)id; }'
code = r'''
#include "colecoes.h"
#include "listasescrita.h"
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static pthread_mutex_t vtTrava = PTHREAD_MUTEX_INITIALIZER;
static ColSource vtFonte;
static int vtProvedor, vtN, vtPagina, vtFim, vtErro;
static unsigned vtGeracao;
static char vtBase[600];
static int getCount, firstId = 100;
static char note[32] = "old";
static void vtDisparar(void) { getCount++; vtN = 3; firstId = 101; strcpy(note, "saved"); }
'''
code += functions
code += r'''
static int sendFixture(const char *m, const char *u, const char *c, char **r) {
  (void)m; (void)u; (void)c; *r = strdup(""); return 204;
}
int main(void) {
  ColSource s = {0}; strcpy(s.prov, "trakt"); s.traktLista = 77;
  desc_vertudo_fonte(&s); firstId = 100; strcpy(note, "old");
  int oldGets = getCount; unsigned oldGeneration = vtGeracao;
  lstesc_teste_envio(sendFixture);
  assert(lstesc_ordenar(77, (long[]){101,102,100}, 3)); lstesc_teste_esperar();
  assert(lstesc_consumir() == LSTESC_OK);
  desc_vertudo_fonte(&s);
  assert(getCount > oldGets && vtGeracao > oldGeneration && firstId == 101);
  strcpy(note, "old"); oldGets = getCount;
  assert(lstesc_nota(77, 101, "saved")); lstesc_teste_esperar(); lstesc_consumir();
  desc_vertudo_fonte(&s); assert(getCount > oldGets && !strcmp(note, "saved"));
  oldGets = getCount; desc_vertudo_invalidar_lista(88); desc_vertudo_fonte(&s);
  assert(getCount == oldGets);
}
'''
with tempfile.TemporaryDirectory(prefix='listas-cache-') as tmp:
    path = pathlib.Path(tmp)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', '-ffunction-sections', '-fdata-sections', '-Isrc',
                    '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    'src/listasescrita.c', str(path / 'test.c'), '-Wl,-dead_strip',
                    '-Wl,-undefined,dynamic_lookup', '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('listas cache: PASS')
