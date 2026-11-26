#!/bin/bash
# A porta de "Peca como fala" de verdade, em 127.0.0.1, falada por curl:
# pagina, estado, pedido, chave, abrir, e o que ela recusa.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'kill $pid 2>/dev/null || true; rm -rf "$tmp"' EXIT
cc -Wall -Wextra -O1 -DNV_PECA_TEST_LOOPBACK tests/pecaserv.c src/pecaserv.c src/peca.c src/js.c src/jsw.c \
   -Isrc -pthread -o "$tmp/serv"
mkfifo "$tmp/in"
"$tmp/serv" < "$tmp/in" > "$tmp/out" &
pid=$!
exec 3> "$tmp/in"
for _ in $(seq 50); do grep -q '^URL ' "$tmp/out" 2>/dev/null && break; sleep 0.1; done
url="$(sed -n 's/^URL //p' "$tmp/out")"
orig="${url%/*}"
falhas=0
ok() { if [ "$2" = "$3" ]; then :; else echo "FALHOU $1: esperava '$3', veio '$2'"; falhas=$((falhas + 1)); fi; }
cod() { curl -s -o /dev/null -w '%{http_code}' --max-time 5 "$@"; }
diz() { echo "$1" >&3; sleep 0.2; tail -n 1 "$tmp/out"; }

ok pagina "$(curl -s --max-time 5 "$url")" "<!doctype html><p>pagina"
ok visto "$(diz v)" "V 1"
ok cabecalhos "$(curl -s -D - -o /dev/null "$url" | grep -ci 'no-store\|nosniff\|default-src')" 3
ok estado-vazio "$(curl -s "$url/s")" "{}"
echo 'e {"e":"livre"}' >&3; sleep 0.2
ok estado "$(curl -s "$url/s")" '{"e":"livre"}'
ok token-errado "$(cod "$orig/aaaaaaaaaaaa")" 404
ok raiz "$(cod "$orig/")" 404
ok host-alheio "$(cod -H 'Host: evil.example' "$url")" 403
ok post-sem-origin "$(cod -X POST -H 'Content-Type: text/plain' -d 'oi' "$url/p")" 403
ok post-origin-alheia "$(cod -X POST -H 'Origin: http://evil.example' -d 'oi' "$url/p")" 403
ok nada-chegou "$(diz p)" "P -"
ok pedir "$(cod -X POST -H "Origin: $orig" -H 'Content-Type: text/plain' --data-binary $'algo  curto\ne engra\xc3\xa7ado' "$url/p")" 204
ok pedido "$(diz p)" $'P algo curto e engra\xc3\xa7ado'
ok pedido-uma-vez "$(diz p)" "P -"
ok pedido-vazio "$(cod -X POST -H "Origin: $orig" --data-binary '   ' "$url/p")" 400
ok chave "$(cod -X POST -H "Origin: $orig" --data-binary $'AIza_teste-1\r\nX: y' "$url/k")" 204
ok chave-limpa "$(diz k)" "K AIza_teste-1Xy"
ok abrir "$(cod -X POST -H "Origin: $orig" --data-binary '2' "$url/a")" 204
ok abrir-indice "$(diz a)" "A 2"
ok abrir-fora "$(cod -X POST -H "Origin: $orig" --data-binary '7' "$url/a")" 204
ok abrir-fora-ignorado "$(diz a)" "A -"
head -c 3000 /dev/zero | tr '\0' 'x' > "$tmp/grande"
ok grande "$(cod -X POST -H "Origin: $orig" --data-binary "@$tmp/grande" "$url/p")" 413
ok metodo "$(cod -X DELETE "$url")" 400
# Conexao lenta (um byte a cada 2 s) nao segura a porta: o prazo e total.
python3 - "${url#http://}" <<'PY' &
import socket, sys, time
h, _, p = sys.argv[1].split('/')[0].partition(':')
s = socket.create_connection((h, int(p)))
try:
    for b in b'GET /':
        s.send(bytes([b])); time.sleep(2)
except OSError:
    pass
PY
lento=$!
sleep 0.5
t0=$(date +%s)
ok lento-nao-segura "$(cod --max-time 8 "$url")" 200
ok lento-prazo "$(( $(date +%s) - t0 <= 6 ))" 1
kill $lento 2>/dev/null || true
# Muitos caminhos errados: a porta se cala (nem o token certo responde).
for _ in $(seq 30); do cod "$orig/bbbbbbbbbbbb" > /dev/null || true; done
ok calada "$(cod "$url" || true)" 000
echo q >&3
wait $pid 2>/dev/null || true
if [ "$falhas" != 0 ]; then echo "pecaserv: $falhas falha(s)"; exit 1; fi
echo "pecaserv: ok"
