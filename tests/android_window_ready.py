#!/usr/bin/env python3
"""Execute production crop request/acknowledgement with queued UI/pre-draw doubles."""
from pathlib import Path
import tempfile
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
start = source.index('    private val janelaPedido =')
end = source.index('    @JvmStatic fun escolher(', start)
methods = source[start:end]
callbacks = source[source.index('    private val antesDoMedia3 ='):source.index('    private val depoisDoMedia3 =')]
fixture = '''
import java.util.concurrent.atomic.AtomicInteger
object View { const val VISIBLE = 0; const val GONE = 8 }
object Log { fun i(tag: String, message: String) {} }
object SystemClock { fun elapsedRealtime() = 0L }
class Surface { var isValid = true }
class SurfaceHolder {
    val surface = Surface(); val surfaceFrame = "frame"
    interface Callback {
        fun surfaceCreated(h: SurfaceHolder)
        fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, a: Int)
        fun surfaceDestroyed(h: SurfaceHolder)
    }
}
class Observer {
    interface OnPreDrawListener { fun onPreDraw(): Boolean }
    val listeners = mutableListOf<OnPreDrawListener>()
    fun addOnPreDrawListener(l: OnPreDrawListener) { listeners.add(l) }
    fun removeOnPreDrawListener(l: OnPreDrawListener) { listeners.remove(l) }
    fun draw() { listeners.toList().forEach { it.onPreDraw() } }
}
class SurfaceView {
    val holder = SurfaceHolder(); val viewTreeObserver = Observer()
    var isLayoutRequested = false; var visibility = View.VISIBLE
    var layoutListener: ((SurfaceView, Int, Int, Int, Int, Int, Int, Int, Int) -> Unit)? = null
    fun addOnLayoutChangeListener(l: (SurfaceView, Int, Int, Int, Int, Int, Int, Int, Int) -> Unit) { layoutListener = l }
    fun layout(changed: Boolean) { layoutListener?.invoke(this, 0, 0, 320, 180, 0, 0, if (changed) 300 else 320, 180) }
    fun invalidate() {}
}
class Queue {
    val pending = mutableListOf<() -> Unit>()
    fun post(f: () -> Unit) { pending.add(f) }
    fun drain() { pending.toList().also { pending.clear() }.forEach { it() } }
}
object P {
    var superficie: SurfaceView? = SurfaceView()
    var player: Any? = Any()
    var recriando = false; var recriaAgendada = false
    val principal = Queue()
    const val TAG = "test"; val sessao = 1; val ultHdr = "none"
    var destruirIni = 0L
    fun observeLayout() = observarJanela(superficie!!)
    fun lifecycle(event: String, h: SurfaceHolder = superficie!!.holder) {
        when (event) {
            "destroyed" -> antesDoMedia3.surfaceDestroyed(h)
            "created" -> antesDoMedia3.surfaceCreated(h)
            "changed" -> antesDoMedia3.surfaceChanged(h, 0, 320, 180)
        }
    }
    fun definirJanela(x: Int, y: Int, w: Int, h: Int, encaixa: Boolean) {}
CALLBACKS
METHODS
}
fun main() {
    fun request(x: Int = 10) = P.janela(x, 20, 320, 180, 0)
    fun draw() { P.principal.drain(); P.superficie!!.viewTreeObserver.draw() }
    request(); check(!P.janelaPronta()); P.principal.drain()
    check(!P.janelaPronta()); draw(); check(P.janelaPronta())
    // Four 1.5-second retries: SDL can query before UI dispatch or pre-draw.
    repeat(4) {
        request()
        check(P.janelaPronta()) { "unchanged crop retry hid playing trailer before UI dispatch" }
        P.principal.drain()
        check(P.janelaPronta()) { "unchanged crop retry hid playing trailer before pre-draw" }
        draw(); check(P.janelaPronta())
    }
    request(11); check(!P.janelaPronta()); P.principal.drain()
    request(12); P.superficie!!.viewTreeObserver.draw()
    check(!P.janelaPronta()) { "stale pre-draw acknowledged changed crop" }
    draw(); check(P.janelaPronta())
    P.superficie = SurfaceView()
    check(!P.janelaPronta()) { "replacement surface inherited old acknowledgement" }
    request(12); check(!P.janelaPronta()); draw(); check(P.janelaPronta())
    fun recover(requestAgain: Boolean = true) {
        check(!P.janelaPronta()) { "recovery inherited stale acknowledgement" }
        if (requestAgain) request(12)
        check(!P.janelaPronta()); draw()
        check(P.janelaPronta()) { "recovery without a fresh crop request stayed hidden" }
    }
    P.superficie!!.holder.surface.isValid = false
    check(!P.janelaPronta()) { "destroyed native surface still ready" }
    P.lifecycle("destroyed"); P.superficie!!.holder.surface.isValid = true; P.lifecycle("created")
    recover(requestAgain = false)
    // SDL polling a transient pending layout (bounds unchanged) cannot strand readiness.
    P.superficie!!.isLayoutRequested = true
    check(!P.janelaPronta()) { "pending layout still ready" }
    P.superficie!!.isLayoutRequested = false
    check(P.janelaPronta()) { "polled transient layout request hid the trailer for good" }
    // Lifecycle callbacks must invalidate even when SDL misses the invalid interval.
    for (event in listOf("destroyed", "created", "changed")) {
        P.lifecycle(event)
        recover(requestAgain = false)
    }
    // Old holders cannot invalidate the currently acknowledged surface.
    P.lifecycle("destroyed", SurfaceHolder()); check(P.janelaPronta())
    P.superficie!!.visibility = View.GONE
    check(!P.janelaPronta()) { "hidden surface still ready" }
    P.lifecycle("destroyed"); P.superficie!!.visibility = View.VISIBLE; P.lifecycle("created")
    recover(requestAgain = false)
    P.superficie!!.isLayoutRequested = true
    check(!P.janelaPronta()) { "pending layout still ready" }
    request(12); draw(); check(!P.janelaPronta())
    P.superficie!!.isLayoutRequested = false
    draw(); check(P.janelaPronta())
    P.observeLayout()
    P.superficie!!.layout(false); check(P.janelaPronta())
    P.superficie!!.layout(true)
    recover(requestAgain = false)
    for (scheduled in listOf(false, true)) {
        P.recriando = !scheduled; P.recriaAgendada = scheduled
        check(!P.janelaPronta()) { "recovering surface still ready" }
        request(12); draw(); check(!P.janelaPronta())
        P.recriando = false; P.recriaAgendada = false
        draw(); check(P.janelaPronta())
    }
    // Surface replaced while a changed request awaited pre-draw: the new view's
    // first layout must confirm the applied request without another crop request.
    val old = P.superficie!!
    request(13); P.principal.drain()
    P.superficie = SurfaceView(); P.observeLayout()
    old.viewTreeObserver.draw()
    check(!P.janelaPronta())
    P.superficie!!.layout(true); draw()
    check(P.janelaPronta()) { "replacement surface stayed hidden after a pending request" }
    println("android_window_ready: valid retries preserved; lifecycle/layout recovery waits for pre-draw")
}
'''.replace('METHODS', methods).replace('CALLBACKS', callbacks).replace('android.view.ViewTreeObserver.OnPreDrawListener', 'Observer.OnPreDrawListener')
# Reuse the existing cached Kotlin compiler setup without running its fixture.
setup = (root / 'tests/android_window_layout.py').read_text()
setup = setup[setup.index('cache = Path.home()'):setup.index('with tempfile.TemporaryDirectory(')]
namespace = dict(Path=Path, os=__import__('os'))
exec(setup, namespace)
java, classpath, stdlib = (namespace[k] for k in ('java', 'classpath', 'stdlib'))
with tempfile.TemporaryDirectory(prefix='nuvio-window-ready-') as work:
    work = Path(work)
    file = work / 'Ready.kt'
    file.write_text(fixture)
    target = work / 'classes'
    subprocess.run([java, '-cp', classpath, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', stdlib,
                    '-jvm-target', '17', '-d', str(target), str(file)], check=True)
    subprocess.run([java, '-cp', f'{target}:{stdlib}', 'ReadyKt'], check=True)
