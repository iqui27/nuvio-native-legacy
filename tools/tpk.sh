#!/bin/bash
# Nuvio como .tpk para Samsung Tizen 6+ (TVs 2021 em diante).
#
#   bash tools/tpk.sh          # compila libnuvio.so e gera o .tpk em build/tpk/
#
# O C do Nuvio vira libnuvio.so (src/*.c src/dts/*.c + src/tpk.c, -DNV_TPK) e um host .NET
# (tizen-tpk/NuvioTpk) abre o GLWindow e a chama a cada quadro. Por que assim, e
# nao um executavel C: ver src/tpk.h. O spike que provou cada peca (.so propria,
# fios, GLWindow) esta em tizen-tpk-spike/.
#
# Tizen 4/5 (2018-2020): pacote NuvioTpk40, com uma libnuvio.so PROPRIA
# (-DNV_TPK40). La a UEP barra o dlopen de arquivo e o host carrega a .so por
# memfd ou por um carregador de ELF proprio (tizen-tpk/NuvioTpk40/Program40.cs),
# que nao monta TLS de compilador e resolve simbolos por DT_HASH. Por isso essa
# .so e compilada a parte, sem _Thread_local (src/rede.c) e com
# --hash-style=both, e o script FALHA se ela sair com PT_TLS, relocacao de TLS
# ou sem DT_HASH (#137, #180). A .so dos hosts Tizen 6+ e a de sempre.
#
# NIVEL DE GPU FORCADO (canario de comparacao, src/gpunivel.h):
#
#   NV_TPK_NIVEL=1 bash tools/tpk.sh   # efeitos leves, 1080p nativo
#   NV_TPK_NIVEL=2 bash tools/tpk.sh   # efeitos leves + desenho interno 720p
#
# vira -DNV_TPK_NIVEL_FORCADO=N nas duas .so (4/5 e 6+) e, alem de build/tpk/,
# os .tpk sao copiados para build/tpk/canary-gpu-<efeitos|720p>/ com o sufixo
# -canary-gpu-<efeitos|720p>. Sem a variavel e o caminho da release: nivel
# ADAPTATIVO (mede a home e desce sozinho so se a GPU nao der conta).
#
# Primeira vez: dependencias estaticas (SDL2 com video dummy, SDL2_image com
# stb, SDL2_ttf com freetype embutido) em ~/.cache/nuvio-tpk/prefix, feitas por
# tools/tpk/deps.sh na imagem tools/tpk/Dockerfile.
set -euo pipefail
cd "$(dirname "$0")/.."
RAIZ="$PWD"
CACHE="${NUVIO_TPK_CACHE:-$HOME/.cache/nuvio-tpk}"
SAIDA="build/tpk"
mkdir -p "$SAIDA" "$CACHE"
# WHICH PACKAGES TO BUILD. Default is all four, as it always was. `NV_TPK_PACOTES`
# exists for the Tizen 4/5 target, which is the one being built right now:
#
#   NV_TPK_PACOTES=NuvioTpk40 bash tools/tpk.sh
#
# Two reasons, and the second is what forces the variable to exist. (1) time:
# each package copies the art and runs a dotnet build. (2) NuvioTpk (API11) uses
# the `net6.0-tizen8.0` TFM, which needs the Samsung WORKLOAD — and that workload
# only accepts Tizen >= 8.0 (see tizen-tpk-spike/README.md). Without the
# workload, that loop iteration dies under `set -e` and takes the WHOLE script
# down, even though the three `tizenNN` packages were already built and their
# .tpk are on disk. So without this variable there is no way to package the 40 on
# a bench that does not have the workload. The three `tizenNN` (40/80/90) use the
# old SDK through NuGet (Tizen.NET.Sdk) and need NO workload at all.
PACOTES="${NV_TPK_PACOTES:-NuvioTpk40 NuvioTpk60 NuvioTpk65 NuvioTpk}"

# Diagnostic label stays out of tracked manifests/appinfo. Tizen manifests
# require a numeric version; the app and filename retain the prerelease label.
ASPECTO_FLAG=""
case "${NV_ASPECTO_DIAG:-0}" in
  0|"") ;;
  1) NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_ASPECTO_DIAG"
     ASPECTO_FLAG="-p:NvAspectoDiag=1" ;;
  *) echo "NV_ASPECTO_DIAG: use 0 or 1" >&2; exit 1 ;;
esac

CANARIO=""
case "${NV_TPK_NIVEL:-}" in
  "") ;;
  1) CANARIO=canary-gpu-efeitos ;;
  2) CANARIO=canary-gpu-720p ;;
  *) echo "NV_TPK_NIVEL=$NV_TPK_NIVEL: use 1 (efeitos leves) ou 2 (720p)" >&2; exit 1 ;;
esac
if [ -n "$CANARIO" ]; then
  NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_TPK_NIVEL_FORCADO=$NV_TPK_NIVEL"
  echo "[canario] nivel de GPU forcado em $NV_TPK_NIVEL ($CANARIO)"
fi

docker info >/dev/null 2>&1 || { echo "Docker parado: abra o Docker/OrbStack" >&2; exit 1; }
docker image inspect nuvio-tpk-sdk >/dev/null 2>&1 ||
  docker build --platform linux/arm/v5 -t nuvio-tpk-sdk tools/tpk/

if [ ! -f "$CACHE/prefix/lib/libSDL2.a" ] || [ ! -f "$CACHE/prefix/lib/libSDL2_ttf.a" ] ||
   [ ! -f "$CACHE/prefix/lib/libwebp.a" ] || [ ! -f "$CACHE/prefix/lib/.sdlimage-webp" ] ||
   [ ! -f "$CACHE/prefix/lib/libass.a" ] || [ ! -f "$CACHE/prefix/lib/.ttf-freetype-externo" ]; then
  echo "[deps] SDL2/SDL2_image/SDL2_ttf estaticos (demora na primeira vez)"
  mkdir -p "$CACHE/src"
  for u in https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz \
           https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.2/SDL2_image-2.8.2.tar.gz \
           https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz \
           https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.4.0.tar.gz; do
    d="$CACHE/src/$(basename "$u" .tar.gz)"
    [ -d "$d" ] || curl -fsSL "$u" | tar xz -C "$CACHE/src"
  done
  # libass e as suas dependencias (mesmas versoes do webOS, tools/build-ass-arm.sh): .tar.xz
  for u in https://download-mirror.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz \
           https://github.com/fribidi/fribidi/releases/download/v1.0.16/fribidi-1.0.16.tar.xz \
           https://github.com/harfbuzz/harfbuzz/releases/download/10.4.0/harfbuzz-10.4.0.tar.xz \
           https://github.com/libass/libass/releases/download/0.17.5/libass-0.17.5.tar.xz; do
    d="$CACHE/src/$(basename "$u" .tar.xz)"
    [ -d "$d" ] || curl -fsSL "$u" | tar xJ -C "$CACHE/src"
  done
  cp tools/tpk/deps.sh "$CACHE/deps.sh"
  docker run --rm --platform linux/arm/v5 -v "$CACHE:/w" nuvio-tpk-sdk sh /w/deps.sh
fi

echo "[1/3] libnuvio.so (ARMv7 softfp, glibc <= 2.28)"
ENVF=$(mktemp "${TMPDIR:-/tmp}/nuvio-tpk-env.XXXXXXXX"); trap 'rm -f "$ENVF"' EXIT
tools/env.sh --env-file "$ENVF"
if [ "${NV_ASPECTO_DIAG:-0}" = 1 ]; then
  # Fail before building a tester package that cannot log in/upload logs.
  tools/env.sh --require-core >/dev/null
  DIAG_VER="${NV_ASPECTO_DIAG_VERSION:-2.0.5-aspect.1}"
  [[ "$DIAG_VER" =~ ^[0-9]+\.[0-9]+\.[0-9]+-aspect\.[0-9]+$ ]] || {
    echo "NV_ASPECTO_DIAG_VERSION: expected x.y.z-aspect.n" >&2; exit 1; }
  sed -i.bak "s/^NV_VERSAO=.*/NV_VERSAO=$DIAG_VER/" "$ENVF"
  rm -f "$ENVF.bak"
fi
# Motor P2P (<raiz>/tpk de tools/p2p-motor/build-tpk.sh, achada por tools/p2p-motor/pasta.sh): liga o motor P2P
# embutido (src/p2pmotor.h) SO na libnuvio.so dos hosts 6+, por dlopen
# (-DNV_P2P_MOTOR_DLOPEN: sem NEEDED, a auto-atualizacao de uma libnuvio.so
# num pacote sem o motor continua abrindo), e poe a libnuvio_engine.so no
# lib/ desses pacotes. O NuvioTpk40 (Tizen 4/5) nunca leva motor.
. tools/p2p-motor/pasta.sh
nv_p2p_resolver tpk
P2P_VOL=""
[ -n "$NV_P2P_DIR" ] && P2P_VOL="-v $NV_P2P_DIR:/p2p"
docker run --rm --platform linux/arm/v5 --env-file "$ENVF" $P2P_VOL \
  -e NUVIO_P2P_MOTOR="${NV_P2P_DIR:+1}" \
  -e NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-}" -e NV_TPK_SEM_ASS="${NV_TPK_SEM_ASS:-}" \
  -v "$RAIZ":/work -v "$CACHE/prefix":/deps -w /work nuvio-tpk-sdk sh -c '
  set -e
  mkdir -p /tmp/o
  # ASS pelo libass (#ass-tpk): o mesmo backend do webOS (src/assrender.c). NV_TPK_SEM_ASS=1
  # volta ao texto simples (diagnostico). O libass e estatico, com FreeType/HarfBuzz/FriBidi.
  ASS_CFLAGS="-DNV_ASS_LIBASS"; ASS_LIBS="-lass -lharfbuzz -lfribidi -lfreetype"
  if [ "${NV_TPK_SEM_ASS:-}" = 1 ]; then ASS_CFLAGS=""; ASS_LIBS="-lfreetype"; fi
  # As -D vao num arquivo de resposta do gcc (@/tmp/flags): assim o xargs -P
  # abaixo compila em paralelo sem reabrir o problema de aspas das chaves.
  # A LISTA DE CHAVES E FECHADA, e nao "tudo o que o env.sh imprimir": o que
  # nao estiver aqui simplesmente nao chega ao compilador, e o sintoma e um
  # "undefined reference" no link (nao um erro no -D). Ao acrescentar uma chave
  # nova ao env.sh, acrescente-a aqui TAMBEM.
  for k in NV_SUPABASE_URL NV_SUPABASE_ANON_KEY NV_TV_LOGIN_BASE NV_TRAKT_CLIENT_ID \
           NV_TRAKT_CLIENT_SECRET NV_SIMKL_CLIENT_ID NV_SIMKL_APP NV_TMDB_API_KEY NV_SEEKR_API_KEY \
           NV_REC_URL NV_DISCORD_CLIENT_ID NV_VERSAO; do
    eval "v=\${$k:-}"
    # No arquivo de resposta o gcc tira aspas e barras como o shell: a aspa
    # da string C tem de ir escapada (-DX=\"valor\").
    v=$(printf "%s" "$v" | sed "s/[\\\\\"]/\\\\&/g")
    printf "%s\n" "-D$k=\\\"$v\\\""
  done > /tmp/flags
  # p2pmotor_motor.c usa dladdr (Dl_info), que o glibc so declara com _GNU_SOURCE, e o
  # -include src/tpk.h puxa os headers antes de qualquer #define do proprio arquivo:
  # o -D tem de ir na linha de comando, SO para esse arquivo.
  P2P_CFLAGS=""
  [ "${NUVIO_P2P_MOTOR:-}" = "1" ] && P2P_CFLAGS="-DNV_P2P_MOTOR -DNV_P2P_MOTOR_DLOPEN -I/p2p/include"
  ls src/*.c src/dts/*.c | grep -v "src/video_tizen.c" | xargs -P 6 -I{} sh -c \
    "gcc $CFLAGS -c {} -o /tmp/o/\$(basename {} .c).o -DNV_TPK $ASS_CFLAGS -include src/tpk.h -fvisibility=hidden -Wno-unused-result $NUVIO_EXTRA_CFLAGS $P2P_CFLAGS \$(case {} in src/p2pmotor_motor.c) echo -D_GNU_SOURCE;; src/p2pmotor.c) echo -D_FILE_OFFSET_BITS=64;; esac) @/tmp/flags -I/deps/include -I/deps/include/SDL2" 
  # SDL e zlib ESTATICOS: a TV nao tem libSDL2 garantida, e a libz entra junto
  # para nao depender da versao do aparelho. GLES/EGL/dl/pthread/m sao do
  # sistema (API nativa publica do Tizen). libwebp tambem estatica (o Tizen nao
  # a expoe a apps; o SDL_image decodifica WebP com ela). curl e libjpeg continuam
  # por dlopen em execucao, como na LG (rede.c, jpegrapido.c, webp.c).
  gcc -shared -o /work/build/tpk/libnuvio.so /tmp/o/*.o -Wl,--no-undefined \
    -Wl,-soname,libnuvio.so -Wl,--exclude-libs,ALL \
    -L/deps/lib -lSDL2_ttf -lSDL2_image -lwebpdemux -lwebp -lsharpyuv -lSDL2 $ASS_LIBS /usr/lib/arm-linux-gnueabi/libz.a \
    -lGLESv2 -ldl -lpthread -lm -lrt
  echo "  $(ls -la /work/build/tpk/libnuvio.so | awk "{print \$5}") bytes"
  objdump -T /work/build/tpk/libnuvio.so | grep -oE "GLIBC_[0-9.]+" | sort -uV | tail -1 | sed "s/^/  glibc minima: /"
  objdump -p /work/build/tpk/libnuvio.so | grep NEEDED
  objdump -T /work/build/tpk/libnuvio.so | grep -E " nv_tpk_" | awk "{print \"  exporta \" \$NF}"
  # Todo [DllImport("libnuvio.so")] do host tem de estar EXPORTADO (-fvisibility=hidden
  # esconde o que nao leva visibility("default"): EntryPointNotFoundException em
  # "apps-init" em 8 TVs da 1.7.4).
  for f in $(grep -rhoE "DllImport\(\"libnuvio.so\"\)\] static extern [a-z]+ nv_[a-z0-9_]+" tizen-tpk/*.cs tizen-tpk/NuvioTpk40/*.cs | awk "{print \$NF}" | sort -u); do
    objdump -T /work/build/tpk/libnuvio.so | grep -qE " $f\$" || { echo "libnuvio.so nao exporta $f (falta visibility default?)" >&2; exit 1; }
  done

  echo "[1b/3] libnuvio-tpk40.so (Tizen 4/5: -DNV_TPK40, sem TLS, com DT_HASH)"
  # So as unidades que citam NV_TPK40 sao recompiladas; as demais sao os MESMOS
  # objetos da .so de cima. O link ganha --hash-style=both para o carregador
  # de ELF do Program40.cs achar o DT_HASH (o padrao do gcc daqui e so GNU_HASH).
  mkdir -p /tmp/o40
  # p2pmotor_motor.c entra sempre aqui: o 4/5 NUNCA leva o motor (a UEP barra
  # .so de arquivo), entao ele e recompilado SEM -DNV_P2P_MOTOR (sem P2P_CFLAGS).
  { grep -l NV_TPK40 src/*.c src/dts/*.c; echo src/p2pmotor_motor.c; } | sort -u | grep -v "src/video_tizen.c" | xargs -P 6 -I{} sh -c \
    "gcc $CFLAGS -c {} -o /tmp/o40/\$(basename {} .c).o -DNV_TPK -DNV_TPK40 $ASS_CFLAGS -include src/tpk.h -fvisibility=hidden -Wno-unused-result $NUVIO_EXTRA_CFLAGS \$(case {} in src/p2pmotor.c) echo -D_FILE_OFFSET_BITS=64;; esac) @/tmp/flags -I/deps/include -I/deps/include/SDL2"
  OBJ40=""
  for o in /tmp/o/*.o; do
    b=$(basename "$o")
    if [ -f "/tmp/o40/$b" ]; then OBJ40="$OBJ40 /tmp/o40/$b"; else OBJ40="$OBJ40 $o"; fi
  done
  gcc -shared -o /work/build/tpk/libnuvio-tpk40.so $OBJ40 -Wl,--no-undefined \
    -Wl,-soname,libnuvio.so -Wl,--exclude-libs,ALL -Wl,--hash-style=both \
    -L/deps/lib -lSDL2_ttf -lSDL2_image -lwebpdemux -lwebp -lsharpyuv -lSDL2 $ASS_LIBS /usr/lib/arm-linux-gnueabi/libz.a \
    -lGLESv2 -ldl -lpthread -lm -lrt
  echo "  $(ls -la /work/build/tpk/libnuvio-tpk40.so | awk "{print \$5}") bytes"
  # CONFERENCIA que o carregador de ELF do host exige (ele recusa o contrario):
  L40=/work/build/tpk/libnuvio-tpk40.so
  if readelf -lW "$L40" | grep -qE "^\s*TLS\s"; then
    echo "libnuvio-tpk40.so tem segmento PT_TLS: sobrou _Thread_local em algum lugar" >&2; exit 1; fi
  if readelf -rW "$L40" | grep -qE "R_ARM_TLS_"; then
    echo "libnuvio-tpk40.so tem relocacao de TLS:" >&2; readelf -rW "$L40" | grep -E "R_ARM_TLS_" | head >&2; exit 1; fi
  if readelf -sW --dyn-syms "$L40" | grep -q "__tls_get_addr"; then
    echo "libnuvio-tpk40.so ainda chama __tls_get_addr" >&2; exit 1; fi
  if ! readelf -dW "$L40" | grep -qE "\(HASH\)"; then
    echo "libnuvio-tpk40.so sem DT_HASH (o link ignorou --hash-style=both?)" >&2; exit 1; fi
  echo "  sem PT_TLS, sem R_ARM_TLS_*, sem __tls_get_addr, com DT_HASH: ok"
  readelf -rW "$L40" | grep -oE "R_ARM_[A-Z0-9_]+" | sort | uniq -c | sed "s/^/  reloc /"
  objdump -T "$L40" | grep -E " nv_tpk" | awk "{print \"  exporta \" \$NF}"
  # Todo [DllImport("libnuvio.so")] do host tem de estar EXPORTADO (-fvisibility=hidden
  # esconde o que nao leva visibility("default"): EntryPointNotFoundException em
  # "apps-init" em 8 TVs da 1.7.4).
  for f in $(grep -rhoE "DllImport\(\"libnuvio.so\"\)\] static extern [a-z]+ nv_[a-z0-9_]+" tizen-tpk/*.cs tizen-tpk/NuvioTpk40/*.cs | awk "{print \$NF}" | sort -u); do
    objdump -T "$L40" | grep -qE " $f\$" || { echo "libnuvio-tpk40.so nao exporta $f (falta visibility default?)" >&2; exit 1; }
  done
'

echo "[2/3] host .NET + pacotes"
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}" PATH="$HOME/.dotnet:$PATH" DOTNET_CLI_TELEMETRY_OPTOUT=1
VER=$(sed -n 's/^NV_VERSAO=//p' "$ENVF")
MAN_VER="${VER%%-*}"
# A arte e a mesma reduzida do .wgt (tools/tizen-art.sh); fonts/ fica ao lado
# de art/, que e onde o main.c procura.
ARTE=$(bash tools/tizen-art.sh)
rm -f "$SAIDA"/*.tpk
for p in $PACOTES; do
  H=tizen-tpk/$p
  rm -rf "$H/lib" "$H/res" "$H/bin" "$H/obj"
  mkdir -p "$H/lib" "$H/res" "$H/shared/res"
  # O NuvioTpk40 leva a .so propria (1b/3); o nome dentro do pacote e o mesmo.
  if [ "$p" = NuvioTpk40 ]; then cp "$SAIDA/libnuvio-tpk40.so" "$H/lib/libnuvio.so"
  else
    cp "$SAIDA/libnuvio.so" "$H/lib/"
    # Motor P2P (6+): a .so ao lado da libnuvio.so, aberta por dlopen.
    [ -n "$NV_P2P_DIR" ] && cp "$NV_P2P_DIR/libnuvio_engine.so" "$H/lib/"
  fi
  cp -R "$ARTE" "$H/res/art"
  # Versao EMPACOTADA para a auto-atualizacao (tizen-tpk/Carga.cs): o host so
  # aplica uma libnuvio.so encenada em data/ se ela for mais nova que isto.
  printf '%s' "$VER" > "$H/res/versao.txt"
  cp -R deploy/app/fonts "$H/res/fonts"
  # Avisos de licenca de terceiros (libtorrent, Boost, OpenSSL, nuvio-engine).
  cp -R deploy/app/licencas "$H/res/licencas"
  # Clipe mudo do canario de audio (#137, Video.PrimeAudio); so o host 6+ usa.
  [ "$p" = NuvioTpk40 ] || cp tizen-tpk/silencio.mp4 "$H/res/"
  cp deploy/app/tizen/icon.png "$H/shared/res/$p.png"
  # The version AND the keyboard-canary privilege are rewritten here, per
  # package — but the SOURCE manifest is a tracked file, so it is saved and put
  # back. Without this every build left the tree dirty (and a later
  # `git commit -a` would commit the build's identity into the source).
  # `sed -i ''` is BSD syntax: on GNU sed the '' becomes the script itself and the
  # command dies with "cannot read ...: No such file". `-i.bak` works on both.
  MAN="$H/tizen-manifest.xml"
  cp "$MAN" "$MAN.orig"
  sed -i.bak "s/ version=\"[^\"]*\">/ version=\"$MAN_VER\">/" "$MAN"
  # CANARIO do teclado/ditado do sistema (tizen-tpk/Texto.cs, #imetv):
  # NUVIO_TPK_TEXTO=1 compila o Texto.cs e poe o privilegio do microfone
  # (recorder, para o Tizen.Uix.Stt) SO neste build — o manifesto do git nao muda,
  # porque e restaurado logo abaixo. Nao vai em release sem teste numa TV.
  FLAG_TEXTO=""
  if [ "${NUVIO_TPK_TEXTO:-}" = 1 ] && [ "$p" != NuvioTpk40 ]; then
    sed -i.bak 's|<privilege>http://tizen.org/privilege/internet</privilege>|&<privilege>http://tizen.org/privilege/recorder</privilege>|' "$MAN"
    FLAG_TEXTO="-p:NvTextoCanario=1"
  fi
  rm -f "$MAN.bak"
  # Restore on BOTH paths: a failed build must not leave the tracked manifest
  # wearing this build's identity.
  if ! dotnet build "$H/$p.csproj" -c Release -nologo -v q $FLAG_TEXTO $ASPECTO_FLAG; then
    mv "$MAN.orig" "$MAN"
    echo "$p: dotnet build falhou" >&2
    exit 1
  fi
  mv "$MAN.orig" "$MAN"
  TPK=$(find "$H/bin/Release" -name '*.tpk' | head -1)
  [ -n "$TPK" ] || { echo "$p: dotnet nao gerou .tpk" >&2; exit 1; }
  cp "$TPK" "$SAIDA/Nuvio-$VER-$p${NUVIO_TPK_TEXTO:+-texto}.tpk"
done

echo "[3/3] conferindo"
for T in "$SAIDA"/*.tpk; do
  L=$(unzip -l "$T")
  # Consumir a lista inteira: grep -q fecha cedo e SIGPIPE mascara o match
  # sob pipefail quando o pacote tem muitas entradas.
  if unzip -Z1 "$T" | grep -E '(^|/)(conta-[^/]*|discord-p[^/]*|jellyfin-p[^/]*|emby-p[^/]*|plex-p[^/]*)\.txt(\.tmp)?$' >/dev/null; then
    echo "tpk.sh: $T leva arquivo privado da conta — abortado" >&2; exit 1
  fi
  grep -qE " lib/libnuvio.so$" <<<"$L" || { echo "$T sem lib/libnuvio.so" >&2; exit 1; }
  grep -qE " res/art/" <<<"$L" || { echo "$T sem res/art" >&2; exit 1; }
  for f in abertura.jpg login-fundo.jpg logo-novo-marca.png logo-classico.png; do
    grep -qE " res/art/marcas/$f$" <<<"$L" || { echo "$T sem res/art/marcas/$f" >&2; exit 1; }
  done
  grep -qE " res/licencas/p2p-avisos.txt$" <<<"$L" || { echo "$T sem res/licencas/p2p-avisos.txt" >&2; exit 1; }
  # Motor pedido (pasta achada) => TODO .tpk 6+ tem de levar a .so dele; o 4/5, nunca.
  case "$T" in
    *-NuvioTpk40.tpk) ! grep -qE " lib/libnuvio_engine.so$" <<<"$L" || { echo "$T leva o motor P2P (o 4/5 nao pode)" >&2; exit 1; } ;;
    *) if [ -n "$NV_P2P_DIR" ]; then
         grep -qE " lib/libnuvio_engine.so$" <<<"$L" || { echo "$T sem lib/libnuvio_engine.so (motor P2P configurado)" >&2; exit 1; }
       fi ;;
  esac
  echo "  $T ($(du -h "$T" | cut -f1))"
done
# O NuvioTpk40 tem de levar a .so SEM TLS, e os outros a de sempre (bytes iguais).
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-stage.XXXXXXXX"); trap 'rm -rf "$D" "$ENVF"' EXIT
for T in "$SAIDA"/*.tpk; do
  unzip -qo "$T" lib/libnuvio.so -d "$D/$(basename "$T" .tpk)"
  case "$T" in
    *-NuvioTpk40.tpk) cmp -s "$D/$(basename "$T" .tpk)/lib/libnuvio.so" "$SAIDA/libnuvio-tpk40.so" || { echo "$T nao leva a libnuvio-tpk40.so" >&2; exit 1; } ;;
    *) cmp -s "$D/$(basename "$T" .tpk)/lib/libnuvio.so" "$SAIDA/libnuvio.so" || { echo "$T nao leva a libnuvio.so comum" >&2; exit 1; } ;;
  esac
done
echo "  NuvioTpk40 leva a libnuvio-tpk40.so; os outros a libnuvio.so comum"
if [ -n "$CANARIO" ]; then
  mkdir -p "$SAIDA/$CANARIO"
  rm -f "$SAIDA/$CANARIO"/*.tpk
  for T in "$SAIDA"/*.tpk; do
    cp "$T" "$SAIDA/$CANARIO/$(basename "$T" .tpk)-$CANARIO.tpk"
  done
  ls -la "$SAIDA/$CANARIO"
fi

# ANEXO DA AUTO-ATUALIZACAO. A release publica ESTA .so (mesma dos quatro
# pacotes) com o sufixo que src/atualizacao.c (acharSo) procura; o GitHub anexa
# o `digest` sha256, que e a barreira de seguranca do staging. Sem este anexo na
# release, o app simplesmente nao oferece a auto-atualizacao (cai na pagina).
cp "$SAIDA/libnuvio.so" "$SAIDA/libnuvio-$VER-tpk-arm.so"
echo "  $SAIDA/libnuvio-$VER-tpk-arm.so (anexo de auto-atualizacao; suba na release)"
# O 4/5 roda a lib SEM TLS (libnuvio-tpk40.so): anexo proprio, que so o host
# 4/5 procura (atualizacao.c com NV_TPK40). Baixar a comum ali quebraria o
# carregador ELF, que recusa PT_TLS.
cp "$SAIDA/libnuvio-tpk40.so" "$SAIDA/libnuvio-$VER-tpk40-arm.so"
echo "  $SAIDA/libnuvio-$VER-tpk40-arm.so (anexo de auto-atualizacao do 4/5)"
