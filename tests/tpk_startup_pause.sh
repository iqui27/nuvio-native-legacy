#!/bin/bash
# Host regression for #684: execute the production startup-pause predicate.
set -eu
cd "$(dirname "$0")/.."
dotnet_bin="${NUVIO_DOTNET:-$HOME/.dotnet/dotnet}"
[ -x "$dotnet_bin" ] || { echo "unverified: .NET SDK unavailable"; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-startup-pause.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import re
import sys

source = Path('tizen-tpk/Program.cs').read_text()
match = re.search(r'bool RecuperaPausaInicial\(\) => (.*?);', source, re.S)
assert match, 'startup-pause predicate missing'
expression = match[1]
expression = expression.replace('relogio.ElapsedMilliseconds', 'elapsed')
work = Path(sys.argv[1])
(work / 'Test.csproj').write_text('''<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup>
<OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework>
</PropertyGroup></Project>''')
(work / 'Test.cs').write_text('''using System;
class Test {
    static bool jaPausou, primeiraTecla, saindo, fim, erroNaTela;
    static int trocados;
    static long elapsed;
    static bool Recover() => ''' + expression + ''';
    static void Check(bool expected, string name) {
        if (Recover() != expected) throw new Exception(name);
        Console.WriteLine("PASS: " + name);
    }
    static void Main() {
        Lifecycle.Run();
        elapsed = 3700; trocados = 2; Check(true, "T7NSN5 first session");
        elapsed = 4200; Check(true, "T7NSN5 second session");
        trocados = 0; Check(true, "pause before first frame");
        trocados = 3; Check(false, "startup frames already delivered");
        trocados = 2; elapsed = 7999; Check(true, "inside retry window");
        elapsed = 8000; Check(false, "retry deadline");
        elapsed = 4200;
        jaPausou = true; Check(false, "second pause stays paused"); jaPausou = false;
        primeiraTecla = true; Check(false, "user interaction stays paused"); primeiraTecla = false;
        saindo = true; Check(false, "exit cannot recover"); saindo = false;
        fim = true; Check(false, "native app ended"); fim = false;
        erroNaTela = true; Check(false, "diagnostics stay visible");
    }
}
''')
# Check lifecycle wiring too: recovery runs after base pause, and relaunch
# cannot raise already disposed windows, even from the delayed callback.
pause = source.split('protected override void OnPause()', 1)[1].split('protected override void OnLowMemory', 1)[0]
assert pause.index('RecuperaPausaInicial()') < pause.index('jaPausou = true')
assert pause.index('base.OnPause();') < pause.index('if (recuperar)')
assert 'NuiWindow.Instance.Show(); NuiWindow.Instance.Raise();' in pause
assert 'SobeGl("startup pause");' in pause and 'OnResume();' in pause
control = source.split('protected override void OnAppControlReceived', 1)[1].split('// ================= SAIDA', 1)[0]
assert control.index('if (saindo || soltou)') < control.index('if (JanelaUnica)')
exit_branch = control.split('if (saindo || soltou)', 1)[1].split('if (JanelaUnica)', 1)[0]
assert 'ArmaSaidaForcada(' in exit_branch and 'Exit();' in exit_branch and 'return;' in exit_branch
callback = control.split('t.Tick +=', 1)[1]
assert callback.index('if (saindo || soltou) return false;') < callback.index('SobeGl("appcontrol")')
assert 'pausado = false;' in callback
print('PASS: lifecycle recovery and exit guards wired')

# Execute the production OnPause against a deterministic NUI event queue.
# A timer tick may occur between the main-window and GL-window hide events.
method = 'public override void OnPause()' + pause[:pause.rfind('}') + 1]
method = re.sub(r'^#.*$', '', method, flags=re.M)
(work / 'Lifecycle.cs').write_text('''using System;
using System.Collections.Generic;
class BaseApp { public virtual void OnPause() {} }
class Window {
    public bool Visible = true;
    public int Raises;
    public void Show() { Visible = true; }
    public void Raise() { Raises++; }
}
class NuiWindow { public static Window Instance = new Window(); }
class NuiTimer {
    public static List<NuiTimer> Timers = new List<NuiTimer>();
    public event Func<object, object, bool> Tick;
    public NuiTimer(uint interval) {}
    public void Start() { Timers.Add(this); }
    public static void Pump() {
        foreach (var timer in Timers.ToArray())
            if (!timer.Tick(null, null)) Timers.Remove(timer);
    }
}
class Video { public void PausarPeloSistema() {} }
class Voice { public void Cancelar() {} }
class Clock { public long ElapsedMilliseconds = 4190; }
class Lifecycle : BaseApp {
    bool jaPausou, primeiraTecla, saindo, soltou, fim, erroNaTela, pausado;
    bool JanelaUnica;
    int trocados = 2;
    Clock relogio = new Clock();
    Video video = new Video();
    Voice voz = new Voice();
    Window gl = new Window();
    bool principalVisivel => NuiWindow.Instance.Visible;
    bool glVisivel => gl.Visible;
    bool RecuperaPausaInicial() => ''' + match[1] + ''';
    void Janela(string message) {}
    void Etapa(string message) {}
    string Contagem() => "";
    void nv_tpk_ciclo(int state) {}
    void SobeGl(string reason) { gl.Show(); gl.Raise(); }
    void OnResume() { pausado = false; }
''' + method + '''
    static void Require(bool condition, string name) {
        if (!condition) throw new Exception(name);
    }
    public static void Run() {
        foreach (bool single in new[] { false, true }) {
            NuiWindow.Instance = new Window(); NuiTimer.Timers.Clear();
            var app = new Lifecycle { JanelaUnica = single };
            app.OnPause();
            NuiTimer.Pump();
            Require(NuiWindow.Instance.Raises == 0, "recovery must wait for delayed hide");
            app.relogio.ElapsedMilliseconds = 4300;
            NuiWindow.Instance.Visible = false;
            NuiTimer.Pump();
            if (!single) Require(!NuiWindow.Instance.Visible, "wait for GL hide too");
            app.relogio.ElapsedMilliseconds = 4310;
            app.gl.Visible = false;
            NuiTimer.Pump();
            Require(NuiWindow.Instance.Visible && (single || app.gl.Visible) && !app.pausado,
                    "delayed hides must finish before recovery");
            int raises = NuiWindow.Instance.Raises;
            NuiTimer.Pump();
            Require(NuiWindow.Instance.Raises == raises && NuiTimer.Timers.Count == 0,
                    "startup recovery runs once");
        }
        foreach (string cancel in new[] { "exit", "disposed", "ended", "error", "key", "deadline", "resume" }) {
            NuiWindow.Instance = new Window(); NuiTimer.Timers.Clear();
            var app = new Lifecycle(); app.OnPause();
            app.saindo = cancel == "exit"; app.soltou = cancel == "disposed";
            app.fim = cancel == "ended"; app.erroNaTela = cancel == "error";
            app.primeiraTecla = cancel == "key";
            if (cancel == "deadline") app.relogio.ElapsedMilliseconds = 8000;
            if (cancel == "resume") app.OnResume();
            NuiWindow.Instance.Visible = false; app.gl.Visible = false;
            NuiTimer.Pump();
            Require(NuiWindow.Instance.Raises == 0 && NuiTimer.Timers.Count == 0,
                    "cancel queued recovery: " + cancel);
        }
        Console.WriteLine("PASS: delayed window hides, one retry, single window, cancellation guards");
    }
}
''')
PY
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$dotnet_bin" build "$work/Test.csproj" -c Release --nologo -v q --source "$work" >"$work/build.log" 2>&1 || { cat "$work/build.log"; exit 1; }
"$dotnet_bin" "$work/bin/Release/net8.0/Test.dll"
