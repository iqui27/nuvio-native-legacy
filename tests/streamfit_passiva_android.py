"""StreamFit passive telemetry, Android side (F03).

1. Compile-check the REAL NvPlayer.kt, ParaleloDataSource.kt, PassivoMedidor.kt and
   NuvioActivity.kt against android-35 + the Media3 1.8.0 / androidx jars already in
   the Gradle cache (+ SDL's Java sources for SDLActivity). No Gradle/APK build.
2. Replay PassivoMedidor.kt (real file, pure Kotlin) on the JVM with a fake clock:
   parallel overlap without double counting, stall zeros, buffer-full idle,
   pause, pre-READY, cache/memory deliveries (never reported), host change,
   epoch change, stale session token, 5..48 window and cadence.
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
sdl = Path.home() / ".cache/nuvio-android/src/SDL2-2.30.9/android-project/app/src/main/java"
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


with tempfile.TemporaryDirectory(prefix="nuvio-streamfit-passive-", dir=os.environ.get("TMPDIR")) as tmp:
    tmp = Path(tmp)
    deps = [sdk, stdlib]
    for name in ("media3-common", "media3-datasource", "media3-exoplayer", "media3-decoder",
                 "media3-extractor", "media3-container"):
        deps.append(aar_jar("androidx.media3", name, "1.8.0", tmp))
    deps.append(aar_jar("androidx.core", "core", "1.13.1", tmp))
    deps.append(jar("androidx.annotation", "annotation-jvm", "1.6.0"))
    deps.append(aar_jar("androidx.annotation", "annotation-experimental", "1.4.0", tmp))
    deps.append(jar("com.google.guava", "guava", "33.3.1-android"))
    sources = [kt / "NvPlayer.kt", kt / "ParaleloDataSource.kt", kt / "PassivoMedidor.kt", kt / "NuvioActivity.kt",
               kt / "AudioSyncTap.kt", kt / "AudioSyncSink.kt", kt / "RecriaHdr.kt",
               kt / "ArranqueVigia.kt", kt / "TouchViewport.kt", kt / "Tombstone.kt",
               # F07: NvPlayer layers the seek cache and the gain processor
               kt / "CacheSessao.kt", kt / "CacheMidia.kt", kt / "GanhoMath.kt", kt / "GanhoAudioProcessor.kt"]
    java_roots = []
    if sdl.is_dir():
        java_roots = ["-Xjava-source-roots=" + str(sdl)]
        sources.append(sdl)
    else:
        sources.remove(kt / "NuvioActivity.kt")
        print("note: SDL Java sources missing; NuvioActivity.kt only checked by redemarca_android.py")
    kotlinc(sources, tmp / "app", os.pathsep.join(map(str, deps)), java_roots)
    print("compile-check: NvPlayer/ParaleloDataSource/PassivoMedidor" +
          ("/NuvioActivity" if java_roots else "") + " against android-35 + Media3 1.8.0: OK")

    replay = tmp / "Replay.kt"
    replay.write_text("""
import space.nuvio.nativelegacy.PassivoMedidor
var agora = 1_000_000L
var rede = 7L
val saidas = mutableListOf<PassivoMedidor.Entrega>()
fun novo() = PassivoMedidor({ agora }, { 1_700_000_000_000L + agora }, { rede }) { saidas.add(it) }
const val H = "https://cdn.example"
fun seg(m: PassivoMedidor, tok: Long, bytesPorConexao: List<Int>, host: String = H) {
    // one wall-clock second; every connection active for the whole second and
    // all of them overlapping in time (the parallel ParaleloDataSource case)
    for (b in bytesPorConexao) m.inicio(tok)
    for (passo in 1..10) {
        for (b in bytesPorConexao) m.bytes(tok, host, b / 10)
        agora += 100
    }
    for (b in bytesPorConexao) m.fim(tok)
}
fun main() {
    // --- parallel overlap: four connections x 250 KB in the SAME second = 8 Mb/s once
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(11); m.estado(t, true, true)
        repeat(6) { seg(m, t, listOf(250_000, 250_000, 250_000, 250_000)) }
        agora += 1; m.inicio(t); m.fim(t)
        check(saidas.size == 1) { "first window after 5 seconds: ${saidas.size}" }
        val e = saidas[0]
        check(e.kbps.size == 5 && e.kbps.all { it == 8000 }) { e.kbps.toList().toString() }
        check(e.geracao == 11 && e.rede == 7L && e.origem == H)
    }
    // --- stall: active transfer, no byte for 3 s -> three zero seconds
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(12); m.estado(t, true, true)
        repeat(3) { seg(m, t, listOf(125_000)) }
        m.inicio(t); m.bytes(t, H, 1)
        agora += 3000; m.bytes(t, H, 1); m.fim(t)
        agora += 1000; m.inicio(t); m.fim(t)
        check(saidas.size == 1 && saidas[0].kbps.toList() == listOf(1000, 1000, 1000, 0, 0)) { saidas.map { it.kbps.toList() }.toString() }
    }
    // --- buffer-full idle: no connection active -> seconds discarded, not zero
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(13); m.estado(t, true, true)
        repeat(3) { seg(m, t, listOf(125_000)) }
        agora += 20_000                      // ExoPlayer stopped reading, workers idle
        repeat(2) { seg(m, t, listOf(125_000)) }
        agora += 1; m.inicio(t); m.fim(t)
        check(saidas.size == 1 && saidas[0].kbps.all { it == 1000 } && saidas[0].kbps.size == 5)
        // partially active second (50%) is not network-bound evidence
        val m2 = novo(); val t2 = m2.sessao(14); m2.estado(t2, true, true); saidas.clear()
        repeat(10) { m2.inicio(t2); agora += 500; m2.bytes(t2, H, 62_500); m2.fim(t2); agora += 500 }
        check(saidas.isEmpty())
    }
    // --- pause and pre-READY are excluded
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(15)
        m.estado(t, true, false)             // playing intent, extractor not READY yet
        repeat(6) { seg(m, t, listOf(125_000)) }
        m.estado(t, false, true)             // paused (playWhenReady=false)
        repeat(6) { seg(m, t, listOf(125_000)) }
        check(saidas.isEmpty()) { "paused/pre-ready leaked" }
        m.estado(t, true, true)
        repeat(6) { seg(m, t, listOf(125_000)) }
        agora += 1; m.inicio(t); m.fim(t)
        check(saidas.size == 1 && saidas[0].kbps.size == 5)
    }
    // --- memory/cache deliveries are never reported: only socket bytes exist here.
    //     A second with activity but bytes from no known host is tainted.
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(16); m.estado(t, true, true)
        repeat(6) { m.inicio(t); agora += 1000; m.bytes(t, null, 999_999); m.fim(t) }
        check(saidas.isEmpty())
    }
    // --- final host change discards the previous host's window
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(17); m.estado(t, true, true)
        repeat(4) { seg(m, t, listOf(125_000)) }
        repeat(4) { seg(m, t, listOf(125_000), "https://other.example") }
        agora += 1; m.inicio(t); m.fim(t)
        check(saidas.isEmpty()) { "mixed hosts delivered" }
    }
    // --- network epoch change mid-series discards it; unknown network never delivers
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(18); m.estado(t, true, true)
        repeat(4) { seg(m, t, listOf(125_000)) }
        rede = 8L
        repeat(4) { seg(m, t, listOf(125_000)) }
        check(saidas.isEmpty()) { "epoch change leaked" }
        rede = 0L
        repeat(8) { seg(m, t, listOf(125_000)) }
        check(saidas.isEmpty())
        rede = 7L
    }
    // --- stale session: events from an old token never count
    run {
        saidas.clear(); val m = novo(); val velho = m.sessao(19); m.estado(velho, true, true)
        val t = m.sessao(20); m.estado(t, true, true)
        repeat(6) { seg(m, velho, listOf(125_000)) }
        check(saidas.isEmpty())
        m.encerrar(); repeat(6) { seg(m, t, listOf(125_000)) }
        check(saidas.isEmpty())
    }
    // --- window cap 48 and cadence (5, then every 10)
    run {
        saidas.clear(); val m = novo(); val t = m.sessao(21); m.estado(t, true, true)
        repeat(70) { seg(m, t, listOf(125_000)) }
        agora += 1; m.inicio(t); m.fim(t)
        check(saidas.map { it.kbps.size } == listOf(5, 15, 25, 35, 45, 48, 48)) { saidas.map { it.kbps.size }.toString() }
    }
    check(PassivoMedidor.origemDe(java.net.URL("https://user:pw@CDN.Example:443/a/b?token=x")) == "https://cdn.example")
    check(PassivoMedidor.origemDe(java.net.URL("http://cdn.example:8080/x")) == "http://cdn.example:8080")
    check(PassivoMedidor.origemDe(java.net.URL("ftp://cdn.example/x")) == null)
    println("streamfit_passiva_android: PASS (overlap once, stall zeros, idle/partial/pause/pre-ready/memory excluded, host/epoch/session invalidation, 5..48 cadence, authority only)")
}
""")
    kotlinc([kt / "PassivoMedidor.kt", replay], tmp / "replay", str(stdlib))
    subprocess.run([java, "-cp", os.pathsep.join((str(tmp / "replay"), str(stdlib))), "ReplayKt"], check=True)
