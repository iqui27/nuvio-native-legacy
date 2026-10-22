#!/usr/bin/env python3
"""KM7 SE (Reddit, 2.0.3): em decoder Amlogic a recriacao da superficie no HDR
nao acontece. Compila RecriaHdr.kt de verdade e confere cada familia."""
import os, subprocess, tempfile
from pathlib import Path
raiz = Path(__file__).resolve().parent.parent
fonte = raiz / 'android/app/src/main/java/space/nuvio/nativelegacy/RecriaHdr.kt'
teste = '''
import space.nuvio.nativelegacy.RecriaHdr
fun caso(nome: String, dec: List<String>, quer: String?) {
    val r = RecriaHdr.dispensa(dec)
    if (r != quer) { println("FAIL $nome: $r (quer $quer)"); falhas++ } else println("ok   $nome")
}
var falhas = 0
fun main() {
    caso("KM7 SE (OMX.amlogic)", listOf("OMX.amlogic.vp9.decoder.awesome2", "OMX.amlogic.hevc.decoder.awesome2"), "Amlogic")
    caso("Amlogic Codec2 (c2.amlogic)", listOf("c2.amlogic.hevc.decoder"), "Amlogic")
    caso("MStar (OMX.MS)", listOf("OMX.MS.HEVC.Decoder"), "MStar")
    caso("TCL do dono (c2.mtk) recria", listOf("c2.mtk.hevc.decoder", "c2.mtk.dvhe.stn.decoder"), null)
    caso("decoder do Google so nao dispensa", listOf("c2.android.avc.decoder"), null)
    if (falhas > 0) { println("FAIL KM7: $falhas caso(s)"); kotlin.system.exitProcess(1) }
    println("PASS KM7: Amlogic e MStar nao recriam a superficie no HDR")
}
'''
cache = Path.home() / '.gradle/caches/modules-2/files-2.1'
def jar(g, a, v):
    m = list((cache / g / a / v).glob('*/*.jar'))
    if not m: raise SystemExit(f'Missing cached {a}:{v}; run Android Gradle build first')
    return str(m[0])
stdlib = jar('org.jetbrains.kotlin', 'kotlin-stdlib', '2.0.21')
cp = ':'.join([jar('org.jetbrains.kotlin', 'kotlin-compiler-embeddable', '2.0.21'), stdlib,
               jar('org.jetbrains.kotlin', 'kotlin-script-runtime', '2.0.21'),
               jar('org.jetbrains.intellij.deps', 'trove4j', '1.0.20200330'),
               jar('org.jetbrains.kotlinx', 'kotlinx-coroutines-core-jvm', '1.6.4'),
               jar('org.jetbrains', 'annotations', '13.0')])
jh = os.environ.get('JAVA_HOME')
if not jh:
    hs = sorted((Path.home() / '.local/jdks').glob('jdk-17*/Contents/Home'))
    jh = str(hs[0]) if hs else None
java = str(Path(jh) / 'bin/java') if jh else 'java'
with tempfile.TemporaryDirectory(prefix='nuvio-recria-hdr-') as w:
    w = Path(w); (w / 'Teste.kt').write_text(teste)
    subprocess.run([java, '-cp', cp, 'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-no-stdlib', '-no-reflect',
                    '-classpath', stdlib, '-jvm-target', '17', '-d', str(w / 'c'), str(fonte), str(w / 'Teste.kt')], check=True)
    r = subprocess.run([java, '-cp', f"{w / 'c'}:{stdlib}", 'TesteKt'])
    raise SystemExit(r.returncode)
