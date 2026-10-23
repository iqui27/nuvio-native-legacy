#!/usr/bin/env python3
"""Run the production style/order handlers without SDL or rendering."""
from pathlib import Path
import os
import subprocess
import tempfile

source = Path('src/ctxmenu.c').read_text()


def block(start):
    begin = source.index(start)
    opening = source.index('{', begin)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


handlers = '\n'.join(block('static ' + signature) for signature in (
    'int estTipo(', 'void estAoVivo(', 'void aplicarEstilo(', 'void ponteiroEstFoco(',
    'void ponteiroEstOk(', 'void ponteiroOrdemToque(',
))
start = source.index('// Pagina de estilos:')
page = source.index('if (pagina == 1) {', start)
keyboard = block(source[page:source.index('\n', page)])
# Select the page-specific occurrence rather than an earlier initialization block.
opening = source.index('{', page)
depth, end = 1, opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
keyboard = source[page:end]
# Exercise the callback actually registered for the desktop Order target.
registration = source[source.index('static void desenhaEstilos(float a)'):]
registration = registration[registration.index('snprintf(rot'):]
registration = registration[:registration.index('linhaEstilo(r, rot,')]
callback = registration.split('ponteiroEstFoco, ')[1].split(',')[0]
alias = '#define ponteiroOrdemAtivar ponteiroOrdemToque\n' if callback == 'ponteiroOrdemAtivar' else ''
program = r'''
#include <assert.h>
#include <stdio.h>
#include "fileiras.h"
enum { SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_RETURN };
static int nEstilos = 2, estFoco, estSel, estTam[FIL_TIPO_N];
static FilEstiloLinha estLin[FIL_TIPO_N];
static int estVivo;
static int ordemLinha = 1, ordemSel, aberto = 1, pagina = 1;
static const char *filChave = "collection_test";
static int savedStyle, savedOrder;
void sync_proteger_ajustes_locais(void) {}
int fil_definir_tipo(const char *key, int type) { (void)key; savedStyle = type; return 1; }
int fil_ordem(const char *key) { (void)key; return savedOrder; }
int fil_definir_ordem(const char *key, int order) { (void)key; savedOrder = order; return 1; }
static int teclaOk(int k) { return k == SDLK_RETURN; }
''' + alias + handlers + '\nstatic void key(int k) {\n' + keyboard + '\n}\n' + r'''
int main(void) {
    estLin[0].n = 1; estLin[0].tipos[0] = FIL_TIPO_CARTAZ;
    estLin[1].n = 2; estLin[1].tipos[0] = FIL_TIPO_COLECAO;
    estLin[1].tipos[1] = FIL_TIPO_DESTAQUE;
    savedStyle = FIL_TIPO_CARTAZ;
    key(SDLK_DOWN); key(SDLK_RIGHT); key(SDLK_DOWN);
    key(SDLK_RIGHT); key(SDLK_RETURN);
    assert(savedStyle == FIL_TIPO_DESTAQUE);
    assert(savedOrder == FIL_ORD_ANO && !aberto);
    aberto = 1; ordemSel = FIL_ORD_LISTA;
    for (int i = 1; i <= FIL_ORD_N; i++) {
        ponteiroEstFoco(nEstilos, 0);
        ORDER_CALLBACK(nEstilos, 0);
        assert(ordemSel == i % FIL_ORD_N);
        assert(aberto);
    }
    key(SDLK_RETURN);
    assert(savedOrder == FIL_ORD_LISTA && savedStyle == FIL_TIPO_DESTAQUE);
    puts("ok  pending style survives Order focus; desktop pointer cycles all four orders once");
}
'''.replace('ORDER_CALLBACK', callback)
with tempfile.TemporaryDirectory(prefix='colordem-menu-') as directory:
    path = Path(directory)
    (path / 'menu.c').write_text(program)
    subprocess.run([os.environ.get('CC', 'cc'), '-Isrc', str(path / 'menu.c'), '-o', str(path / 'menu')], check=True)
    subprocess.run([str(path / 'menu')], check=True)
