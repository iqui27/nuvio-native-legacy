#!/bin/bash
# Build do Nuvio para Android TV (SDL2 + GLES2, nucleo C em libmain.so).
#
#   tools/android.sh            -> APK debug em build/android/Nuvio-<v>-android-debug.apk
#   NUVIO_P2P_MOTOR=<raiz> tools/android.sh -> motor P2P (padrao: pasta achada por tools/p2p-motor/pasta.sh; =none desliga)
#   NUVIO_KEYSTORE=... NUVIO_KEYSTORE_PASS=... NUVIO_KEY_ALIAS=... NUVIO_KEY_PASS=... tools/android.sh
#                               -> tambem o release assinado, Nuvio-<v>-android.apk
#
# Sem keystore o release fica de fora (e o debug NAO leva o nome final, para
# ninguem publicar um APK de debug por engano).
#
# (O estagio da arte e relativo: o tizen-art.sh nao aguenta espaco no caminho.)
# Preparo: fontes do SDL em ~/.cache/nuvio-android/src (mesmas URLs/versoes do
# tools/tpk.sh), arte reduzida (tools/tizen-art.sh), fontes, o arquivo de chaves
# do tools/env.sh (build/android/nuvio-env.cmake, FORA DO GIT) e as libs nativas
# curl/libjpeg se o outro agente ja as compilou em ~/.cache/nuvio-android/prefix.
set -euo pipefail
cd "$(dirname "$0")/.."
RAIZ="$PWD"
RELEASE_ONLY=0
case "${1:-}" in "") ;; --release-only) RELEASE_ONLY=1;; *) echo "opcao desconhecida: $1" >&2; exit 2;; esac
CACHE="${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}"
EST="$RAIZ/build/android"

export JAVA_HOME="${JAVA_HOME:-$(ls -d "$HOME"/.local/jdks/jdk-17*/Contents/Home 2>/dev/null | head -1)}"
export ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
[ -x "$JAVA_HOME/bin/java" ] || { echo "android.sh: JDK 17 nao achado (JAVA_HOME)" >&2; exit 1; }
for d in ndk/27.2.12479018 platforms/android-35 build-tools/35.0.0 cmake/3.22.1; do
  [ -d "$ANDROID_HOME/$d" ] || { echo "android.sh: falta $ANDROID_HOME/$d (sdkmanager)" >&2; exit 1; }
done

VER=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' deploy/app/appinfo.json | head -1)
[ -n "$VER" ] || { echo "android.sh: sem version em deploy/app/appinfo.json" >&2; exit 1; }

echo "[1/5] fontes do SDL em $CACHE/src"
mkdir -p "$CACHE/src"
for u in https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz \
         https://github.com/libsdl-org/SDL_image/releases/download/release-2.8.2/SDL2_image-2.8.2.tar.gz \
         https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz \
         https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.4.0.tar.gz; do
  d="$CACHE/src/$(basename "$u" .tar.gz)"
  [ -d "$d" ] || curl -fsSL "$u" | tar xz -C "$CACHE/src"
done

echo "[2/5] arte e fontes -> $EST/assets"
rm -rf "$EST/assets"; mkdir -p "$EST/assets"
NUVIO_ARTE_ESTAGIO="build/android/assets/art" bash tools/tizen-art.sh >/dev/null
mkdir -p "$EST/assets/fonts"
cp deploy/app/fonts/* "$EST/assets/fonts/"
mkdir -p "$EST/assets/licencas"
cp deploy/app/licencas/* "$EST/assets/licencas/"   # avisos do motor P2P (libtorrent, Boost, OpenSSL, nuvio-engine)

echo "[3/5] chaves (-D) e libs nativas"
ENVF="$(mktemp "${TMPDIR:-/tmp}/nuvio-android-env.XXXXXXXX")"; trap 'rm -f "$ENVF"' EXIT
# env.sh le UMA opcao so: a checagem de configuracao vai numa chamada propria.
[ -z "${NUVIO_REQUIRE_CORE:-}" ] || tools/env.sh --require-core >/dev/null
tools/env.sh --env-file "$ENVF"
# KEY=valor -> set(KEY "valor") com escape de \ " $ . Fora do git (build/).
: > "$EST/nuvio-env.cmake.new"; chmod 600 "$EST/nuvio-env.cmake.new"
while IFS= read -r l; do
  k="${l%%=*}"; v="${l#*=}"
  v=$(printf '%s' "$v" | sed 's/[\\"$]/\\&/g')
  printf 'set(%s "%s")\n' "$k" "$v" >> "$EST/nuvio-env.cmake.new"
done < "$ENVF"
# Nao invalida CMake por timestamp quando a configuracao e identica.
if cmp -s "$EST/nuvio-env.cmake.new" "$EST/nuvio-env.cmake"; then rm "$EST/nuvio-env.cmake.new"
else mv "$EST/nuvio-env.cmake.new" "$EST/nuvio-env.cmake"; fi
# libcurl/libjpeg/libwebp por dlopen: se o outro agente ja as compilou, entram em lib/<abi>/.
rm -rf "$EST/jnilibs"
for abi in arm64-v8a armeabi-v7a; do
  for so in libcurl.so libjpeg.so libwebp.so; do
    f="$CACHE/prefix/$abi/lib/$so"
    if [ -f "$f" ]; then mkdir -p "$EST/jnilibs/$abi"; cp "$f" "$EST/jnilibs/$abi/"
    else echo "  aviso: $abi/$so ausente em $CACHE/prefix (o APK segue sem ela)"; fi
  done
done

echo "[4/5] gradle"
GR=(android/gradlew -p android --console=plain -Pnuvio.sdlSrc="$CACHE/src" -Pnuvio.estagio="$EST")
# Motor P2P embutido: pasta achada por tools/p2p-motor/pasta.sh (NUVIO_P2P_MOTOR,
# ou a pasta padrao); configurada e incompleta = erro, nunca pacote sem motor calado.
. tools/p2p-motor/pasta.sh
nv_p2p_resolver android
[ -n "$NV_P2P_DIR" ] && GR+=(-Pnuvio.p2pMotor="$NV_P2P_DIR")
TAREFAS=()
[ "$RELEASE_ONLY" = 1 ] || TAREFAS=(assembleDebug)
# Chave de release FIXA (o Android so atualiza por cima com a mesma
# assinatura): ~/.nuvio-android/release.env, fora do repo, chmod 600. A copia
# de seguranca e o item do Vaultwarden. Variaveis no ambiente ganham do arquivo.
if [ -z "${NUVIO_KEYSTORE:-}" ] && [ -f "$HOME/.nuvio-android/release.env" ]; then
  set -a; . "$HOME/.nuvio-android/release.env"; set +a
fi
[ -n "${NUVIO_KEYSTORE:-}" ] && TAREFAS+=(assembleRelease)
[ "${#TAREFAS[@]}" -gt 0 ] || { echo "android.sh: release-only exige chave" >&2; exit 1; }
[ -z "${NUVIO_BUILD_JOBS:-}" ] || GR+=(--max-workers="$NUVIO_BUILD_JOBS")
"${GR[@]}" "${TAREFAS[@]}"

echo "[5/5] conferencia"
SAIDA="$EST"
DBG="android/app/build/outputs/apk/debug/app-debug.apk"
APKS=()
if [ "$RELEASE_ONLY" = 0 ]; then
  cp "$DBG" "$SAIDA/Nuvio-$VER-android-debug.apk"
  APKS+=("$SAIDA/Nuvio-$VER-android-debug.apk")
fi
if [ -n "${NUVIO_KEYSTORE:-}" ]; then
  cp android/app/build/outputs/apk/release/app-release.apk "$SAIDA/Nuvio-$VER-android.apk"
  APKS+=("$SAIDA/Nuvio-$VER-android.apk")
else
  echo "  release fora: sem NUVIO_KEYSTORE"
fi

# Mesma lista de credenciais de pessoa do tools/release-samsung.sh.
SEGREDO='(^|/)(trakt|addons|tmdb|mdblist|sessao|simkl[^/]*|fanart|diag-token)\.txt$|collections\.json$|catalogo-rede\.bin|local\.properties|\.env$|(^|/)(trakt|stalker|xtream|listas)-p[0-9]|(^|/)conta-[^/]*\.txt(\.tmp)?$'
for a in "${APKS[@]}"; do
  L=$(unzip -Z1 "$a")
  for need in lib/arm64-v8a/libmain.so lib/armeabi-v7a/libmain.so lib/arm64-v8a/libSDL2.so lib/armeabi-v7a/libSDL2.so; do
    printf '%s\n' "$L" | grep -qx "$need" || { echo "android.sh: $a nao tem $need" >&2; exit 1; }
  done
  printf '%s\n' "$L" | grep -q '^assets/art/' || { echo "android.sh: $a sem assets/art" >&2; exit 1; }
  printf '%s\n' "$L" | grep -q '^assets/fonts/' || { echo "android.sh: $a sem assets/fonts" >&2; exit 1; }
  printf '%s\n' "$L" | grep -q '^assets/licencas/p2p-avisos.txt$' || { echo "android.sh: $a sem assets/licencas" >&2; exit 1; }
  if [ -n "$NV_P2P_DIR" ]; then   # motor pedido: o simbolo da API C tem de estar nas DUAS libmain.so
    for abi in arm64-v8a armeabi-v7a; do
      m=$(unzip -p "$a" "lib/$abi/libmain.so" | strings | grep -c 'Nuvio Engine/' || true)
      [ "$m" -ge 1 ] || { echo "android.sh: $a lib/$abi/libmain.so SEM o motor P2P (marca "Nuvio Engine/")" >&2; exit 1; }
    done
    echo "  motor P2P dentro das duas libmain.so"
  fi
  n=$(printf '%s\n' "$L" | grep -c -E "$SEGREDO" || true)
  [ "$n" = "0" ] || { echo "android.sh: $a leva $n arquivo(s) de pessoa:" >&2; printf '%s\n' "$L" | grep -E "$SEGREDO" >&2; exit 1; }
  echo "ok: $a ($(du -h "$a" | cut -f1))"
  printf '%s\n' "$L" | grep -E '^lib/.*\.so$' | sed 's/^/  /'
done
