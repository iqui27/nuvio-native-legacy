#!/usr/bin/env python3
"""Run NvPlayer's actual TV/Mobile layout on JVM doubles, without an emulator.

The methods are extracted so tests exercise production arithmetic/guards rather
than a second implementation. Doubles count allocation/layout/recreation and
simulate a layout assignment failure. Android runtime behavior still needs TV
validation; compileDebugKotlin checks the real SDK types separately.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
methods = []
for signature in ('larguraLogica(): Float', 'alturaLogica(): Float', 'aplicarJanela()'):
    start = source.index(f'    private fun {signature} {{')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    methods.append(source[start:end])
fixture = r'''
object Gravity { const val TOP = 48; const val START = 8388611 }
object Counts { var allocations = 0; var assignments = 0; var recreations = 0; var lastDelay = 0L }
object FrameLayout {
    class LayoutParams(var width: Int, var height: Int) {
        init { Counts.allocations++ }
        var gravity = 0; var leftMargin = 0; var topMargin = 0
        var rightMargin = 0; var bottomMargin = 0
    }
}
class Metrics(var widthPixels: Int = 1920, var heightPixels: Int = 1080)
class Resources { val displayMetrics = Metrics() }
class Layer(var width: Int, var height: Int) { val resources = Resources() }
class Surface {
    var fail = false
    var layoutParams: Any? = null
        set(value) {
            if (fail) { fail = false; error("simulated layout failure") }
            field = value; Counts.assignments++
        }
}
object PlayerFixture {
    const val TELA_W = 1920; const val TELA_H = 1080
    const val RECRIA_ESPERA_MS = 350L
    const val RECRIA_ESPERA_HDR_MS = 600L
    var camada: Layer? = Layer(1920, 1080)
    var superficie: Surface? = Surface()
    var jx = 0; var jy = 0; var jw = 1920; var jh = 1080
    var ultHdr = "none"
    var estavelEmitido = false
    var interfaceMobile = false
    fun recriarSuperficie(delay: Long) { check(delay == 350L || delay == 600L); Counts.lastDelay = delay; Counts.recreations++ }
    fun apply() = aplicarJanela()
METHOD
}
fun main() {
    val p = PlayerFixture
    p.apply()
    check(Counts.allocations == 1 && Counts.assignments == 1 && Counts.recreations == 1)
    repeat(1000) { p.apply() }
    check(Counts.allocations == 1 && Counts.assignments == 1 && Counts.recreations == 1)
    p.camada!!.width = 3840; p.camada!!.height = 2160; p.apply()
    var lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    check(lp.width == 3840 && lp.height == 2160)
    check(Counts.allocations == 2 && Counts.assignments == 2 && Counts.recreations == 2)
    p.apply(); check(Counts.assignments == 2)
    p.jx = 10; p.jy = 20; p.jw = 101; p.jh = 61; p.apply()
    lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    check(lp.leftMargin == 20 && lp.topMargin == 40 && lp.width == 202 && lp.height == 122)
    p.superficie = Surface(); p.apply()
    check(Counts.assignments == 4 && Counts.recreations == 4)
    // Existing params can be changed externally: no global memo may hide it.
    lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    lp.gravity = 0; lp.rightMargin = 5; p.apply()
    check(Counts.assignments == 5 && Counts.recreations == 5)
    p.jw = 200; p.superficie!!.fail = true
    try { p.apply(); error("expected failure") } catch (e: IllegalStateException) {
        check(e.message == "simulated layout failure")
    }
    check(Counts.assignments == 5 && Counts.recreations == 5)
    p.apply(); check(Counts.assignments == 6 && Counts.recreations == 6)
    p.camada!!.width = 0; p.camada!!.height = 0; p.apply()
    lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    check(lp.leftMargin == 10 && lp.width == 200)
    p.superficie = null; p.apply()
    check(Counts.assignments == 7 && Counts.recreations == 7)
    // HDR/DV (TCL 2160p DV, 11/10): mudanca de TAMANHO antes da superficie
    // estavel nao pode recriar a superficie. Cada recriacao no HDR obriga o
    // Media3 a soltar a saida do decoder e religa o DispLink; o aspecto salvo
    // aplicado apos o primeiro quadro e cada troca manual recriavam ~12 vezes
    // em 30 s, com o fio principal parado no glClear (pior quadro 6612.7 ms).
    // Antes de estavel a recriacao fica so nas transicoes de modo
    // (hdrNaSuperficie); DEPOIS dela, uma por mudanca (600 ms).
    p.superficie = Surface(); p.ultHdr = "DolbyVision"
    val recriacoesHdr = Counts.recreations
    p.jx = -189; p.jy = -80; p.jw = 2300; p.jh = 1242; p.apply()
    lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    check(lp.width == 2300 && lp.height == 1242 && lp.leftMargin == -189 && lp.topMargin == -80)
    check(Counts.assignments == 8 && Counts.recreations == recriacoesHdr)
    p.jx = 0; p.jy = 21; p.jw = 1920; p.jh = 1037; p.apply()
    check(Counts.assignments == 9 && Counts.recreations == recriacoesHdr)
    p.ultHdr = "HDR10"; p.jw = 2083; p.jh = 1125; p.apply()
    check(Counts.assignments == 10 && Counts.recreations == recriacoesHdr)
    // Superficie estavel (14/10): troca de aspecto no HDR recria UMA vez, com a
    // espera longa. Sem isso o plano da TCL fica preso na geometria de
    // abertura e a troca Crop -> Ajuste nao aparece no filme rodando.
    p.estavelEmitido = true
    p.jw = 2084; p.apply()
    check(Counts.assignments == 11 && Counts.recreations == recriacoesHdr + 1)
    check(Counts.lastDelay == 600L)
    p.ultHdr = "none"   // SDR continua recriando: a TCL prende a geometria do plano
    p.jw = 2300; p.apply()
    check(Counts.assignments == 12 && Counts.recreations == recriacoesHdr + 2)
    p.ultHdr = ""        // HDR ainda desconhecido: recria como SDR, como sempre
    p.jw = 1920; p.apply()
    check(Counts.assignments == 13 && Counts.recreations == recriacoesHdr + 3)
    // Switch modes on the same layer, then rotate/resize. Fullscreen logical
    // requests must cover every physical pixel in TV and Mobile alike.
    for (case in listOf(
        intArrayOf(2560, 1080, 0, 1920, 1080),
        intArrayOf(2560, 1080, 1, 2560, 1080),
        intArrayOf(1080, 2400, 1, 1080, 2400),
        intArrayOf(1080, 2400, 0, 1920, 1080),
        intArrayOf(720, 1600, 1, 1080, 2400),
        intArrayOf(1600, 720, 1, 2400, 1080)
    )) {
        p.camada!!.width = case[0]; p.camada!!.height = case[1]
        p.interfaceMobile = case[2] == 1
        p.jx = 0; p.jy = 0; p.jw = case[3]; p.jh = case[4]; p.apply()
        lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
        check(lp.leftMargin == 0 && lp.topMargin == 0)
        check(lp.width == case[0] && lp.height == case[1])
        val assignments = Counts.assignments
        val recreations = Counts.recreations
        p.apply()
        check(Counts.assignments == assignments && Counts.recreations == recreations)
    }
    // Fractional scaling rounds the edges, not width/height independently.
    p.camada!!.width = 720; p.camada!!.height = 1600
    p.jx = 101; p.jy = 61; p.jw = 200; p.jh = 100; p.apply()
    lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    check(lp.leftMargin == 67 && lp.topMargin == 41 && lp.width == 134 && lp.height == 66)
    // Before layout, Mobile also falls back to the TV canvas and metrics.
    p.camada!!.width = 0; p.camada!!.height = 0
    p.jx = 0; p.jy = 0; p.jw = 1920; p.jh = 1080; p.apply()
    lp = p.superficie!!.layoutParams as FrameLayout.LayoutParams
    check(lp.leftMargin == 0 && lp.topMargin == 0 && lp.width == 1920 && lp.height == 1080)
    val assignments = Counts.assignments
    p.camada = null; p.apply()
    check(Counts.assignments == assignments)
    println("android_window_layout: TV/Mobile geometry, mode switch, rotation, rounding, fallback, repeated requests, retry and HDR/DV ok")
}
'''.replace('METHOD', '\n\n'.join(methods))
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
def jar(group, artifact, version):
    matches = list((cache / group / artifact / version).glob('*/*.jar'))
    if not matches:
        raise SystemExit(f'Missing cached {artifact}:{version}; run Android Gradle build first')
    return str(matches[0])
stdlib = jar('org.jetbrains.kotlin', 'kotlin-stdlib', '2.0.21')
classpath = ':'.join([
    jar('org.jetbrains.kotlin', 'kotlin-compiler-embeddable', '2.0.21'), stdlib,
    jar('org.jetbrains.kotlin', 'kotlin-script-runtime', '2.0.21'),
    jar('org.jetbrains.intellij.deps', 'trove4j', '1.0.20200330'),
    jar('org.jetbrains.kotlinx', 'kotlinx-coroutines-core-jvm', '1.6.4'),
    jar('org.jetbrains', 'annotations', '13.0'),
])
java_home = os.environ.get('JAVA_HOME')
if not java_home:
    homes = sorted((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home'))
    java_home = str(homes[0]) if homes else None
java = str(Path(java_home) / 'bin/java') if java_home else 'java'
with tempfile.TemporaryDirectory(prefix='nuvio-window-layout-') as work:
    work = Path(work)
    file = work / 'WindowFixture.kt'; file.write_text(fixture)
    target = work / 'classes'
    subprocess.run([java, '-cp', classpath, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-classpath', stdlib,
                    '-jvm-target', '17', '-d', str(target), str(file)], check=True)
    subprocess.run([java, '-cp', f'{target}:{stdlib}', 'WindowFixtureKt'], check=True)
