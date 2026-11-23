#!/bin/bash
# Resolver de producao com o motor de playbooks do Farol (CORRECAO-FAROL6B/6C).
# Compila src/resolver.c, src/extplayer.c (Android), src/addonguia.c,
# src/addonmodo.c e os trechos reais de src/player.c e src/app.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-resolver-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

python3 - "$work" <<'PY'
from pathlib import Path
import re, sys

out = Path(sys.argv[1]) / "farol_prod_bridge.inc"
player = Path("src/player.c").read_text(encoding="utf-8")
app = Path("src/app.c").read_text(encoding="utf-8")

def extract_fn(src, sig_re, name, required=True):
    m = re.search(sig_re + r".*?^\}", src, re.M | re.S)
    if not m:
        if required:
            raise SystemExit(f"falha ao extrair {name}")
        return ""
    return m.group(0)

def extract_brace_block(src, start_marker, name):
    i = src.index(start_marker)
    j = src.index("{", i)
    depth = 0
    k = j
    while k < len(src):
        c = src[k]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
        k += 1
    raise SystemExit(f"bloco nao fechado: {name}")

sair = re.search(r"^static void sairParaExterno\(void\) \{[^\n]*\}", player, re.M)
if not sair:
    raise SystemExit("falha ao extrair sairParaExterno")
ext_disp = extract_fn(player, r"^int player_externo_disponivel\(void\) \{", "player_externo_disponivel")
ext_abrir = extract_fn(player, r"^int player_abrir_externo\(void\) \{", "player_abrir_externo")
ult_addon = extract_fn(player, r"^uint64_t player_ultimo_addon_farol\(void\) \{", "player_ultimo_addon_farol", required=False)
ult_tit = extract_fn(player, r"^int player_ultimo_titulo\(CatItem \*out, int \*temporada, int \*episodio\) \{", "player_ultimo_titulo", required=False)

app_res = extract_brace_block(app, "  if (resolver_aberto()) {", "resolver_aberto em app.c")
app_agp = extract_brace_block(app, "  {\n    int p = addonguia_pedido();", "addonguia_pedido em app.c")

parts = [
    "// Gerado por tests/farol_resolver.sh a partir de src/player.c e src/app.c.",
    sair.group(0),
    ext_disp,
    ext_abrir,
    ult_addon,
    ult_tit,
    "static void app_resolver_despachar(const SDL_Event *e) {",
    app_res,
    "}",
    "static void app_addonguia_consumir(void) {",
    app_agp,
    "}",
]
out.write_text("\n\n".join(p for p in parts if p) + "\n", encoding="utf-8")
PY

"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
  -Wall -Wextra -O1 -g \
  tests/farol_resolver.c src/resolver.c src/resolverregra.c src/farolregra.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c src/extplayer.c \
  src/addonguia.c src/addonmodo.c \
  -I"$work" -Isrc $(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2) \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) -lm -lpthread \
  -o "$work/farol-resolver-test"

if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/farol-resolver-test"
else
  "$work/farol-resolver-test"
fi
echo "farol_resolver: tudo ok"
