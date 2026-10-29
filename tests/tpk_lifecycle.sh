#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
[ -x "$dotnet_bin" ] || { echo "unverified: .NET SDK unavailable";exit 1; }
output_base="${NUVIO_HOST_TEST_DIR:-${TMPDIR:-/tmp}}"
work=$(mktemp -d "$output_base/nuvio-lifecycle.XXXXXX")
trap 'rm -rf "$work"' EXIT
project="$PWD/tests/tpk_lifecycle/TpkLifecycle.csproj"
# Select the installed .NET 8 SDK for this net8.0 offline test. A newer SDK
# can request reference packs absent from the machine.
sdk8=$("$dotnet_bin" --list-sdks | awk '$1 ~ /^8\./ {v=$1} END {print v}')
if [ -n "$sdk8" ]; then printf '{"sdk":{"version":"%s"}}\n' "$sdk8" > "$work/global.json"; fi
cd "$work"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$dotnet_bin" build "$project" -c Release --nologo -v q --source "$work" \
  -p:StartupObject=Test -p:BaseIntermediateOutputPath="$work/obj/" -p:OutputPath="$work/bin/" >"$work/build.log" 2>&1 || { cat "$work/build.log";exit 1; }
"$dotnet_bin" "$work/bin/TpkLifecycle.dll"
