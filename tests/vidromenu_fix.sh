#!/bin/bash
# Focused review regressions; local GL only, no network/install/deploy.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/vidromenu/fix
mkdir -p "$out/after" "$out/opaque"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detglass-dados-fix.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
bash tests/vidro_acento.sh "$out/after"
export NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1 NUVIO_SHOT_MENU_PROBE=1
NUVIO_SHOT_MENU=1 bash tests/detalhe_glass_shot.sh "$out/after" filme-topo
if NUVIO_SHOT_MENU=1 NUVIO_SHOT_MENU_OPAQUE=1 "${OBJ:-${TMPDIR:-/tmp}/nuvio-detglass-obj}/shot" "$out/opaque" filme-topo > "$out/sidebar-opaque.log" 2>&1; then
  echo 'FAIL: opaque detail sidebar accepted'; exit 1
fi
rg -q 'FAIL menu-frost' "$out/sidebar-opaque.log"
NUVIO_AJ_QUADROS=v2-menu NUVIO_SHOT_MENU=2 bash tests/ajustes_shot.sh "$out/after/ajustes"
if NUVIO_AJ_QUADROS=v2-menu NUVIO_SHOT_MENU=2 NUVIO_SHOT_MENU_OPAQUE=1 "${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-ajustes-shot}" "$out/opaque/ajustes" > "$out/settings-opaque.log" 2>&1; then
  echo 'FAIL: opaque Settings sidebar accepted'; exit 1
fi
rg -q 'FAIL menu-frost' "$out/settings-opaque.log"
echo 'PASS: accent frost, detail/Settings texture response, opaque rejection'
