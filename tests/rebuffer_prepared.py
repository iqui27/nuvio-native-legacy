"""Replay production preparation queue and buffer sizing with Android boundaries stubbed."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
source = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
jars = list(cache.glob('org.jetbrains.kotlin/*/2.0.21/*/*.jar'))
jars += list(cache.glob('org.jetbrains.intellij.deps/trove4j/*/*/*.jar'))
jars += list(cache.glob('org.jetbrains/annotations/13.0/*/*.jar'))
jars += list(cache.glob('org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar'))
stdlib = next(cache.glob('org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar'))
java = str(next((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home/bin/java')))

def function(name):
    start = source.index('    private fun ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

preparo = re.search(r'private class Preparo\([\s\S]*?\) \{', source).group()[:-1] + '{ var recursos: RecursosPlayer? = null }'
queue = source[source.index('    @JvmOverloads @JvmStatic fun preparar('):source.index('    private fun prepararMain(')]
main = function('prepararMain')
main = main[:main.index('            s.recursos = r')] + '            s.recursos = r\n        } catch (e: Exception) { throw e }\n    }'
create = function('criarPlayer')
create = create[:create.index('        try {')] + '        return r\n    }'
harness = '''
import java.util.concurrent.atomic.AtomicInteger
object Context { const val ACTIVITY_SERVICE = "activity" }
class Activity { fun getSystemService(s: String): Any = android.app.ActivityManager() }
object SystemClock { fun elapsedRealtime() = 10000L }
class Principal { fun post(f: () -> Unit) = f(); fun postDelayed(f: () -> Unit, n: Long) = f() }
class CargaPlayer(val min: Int, val max: Int, val bytes: Int, val preparing: Boolean)
class RecursosPlayer(val carga: CargaPlayer, val max: Int, val language: String)
object Replay {
    var kbpsDeclarado = 0
    var poucaRamAparelho = false
    val travaPreparo = Any()
    val pedidosPreparo = AtomicInteger()
    class ConsultaPreparo(val token: Int, val state: Int)
    var consultaPreparo = ConsultaPreparo(0, 0)
    val principal = Principal()
    val activity: Activity? = Activity()
    val liberacoesEmCurso = AtomicInteger()
    val player: Any? = null
    var preparo: Preparo? = null
    fun soltarPreparo() { preparo = null }
    fun preparoAtual(s: Preparo) = preparo === s
    fun publicarPreparo(s: Preparo, state: Int) { error("unexpected preparation failure") }
'''
# Keep implementation declarations private as in production.
harness = harness.replace('    var preparo: Preparo?', '    private var preparo: Preparo?').replace('    fun preparoAtual', '    private fun preparoAtual').replace('    fun publicarPreparo', '    private fun publicarPreparo')
harness += preparo + '\n' + queue + main + '\n' + create + '''
    fun checkBuffer(next: Boolean, previous: Int) {
        kbpsDeclarado = previous
        val method = Replay::class.java.declaredMethods.filter { it.name == if (next) "prepararProximo" else "preparar" }.maxBy { it.parameterCount }
        val args = mutableListOf<Any>("https://example.invalid/remux.mkv", "", 0, 0, 1, 0, 0, "")
        if (method.parameterCount == 9) args.add(52800)
        method.invoke(null, *args.toTypedArray())
        check(preparo!!.recursos!!.carga.bytes == 80 * 1024 * 1024) { "prepared remux retained previous bitrate $previous: ${preparo!!.recursos!!.carga.bytes}" }
        check(kbpsDeclarado == previous) { "preparation changed active playback bitrate" }
    }
}
fun main() {
    for (next in listOf(false, true)) for (previous in listOf(0, 2000)) Replay.checkBuffer(next, previous)
    println("prepared remux buffer: OK")
}
'''
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / 'Replay.kt').write_text(harness)
    (tmp / 'Android.kt').write_text('''package android.app
class ActivityManager {
 val isLowRamDevice = true
 val memoryClass = 192
 class MemoryInfo { var totalMem = 2L shl 30 }
 fun getMemoryInfo(m: MemoryInfo) {}
}
''')
    subprocess.run([java, '-cp', os.pathsep.join(map(str, jars)), 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-reflect', '-no-stdlib', '-nowarn', '-classpath', str(stdlib), '-d', str(tmp / 'classes'), str(tmp / 'Replay.kt'), str(tmp / 'Android.kt')], check=True)
    subprocess.run([java, '-Xmx512m', '-cp', os.pathsep.join([str(tmp / 'classes'), str(stdlib)]), 'ReplayKt'], check=True)
