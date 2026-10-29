#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import subprocess
import tempfile

source = Path('src/vertudo.c').read_text()
start = source.index('void vertudo_perf_cycle(void) {')
end = source.index('\n}\n', start) + 3
code = '''
#include <assert.h>
#include <stddef.h>
typedef struct { int nSources; } ColFolder;
static ColFolder folder = {1};
static const ColFolder *collection;
static int perftv_enabled = 1, aberta = 1, available = 1;
static int layEscolha, source, tabFocus = 1, opened, cycled;
enum { VT_LAY_GRADE = 1 };
static const ColFolder *col_folder(int i) { assert(i == 0); return available ? &folder : NULL; }
static void vertudo_colecao(const ColFolder *f) { collection = f; aberta = 1; opened++; }
static void resolverLayout(void) {}
static int primeiraFonte(void) { assert(collection); return 0; }
static void openSource(void) { assert(collection); }
static void alternarExibicao(void) { assert(collection); cycled++; }
''' + source[start:end] + '''
int main(void) {
  /* Standalone See all must acquire a collection before any layout cycle. */
  vertudo_perf_cycle();
  assert(collection == &folder && opened == 1 && cycled == 0);
  assert(layEscolha == VT_LAY_GRADE && tabFocus == 0);
  for (int i = 0; i < 20; i++) vertudo_perf_cycle();
  assert(opened == 1 && cycled == 20);
  collection = NULL; available = 0;
  vertudo_perf_cycle();
  assert(!collection && opened == 1 && cycled == 20);
  available = 1; folder.nSources = 0;
  vertudo_perf_cycle();
  assert(!collection && opened == 1 && cycled == 20);
  folder.nSources = 1; perftv_enabled = 0;
  vertudo_perf_cycle();
  assert(!collection && opened == 1 && cycled == 20);
  perftv_enabled = 1; aberta = 0;
  vertudo_perf_cycle();
  assert(collection == &folder && aberta && opened == 2 && cycled == 20);
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'cycle.c'
    binary = Path(directory) / 'cycle'
    c.write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('vertudo_perf_cycle: PASS')
PY
