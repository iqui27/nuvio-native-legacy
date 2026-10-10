#!/bin/bash
# Gera e CONFERE o APK Android TV de uma release vX.Y.Z, sem publicar.
#
#   bash tools/release-android.sh            # APK de release assinado
#
# Sai em build/release-<versao>/ (a mesma pasta do release-samsung.sh):
#   Nuvio-<v>-android.apk     Android 7+ (arm64-v8a + armeabi-v7a). E ESTE nome
#                             que a auto-atualizacao procura em releases/latest
#                             (atualizacao.c: sufixo "-android.apk"; debug e
#                             preview nao casam).
#   SHA256SUMS-android        juntar ao SHA256SUMS da LG/Samsung
#
# O que ele RECUSA (sai com erro):
#   - arvore suja (o APK leva o que esta no disco);
#   - appinfo.json e tools/tizen-config.xml com versoes diferentes;
#   - sem a chave de release (~/.nuvio-android/release.env, ou NUVIO_KEYSTORE*
#     no ambiente): APK de release com outra chave nao instala por cima;
#   - APK assinado com OUTRA chave que a fixada abaixo (CERT_SHA256);
#   - versionName diferente da release;
#   - arquivo de pessoa dentro do APK;
#   - faltando libmain/libSDL2/libcurl em alguma das duas ABIs;
#   - motor P2P configurado (NUVIO_P2P_MOTOR ou a pasta padrao, ver
#     tools/p2p-motor/pasta.sh) e ausente das libmain.so, ou sem os avisos de licenca.
set -eo pipefail
cd "$(dirname "$0")/.."

# Impressao SHA-256 do certificado da chave de release (gerada em 30/09/2026,
# copia de seguranca no Vaultwarden). Trocar a chave = ninguem atualiza por
# cima; so mude isto junto de um aviso para desinstalar e reinstalar.
CERT_SHA256=c3c967d4fac138de15126da01ea3ae43b52e6aa23106e336c2acc607ae0f2add

python3 tools/release.py --check-tree
tools/env.sh --require-core >/dev/null

VER=$(sed -n 's/.*"version": *"\([0-9.]*\)".*/\1/p' deploy/app/appinfo.json)
VT=$(grep -v '<?xml' tools/tizen-config.xml | sed -n 's/.*[[:space:]]version="\([0-9.]*\)".*/\1/p' | head -1)
[ -n "$VER" ] && [ "$VER" = "$VT" ] || { echo "release-android: appinfo.json ($VER) != tizen-config.xml ($VT)" >&2; exit 1; }

if [ -z "${NUVIO_KEYSTORE:-}" ] && [ -f "$HOME/.nuvio-android/release.env" ]; then
  set -a; . "$HOME/.nuvio-android/release.env"; set +a
fi
[ -n "${NUVIO_KEYSTORE:-}" ] && [ -f "$NUVIO_KEYSTORE" ] || {
  echo "release-android: sem a chave de release. Restaure ~/.nuvio-android/release.jks e release.env do Vaultwarden." >&2; exit 1; }

OUT="${NUVIO_RELEASE_OUT:-build/release-$VER}"; mkdir -p "$OUT"
rm -f "$OUT"/Nuvio-*-android*.apk "$OUT/SHA256SUMS-android"
echo "== release-android $VER (commit $(git rev-parse --short HEAD)) -> $OUT"

# Release sem conta/sync instala e so mostra "0 addons" na TV (01/10/2026,
# worktree em /private/tmp sem o local.properties do caminho padrao).
NUVIO_REQUIRE_CORE=1 bash tools/android.sh --release-only
A="build/android/Nuvio-$VER-android.apk"
[ -f "$A" ] || { echo "release-android: faltou $A (o android.sh nao gerou o release)" >&2; exit 1; }

SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
BT=$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)
CERT=$("$BT/apksigner" verify --print-certs "$A" 2>/dev/null | sed -n 's/^Signer #1 certificate SHA-256 digest: //p')
[ "$CERT" = "$CERT_SHA256" ] || { echo "release-android: $A assinado com $CERT, esperado $CERT_SHA256" >&2; exit 1; }
VN=$("$BT/aapt2" dump badging "$A" 2>/dev/null | sed -n "s/.*versionName='\([^']*\)'.*/\1/p" | head -1)
[ "$VN" = "$VER" ] || { echo "release-android: versionName $VN != $VER" >&2; exit 1; }

SEGREDO='(^|/)(trakt|addons|tmdb|mdblist|sessao|simkl[^/]*|fanart|diag-token)\.txt$|collections\.json$|catalogo-rede\.bin|local\.properties|\.env$|(^|/)(trakt|stalker|xtream|listas|discord)-p[0-9]|\.jks$|\.keystore$'
# A lista uma vez so: `unzip | grep -q` com pipefail falha quando o grep fecha
# o pipe antes de o unzip terminar.
LISTA=$(unzip -Z1 "$A")
n=$(printf '%s\n' "$LISTA" | grep -c -E "$SEGREDO" || true)
[ "$n" = "0" ] || { echo "release-android: $A leva $n arquivo(s) de pessoa:" >&2; printf '%s\n' "$LISTA" | grep -E "$SEGREDO" >&2; exit 1; }
for abi in arm64-v8a armeabi-v7a; do
  for so in libmain.so libSDL2.so libcurl.so libjpeg.so libwebp.so libffmpegJNI.so; do
    case $'\n'"$LISTA"$'\n' in *$'\n'"lib/$abi/$so"$'\n'*) ;; *) echo "release-android: faltou lib/$abi/$so" >&2; exit 1;; esac
  done
done

# Motor P2P (dono, 1.8): com a pasta do motor achada, o APK TEM de levar o motor
# nas duas ABIs e os avisos de licenca; o android.sh ja recusou o contrario, isto
# e a segunda conferencia sobre o arquivo que vai para a release.
. tools/p2p-motor/pasta.sh
nv_p2p_resolver android
case $'\n'"$LISTA"$'\n' in *$'\n'"assets/licencas/p2p-avisos.txt"$'\n'*) ;; *) echo "release-android: faltou assets/licencas/p2p-avisos.txt" >&2; exit 1;; esac
if [ -n "$NV_P2P_DIR" ]; then
  for abi in arm64-v8a armeabi-v7a; do
    m=$(unzip -p "$A" "lib/$abi/libmain.so" | strings | grep -c 'Nuvio Engine/' || true)
    [ "$m" -ge 1 ] || { echo "release-android: $A lib/$abi/libmain.so sem o motor P2P" >&2; exit 1; }
  done
  echo "release-android: motor P2P (nuvio-engine) nas duas ABIs"
else
  echo "release-android: ATENCAO, APK SEM motor P2P (NUVIO_P2P_MOTOR=none ou sem pasta)" >&2
fi

cp "$A" "$OUT/"
( cd "$OUT" &&
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 Nuvio-*-android.apk
  else sha256sum Nuvio-*-android.apk; fi > SHA256SUMS-android )
python3 tools/release.py --check-tree
echo
echo "== pronto, sem publicar:"
ls -la "$OUT"/Nuvio-*-android.apk "$OUT/SHA256SUMS-android"
echo "Release completa (LG + Android + Samsung + SHA256SUMS + Homebrew): bash tools/release.sh $VER"
