#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Contrato do #754: o OnUpdate(IntPtr,IntPtr) do TPK40 recebe os ENDERECOS das
# variaveis do GLApplication::Run com os handles EGL (lea, nao mov, no runtime
# 5.5) e tem de desreferencia-los antes do eglSwapBuffers; superficie zero num
# endereco valido nao apresenta; "ok first-frame" exige EGL_TRUE.
#
# Compila o corpo REAL do metodo (extraido de Program40.cs para Host.cs) num
# console .NET que chama o callback como o runtime chama: dois slots IntPtr
# com display/superficie dentro. O stub de eglSwapBuffers conta chamadas com
# argumentos que nao sao os handles 0xd1/0x51.
# Precisa de dotnet e python3; sem eles, avisa e sai 0.
set -eu
cd "$(dirname "$0")/.."
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}" PATH="$HOME/.dotnet:$PATH" DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
command -v dotnet >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1 || { echo "pulado: sem dotnet ou python3"; exit 0; }
T=$(mktemp -d "${TMPDIR:-/tmp}/nv-tpk40-swap.XXXXXX")
trap 'rm -rf "$T"' EXIT
cp tests/tpk40_swap/Tpk40Swap.csproj tests/tpk40_swap/Runner.cs "$T/"
python3 - "$T/Host.cs" <<'PY'
import re, sys
src = open('tizen-tpk/NuvioTpk40/Program40.cs').read()
m = re.search(r'protected override void OnUpdate\(IntPtr eglDisplay, IntPtr eglSurface\)\s*\{', src)
assert m, 'FALHA: OnUpdate(IntPtr,IntPtr) ausente'
i = m.end() - 1
depth = 0
for j in range(i, len(src)):
    if src[j] == '{': depth += 1
    elif src[j] == '}':
        depth -= 1
        if depth == 0: break
metodo = src[m.start():j + 1].replace('protected override', 'public', 1)
open(sys.argv[1], 'w').write('''using System;
using System.Runtime.InteropServices;

static class NvLib { public static Func<int> Quadro = () => 0; }

class Host
{
    public static int SwapRet = 1;
    public bool fim, rodando = true, primeiroQuadro, teclasReservadas;
    public int Swaps, InvalidSwaps;
    public bool FirstFrameLogged;
    void ReservaTeclasMidia() { teclasReservadas = true; }
    void Etapa(string s) { if (s == "ok first-frame") FirstFrameLogged = true; }
    int eglSwapBuffers(IntPtr display, IntPtr surface)
    {
        Swaps++;
        if (display != new IntPtr(0xD1) || surface != new IntPtr(0x51)) InvalidSwaps++;
        return SwapRet;
    }

''' + metodo + '''
}
''')
PY
dotnet build "$T/Tpk40Swap.csproj" -c Release -o "$T/bin" -nologo -v q >"$T/build.log" 2>&1 || { cat "$T/build.log"; echo "FALHOU: build"; exit 1; }
"$T/bin/Tpk40Swap"
