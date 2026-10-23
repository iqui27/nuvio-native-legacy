#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
import pathlib, sys
work = pathlib.Path(sys.argv[1])
source = pathlib.Path('tizen-tpk/Program.cs').read_text()
start = source.index('        static string EtapaAberta(string[] linhas)')
end = source.index('\n        // Aviso', start)
(work / 'Test.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework></PropertyGroup></Project>')
(work / 'Test.cs').write_text('''using System;
using System.Collections.Generic;
class Test {
''' + source[start:end] + '''
static void Main() {
    string[] startup = { "host begin nui-run", "host begin first-frame" };
    Action<string, string[], bool> check = (name, lines, notice) => {
        if ((EtapaAberta(lines) != null) != notice) throw new Exception(name);
    };
    check("owner background eviction", new[] { "host begin nui-run", "host begin first-key", "host note main-window focus=False t=1895,7s calls=90013 frames=71268 paused=True @1895,70s" }, false);
    check("steady foreground eviction", new[] { "host begin nui-run", "host begin first-key", "host begin steady-frames", "host ok steady-frames t=1.0s calls=30 frames=30 paused=False" }, false);
    check("focus loss", new[] { "host begin nui-run", "host begin first-key", "host note main-window focus=False t=2.0s calls=60 frames=60 paused=False" }, false);
    check("before first frame", startup, true);
    check("paused before first frame", new[] { startup[0], startup[1], "host note pause t=1.0s calls=0 frames=0 paused=True" }, true);
    check("not yet steady", new[] { "host begin nui-run", "host begin steady-frames", "host note watchdog calls=1 frames=1 paused=False" }, true);
    check("unfinished operation after steady frames", new[] { "host begin nui-run", "host ok steady-frames", "native begin load", "host note pause calls=90 frames=60 paused=True" }, true);
    check("clean termination", new[] { "host begin nui-run", "host begin first-key", "host note terminate" }, false);
    Console.WriteLine("breadcrumb regression: PASS");
}
}
''')
PY
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$dotnet_bin" build "$work/Test.csproj" --nologo -v q --source "$work"
"$dotnet_bin" "$work/bin/Debug/net8.0/Test.dll"
