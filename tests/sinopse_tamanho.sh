#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d /tmp/nuvio-sinopse.XXXXXX)
trap 'rm -rf "$work"' EXIT
python3 - "$work/sinopse_tamanho_impl.h" <<'PY'
from pathlib import Path
import sys
s = Path('src/text.c').read_text()
a = s.index('enum { PESO_REGULAR')
b = s.index('}', s.index('void txt_estilo_medidas(')) + 1
Path(sys.argv[1]).write_text(s[a:b] + '\n')
PY
cc -Isrc -I"$work" -x c -o "$work/teste" - <<'C'
#include "layout.h"
#include "text.h"
#include <assert.h>
#include "sinopse_tamanho_impl.h"
int main(void) {
  int corpo, peso;
  assert(NV_FT_HERO_SIN == 22);
  assert(NV_LD_HERO_SIN == 30);
  txt_estilo_medidas(TXT_HERO_SIN_GRANDE, &corpo, &peso);
  assert(corpo == 24 && peso == PESO_REGULAR);
  return 0;
}
C
"$work/teste"
