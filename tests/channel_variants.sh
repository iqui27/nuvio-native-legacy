#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-channel-variants.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/m3u_resolver.inc" <<'PY'
from pathlib import Path
import sys
s = Path("src/app.c").read_text()
ui = Path("src/guiav5ui.c").read_text()
assert 'gv_hints' not in ui and 'plrui_dicas' not in ui, 'Guide must not draw a control-hint bar'
start = s.index("static int resolverCanalM3u(void)")
end = s.index("// O CARTAO DE ERRO", start)
Path(sys.argv[1]).write_text(s[start:end])
start = s.index("static void *escolherVariantes(void *u)")
end = s.index("static void *escolherFonteStalker", start)
Path(sys.argv[1]).with_name("variant_worker.inc").write_text(s[start:end])
start = s.index('      if (stalker_e_id(player_id_canal())')
end = s.index('      // A MESMA URL DE NOVO', start)
Path(sys.argv[1]).with_name('stalker_watchdog.inc').write_text(s[start:end])
start = s.index('    const char *idAtual = player_id_canal();', s.index('} else if (tipo == FJOB_STALKER)'))
end = s.index('  } else if (tipo == FJOB_VARIANTES)', start)
Path(sys.argv[1]).with_name('stalker_consumer.inc').write_text(s[start:end])
PY
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
extra=()
if [ -f src/backupcategories.c ]; then extra+=(src/backupcategories.c); fi
cc ${extra[@]+"${extra[@]}"} tests/channel_variants.c src/guiaaddons.c src/guiafiltro.c src/js.c src/ltv.c src/guiav5.c src/lembrete.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -I"$work" "${flags[@]}" -lpthread -lm -o "$work/test"
"$work/test" "$work"
cc tests/channel_variants_renewal.c -Isrc -I"$work" -o "$work/renewal"
"$work/renewal"
