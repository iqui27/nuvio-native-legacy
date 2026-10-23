#!/bin/bash
# #205: the source-failure classifier says who owns the problem (add-on/debrid, connection, app/TV).
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontesonda-causa.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -O1 -g -Wall -Wextra -Isrc src/fontesonda.c tests/fontesonda_causa.c -o "$dir/t"
"$dir/t"
# The player card uses it and the new strings are in the table; no URL in the log line.
grep -q 'fontesonda_causa(video_erro_texto()' src/player.c
grep -q '"O servidor da fonte recusou o link (HTTP %d)."' src/idioma_tab.h
! grep -n 'causa do erro' src/player.c | grep -q 'url'
# Stall / timeout cards point at Settings > This TV > Speed test (string is in the table).
grep -q 'Confira Ajustes > Esta TV > Teste de velocidade' src/app.c
grep -q 'Check Settings > This TV > Speed test and make sure the file is smaller' src/idioma_tab.h
grep -q 'motivo == 2 || motivo == 3' src/app.c
echo "ok fontesonda_causa"
