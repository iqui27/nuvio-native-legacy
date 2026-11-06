"""Exercise production direct frame ownership, nominal scheduling and hint separation."""
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniatura_coverage.h"
typedef uint32_t Uint32;
#define GRADE_MS 10000L
#define M_PRONTO 2
typedef struct { unsigned char *jpg; int n; long pts; unsigned char prov, salvo; unsigned revision; } Tile;
typedef struct {
  int n, feitos, direto;
  long dur;
  Tile *tiles;
  unsigned char *tentado;
  MiniCoverage coverage;
  int *slotFrame;
  unsigned char *slotState;
  int64_t *framePts;
  long *slotKey;
  Uint32 primeiro;
} Job;
static Job *job;
static int estado, scrubbing;
static volatile long playheadMs, posMs;
static Uint32 SDL_GetTicks(void) { return 1234; }
static long mSlotKeyframe(long long handle, long target) {
  (void)handle;
  if (target == 20000 || target == 30000) return 25000;
  if (target == 40000) return -1; /* genuine indexed gap */
  if (target == 50000) return -2; /* extractor fallback without an index */
  if (target == 0) return -1;     /* zero policy belongs to the real Kotlin resolver */
  return target;
}
static Job create(int count, long duration) {
  Job j = {0}; j.n=count; j.dur=duration; j.direto=1;
  j.tiles=calloc((size_t)count,sizeof *j.tiles);
  j.tentado=calloc((size_t)count,1);
  j.slotFrame=malloc((size_t)count*sizeof *j.slotFrame);
  j.slotState=calloc((size_t)count,1);
  j.framePts=malloc((size_t)count*sizeof *j.framePts);
  j.slotKey=malloc((size_t)count*sizeof *j.slotKey);
  assert(j.tiles && j.tentado && j.slotFrame && j.slotState && j.framePts && j.slotKey);
  for(int i=0;i<count;i++) { j.framePts[i]=-1; j.slotKey[i]=-1; }
  mini_coverage_init(&j.coverage,count,duration,j.slotFrame,j.slotState);
  playheadMs=posMs=0; scrubbing=0;
  return j;
}
static void destroy(Job *j) {
  int freed=0;
  for(int i=0;i<j->n;i++) if(j->tiles[i].jpg) {
    for(int k=0;k<i;k++) assert(j->tiles[k].jpg!=j->tiles[i].jpg);
    free(j->tiles[i].jpg);freed++;
  }
  assert(freed==j->feitos);
  free(j->tiles);free(j->tentado);free(j->slotFrame);free(j->slotState);free(j->framePts);free(j->slotKey);
  if(job==j)job=NULL;
}
static unsigned char *image(unsigned char value) {
  unsigned char *p=malloc(4);assert(p);memset(p,value,4);return p;
}
'''

checks = r'''
static int publish(Job *j,long pts,unsigned char value) {
  unsigned char *jpeg=image(value);
  int owner=diretoGuardarFrame(j,pts,&jpeg,4);
  free(jpeg); /* duplicate/rejected images remain caller-owned */
  return owner;
}
static void drift_does_not_retire_a_nominal_target(void) {
  Job j=create(7,60000);job=&j;
  j.tentado[3]=1;mini_coverage_mark(&j.coverage,3,MINI_COVER_PENDING);
  assert(publish(&j,22397,1)==0);
  assert(j.tiles[0].pts==22397 && j.feitos==1 && j.primeiro==1234 && estado==M_PRONTO);
  assert(j.slotFrame[2]==0 && j.slotState[2]==MINI_COVER_COVERED);
  assert(j.slotFrame[3]==-1 && j.slotState[3]==MINI_COVER_PENDING);
  assert(!j.tentado[2]); /* owner/rounded-PTS position never claims a nominal slot */
  mini_coverage_mark(&j.coverage,3,MINI_COVER_FAILED);
  assert(!diretoCoverageComplete(&j));
  assert(publish(&j,32000,2)==1);
  assert(j.slotFrame[3]==1 && j.slotState[3]==MINI_COVER_COVERED);
  assert(j.tiles[0].pts==22397 && j.tiles[1].pts==32000 && j.feitos==2);
  diretoRebuildCoverage(&j);
  assert(j.slotFrame[2]==0 && j.slotFrame[3]==1);
  destroy(&j);
}
static void rounded_timestamp_collisions_keep_both_owners(void) {
  Job j=create(7,60000);job=&j;
  assert((22397+5000)/10000==(24400+5000)/10000);
  int first=publish(&j,22397,3),second=publish(&j,24400,4);
  assert(first==0 && second==1 && j.feitos==2);
  unsigned char *original=j.tiles[0].jpg;
  unsigned revision=j.tiles[0].revision;
  assert(publish(&j,22397,9)==first);
  assert(j.tiles[0].jpg==original && j.tiles[0].jpg[0]==3);
  assert(j.tiles[0].revision==revision && j.feitos==2);
  assert(publish(&j,-1,5)==-1 && publish(&j,60001,5)==-1 && j.feitos==2);
  destroy(&j);
}
static void aliases_complete_without_duplicate_jpeg_ownership(void) {
  Job j=create(7,60000);job=&j;
  publish(&j,0,1);publish(&j,10000,2);int shared=publish(&j,25000,3);
  publish(&j,45000,4);publish(&j,60000,5);
  assert(j.feitos==5 && mini_coverage_covered_count(&j.coverage)==7);
  assert(j.slotFrame[2]==shared && j.slotFrame[3]==shared);
  assert(diretoCoverageComplete(&j) && !restamNaEtapa(&j,6) && !restamNaEtapa(&j,3));
  int stage=-1;assert(proximoDireto(&j,4,&stage)==-1);
  destroy(&j);
}
static void exhausted_claims_do_not_prove_completion(void) {
  Job j=create(7,60000);job=&j;
  for(int k=0;k<j.n;k++) {
    j.tentado[k]=1;
    mini_coverage_mark(&j.coverage,k,k%2 ? MINI_COVER_PENDING : MINI_COVER_UNAVAILABLE);
  }
  int stage=-1;assert(proximoDireto(&j,4,&stage)==-1);
  assert(!diretoCoverageComplete(&j) && restamNaEtapa(&j,6) && restamNaEtapa(&j,3));
  mini_coverage_mark(&j.coverage,3,MINI_COVER_FAILED);
  assert(!livreDireto(&j,3));
  destroy(&j);
}
static void scheduler_and_metadata_remain_separate(void) {
  Job j=create(7,60000);job=&j;int stage=-1;
  assert(proximoDireto(&j,4,&stage)==0 && stage==1);
  mini_coverage_mark(&j.coverage,0,MINI_COVER_UNAVAILABLE);
  assert(proximoDireto(&j,4,&stage)==3 && stage==1);
  j.tentado[3]=1;mini_coverage_mark(&j.coverage,3,MINI_COVER_PENDING);
  assert(proximoDireto(&j,4,&stage)==6 && stage==1);
  long payload[]={0,20000,30000,40000},headers[]={30000,40000,50000,60000};
  int np=resolverSondas(1,payload,4),nh=resolverSondas(1,headers,4);
  assert(np==1 && payload[0]==25000);
  assert(nh==3 && headers[0]==25000 && headers[1]==50000 && headers[2]==60000);
  assert(np==1); /* shrinking payload hints cannot promote a metadata-only target */
  assert(j.slotState[0]==MINI_COVER_UNAVAILABLE && j.slotState[3]==MINI_COVER_PENDING);
  destroy(&j);
}
int main(void) {
  drift_does_not_retire_a_nominal_target();
  rounded_timestamp_collisions_keep_both_owners();
  aliases_complete_without_duplicate_jpeg_ownership();
  exhausted_claims_do_not_prove_completion();
  scheduler_and_metadata_remain_separate();
  puts("PASS: production direct frame owners, nominal aliases, completion, scheduler and separated hints");
  return 0;
}
'''

functions = "\n\n".join(extract(name) for name in (
    "diretoCoverageComplete", "diretoRebuildCoverage", "diretoGuardarFrame", "livreDireto",
    "proximoDireto", "restamNaEtapa", "resolverSondas"))
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "direct.c").write_text(prefix + functions + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(root / "src"),
                    str(path / "direct.c"), "-o", str(path / "direct")], check=True)
    subprocess.run([str(path / "direct")], check=True, timeout=10)
