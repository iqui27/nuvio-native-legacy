#!/usr/bin/env python3
"""Exercise the production add-on label resolver with a discovery transport hash."""
import pathlib
import subprocess
import tempfile
src = pathlib.Path('src/addons.c').read_text()
a = src.index('const char *addons_nome_por_id(')
b = src.index('\nconst char *addons_base(', a)
code = '''
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int nAddon = 1;
static struct { char id[96], nome[64], base[256]; } addon[1] = {
  {"manifest.id", "Xperience", "https://fixture.invalid/manifest.json"}
};
''' + src[a:b] + '''
int main(void) {
  unsigned h = 2166136261u;
  for (const unsigned char *p = (const unsigned char *)addon[0].base; *p; p++) h = (h ^ *p) * 16777619u;
  char key[16]; snprintf(key, sizeof key, "#%08x", h);
  assert(!strcmp(addons_nome_por_id(key), "Xperience"));
  assert(!strcmp(addons_nome_por_id("manifest.id"), "Xperience"));
  assert(!addons_nome_por_id("#ffffffff")[0]);
  puts("Source labels resolve manifest IDs and discovery hashes: PASS");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p = pathlib.Path(tmp)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
