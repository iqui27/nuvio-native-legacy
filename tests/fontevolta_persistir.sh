#!/bin/bash
# "Reusar o ultimo link" ligado: uma entrada por episodio, no disco (falso), com
# validade no relogio de parede. Sem rede: sonda falsa, como tests/fontevolta.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontevolta-p.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cat > "$dir/sonda.c" <<'C'
int stream_url_serve(const char *u, const char *c) { (void)u; (void)c; return 0; }
C
if [ "$(uname)" = Darwin ]; then inc=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
else read -r -a inc <<< "$(pkg-config --cflags sdl2)"; fi
cc -O1 -g -Wall -Isrc "${inc[@]}" ${SANITIZE:+-fsanitize=address,undefined} \
  "$dir/sonda.c" tests/fontevolta_persistir.c src/fontefiltro.c src/linguas.c -o "$dir/t" -lpthread
"$dir/t"
