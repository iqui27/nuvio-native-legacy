#!/usr/bin/env python3
"""Run the production open/prepare branches on JVM with a tiny Media3 double."""
from pathlib import Path
import os
import subprocess
import tempfile

source = Path('android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
start = source.index('            if (preparada == null) {\n                val mediaItem')
end = source.index('            principal.postDelayed(tique(minha)', start)
branches = source[start:end]
initial_pause = source[source.index('            if (!reabrindo) pausaPedida ='):].splitlines()[0].strip()
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
compiler = list(cache.glob('org.jetbrains.kotlin/*/2.0.21/*/*.jar'))
for group in ('org.jetbrains.intellij.deps/trove4j', 'org.jetbrains/annotations',
              'org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm'):
    compiler += list(cache.glob(f'{group}/*/*/*.jar'))
stdlib = next(cache.glob('org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar'))
java = os.environ.get('NUVIO_TEST_JAVA') or str(next((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home/bin/java')))
fixture = r'''
object Log { fun w(t: String, s: String) {} ; fun i(t: String, s: String) {} }
class P {
    var playWhenReady = false
        set(v) { field = v; if (v) starts++ }
    var starts = 0
    val currentTracks = 0; val videoSize = 0; val playbackState = 3
    fun setMediaItem(m: String, pos: Long = 0) {}
    fun setMediaSource(m: String, pos: Long) {}
    fun prepare() { check(!playWhenReady) { "lobby prepare auto-plays" } }
}
class Preparo(val proximo: Boolean) {
    val fracaoPendente = 0; val posicaoFonteMs = 0L
    val inicioAceito = true; val token = 1
}
class Ouv { fun onTracksChanged(t: Int) {}; fun onVideoSizeChanged(t: Int) {}; fun onPlaybackStateChanged(t: Int) {} }
class R { val fontePrecarregada: String? = "warm" }
object Fixture {
    const val TAG = "test"; const val EV_PAUSADO = 3
    var fracaoAtual = 0; var fracaoPendente = 0; val geracao = 1
    var pausaPedida = true
    fun itemMidia(url: String) = url
    fun confirmarRetomada(g: Int, b: Boolean) {}
    fun aplicarFracao(p: P) {}
    fun ev(e: Int) {}
    fun open(preparada: Preparo?, inicioMs: Int) {
        val p = P(); val url = "fixture"; val r = R(); val ouv = Ouv()
        val reabrindo = false; val pausado = true
        pausaPedida = false
        INITIAL_PAUSE
        BRANCHES
        check(!p.playWhenReady && p.starts == 0) { "prepared adoption started film before lobby Go" }
        // Only the explicit room clock command releases playback.
        pausaPedida = false; p.playWhenReady = !pausaPedida
        check(p.playWhenReady && p.starts == 1)
    }
}
fun main() {
    for (warm in listOf(null, Preparo(false), Preparo(true))) {
        Fixture.pausaPedida = true; Fixture.open(warm, 0)
        Fixture.pausaPedida = true; Fixture.open(warm, 12000)
    }
    println("PASS Android cold/warm/next prepare stays paused; only room Go starts playback")
}
'''.replace('BRANCHES', branches).replace('INITIAL_PAUSE', initial_pause)
with tempfile.TemporaryDirectory(prefix='juntos-pause-') as tmp:
    tmp = Path(tmp); kt = tmp / 'Pause.kt'; kt.write_text(fixture)
    subprocess.run([java, '-cp', os.pathsep.join(map(str, compiler)),
                    'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-reflect', '-no-stdlib', '-nowarn',
                    '-classpath', str(stdlib), '-d', str(tmp / 'classes'), str(kt)], check=True)
    subprocess.run([java, '-cp', f'{tmp / "classes"}{os.pathsep}{stdlib}', 'PauseKt'], check=True)
# JNI must carry pause before any open can run, including prepared adoption.
native = Path('src/video_android.c').read_text()
assert 'mAbrirPausada' in native and 'juntos_precarregando()' in native
assert 'pausado = pausado' in source
print('PASS paused open crosses JNI with the source request')
