#!/bin/bash
# #565: PenguPlay fixture — manifest probe and sign-in stream. No network.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pengu565.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections -Wall -Wno-unused-function -Wno-macro-redefined)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" -DPENGU_ADDONS tests/pengu.c src/js.c src/watchstate.c -lpthread -o "$dir/manifest"
"$dir/manifest"

# Includes streams.c; everything outside the offer filter is dead-stripped.
srcs=(tests/pengu.c src/stream_parse.c src/streams.c src/fontesmortas.c src/js.c src/badges.c src/streamfit.c src/naovideo.c)
cc "${flags[@]}" "${srcs[@]}" -o "$dir/streams"
"$dir/streams"

# 2.0.6 carried the filter that failed this source. Prove the same source text
# fails under a copy of that predicate, then the real compiled WGT path passes.
python3 - "$dir" <<'PY'
import subprocess, sys
body = subprocess.check_output(["git", "show", "v2.0.6:src/streams.c"], text=True)
marker = "static int fonteOferecida(const Stream *s) {"
pos = body.index(marker)
end = body.index("}\n", pos) + 3
with open(sys.argv[1] + "/old-565.c", "w") as out:
    out.write(body[pos:end].replace("fonteOferecida(", "fonteOferecida206("))
PY
cat >"$dir/pengu206.c" <<C
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int p2p_ativo(void) { return 0; }
int debrid_ativo(void) { return 0; }
C
cat "$dir/old-565.c" >>"$dir/pengu206.c"
cat >>"$dir/pengu206.c" <<'C'
static char *fixture(const char *name) {
  char path[160];
  snprintf(path, sizeof path, "tests/fixtures/pengu/%s.json", name);
  FILE *f = fopen(path, "rb");
  assert(f); assert(fseek(f, 0, SEEK_END) == 0);
  long n = ftell(f); assert(n >= 0 && fseek(f, 0, SEEK_SET) == 0);
  char *body = calloc((size_t)n + 1, 1);
  assert(body && fread(body, 1, (size_t)n, f) == (size_t)n); fclose(f);
  return body;
}
int main(void) {
  char *body = fixture("stream");
  const char *hint = "\"notWebReady\":false";
  char *value = strstr(body, hint); assert(value);
  value += strlen(hint) - 5; memcpy(value, "true ", 5);
  Stream *v = NULL;
  assert(stream_extrair(body, "PenguPlay", &v) == 1);
  assert(v[0].notWebReady && v[0].url[0]);
  int offered = fonteOferecida206(v);
  free(v); free(body);
  if (offered) return 0;

  // Captured authenticated response, same path: 27 http(s) notWebReady
  // streams the 2.0.6 predicate hides with P2P off.
  body = fixture("stream-auth-movie_tt0111161");
  v = NULL;
  assert(stream_extrair(body, "PenguPlay", &v) == 27);
  int hidden = 0;
  for (int i = 0; i < 27; i++) hidden += !fonteOferecida206(&v[i]);
  free(v); free(body);
  printf("pengu206: 2.0.6 hides %d/27 public streams (expected FAIL)\n", hidden);
  return hidden == 27 ? 1 : 0;
}
C
cc "${flags[@]}" "$dir/pengu206.c" src/stream_parse.c src/fontesmortas.c src/js.c src/badges.c src/streamfit.c src/naovideo.c src/fontefiltro.c src/linguas.c -o "$dir/pengu206"
if "$dir/pengu206"; then
  echo "FAIL: copied v2.0.6 predicate now passes" >&2
  exit 1
fi

# Same fixture through the real current streams.c selected as the .wgt path.
# The WGT check needs only the selected predicates and stream parser.
cc "${flags[@]}" -D__EMSCRIPTEN__ "${srcs[@]}" -o "$dir/streams_wgt"
"$dir/streams_wgt"
echo "pengu.sh: ok"
