"""Exercise production NVM3 ownership, verified coverage, and interrupted appends."""
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
#include "miniatura_coverage.h"
#define CACHE_TTL_S 7200
#define M_PRONTO 1
typedef struct { unsigned char *jpg; int n; long pts; unsigned char prov, salvo; unsigned revision; } Tile;
typedef struct {
  int n, feitos, direto, appendPronto, cacheTipo, cacheChecado, auditarRede;
  long dur; Tile *tiles; unsigned char *tentado;
  MiniCoverage coverage; int *slotFrame; unsigned char *slotState; int64_t *framePts;
  char arq[640];
} Job;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static Job *job;
static int estado;
static void limparCache(const char *path) { (void)path; }
'''
checks = r'''
static long size(const char *path) { struct stat s; assert(!stat(path,&s)); return s.st_size; }
static Job create(const char *path) {
  Job j = {0}; j.n=3; j.dur=29999; j.direto=1;
  j.tiles=calloc(3,sizeof(Tile)); j.tentado=calloc(3,1);
  j.slotFrame=calloc(3,sizeof *j.slotFrame);j.slotState=calloc(3,1);j.framePts=calloc(3,sizeof *j.framePts);
  for(int k=0;k<3;k++)j.framePts[k]=-1;
  mini_coverage_init(&j.coverage,j.n,j.dur,j.slotFrame,j.slotState);
  snprintf(j.arq,sizeof j.arq,"%s",path); return j;
}
static void release(Job *j) {
  for(int k=0;k<j->n;k++)free(j->tiles[k].jpg);
  free(j->tiles);free(j->tentado);free(j->slotFrame);free(j->slotState);free(j->framePts);
}
static void picture(Job *j,int owner,long pts,const char *data) {
  assert(owner>=0 && owner<j->n && !j->tiles[owner].jpg);
  j->tiles[owner].jpg=(unsigned char*)strdup(data);j->tiles[owner].n=(int)strlen(data);
  j->tiles[owner].pts=pts;j->feitos++;diretoRebuildCoverage(j);
}
static long completeFlag(const char *path) {
  FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,12,SEEK_SET));long complete;
  assert(lerI32(f,&complete));fclose(f);return complete;
}
static void forceFlag(const char *path,int complete) {
  FILE *f=fopen(path,"r+b");assert(f);assert(!fseek(f,12,SEEK_SET));escreverI32(f,complete);assert(!fclose(f));
}
int main(int argc, char **argv) {
  assert(argc==2); Job j=create(argv[1]); job=&j;
  for(int k=0;k<2;k++)picture(&j,k,k*10000,"abc");
  salvar(&j,0); assert(j.appendPronto && size(j.arq)==46);
  picture(&j,2,20000,"def");
  salvar(&j,0); assert(size(j.arq)==61);
  salvar(&j,1); assert(size(j.arq)==61 && completeFlag(j.arq)==1);
  Job resumed=create(argv[1]); job=&resumed;
  assert(carregarInformado(&resumed)==1 && resumed.feitos==3);
  assert(resumed.cacheChecado && resumed.cacheTipo==2);
  assert(resumed.tiles[2].pts==20000 && !memcmp(resumed.tiles[2].jpg,"def",3));
  assert(mini_coverage_covered_count(&resumed.coverage)==3);
  for(int k=0;k<3;k++)assert(!resumed.tentado[k]);
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
  assert(mini_coverage_complete(&resumed.coverage)); // header still correctly says incomplete
  salvar(&resumed,0); assert(size(argv[1])==61); // first checkpoint repairs/truncates old tail
  release(&resumed); release(&j);

  char aliasPath[700],gapPath[700],copyPath[700];
  snprintf(aliasPath,sizeof aliasPath,"%s.alias",argv[1]);
  snprintf(gapPath,sizeof gapPath,"%s.gap",argv[1]);
  snprintf(copyPath,sizeof copyPath,"%s.copy",argv[1]);
  Job alias=create(aliasPath);job=&alias;
  picture(&alias,2,5000,"abc");picture(&alias,0,20000,"def");
  assert(alias.feitos==2 && mini_coverage_covered_count(&alias.coverage)==3);
  salvar(&alias,1);assert(completeFlag(aliasPath)==1 && size(aliasPath)==46);
  resumed=create(aliasPath);job=&resumed;
  assert(carregarInformado(&resumed)==1 && resumed.feitos==2);
  assert(resumed.slotFrame[0]==2 && resumed.slotFrame[1]==2 && resumed.slotFrame[2]==0);
  for(int k=0;k<3;k++)assert(!resumed.tentado[k]);
  release(&resumed);release(&alias); // one JPEG owner even when two positions alias it

  Job gap=create(gapPath);job=&gap;picture(&gap,0,22397,"abc");
  memset(gap.tentado,1,3);salvar(&gap,1);
  assert(completeFlag(gapPath)==0); // attempted/exhausted is never proof of completion
  forceFlag(gapPath,1); // old or corrupt complete flags must be revalidated at load
  resumed=create(gapPath);job=&resumed;
  assert(!carregarInformado(&resumed) && resumed.feitos==1 && resumed.cacheTipo==1);
  assert(mini_coverage_covered_count(&resumed.coverage)==1 && resumed.slotFrame[2]==0);
  assert(resumed.slotFrame[0]==-1 && resumed.slotFrame[1]==-1);
  for(int k=0;k<3;k++)assert(!resumed.tentado[k]);
  release(&resumed);
  resumed=create(gapPath);resumed.dur--;job=&resumed;
  assert(!carregarInformado(&resumed) && resumed.feitos==0 && resumed.cacheTipo==0);
  release(&resumed);release(&gap);

  Job copy=create(copyPath);copy.direto=0;job=&copy;picture(&copy,2,22397,"abc");
  salvar(&copy,1);assert(completeFlag(copyPath)==1); // copy route retains its original contract
  resumed=create(copyPath);resumed.direto=0;job=&resumed;
  assert(carregarInformado(&resumed)==1 && resumed.tentado[2]==1 && resumed.feitos==1);
  release(&resumed);release(&copy);

  Job missing=create("/tmp/nuvio-no-such-thumbnail-set.nvm"); job=&missing;
  assert(!carregarInformado(&missing) && missing.cacheChecado && missing.cacheTipo==0);
  release(&missing);
  puts("PASS: verified direct coverage/aliases, sparse flags, ownership, duration, copy route, bypass and append recovery");
}
'''
source = prefix + "\nstatic void salvar(Job *j,int completo);\nstatic void mPipelineState(int state,int begin) { (void)state; (void)begin; }\n" + "\n".join(extract(sig) for sig in (
    "static int diretoCoverageComplete(", "static void diretoRebuildCoverage(",
    "static void escreverI32(", "static int lerI32(", "static int carregar0(", "static int carregar(", "static int carregarInformado(", "static void salvar0(", "static void salvar(")) + checks
with tempfile.TemporaryDirectory(prefix="nuvio-cache-") as tmp:
    c = Path(tmp) / "cache.c"
    exe = Path(tmp) / "cache"
    c.write_text(source)
    subprocess.run(["cc", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I", str(root / "src"),
                    "-fno-omit-frame-pointer", str(c), "-pthread", "-o", str(exe)], check=True)
    subprocess.run([str(exe), str(Path(tmp) / "set.nvm")], check=True)
