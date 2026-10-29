#!/bin/bash
# #202: velocidade de reproducao (lista, passo, rotulo, tempo de relogio).
#   bash tests/velocidade.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-velocidade.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/android_speed_gate.h" <<'PY'
import re
import sys
from pathlib import Path
source = Path('src/video_android.c').read_text()
gate = re.search(r'int  video_velocidade_bloqueada\(void\) \{[^}]*\}', source)
assert gate, 'Android speed gate missing'
Path(sys.argv[1]).write_text(gate.group())
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -I"$work" -Isrc src/velocidade.c tests/velocidade.c -lm -o "$work/t"
"$work/t"
python3 tests/velocidade_android.py
# A ilha e a contagem do proximo episodio contam em tempo de relogio.
# ...com a velocidade MEDIDA, nunca a pedida (TCL 06/10: "ok" com o video a 1x).
grep -q 'vel_tempo_real((double)(duracaoSeg - posSeg), velMed.efetiva)' src/player.c
grep -q 'relogio_taxa(&relLeg, velMed.efetiva / 100.0)' src/player.c
grep -q 'vel_tempo_real(durSeg - posSeg, player_velocidade_efetiva())' src/posplay.c
! grep -q 'relogio_taxa(&relLeg, video_velocidade_atual()' src/player.c
# O .wgt nao oferece a linha (AVPlay setSpeed e trick play inteiro).
grep -q 'int  video_velocidade_suportada(void) { return 0; }' src/video_tizen.c
echo "velocidade: legenda, termina as, proximo episodio (pela efetiva) e .wgt conferidos"
