#!/usr/bin/env python3
"""Replay real audio diagnostic bodies with Media3 1.8.0 and a controlled clock/log.
NvPlayer's complete Android compilation is covered by audiosync_android.py.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kt = root / "android/app/src/main/java/space/nuvio/nativelegacy"
source = (kt / "NvPlayer.kt").read_text()
prelude = (root / "tests/streamfit_passiva_android.py").read_text().split("with tempfile.TemporaryDirectory", 1)[0]
exec(compile(prelude, "streamfit_passiva_android.py", "exec"))


def block(start):
    start = source.index(start)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r'''
import androidx.media3.common.C
import androidx.media3.common.Format
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.datasource.DataSourceException
import androidx.media3.datasource.HttpDataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.HttpDataSource.InvalidResponseCodeException
import androidx.media3.exoplayer.ExoPlaybackException
import androidx.media3.exoplayer.analytics.AnalyticsListener
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.exoplayer.mediacodec.MediaCodecDecoderException
import android.os.SystemClock
import android.util.Log

class FakePlayer {
    var playbackState = Player.STATE_READY
    var isPlaying = false
}
object Fixture {
    const val TAG = "NvPlayer"
    const val EV_PAUSADO = 3
    var sessao = 1
    var pedidoAtivo = 1
    val pedidos = java.util.concurrent.atomic.AtomicInteger(1)
    var player: FakePlayer? = FakePlayer()
    val native = mutableListOf<String>()
    fun nativeAudioLog(s: String) { native.add(s) }
    val errors = mutableListOf<String>()
    fun nativeErroTexto(s: String) { errors.add(s) }
    fun fatal(error: PlaybackException) {
        val renderer = "audio"
        val abriuEm = 0L
        FATAL_DIAGNOSTICS
    }
    fun ev(t: Int) {}
    fun medirEstado(p: FakePlayer) {}
    CURRENT
    REPORT
    FIELDS
    CAUSE_FIELDS
    fun listener(minha: Int) = object : Player.Listener {
        VOLUME
        SUPPRESSION
        PLAY_WHEN_READY
    }
    ANALYTICS
        SINK
        CODEC
    }
}
fun main() {
    val f = Fixture
    val token = "SECRET_DASH_TOKEN_345"
    val format = Format.Builder().setId("https://cdn.example/$token")
        .setLabel("private-$token").setSampleMimeType("audio/raw")
        .setSampleRate(48000).setChannelCount(2).setPcmEncoding(C.ENCODING_PCM_16BIT).build()
    // Real Media3 exception: the raw message demonstrably carries Format.id.
    val init = AudioSink.InitializationException(0, 48000, 12, 4096, 0, format, true,
        IllegalStateException("codec-$token"))
    check(init.message!!.contains(token)) { "fixture must reproduce the original leak" }
    val write = AudioSink.WriteException(-6, format, false)
    val config = AudioSink.ConfigurationException("arbitrary-$token", format)
    val codec = MediaCodecDecoderException(IllegalStateException("codec-$token"), null)
    val event = AnalyticsListener.EventTime(0, androidx.media3.common.Timeline.EMPTY, 0, null,
        0, androidx.media3.common.Timeline.EMPTY, 0, null, 0, 0)
    val a = f.analitico(f.sessao)
    a.onAudioSinkError(event, init)
    a.onAudioSinkError(event, write)
    a.onAudioSinkError(event, config)
    a.onAudioCodecError(event, codec)
    a.onAudioCodecError(event, IllegalArgumentException("raw-$token"))
    check(f.native[0] == "sessao=1 sink erro=InitializationException hz=48000 canais=2 encoding=2 mime=audio/raw isRecoverable=true repeticoes=0")
    check(f.native[1].contains("errorCode=-6") && f.native[1].contains("isRecoverable=false"))
    check(f.native[3].contains("errorCode="))
    check(f.native.none { token in it } && Log.lines.none { token in it }) { "token reached a log" }
    // A fatal player error wrapping the sink must not reintroduce its message.
    f.fatal(PlaybackException("fatal-$token", init, 5001))
    check(f.errors.single().contains("InitializationException <- IllegalStateException"))
    check(f.errors.none { token in it } && Log.errors.none { token in it })
    // Real HTTP exceptions carry private URL, request/response headers and body.
    val spec = DataSpec.Builder().setUri("https://cdn.example/$token")
        .setHttpRequestHeaders(mapOf("Authorization" to token)).build()
    val statuses = listOf(403, 404, 503)
    for (status in statuses) {
        val http = InvalidResponseCodeException(status, "response-$token",
            java.io.IOException("nested-$token"), mapOf("Set-Cookie" to listOf(token)),
            spec, "body-$token".toByteArray())
        f.fatal(ExoPlaybackException.createForSource(http, PlaybackException.ERROR_CODE_IO_BAD_HTTP_STATUS))
        check(f.errors.last().contains("responseCode=$status")) { "HTTP status missing: ${f.errors.last()}" }
        check(Log.errors.last().contains("responseCode=$status")) { "logcat HTTP status missing" }
    }
    check(f.errors.takeLast(3).toSet().size == 3 && Log.errors.takeLast(3).toSet().size == 3)
    val rendererError = ExoPlaybackException.createForRenderer(write, "renderer-$token", 7,
        format, C.FORMAT_HANDLED, false, PlaybackException.ERROR_CODE_AUDIO_TRACK_WRITE_FAILED)
    f.fatal(PlaybackException("wrapper-$token", rendererError, 1000))
    check(f.errors.last().contains("ExoPlaybackException errorCode=5002 type=1 rendererIndex=7 <- WriteException errorCode=-6"))
    f.fatal(PlaybackException("wrapper-$token", codec, 4003))
    check(f.errors.last().contains("MediaCodecDecoderException errorCode=${codec.errorCode}"))
    check(f.errors.zip(Log.errors).all { (native, log) -> log.endsWith(native) })
    check((f.errors + Log.errors).none { token in it || "https://" in it || "Authorization" in it || "Set-Cookie" in it })
    val codecCount = f.native.size
    repeat(10000) { a.onAudioCodecError(event, codec) }
    check(f.native.size == codecCount) { "codec burst was not limited" }

    // Per-buffer discontinuity bursts: no JNI/logcat writes inside the interval.
    val discontinuity = AudioSink.UnexpectedDiscontinuityException(500000, 0)
    val before = f.native.size
    a.onAudioSinkError(event, discontinuity)
    check(f.native.last().contains("expectedUs=0 actualUs=500000"))
    repeat(10000) { a.onAudioSinkError(event, discontinuity) }
    check(f.native.size == before + 1)
    SystemClock.now = 29999
    a.onAudioSinkError(event, discontinuity)
    check(f.native.size == before + 1)
    SystemClock.now = 30000
    a.onAudioSinkError(event, discontinuity)
    check(f.native.last().endsWith("repeticoes=10002"))
    a.onAudioCodecError(event, codec)
    check(f.native.last().contains("codec erro=MediaCodecDecoderException") && f.native.last().endsWith("repeticoes=10001"))
    a.onAudioCodecError(event, discontinuity) // same exception, separate callback type
    check(f.native.last().contains("codec erro=UnexpectedDiscontinuityException") && f.native.last().endsWith("repeticoes=0"))
    val count = f.native.size
    repeat(10000) { a.onAudioSinkError(event, discontinuity) }
    check(f.native.size == count)
    SystemClock.now = 60000
    a.onAudioSinkError(event, discontinuity)
    check(f.native.last().endsWith("repeticoes=20003"))

    val oldListener = f.listener(1)
    f.sessao = 2
    val newAnalytics = f.analitico(2)
    a.onAudioSinkError(event, discontinuity)
    oldListener.onVolumeChanged(0f)
    oldListener.onPlaybackSuppressionReasonChanged(1)
    oldListener.onPlayWhenReadyChanged(false, 2)
    check(f.native.size == count + 1) { "stale session logged" }
    newAnalytics.onAudioSinkError(event, discontinuity)
    check(f.native.last().startsWith("sessao=2") && f.native.last().endsWith("repeticoes=0"))
    val listener = f.listener(2)
    val ready = f.player!!.playbackState
    listener.onVolumeChanged(0.2f)
    listener.onPlaybackSuppressionReasonChanged(Player.PLAYBACK_SUPPRESSION_REASON_TRANSIENT_AUDIO_FOCUS_LOSS)
    listener.onPlayWhenReadyChanged(false, Player.PLAY_WHEN_READY_CHANGE_REASON_AUDIO_FOCUS_LOSS)
    listener.onPlayWhenReadyChanged(true, Player.PLAY_WHEN_READY_CHANGE_REASON_USER_REQUEST)
    check(f.native.takeLast(4) == listOf("sessao=2 volume=0.2", "sessao=2 suppression=1",
        "sessao=2 playWhenReady=false reason=2", "sessao=2 playWhenReady=true reason=1"))
    check(f.player!!.playbackState == ready) { "READY must not need to change" }
    check(Log.lines == f.native.map { "[audio] $it" }) { "native/logcat mirrors differ" }
    println("audio_diagnostics_android: PASS (real HTTP 403/404/503 distinct in native/logcat, URL/header/body/message secrets excluded, numeric cause fields, Format.id excluded, rate/count per session/type, stale callbacks, volume/suppression/playWhenReady after READY)")
}
'''
fixture = fixture.replace("CURRENT", next(line for line in source.splitlines() if "private fun atual(" in line).replace("private ", ""))
fixture = fixture.replace("REPORT", block("    private fun relatarAudio(").replace("private ", ""))
fixture = fixture.replace("CAUSE_FIELDS", block("    private fun camposErroCausa(").replace("private ", ""))
fixture = fixture.replace("FIELDS", block("    private fun camposErroAudio(").replace("private ", ""))
for marker, method in (("VOLUME", "onVolumeChanged"), ("SUPPRESSION", "onPlaybackSuppressionReasonChanged"),
                       ("PLAY_WHEN_READY", "onPlayWhenReadyChanged"), ("SINK", "onAudioSinkError"), ("CODEC", "onAudioCodecError")):
    fixture = fixture.replace(marker, block("        override fun " + method + "("), 1)
start = source.index('            // Fatal errors can wrap')
end = source.index('            // Decoder que falha', start)
fixture = fixture.replace("FATAL_DIAGNOSTICS", source[start:end])
start = source.index("    private fun analitico(")
end = source.index("        override fun onAudioDecoderInitialized", start)
fixture = fixture.replace("ANALYTICS", source[start:end].replace("private fun analitico", "fun analitico"))

with tempfile.TemporaryDirectory(prefix="nuvio-audio-diagnostics-") as tmp:
    tmp = Path(tmp)
    deps = [sdk, stdlib]
    for name in ("media3-common", "media3-exoplayer", "media3-decoder", "media3-datasource"):
        deps.append(aar_jar("androidx.media3", name, "1.8.0", tmp))
    deps.append(jar("com.google.guava", "guava", "33.3.1-android"))
    # Only platform methods needed by real Format/exception construction and logs.
    stubs = {
        "Clock.kt": "package android.os\nobject SystemClock { var now = 0L; @JvmStatic fun elapsedRealtime() = now }",
        "Log.kt": "package android.util\nobject Log { val lines = mutableListOf<String>(); val errors = mutableListOf<String>(); @JvmStatic fun i(t: String, s: String): Int { lines.add(s); return 0 }; @JvmStatic fun w(t: String, s: String): Int { errors.add(s); return 0 } }",
        "Uri.kt": "package android.net\nclass Uri private constructor(private val value: String) { companion object { @JvmStatic fun parse(s: String) = Uri(s); @JvmField val EMPTY = Uri(\"\") }; fun normalizeScheme() = this; override fun toString() = value }",
        "TextUtils.kt": "package android.text\nobject TextUtils { @JvmStatic fun isEmpty(s: CharSequence?) = s == null || s.isEmpty(); @JvmStatic fun equals(a: CharSequence?, b: CharSequence?) = a?.toString() == b?.toString() }",
    }
    sources = []
    for name, text in {**stubs, "Replay.kt": fixture}.items():
        p = tmp / name
        p.write_text(text)
        sources.append(p)
    kotlinc(sources, tmp / "replay", os.pathsep.join(map(str, deps)))
    subprocess.run([java, "-cp", os.pathsep.join(map(str, [tmp / "replay", *deps])), "ReplayKt"], check=True)
