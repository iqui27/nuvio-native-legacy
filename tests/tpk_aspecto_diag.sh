#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aspecto.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
cat > "$work/native.c" <<'C'
static double v[9];
static int errors[4];
double probe(int i) { return v[i]; }
void fail(int op, int rc) { errors[op]=rc; }
int player_set_display_mode(void *p, int mode) { v[0]=mode; return errors[1]; }
int player_set_display_roi_area(void *p,int x,int y,int w,int h) { v[1]=x;v[2]=y;v[3]=w;v[4]=h;return errors[2]; }
int player_set_video_roi_area(void *p,double x,double y,double w,double h) { v[5]=x;v[6]=y;v[7]=w;v[8]=h;return errors[3]; }
C
cc -shared -fPIC "$work/native.c" -o "$work/libcapi-media-player.so.0"
# Reuse lifecycle stubs; no second fake player implementation.
python3 - "$work" "$PWD" <<'PY'
from pathlib import Path
import sys
work,root=map(Path,sys.argv[1:])
s=(root/'tests/tpk_lifecycle/Test.cs').read_text()
s=s.replace('LetterBox, Roi, FullScreen','LetterBox, Roi, FullScreen, CroppedFull')
s=s.replace('public class Player {','public class Player { public IntPtr Handle => new IntPtr(1);')
s=s.replace('public int Width=1920, Height=1080;', 'public static int VideoW=1920, VideoH=1080; public int Width=>VideoW; public int Height=>VideoH;')
(work/'Stubs.cs').write_text(s)
paths=['tests/tpk_aspecto_diag.cs','tizen-tpk/Video.cs','tizen-tpk/AspectoDiag.cs','tizen-tpk/VideoWindowMetrics.cs']
(work/'Diag.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><DefineConstants>NV_ASPECTO_DIAG</DefineConstants><StartupObject>AspectoDiagTest</StartupObject><EnableDefaultCompileItems>false</EnableDefaultCompileItems></PropertyGroup><ItemGroup><Compile Include="Stubs.cs"/>'+''.join('<Compile Include="'+str(root/p)+'"/>' for p in paths)+'</ItemGroup></Project>')
PY
DOTNET_CLI_TELEMETRY_OPTOUT=1 "${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" build "$work/Diag.csproj" -c Release --nologo -v q > "$work/build.log" 2>&1 || { cat "$work/build.log"; exit 1; }
"${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}" "$work/bin/Release/net8.0/Diag.dll" "$work/libcapi-media-player.so.0"
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
cc -O1 -g -Wall -DNV_TPK -DNV_ASPECTO_DIAG -Isrc tests/tpk-roi.c src/video_tpk.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread $SDL -o "$work/bridge"
"$work/bridge"
cat > "$work/keys.c" <<'C'
#include <assert.h>
#include "tpkteclas.h"
int main(void) {
  SDL_Event e;
  assert(tpkteclas_evento("9", 1, &e) && e.key.keysym.sym == SDLK_9);
#ifdef NV_ASPECTO_DIAG
  assert(tpkteclas_evento("x", 1, &e) && e.key.keysym.sym == SDLK_x);
  assert(tpkteclas_evento("X", 1, &e) && e.key.keysym.sym == SDLK_x);
#else
  assert(!tpkteclas_evento("x", 1, &e));
#endif
  return 0;
}
C
for flag in '' '-DNV_ASPECTO_DIAG'; do
  cc $flag -Isrc "$work/keys.c" src/tpkteclas.c $SDL -o "$work/keys"
  "$work/keys"
done
