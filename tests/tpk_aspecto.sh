#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aspecto.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
cat > "$work/native.c" <<'C'
static int v[7],errors[3];
int probe(int i) { return v[i]; }
void fail(int op,int rc) { errors[op]=rc; }
int player_set_display_mode(void *p,int mode) { if(p)v[5]++;v[0]=mode;return errors[1]; }
int player_set_display_roi_area(void *p,int x,int y,int w,int h) { v[6]++;v[1]=x;v[2]=y;v[3]=w;v[4]=h;return errors[2]; }
C
cc -shared -fPIC "$work/native.c" -o "$work/libcapi-media-player.so.0"
python3 - "$work" "$PWD" <<'PY'
from pathlib import Path
import sys
work,root=map(Path,sys.argv[1:])
s=(root/'tests/tpk_lifecycle/Test.cs').read_text()
if 'IntPtr Handle' not in s: s=s.replace('public class Player {','public class Player { public IntPtr Handle => new IntPtr(1);')
s=s.replace('public class Settings { public PlayerDisplayMode Mode {get;set;}', 'public class Settings { PlayerDisplayMode mode; public PlayerDisplayMode Mode {get=>mode;set {mode=value; Native.mode((int)value);}}')
s=s.replace('public void SetRoi(Rectangle r) {}', 'public bool FailRoi; public void SetRoi(Rectangle r) { if(FailRoi) throw new Exception("legacy ROI rejected"); }')
s+='\nstatic class Native { [System.Runtime.InteropServices.DllImport("libcapi-media-player.so.0", EntryPoint="player_set_display_mode")] internal static extern int set(System.IntPtr p,int m); internal static void mode(int m) { set(System.IntPtr.Zero,m); } }\n'
(work/'Stubs.cs').write_text(s)
paths=['tests/tpk_aspecto.cs','tizen-tpk/Video.cs','tizen-tpk/VideoWindowMetrics.cs']
(work/'Aspecto.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><StartupObject>AspectoTest</StartupObject><EnableDefaultCompileItems>false</EnableDefaultCompileItems></PropertyGroup><ItemGroup><Compile Include="Stubs.cs"/>'+''.join('<Compile Include="'+str(root/p)+'"/>' for p in paths)+'</ItemGroup></Project>')
PY
"${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" build "$work/Aspecto.csproj" -c Release --nologo -v q >"$work/build.log" 2>&1 || { cat "$work/build.log";exit 1; }
"${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" "$work/bin/Release/net8.0/Aspecto.dll" "$work/libcapi-media-player.so.0"
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
for flag in '' '-DNV_TPK40'; do
  cc -O1 -g -Wall -DNV_TPK $flag -Isrc tests/tpk-aspecto-roi.c src/video_tpk.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c src/capmkv.c src/mkv.c tests/capmkv_stub.c tests/dts_tpk_stub.c -lpthread $SDL -o "$work/bridge"
  "$work/bridge"
done
