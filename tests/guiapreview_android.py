#!/usr/bin/env python3
"""Run production Android window handling through a delayed guide preview open."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
start = source.index('    private fun definirJanela(')
end = source.index('    private fun larguraLogica()', start)
methods = source[start:end]
start = source.index('    private fun reaplicarJanela()')
end = source.index('\n    //', start)
methods += source[start:end]
# Execute the exact window setup used after attaching the decoder surface.
start = source.index('            }\n', source.index('sv.holder.addCallback(depoisDoMedia3)', source.index('    private fun abrirMain('))) + len('            }\n')
end = source.index('\n\n            if (preparada == null)', start)
open_window = source[start:end]
fixture = r'''
object Log { fun i(tag: String, message: String) {} }
class Surface { val holder = Any() }
object P {
    const val TAG = "fixture"; val sessao = 1; val ultHdr = "none"; val semRecriar = true
    var superficie: Surface? = null
    var player: Any? = null
    var pedX = 0; var pedY = 0; var pedW = 1920; var pedH = 1080; var pedEncaixa = true
    var temJanela = false
    var jx = 0; var jy = 0; var jw = 1920; var jh = 1080
    var videoW = 0; var videoH = 0
    var deliveries = 0
    fun aplicarJanela() { deliveries++ }
    fun aplicarEncaixe() { jx = 0; jy = 0; jw = 1920; jh = 1080; aplicarJanela() }
    fun request(x: Int, y: Int, w: Int, h: Int) = definirJanela(x, y, w, h, true)
    fun attach() { player = Any(); superficie = Surface(); OPEN_WINDOW }
    fun dimensions(w: Int, h: Int) { videoW = w; videoH = h; if (temJanela) calcularJanela() else aplicarEncaixe() }
METHODS
}
fun main() {
    // TCL waits for the previous decoder release: no player when JNI window arrives.
    P.request(1294, 186, 560, 315)
    check(P.deliveries == 0) { "window applied before the decoder existed" }
    P.attach()
    check(listOf(P.jx, P.jy, P.jw, P.jh) == listOf(1294, 186, 560, 315)) {
        "delayed Android open lost guide preview window: ${P.jx},${P.jy} ${P.jw}x${P.jh}"
    }
    P.dimensions(1920, 1080)
    check(P.jx == 1294 && P.jw == 560 && P.jh == 315)
    // Latest focus geometry wins during the wait; 4:3 content remains contained.
    P.player = null; P.temJanela = false; P.videoW = 0; P.videoH = 0
    P.request(1294, 186, 560, 315); P.request(1514, 184, 330, 176)
    P.attach(); P.dimensions(1440, 1080)
    check(P.jw == 235 && P.jh == 176 && P.jx == 1561 && P.jy == 184)
    // Normal full-screen session starts after release cleared the guide request.
    P.player = null; P.temJanela = false; P.attach()
    check(P.jx == 0 && P.jy == 0 && P.jw == 1920 && P.jh == 1080)
    // Requests after an immediate open still work.
    P.videoW = 1920; P.videoH = 1080; P.request(1294, 186, 560, 315)
    check(P.jx == 1294 && P.jw == 560 && P.jh == 315)
    println("PASS: Android delayed preview open, latest window, aspect containment and full-screen reset")
}
'''.replace('METHODS', methods).replace('OPEN_WINDOW', open_window)
setup = (root / 'tests/android_window_layout.py').read_text()
setup = setup[setup.index('cache = Path.home()'):setup.index('with tempfile.TemporaryDirectory(')]
namespace = dict(Path=Path, os=os)
exec(setup, namespace)
java, classpath, stdlib = (namespace[k] for k in ('java', 'classpath', 'stdlib'))
with tempfile.TemporaryDirectory(prefix='nuvio-guide-preview-') as work:
    work = Path(work)
    file = work / 'Preview.kt'
    file.write_text(fixture)
    target = work / 'classes'
    subprocess.run([java, '-cp', classpath, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', stdlib,
                    '-jvm-target', '17', '-d', str(target), str(file)], check=True)
    subprocess.run([java, '-cp', f'{target}:{stdlib}', 'PreviewKt'], check=True)
