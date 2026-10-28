#!/usr/bin/env python3
"""Exercise the renderer's real metadata join without GL or an app build."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/detail.c').read_text()
if 'static void epMeta(' in source:
    start = source.index('static void epMeta(')
    end = source.index('\n}\n', start) + 3
    helper = source[start:end]
else:
    # Baseline expression, so the regression fails on the actual empty HDR bug.
    helper = '''static void epMeta(char *dst, size_t n, const char *a, const char *b, const char *c) {
      snprintf(dst,n,"%s · %s · %s",a?a:"",b?b:"",c?c:"");
    }'''
program = r'''
#include <stdio.h>
#include <string.h>
#include <assert.h>
''' + helper + r'''
int main(void) {
  const char *parts[]={"1080p","HDR10","DTS-HD MA 5.1"};
  for(int mask=0;mask<8;mask++) {
    char expected[200]="", actual[200];
    for(int i=0;i<3;i++) if(mask&(1<<i)) {
      if(expected[0]) strcat(expected," · ");
      strcat(expected,parts[i]);
    }
    epMeta(actual,sizeof actual,mask&1?parts[0]:NULL,mask&2?parts[1]:"",mask&4?parts[2]:NULL);
    if(strcmp(actual,expected)) { fprintf(stderr,"FAIL mask=%d: [%s], expected [%s]\n",mask,actual,expected); return 1; }
  }
  char small[5]={0}; epMeta(small,sizeof small,"1080p","HDR10","AAC");
  assert(small[4]==0); epMeta(NULL,0,NULL,NULL,NULL);
  puts("PASS: eight metadata combinations and bounded output");
}
'''
with tempfile.TemporaryDirectory(prefix='episode-meta-') as tmp:
    c = Path(tmp) / 'check.c'; binary = Path(tmp) / 'check'
    c.write_text(program)
    subprocess.run(['cc', '-std=c99', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
