#!/bin/bash
# Watch Together de ponta a ponta: motor e websocket do app (C) contra o
# nuvio-juntos real em `wrangler dev`. Precisa de servidor/juntos/node_modules
# (cd servidor/juntos && npm install); sem ele, SKIP. Nada remoto.
#   bash tests/juntos_e2e.sh
set -eu
cd "$(dirname "$0")/.."
W=servidor/juntos/node_modules/.bin/wrangler
[ -x "$W" ] || { echo "SKIP: sem wrangler em servidor/juntos (npm install)"; exit 0; }
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntose2e.XXXXXX")
PORTA=$((8600 + RANDOM % 90))
SEG=segredo-e2e
(cd servidor/juntos && WRANGLER_SEND_METRICS=false exec node_modules/.bin/wrangler dev --port $PORTA --ip 127.0.0.1 \
  --var SALA_SEGREDO:$SEG --persist-to "$work/estado" >"$work/dev.log" 2>&1) &
DEV=$!
trap 'pkill -P $DEV 2>/dev/null; kill $DEV 2>/dev/null; rm -rf "$work"; true' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  src/juntosmotor.c src/juntossync.c src/juntosproto.c src/js.c src/discordws.c src/rede.c src/redeurl.c \
  tests/juntos_e2e.c -L/opt/homebrew/lib -lSDL2 -lz -lm -o "$work/t" 2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
for i in $(seq 1 120); do curl -sf "http://127.0.0.1:$PORTA/saude" >/dev/null && break; sleep 0.25; done
read -r BA BC < <(node --input-type=module -e "
import { assinar } from './servidor/juntos/src/bilhete.js';
const sala = [...crypto.getRandomValues(new Uint8Array(16))].map((b) => b.toString(16).padStart(2, '0')).join('');
const exp = Math.floor(Date.now() / 1000) + 60;
const a = await assinar('$SEG', { sala, pessoa: 'nuvio:anfitriao', nome: 'Ana', av: '', papel: 'anfitriao', exp, n: 'a1',
  cfg: { ep: 'tt0000001', titulo: 'Teste', imp: { altura: 1080 }, codigo: 'AAAAAA' } });
const c = await assinar('$SEG', { sala, pessoa: 'nuvio:convidado', nome: 'Caio', av: '', papel: 'convidado', exp, n: 'c1' });
console.log(a, c);")
"$work/t" "$PORTA" "$BA" "$BC"
