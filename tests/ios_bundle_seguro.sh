#!/bin/bash
# The iOS bundle never carries personal files. A working tree keeps ignored
# local files next to the art (deploy/app/art/addons.txt with add-on URLs,
# trakt.txt, ajustes.txt, local.properties...). This plants decoys with those
# ignored names (only where no real file exists, removed on exit), builds,
# and fails if a decoy, any file git does not track, or anything named like a
# key/token/secret/properties/.env file ends up in Nuvio.app.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_IOS_BUILD="${NUVIO_IOS_BUILD:-build/ios-test}"
MARCA="nuvio-isca-$$-nao-pode-ir-no-app"
ISCAS=(deploy/app/art/addons.txt deploy/app/art/trakt.txt deploy/app/art/ajustes.txt deploy/app/art/local.properties)
PLANTADAS=()
limpar() { for f in "${PLANTADAS[@]+"${PLANTADAS[@]}"}"; do rm -f "$f"; done; }
trap limpar EXIT
for f in "${ISCAS[@]}"; do
  git check-ignore -q "$f" || { echo "FAIL: $f is not git-ignored, not a valid decoy"; exit 1; }
  [ -e "$f" ] && continue   # a real personal file: never touched
  printf '%s\n' "$MARCA" > "$f"
  PLANTADAS+=("$f")
done

SEG_LOG="${NUVIO_IOS_SEGURO_LOG:-/tmp/nuvio-ios-seguro.log}"
bash tools/ios.sh --build >"$SEG_LOG" 2>&1 || { tail -30 "$SEG_LOG"; echo "FAIL: build"; exit 1; }
APP="$NUVIO_IOS_BUILD/Nuvio.app"
[ -f "$APP/art/00.jpg" ] || { echo "FAIL: art missing from the bundle"; exit 1; }

falhou=0
if grep -rl "$MARCA" "$APP" >/dev/null 2>&1; then
  echo "FAIL: decoy content inside the bundle:"; grep -rl "$MARCA" "$APP" | sed "s|^$APP/|  |"; falhou=1
fi
# Everything under art/ and fonts/ must be a file git tracks.
git ls-files deploy/app/art deploy/app/fonts | sort > "$NUVIO_IOS_BUILD/rastreados.txt"
fora=$(cd "$APP" && find art fonts -type f | sed 's|^|deploy/app/|' | sort | comm -23 - "$OLDPWD/$NUVIO_IOS_BUILD/rastreados.txt")
if [ -n "$fora" ]; then echo "FAIL: files git does not track in the bundle:"; printf '  %s\n' $fora; falhou=1; fi
# Names that look like credentials anywhere in the bundle (art named
# "secret-gold.png" is an achievement badge, so media files are exempt).
nomes=$(cd "$APP" && find . -type f \( -iname '*.properties' -o -iname '.env*' -o -iname '*token*' -o -iname '*secret*' \
  -o -iname '*credential*' -o -iname '*.key' -o -iname '*apikey*' \) \
  ! -iname '*.png' ! -iname '*.jpg' ! -iname '*.webp' ! -iname '*.svg' ! -iname '*.gif')
if [ -n "$nomes" ]; then echo "FAIL: credential-like names in the bundle:"; printf '  %s\n' $nomes; falhou=1; fi
[ "$falhou" = 0 ] || exit 1
echo "PASS: bundle carries only tracked art and fonts (${#PLANTADAS[@]} decoys planted, none shipped)"
