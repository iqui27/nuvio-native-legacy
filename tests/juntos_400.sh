#!/bin/bash
# Production POST body, offline JSON.parse and the server's actual episode regex.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-400.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -DNV_ANDROID -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/juntos_400.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/body"
"$work/body" "$work/body.jsonl" >"$work/log"
node --input-type=module - "$work/body.jsonl" <<'JS'
import fs from 'node:fs';
import assert from 'node:assert/strict';
const source = fs.readFileSync('servidor/recomendacoes/src/sala.js', 'utf8');
const pattern = source.match(/if \(!\/(.+)\/\.test\(ep\)\) return erro\("ep invalido", 400\)/);
assert(pattern, 'server validation rule found');
const valid = new RegExp(pattern[1]);
const bodies = [];
for (const line of fs.readFileSync(process.argv[2], 'utf8').trim().split('\n')) {
  const body = JSON.parse(line);
  assert(valid.test(body.ep), `400: ep invalido (${body.ep})`);
  bodies.push(body);
}
assert.deepEqual(bodies.map(b => b.ep), ['tt4158110:1:2', 'tt4158110:1:2', 'tt4158110:1:2', 'tt4158110:2:3', 'tt4158110']);
assert.equal(bodies[4].imp.arquivo.length, 120);
assert.equal(bodies[4].imp.binge.length, 80);
assert.equal(bodies[4].pos, 0);
assert.equal(bodies[4].titulo.length, 159);
assert.equal(bodies[4].poster.length, 511);
assert.equal(bodies[4].convidados.length, 10);
assert(bodies[4].convidados.every(id => id.length === 95));
console.log('juntos_400: episode normalization, complete escaped JSON, explicit errors OK');
JS
rg -qF '[juntos] criar sala falhou (400: ep invalido)' "$work/log"
rg -qF '[juntos] criar sala falhou (400: json invalido)' "$work/log"
if rg -q 'token-secret|private.invalid' "$work/log"; then
  echo 'FAIL: private server text leaked'; exit 1
fi
