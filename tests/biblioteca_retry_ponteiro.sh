#!/bin/bash
# The failed-list "Recarregar" pill must register a pointer target that focuses
# the retry action (the key handler then needs estadoFoco), with the same rect.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import re, sys
s = open('src/biblioteca.c').read()
focar = re.search(r'static void ponteiroRetry\(int a, int b\) \{[^}]*estadoFoco = 1;[^}]*\}', s)
pill = s.index('botao_pilula(button, label, "aj_rotate-cw"')
alvo = re.search(r'ponteiro_alvo\(button\.x, button\.y, button\.w, button\.h, ponteiroRetry,', s[pill:pill + 400])
if not (focar and alvo and s.index('static void ponteiroRetry') < pill):
    sys.exit('biblioteca: retry pill has no pointer target')
print('biblioteca-retry-ponteiro: ok')
PY
