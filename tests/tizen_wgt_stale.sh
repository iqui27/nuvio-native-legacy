#!/bin/bash
# tizen-wgt.sh nunca pode empacotar um build velho. Caso real do 2.0.3: o
# tools/tizen.sh falhou no link (simbolo indefinido), mas o build/tizen da
# rodada anterior continuava la com o .nuvio-build-stamp dele, que batia com o
# index.wasm velho — e o tizen-wgt.sh saiu rc=0 com um .wgt do codigo antigo.
#
# Sem emscripten: o emcc e falso (emsdk_env.sh de mentira). Prova:
#   A. tizen.sh falha  => build antigo nao sobrevive => tizen-wgt.sh rc!=0, sem .wgt
#   B. wasm/js mais velhos que o inicio do build (stamp) => rc!=0, sem .wgt
#   C. build fresco e coerente => rc=0 e o .wgt sai
set -euo pipefail
cd "$(dirname "$0")/.."
REPO=$(pwd)

TMP=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-wgt-stale.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
FAIL() { echo "tizen_wgt_stale: FAIL $*" >&2; exit 1; }

command -v zip >/dev/null || { echo "tizen_wgt_stale: SKIP sem zip"; exit 0; }

SB="$TMP/repo"
mkdir -p "$SB/src" "$SB/deploy" "$TMP/emsdk/bin"
cp -R tools "$SB/tools"
cp -R deploy/app "$SB/deploy/app"
echo 'int x;' > "$SB/src/a.c"
cat > "$TMP/emsdk/emsdk_env.sh" <<EOS
export PATH="$TMP/emsdk/bin:\$PATH"
EOS
printf '#!/bin/sh\necho "emcc falso: link falhou" >&2\nexit 1\n' > "$TMP/emsdk/bin/emcc"
chmod +x "$TMP/emsdk/bin/emcc"
cat > "$TMP/p.properties" <<'EOS'
NUVIO_SUPABASE_URL=https://config.example.invalid
NUVIO_SUPABASE_ANON_KEY=test-anon-key
TV_LOGIN_WEB_BASE_URL=https://login.example.invalid
EOS
export NUVIO_PROPERTIES="$TMP/p.properties" EMSDK_DIR="$TMP/emsdk"
export NUVIO_WGT_NOME=Teste NUVIO_WGT_MANTER=0

sha() { shasum -a 256 "$@" | awk '{print $1}'; }

# Monta um build/tizen "bom" com stamp coerente. $1 = build-start (epoch).
fazer_build() {
  local ini="$1" d="$SB/build/tizen"
  rm -rf "$d"; mkdir -p "$d"
  echo '<html></html>' > "$d/index.html"
  echo 'var a=1;' > "$d/index.js"
  echo 'dec' > "$d/decodificador.js"
  echo 'wasm' > "$d/index.wasm"
  local fp; fp=$(cd "$SB" && tools/env.sh --require-core | shasum -a 256 | awk '{print $1}')
  { printf 'format=1\nconfig-fingerprint=%s\nwasm-sha256=%s\n' "$fp" "$(sha "$d/index.wasm")"
    [ -z "$ini" ] || printf 'build-start=%s\n' "$ini"
  } > "$d/.nuvio-build-stamp"
}
wgt() { ( cd "$SB" && rm -f Teste.wgt && bash tools/tizen-wgt.sh >"$TMP/out" 2>"$TMP/err" ); }

# A. build antigo valido + tizen.sh que falha
fazer_build ""
if ( cd "$SB" && bash tools/tizen.sh >"$TMP/tout" 2>"$TMP/terr" ); then FAIL "A: tizen.sh com emcc falso devia falhar"; fi
if wgt; then FAIL "A: tizen-wgt.sh empacotou build velho depois de tizen.sh falhar"; fi
[ ! -e "$SB/Teste.wgt" ] || FAIL "A: .wgt foi produzido"
grep -q 'tizen.sh' "$TMP/err" || FAIL "A: mensagem sem referencia ao tizen.sh"

# B. arquivos mais velhos que o inicio do build
fazer_build "$(( $(date +%s) + 3600 ))"
if wgt; then FAIL "B: tizen-wgt.sh aceitou wasm/js mais velhos que o inicio do build"; fi
[ ! -e "$SB/Teste.wgt" ] || FAIL "B: .wgt foi produzido"
grep -q 'build-start\|mais velho' "$TMP/err" || FAIL "B: mensagem sem explicar o atraso"

# C. build fresco
fazer_build "$(( $(date +%s) - 60 ))"
wgt || { cat "$TMP/err" >&2; FAIL "C: build fresco recusado"; }
[ -s "$SB/Teste.wgt" ] || FAIL "C: .wgt nao saiu"

echo "tizen_wgt_stale: OK"
