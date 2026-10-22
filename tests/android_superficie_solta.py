#!/usr/bin/env python3
"""NvPlayer's real surface recreation on JVM doubles, with the playback thread busy.

Media3 1.8.0 (bytecode of ExoPlayerImpl.setVideoOutputInternal /
ExoPlayerImplInternal.setVideoOutput): destroying the video Surface while the
decoder still renders into it makes ComponentListener.surfaceDestroyed wait
for the PLAYBACK thread on the MAIN thread, up to detachSurfaceTimeoutMs
(2000 ms). The owner's TCL logged the `recriar` runnable holding the main
thread for 7470 ms (10-08 00:49:48, "Slow dispatch took 7470ms ...
NvPlayer$$ExternalSyntheticLambda8" = recriar$lambda); a key pressed during
that is an ANR.

The doubles keep that contract: SurfaceView.visibility = GONE blocks the
calling thread until the playback thread detached the renderer (or 2 s), and
the playback thread starts busy for 600 ms. The real `recriar` (and the
members it may use, `soltando` / `soltarSaida`) is extracted from NvPlayer.kt.
Invariant: no main-thread task takes 100 ms, and the surface is still
recreated (GONE then VISIBLE, focus returned) once the playback thread
answers. Android/Media3 runtime behaviour still needs the TV.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()


def bloco(inicio, obrigatorio=True):
    start = source.find(inicio)
    if start < 0:
        if obrigatorio:
            raise SystemExit(f'NvPlayer.kt sem "{inicio.strip()}"')
        return ''
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def linha(inicio):
    start = source.find(inicio)
    return '' if start < 0 else source[start:source.index('\n', start)]


membros = '\n'.join([linha('    private var soltando'),
                     bloco('    private fun soltarSaida(', False),
                     bloco('    private val recriar = Runnable {')])
fixture = r'''
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

object View { const val GONE = 8; const val VISIBLE = 0 }
object C { const val TRACK_TYPE_VIDEO = 2; const val TRACK_TYPE_AUDIO = 1 }
object SystemClock { fun elapsedRealtime() = System.nanoTime() / 1_000_000 }
object Log {
    fun i(tag: String, m: String): Int { println("  I $m"); return 0 }
    fun w(tag: String, m: String): Int { println("  W $m"); return 0 }
}
class PlayerMessage(private val p: ExoPlayer, private val alvo: Target) {
    fun interface Target { fun handleMessage(messageType: Int, message: Any?) }
    private var tipo = 0
    private var carga: Any? = null
    fun setType(t: Int): PlayerMessage { tipo = t; return this }
    fun setPayload(o: Any?): PlayerMessage { carga = o; return this }
    // Como o ExoPlayerImplInternal: depois do release a mensagem e descartada.
    fun send(): PlayerMessage { if (!p.liberado) p.fio.execute { alvo.handleMessage(tipo, carga) }; return this }
}
interface Renderer : PlayerMessage.Target { companion object { const val MSG_SET_VIDEO_OUTPUT = 1 } }
class Decoder(val tipo: Int) : Renderer {
    @Volatile var saida: Any? = null
    override fun handleMessage(messageType: Int, message: Any?) {
        if (messageType == Renderer.MSG_SET_VIDEO_OUTPUT) saida = message
    }
}
class ExoPlayer {
    val fio = Executors.newSingleThreadExecutor { r -> Thread(r, "playback").apply { isDaemon = true } }
    val renderers = listOf(Decoder(C.TRACK_TYPE_AUDIO), Decoder(C.TRACK_TYPE_VIDEO))
    @Volatile var liberado = false
    val rendererCount get() = renderers.size
    fun getRendererType(i: Int) = renderers[i].tipo
    fun getRenderer(i: Int): Renderer = renderers[i]
    fun createMessage(t: PlayerMessage.Target) = PlayerMessage(this, t)
    // O fio de reproducao preso (decoder, HAL de audio) por `ms`.
    fun ocupar(ms: Long) { fio.execute { Thread.sleep(ms) } }
}
class Surface { @Volatile var isValid = true }
class Holder { @Volatile var surface: Surface? = null }
// SurfaceView + o SurfaceHolder.Callback do Media3 (ExoPlayerImpl.ComponentListener).
class SurfaceView(private val p: ExoPlayer) {
    val holder = Holder().also { it.surface = Surface(); p.renderers[1].saida = it.surface }
    val historia = ArrayList<String>()
    var visibility = View.VISIBLE
        set(v) {
            if (v == field) return
            field = v
            if (v == View.GONE) {
                historia.add(if (p.renderers[1].saida == null) "GONE-solto" else "GONE-ligado")
                // surfaceDestroyed -> setVideoOutputInternal(null): espera o fio
                // de reproducao AQUI, ate detachSurfaceTimeoutMs.
                val feito = CountDownLatch(1)
                if (!p.liberado) p.fio.execute { p.renderers[1].saida = null; feito.countDown() }
                else feito.countDown()
                feito.await(2000, TimeUnit.MILLISECONDS)
                holder.surface?.isValid = false
            } else {
                historia.add("VISIBLE")
                val s = Surface(); holder.surface = s
                if (!p.liberado) p.fio.execute { p.renderers[1].saida = s }
            }
        }
}
class NuvioActivity { var focos = 0; fun devolverFoco() { focos++ } }
// O fio principal: uma fila, e o maior tempo que UMA tarefa segurou o fio.
class Principal {
    private val fio = Executors.newSingleThreadScheduledExecutor { r -> Thread(r, "main").apply { isDaemon = true } }
    @Volatile var pior = 0L
    @Volatile var falha: Throwable? = null
    private fun medida(r: Runnable) = Runnable {
        val ini = System.nanoTime()
        try { r.run() } catch (t: Throwable) { falha = t }
        val ms = (System.nanoTime() - ini) / 1_000_000
        if (ms > pior) pior = ms
    }
    fun post(r: Runnable): Boolean { fio.execute(medida(r)); return true }
    fun postDelayed(r: Runnable, ms: Long): Boolean { fio.schedule(medida(r), ms, TimeUnit.MILLISECONDS); return true }
    fun removeCallbacks(r: Runnable) {}
    fun esperar() { val l = CountDownLatch(1); fio.execute { l.countDown() }; l.await() }
}
object Fixture {
    const val TAG = "NvPlayer"
    val principal = Principal()
    var activity: Any? = NuvioActivity()
    var player: ExoPlayer? = null
    var superficie: SurfaceView? = null
MEMBROS
    fun recriarAgora() { principal.post(recriar) }
}
fun esperarAte(ms: Long, ok: () -> Boolean): Boolean {
    val fim = System.currentTimeMillis() + ms
    while (System.currentTimeMillis() < fim) { if (ok()) return true; Thread.sleep(5) }
    return ok()
}
fun main() {
    val f = Fixture
    val act = f.activity as NuvioActivity

    // 1. Fio de reproducao preso 600 ms: o fio principal nao pode ficar com ele.
    var p = ExoPlayer(); var sv = SurfaceView(p)
    f.principal.post { f.player = p; f.superficie = sv }
    f.principal.post { p.ocupar(600) }
    f.recriarAgora()
    f.principal.esperar()
    check(f.principal.falha == null) { "excecao no fio principal: ${f.principal.falha}" }
    check(f.principal.pior < 100) {
        "fio principal preso ${f.principal.pior} ms na recriacao da superficie com o fio de reproducao ocupado"
    }
    check(esperarAte(3000) { sv.historia.size == 2 && act.focos == 1 }) { "nao recriou: ${sv.historia}" }
    f.principal.esperar()
    check(sv.historia == listOf("GONE-solto", "VISIBLE")) { "ordem errada: ${sv.historia}" }
    check(esperarAte(1000) { p.renderers[1].saida === sv.holder.surface }) { "decoder sem a superficie nova" }
    check(f.principal.pior < 100) { "fio principal preso ${f.principal.pior} ms" }

    // 2. Duas recriacoes pedidas com a primeira ainda esperando: uma so.
    p.ocupar(300)
    f.recriarAgora(); f.recriarAgora()
    check(esperarAte(3000) { sv.historia.size >= 4 }) { "segunda recriacao nao veio: ${sv.historia}" }
    Thread.sleep(200); f.principal.esperar()
    check(sv.historia.size == 4 && act.focos == 2) { "recriou demais: ${sv.historia}" }
    check(f.principal.pior < 100) { "fio principal preso ${f.principal.pior} ms" }

    // 3. Player liberado com o pedido no ar: nada acontece na superficie dele,
    //    e o player seguinte recria normalmente.
    p.ocupar(200)
    f.recriarAgora()
    f.principal.post { p.liberado = true }
    val p2 = ExoPlayer(); val sv2 = SurfaceView(p2)
    f.principal.post { f.player = p2; f.superficie = sv2 }
    f.recriarAgora()
    check(esperarAte(3000) { sv2.historia.size == 2 }) { "player novo nao recriou: ${sv2.historia}" }
    Thread.sleep(300); f.principal.esperar()
    check(sv.historia.size == 4) { "mexeu na superficie do player liberado: ${sv.historia}" }
    check(sv2.historia == listOf("GONE-solto", "VISIBLE"))
    check(f.principal.falha == null) { "excecao no fio principal: ${f.principal.falha}" }
    check(f.principal.pior < 100) { "fio principal preso ${f.principal.pior} ms" }
    println("android_superficie_solta: fio principal livre (pior ${f.principal.pior} ms), recriacao unica e player liberado ok")
}
'''.replace('MEMBROS', membros)
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'


def jar(group, artifact, version):
    matches = list((cache / group / artifact / version).glob('*/*.jar'))
    if not matches:
        raise SystemExit(f'Missing cached {artifact}:{version}; run Android Gradle build first')
    return str(matches[0])


stdlib = jar('org.jetbrains.kotlin', 'kotlin-stdlib', '2.0.21')
classpath = ':'.join([
    jar('org.jetbrains.kotlin', 'kotlin-compiler-embeddable', '2.0.21'), stdlib,
    jar('org.jetbrains.kotlin', 'kotlin-script-runtime', '2.0.21'),
    jar('org.jetbrains.intellij.deps', 'trove4j', '1.0.20200330'),
    jar('org.jetbrains.kotlinx', 'kotlinx-coroutines-core-jvm', '1.6.4'),
    jar('org.jetbrains', 'annotations', '13.0'),
])
java_home = os.environ.get('JAVA_HOME')
if not java_home:
    homes = sorted((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home'))
    java_home = str(homes[0]) if homes else None
java = str(Path(java_home) / 'bin/java') if java_home else 'java'
with tempfile.TemporaryDirectory(prefix='nuvio-superficie-solta-') as work:
    work = Path(work)
    file = work / 'SuperficieFixture.kt'; file.write_text(fixture)
    target = work / 'classes'
    subprocess.run([java, '-cp', classpath, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', stdlib,
                    '-jvm-target', '17', '-d', str(target), str(file)], check=True)
    subprocess.run([java, '-cp', f'{target}:{stdlib}', 'SuperficieFixtureKt'], check=True)
