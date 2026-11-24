#!/bin/bash
# FAROL — issue #53 (Consentimento do gosto: estados, persistencia, espelho == corpo, D-pad e captura PNG).
#   bash tests/farol_consent.sh
#   SANITIZE=0 bash tests/farol_consent.sh
#   CC=clang SANITIZE=1 bash tests/farol_consent.sh
set -eu
cd "$(dirname "$0")/.."

falha() { echo "farol_consent: FALHA ($*)" >&2; exit 1; }

# 1. Contratos estaticos (fronteira de arquivos, sem _Thread_local, sem logs, farolconsent puro sem SDL).
for f in src/farolconsent.h src/farolconsent.c src/farolconsentui.h src/farolconsentui.c; do
  [ -f "$f" ] || falha "arquivo ausente: $f"
done

if grep -nE '(^|[^/])(_Thread_local|__thread)' \
     src/farolconsent.h src/farolconsent.c src/farolconsentui.h src/farolconsentui.c src/resolver.c \
   | grep -vE '^[a-zA-Z0-9_./-]+:[0-9]+:[[:space:]]*(//|\*)'; then
  falha "_Thread_local proibido"
fi

if grep -nE '\b(printf|fprintf|syslog|NSLog|LOGD|puts|fputs)\s*\(' \
     src/farolconsent.h src/farolconsent.c src/farolconsentui.h src/farolconsentui.c; then
  falha "log/printf proibido em farolconsent*"
fi

if git diff --quiet HEAD -- src/resolver.c src/resolver.h 2>/dev/null; then :; else
  if git diff HEAD -- src/resolver.c src/resolver.h | grep -nE '^[+-].*(ACHAR_X|RESOLVER_B_ACHAR|M_ENVIANDO|M_ENVIADO|farolenvioui_abrir)'; then
    falha "mexeu em RESOLVER_FASE_ENVIO ou ACHAR_X"
  fi
fi

ccbin="${CC:-cc}"
flags=(-Wall -Wextra -O1 -g -std=gnu11 -DRESOLVER_TESTE -DNV_LINUX_DESKTOP)
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer) ;;
  *) falha "SANITIZE invalido: ${SANITIZE}" ;;
esac

# farolconsent.c deve compilar puro SEM headers SDL; arquivos novos com -Werror.
"$ccbin" -Wall -Wextra -Werror -O1 -g -std=gnu11 -Isrc -fsyntax-only src/farolconsent.c
"$ccbin" "${flags[@]}" -Werror $(pkg-config --cflags sdl2 glesv2) -Isrc -fsyntax-only src/farolconsentui.c
"$ccbin" "${flags[@]}" -Werror -Isrc -fsyntax-only src/farolcompor.c
"$ccbin" "${flags[@]}" -Werror $(pkg-config --cflags sdl2 glesv2) -Isrc -Itests -fsyntax-only tests/farol_consent.c

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-consent.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/dados"

"$ccbin" "${flags[@]}" -Isrc -Itests \
  $(pkg-config --cflags sdl2 glesv2) \
  tests/farol_consent.c \
  src/farolconsent.c src/farolconsentui.c \
  src/farolcompor.c src/farolgosto.c src/farolrec.c src/farolpeca.c src/faroltexto.c \
  src/farolregra.c src/farol.c src/farolanel.c src/farolerro.c src/farolperfil.c src/farolredator.c \
  src/resolver.c src/resolverregra.c \
  src/fontesonda.c src/extplayer.c src/addonmodo.c \
  src/js.c src/rede.c src/redeurl.c \
  $(pkg-config --libs sdl2) -lm -lpthread -ldl \
  -o "$work/consent"

"$work/consent" "$work/dados" >"$work/log" 2>&1 || { cat "$work/log" >&2; falha "testes funcionais"; }
cat "$work/log"

# 2. Captura PNG 1920x1080 no FBO GLES2 (com limpeza garantida via trap EXIT).
if [ "${FAROL_CONSENT_SKIP_SHOT:-0}" != 1 ] && \
   pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2 && \
   { [ -n "${DISPLAY:-}" ] || command -v xvfb-run >/dev/null 2>&1; }; then
  if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
  runner=()
  if [ -z "${DISPLAY:-}" ]; then
    runner=(xvfb-run -a -s "-screen 0 1920x1080x24")
  fi
  objdir="$work/obj"
  shotdir="$work/shots"
  dados_shot="$work/dados-shot"
  mkdir -p "$objdir" "$shotdir" "$dados_shot"
  ls src/*.c src/dts/*.c | grep -v '/main\.c$' > "$work/fontes"
  read -r -a cf <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2)"
  read -r -a lf <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2)"
  fl_shot=(-O1 -g -Isrc -DAJUSTES_TESTE -DAVISOS_TESTE_ENVIO -DRESOLVER_TESTE -DNV_LINUX_DESKTOP
           -Wall -Wextra -Wno-deprecated-declarations "${cf[@]}")
  case "$("$ccbin" --version 2>/dev/null | head -n1)" in
    *clang*|*Clang*) fl_shot+=(-Wno-macro-redefined) ;;
  esac
  if [ "${SANITIZE:-1}" = 1 ]; then
    fl_shot+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  fi
  printf '%s\0' "${fl_shot[@]}" > "$work/flags"
  export ccbin objdir
  jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
  xargs -a "$work/fontes" -P "$jobs" -I{} bash -c '
    mapfile -d "" f < "$0"
    o="$objdir/$(echo "{}" | tr / _).o"
    "$ccbin" "${f[@]}" -c "{}" -o "$o"' "$work/flags"
  "$ccbin" "${fl_shot[@]}" -DFAROL_CONSENT_SHOT \
    tests/farol_consent.c "$objdir"/*.o -o "$work/consent-shot" "${lf[@]}" -lz -pthread -lm -ldl
  NUVIO_DADOS="$dados_shot" ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
    "${runner[@]+"${runner[@]}"}" "$work/consent-shot" "$shotdir"
  python3 - "$shotdir" <<'PY'
import pathlib, struct, sys
d = pathlib.Path(sys.argv[1])
for name in ("consent-vidro.png", "consent-solido.png"):
    p = d / name
    assert p.is_file(), f"PNG ausente: {name}"
    data = p.read_bytes()
    assert len(data) > 5000, f"PNG pequeno demais: {name} ({len(data)} B)"
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"assinatura PNG invalida: {name}"
    w, h = struct.unpack(">II", data[16:24])
    assert (w, h) == (1920, 1080), f"dimensoes invalidas em {name}: {w}x{h}"
    print(f"PNG ok: {name} ({w}x{h}, {len(data)} B)")
PY
else
  echo "farol_consent: AVISO - GLES2/xvfb-run ausentes ou pulado; captura PNG nao rodada" >&2
fi

echo "farol_consent: tudo ok"
