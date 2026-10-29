"""Run the production APK retention policy on the JVM, without Android."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path('android/app/src/main/java/space/nuvio/nativelegacy')
activity = (root / 'NuvioActivity.kt').read_text()
assert 'fun prepararApk(caminho: String): Boolean' in activity, 'Missing pre-download cleanup'
assert 'ApkCache.prune(File(regenerable, "atualizacao")' in activity, 'Missing startup cleanup'
assert 'android_preparar_apk(arq)' in Path('src/atualizacao.c').read_text()
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
def jar(group, artifact, version):
    return str(next((cache / group / artifact / version).glob('*/*.jar')))
stdlib = jar('org.jetbrains.kotlin', 'kotlin-stdlib', '2.0.21')
cp = ':'.join([stdlib, jar('org.jetbrains.kotlin', 'kotlin-compiler-embeddable', '2.0.21'),
    jar('org.jetbrains.kotlin', 'kotlin-script-runtime', '2.0.21'),
    jar('org.jetbrains.intellij.deps', 'trove4j', '1.0.20200330'),
    jar('org.jetbrains.kotlinx', 'kotlinx-coroutines-core-jvm', '1.6.4'),
    jar('org.jetbrains', 'annotations', '13.0')])
homes = sorted((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home'))
java_home = os.environ.get('JAVA17_HOME') or (str(homes[0]) if homes else None)
java = str(Path(java_home) / 'bin/java') if java_home else 'java'
with tempfile.TemporaryDirectory(prefix='nuvio-apk-') as work:
    work = Path(work)
    fixture = work / 'Main.kt'
    fixture.write_text('''
import java.io.File
import space.nuvio.nativelegacy.ApkCache
fun main(args: Array<String>) {
    val dir = File(args[0], "updates").apply { mkdirs() }
    fun apk(name: String) = File(dir, name).apply { writeText("apk") }
    val old = apk("old.apk")
    val pending = apk("pending.apk")
    val partial = apk("partial.apk")
    val note = File(dir, "note.txt").apply { writeText("keep") }
    val versions = mapOf("old.apk" to 20600L, "pending.apk" to 20700L)
    val version: (File) -> Long? = { versions[it.name] }
    ApkCache.prune(dir, 20600, null, version)
    check(!old.exists() && !partial.exists() && pending.exists() && note.exists())
    // A replacement download supersedes the retained pending package.
    ApkCache.prune(dir, 20600, "next.apk", version)
    check(!pending.exists())
    val next = apk("next.apk")
    ApkCache.prune(dir, 20600, next.name, version)
    check(next.exists()) // Incomplete active download is protected.
    val nextVersion: (File) -> Long? = { 20800L }
    ApkCache.prune(dir, 20700, null, nextVersion)
    check(next.exists()) // Restart before installation must retain it.
    ApkCache.prune(dir, 20800, null, nextVersion)
    check(!next.exists()) // Successful install, including equal versionCode.
    val stale = apk("stale.apk")
    ApkCache.prune(dir, 20900, null, nextVersion)
    check(!stale.exists() && note.exists())
    println("APK startup/download/install retention PASS")
}
''')
    target = work / 'classes'
    subprocess.run([java, '-cp', cp, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
        '-no-stdlib', '-no-reflect', '-nowarn', '-classpath', stdlib,
        '-jvm-target', '17', '-d', str(target), str(root / 'ApkCache.kt'), str(fixture)], check=True)
    subprocess.run([java, '-cp', f'{target}:{stdlib}', 'MainKt', str(work)], check=True)
