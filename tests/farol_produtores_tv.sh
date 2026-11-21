#!/bin/bash
# Produtores do anel do Farol nos backends de TV (lote sense, issue 9).
# Ver tests/farol_produtores_tv.c — cada modo compila o backend real e dirige
# a cadeia de erro que a TV produz: "2004 player error:…" (JNI Media3),
# PLAYER_ERROR_CONNECTION_FAILED (AVPlay via nv_av em C — nao ha emcc no VPS,
# entao EM_JS vira declaracao), "erro ConnectionFailed",
# "erro AudioCodecNotSupported" e o relogio congelado (host .tpk).
#   bash tests/farol_produtores_tv.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=thread bash tests/farol_produtores_tv.sh  (TSan; o helper tem fios)
#   SANITIZE=0 bash tests/farol_produtores_tv.sh       (sem sanitizador)
set -eu
cd "$(dirname "$0")/.."

ccbin="${CC:-cc}"
flags=(-O1 -g -Wall -Wno-unused-function -Wno-unused-variable)
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    # Mesmo motivo do farol_anel: o TSan do gcc 13 morre no arranque aqui.
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-produtores-tv.XXXXXX")
trap 'rm -rf "$work"' EXIT

gc=(-ffunction-sections -fdata-sections -Wl,--gc-sections)
sdl="$(sdl2-config --cflags)"
jdk="${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}"
jni=(-I"$jdk/include" -I"$jdk/include/linux")
farol="src/farolerro.c src/farolanel.c src/fontesonda.c"

# Android: o JNI real (nativeEvento) com as cadeias do Media3.
"$ccbin" "${flags[@]}" -Isrc $sdl "${jni[@]}" "${gc[@]}" -DTEST_ANDROID -DNV_ANDROID \
  tests/farol_produtores_tv.c $farol -lpthread -lm -o "$work/android"
"$work/android"

# .tpk: as linhas do log do host, o EV_ERRO nativo e recuperarPlayer.
"$ccbin" "${flags[@]}" -Isrc $sdl "${gc[@]}" -DTEST_TPK -DNV_TPK \
  tests/farol_produtores_tv.c $farol -lpthread -lm -o "$work/tpk"
"$work/tpk"

# .wgt: video_tizen.c inteiro com o stub de emscripten.h + nv_av em C no
# contrato do webapis.avplay (disp/abrir/estado/erro).
"$ccbin" "${flags[@]}" -Isrc -Itests/farol_wgt_stub $sdl "${gc[@]}" \
  -DTEST_TIZEN -D__EMSCRIPTEN__ \
  tests/farol_produtores_tv.c $farol -lpthread -lm -o "$work/tizen"
"$work/tizen"

# Nivel do helper: as cadeias que so nascem dentro do video_bombear, a
# retentativa e 4 fios de produtor na trava do anel (o que o TSan cobre).
"$ccbin" "${flags[@]}" -Isrc "${gc[@]}" \
  tests/farol_produtores_tv.c $farol -lpthread -lm -o "$work/helper"
"$work/helper"

# Estatico: cada ponto novo usa o helper (o tok sai sempre de e.cod, fechado);
# nenhum arquivo de video chama farol_ev direto — nem texto cru nem URL.
for f in video_android.c video_tizen.c video_tpk.c; do
  if grep -n 'farol_ev(' "src/$f"; then
    echo "farol_produtores_tv: FALHA (farol_ev direto em src/$f)" >&2
    exit 1
  fi
done
test "$(grep -c 'farolprod_erro(' src/video_android.c)" -ge 4
test "$(grep -c 'farolprod_recon(' src/video_android.c)" -ge 1
test "$(grep -c 'farolprod_erro(' src/video_tizen.c)" -ge 2
test "$(grep -c 'farolprod_recon(' src/video_tizen.c)" -ge 3
test "$(grep -c 'farolprod_erro(' src/video_tpk.c)" -ge 6
test "$(grep -c 'farolprod_recon(' src/video_tpk.c)" -ge 1

echo "farol_produtores_tv: tudo ok"
