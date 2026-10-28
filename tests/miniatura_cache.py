"""Exercise production NVM3 checkpoints/resume, including interrupted appends."""
from pathlib import Path
import subprocess
import tempfile
import re

root = Path(__file__).resolve().parents[1]
text = (root / "src/miniatura_android.c").read_text()


def extract(signature):
    start = re.search(re.escape(signature) + r"[^;{}]*\)\s*\{", text).start()
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


prefix = r'''
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#define CACHE_TTL_S 7200
#define M_PRONTO 1
typedef struct { unsigned char *jpg; int n; long pts; unsigned char prov, salvo; unsigned revision; } Tile;
typedef struct { int n, feitos, direto, appendPronto, cacheTipo, cacheChecado, auditarRede; long dur; Tile *tiles; unsigned char *tentado; char arq[640]; } Job;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static Job *job;
static int estado;
static void limparCache(const char *path) { (void)path; }
'''
checks = r'''
static long size(const char *path) { struct stat s; assert(!stat(path,&s)); return s.st_size; }
static Job create(const char *path) {
  Job j = {0}; j.n=3; j.dur=30; j.direto=1;
  j.tiles=calloc(3,sizeof(Tile)); j.tentado=calloc(3,1); snprintf(j.arq,sizeof j.arq,"%s",path); return j;
}
static void release(Job *j) { for(int k=0;k<j->n;k++) free(j->tiles[k].jpg); free(j->tiles); free(j->tentado); }
int main(int argc, char **argv) {
  assert(argc==2); Job j=create(argv[1]); job=&j;
  for(int k=0;k<2;k++) { j.tiles[k].jpg=(unsigned char*)strdup("abc"); j.tiles[k].n=3; j.tiles[k].pts=k*10000; j.feitos++; }
  salvar(&j,0); assert(j.appendPronto && size(j.arq)==46);
  j.tiles[2].jpg=(unsigned char*)strdup("def"); j.tiles[2].n=3; j.tiles[2].pts=20000; j.feitos++;
  salvar(&j,0); assert(size(j.arq)==61);
  salvar(&j,1); assert(size(j.arq)==61); // no duplicate records on repeated checkpoint
  Job resumed=create(argv[1]); job=&resumed;
  assert(carregarInformado(&resumed)==1 && resumed.feitos==3);
  assert(resumed.cacheChecado && resumed.cacheTipo==2);
  assert(resumed.tiles[2].pts==20000 && !memcmp(resumed.tiles[2].jpg,"def",3));
  release(&resumed);
  Job audit=create(argv[1]); audit.auditarRede=1; job=&audit;
  assert(!carregarInformado(&audit) && audit.feitos==0 && audit.cacheTipo==0);
  long preserved=size(argv[1]); salvar(&audit,0); assert(size(argv[1])==preserved);
  release(&audit);
  FILE *f=fopen(argv[1],"r+b"); assert(f); fseek(f,12,SEEK_SET); escreverI32(f,0); fseek(f,0,SEEK_END);
  escreverI32(f,2); escreverI32(f,20000); escreverI32(f,100); fputc(1,f); fclose(f);
  resumed=create(argv[1]); job=&resumed;
  assert(!carregarInformado(&resumed) && resumed.feitos==3);
  assert(resumed.cacheChecado && resumed.cacheTipo==1); // torn tail preserves previous pictures
  salvar(&resumed,0); assert(size(argv[1])==61); // first checkpoint repairs/truncates old tail
  release(&resumed); release(&j);
  Job missing=create("/tmp/nuvio-no-such-thumbnail-set.nvm"); job=&missing;
  assert(!carregarInformado(&missing) && missing.cacheChecado && missing.cacheTipo==0);
  release(&missing);
  puts("PASS: incremental checkpoints, no duplicate records, resume and interrupted append recovery");
}
'''
source = prefix + "\nstatic void salvar(Job *j,int completo);\nstatic void mPipelineState(int state,int begin) { (void)state; (void)begin; }\n" + "\n".join(extract(sig) for sig in (
    "static void escreverI32(", "static int lerI32(", "static int carregar0(", "static int carregar(", "static int carregarInformado(", "static void salvar0(", "static void salvar(")) + checks
with tempfile.TemporaryDirectory(prefix="nuvio-cache-") as tmp:
    c = Path(tmp) / "cache.c"
    exe = Path(tmp) / "cache"
    c.write_text(source)
    subprocess.run(["cc", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", str(c), "-pthread", "-o", str(exe)], check=True)
    subprocess.run([str(exe), str(Path(tmp) / "set.nvm")], check=True)
