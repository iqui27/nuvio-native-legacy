#!/usr/bin/env python3
"""Run the real Kotlin export helper against small, offline Android test doubles."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
gradle = next((Path.home() / '.gradle/wrapper/dists/gradle-8.9-bin').glob('*/gradle-8.9/lib'))
stdlib = gradle / 'kotlin-stdlib-1.9.23.jar'
java_home = subprocess.check_output(['/usr/libexec/java_home', '-v', '17'], text=True).strip()
java = str(Path(java_home) / 'bin/java')
with tempfile.TemporaryDirectory(prefix='registro-download-') as temporary:
    work = Path(temporary)
    sources = {
        'Bundle.kt': '''package android.os
class Bundle {
    private val values = mutableMapOf<String, Any>()
    fun putString(key: String, value: String) { values[key] = value }
    fun getString(key: String): String? = values[key] as? String
    fun putBundle(key: String, value: Bundle) { values[key] = value }
    fun getBundle(key: String): Bundle? = values[key] as? Bundle
}
''',
        'Intent.kt': '''package android.content
import java.io.File
class Intent(val action: String = "") {
    var data: File? = null
    var type: String? = null
    fun addCategory(value: String) {}
    fun putExtra(key: String, value: String) {}
    companion object {
        const val ACTION_CREATE_DOCUMENT = "create"
        const val CATEGORY_OPENABLE = "openable"
        const val EXTRA_TITLE = "title"
    }
}
class ContentResolver {
    fun openOutputStream(file: File, mode: String) = file.outputStream()
}
''',
        'Activity.kt': '''package android.app
import android.content.ContentResolver
import android.content.Intent
import java.util.concurrent.LinkedBlockingQueue
class Activity {
    val isFinishing = false
    val isDestroyed = false
    val contentResolver = ContentResolver()
    val callbacks = LinkedBlockingQueue<Runnable>()
    fun startActivityForResult(intent: Intent, code: Int) {}
    fun runOnUiThread(action: Runnable) { callbacks.add(action) }
    companion object { const val RESULT_OK = -1 }
}
''',
        'Toast.kt': '''package android.widget
import android.app.Activity
class Toast(private val message: String) {
    fun show() { last = message }
    companion object {
        const val LENGTH_LONG = 1
        var last = ""
        fun makeText(activity: Activity, message: String, duration: Int) = Toast(message)
    }
}
''',
        'Test.kt': '''import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import space.nuvio.nativelegacy.RegistroDownload
import java.io.File
import java.util.concurrent.TimeUnit

// Optional reflection lets the original helper reach the behavioral assertion.
fun state(helper: RegistroDownload, method: String, bundle: Bundle) {
    helper.javaClass.methods.find { it.name == method }?.invoke(helper, bundle)
}
fun main(args: Array<String>) {
    val dir = File(args[0])
    val source = File(dir, "session.log").apply {
        writeText("session line\\npassword=private\\nhttps://example.org/secret\\n")
    }
    fun pending(): Bundle {
        val original = RegistroDownload(Activity())
        original.abrir(source.path, "saved", "failed")
        val saved = Bundle()
        state(original, "salvar", saved)
        original.fechar()
        return saved
    }
    val saved = pending()
    val activity = Activity()
    val restored = RegistroDownload(activity)
    state(restored, "restaurar", saved)
    check(restored.escolhendoArquivo) { "Picker request lost after process recreation" }
    check(!restored.resultado(123, Activity.RESULT_OK, null))
    val output = File(dir, "export.txt").apply { writeText("") }
    check(restored.resultado(RegistroDownload.PEDIDO, Activity.RESULT_OK,
        Intent().apply { data = output }))
    check(!restored.escolhendoArquivo)
    activity.callbacks.poll(5, TimeUnit.SECONDS)?.run() ?: error("Export never finished")
    check(output.readText() == RegistroDownload.limpar(source.readText()))
    check(Toast.last == "saved")
    val finished = Bundle()
    state(restored, "salvar", finished)
    val idle = RegistroDownload(Activity())
    state(idle, "restaurar", finished)
    check(!idle.escolhendoArquivo) { "Completed export restored as pending" }
    val cancelled = RegistroDownload(Activity())
    state(cancelled, "restaurar", pending())
    check(cancelled.resultado(RegistroDownload.PEDIDO, 0, null))
    check(!cancelled.escolhendoArquivo)
    val cancelledState = Bundle()
    state(cancelled, "salvar", cancelledState)
    state(idle, "restaurar", cancelledState)
    check(!idle.escolhendoArquivo) { "Cancelled export restored as pending" }
    val failureActivity = Activity()
    val failure = RegistroDownload(failureActivity)
    state(failure, "restaurar", pending())
    source.delete()
    failure.resultado(RegistroDownload.PEDIDO, Activity.RESULT_OK, Intent().apply { data = output })
    failureActivity.callbacks.poll(5, TimeUnit.SECONDS)?.run() ?: error("Failure never reported")
    check(Toast.last == "failed")
    println("registro download: recreation, export, redaction, cancellation, failure OK")
}
''',
    }
    for name, content in sources.items():
        (work / name).write_text(content)
    jar = work / 'test.jar'
    subprocess.run([
        java, '-cp', str(gradle / '*'), 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
        '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', str(stdlib),
        '-d', str(jar), *map(str, work.glob('*.kt')),
        str(root / 'android/app/src/main/java/space/nuvio/nativelegacy/RegistroDownload.kt'),
    ], check=True)
    subprocess.run([java, '-cp', os.pathsep.join(map(str, [jar, stdlib])),
                    'TestKt', str(work)], check=True)

activity = (root / 'android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt').read_text()
create = activity.split('override fun onCreate(savedInstanceState: Bundle?) {', 1)[1]
assert create.index('registroDownload.restaurar(savedInstanceState)') < create.index('super.onCreate(savedInstanceState)')
save = activity.split('override fun onSaveInstanceState(outState: Bundle) {', 1)[1].split('\n    }', 1)[0]
assert 'registroDownload.salvar(outState)' in save
assert 'super.onSaveInstanceState(outState)' in save
print('registro download: Activity saved-state wiring OK')
