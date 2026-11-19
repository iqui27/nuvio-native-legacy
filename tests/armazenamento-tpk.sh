#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-espaco-tpk.XXXXXX)
trap 'rm -rf "$dir"' EXIT
export DOTNET_CLI_HOME="$dir" DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
cat > "$dir/Test.csproj" <<PROJECT
<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework></PropertyGroup><ItemGroup><Compile Include="$PWD/tizen-tpk/Carga.cs" /></ItemGroup></Project>
PROJECT
cat > "$dir/Test.cs" <<'CS'
using System;
using System.IO;
using NuvioTpk;
class Test {
    static void Main(string[] args) {
        string d=args[0];
        foreach(string n in new[] {"libnuvio.staged.so","libnuvio-1.so","libnuvio-2.so","libnuvio.previous.so","libnuvio.download","sessao.txt","libnuvio_engine.so"})
            File.WriteAllText(Path.Combine(d,n),"teste");
        NvCarga.LimparAntigos(d);
        foreach(string n in new[] {"libnuvio-1.so","libnuvio-2.so","libnuvio.previous.so","libnuvio.download"})
            if(File.Exists(Path.Combine(d,n))) throw new Exception("sobrou " + n);
        foreach(string n in new[] {"libnuvio.staged.so","sessao.txt","libnuvio_engine.so"})
            if(!File.Exists(Path.Combine(d,n))) throw new Exception("apagou " + n);
        Console.WriteLine("PASS staged: pacote + staged; dados e motor preservados");
    }
}
CS
"${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" build "$dir/Test.csproj" --nologo -v q -o "$dir/bin"
mkdir "$dir/dados"
"${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" "$dir/bin/Test.dll" "$dir/dados"
