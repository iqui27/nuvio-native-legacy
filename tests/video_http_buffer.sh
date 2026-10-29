#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
source = Path('src/video.c').read_text()
builder = source.split('static void montarCarga(', 1)[1].split('\nstatic int tocarInterno(', 1)[0]
Path(sys.argv[1], 'builder.c').write_text('''#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "app_id.h"
#include "cacheboost.h"
static void montarCarga(''' + builder + r'''
int main(void) {
  char automatic[8192], actual[8192], expected[8192];
  const char *url = "https://example.org/movie.mkv";
  snprintf(expected, sizeof expected,
    "{\"payload\":{\"option\":{\"useSeekableRanges\":true,"
    "\"appId\":\"" NV_APP_ID "\",\"bufferControl\":{\"userBufferCtrl\":false},"
    "\"windowId\":\"window_id_dummy\"}},\"uri\":\"%s\",\"type\":\"media\"}", url);
  montarCarga(automatic, sizeof automatic, url, 0, "", "", "window_id_dummy", 0, 4096);
  assert(!strcmp(automatic, expected));
  for (int preset = 1; preset <= 3; preset++) {
    unsigned bytes = (32U << preset) * 1024U * 1024U;
    char fields[256];
    snprintf(fields, sizeof fields,
      "\"bufferControl\":{\"userBufferCtrl\":true,\"preBufferByte\":%u,"
      "\"bufferMinLevel\":%u,\"bufferMaxLevel\":%u}", bytes / 8, bytes / 4, bytes);
    montarCarga(actual, sizeof actual, url, 0, "", "", "window_id_dummy", preset, 4096);
    assert(strstr(actual, fields));
    if (preset == 2) printf("128 MiB: %s\n", actual);
    for (int mode = 1; mode <= 2; mode++) {
      montarCarga(expected, sizeof expected, url, mode, "", "", "window_id_dummy", 0, 4096);
      montarCarga(actual, sizeof actual, url, mode, "", "", "window_id_dummy", preset, 4096);
      assert(!strcmp(actual, expected));
    }
  }
  montarCarga(actual, sizeof actual, url, 0, "", "", "window_id_dummy", 3, 320);
  assert(strstr(actual, "\"bufferMaxLevel\":134217728"));
  for (int freeMb = -1; freeMb <= 167; freeMb += 168) {
    montarCarga(actual, sizeof actual, url, 0, "", "", "window_id_dummy", 3, freeMb);
    assert(!strcmp(actual, automatic));
  }
  montarCarga(expected, sizeof expected, "file:///movie.mkv", 0, "", "", "window_id_dummy", 0, 4096);
  montarCarga(actual, sizeof actual, "file:///movie.mkv", 0, "", "", "window_id_dummy", 2, 4096);
  assert(!strcmp(actual, expected));
  puts("video_http_buffer: ok");
}
''')
PY
cc -std=gnu11 -Wall -Wextra -Werror -Isrc "$work/builder.c" src/cacheboost.c -pthread -lm -o "$work/test"
"$work/test"
