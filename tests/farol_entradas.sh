#!/bin/bash
# Teste dirigido da issue #23 do Farol (Entradas e batismo + FAROL27-r1).
#   bash tests/farol_entradas.sh
#   SANITIZE=1 bash tests/farol_entradas.sh
set -euo pipefail
cd "$(dirname "$0")/.."

# 1. Varredura estatica: nenhum arquivo de UI em src/ (fora idioma_*.h,
#    FT_RESOLVER_NOME em faroltexto.h e comentarios) pode exibir "Resolver um problema".
python3 - <<'PY'
import pathlib, re, sys

bad = []
for p in sorted(pathlib.Path("src").rglob("*")):
    if not p.is_file() or p.suffix not in (".c", ".h", ".inc"):
        continue
    if p.name.startswith("idioma_"):
        continue
    txt = p.read_text(encoding="utf-8", errors="replace")
    # Remove comentarios de bloco e linha preservando strings
    out = []
    i = 0
    n = len(txt)
    while i < n:
        c = txt[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n:
                if txt[j] == '\\':
                    j += 2
                    continue
                if txt[j] == q:
                    j += 1
                    break
                j += 1
            out.append(txt[i:j])
            i = j
        elif txt[i:i+2] == "//":
            j = txt.find("\n", i)
            i = n if j < 0 else j
        elif txt[i:i+2] == "/*":
            j = txt.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    clean = "".join(out)
    for lineno, line in enumerate(clean.splitlines(), 1):
        if "Resolver um problema" in line:
            if p.name == "faroltexto.h" and "FT_RESOLVER_NOME" in line:
                continue
            bad.append(f"{p}:{lineno}: {line.strip()}")

if bad:
    print("FALHA varredura estatica 'Resolver um problema':")
    for b in bad:
        print(" ", b)
    sys.exit(1)
print("varredura estatica: ok (0 literais 'Resolver um problema' fora idioma_*/FT_RESOLVER_NOME)")
PY

dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-entradas.XXXXXX")
trap 'rm -rf "$dir" /tmp/nuvio-key' EXIT
mkdir -p "$dir/data-ui" "$dir/data-key"

sdl=()
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a sdl <<< "$(pkg-config --cflags sdl2)"
fi
if [ "$(uname -s)" = Darwin ]; then
  sdl+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  strip=(-Wl,-dead_strip)
  libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  strip=(-Wl,--gc-sections)
  libs=($(pkg-config --libs sdl2 SDL2_image SDL2_ttf gl 2>/dev/null) -lz -lm -lpthread)
fi

flags=(-O1 -g -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined -ffunction-sections -fdata-sections)
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

sources_ui=()
sources_key=()
for source in src/*.c src/dts/*.c; do
  case "$source" in
    src/main.c) continue ;;
    src/ajustes.c) sources_key+=("$source"); continue ;;
  esac
  sources_ui+=("$source")
  sources_key+=("$source")
done

"$ccbin" "${sources_ui[@]}" tests/farol_entradas.c -Isrc "${sdl[@]}" \
  -D_GNU_SOURCE -DAJUSTES_TESTE -DRESOLVER_TESTE "${flags[@]}" "${strip[@]}" \
  "${libs[@]}" -o "$dir/teste_ui"
"$dir/teste_ui" "$dir/data-ui"

"$ccbin" "${sources_key[@]}" tests/farol_entradas.c -Isrc "${sdl[@]}" \
  -D_GNU_SOURCE -DAJUSTES_TESTE -DRESOLVER_TESTE -DFAROL_TESTE_KEY "${flags[@]}" "${strip[@]}" \
  "${libs[@]}" -o "$dir/teste_key"
"$dir/teste_key" "$dir/data-key"

echo "farol_entradas: tudo ok"
