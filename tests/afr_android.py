"""Run the real #641 display-mode picker on JVM doubles; no Android device."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
start = source.index('    private fun afrModoPara(')
end = source.index('\n    private fun afrAplicar(', start)
body = source[start:end]
bridge = (root / 'src/video_android.c').read_text()
assert 'Java_space_nuvio_nativelegacy_NvPlayer_nativeAfr' in bridge
assert 'return ajustes_afr();' in bridge
assert 'afrAplicar()' in source and 'afrRestaurar()' in source
assert 'preferredDisplayModeId' in source
# Guards added by the review: trailers keep the home cadence, exit/pause
# restores the panel mode even with the player retained.
assert 'afrTitulo.isEmpty()' in source
assert 'afrSair()' in source and 'NvPlayer.afrSair()' in (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt').read_text()
assert 'afrRestaurar()' in source[source.index('fun pausarPeloSistema'):source.index('fun encerrar')]
doubles = r'''
package android.view
class Display(val mode: Mode, val supportedModes: List<Mode>) {
    class Mode(val modeId: Int, val physicalWidth: Int, val physicalHeight: Int, val refreshRate: Float)
}
'''
fixture = r'''
import android.view.Display
class Test {
BODY
    companion object {
        @JvmStatic fun main(args: Array<String>) {
            val t = Test()
            fun d(cur: Display.Mode, vararg m: Display.Mode) = Display(cur, m.toList())
            // Mi Box / TCL-style panel: only 2160p60 -> integer multiple wins.
            val p60 = Display.Mode(1, 3840, 2160, 60f)
            check(t.afrModoPara(30f, d(p60, p60)) == p60)
            // Panel with 24/50/60: film picks the matching 23.976 mode.
            val m24 = Display.Mode(2, 3840, 2160, 23.976f)
            val m50 = Display.Mode(3, 3840, 2160, 50f)
            val panel = d(p60, p60, m24, m50)
            check(t.afrModoPara(23.976f, panel) == m24)
            check(t.afrModoPara(25f, panel) == m50)
            // Unsupported cadence stays put: 25 fps on a 24/60-only panel has
            // no integer multiple -> no switch (review scenario, no blackout).
            check(t.afrModoPara(25f, d(p60, p60, m24)) == null)
            check(t.afrModoPara(30f, d(m50, m50, m24)) == null)
            // 120 Hz panel: no integer multiple for 24fps except 120 (5x) and
            // 24 (1x) — both qualify, the closest rate wins the tie-break.
            val m120 = Display.Mode(4, 3840, 2160, 120f)
            val panel120 = d(p60, p60, m120, m24)
            check(t.afrModoPara(24f, panel120) == m24)
            // 25fps PAL on a 50 Hz mode: 2x integer multiple.
            check(t.afrModoPara(25f, d(p60, p60, m50)) == m50)
            // Resolution differs -> skip (mode must keep the panel resolution).
            val m24hd = Display.Mode(5, 1920, 1080, 30f)
            check(t.afrModoPara(30f, d(p60, p60, m24hd)) == p60)
            // Unknown fps -> no switch.
            check(t.afrModoPara(0f, panel) == null)
            println("afr: integer-multiple pick, resolution filter, unknown fps noop, unsupported cadence noop")
        }
    }
}
'''.replace('BODY', body)
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
compiler = list(cache.glob('org.jetbrains.kotlin/*/2.0.21/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.intellij.deps/trove4j/*/*/*.jar'))
compiler += list(cache.glob('org.jetbrains/annotations/*/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar'))
stdlib = next(cache.glob('org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar'))
java = str(next((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home/bin/java')))
with tempfile.TemporaryDirectory(prefix='nuvio-afr-jvm-', dir=os.environ.get('TMPDIR')) as tmp:
    tmp = Path(tmp)
    (tmp / 'Doubles.kt').write_text(doubles)
    (tmp / 'Replay.kt').write_text(fixture)
    subprocess.run([java, '-cp', os.pathsep.join(map(str, compiler)),
                    'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-reflect', '-no-stdlib',
                    '-nowarn', '-classpath', str(stdlib), '-d', str(tmp / 'classes'),
                    str(tmp / 'Doubles.kt'), str(tmp / 'Replay.kt')], check=True)
    subprocess.run([java, '-cp', os.pathsep.join([str(tmp / 'classes'), str(stdlib)]), 'Test'], check=True)
