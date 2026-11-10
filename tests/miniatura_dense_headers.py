"""Production dense metadata candidates must never claim or download a target."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/miniatura_android.c").read_text()

def extract(name):
    match = re.search(r"static [^;{}\n]+\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", source)
    assert match, name
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]

prefix = r'''
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "miniatura_coverage.h"
#define GRADE_MS 10000L
typedef struct { unsigned char *jpg; } Tile;
typedef struct { int n; Tile *tiles; unsigned char *tentado; unsigned char *slotState; } Job;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static long playheadMs;
'''
checks = r'''
int main(void) {
  Tile tiles[900] = {0}; unsigned char attempted[900] = {0}, states[900] = {0};
  Job j = {.n=12, .tiles=tiles, .tentado=attempted, .slotState=states};
  long out[66]; for(int i=0;i<66;i++)out[i]=-42;
  assert(sondasDensas(&j,out,64)==8);
  const long expected[] = {10000,20000,40000,50000,70000,80000,100000,110000};
  assert(!memcmp(out,expected,sizeof expected));
  for(int i=0;i<900;i++)assert(!attempted[i] && !tiles[i].jpg);
  assert(out[8]==-42);
  attempted[1]=1; tiles[2].jpg=(unsigned char*)"cached"; states[2]=MINI_COVER_COVERED;
  assert(sondasDensas(&j,out,64)==6 && out[0]==40000);
  assert(attempted[1]==1 && tiles[2].jpg);
  assert(sondasDensas(&j,out,1)==1 && out[0]==40000);
  assert(sondasDensas(&j,out,0)==0);
  playheadMs=999999999;
  assert(sondasDensas(&j,out,64)==6 && out[0]==110000 && out[1]==100000);
  j.n=900; playheadMs=600000;
  for(int i=0;i<66;i++)out[i]=-42;
  assert(sondasDensas(&j,out,900)==64 && out[64]==-42 && out[65]==-42);
  for(int i=0;i<64;i++) {
    assert(out[i]>=0 && out[i]<900*GRADE_MS && out[i]/GRADE_MS%3!=0);
    for(int k=0;k<i;k++)assert(out[k]!=out[i]);
  }
  puts("PASS: dense header candidates bounded, unique, read-only and separate from coarse payload targets");
}
'''
with tempfile.TemporaryDirectory(prefix="nuvio-dense-headers-") as tmp:
    p = Path(tmp)
    (p / "test.c").write_text(prefix + extract("livreDireto") + extract("sondasDensas") + checks)
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I", str(root / "src"),
                    str(p / "test.c"), "-pthread", "-o", str(p / "test")], check=True)
    subprocess.run([str(p / "test")], check=True)
