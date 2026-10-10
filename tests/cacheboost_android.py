"""F07 seek cache + volume boost, Android side.

1. Compile-check the REAL NvPlayer.kt, CacheMidia.kt, CacheSessao.kt, GanhoMath.kt,
   GanhoAudioProcessor.kt with ParaleloDataSource.kt, PassivoMedidor.kt and
   NuvioActivity.kt against android-35 + the Media3 1.8.0 / androidx jars already in
   the Gradle cache (+ SDL's Java sources). No Gradle/APK build.
2. Run the real CacheSessao.kt, GanhoMath.kt and GanhoAudioProcessor.kt on the JVM:
   statvfs-fit limits, session key, folder names, crash-leftover cleanup (with a
   symlink that must not be followed), the write guard under ENOSPC / IO errors /
   failures in open and close / a sibling sink failing, and the gain math (exact at
   100%, linear below the knee, soft-limited, monotonic, never beyond full scale) and the
   processor on 16-bit and float buffers.
No device, network or downloads.
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
sdl = Path(os.environ.get("NUVIO_ANDROID_CACHE", str(Path.home() / ".cache/nuvio-android"))) / "src/SDL2-2.30.9/android-project/app/src/main/java"
assert sdk.is_file() and any("compiler-embeddable" in str(x) for x in compiler)
cp = os.pathsep.join(map(str, compiler))
java = os.environ.get("NUVIO_TEST_JAVA") or str(next((Path.home() / ".local/jdks").glob("jdk-17*/Contents/Home/bin/java")))


def kotlinc(sources, out, classpath, extra=()):
    subprocess.run([java, "-cp", cp, "org.jetbrains.kotlin.cli.jvm.K2JVMCompiler",
                    "-no-reflect", "-no-stdlib", "-jvm-target", "17", "-classpath", classpath,
                    *extra, "-d", str(out), *map(str, sources)], check=True)


def aar_jar(group, name, version, tmp):
    aar = next(cache.glob(f"{group}/{name}/{version}/*/{name}-{version}.aar"))
    dst = tmp / f"{name}-{version}.jar"
    with zipfile.ZipFile(aar) as z:
        dst.write_bytes(z.read("classes.jar"))
    return dst


def jar(group, name, version):
    return next(cache.glob(f"{group}/{name}/{version}/*/{name}-{version}.jar"))


with tempfile.TemporaryDirectory(prefix="nuvio-cacheboost-android-", dir=os.environ.get("TMPDIR")) as tmp:
    tmp = Path(tmp)
    deps = [sdk, stdlib]
    media3 = {}
    for name in ("media3-common", "media3-datasource", "media3-exoplayer", "media3-decoder",
                 "media3-extractor", "media3-container", "media3-database"):
        media3[name] = aar_jar("androidx.media3", name, "1.8.0", tmp)
        deps.append(media3[name])
    deps.append(aar_jar("androidx.core", "core", "1.13.1", tmp))
    deps.append(jar("androidx.annotation", "annotation-jvm", "1.6.0"))
    deps.append(aar_jar("androidx.annotation", "annotation-experimental", "1.4.0", tmp))
    guava = jar("com.google.guava", "guava", "33.3.1-android")
    deps.append(guava)
    sources = [kt / n for n in ("NvPlayer.kt", "ParaleloDataSource.kt", "PassivoMedidor.kt", "NuvioActivity.kt",
                                "CacheSessao.kt", "CacheMidia.kt", "GanhoMath.kt", "GanhoAudioProcessor.kt",
                                "AudioSyncTap.kt", "AudioSyncSink.kt", "RecriaHdr.kt", "ArranqueVigia.kt")]
    java_roots = []
    if sdl.is_dir():
        java_roots = ["-Xjava-source-roots=" + str(sdl)]
        sources.append(sdl)
    else:
        # A Activity real so devolve o foco aqui; SDL nao e necessario neste teste.
        activity = tmp / "NuvioActivity.kt"
        activity.write_text("package space.nuvio.nativelegacy\n"
                            "class NuvioActivity : android.app.Activity() {\n"
                            "  fun devolverFoco() {}\n"
                            "  fun nativeQuadros(): Long = -1L\n"
                            "  fun nativeEtapa(): String = \"lib-nao-carregou\"\n"
                            "  fun estadoSdl(): String = \"\"\n"
                            "  fun nativeRecUrl(): String = \"\"\n"
                            "}\n")
        sources.remove(kt / "NuvioActivity.kt")
        sources.insert(0, activity)
        print("note: SDL Java sources missing; NuvioActivity.kt stubbed here")
    kotlinc(sources, tmp / "app", os.pathsep.join(map(str, deps)), java_roots + (["-Werror"] if os.environ.get("NV_KT_WERROR") else []))
    print("compile-check: NvPlayer/CacheMidia/CacheSessao/GanhoMath/GanhoAudioProcessor (+ParaleloDataSource/PassivoMedidor"
          + ("/NuvioActivity" if java_roots else "") + ") against android-35 + Media3 1.8.0: OK")

    replay = tmp / "Replay.kt"
    replay.write_text(r"""
import space.nuvio.nativelegacy.CacheSessao
import space.nuvio.nativelegacy.GanhoMath
import space.nuvio.nativelegacy.GanhoAudioProcessor
import androidx.media3.common.C
import androidx.media3.common.audio.AudioProcessor
import java.io.File
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files

const val MB = 1L shl 20
fun main() {
    // --- statvfs fit: largest option <= asked that leaves 512 MB free; unknown = off
    check(CacheSessao.limiteMb(256, 768 * MB) == 256)
    check(CacheSessao.limiteMb(256, 768 * MB - 1) == 0)
    check(CacheSessao.limiteMb(1024, 1536 * MB) == 1024)
    check(CacheSessao.limiteMb(1024, 1536 * MB - 1) == 512)
    check(CacheSessao.limiteMb(1024, 900 * MB) == 256)
    check(CacheSessao.limiteMb(512, 64_000 * MB) == 512)
    check(CacheSessao.limiteMb(0, 64_000 * MB) == 0)
    check(CacheSessao.limiteMb(512, 0) == 0 && CacheSessao.limiteMb(512, -1) == 0)
    check(CacheSessao.limiteMb(100, 64_000 * MB) == 0)   // not an offered size

    // --- key and names
    check(CacheSessao.chave("s3", "https://cdn.example/f.mkv?t=1#frag") == "s3|https://cdn.example/f.mkv?t=1")
    check(CacheSessao.chave("s3", "x") != CacheSessao.chave("s4", "x"))
    check(CacheSessao.nomeValido("b1a2-s9") && !CacheSessao.nomeValido("..") && !CacheSessao.nomeValido("a/b")
          && !CacheSessao.nomeValido("") && !CacheSessao.nomeValido("A"))
    try { CacheSessao.pastaSessao(File("/tmp"), "..", "s1"); error("accepted ..") } catch (e: IllegalArgumentException) { }

    // --- crash leftovers: other boot folders go, the current one stays, a
    //     symlink inside is removed as a link and its target is untouched
    val raiz = Files.createTempDirectory("nv-seek").toFile()
    val fora = Files.createTempDirectory("nv-fora").toFile()
    File(fora, "precioso.txt").writeText("x")
    val velho = File(File(raiz, "bold"), "s1"); velho.mkdirs(); File(velho, "a.exo").writeBytes(ByteArray(10))
    Files.createSymbolicLink(File(File(raiz, "bold"), "link").toPath(), fora.toPath())
    val atual = CacheSessao.pastaSessao(raiz, "bnow", "s1"); atual.mkdirs(); File(atual, "b.exo").writeBytes(ByteArray(10))
    check(CacheSessao.limparSobras(raiz, "bnow") == 1)
    check(!File(raiz, "bold").exists() && File(atual, "b.exo").exists())
    check(File(fora, "precioso.txt").exists()) { "followed a symlink" }
    check(CacheSessao.limparSobras(File(raiz, "missing"), "bnow") == 0)
    check(CacheSessao.apagar(atual) && !atual.exists())
    raiz.deleteRecursively(); fora.deleteRecursively()

    // --- ENOSPC recognition (Android wraps ErrnoException in IOException)
    check(CacheSessao.semEspaco(IOException("write failed", IOException("write failed: ENOSPC (No space left on device)"))))
    check(CacheSessao.semEspaco(IOException("No space left on device")))
    check(!CacheSessao.semEspaco(IOException("Connection reset")) && !CacheSessao.semEspaco(null))

    // --- write guard: buffered, commits only successful inner writes, first
    //     failure turns the session off once, data keeps flowing
    class Interno(var falharEm: Int = Int.MAX_VALUE, var falharAbrir: Boolean = false, var falharFechar: Boolean = false) {
        val gravado = java.io.ByteArrayOutputStream(); var abriu = 0; var fechou = 0; var escritas = 0
        fun abrir() { abriu++; if (falharAbrir) throw IOException("open: ENOSPC") }
        fun escrever(b: ByteArray, o: Int, n: Int) {
            if (gravado.size() + n > falharEm) {
                val cabe = falharEm - gravado.size(); gravado.write(b, o, cabe)   // partial OS write
                throw IOException("write failed: ENOSPC (No space left on device)")
            }
            escritas++; gravado.write(b, o, n)
        }
        fun fechar() { fechou++; if (falharFechar) throw IllegalStateException("cache released") }
    }
    val avisos = mutableListOf<Boolean>()
    fun estado() = CacheSessao.Estado { avisos.add(it) }
    fun escrita(e: CacheSessao.Estado, i: Interno, tam: Int = 4096) =
        CacheSessao.Escrita(e, tam, i::abrir, i::escrever, i::fechar)
    val dados = ByteArray(20_000) { (it * 7).toByte() }
    run {   // healthy: everything lands, in order, flushed in buffer-sized chunks
        avisos.clear(); val i = Interno(); val e = estado(); val w = escrita(e, i)
        w.abrir(); w.escrever(dados, 0, 5000); w.escrever(dados, 5000, 15000); w.fechar()
        check(i.gravado.toByteArray().contentEquals(dados) && i.abriu == 1 && i.fechou == 1 && avisos.isEmpty() && !e.desligado)
        check(i.escritas == 5) { "buffered writes: ${i.escritas}" }
    }
    run {   // ENOSPC mid-stream: one notice, later bytes discarded, inner still closed
        avisos.clear(); val i = Interno(falharEm = 10_000); val e = estado(); val w = escrita(e, i)
        w.abrir(); w.escrever(dados, 0, 20_000); w.escrever(dados, 0, 4096); w.fechar()
        check(avisos == listOf(true) && e.desligado && i.fechou == 1)
        // what the inner sink acknowledged (its own count) is an exact prefix
        val ok = i.escritas * 4096
        check(ok == 8192 && dados.copyOfRange(0, ok).contentEquals(i.gravado.toByteArray().copyOfRange(0, ok)))
        // next sink of the same session: never opens the inner sink again
        val i2 = Interno(); val w2 = escrita(e, i2)
        w2.abrir(); w2.escrever(dados, 0, 9000); w2.fechar()
        check(i2.abriu == 0 && i2.fechou == 0 && avisos.size == 1)
    }
    run {   // failure in open: off, no close of a never-opened sink, generic IO = not ENOSPC
        avisos.clear(); val i = Interno(falharAbrir = true); val e = estado(); val w = escrita(e, i)
        w.abrir(); w.escrever(dados, 0, 100); w.fechar()
        check(avisos == listOf(true) && i.fechou == 0)
    }
    run {   // a sibling sink fails: this one stops writing but commits what it wrote
        avisos.clear(); val e = estado(); val i = Interno(); val w = escrita(e, i)
        w.abrir(); w.escrever(dados, 0, 4096)          // one full buffer flushed
        e.falhou(IOException("Connection reset"))      // sibling
        w.escrever(dados, 4096, 8000); w.fechar()
        check(avisos == listOf(false) && i.gravado.size() == 4096 && i.fechou == 1)
    }
    run {   // close throws (released cache): swallowed, reported once
        avisos.clear(); val e = estado(); val i = Interno(falharFechar = true); val w = escrita(e, i)
        w.abrir(); w.escrever(dados, 0, 100); w.fechar(); w.fechar()
        check(avisos == listOf(false) && i.gravado.size() == 100)
    }

    // --- gain math
    check(GanhoMath.volumePlayer(50) == 0.5f && GanhoMath.volumePlayer(150) == 1f && GanhoMath.volumePlayer(-9) == 0f)
    check(GanhoMath.reforco(80) == 1f && GanhoMath.reforco(150) == 1.5f && GanhoMath.reforco(999) == 2f)
    check(Math.abs(GanhoMath.decibeis(200) - 6.0206) < 1e-3 && GanhoMath.decibeis(0) == -120.0)
    for (s in listOf<Short>(-32768, -1, 0, 1, 12345, 32767)) check(GanhoMath.pcm16(s, 1f) == s) { "100% must be bit-exact" }
    check(Math.abs(GanhoMath.amostra(0.3f, 2f) - 0.6f) < 1e-6)           // linear below the knee
    var antes = -2f
    var x = -1f
    while (x <= 1f) {
        for (g in listOf(1.1f, 1.5f, 2f)) {
            val y = GanhoMath.amostra(x, g)
            check(Math.abs(y) <= 1f) { "beyond full scale at x=$x g=$g" }
            check(Math.abs(y) <= Math.abs(x * g) + 1e-6f)                // never louder than linear
        }
        val y2 = GanhoMath.amostra(x, 2f)
        check(y2 >= antes - 1e-7f) { "not monotonic at $x" }; antes = y2
        x += 0.001f
    }
    val k = GanhoMath.JOELHO
    check(Math.abs(GanhoMath.amostra(k / 2f + 1e-4f, 2f) - GanhoMath.amostra(k / 2f - 1e-4f, 2f) - 4e-4f) < 1e-4f)  // slope 1*g across the knee
    check(GanhoMath.pcm16(32767, 2f) in 30000..32767 && GanhoMath.pcm16(-32768, 2f) in -32768..-30000)

    // --- the processor on real buffers
    fun processar(enc: Int, g: Float, enche: (ByteBuffer) -> Unit, n: Int): ByteBuffer {
        val p = GanhoAudioProcessor(); p.ganho = g
        val out = p.configure(AudioProcessor.AudioFormat(48000, 2, enc))
        check(out.encoding == enc && p.isActive)
        p.flush()
        val inp = ByteBuffer.allocateDirect(n).order(ByteOrder.nativeOrder()); enche(inp); inp.flip()
        p.queueInput(inp); check(!inp.hasRemaining())
        return p.output.order(ByteOrder.nativeOrder())
    }
    run {
        val o = processar(C.ENCODING_PCM_16BIT, 2f, { b -> b.putShort(1000); b.putShort(-1000); b.putShort(32767); b.putShort(0) }, 8)
        check(o.short == 2000.toShort() && o.short == (-2000).toShort() && o.short in 30000..32767 && o.short == 0.toShort())
        val o1 = processar(C.ENCODING_PCM_16BIT, 1f, { b -> b.putShort(32767); b.putShort(-32768) }, 4)
        check(o1.short == 32767.toShort() && o1.short == (-32768).toShort())
        val of = processar(C.ENCODING_PCM_FLOAT, 1.5f, { b -> b.putFloat(0.2f); b.putFloat(-1f) }, 8)
        check(Math.abs(of.float - 0.3f) < 1e-6f && of.float > -1f)
        val p = GanhoAudioProcessor()
        check(p.configure(AudioProcessor.AudioFormat(48000, 6, C.ENCODING_AC3)) == AudioProcessor.AudioFormat.NOT_SET && !p.isActive)
    }
    println("cacheboost_android: PASS (statvfs fit, key/names, leftover cleanup without following links, ENOSPC guard, gain exact/linear/limited/monotonic, processor 16-bit/float/bitstream)")
}
""")
    rt = os.pathsep.join(map(str, (stdlib, media3["media3-common"], guava, sdk)))
    kotlinc([kt / "CacheSessao.kt", kt / "GanhoMath.kt", kt / "GanhoAudioProcessor.kt", replay], tmp / "replay",
            os.pathsep.join(map(str, deps)))
    # android.jar is only stubs: at run time it must come AFTER the real jars
    # and nothing here may touch an Android class.
    subprocess.run([java, "-cp", os.pathsep.join((str(tmp / "replay"), rt)), "ReplayKt"], check=True)
