#!/bin/bash
# No restore/packaging: real Voz + extracted Program lifecycle, then release/canary compiles.
set -euo pipefail
cd "$(dirname "$0")/.."
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
work=$(mktemp -d "${NUVIO_HOST_TEST_DIR:-${TMPDIR:-/private/var/tmp}}/nuvio-voz-host.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" "$PWD" "$dotnet_bin" <<'PY'
from pathlib import Path
import json, os, subprocess, sys
w, root = map(Path, sys.argv[1:3])
home = Path.home()
sdk = sorted((home/'.dotnet/sdk').glob('*/Roslyn/bincore/csc.dll'))[-1]
framework = sorted((home/'.dotnet/packs/Microsoft.NETCore.App.Ref').glob('*/ref/net8.0'))[-1]
(w/'csc-path').write_text(str(sdk))
(w/'framework.rsp').write_text('\n'.join('-r:"'+str(p)+'"' for p in sorted(framework.glob('*.dll'))))
native = (root/'src/tpk.c').read_text()
a = native.index('  if (!appEsperando) {'); b = native.index('\n  eglMakeCurrent', a)
(w/'startup-clear.c').write_text('''#include <assert.h>
#include <pthread.h>
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int appEsperando, esperaMs, clears;
#define GL_COLOR_BUFFER_BIT 1
static void glClearColor(float r, float g, float b, float a) {}
static void glClear(int mask) { clears++; }
static int startup(void) {
pthread_mutex_lock(&trava);
'''+native[a:b]+'''
return -99;
}
int main(void) {
esperaMs = -1;
#ifdef NV_TPK40
assert(startup() == 1 && "TVGLApplication must keep receiving startup callbacks");
#else
assert(startup() == 0 && "API8 black clear is not an app frame");
#endif
assert(clears == 1);
esperaMs = 50;
assert(startup() == 0);
return 0;
}
''')
s = (root/'tizen-tpk/Program.cs').read_text()
def method(marker):
    a = s.index(marker); b = s.index('{', a); depth = 1; end = b + 1
    while depth:
        if s[end] == '{': depth += 1
        elif s[end] == '}': depth -= 1
        end += 1
    return s[a:end]
(w/'Lifecycle.cs').write_text('''using System;
using System.Threading;
using Tizen.NUI;
using NuiWindow = Tizen.NUI.Window;
namespace NuvioTpk {
class TestVideo { public void PausarPeloSistema() {} public void Encerrar() { NuiWindow.Instance.Hide(); } }
class Program : NUIApplication {
Voz voz; TestVideo video = new TestVideo(); GLView glView; GLWindow gl;
bool pausado, jaPausou, principalVisivel = true, glVisivel = true, soltou;
bool erroNaTela, saindo, fim; int chamadas, trocados, subidas;
long ultimaChamadaMs, paradoDesdeMs = -1, inicioDesenhoMs;
object telaParado; TestClock relogio = new TestClock();
public string Error; public int FrameResult;
class TestClock { public long ElapsedMilliseconds; }
long ParadoMs() => relogio.ElapsedMilliseconds - ultimaChamadaMs;
void SobeGl(string s) {} void TiraTelaParado() {} void MostraTelaParado(long ms) {}
void Erro(string title, string detail) { Error = title; erroNaTela = true; }
int nv_tpk_quadro() { Fault.Hit("native-frame"); return FrameResult; }
public void DrawAt(long ms) { relogio.ElapsedMilliseconds = ms; Quadro(); }
public void WatchAt(long ms) { relogio.ElapsedMilliseconds = ms; gl = new GLWindow(); Vigia(); }
const bool JanelaUnica = true;
public Program(Voz v) { voz = v; }
void Janela(string s) {} void Etapa(string s) {} string Contagem() => "";
void nv_tpk_ciclo(int memoria) {}
public void Pause() { OnPause(); } public void Resume() { OnResume(); } public void Close() { SoltaTudo("test"); }
'''+ '\n'.join(method(x) for x in ['protected override void OnPause()', 'protected override void OnResume()', 'void SoltaTudo(string porque)', 'bool DesenhoPronto()', 'void Vigia()', 'int Quadro()'])+'\n}}')
(w/'host.rsp').write_text('\n'.join('"'+str(p)+'"' for p in [root/'tests/tpk_voz_host.cs', root/'tizen-tpk/Voz.cs', w/'Lifecycle.cs']))
(w/'host.runtimeconfig.json').write_text('{"runtimeOptions":{"tfm":"net8.0","framework":{"name":"Microsoft.NETCore.App","version":"8.0.0"}}}')
for api in [8, 9, 11]:
    base = home/'.nuget/packages'/f'tizen.net.api{api}' if api != 11 else home/'.dotnet/packs/Samsung.Tizen.Ref.API11'
    refs = sorted(base.glob('*/ref/*'))[-1]
    (w/f'api{api}.rsp').write_text('\n'.join('-r:"'+str(p)+'"' for p in sorted(refs.glob('*.dll')) if not (framework/p.name).exists()))
(w/'production.rsp').write_text('\n'.join('"'+str(root/'tizen-tpk'/p)+'"' for p in ['Program.cs','Apps.cs','Video.cs','VideoWindowMetrics.cs','Carga.cs','Voz.cs']))
canary = json.loads(subprocess.check_output([sys.argv[3], 'msbuild', str(root/'tizen-tpk/NuvioTpk/NuvioTpk.csproj'), '-nologo', '-p:NvTextoCanario=1', '-getProperty:DefineConstants', '-getItem:Compile']))
(w/'canary.rsp').write_text('-define:'+canary['Properties']['DefineConstants'].replace(';', ',')+'\n'+'\n'.join('"'+p['FullPath']+'"' for p in canary['Items']['Compile']))
PY
cat > "$work/native.c" <<'C'
#include <string.h>
static void (*start)(void), (*stop)(void);
static int count, result;
static char value[256];
void nv_tpk_voz_registrar(void (*a)(void), void (*b)(void), int mic) { start=a;stop=b; }
void nv_tpk_texto_valor(const char *s) { strncpy(value,s,255);value[255]=0; }
void nv_tpk_texto_fim(int r) { count++;result=r; }
void host_start(void) { start(); }
void host_stop(void) { stop(); }
void host_reset(void) { count=0;result=-1;value[0]=0; }
int host_count(void) { return count; }
int host_result(void) { return result; }
const char *host_value(void) { return value; }
C
cc -shared -fPIC "$work/native.c" -o "$work/libnuvio.so"
cc "$work/startup-clear.c" -pthread -o "$work/startup-clear"
"$work/startup-clear"
cc -DNV_TPK40 "$work/startup-clear.c" -pthread -o "$work/startup-clear40"
"$work/startup-clear40"
echo "PASS: API8 black clear is not an app frame; API4 keeps startup callbacks"
csc=$(cat "$work/csc-path")
"$dotnet_bin" "$csc" -nologo -langversion:8 -target:exe -define:NV_API11 -nowarn:0649 -out:"$work/host.dll" @"$work/framework.rsp" @"$work/host.rsp"
"$dotnet_bin" "$work/host.dll" "$work/libnuvio.so"
for api in 8 9 11; do
  "$dotnet_bin" "$csc" -nologo -langversion:8 -target:library -define:NV_API$api -out:"$work/host-api$api.dll" @"$work/framework.rsp" @"$work/api$api.rsp" @"$work/production.rsp"
  echo "PASS: full release host compile against cached Tizen API$api"
done
"$dotnet_bin" "$csc" -nologo -langversion:8 -target:library -out:"$work/host-canary-api11.dll" @"$work/framework.rsp" @"$work/api11.rsp" @"$work/canary.rsp"
echo "PASS: full NvTextoCanario=1 host compile against cached Tizen API11"
