#!/usr/bin/env python3
"""Run the production credential extraction on object and serialized JSON."""
import os
import pathlib
import subprocess
import tempfile
src = subprocess.check_output(['git', 'show', os.environ['SYNC_REV'] + ':src/sync.c'], text=True) if os.environ.get('SYNC_REV') else pathlib.Path('src/sync.c').read_text()
a = src.index('    if (!js_bruto(p, f, "credential_json"')
b = src.index('    else if (!strcmp(prov, "tmdb"))', a)
block = src[a:b]
code = r'''
#include "js.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(const char *p) {
  const char *f = p + strlen(p);
  char cred[900], prov[48] = "trakt", traktTok[300] = "";
  int temTraktRem = 0;
  do {
''' + block + r'''
  } while (0);
  assert(temTraktRem && !strcmp(traktTok, "fixture"));
}
int main(void) {
  check("{\"credential_json\":{\"access_token\":\"fixture\"}}");
  check("{\"credential_json\":\"{\\\"access_token\\\":\\\"fixture\\\"}\"}");
  puts("Trakt object/serialized credentials: PASS");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = pathlib.Path(tmp)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', '-Isrc', 'src/js.c', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
