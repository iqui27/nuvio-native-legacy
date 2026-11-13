#!/bin/bash
# #736: package copy naming must be byte-identical to base for default builds
# (flag unset, incl. the pre-existing NuvioTpk40-texto fail-closed name) and
# only gain the -experimental-dts suffix under the opt-in.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv736-name.XXXXXX")
trap 'rm -rf "$work"' EXIT
line=$(grep '  cp "\$TPK" "\$SAIDA/Nuvio-' tools/tpk.sh)
TPK="$work/source"; SAIDA="$work"; VER=1.0
printf payload > "$TPK"

fail() { printf '%s\n' "$1" >&2; exit 1; }

# default path (no experimental suffix): exact base names
SUFFIX=''
for NUVIO_TPK_TEXTO in '' 1; do
  p=NuvioTpk40
  eval "$line"
  name="Nuvio-1.0-NuvioTpk40${NUVIO_TPK_TEXTO:+-texto}.tpk"
  test -f "$work/$name" || fail "default Tpk40 name changed with texto=$NUVIO_TPK_TEXTO"
  rm "$work/$name"
done
p=NuvioTpk60
for NUVIO_TPK_TEXTO in '' 1; do
  eval "$line"
  name="Nuvio-1.0-NuvioTpk60${NUVIO_TPK_TEXTO:+-texto}.tpk"
  test -f "$work/$name" || fail "default Tpk60 name changed with texto=$NUVIO_TPK_TEXTO"
  rm "$work/$name"
done

# experimental path: only the opt-in suffix appears
SUFFIX='-experimental-dts'; NUVIO_TPK_TEXTO=''
p=NuvioTpk40
eval "$line"
test -f "$work/Nuvio-1.0-NuvioTpk40-experimental-dts.tpk" || fail 'experimental Tpk40 name wrong'
test "$(compgen -G "$work/*.tpk" | wc -l)" -eq 1 || fail 'unexpected extra names in output'

printf 'package naming: default identical to base, experimental suffix only under opt-in\n'
