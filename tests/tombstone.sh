#!/bin/bash
# Resumo do tombstone do Android (Tombstone.kt): um tombstone sintetico em
# protobuf passa pelo leitor e as linhas "[queda]" tem de sair com o sinal, o
# fio que caiu e a pilha. Usa o compilador Kotlin do cache do Gradle e um JDK 17
# (o Kotlin 2.0.21 nao roda no JDK 25); sem eles, pula.
#
#   bash tests/tombstone.sh
set -euo pipefail
cd "$(dirname "$0")/.."
R="$HOME/.gradle/caches/modules-2/files-2.1"
J="${JAVA17_HOME:-$(/usr/libexec/java_home -v 17 2>/dev/null || ls -d "$HOME"/.local/jdks/jdk-17*/Contents/Home 2>/dev/null | head -1)}"
STD=$(find "$R/org.jetbrains.kotlin/kotlin-stdlib/2.0.21" -name "kotlin-stdlib-2.0.21.jar" 2>/dev/null | head -1)
CP=$(find "$R/org.jetbrains.kotlin/kotlin-compiler-embeddable/2.0.21" "$R/org.jetbrains.kotlin/kotlin-stdlib/2.0.21" \
  "$R/org.jetbrains.kotlin/kotlin-script-runtime/2.0.21" "$R/org.jetbrains.kotlin/kotlin-daemon-embeddable/2.0.21" \
  "$R/org.jetbrains.intellij.deps" "$R/org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm" "$R/org.jetbrains/annotations" \
  -name "*.jar" ! -name "*sources*" 2>/dev/null | tr '\n' ':')
if [ -z "$J" ] || [ ! -x "$J/bin/java" ] || [ -z "$STD" ] || [ -z "$CP" ]; then
  echo "tombstone: pulado (sem JDK 17 ou sem o Kotlin 2.0.21 no cache do Gradle)"; exit 0
fi
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tombstone.XXXXXX")
trap 'rm -rf "$D"' EXIT
# So o leitor: doExitInfo usa android.app, que nao existe na JVM.
python3 - "$D" <<'PY'
import sys
d=sys.argv[1]
src=open('android/app/src/main/java/space/nuvio/nativelegacy/Tombstone.kt').read()
src=src[:src.index('    // O ApplicationExitInfo de um crash nativo')]+'}\n'
open(d+'/Tombstone.kt','w').write(src)
def vi(n):
  o=b''
  while True:
    b=n&0x7f; n>>=7
    if n: o+=bytes([b|0x80])
    else: return o+bytes([b])
def f(num,v):
  if isinstance(v,int): return vi(num<<3)+vi(v)
  if isinstance(v,str): v=v.encode()
  return vi(num<<3|2)+vi(len(v))+v
sig=f(1,11)+f(2,'SIGSEGV')+f(3,1)+f(4,'SEGV_MAPERR')+f(8,1)+f(9,0xdeadbeef)
q1=f(1,0x1234)+f(2,0x7f001234)+f(4,'glTexSubImage2D')+f(5,16)+f(6,'/vendor/lib64/egl/libGLES_mali.so')
q2=f(1,0xabc)+f(6,'/data/app/x/lib/arm64/libmain.so')
outro=f(1,100)+f(2,'main')+f(4,f(1,1)+f(6,'/system/lib64/libc.so'))
fio=f(1,4242)+f(2,'SDLThread')+f(4,q1)+f(4,q2)
t=(f(1,1)+f(2,'fp')+f(5,100)+f(6,4242)+f(10,sig)+f(15,f(1,'null pointer dereference'))
   +f(16,f(1,100)+f(2,outro))+f(16,f(1,4242)+f(2,fio))+f(17,f(1,5))+f(20,451))
open(d+'/t.pb','wb').write(t)
open(d+'/meio.pb','wb').write(t[:len(t)//2])
PY
cat > "$D/Main.kt" <<'KT'
import space.nuvio.nativelegacy.Tombstone
fun main(a: Array<String>) { for (f in a) { println("== $f"); Tombstone.resumo(java.io.File(f).readBytes()).forEach(::println) } }
KT
"$J/bin/java" -cp "$CP" org.jetbrains.kotlin.cli.jvm.K2JVMCompiler -no-stdlib -no-reflect -jdk-home "$J" \
  -cp "$STD" "$D/Tombstone.kt" "$D/Main.kt" -d "$D/out" 2>&1 | grep -v '^warning' || true
saida=$("$J/bin/java" -cp "$D/out:$STD" MainKt "$D/t.pb" "$D/meio.pb" /dev/null)
echo "$saida"
falha=0
confere() { grep -qF -- "$1" <<<"$saida" || { echo "FALTOU: $1"; falha=1; }; }
confere '[queda] tombstone: sinal SIGSEGV (SEGV_MAPERR) endereco 0xdeadbeef aberto ha 451 s'
confere '[queda] causa: null pointer dereference'
confere '[queda] fio "SDLThread" tid=4242'
confere '[queda] #00 libGLES_mali.so+0x1234 (glTexSubImage2D+16)'
confere '[queda] #01 libmain.so+0xabc'
confere '(nao e o fio que caiu: tombstone sem ele)'
confere '[queda] tombstone sem a pilha do fio que caiu'
[ $falha = 0 ] && echo "tombstone: ok"
exit $falha
