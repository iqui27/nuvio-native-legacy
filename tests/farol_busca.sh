#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-busca.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT

flags=(-Wall -Wextra -Werror -O1 -g -std=gnu11 -Isrc -pthread)
[ "$(uname -s)" = Darwin ] || flags+=(-ldl)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac

# 1. Modulo de parse puro: compila e linka sozinho, sem rede, SDL ou pthread.
"${CC:-cc}" "${flags[@]}" tests/farol_busca_parse.c src/farolbuscaparse.c -o "$tmp/parse"
"$tmp/parse"

# 2. Cliente assincrono contra servidor local (padrao aio_network_server.py).
python3 tests/farol_busca_server.py "$tmp/port" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep 0.1; done
test -s "$tmp/port"
base="http://127.0.0.1:$(cat "$tmp/port")"

"${CC:-cc}" "${flags[@]}" \
  tests/farol_busca.c \
  src/farolbusca.c \
  src/farolbuscaparse.c \
  src/farolacha.c \
  src/rede.c \
  src/redeurl.c \
  -o "$tmp/cliente"
"$tmp/cliente" "$base" >"$tmp/log" 2>&1

# Privacidade: nem URL, nem frase nem token sintetico vazam em stdout/stderr.
python3 - "$tmp/log" "$base" <<'PY'
import sys
text = open(sys.argv[1]).read()
assert sys.argv[2] not in text, "URL vazou no log"
assert 'secreto_canario_frase' not in text, "frase vazou no log"
assert 'sint_tok_segredo' not in text, "token vazou no log"
PY

echo "farol_busca: tudo ok"
