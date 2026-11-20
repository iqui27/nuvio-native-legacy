#!/bin/bash
# Both P1 regressions: real Library input/rendering and Foundation HTTP receipt.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'kill "${server:-}" 2>/dev/null || true; rm -rf "$work"' EXIT
python3 tests/ios_m2_http_server.py >"$work/port" &
server=$!
for ((i=0; i<100; i++)); do
  [ -s "$work/port" ] && break
  sleep .05
done
python3 - "$work/http.m" <<'PY'
from pathlib import Path
import sys
host = Path('ios/ios.m').read_text()
if Path('ios/http.inc').exists():
    transport = Path('ios/http.inc').read_text()
    pedido = Path('src/rede_pedido.inc').read_text()
    assert 'c->tetoCorpo' in pedido
    if 'nv_host_http_limit_cancel' in transport:
        transport = '#define NV_HTTP_CANCEL_TEST 1\n' + transport
    assert 'bytes == -1 ? REDE_LIMITE_CORPO' in pedido
else:
    transport = host[host.index('char *nv_host_http('):host.index('long ios_ram_mb(')]
    transport += '''
static char *nv_host_http_limit(const char *v, const char *u, const char *h,
 const char *b, int ms, size_t cap, int *s, int *ra, long *n, char *e, size_t ne) {
 (void)cap; return nv_host_http(v,u,h,b,ms,s,ra,n,e,ne);
}
'''
Path(sys.argv[1]).write_text('#import <Foundation/Foundation.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\n#include <limits.h>\n#include <errno.h>\n' + transport + Path('tests/ios_m2_http.m').read_text())
PY
cc -fobjc-arc -fblocks "$work/http.m" -framework Foundation -o "$work/http"
http_rc=0
"$work/http" "$(<"$work/port")" || http_rc=$?
if [ "${1:-}" = http ]; then exit "$http_rc"; fi
# Reuse the existing GL harness, adding real pointer and D-pad assertions.
export NUVIO_M2_CONTROLS=1
library_rc=0
bash tests/biblioteca_retrato.sh || library_rc=$?
search_rc=0
bash tests/busca_toque_modo.sh || search_rc=$?
[ "$http_rc" = 0 ] && [ "$library_rc" = 0 ] && [ "$search_rc" = 0 ]
