#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
[ -x "$dotnet_bin" ] || { echo "unverified: .NET SDK unavailable";exit 1; }
output_base="${NUVIO_HOST_TEST_DIR:-${TMPDIR:-/tmp}}"
work=$(mktemp -d "$output_base/nuvio-window-metrics.XXXXXX")
trap 'rm -rf "$work"' EXIT
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$dotnet_bin" build tests/tpk_window_metrics/TpkWindowMetrics.csproj -c Release --nologo -v q \
  -p:BaseIntermediateOutputPath="$work/obj/" -p:OutputPath="$work/bin/" >"$work/build.log" 2>&1 || { cat "$work/build.log";exit 1; }
"$dotnet_bin" "$work/bin/TpkWindowMetrics.dll"
