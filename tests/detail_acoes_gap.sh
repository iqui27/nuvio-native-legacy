#!/bin/bash
# Compile the production action row and renderer geometry, not a parallel model.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-acoes.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 - "${DETAIL_C:-src/detail.c}" "$dir/row.h" <<'PY'
import pathlib, sys
s = pathlib.Path(sys.argv[1]).read_text()
constants = pathlib.Path('src/detail.h').read_text().splitlines()
out = [line for line in constants if line.startswith('#define NV_DETW2_') and any(line.split()[1] == name for name in ('NV_DETW2_X', 'NV_DETW2_BTN_H', 'NV_DETW2_CIRC', 'NV_DETW2_BTN_GAP', 'NV_DETW2_FOCO_SX', 'NV_DETW2_FOCO_SY'))]
# Keep the exact production focus transform; intercept only the drawing boundary.
start = s.index('static void desenhaBotao(')
end = s.index('  if (circular) {', start)
out.append(s[start:end] + '  record(r, icone);\n}\n')
b = pathlib.Path('src/botoes.c').read_text()
start = b.index('void botao_pilula(')
end = b.index('  if (vidro) {', start)
out.append(b[start:end] + '  record(r, -1);\n}\n')
start = s.index('  { float cyBtn = yAcoes +')
end = s.index('  // --- ILHA DE AMIGOS', start)
out.append('static void row(void) { float a = 1;\n' + s[start:end] + '\n}\n')
pathlib.Path(sys.argv[2]).write_text('\n'.join(out))
PY
cc -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable -Isrc -I"$dir" tests/detail_acoes_gap.c -o "$dir/t" -lm
"$dir/t"
