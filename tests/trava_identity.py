"""Exercise the production override cache across identity transitions."""
from pathlib import Path
import subprocess
import tempfile

ui = Path('src/travaui.c').read_text()
cache = ui.split('// Episodios liberados', 1)[1].split('static int alcancouPerfis', 1)[0]
cache = cache[cache.index('static struct'):]
start = ui.index('static int identidadeMudou(') if 'static int identidadeMudou(' in ui else ui.index('static int liberado(')
functions = ui[start:ui.index('int travaui_segura(')]
program = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned identity;
static unsigned trava_identidade_geracao(void) { return identity; }
''' + cache + functions + r'''
int main(void) {
  liberar("tt1", 1, 5); assert(liberado("tt1", 1, 5));
  identity++; assert(!liberado("tt1", 1, 5)); /* profile B */
  identity++; assert(!liberado("tt1", 1, 5)); /* back to A */
  liberar("tt1", 1, 5); identity++; /* another account, same profile */
  assert(!liberado("tt1", 1, 5));
  liberar("tt1", 1, 5); identity += 2; /* logout/login between reads */
  assert(!liberado("tt1", 1, 5));
  liberar("tt1", 1, 5); assert(liberado("tt1", 1, 5));
  assert(!liberado("tt1", 1, 6));
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'identity.c'
    exe = Path(directory) / 'identity'
    c.write_text(program)
    subprocess.run(['cc', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('override identity scope: ok')
