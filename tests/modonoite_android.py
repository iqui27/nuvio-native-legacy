"""Night mode / dialogue clarity, Android side (NoiteMath.kt + NoiteAudioProcessor.kt).

Runs the REAL Kotlin on the JVM against Media3 1.8.0 from the Gradle cache and
compares it, sample by sample, with the C reference (src/modonoite.c): the
vectors come from tests/modonoite.sh (NUVIO_NOITE_VETORES). Then the processor
itself: off = bit-exact copy, 16-bit and float, 5.1 kept as 5.1, bitstream
refused (passthrough is never touched), level changed live.
No device, no Gradle/APK build. The compile-check of the wiring in NvPlayer.kt
is tests/cacheboost_android.py.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parent.parent
kt = root / "android/app/src/main/java/space/nuvio/nativelegacy"
cache = Path.home() / ".gradle/caches/modules-2/files-2.1"
compiler = list(cache.glob("org.jetbrains.kotlin/*/2.0.21/*/*.jar"))
compiler += list(cache.glob("org.jetbrains.intellij.deps/trove4j/*/*/*.jar"))
compiler += list(cache.glob("org.jetbrains/annotations/13.0/*/*.jar"))
compiler += list(cache.glob("org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar"))
stdlib = next(cache.glob("org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar"))
sdk = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Library/Android/sdk"))) / "platforms/android-35/android.jar"
assert sdk.is_file() and any("compiler-embeddable" in str(x) for x in compiler)
cp = os.pathsep.join(map(str, compiler))
java = os.environ.get("NUVIO_TEST_JAVA") or str(next((Path.home() / ".local/jdks").glob("jdk-17*/Contents/Home/bin/java")))


def aar_jar(group, name, version, tmp):
    aar = next(cache.glob(f"{group}/{name}/{version}/*/{name}-{version}.aar"))
    dst = tmp / f"{name}-{version}.jar"
    with zipfile.ZipFile(aar) as z:
        dst.write_bytes(z.read("classes.jar"))
    return dst


def jar(group, name, version):
    return next(cache.glob(f"{group}/{name}/{version}/*/{name}-{version}.jar"))


with tempfile.TemporaryDirectory(prefix="nuvio-modonoite-android-", dir=os.environ.get("TMPDIR")) as tmp:
    tmp = Path(tmp)
    vetores = tmp / "vetores.txt"
    subprocess.run(["bash", str(root / "tests/modonoite.sh")], check=True, stdout=subprocess.DEVNULL,
                   env={**os.environ, "NUVIO_NOITE_VETORES": str(vetores)})
    common = aar_jar("androidx.media3", "media3-common", "1.8.0", tmp)
    guava = jar("com.google.guava", "guava", "33.3.1-android")
    deps = [sdk, stdlib, common, jar("androidx.annotation", "annotation-jvm", "1.6.0"),
            aar_jar("androidx.annotation", "annotation-experimental", "1.4.0", tmp), guava]
    replay = tmp / "Replay.kt"
    replay.write_text(r"""
import androidx.media3.common.C
import androidx.media3.common.audio.AudioProcessor
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import space.nuvio.nativelegacy.NoiteAudioProcessor
import space.nuvio.nativelegacy.NoiteMath

fun main(args: Array<String>) {
    // --- the same numbers as modonoite_param / modonoite_ganho_db / modonoite_canal
    check(NoiteMath.ganhoDb(0, -10.0) == 0.0 && NoiteMath.ganhoDb(1, -60.0) == 6.0 && NoiteMath.ganhoDb(2, -60.0) == 12.0)
    check(Math.abs(NoiteMath.ganhoDb(1, 0.0) + 6) < 1e-9 && Math.abs(NoiteMath.ganhoDb(2, 0.0) + 12) < 1e-9)
    check(NoiteMath.nivel(-1) == 0 && NoiteMath.nivel(3) == 0 && NoiteMath.nivel(2) == 2)
    val c51 = intArrayOf(NoiteMath.FRENTE, NoiteMath.FRENTE, NoiteMath.CENTRO, NoiteMath.LFE, NoiteMath.SURROUND, NoiteMath.SURROUND)
    for (i in 0 until 6) check(NoiteMath.canal(6, i) == c51[i])
    for (i in 0 until 4) check(NoiteMath.canal(4, i) == NoiteMath.FRENTE && NoiteMath.canal(2, i) == NoiteMath.FRENTE)
    check(NoiteMath.canal(5, 3) == NoiteMath.SURROUND && NoiteMath.canal(8, 3) == NoiteMath.LFE && NoiteMath.canal(9, 2) == NoiteMath.FRENTE)

    // --- sample by sample against the C reference
    val esperado = HashMap<String, Float>()
    File(args[0]).forEachLine { l -> val p = l.split(" "); esperado[p[0] + " " + p[1] + " " + p[2] + " " + p[3]] = p[5].toFloat() }
    check(esperado.size > 2000)
    var pior = 0f; var conferidos = 0
    for (nivel in 1..2) for (canais in 1..8) {
        val d = NoiteMath.Dsp(nivel, canais, 48000); check(d.ativo)
        var s = 12345 + nivel * 100 + canais
        val x = FloatArray(canais)
        for (q in 0 until 2000) {
            val amp = if (q < 600) 0.02f else if (q < 1200) 0.9f else 0.2f
            for (c in 0 until canais) { s = s * 1664525 + 1013904223; x[c] = amp * ((s ushr 8).toFloat() / 8388608.0f - 1.0f) }
            d.quadro(x)
            for (c in 0 until canais) {
                val e = esperado["$nivel $canais $q $c"] ?: continue
                val dif = Math.abs(e - x[c]); if (dif > pior) pior = dif
                conferidos++
            }
        }
    }
    check(conferidos == esperado.size) { "$conferidos of ${esperado.size}" }
    check(pior < 2e-5f) { "Kotlin drifted from the C reference: $pior" }
    check(!NoiteMath.Dsp(0, 2, 48000).ativo && !NoiteMath.Dsp(2, 9, 48000).ativo && !NoiteMath.Dsp(2, 2, 0).ativo)

    // --- the processor on real buffers
    fun processar(p: NoiteAudioProcessor, enc: Int, canais: Int, enche: (ByteBuffer) -> Unit, n: Int): ByteBuffer {
        val out = p.configure(AudioProcessor.AudioFormat(48000, canais, enc))
        check(out.encoding == enc && out.channelCount == canais && p.isActive)   // 5.1 stays 5.1: no downmix
        p.flush()
        val inp = ByteBuffer.allocateDirect(n).order(ByteOrder.nativeOrder()); enche(inp); inp.flip()
        p.queueInput(inp); check(!inp.hasRemaining())
        return p.output.order(ByteOrder.nativeOrder())
    }
    val p = NoiteAudioProcessor()
    NoiteAudioProcessor.nivel = NoiteMath.DESLIGADO
    val o0 = processar(p, C.ENCODING_PCM_16BIT, 2, { b -> b.putShort(1000); b.putShort(-32768); b.putShort(32767); b.putShort(0) }, 8)
    check(o0.remaining() == 8 && o0.short == 1000.toShort() && o0.short == (-32768).toShort() && o0.short == 32767.toShort() && o0.short == 0.toShort())
    // Live: quiet 5.1 dialogue on the centre, 1 s. both levels lift it.
    fun centro(nivel: Int, enc: Int): Double {
        NoiteAudioProcessor.nivel = nivel
        val quadros = 48000
        val o = processar(p, enc, 6, { b -> for (q in 0 until quadros) for (c in 0 until 6) {
            val a = if (c == 2) 0.02 * Math.sin(2 * Math.PI * 300 * q / 48000.0) else 0.0
            if (enc == C.ENCODING_PCM_16BIT) b.putShort((a * 32768).toInt().toShort()) else b.putFloat(a.toFloat()) } },
            quadros * 6 * (if (enc == C.ENCODING_PCM_16BIT) 2 else 4))
        var s = 0.0
        for (q in 0 until quadros) for (c in 0 until 6) {
            val v = if (enc == C.ENCODING_PCM_16BIT) o.short / 32768.0 else o.float.toDouble()
            if (c == 2) s += v * v else check(v == 0.0)
        }
        check(!o.hasRemaining())
        return 20 * Math.log10(Math.sqrt(s / quadros))
    }
    for (enc in intArrayOf(C.ENCODING_PCM_16BIT, C.ENCODING_PCM_FLOAT)) {
        val base = centro(0, enc); val leve = centro(1, enc); val forte = centro(2, enc)
        // Leve: +6 make-up +3 centre. Forte: +12 +6, minus ~3 dB because the lifted
        // centre (-28 dBFS) is already 4 dB over the -32 dB threshold at 4:1.
        check(Math.abs(leve - base - 9) < 0.3 && Math.abs(forte - base - 15.2) < 0.6) { "$enc: $base $leve $forte" }
    }
    // A blast at full scale never clips.
    NoiteAudioProcessor.nivel = NoiteMath.FORTE
    val ob = processar(p, C.ENCODING_PCM_FLOAT, 2, { b -> for (i in 0 until 9600) b.putFloat(if (i % 2 == 0) 1f else -1f) }, 9600 * 4)
    while (ob.hasRemaining()) check(Math.abs(ob.float) <= 1f)
    // Bitstream (passthrough) is refused: the processor stays out of the chain.
    check(p.configure(AudioProcessor.AudioFormat(48000, 6, C.ENCODING_AC3)) == AudioProcessor.AudioFormat.NOT_SET)
    p.flush(); check(!p.isActive)
    check(p.configure(AudioProcessor.AudioFormat(48000, 2, C.ENCODING_PCM_24BIT)) == AudioProcessor.AudioFormat.NOT_SET)
    NoiteAudioProcessor.nivel = NoiteMath.DESLIGADO
    println("modonoite_android: PASS (Kotlin == C reference on $conferidos samples, worst diff $pior; processor off/16-bit/float/5.1/live level/no clip/bitstream)")
}
""")
    out = tmp / "replay"
    subprocess.run([java, "-cp", cp, "org.jetbrains.kotlin.cli.jvm.K2JVMCompiler", "-no-reflect", "-no-stdlib",
                    "-jvm-target", "17", "-classpath", os.pathsep.join(map(str, deps)), "-d", str(out),
                    str(kt / "NoiteMath.kt"), str(kt / "GanhoMath.kt"), str(kt / "NoiteAudioProcessor.kt"), str(replay)], check=True)
    rt = os.pathsep.join(map(str, (stdlib, common, guava, sdk)))
    subprocess.run([java, "-cp", os.pathsep.join((str(out), rt)), "ReplayKt", str(vetores)], check=True)
