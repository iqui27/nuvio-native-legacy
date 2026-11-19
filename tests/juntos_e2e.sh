#!/bin/bash
# Watch Together de ponta a ponta: motor e websocket do app (C) contra o
# nuvio-juntos real em `wrangler dev`, totalmente isolado: porta loopback
# aleatoria, --persist-to temporario, HOME/XDG proprios e .env sintetico vazio.
# Wrangler vem de NUVIO_WRANGLER, node_modules ou do cache instalado
# (tests/servidor_local.sh); sem binario instalado o teste FALHA — nunca
# `npx wrangler@4` (resolveria via registry). Nada remoto.
#   bash tests/juntos_e2e.sh
set -eu
cd "$(dirname "$0")/.."
. tests/servidor_local.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntose2e.XXXXXX")
NV_DEV_PID=""
trap 'nv_dev_stop "$NV_DEV_PID"; rm -rf "$work"' EXIT
NV_W="$(nv_wrangler_bin)" || { echo "FALHOU: sem wrangler 4.149.0 instalado (NUVIO_WRANGLER)"; exit 1; }
nv_isolate_env "$work"
nv_juntos_test_config "$work/wrangler.toml"
SEG=segredo-e2e
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  src/juntosmotor.c src/juntossync.c src/juntosproto.c src/js.c src/discordws.c src/rede.c src/redeurl.c \
  tests/juntos_e2e.c -L/opt/homebrew/lib -lSDL2 -lz -lm -o "$work/t" 2>"$work/cc.log" || { cat "$work/cc.log"; exit 1; }
nv_dev_start "$work/wrangler.toml" "$work" /saude --var "SALA_SEGREDO:$SEG" \
  || { echo "FALHOU: servidor juntos nao subiu"; exit 1; }
PORTA=$NV_DEV_PORT
read -r BA BC < <(node --input-type=module -e "
import { assinar } from './servidor/juntos/src/bilhete.js';
const sala = [...crypto.getRandomValues(new Uint8Array(16))].map((b) => b.toString(16).padStart(2, '0')).join('');
const exp = Math.floor(Date.now() / 1000) + 60;
const a = await assinar('$SEG', { sala, pessoa: 'nuvio:anfitriao', nome: 'Ana', av: '', papel: 'anfitriao', exp, n: 'a1',
  cfg: { ep: 'tt0000001', titulo: 'Teste', imp: { altura: 1080 }, codigo: 'AAAAAA' } });
const c = await assinar('$SEG', { sala, pessoa: 'nuvio:convidado', nome: 'Caio', av: '', papel: 'convidado', exp, n: 'c1' });
console.log(a, c);")
"$work/t" "$PORTA" "$BA" "$BC"
