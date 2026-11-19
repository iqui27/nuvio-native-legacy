#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-lg-selection.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/lg_subtitle_selection.c -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -Wno-missing-field-initializers -o "$work/t"
# Compile the actual LG native-selection entry point with an offline API spy.
python3 - "$work/native.c" <<'PY'
from pathlib import Path
import sys
source = Path('src/video.c').read_text()
start = source.index('void video_escolher_legenda(int i) {', source.index('void video_escolher_legenda(int i) {') + 1)
end = source.index('\n}\n', start) + 3
Path(sys.argv[1]).write_text(r'''
#include "video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int dtsSessao, ligado = 1, legAtual, posSeg, selects, enables;
static char midia[] = "test", legUrlAtual[8];
static VideoFaixa track = {.numero = -2, .stream_index = -1};
const VideoFaixa *video_legenda(int i) { return i == 0 ? &track : NULL; }
static void dts_playback_subtitle(int s, int i) { (void)s; (void)i; }
static void dts_playback_seek(int s, int p) { (void)s; (void)p; }
static void soLog(void) {}
static void aplicarEstilo(void) {}
static void chamar(const char *method, const char *body, void (*cb)(void)) {
  (void)cb;
  if (!strcmp(method, "selectTrack")) selects++;
  if (!strcmp(method, "setSubtitleEnable")) enables = strstr(body, "true") != NULL;
}
''' + source[start:end] + r'''
int main(void) {
  enables = 1;
  video_escolher_legenda(0);
  assert(!selects && !enables && legAtual == -1);
  track.numero = 3;
  video_escolher_legenda(0);
  assert(selects == 1 && enables && legAtual == 0);
  puts("LG native subtitle selection: PASS");
}
''')
PY
cc "$work/native.c" -Isrc -o "$work/native"
rc=0
"$work/native" || rc=1
"$work/t" synthetic || rc=1
"$work/t" external || rc=1
exit "$rc"
