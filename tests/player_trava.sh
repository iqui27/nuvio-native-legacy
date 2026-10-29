#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-trava.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 - "$dir/test.c" <<'PY'
from pathlib import Path
import sys
s = Path('src/travaui.c').read_text()
worker = s[s.index('static int pinFio(void *u) {'):s.index('static void conferirPin(void) {')]
Path(sys.argv[1]).write_text('''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int mtx, pinResposta, pinFioVivo;
static char pin[5];
static void SDL_LockMutex(int unused) { (void)unused; }
static void SDL_UnlockMutex(int unused) { (void)unused; }
static int perfis_verificar_pin(int p, const char *value) { return p == atoi(value); }
''' + worker + '''
int main(void) {
  for (int value = 1; value <= 4; value++) {
    int *members = calloc(3, sizeof *members);
    members[0] = 2; members[1] = 2; members[2] = 3;
    snprintf(pin, sizeof pin, "%d", value);
    pinFioVivo = 1;
    pinFio(members);
    assert(pinResposta == (value == 2 || value == 3));
    assert(pinFioVivo == 0);
  }
  return 0;
}
'''.replace('int main(void)', '#include <stdio.h>\nint main(void)'))
PY
cc -std=c11 -Wall -Wextra "$dir/test.c" -o "$dir/test"
"$dir/test"
