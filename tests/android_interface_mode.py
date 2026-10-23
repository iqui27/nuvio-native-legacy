#!/usr/bin/env python3
"""Exercise the production Android device policy and saved mode on the JVM.

Uses the same cached Kotlin compiler as android_window_layout.py. Run an
Android Gradle build first; no emulator, Android doubles or source extraction.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = r'''
package space.nuvio.nativelegacy
import java.io.File
fun main(args: Array<String>) {
    val p = InterfaceMode
    for (ui in listOf(false, true)) for (tv in listOf(false, true))
        for (leanback in listOf(false, true))
            check(p.automaticMobile(ui, tv, leanback) == !(ui || tv || leanback))
    for (automatic in listOf(false, true)) {
        check(!p.resolve(p.TV, automatic))
        check(p.resolve(p.MOBILE, automatic))
        for (choice in listOf(p.AUTO, -1, 99)) check(p.resolve(choice, automatic) == automatic)
    }
    for (player in listOf(false, true))
        check(p.orientation(false, player) == InterfaceMode.Orientation.LANDSCAPE)
    check(p.orientation(true, false) == InterfaceMode.Orientation.FULL_USER)
    check(p.orientation(true, true) == InterfaceMode.Orientation.SENSOR_LANDSCAPE)
    check(p.viewport(false, 1080, 2340) == null)
    check(p.viewport(true, 1080, 2340) == TouchViewport.Bounds(0, 0, 1080, 2340))
    check(p.viewport(true, 2340, 1080) == TouchViewport.Bounds(0, 0, 2340, 1080))
    check(p.viewport(true, 0, 1080) == null)

    fun saved(vararg lines: String) = p.preference(lines.asSequence())
    check(saved() == p.AUTO)
    check(saved("unrelated 1", "interfaceModoLocal invalid") == p.AUTO)
    check(saved("interfaceModoLocal 0") == p.TV)
    check(saved("  interfaceModoLocal\t1\r") == p.MOBILE)
    check(saved("interfaceModoLocal 1", "interfaceModoLocal 2") == p.AUTO)
    check(saved("interfaceModoLocal 1", "interfaceModoLocal -1",
                "interfaceModoLocal 3", "interfaceModoLocal 999999999999999999999") == p.MOBILE)
    val prefix = "interfaceModoLocal 1"
    check(saved(prefix + " ".repeat(94 - prefix.length)) == p.MOBILE)
    check(saved(prefix + " ".repeat(95 - prefix.length)) == p.AUTO)
    // Match the native byte limit, including multibyte text and CRLF.
    check(saved(prefix + "é".repeat(37)) == p.MOBILE)
    check(saved(prefix + "é".repeat(38)) == p.AUTO)
    check(saved(prefix + " ".repeat(94 - prefix.length) + "\r") == p.MOBILE)
    check(saved("interfaceModoLocal 0", prefix + " ".repeat(96),
                "interfaceModoLocal 2") == p.AUTO)
    val file = File(args[0], "ajustes.txt")
    check(p.readPreference(file) == p.AUTO)
    for (choice in listOf(p.TV, p.MOBILE, p.AUTO)) {
        file.writeText("other 10\ninterfaceModoLocal $choice\n")
        check(p.readPreference(file) == choice)
    }
    println("android_interface_mode: PASS (TV detection, saved choice, Auto, orientation, viewport)")
}
'''

cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
def jar(group, artifact, version):
    matches = list((cache / group / artifact / version).glob('*/*.jar'))
    if not matches:
        raise SystemExit(f'Missing cached {artifact}:{version}; run Android Gradle build first')
    return str(matches[0])

stdlib = jar('org.jetbrains.kotlin', 'kotlin-stdlib', '2.0.21')
classpath = os.pathsep.join([
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
production = root / 'android/app/src/main/java/space/nuvio/nativelegacy'
with tempfile.TemporaryDirectory(prefix='nuvio-interface-mode-') as work:
    work = Path(work)
    source = work / 'InterfaceFixture.kt'
    source.write_text(fixture)
    target = work / 'classes'
    subprocess.run([java, '-cp', classpath, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler',
                    '-no-stdlib', '-no-reflect', '-classpath', stdlib,
                    '-jvm-target', '17', '-d', str(target), str(source),
                    str(production / 'InterfaceMode.kt'), str(production / 'TouchViewport.kt')], check=True)
    subprocess.run([java, '-cp', os.pathsep.join([str(target), stdlib]),
                    'space.nuvio.nativelegacy.InterfaceFixtureKt', str(work)], check=True)
