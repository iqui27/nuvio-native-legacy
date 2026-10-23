#!/bin/bash
# #524: actual on-demand title resolver and existing known/uncertain detail paths.
# Run each scenario separately so the baseline shows every RED and control.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-title-identity524.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/title_identity524.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/teste" -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
if [ "$#" -gt 0 ]; then "$dir/teste" "$1"; exit; fi
failed=0
for scenario in tmdb-tv episode-id movie-control known-series-control uncertain-detail-control \
  tmdb-series-alias tmdb-show-alias cached-opposite-movie cached-opposite-series \
  cached-uncertain cached-channel cached-show-control opposite-response-type \
  different-response-id missing-series-no-movie context-reset malformed-episode-control \
  seed-opposite-movie seed-opposite-series seed-matching-show-control \
  seed-current-session seed-stale-session seed-cached-stale-session \
  native-tmdb-canonical-alias native-tmdb-wrong-id native-tmdb-wrong-type; do
  if "$dir/teste" "$scenario"; then printf 'PASS %s\n' "$scenario";
  else printf 'FAIL %s\n' "$scenario"; failed=1; fi
done
exit "$failed"
