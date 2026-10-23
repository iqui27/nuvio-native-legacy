#!/bin/bash
# #510: recover embedded text tracks omitted by LG sourceInfo, without network.
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-lg-subs.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
python3 - "$DIR/test.c" <<'PY'
from pathlib import Path
import sys
source = Path('src/video.c').read_text()
assert 'nLeg = faixasmkv_acrescentar_texto(faixaLeg, nLeg, NV_FAIXA_MAX, fx, n);' in source, 'LG probe never publishes omitted embedded text tracks'
Path(sys.argv[1]).write_text(r'''
#include "faixasmkv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
const char *i18n(const char *s) { return s; }
int main(void) {
  MkvFaixa fx[] = {
    {.tipo=1, .codec="V_MPEG4/ISO/AVC"},
    {.tipo=17, .codec="S_TEXT/ASS", .idioma="eng", .nome="Full"},
    {.tipo=17, .codec="S_HDMV/PGS", .idioma="jpn"},
    {.tipo=17, .codec="S_TEXT/SSA", .idioma="spa"},
    {.tipo=17, .codec="S_TEXT/UTF8", .idioma="fra"},
    {.tipo=17, .codec="S_TEXT/WEBVTT", .idioma="deu"},
    {.tipo=17, .codec="S_VOBSUB", .idioma="ita"}
  };
  VideoFaixa leg[8] = {0};
  int n = faixasmkv_acrescentar_texto(leg, 0, 8, fx, 7);
  assert(n == 4);
  assert(leg[0].ordinalMkv == 0 && leg[1].ordinalMkv == 2);
  assert(leg[2].ordinalMkv == 3 && leg[3].ordinalMkv == 4);
  assert(!strcmp(leg[0].idioma, "eng") && strstr(leg[0].rotulo, "Full"));
  for (int i = 0; i < n; i++) {
    assert(leg[i].numero < 0 && leg[i].stream_index == -1);
    assert(faixasmkv_overlay(leg[i].codec, 1));
    for (int j = 0; j < i; j++) assert(leg[i].numero != leg[j].numero);
  }
  assert(faixasmkv_acrescentar_texto(leg, n, 8, fx, 7) == n);
  assert(faixasmkv_acrescentar_texto(leg, 0, 1, fx, 7) == 1);
  // One native SRT track: retain its identity and append only missing tracks.
  memset(leg, 0, sizeof leg);
  leg[0].numero = 3; leg[0].ordinalMkv = 3;
  strcpy(leg[0].codec, "S_TEXT/UTF8");
  assert(faixasmkv_acrescentar_texto(leg, 1, 8, fx, 7) == 4);
  assert(leg[0].numero == 3 && leg[0].ordinalMkv == 3);
  // Keep the existing image-only API image-only.
  memset(leg, 0, sizeof leg);
  assert(faixasmkv_acrescentar_imagem(leg, 0, 8, fx, 7) == 1);
  assert(!strcmp(leg[0].codec, "S_HDMV/PGS") && leg[0].numero == -1);
  puts("LG omitted embedded text subtitles: PASS");
  return 0;
}
''')
PY
cc -Isrc "$DIR/test.c" src/faixasmkv.c src/mkv.c src/linguas.c src/rede.c src/redeurl.c src/audioinfo.c \
  -o "$DIR/test" -O1 -g -Wall -I/opt/homebrew/include -Wno-deprecated-declarations
"$DIR/test"
