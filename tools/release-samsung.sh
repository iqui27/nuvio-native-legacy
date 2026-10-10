#!/bin/bash
# Gera e CONFERE todos os pacotes Samsung de uma release vX.Y.Z, sem publicar.
#
#   bash tools/release-samsung.sh            # .wgt + os 4 .tpk + anexos
#   bash tools/release-samsung.sh --so-tpk   # pula o .wgt (Emscripten)
#
# Sai em build/release-<versao>/ com os nomes que vao para o GitHub:
#   NuvioTV-<v>-tizen.wgt                     WASM, Tizen 5.5+
#   Nuvio-<v>-NuvioTpk40.tpk                  nativo, Tizen 4.0-5.5 (2018-2020)
#   Nuvio-<v>-NuvioTpk60.tpk                  nativo, Tizen 6.0 (2021)
#   Nuvio-<v>-NuvioTpk65.tpk                  nativo, Tizen 6.5-7 (2022-2023)
#   Nuvio-<v>-NuvioTpk.tpk                    nativo, Tizen 8-9 (2024+)
#   libnuvio-<v>-tpk-arm.so / -tpk40-arm.so   auto-atualizacao (#181): SEM ESTES
#                                             ANEXOS a TV nao se atualiza sozinha
#   SHA256SUMS-samsung                        juntar ao SHA256SUMS da LG
#
# O que ele RECUSA (sai com erro, nada fica pela metade sem aviso):
#   - arvore suja (todo script compila a arvore de trabalho; WIP de outra
#     sessao ja entrou num pacote em 27/09/2026);
#   - appinfo.json e tools/tizen-config.xml com versoes diferentes;
#   - qualquer arquivo de pessoa dentro de qualquer pacote;
#   - pacote 4/5 com o privilegio Partner drminfo (a TV do rawldon recusava
#     com 118014; Public funciona desde 29/09/2026);
#   - lib do 4/5 com TLS ou sem DT_HASH (tests/tpk40_tls.sh);
#   - .tpk com versao de manifesto diferente da release.
#
# Depois dele: conferir o que imprimiu, juntar com o .ipk da LG e publicar pela
# skill samsung-release (gh release create com TUDO de build/release-<v>/).
set -eo pipefail
cd "$(dirname "$0")/.."

SO_TPK=0; [ "$1" = "--so-tpk" ] && SO_TPK=1

python3 tools/release.py --check-tree
tools/env.sh --require-core >/dev/null

VER=$(sed -n 's/.*"version": *"\([0-9.]*\)".*/\1/p' deploy/app/appinfo.json)
VT=$(grep -v '<?xml' tools/tizen-config.xml | sed -n 's/.*[[:space:]]version="\([0-9.]*\)".*/\1/p' | head -1)
[ -n "$VER" ] && [ "$VER" = "$VT" ] || { echo "release-samsung: appinfo.json ($VER) != tizen-config.xml ($VT)" >&2; exit 1; }
OUT="${NUVIO_RELEASE_OUT:-build/release-$VER}"; mkdir -p "$OUT"
rm -f "$OUT"/Nuvio-*-NuvioTpk*.tpk "$OUT"/NuvioTV-*-tizen.wgt "$OUT"/libnuvio-*-tpk*.so "$OUT/SHA256SUMS-samsung"
echo "== release-samsung $VER (commit $(git rev-parse --short HEAD)) -> $OUT"

# Credenciais: a lista de exclusao dos scripts e INTENCAO; isto e o fato.
SEGREDO='(^|/)(trakt|addons|tmdb|mdblist|sessao|simkl[^/]*|fanart|diag-token)\.txt$|collections\.json$|catalogo-rede\.bin|local\.properties|\.env$|(^|/)(trakt|stalker|xtream|listas)-p[0-9]|(^|/)conta-[^/]*\.txt(\.tmp)?$'
confere() {  # $1 pacote (zip)
  local n; n=$(unzip -Z1 "$1" | grep -c -E "$SEGREDO" || true)
  [ "$n" = "0" ] || { echo "release-samsung: $1 leva $n arquivo(s) de pessoa:" >&2; unzip -Z1 "$1" | grep -E "$SEGREDO" >&2; exit 1; }
}

# 1. .tpk (os quatro de uma vez; tools/tpk.sh reescreve os manifestos)
bash tools/tpk.sh
bash tests/tpk40_tls.sh
for H in NuvioTpk40 NuvioTpk60 NuvioTpk65 NuvioTpk; do
  T="build/tpk/Nuvio-$VER-$H.tpk"
  [ -f "$T" ] || { echo "release-samsung: faltou $T" >&2; exit 1; }
  confere "$T"
  MV=$(unzip -p "$T" tizen-manifest.xml | grep '<manifest' | sed -n 's/.*[[:space:]]version="\([0-9.]*\)".*/\1/p' | head -1)
  [ "$MV" = "$VER" ] || { echo "release-samsung: $T diz versao $MV" >&2; exit 1; }
  if [ "$H" = NuvioTpk40 ] && unzip -p "$T" tizen-manifest.xml | grep -q drminfo; then
    echo "release-samsung: $T pede drminfo (Partner). Tire do manifesto: Public funciona e Partner recusa em parte das TVs." >&2; exit 1
  fi
  cp "$T" "$OUT/"
done
cp "build/tpk/libnuvio-$VER-tpk-arm.so" "build/tpk/libnuvio-$VER-tpk40-arm.so" "$OUT/"
# O pacote 6+ tem de levar a MESMA .so do anexo, e o 4/5 a dele: a auto-
# atualizacao compara versao, nao bytes, mas um anexo errado seria carregado.
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-samsung-check.XXXXXXXX"); trap 'rm -rf "$D"' EXIT
unzip -qo "$OUT/Nuvio-$VER-NuvioTpk.tpk" lib/libnuvio.so -d "$D/6"
unzip -qo "$OUT/Nuvio-$VER-NuvioTpk40.tpk" lib/libnuvio.so -d "$D/4"
cmp -s "$D/6/lib/libnuvio.so" "$OUT/libnuvio-$VER-tpk-arm.so"   || { echo "release-samsung: anexo 6+ != .so do pacote" >&2; exit 1; }
cmp -s "$D/4/lib/libnuvio.so" "$OUT/libnuvio-$VER-tpk40-arm.so" || { echo "release-samsung: anexo 4/5 != .so do pacote" >&2; exit 1; }

# Motor P2P (dono, 1.8): pasta do motor achada => os .tpk 6+ levam libnuvio_engine.so
# (tpk.sh ja conferiu) e a libnuvio.so deles abre essa .so por dlopen; o 4/5 e o
# anexo dele nunca. NUVIO_P2P_MOTOR=none faz a release sair sem motor, avisando.
. tools/p2p-motor/pasta.sh
nv_p2p_resolver tpk
if [ -n "$NV_P2P_DIR" ]; then
  # contagem em vez de `grep -q`: com pipefail o grep -q fecha o pipe cedo e o SIGPIPE derruba o strings.
  n6=$(strings "$OUT/libnuvio-$VER-tpk-arm.so" | grep -c 'nuvio_engine_api_version' || true)
  n4=$(strings "$OUT/libnuvio-$VER-tpk40-arm.so" | grep -c 'nuvio_engine_api_version' || true)
  [ "$n6" -ge 1 ] || { echo "release-samsung: libnuvio-$VER-tpk-arm.so (6+) sem o motor P2P" >&2; exit 1; }
  [ "$n4" = 0 ] || { echo "release-samsung: libnuvio-$VER-tpk40-arm.so (4/5) leva o motor P2P" >&2; exit 1; }
  echo "== motor P2P: .tpk 6+ com libnuvio_engine.so, 4/5 sem (como deve)"
else
  echo "== ATENCAO: .tpk sem motor P2P (NUVIO_P2P_MOTOR=none ou sem pasta)" >&2
fi

# 2. .wgt
if [ "$SO_TPK" = 0 ]; then
  bash tools/tizen.sh
  NUVIO_WGT_NOME="NuvioTV-$VER-tizen" bash tools/tizen-wgt.sh
  confere "NuvioTV-$VER-tizen.wgt"
  mv "NuvioTV-$VER-tizen.wgt" "$OUT/"
fi

( cd "$OUT" || exit
  checksum_files=(Nuvio-*-NuvioTpk*.tpk libnuvio-*-tpk*.so)
  [ "$SO_TPK" = 1 ] || checksum_files+=(NuvioTV-*-tizen.wgt)
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "${checksum_files[@]}"
  else sha256sum "${checksum_files[@]}"; fi > SHA256SUMS-samsung )
python3 tools/release.py --check-tree
echo
echo "== pronto, sem publicar. Conteudo de $OUT:"
ls -la "$OUT"
echo "Release completa (LG + Android + Samsung + SHA256SUMS + Homebrew): bash tools/release.sh $VER"
