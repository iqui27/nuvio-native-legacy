#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
# Execute the Activity's actual trim callback with offline Kotlin/Android stubs.
python3 - <<'PY'
import glob
import os
from pathlib import Path
import re
import subprocess
import tempfile

source = Path('android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt').read_text()
def method(name):
    start = source.index('override fun ' + name + '(')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

callback = method('onTrimMemory')
harness = '''
object SDLActivity {
    var low = 0
    fun nativeLowMemory() { low++ }
}
open class Base {
    open fun onTrimMemory(level: Int) {}
}
class Activity : Base() {
    var trimResumida = false
    var trims = 0
    var cap = 100
    private fun nativeTrimMemoria(nivel: Int) {
        trims++
        cap = if (nivel >= 60) 25 else if (nivel >= 20) 50 else 40
    }
    CALLBACK
}
fun main() {
    val a = Activity()
    a.trimResumida = true
    repeat(100) {
        for (level in listOf(20, 40, 60, 80)) a.onTrimMemory(level)
        check(a.cap == 100 && a.trims == 0 && SDLActivity.low == 0)
    }
    for (level in listOf(5, 10, 15)) {
        a.onTrimMemory(level)
        check(a.trims > 0 && a.cap < 100 && SDLActivity.low > 0)
    }
    a.trimResumida = false
    val before = a.trims
    for (level in listOf(20, 40, 60, 80)) a.onTrimMemory(level)
    check(a.trims == before + 4)
    a.trimResumida = true
    val resumed = a.trims
    repeat(100) { a.onTrimMemory(20) }
    check(a.trims == resumed)
    println("ok: repeated visible background trims ignored; real pressure and hidden trims forwarded")
}
'''.replace('CALLBACK', callback)
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
patterns = [
    'org.jetbrains.kotlin/kotlin-compiler-embeddable/2.0.21/*/*.jar',
    'org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar',
    'org.jetbrains.kotlin/kotlin-script-runtime/*/*/*.jar',
    'org.jetbrains.kotlin/kotlin-reflect/1.6.10/*/*.jar',
    'org.jetbrains.intellij.deps/trove4j/*/*/*.jar',
    'org.jetbrains/annotations/13.0/*/*.jar',
    'org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar',
]
jars = [p for pattern in patterns for p in glob.glob(str(cache / pattern))]
assert any('kotlin-compiler-embeddable' in p for p in jars), 'offline Kotlin compiler missing'
java_home = subprocess.check_output(['/usr/libexec/java_home', '-v', '17'], text=True).strip()
java = java_home + '/bin/java'
cp = os.pathsep.join(jars)
with tempfile.TemporaryDirectory(prefix='nuvio-trim-') as tmp:
    kt = Path(tmp) / 'Trim.kt'
    kt.write_text(harness)
    subprocess.run([java, '-cp', cp, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-classpath', cp,
                    '-d', tmp, str(kt)], check=True)
    subprocess.run([java, '-cp', tmp + os.pathsep + cp, 'TrimKt'], check=True)
# Lifecycle writes must precede super calls: callbacks can arrive during them.
for name, value in [('onResume', 'true'), ('onPause', 'false')]:
    body = method(name)
    assert re.search(r'trimResumida\s*=\s*' + value, body), name
    assert body.index('trimResumida') < body.index('super.'), name
PY
