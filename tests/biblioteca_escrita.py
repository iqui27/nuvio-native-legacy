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
static void abrirStub(const LstLista *l, const char *m) { (void)l; (void)m; reloads++; }
static void minhasStub(void) {}
#define lst_abrir abrirStub
#define lst_minhas_pedir minhasStub
static void pedirFonte(void) {}
static void remapear(int x) { (void)x; }
static void dizer(const char *s) { (void)s; }
'''
code += src[a:b]
a = src.index('static void abrirLista(int i) {')
b = src.index('\n}', a) + 2
code += '\nstatic int abertaPropria, edModo, ordemPega, ordemMudou, acaoSel;\n'
code += 'enum { ED_NADA }; static char recado[160]; static struct { int fileira, coluna; } foco;\n'
code += src[a:b]
code += r'''
int main(void) {
  aberta.fonte = LST_TRAKT; aberta.traktId = 88; strcpy(aberta.descricao, "Description B");
  escrevendo = 1; pollEscrita(); assert(!strcmp(aberta.descricao, "Description B"));
  operation = LSTESC_ORDENAR; escrevendo = 1; pollEscrita(); assert(!reloads);
  result = LSTESC_FALHA; escrevendo = 1; pollEscrita(); assert(!reloads);
  assert(lst_ler_trakt("[{\"name\":\"A\",\"ids\":{\"trakt\":77},\"description\":\"Old\"},"
                       "{\"name\":\"B\",\"ids\":{\"trakt\":88},\"description\":\"Description B\"}]", 0) == 2);
  lst_minhas_teste("[{\"name\":\"A\",\"ids\":{\"trakt\":77},\"description\":\"Old\"}]");
  result = LSTESC_OK; operation = LSTESC_DESCRICAO; abrirLista(0);
  escrevendo = 1; pollEscrita(); assert(!strcmp(aberta.descricao, "Description A"));
  LstLista own; assert(lst_minha(0, &own) && !strcmp(own.descricao, "Description A"));
  temAberta = 0; abrirLista(0); assert(!strcmp(aberta.descricao, "Description A"));
  abrirLista(1); assert(!strcmp(aberta.descricao, "Description B"));
  escrevendo = 1; pollEscrita();
  abrirLista(0); assert(!strcmp(aberta.descricao, "Description A"));
  result = LSTESC_FALHA; strcpy(descNova, "Rejected"); escrevendo = 1; pollEscrita();
  abrirLista(0); assert(!strcmp(aberta.descricao, "Description A"));
  result = LSTESC_OK; descNova[0] = 0; escrevendo = 1; pollEscrita();
  abrirLista(0); assert(!aberta.descricao[0]);
}
'''
with tempfile.TemporaryDirectory(prefix='biblioteca-escrita-') as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-Isrc', '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                    '-ffunction-sections', '-fdata-sections', 'src/listas.c', 'src/js.c',
                    str(p / 'test.c'), '-Wl,-dead_strip', '-Wl,-undefined,dynamic_lookup',
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('biblioteca late response: PASS')
