#!/usr/bin/env python3
"""Exercise add-item confirmations and failure notices in the production worker."""
import pathlib
import subprocess
import tempfile

code = r'''
#include "listasescrita.h"
#include "ilha.h"
#include "ilhaacao.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *response;
static int invalidations, noticeType, flew;
static char notice[260];
void desc_vertudo_invalidar_lista(long id) { assert(id == 77); invalidations++; }
const char *i18n(const char *s) { return s; }
void ilhaacao_feita(const IlhaAcao *a) {
  snprintf(notice, sizeof notice, "%s", a->frase); noticeType = a->tipo; flew = a->voar;
}
static int sendFixture(const char *m, const char *u, const char *c, char **r) {
  (void)m; (void)u; (void)c; *r = response ? strdup(response) : NULL; return 201;
}
static void check(const char *json, const char *type, int expected, int missing) {
  CatItem ci = {0}; LstLista l = {0};
  strcpy(ci.imdb, "tt101:1:2"); strcpy(ci.tipo, type); strcpy(ci.titulo, "Fixture");
  l.traktId = 77; strcpy(l.titulo, "Own");
  response = json; int before = invalidations;
  assert(lstesc_adicionar_titulo(&ci, &l)); lstesc_teste_esperar();
  assert(lstesc_estado() == expected); lstesc_passo();
  assert(invalidations == before + (expected == LSTESC_OK));
  assert(noticeType == (expected == LSTESC_OK ? ILHA_ACENTO : ILHA_ERRO));
  assert(flew == (expected == LSTESC_OK));
  if (missing) assert(!strcmp(notice, "Nada encontrado para “Fixture”."));
  else assert(!strcmp(notice, expected == LSTESC_OK ? "Adicionado a Own" : "Não foi possível adicionar à lista"));
}
int main(void) {
  lstesc_teste_envio(sendFixture);
  check("{\"added\":{\"movies\":0},\"existing\":{\"movies\":0},\"not_found\":{\"movies\":[{\"ids\":{\"imdb\":\"tt101\"}}]}}", "movie", LSTESC_FALHA, 1);
  check("{\"added\":{\"movies\":1}}", "movie", LSTESC_OK, 0);
  check("{\"existing\":{\"movies\":1}}", "movie", LSTESC_OK, 0);
  check("{\"added\":{\"shows\":1}}", "series", LSTESC_OK, 0);
  check("{\"existing\":{\"shows\":1}}", "series", LSTESC_OK, 0);
  check("{\"added\":{\"shows\":1},\"existing\":{\"movies\":0}}", "movie", LSTESC_FALHA, 0);
  check("{}", "movie", LSTESC_FALHA, 0);
  check(NULL, "movie", LSTESC_FALHA, 0);
}
'''
with tempfile.TemporaryDirectory(prefix='listas-adicionar-') as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-ffunction-sections', '-fdata-sections', '-Isrc',
                    '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    'src/listasescrita.c', 'src/js.c', str(p / 'test.c'),
                    '-Wl,-dead_strip', '-Wl,-undefined,dynamic_lookup', '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('listas add-item response: PASS')
