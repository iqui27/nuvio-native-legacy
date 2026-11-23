#!/bin/bash
# Prova dirigida de identidade/atomicidade de erro Android (Kotlin/JNI -> C -> Resolver).
set -euo pipefail
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

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-android-erro.XXXXXX")
trap 'rm -rf "$work"' EXIT

ndk="${ANDROID_NDK_HOME:-${ANDROID_HOME:-/home/ubuntu/nuvio-qa-21/global/toolchains/android-sdk}/ndk/27.2.12479018}"
mkdir -p "$work/include"
cp "$ndk"/toolchains/llvm/prebuilt/*/sysroot/usr/include/jni.h "$work/include/"
if [ -d "${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}/src/SDL2-2.30.9/include" ]; then
  ln -s "${NUVIO_ANDROID_CACHE:-$HOME/.cache/nuvio-android}/src/SDL2-2.30.9/include" "$work/include/SDL2"
elif [ -d /usr/include/SDL2 ]; then
  ln -s /usr/include/SDL2 "$work/include/SDL2"
fi

python3 - "$work" <<'PY'
from pathlib import Path
import re, sys

out = Path(sys.argv[1]) / "farol_android_bridge.inc"
player = Path("src/player.c").read_text(encoding="utf-8")
app = Path("src/app.c").read_text(encoding="utf-8")

def extract_fn(src, sig_re, name):
    m = re.search(sig_re + r".*?^\}", src, re.M | re.S)
    if not m:
        raise SystemExit(f"falha ao extrair {name}")
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

parts = [
    "// Gerado por tests/farol_android_erro.sh a partir de src/player.c e src/app.c.",
    sair.group(0),
    extract_fn(player, r"^static void farolTentativaIniciar\(void\) \{", "farolTentativaIniciar"),
    extract_fn(player, r"^static void farolFonteRegistrar\(const char \*url, int temCabs\) \{", "farolFonteRegistrar"),
    extract_fn(player, r"^static void farolErroRegistrar\(const FarolErro \*fe, int foiDebrid, uint64_t addonId\) \{", "farolErroRegistrar"),
    extract_fn(player, r"^static void erroCausaDoPlayer\(void\) \{", "erroCausaDoPlayer"),
    extract_fn(player, r"^void player_erro_fonte\(void\) \{", "player_erro_fonte"),
    extract_fn(player, r"^int player_ultimo_erro_farol\(FarolErro \*out, int \*foiDebrid\) \{", "player_ultimo_erro_farol"),
    extract_fn(player, r"^uint64_t player_ultimo_erro_tentativa\(void\) \{", "player_ultimo_erro_tentativa"),
    extract_fn(player, r"^uint64_t player_ultimo_addon_farol\(void\) \{", "player_ultimo_addon_farol"),
    extract_fn(player, r"^int player_externo_disponivel_tentativa\(uint64_t tentativa\) \{", "player_externo_disponivel_tentativa"),
    extract_fn(player, r"^int player_externo_disponivel\(void\) \{", "player_externo_disponivel"),
    extract_fn(player, r"^int player_abrir_externo_tentativa\(uint64_t tentativa\) \{", "player_abrir_externo_tentativa"),
    extract_fn(player, r"^int player_abrir_externo\(void\) \{", "player_abrir_externo"),
    "static void app_resolver_despachar(const SDL_Event *e) {",
    extract_brace_block(app, "  if (resolver_aberto()) {", "resolver_aberto em app.c"),
    "}",
]
out.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
PY

"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -DNV_ANDROID -DNV_NDK_JNI -DNV_SHOT_HOOKS \
  -Wall -Wextra -O1 -g -Wno-deprecated-declarations -Wno-macro-redefined \
  -I"$work/include" -I"$work" -Isrc \
  -Wl,--wrap=farol_ev \
  tests/farol_android_erro.c src/resolver.c src/resolverregra.c src/farolregra.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c src/extplayer.c \
  src/addonguia.c src/addonmodo.c src/video_modo.c src/capmkv.c src/mkv.c tests/capmkv_stub.c \
  -lm -lpthread -o "$work/farol-android-erro-test"

if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/farol-android-erro-test"
else
  "$work/farol-android-erro-test"
fi
