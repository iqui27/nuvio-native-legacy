#!/bin/bash
# FAROL — issue 22 (Vai junto / Nunca vai, Enviado, Ja corrigido e estados §3.3).
#   bash tests/farol_vai.sh             (SANITIZE=1 padrao; roda 0 e 1 no CI)
#   SANITIZE=0 bash tests/farol_vai.sh
set -eu
cd "$(dirname "$0")/.."

falha() { echo "farol_vai: FALHA ($*)" >&2; exit 1; }

# 1. Contratos estaticos (fronteira de arquivos, sem TLS, sem logs sensiveis).
for f in src/farolvai.h src/farolenvioui.h src/farolenvioui.c; do
  [ -f "$f" ] || falha "arquivo ausente: $f"
done
if grep -nE '(^|[^/])(_Thread_local|__thread)' src/farolvai.h src/farolenvioui.h src/farolenvioui.c src/resolver.c src/resolver.h \
   | grep -vE '^[a-zA-Z0-9_./-]+:[0-9]+:[[:space:]]*(//|\*)'; then
  falha "_Thread_local proibido"
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|LOGD|puts|fputs)\s*\(' src/farolenvioui.c src/farolvai.h; then
  falha "log proibido em farolenvioui/farolvai"
fi
if git diff --quiet HEAD -- src/app.c 2>/dev/null; then :; else
  if git diff HEAD -- src/app.c | grep -nE '^[+-].*(urlCanalRota|retentarCanal)'; then
    falha "src/app.c mexeu no canal ao vivo"
  fi
fi
if git diff HEAD -- src/resolver.c src/resolver.h 2>/dev/null | grep -nE '^[+-].*(ACHAR_X|RESOLVER_B_ACHAR)'; then
  falha "mexeu na coluna da #25"
fi

flags=(-Wall -Wextra -O1 -g -std=gnu11 -DRESOLVER_TESTE -DNV_LINUX_DESKTOP)
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  *) falha "SANITIZE invalido: ${SANITIZE}" ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-vai.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$work"' EXIT

python3 - "$work" <<'PY'
from pathlib import Path
import sys

app = Path("src/app.c").read_text(encoding="utf-8")
marker = "  if (resolver_aberto()) {"
i = app.index(marker)
j = app.index("{", i)
depth = 0
k = j
while k < len(app):
    if app[k] == "{":
        depth += 1
    elif app[k] == "}":
        depth -= 1
        if depth == 0:
            block = app[i:k + 1]
            break
    k += 1
else:
    raise SystemExit("bloco resolver_aberto nao fechado")

out = Path(sys.argv[1]) / "app_resolver_bridge.inc"
out.write_text(
    "// Extraido de src/app.c para validar RESOLVER_PEDIU_CONTAS.\n"
    "static void app_resolver_despachar(const SDL_Event *e) {\n"
    + block + "\n}\n",
    encoding="utf-8",
)
PY
python3 tests/farol_envio_server.py "$work/port" &
srv=$!
for _ in $(seq 100); do [ -s "$work/port" ] && break; sleep 0.1; done
test -s "$work/port" || falha "servidor local nao subiu"
base="http://127.0.0.1:$(cat "$work/port")"

# Syntax check rigoroso nos arquivos novos
"$ccbin" "${flags[@]}" -Werror $(pkg-config --cflags sdl2 glesv2) -Isrc -fsyntax-only src/farolenvioui.c

# 2. Teste logico + servidor HTTP + D-pad + GL texture lifecycle
"$ccbin" "${flags[@]}" -I"$work" -Isrc -Itests \
  $(pkg-config --cflags sdl2 glesv2) \
  tests/farol_vai.c src/farolenvioui.c src/farolenvio.c src/farolpacote.c \
  src/farolanel.c src/farolperfil.c src/farolredator.c src/farolerro.c \
  src/farolregra.c src/farol.c src/resolver.c src/resolverregra.c \
  src/fontesonda.c src/extplayer.c src/addonmodo.c \
  src/qr.c src/rede.c src/redeurl.c src/regcodigo.c \
  $(pkg-config --libs sdl2) -lm -lpthread -ldl -o "$work/vai"

mkdir -p "$work/dados"
"$work/vai" "$base" "$work/dados" >"$work/log" 2>&1 || { cat "$work/log" >&2; falha "testes funcionais"; }
cat "$work/log"
if grep -q 'tok-farol-18' "$work/log" || grep -qF "$base" "$work/log"; then
  falha "token ou URL vazou no log"
fi

# 3. Captura PNG das 3 telas em diretorio temporario (removido pelo trap EXIT)
if pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2 && { [ -n "${DISPLAY:-}" ] || command -v xvfb-run >/dev/null 2>&1; }; then
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
  objcopy --weaken-symbol=avisos_registro_cabecalhos "$objdir/src_avisos.c.o"
  "$ccbin" "${fl_shot[@]}" -DFAROL_VAI_SHOT \
    tests/farol_vai.c "$objdir"/*.o -o "$work/vai-shot" "${lf[@]}" -lz -pthread -lm -ldl
  NUVIO_DADOS="$dados_shot" ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
    "${runner[@]+"${runner[@]}"}" "$work/vai-shot" "$shotdir" "$base"
  python3 - "$shotdir" <<'PY'
import pathlib, struct, sys
d = pathlib.Path(sys.argv[1])
for name in ("vai-4-confirma.png", "vai-5-enviado.png", "vai-6-corrigido.png"):
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
  echo "farol_vai: AVISO - GLES2/xvfb-run ausentes; capturas PNG puladas" >&2
fi

echo "farol_vai: tudo ok"
