#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-responsive.XXXXXX")
trap 'rm -rf "$work"' EXIT
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
"$dotnet_bin" build tests/tpk_lifecycle/TpkLifecycle.csproj -c Release --nologo -v q \
  -p:StartupObject=Responsive -p:BaseIntermediateOutputPath="$work/obj/" -p:OutputPath="$work/bin/" >"$work/build.log" 2>&1 || { cat "$work/build.log"; exit 1; }
"$dotnet_bin" "$work/bin/TpkLifecycle.dll"
