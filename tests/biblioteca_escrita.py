#!/usr/bin/env python3
import pathlib
import subprocess
import tempfile
src = pathlib.Path('src/biblioteca.c').read_text()
a = src.index('static void pollEscrita(void) {')
b = src.index('\n}', a) + 2
code = r'''
#include "listas.h"
#include "listasescrita.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int escrevendo, temAberta = 1, modo, fonte;
static LstLista aberta;
static char abertaMidia[8], descNova[192] = "Description A";
enum { MODO_LISTAS, FONTE_TRAKT };
static int result = LSTESC_OK, operation = LSTESC_DESCRICAO, reloads;
int lstesc_estado(void) { return result; }
int lstesc_qual(void) { return operation; }
long lstesc_lista_id(void) { return 77; }
int lstesc_consumir(void) { return result; }
void lst_abrir(const LstLista *l, const char *m) { (void)l; (void)m; reloads++; }
void lst_minhas_pedir(void) {}
static void pedirFonte(void) {}
static void remapear(int x) { (void)x; }
static void dizer(const char *s) { (void)s; }
'''
code += src[a:b]
code += r'''
int main(void) {
  aberta.fonte = LST_TRAKT; aberta.traktId = 88; strcpy(aberta.descricao, "Description B");
  escrevendo = 1; pollEscrita(); assert(!strcmp(aberta.descricao, "Description B"));
  operation = LSTESC_ORDENAR; escrevendo = 1; pollEscrita(); assert(!reloads);
  result = LSTESC_FALHA; escrevendo = 1; pollEscrita(); assert(!reloads);
  result = LSTESC_OK; operation = LSTESC_DESCRICAO; aberta.traktId = 77;
  escrevendo = 1; pollEscrita(); assert(!strcmp(aberta.descricao, "Description A"));
}
'''
with tempfile.TemporaryDirectory(prefix='biblioteca-escrita-') as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('biblioteca late response: PASS')
