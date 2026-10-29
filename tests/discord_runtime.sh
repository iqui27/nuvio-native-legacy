#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Deterministic transport/auth lifecycle tests: no network or account.
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -pthread)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/discordws_local.c -o ${TMPDIR:-/tmp}/nuvio-discordws-local
${TMPDIR:-/tmp}/nuvio-discordws-local
cc "${flags[@]}" tests/discord_lifecycle.c src/js.c src/jsw.c -o ${TMPDIR:-/tmp}/nuvio-discord-lifecycle
${TMPDIR:-/tmp}/nuvio-discord-lifecycle
cc "${flags[@]}" -DNV_TPK40 tests/discord_lifecycle.c src/js.c src/jsw.c -o ${TMPDIR:-/tmp}/nuvio-discord-lifecycle-tpk40
${TMPDIR:-/tmp}/nuvio-discord-lifecycle-tpk40
cc "${flags[@]}" -ffunction-sections -fdata-sections -Wl,-dead_strip tests/discord_tls.c src/js.c src/redeurl.c -o ${TMPDIR:-/tmp}/nuvio-discord-tls
${TMPDIR:-/tmp}/nuvio-discord-tls
