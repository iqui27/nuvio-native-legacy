#!/usr/bin/env python3
"""Execute production Android mute/gain code across delayed player attachment."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()

def block(marker):
    start = source.index(marker)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

volume = block('@JvmStatic fun volume(')
gain = block('private fun aplicarGanho()')
# Include retained volume state when present; old implementation still compiles.
state = '\n'.join(line for line in source.splitlines() if 'private var volumePct =' in line)
fixture = '''
class FakePlayer { var volume = 1f }
class Processor { var ganho = 1f }
object GanhoMath {
    fun volumePlayer(pct: Int) = pct.coerceAtMost(100) / 100f
    fun reforco(pct: Int) = pct.coerceAtLeast(100) / 100f
}
object P {
    val principal = object { fun post(action: () -> Unit) { action() } }
    var player: FakePlayer? = null
    var processador: Processor? = Processor()
    var ganhoPct = 100
    fun relatarPreviewAudio() {}
    STATE
    VOLUME
    GAIN
    fun attach() { player = FakePlayer(); aplicarGanho() }
    fun gain(pct: Int) { ganhoPct = pct; aplicarGanho() }
}
fun main() {
    P.attach(); check(P.player!!.volume == 1f)
    P.volume(0); check(P.player!!.volume == 0f)
    P.gain(150); check(P.player!!.volume == 0f) { "gain update erased preview mute" }
    P.player = null; P.volume(0); P.attach()
    check(P.player!!.volume == 0f) { "delayed open lost mute" }
    P.player = null; P.volume(100); P.attach()
    check(P.player!!.volume == 1f) { "delayed open lost unmute" }
    P.volume(-1); check(P.player!!.volume == 0f)
    P.volume(200); check(P.player!!.volume == 1f)
    P.gain(50); check(P.player!!.volume == .5f)
    println("PASS: Android preview mute/unmute survives delayed open and gain updates")
}
'''.replace('STATE', state).replace('VOLUME', volume).replace('GAIN', gain)
# Named handler preserves Kotlin visibility of post().
fixture = fixture.replace('object P {', 'class Handler { fun post(action: () -> Unit) { action() } }\nobject P {')
fixture = fixture.replace('val principal = object { fun post(action: () -> Unit) { action() } }', 'val principal = Handler()')
setup = (root / 'tests/android_window_layout.py').read_text()
setup = setup[setup.index('cache = Path.home()'):setup.index('with tempfile.TemporaryDirectory(')]
namespace = dict(Path=Path, os=os)
exec(setup, namespace)
java, classpath, stdlib = (namespace[k] for k in ('java', 'classpath', 'stdlib'))
with tempfile.TemporaryDirectory(prefix='nuvio-guide-audio-') as work:
    work = Path(work)
    file = work / 'Audio.kt'
    file.write_text(fixture)
    target = work / 'classes'
    subprocess.run([java, '-cp', classpath, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', stdlib,
                    '-jvm-target', '17', '-d', str(target), str(file)], check=True)
    subprocess.run([java, '-cp', f'{target}:{stdlib}', 'AudioKt'], check=True)
