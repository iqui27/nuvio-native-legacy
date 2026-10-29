#!/bin/bash
# Real host + native backend regression, offline and without a TV.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vod-end.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" "$PWD" <<'PY'
from pathlib import Path
import sys
work, root = map(Path, sys.argv[1:])
(work/'NuGet.Config').write_text('<configuration><packageSources><clear/></packageSources></configuration>')
paths = ['tests/tpk_lifecycle/Test.cs', 'tests/tpk-vod-end.cs', 'tizen-tpk/Video.cs', 'tizen-tpk/VideoWindowMetrics.cs']
(work/'End.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><StartupObject>VodEndTest</StartupObject><EnableDefaultCompileItems>false</EnableDefaultCompileItems></PropertyGroup><ItemGroup>'+''.join('<Compile Include="'+str(root/p)+'"/>' for p in paths)+'</ItemGroup></Project>')
PY
DOTNET_CLI_TELEMETRY_OPTOUT=1 "${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" build "$work/End.csproj" --configfile "$work/NuGet.Config" -c Release --nologo -v q >"$work/build.log" 2>&1 || { tail -40 "$work/build.log"; exit 1; }
"${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" "$work/bin/Release/net8.0/End.dll"
bash tests/tpk-preso.sh
