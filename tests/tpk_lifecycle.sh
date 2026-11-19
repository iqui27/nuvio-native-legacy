#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
[ -x "$dotnet_bin" ] || { echo "unverified: .NET SDK unavailable";exit 1; }
output_base="${NUVIO_HOST_TEST_DIR:-${TMPDIR:-/tmp}}"
work=$(mktemp -d "$output_base/nuvio-lifecycle.XXXXXX")
trap 'rm -rf "$work"' EXIT
repo="$PWD"
project="$PWD/tests/tpk_lifecycle/TpkLifecycle.csproj"
# Select the installed .NET 8 SDK for this net8.0 offline test. A newer SDK
# can request reference packs absent from the machine.
sdk8=$("$dotnet_bin" --list-sdks | awk '$1 ~ /^8\./ {v=$1} END {print v}')
if [ -n "$sdk8" ]; then printf '{"sdk":{"version":"%s"}}\n' "$sdk8" > "$work/global.json"; fi
cd "$work"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$dotnet_bin" build "$project" -c Release --nologo -v q --source "$work" \
  -p:StartupObject=Test -p:BaseIntermediateOutputPath="$work/obj/" -p:OutputPath="$work/bin/" >"$work/build.log" 2>&1 || { cat "$work/build.log";exit 1; }
"$dotnet_bin" "$work/bin/TpkLifecycle.dll"

# Wiring do Program.cs (#783): a pausa continua PARANDO o video (#412) e a
# OnResume da plataforma pede a retomada da sessao parada pelo sistema.
python3 - "$repo/tizen-tpk/Program.cs" <<'PY'
from pathlib import Path
import sys
src = Path(sys.argv[1]).read_text()
pause = src.split('protected override void OnPause()', 1)[1].split('protected override void OnLowMemory', 1)[0]
assert 'PausarPeloSistema' in pause, 'OnPause precisa parar a sessao pela Home'
resume = src.split('protected override void OnResume()', 1)[1].split('protected override', 1)[0]
assert 'RetomarDoSistema' in resume, 'OnResume precisa retomar a sessao do sistema'
assert resume.index('Etapa') < resume.index('RetomarDoSistema'), 'log antes da retomada'
print('PASS: wiring OnPause/OnResume do #783')
PY
