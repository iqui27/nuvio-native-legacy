"""Subtitle sync by audio, Android side (F06).

1. Compile-check the REAL NvPlayer.kt + AudioSyncSink.kt (Media3 ForwardingAudioSink)
   + AudioSyncTap.kt (and the files NvPlayer needs) against android-35 + the Media3
   1.8.0 jars in the Gradle cache. Same toolchain as tests/streamfit_passiva_android.py.
   No Gradle/APK build, no device.
2. Replay AudioSyncTap.kt (real file, pure Kotlin) on the JVM with synthetic buffers:
   48 kHz stereo PCM16 / 44.1 kHz 5.1 float / 24-bit -> mono 16 kHz, media-time of
   each delivery, the same (buffer, pts) re-offered by the sink is copied once, the
   player's buffer position/limit untouched, one fixed output array (no allocation
   per buffer), tap off = nothing, bitstream (passthrough) = FMT_BITSTREAM and nothing.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
root = Path(__file__).resolve().parent.parent
kt = root / "android/app/src/main/java/space/nuvio/nativelegacy"

# Reuse the F03 helpers (same cache, same compiler, same jars).
src = (Path(__file__).resolve().parent / "streamfit_passiva_android.py").read_text()
prelude = src[:src.index("with tempfile.TemporaryDirectory")]
exec(compile(prelude, "streamfit_passiva_android.py", "exec"))

with tempfile.TemporaryDirectory(prefix="nuvio-audiosync-", dir=os.environ.get("TMPDIR")) as tmp:
    tmp = Path(tmp)
    deps = [sdk, stdlib]
    for name in ("media3-common", "media3-datasource", "media3-exoplayer", "media3-decoder",
                 "media3-extractor", "media3-container", "media3-database"):
        deps.append(aar_jar("androidx.media3", name, "1.8.0", tmp))
    deps.append(aar_jar("androidx.core", "core", "1.13.1", tmp))
    deps.append(jar("androidx.annotation", "annotation-jvm", "1.6.0"))
    deps.append(aar_jar("androidx.annotation", "annotation-experimental", "1.4.0", tmp))
    deps.append(jar("com.google.guava", "guava", "33.3.1-android"))
    # A Activity so devolve o foco neste teste de player; SDL nao e necessario.
    activity = tmp / "NuvioActivity.kt"
    activity.write_text("package space.nuvio.nativelegacy\n"
                        "class NuvioActivity : android.app.Activity() { fun devolverFoco() {} }\n")
    sources = [activity, kt / "RecriaHdr.kt", kt / "NvPlayer.kt", kt / "ParaleloDataSource.kt", kt / "PassivoMedidor.kt",
               kt / "AudioSyncTap.kt", kt / "AudioSyncSink.kt",
               # F07: NvPlayer layers the seek cache and the gain processor
               kt / "CacheSessao.kt", kt / "CacheMidia.kt", kt / "GanhoMath.kt", kt / "GanhoAudioProcessor.kt"]
    kotlinc(sources, tmp / "app", os.pathsep.join(map(str, deps)))
    print("compile-check: NvPlayer/AudioSyncSink/AudioSyncTap against android-35 + Media3 1.8.0: OK")

    replay = tmp / "Replay.kt"
    replay.write_text("""
import space.nuvio.nativelegacy.AudioSyncTap
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.sin

class Saida { val pts = mutableListOf<Long>(); val n = mutableListOf<Int>(); val arrays = HashSet<Int>(); var amostras = ArrayList<Short>() }
fun novo(s: Saida, estados: MutableList<Int>) = AudioSyncTap({ a, n, p ->
    s.pts.add(p); s.n.add(n); s.arrays.add(System.identityHashCode(a)); for (i in 0 until n) s.amostras.add(a[i])
}, { estados.add(it) })

fun pcm16(hz: Int, canais: Int, quadros: Int, f: Double, t0: Int): ByteBuffer {
    val b = ByteBuffer.allocateDirect(quadros * canais * 2).order(ByteOrder.nativeOrder())
    for (q in 0 until quadros) { val v = (0.5 * sin(2 * PI * f * (t0 + q) / hz) * 32767).toInt().toShort()
        for (c in 0 until canais) b.putShort(v) }
    b.flip(); return b
}

fun main() {
    // --- 48 kHz stereo PCM16: 1 s in 20 ms buffers -> 16000 samples, pts per buffer
    run {
        val s = Saida(); val e = mutableListOf<Int>(); val t = novo(s, e)
        t.configurar(true, AudioSyncTap.ENC_PCM16, 2, 48000)
        check(e.last() == AudioSyncTap.FMT_PCM)
        t.buffer(pcm16(48000, 2, 960, 440.0, 0), 0)          // off: nothing
        check(s.n.isEmpty())
        t.ligado = true
        for (k in 0 until 50) {
            val b = pcm16(48000, 2, 960, 440.0, k * 960)
            val pos = b.position(); val lim = b.limit()
            t.buffer(b, 10_000_000L + k * 20_000L)
            t.buffer(b, 10_000_000L + k * 20_000L)            // re-offered: copied once
            check(b.position() == pos && b.limit() == lim) { "player buffer touched" }
        }
        val total = s.n.sum()
        check(total in 15990..16010) { "samples $total" }
        check(s.pts.first() == 10_000_000L && abs(s.pts[1] - 10_020_000L) <= 63) { s.pts.take(3).toString() }
        check(s.arrays.size == 1) { "one fixed output array" }
        val pico = s.amostras.maxOf { abs(it.toInt()) }
        check(pico in 15000..17000) { "downmix amplitude $pico" }
        // flush/seek: next buffer restarts the phase and the pts follows the new buffer
        t.descontinuidade(); s.pts.clear()
        t.buffer(pcm16(48000, 2, 960, 440.0, 0), 99_000_000L)
        check(s.pts.single() == 99_000_000L)
    }
    // --- 44.1 kHz 5.1 float and 24-bit mono
    run {
        val s = Saida(); val e = mutableListOf<Int>(); val t = novo(s, e); t.ligado = true
        t.configurar(true, AudioSyncTap.ENC_FLOAT, 6, 44100)
        val b = ByteBuffer.allocateDirect(4410 * 6 * 4).order(ByteOrder.LITTLE_ENDIAN)
        for (q in 0 until 4410) for (c in 0 until 6) b.putFloat(if (c == 0) 0.6f else 0f)
        b.flip(); t.buffer(b, 0)
        check(s.n.sum() in 1598..1602) { "44.1k->16k ${s.n.sum()}" }
        check(abs(s.amostras[100] - (32767 * 0.1).toInt()) < 40) { "5.1 downmix ${s.amostras[100]}" }
        val s2 = Saida(); val t2 = novo(s2, e); t2.ligado = true
        t2.configurar(true, AudioSyncTap.ENC_PCM24, 1, 16000)
        val c = ByteBuffer.allocate(3 * 3); c.put(byteArrayOf(0, 0, 0x40, 0, 0, 0xC0.toByte(), 0, 0, 0)); c.flip()
        t2.buffer(c, 5)
        check(s2.amostras.toList() == listOf<Short>(16383, -16383, 0)) { s2.amostras.toString() }
    }
    // --- bitstream (AC3/E-AC3/DTS passthrough, offload): reported, nothing copied
    run {
        val s = Saida(); val e = mutableListOf<Int>(); val t = novo(s, e); t.ligado = true
        t.configurar(false, 0, 6, 48000)
        check(e.last() == AudioSyncTap.FMT_BITSTREAM)
        t.buffer(ByteBuffer.allocate(1536), 0)
        check(s.n.isEmpty())
        t.encerrar(); check(e.last() == AudioSyncTap.FMT_NENHUM)
    }
    println("audiosync_android: PASS (48k/44.1k/5.1/float/24-bit -> mono 16k, pts per delivery, re-offer once, buffer untouched, fixed array, off/bitstream silent)")
}
""")
    kotlinc([kt / "AudioSyncTap.kt", replay], tmp / "replay", str(stdlib))
    subprocess.run([java, "-cp", os.pathsep.join((str(tmp / "replay"), str(stdlib))), "ReplayKt"], check=True)
