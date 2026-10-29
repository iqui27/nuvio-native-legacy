"""Run the real player construction function on JVM doubles; no Android device."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
start = source.index('    private fun construirPlayer(')
end = source.index('\n    private fun abrirMain(', start)
body = source[start:end]
bridge = (root / 'src/video_android.c').read_text()
assert 'Java_space_nuvio_nativelegacy_NvPlayer_nativeTunneling' in bridge
assert 'return ajustes_tunneling();' in bridge
assert 'tunneling=${audioTrackConfig.tunneling}' in source
fixture = r'''
import java.util.concurrent.atomic.AtomicInteger
class Activity
object C { const val TRACK_TYPE_TEXT = 3; const val USAGE_MEDIA = 1; const val AUDIO_CONTENT_TYPE_MOVIE = 1 }
object Log { fun w(tag: String, message: String) {} }
object androidx { object media3 { object common { object util { object Util {
    fun generateAudioSessionIdV21(act: Activity) = 1
} } } } }
class AudioAttributes { class Builder {
    fun setUsage(v: Int) = this
    fun setContentType(v: Int) = this
    fun build() = AudioAttributes()
} }
open class DefaultTrackSelector(act: Activity) {
    var parameters = Parameters()
    class Parameters(val tunnelingEnabled: Boolean = false) {
        fun buildUpon() = Builder(tunnelingEnabled)
        class Builder(var enabled: Boolean) {
            fun setTunnelingEnabled(on: Boolean) = apply { enabled = on }
            fun setTrackTypeDisabled(type: Int, disabled: Boolean) = this
            fun build() = Parameters(enabled)
        }
    }
}
class SeletorAudioPreparo(act: Activity, val language: String) : DefaultTrackSelector(act)
class ExoPlayer(val selector: DefaultTrackSelector) {
    var audioSessionId = 0
    var trackSelectionParameters = DefaultTrackSelector.Parameters()
    fun setAudioAttributes(a: AudioAttributes, focus: Boolean) {}
    class Builder(val act: Activity, renderers: Any) {
        var selector = DefaultTrackSelector(act)
        fun setLoadControl(v: Any) = this
        fun setMediaSourceFactory(v: Any) = this
        fun setReleaseTimeoutMs(v: Int) = this
        fun setDetachSurfaceTimeoutMs(v: Int) = this
        fun setTrackSelector(v: DefaultTrackSelector) = apply { selector = v }
        fun setPlaybackLooper(v: Any) = this
        fun setBandwidthMeter(v: Any) = this
        fun build() = ExoPlayer(selector)
    }
}
class Thread { val looper = Any() }
class RecursosPlayer(val idiomaAudio: String) {
    val renderizadores = Any(); val carga = Any(); val fabricaMidia = Any()
    val fioPreparo: Thread? = null; val bandaPreparo: Any? = null
    lateinit var p: ExoPlayer
}
object Test {
    const val TAG = "test"
    val liberacoesEmCurso = AtomicInteger()
    var enabled = false
    fun nativeTunneling() = enabled
BODY
    @JvmStatic fun main(args: Array<String>) {
        for (language in listOf("", "en")) for (on in listOf(false, true, false)) {
            enabled = on
            val r = RecursosPlayer(language)
            construirPlayer(Activity(), r)
            check(r.p.selector.parameters.tunnelingEnabled == on)
            if (language.isNotEmpty()) check((r.p.selector as SeletorAudioPreparo).language == language)
        }
        println("tunneling: default off, on/off selector mapping, prepared audio preserved")
    }
}
'''.replace('BODY', body)
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
compiler = list(cache.glob('org.jetbrains.kotlin/*/2.0.21/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.intellij.deps/trove4j/*/*/*.jar'))
compiler += list(cache.glob('org.jetbrains/annotations/13.0/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar'))
stdlib = next(cache.glob('org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar'))
java = str(next((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home/bin/java')))
with tempfile.TemporaryDirectory(prefix='nuvio-tunneling-jvm-', dir=os.environ.get('TMPDIR')) as tmp:
    tmp = Path(tmp)
    replay = tmp / 'Replay.kt'
    replay.write_text(fixture)
    subprocess.run([java, '-cp', os.pathsep.join(map(str, compiler)),
                    'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-reflect', '-no-stdlib',
                    '-nowarn', '-classpath', str(stdlib), '-d', str(tmp / 'classes'), str(replay)], check=True)
    subprocess.run([java, '-cp', os.pathsep.join([str(tmp / 'classes'), str(stdlib)]), 'Test'], check=True)
