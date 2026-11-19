#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path

rows = {
    fields[1].strip(): fields
    for line in Path('docs/issues/triagem-branches-velhas.md').read_text().splitlines()
    if line.startswith('| agente/')
    for fields in [line.split('|')]
}
checks = {
    'dicatoque already integrated': (
        rows['agente/205-dicatoque'][3].strip() == 'JA_DENTRO'
        and all(text in rows['agente/205-dicatoque'][4] for text in (
            'aj2DicaOk', 'aj2DicaVoltar', 'src/ajustes_ux_v2.inc:513-528',
            'ponteiro_alvo', 'tests/ajustes_ux_toque_modo.c'))
    ),
    'credits equivalent evidence': (
        'ajustes_auto_creditos' not in rows['agente/204-pr381'][4]
        and 'src/ajustes.c:2250' in rows['agente/204-pr381'][4]
        and 'ajustes_pular_creditos' in rows['agente/204-pr381'][4]
    ),
    'P2P path and count': (
        '`tools/arm.sh` (4 ocorrências' in rows['agente/p2p'][4]
    ),
}
for name, passed in checks.items():
    print(f'{"PASS" if passed else "FAIL"}: {name}')
raise SystemExit(0 if all(checks.values()) else 1)
PY
