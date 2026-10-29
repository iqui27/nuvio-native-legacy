"""Offline JVM regression using the real Media3 DataSpec and live data source."""
from pathlib import Path
import os
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parent.parent
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
jdk = next((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home'))
compiler = list(cache.glob('org.jetbrains.kotlin/*/2.0.21/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.intellij.deps/trove4j/*/*/*.jar'))
compiler += list(cache.glob('org.jetbrains/annotations/13.0/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar'))
stdlib = next(cache.glob('org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar'))
sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Library/Android/sdk'))) / 'platforms/android-35/android.jar'
kt = root / 'android/app/src/main/java/space/nuvio/nativelegacy'
test = root / 'android/app/src/test/java'
with tempfile.TemporaryDirectory(prefix='nuvio-live-ts-') as directory:
    work = Path(directory)
    deps = [stdlib, sdk]
    for name in ('media3-common', 'media3-datasource'):
        aar = next(cache.glob(f'androidx.media3/{name}/1.8.0/*/*.aar'))
        jar = work / (name + '.jar')
        with zipfile.ZipFile(aar) as archive:
            jar.write_bytes(archive.read('classes.jar'))
        deps.append(jar)
    for pattern in ('junit/junit/4.13.2/*/*.jar', 'org.hamcrest/hamcrest-core/1.3/*/*.jar',
                    'com.google.guava/guava/*/*/*.jar', 'androidx.annotation/annotation-jvm/*/*/*.jar'):
        deps.append(next(cache.glob(pattern)))
    aar = next(cache.glob('androidx.annotation/annotation-experimental/*/*/*.aar'))
    jar = work / 'annotation-experimental.jar'
    with zipfile.ZipFile(aar) as archive:
        jar.write_bytes(archive.read('classes.jar'))
    deps.append(jar)
    classes = work / 'classes'
    classes.mkdir()
    subprocess.run([str(jdk / 'bin/javac'), '-d', str(classes), str(test / 'android/net/Uri.java')], check=True)
    classpath = os.pathsep.join(map(str, [classes, *deps]))
    subprocess.run([str(jdk / 'bin/java'), '-cp', os.pathsep.join(map(str, compiler)),
                    'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-reflect', '-no-stdlib',
                    '-jvm-target', '17', '-classpath', classpath, '-d', str(classes),
                    str(kt / 'LivePlaylistDataSource.kt'),
                    str(test / 'space/nuvio/nativelegacy/LiveTsDataSourceTest.kt')], check=True)
    subprocess.run([str(jdk / 'bin/java'), '-cp', classpath, 'org.junit.runner.JUnitCore',
                    'space.nuvio.nativelegacy.LiveTsDataSourceTest'], check=True)
