#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-trava.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 - "$dir/test.c" <<'PY'
from pathlib import Path
import sys
s = Path('src/player.c').read_text()
worker = s[s.index('typedef struct { int perfil'):s.index('static int travaFonte(')]
Path(sys.argv[1]).write_text('''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
typedef struct { int n, perfil[8]; } Trava;
static _Atomic int travaPinResultado;
static int travaId = 7;
static int perfis_verificar_pin(int p, const char *pin) { return p == atoi(pin); }
static int trava_obter(int id, Trava *t) { assert(id == 7); t->n = 3; t->perfil[0]=1; t->perfil[1]=2; t->perfil[2]=3; return 1; }
''' + worker + '''
int main(void) {
  for (int pin = 1; pin <= 4; pin++) {
    TravaPinJob *j = calloc(1, sizeof *j);
    j->perfil = 1; j->id = travaId;
    snprintf(j->pin, sizeof j->pin, "%d", pin);
    verificarTravaPin(j);
    assert(atomic_load(&travaPinResultado) == (pin == 2 || pin == 3 ? 1 : -1));
  }
  return 0;
}
'''.replace('int main(void)', '#include <stdio.h>\nint main(void)'))
PY
cc -std=c11 -Wall -Wextra "$dir/test.c" -o "$dir/test"
"$dir/test"
