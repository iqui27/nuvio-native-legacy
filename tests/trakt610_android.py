#!/usr/bin/env python3
"""Run the real Kotlin HTTP method, JNI decoder and Android POST fallback offline."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
os.chdir(root)
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
java_home = Path('/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home')

def jar(group, name, version):
    return next((cache / group / name / version).glob('*/*.jar'))

compiler = [jar(*p) for p in [
    ('org.jetbrains.kotlin', 'kotlin-compiler-embeddable', '2.0.21'),
    ('org.jetbrains.kotlin', 'kotlin-stdlib', '2.0.21'),
    ('org.jetbrains.kotlin', 'kotlin-script-runtime', '2.0.21'),
    ('org.jetbrains.kotlin', 'kotlin-reflect', '1.6.10'),
    ('org.jetbrains.intellij.deps', 'trove4j', '1.0.20200330'),
    ('org.jetbrains.kotlinx', 'kotlinx-coroutines-core-jvm', '1.6.4'),
    ('org.jetbrains', 'annotations', '13.0'),
]]

def function(source, start):
    begin = source.index(start)
    brace = source.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]

method = function(Path('android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt').read_text(), '    fun httpPedir(')
bridge = function(Path('src/android.c').read_text(), 'char *android_http(')
with tempfile.TemporaryDirectory(prefix='trakt610-android-') as tmp:
    d = Path(tmp)
    (d / 'Bridge.kt').write_text('''
class Bridge {
    companion object {
        @JvmStatic fun setup() {
            java.net.URL.setURLStreamHandlerFactory { _ -> object : java.net.URLStreamHandler() {
                override fun openConnection(u: java.net.URL) = object : java.net.HttpURLConnection(u) {
                    override fun connect() {}
                    override fun disconnect() {}
                    override fun usingProxy() = false
                    override fun getResponseCode() = 429
                    override fun getHeaderField(n: String?): String? = if (n.equals("Retry-After", true)) when(u.path) {
                        "/seconds" -> "120"
                        "/invalid" -> "nonsense"
                        "/date" -> java.time.ZonedDateTime.now(java.time.ZoneOffset.UTC).plusSeconds(120).format(java.time.format.DateTimeFormatter.RFC_1123_DATE_TIME)
                        else -> null
                    } else null
                    override fun getErrorStream() = "limited".byteInputStream()
                    override fun getOutputStream() = java.io.ByteArrayOutputStream()
                }
            } }
        }
    }
''' + method + '\n}\n')
    subprocess.run(['/Library/Java/JavaVirtualMachines/temurin-17.jre/Contents/Home/bin/java', '-cp', os.pathsep.join(map(str, compiler)),
                    'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-stdlib', '-no-reflect',
                    '-classpath', str(compiler[1]), '-d', str(d), str(d / 'Bridge.kt')], check=True)
    (d / 'bridge.c').write_text('''
#define NV_ANDROID 1
#include <assert.h>
#include <jni.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static JNIEnv *envTest;
static jobject actTest;
static void *SDL_AndroidGetJNIEnv(void) { return envTest; }
static void *SDL_AndroidGetActivity(void) { return (*envTest)->NewLocalRef(envTest, actTest); }
static jmethodID metodo(JNIEnv *e, jobject a, jclass *c, const char *n, const char *s) {
  *c = (*e)->GetObjectClass(e, a); return (*e)->GetMethodID(e, *c, n, s);
}
''' + bridge + '''
#include "src/rede.c"
void android_etapa(const char *s) { (void)s; }
static void *init(void) { return (void *)1; }
static int option(void *c, int o, ...) { (void)c; (void)o; return 0; }
static int perform(void *c) { (void)c; return 7; }
static void cleanup(void *c) { (void)c; }
int main(int argc, char **argv) {
  (void)argc;
  JavaVM *vm;
  JavaVMOption opt = { argv[1], NULL };
  JavaVMInitArgs args = { JNI_VERSION_1_8, 1, &opt, JNI_FALSE };
  assert(JNI_CreateJavaVM(&vm, (void **)&envTest, &args) == JNI_OK);
  jclass cls = (*envTest)->FindClass(envTest, "Bridge"); assert(cls);
  (*envTest)->CallStaticVoidMethod(envTest, cls, (*envTest)->GetStaticMethodID(envTest, cls, "setup", "()V"));
  actTest = (*envTest)->NewObject(envTest, cls, (*envTest)->GetMethodID(envTest, cls, "<init>", "()V"));
  curl_init = init; curl_setopt_f = option; curl_perform = perform; curl_cleanup = cleanup;
  pronto = 1;
  const char *paths[] = { "seconds", "date", "invalid", "absent" };
  for (int route = 0; route < 2; route++) for (int i = 0; i < 4; i++) {
    curlRuim = route == 0;
    int st = 0, retry = -1;
    char url[128]; snprintf(url, sizeof url, "https://example.test/%s", paths[i]);
    char *body = rede_postar_st_retry(url, 1, NULL, "{}", &st, &retry);
    assert(body && !strcmp(body, "limited") && st == 429);
    if (i == 0) assert(retry == 120);
    else if (i == 1) assert(retry >= 118 && retry <= 120);
    else assert(retry == 0);
    free(body);
  }
  (*vm)->DestroyJavaVM(vm);
  puts("#610 Android: PASS (Kotlin headers, JNI packet, direct and curl-error fallback)");
}
''')
    subprocess.run(['cc', '-O1', '-g', '-I.', '-Isrc', '-I' + str(java_home / 'include'),
                    '-I' + str(java_home / 'include/darwin'), str(d / 'bridge.c'), 'src/redeurl.c',
                    '-L' + str(java_home / 'lib/server'), '-Wl,-rpath,' + str(java_home / 'lib/server'),
                    '-ljvm', '-lpthread', '-o', str(d / 'bridge')], check=True)
    subprocess.run([str(d / 'bridge'), '-Djava.class.path=' + str(d) + os.pathsep + str(compiler[1])], check=True)
