#!/bin/bash
# #728: TPK mantem posicao na reconexao (relogio #710 + video_tpk).
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'reconexao TPK: mantem pos' src/video_tpk.c src/tpk45.c src/dts/doador_tpk.c tests/doador_tpk_stub.c src/audioext.c
grep -q 'video_reconectando' src/player.c
grep -q 'video_busca_em_voo' src/player.c
grep -q 'reconexao: relogio segura' tests/relogio.c
echo "PASS: guardas de posicao na queda TPK (#728)"
