#!/bin/bash
# Build do Nuvio para Android TV/Mobile (SDL2 + GLES2, nucleo C em libmain.so).
#
#   NUVIO_APK_LITE=1 tools/android.sh --release-only -> signed APK without the P2P motor
#   tools/android.sh --debug-only -> debug only, never read release signing keys
#   tools/android.sh            -> APK debug em build/android/Nuvio-<v>-android-debug.apk
#   NUVIO_RUST_MKVCORE=0 tools/android.sh -> nucleo mkv em C (padrao: Rust, ligado em 14/10/2026)
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
# Offline validation must never fetch missing build inputs.
if [ "${NUVIO_ANDROID_OFFLINE:-0}" = 1 ]; then
  curl() { echo 'android.sh: network disabled for offline build' >&2; return 1; }
  export -f curl
  export CARGO_NET_OFFLINE=true
fi
RELEASE_ONLY=0
DEBUG_ONLY=0
case "${1:-}" in "") ;; --release-only) RELEASE_ONLY=1;; --debug-only) DEBUG_ONLY=1;; *) echo "opcao desconhecida: $1" >&2; exit 2;; esac
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
    # Sem libcurl o app cai inteiro no HttpURLConnection e imagem chega com 0 B
    # (11/10: limpeza de disco apagou ~/.cache/*; APK sem logo nem capa na TCL).
    elif [ "$so" = libcurl.so ] && [ -z "${NUVIO_SEM_CURL:-}" ]; then
      echo "ERRO: $abi/libcurl.so ausente em $CACHE/prefix (NUVIO_SEM_CURL=1 para seguir sem)" >&2; exit 1
    else echo "  aviso: $abi/$so ausente em $CACHE/prefix (o APK segue sem ela)"; fi
  done
done

echo "[4/5] gradle"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$ANDROID_HOME/ndk/27.2.12479018}"
FFMPEG_AAR="$EST/media3-ffmpeg-16kb.aar"
bash tools/android/ffmpeg-android.sh "$FFMPEG_AAR" arm64-v8a armeabi-v7a
# Seek-bar thumbnails made on the device: thumbnail-only FFmpeg (src/miniatura*.c).
MINI_FF="$CACHE/mini-ffmpeg"
bash tools/android/ffmpeg-miniaturas.sh "$MINI_FF" arm64-v8a armeabi-v7a
# LGPL: its license text and source pin travel with the APK.
mkdir -p "$EST/assets/licencas/ffmpeg-miniaturas"
cp "$MINI_FF/arm64-v8a/COPYING.LGPLv2.1" "$MINI_FF/arm64-v8a/SOURCE.txt" "$EST/assets/licencas/ffmpeg-miniaturas/"
GR=(android/gradlew -p android --console=plain -Pnuvio.sdlSrc="$CACHE/src" -Pnuvio.estagio="$EST" -Pnuvio.ffmpegAar="$FFMPEG_AAR" -Pnuvio.miniFfmpeg="$MINI_FF")
[ "${NUVIO_ANDROID_OFFLINE:-0}" != 1 ] || GR+=(--offline)
# Motor P2P embutido: pasta achada por tools/p2p-motor/pasta.sh (NUVIO_P2P_MOTOR,
# ou a pasta padrao); configurada e incompleta = erro, nunca pacote sem motor calado.
. tools/p2p-motor/pasta.sh
LITE="${NUVIO_APK_LITE:-0}"
NV_P2P_DIR=""
if [ "$LITE" = 1 ]; then
  GR+=(-Pnuvio.apkLite=1)
else
  nv_p2p_resolver android
fi
# Nucleo EBML/NAL do mkv.c em Rust, ligado por padrao (dono aprovou em 14/10/2026);
# NUVIO_RUST_MKVCORE=0 volta ao C para comparar.
if [ "${NUVIO_RUST_MKVCORE:-1}" = 1 ]; then
  NV_RUST_MKV_DIR="$EST/rust-mkvcore"
  tools/rust-mkvcore.sh --android "$NV_RUST_MKV_DIR" >&2
  GR+=(-Pnuvio.rustMkvcore="$NV_RUST_MKV_DIR")
fi
TAREFAS=()
[ "$RELEASE_ONLY" = 1 ] || TAREFAS=(assembleDebug)
# Chave de release FIXA (o Android so atualiza por cima com a mesma
# assinatura): ~/.nuvio-android/release.env, fora do repo, chmod 600. A copia
# de seguranca e o item do Vaultwarden. Variaveis no ambiente ganham do arquivo.
if [ "$DEBUG_ONLY" = 0 ] && [ -z "${NUVIO_KEYSTORE:-}" ] && [ -f "$HOME/.nuvio-android/release.env" ]; then
  set -a; . "$HOME/.nuvio-android/release.env"; set +a
fi
[ "$DEBUG_ONLY" = 0 ] && [ -n "${NUVIO_KEYSTORE:-}" ] && TAREFAS+=(assembleRelease)
[ "${#TAREFAS[@]}" -gt 0 ] || { echo "android.sh: release-only exige chave" >&2; exit 1; }
[ -z "${NUVIO_BUILD_JOBS:-}" ] || GR+=(--max-workers="$NUVIO_BUILD_JOBS")
GR_FULL=("${GR[@]}")
[ -z "$NV_P2P_DIR" ] || GR_FULL+=(-Pnuvio.p2pMotor="$NV_P2P_DIR")
"${GR_FULL[@]}" "${TAREFAS[@]}"

echo "[5/5] conferencia"
SAIDA="$EST"
APP_BUILD="android/app/build"
if [ "$LITE" = 1 ]; then APP_BUILD="android/app/build-lite"; fi
APKS=()
if [ "$RELEASE_ONLY" = 0 ]; then
  if [ "$LITE" = 1 ]; then
    for pair in armeabi-v7a:armv7 arm64-v8a:arm64; do
      cp "$APP_BUILD/outputs/apk/debug/app-${pair%:*}-debug.apk" "$SAIDA/Nuvio-$VER-android-lite-${pair#*:}-debug.apk"
      APKS+=("$SAIDA/Nuvio-$VER-android-lite-${pair#*:}-debug.apk")
    done
  else
    cp "$APP_BUILD/outputs/apk/debug/app-debug.apk" "$SAIDA/Nuvio-$VER-android-debug.apk"
    APKS+=("$SAIDA/Nuvio-$VER-android-debug.apk")
  fi
fi
if [ "$DEBUG_ONLY" = 0 ] && [ -n "${NUVIO_KEYSTORE:-}" ]; then
  if [ "$LITE" != 1 ]; then
    cp "$APP_BUILD/outputs/apk/release/app-release.apk" "$SAIDA/Nuvio-$VER-android.apk"
    APKS+=("$SAIDA/Nuvio-$VER-android.apk")
    "${GR[@]}" -Pnuvio.apkLite=1 assembleRelease
  fi
  for pair in armeabi-v7a:armv7 arm64-v8a:arm64; do
    cp "android/app/build-lite/outputs/apk/release/app-${pair%:*}-release.apk" "$SAIDA/Nuvio-$VER-android-lite-${pair#*:}.apk"
    APKS+=("$SAIDA/Nuvio-$VER-android-lite-${pair#*:}.apk")
  done
else
  echo "  release fora: sem NUVIO_KEYSTORE"
fi

# Mesma lista de credenciais de pessoa do tools/release-samsung.sh.
SEGREDO='(^|/)(livetv-contas-|epg-conta-|m3u-|debrid|provider-scope|fontefalha|fontesmortas|catalogo-|guia-(fav|modo|preview|cache|addons|lembretes|categorias)|epg-extra|recomendacoes|amigos-vistos)[^/]*\.(txt|json|bin|ts|id)(\.tmp[^/]*)?$|(^|/)(trakt|addons|tmdb|mdblist|sessao|simkl[^/]*|fanart|diag-token)\.txt$|collections\.json$|catalogo-rede\.bin|local\.properties|\.env$|(^|/)(trakt|stalker|xtream|listas)-p[0-9]|(^|/)conta-[^/]*\.txt(\.tmp)?$'
for a in "${APKS[@]}"; do
  L=$(unzip -Z1 "$a")
  ABIS=(arm64-v8a armeabi-v7a)
  case "$a" in *-lite-armv7*.apk) ABIS=(armeabi-v7a);; *-lite-arm64*.apk) ABIS=(arm64-v8a);; esac
  actual_abis=$(printf '%s\n' "$L" | awk -F/ '$1 == "lib" {print $2}' | sort -u)
  expected_abis=$(printf '%s\n' "${ABIS[@]}" | sort -u)
  [ "$actual_abis" = "$expected_abis" ] || { echo "android.sh: $a ABI mismatch" >&2; exit 1; }
  for abi in "${ABIS[@]}"; do
    for so in libmain.so libSDL2.so libcurl.so libjpeg.so libwebp.so libffmpegJNI.so; do
      printf '%s\n' "$L" | grep -x "lib/$abi/$so" >/dev/null || { echo "android.sh: $a missing lib/$abi/$so" >&2; exit 1; }
    done
  done
  printf '%s\n' "$L" | grep '^assets/art/' >/dev/null || { echo "android.sh: $a sem assets/art" >&2; exit 1; }
  printf '%s\n' "$L" | grep '^assets/fonts/' >/dev/null || { echo "android.sh: $a sem assets/fonts" >&2; exit 1; }
  printf '%s\n' "$L" | grep '^assets/licencas/p2p-avisos.txt$' >/dev/null || { echo "android.sh: $a sem assets/licencas" >&2; exit 1; }
  if [[ "$a" = *-lite*.apk ]]; then
    for abi in "${ABIS[@]}"; do
      m=$(unzip -p "$a" "lib/$abi/libmain.so" | strings | grep -c 'Nuvio Engine/' || true)
      [ "$m" = 0 ] || { echo "android.sh: $a contains the P2P motor" >&2; exit 1; }
    done
  elif [ -n "$NV_P2P_DIR" ]; then   # motor pedido: o simbolo da API C tem de estar nas DUAS libmain.so
    for abi in arm64-v8a armeabi-v7a; do
      m=$(unzip -p "$a" "lib/$abi/libmain.so" | strings | grep -c 'Nuvio Engine/' || true)
      [ "$m" -ge 1 ] || { echo "android.sh: $a lib/$abi/libmain.so SEM o motor P2P (marca "Nuvio Engine/")" >&2; exit 1; }
    done
    echo "  motor P2P dentro das duas libmain.so"
  fi
  n=$(printf '%s\n' "$L" | grep -c -E "$SEGREDO" || true)
  [ "$n" = "0" ] || { echo "android.sh: $a leva $n arquivo(s) de pessoa:" >&2; printf '%s\n' "$L" | grep -E "$SEGREDO" >&2; exit 1; }
  python3 tools/android-apk-pages.py "$a"
  echo "ok: $a ($(du -h "$a" | cut -f1))"
  printf '%s\n' "$L" | grep -E '^lib/.*\.so$' | sed 's/^/  /'
done

