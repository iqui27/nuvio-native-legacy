#!/usr/bin/env python3
"""Replay the real speed bridge and sink policy offline against cached Media3."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kt = root / "android/app/src/main/java/space/nuvio/nativelegacy"
prelude = (root / "tests/streamfit_passiva_android.py").read_text().split("with tempfile.TemporaryDirectory", 1)[0]
exec(compile(prelude, "streamfit_passiva_android.py", "exec"))
source = (kt / "NvPlayer.kt").read_text()
start = source.index("    @JvmStatic fun velocidade(")
end = source.index("    private val janelaPedido", start)
method = source[start:end]
fixture = r'''
import space.nuvio.nativelegacy.*
import android.util.Log
import androidx.media3.common.Format
import androidx.media3.common.MimeTypes
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.exoplayer.audio.AudioOffloadSupport
import java.lang.reflect.Proxy

class Queue {
    val tasks = mutableListOf<() -> Unit>()
    fun post(task: () -> Unit) { tasks.add(task) }
    fun drain() { val pending = tasks.toList(); tasks.clear(); pending.forEach { it() } }
}
class Parameters(var speed: Float = 1f)
class FakePlayer {
    var currentPosition = 42000L
    var playWhenReady = false
    var playbackParameters = Parameters()
    val calls = mutableListOf<String>()
    fun stop() { calls.add("stop"); currentPosition = 0 }
    fun seekTo(pos: Long) { calls.add("seek"); currentPosition = pos }
    fun setPlaybackSpeed(speed: Float) { calls.add("speed"); playbackParameters.speed = speed }
    fun prepare() { calls.add("prepare") }
}
class Resources { var velocidadePcm = false }
object Bridge {
    const val TAG = "test"
    val principal = Queue()
    val pedidos = java.util.concurrent.atomic.AtomicInteger(1)
    var player: FakePlayer? = FakePlayer()
    var recursosAtuais: Resources? = Resources()
    SPEED_METHOD
}
fun main() {
    val p = Bridge.player!!
    val r = Bridge.recursosAtuais!!
    for (speed in listOf(125, 150, 175, 200, 75, 100)) {
        val changed = r.velocidadePcm != (speed != 100)
        p.calls.clear()
        Bridge.velocidade(speed)
        Bridge.principal.drain()
        check(p.playbackParameters.speed == speed / 100f)
        check(r.velocidadePcm == (speed != 100))
        check(p.currentPosition == 42000L && !p.playWhenReady)
        check(p.calls == if (changed) listOf("stop", "seek", "speed", "prepare") else listOf("speed"))
    }
    p.playWhenReady = true
    Bridge.velocidade(150); Bridge.principal.drain()
    check(p.playWhenReady)
    Bridge.velocidade(100)
    Bridge.pedidos.incrementAndGet(); Bridge.principal.drain()
    check(r.velocidadePcm && p.playbackParameters.speed == 1.5f) // stale request ignored
    Bridge.velocidade(100); Bridge.principal.drain()
    check(!r.velocidadePcm && p.playbackParameters.speed == 1f)

    var forcePcm = false
    val delegate = Proxy.newProxyInstance(AudioSink::class.java.classLoader,
        arrayOf(AudioSink::class.java)) { _, method, _ ->
        when (method.name) {
            "getFormatSupport" -> AudioSink.SINK_FORMAT_SUPPORTED_DIRECTLY
            "supportsFormat" -> true
            "getFormatOffloadSupport" -> AudioOffloadSupport.Builder().setIsFormatSupported(true).build()
            else -> error("Unexpected call: ${method.name}")
        }
    } as AudioSink
    val sink = AudioSyncSink(delegate, AudioSyncTap({ _, _, _ -> }, { }), { forcePcm })
    val encoded = listOf(MimeTypes.AUDIO_AC3, MimeTypes.AUDIO_E_AC3, MimeTypes.AUDIO_DTS)
        .map { Format.Builder().setSampleMimeType(it).build() }
    val pcm = Format.Builder().setSampleMimeType(MimeTypes.AUDIO_RAW).build()
    for (speed in listOf(100, 125, 150, 175, 200, 100)) {
        forcePcm = speed != 100
        for (format in encoded) {
            check(sink.supportsFormat(format) == !forcePcm)
            check(sink.getFormatSupport(format) == if (forcePcm) AudioSink.SINK_FORMAT_UNSUPPORTED else AudioSink.SINK_FORMAT_SUPPORTED_DIRECTLY)
            check(sink.getFormatOffloadSupport(format).isFormatSupported == !forcePcm)
        }
        check(sink.supportsFormat(pcm))
    }
    println("velocidade_android: PASS (real bridge/sink: speed, PCM fallback/restoration, pause/position, stale requests)")
}
'''.replace("SPEED_METHOD", method)
with tempfile.TemporaryDirectory(prefix="nuvio-speed-android-") as work:
    work = Path(work)
    deps = [sdk, stdlib, aar_jar("androidx.annotation", "annotation-experimental", "1.4.0", work)]
    for name in ("media3-common", "media3-exoplayer"):
        deps.append(aar_jar("androidx.media3", name, "1.8.0", work))
    deps.append(jar("com.google.guava", "guava", "33.3.1-android"))
    sources = [kt / "AudioSyncSink.kt", kt / "AudioSyncTap.kt"]
    for name, content in {
        "Replay.kt": fixture,
        "Log.kt": "package android.util\nobject Log { @JvmStatic fun i(tag: String, message: String) = 0 }",
        "TextUtils.kt": "package android.text\nobject TextUtils { @JvmStatic fun isEmpty(s: CharSequence?) = s == null || s.isEmpty(); @JvmStatic fun equals(a: CharSequence?, b: CharSequence?) = a?.toString() == b?.toString() }",
    }.items():
        path = work / name
        path.write_text(content)
        sources.append(path)
    kotlinc(sources, work / "classes", os.pathsep.join(map(str, deps)))
    subprocess.run([java, "-cp", os.pathsep.join(map(str, [work / "classes", *deps])), "ReplayKt"], check=True)
