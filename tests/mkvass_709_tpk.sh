#!/bin/bash
# #709: cap mkvass parallel ranges on TPK (Tizen 6 anime ASS)
set -eu
cd "$(dirname "$0")/.."
grep -q 'MKVASS_PARALELOS_TPK60' src/mkvass.c
grep -q '#709' src/mkvass.c
grep -q 'f->paralelos > MKVASS_PARALELOS_TPK60' src/mkvass.c
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-709.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Replay do trecho real de producao: a reducao de paralelos so pode agir na
# Tizen 6. A libnuvio.so e a mesma nos pacotes 60/65/8/9 e o TPK40 reusa os
# objetos, entao a decisao tem de ser em tempo de execucao.
python3 - "$work" <<'PY'
from pathlib import Path
import re, sys
src = Path('src/mkvass.c').read_text()
m = re.search(r'#ifdef NV_TPK\n(.*?)#endif', src[src.index('if (!ehAss || !priv) return -1;'):], re.S)
assert m, 'trecho #709 nao achado'
corpo = m.group(1)
Path(sys.argv[1], 'repro709.c').write_text('''
#include <stdlib.h>
#include <assert.h>
#define NV_TPK 1
#define MKVASS_PARALELOS 3
#define MKVASS_PARALELOS_TPK60 2
typedef struct { int paralelos; } Fio;
static int tizenMaior;
static void video_tpk_tizen_definir(const char *v) { tizenMaior = v ? atoi(v) : 0; }
static int video_tpk_tizen_maior(void) { return tizenMaior; }
static int decide(Fio *f) {
%s
  return f->paralelos;
}
int main(void) {
  Fio f;
  const char *comCap[] = { "6.0", "6.5" };
  const char *semCap[] = { "4.0", "5.0", "5.5", "8.0", "9.0", "0", 0 };
  int i;
  for (i = 0; i < 2; i++) {
    f.paralelos = 3; video_tpk_tizen_definir(comCap[i]);
    assert(decide(&f) == 2);
  }
  for (i = 0; semCap[i]; i++) {
    f.paralelos = 3; video_tpk_tizen_definir(semCap[i]);
    assert(decide(&f) == 3);
  }
  /* Sem versao conhecida (platform.version falhou): nao reduz. */
  f.paralelos = 3; video_tpk_tizen_definir(0);
  assert(decide(&f) == 3);
  /* Teto ja baixo nao sobe. */
  f.paralelos = 1; video_tpk_tizen_definir("6.0");
  assert(decide(&f) == 1);
  return 0;
}
''' % corpo)
PY
cc -std=gnu11 -O1 -Wall -Wextra -o "$work/repro709" "$work/repro709.c"
"$work/repro709"
cc -std=gnu11 -O1 -DNV_MKVFIX_SEM_REDE -DNV_TPK -Isrc -c -o "$work/mkvass709.o" src/mkvass.c 2>/dev/null || cc -std=gnu11 -O1 -DNV_TPK -Isrc -fsyntax-only src/mkvass.c
echo "mkvass_709_tpk: PASS"
