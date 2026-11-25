"""Run isolated production C paths without Android, SDL or a running server.

Usage: python3 tests/thumbs_report_regressions.py (requires cc and pthreads).
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(path, signature):
    text = (ROOT / path).read_text(encoding="utf-8")
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "idiomacod.h"
enum { AJ_TMDB_IDIOMA };
static int valor[1], uiLanguage=IDIOMA_EN;
static int ajustes_idioma(void) { return uiLanguage; }
typedef unsigned GLuint;
#define GRADE_MS 10000L
#define VIZINHO_SLOTS 30
#define TOL_COPIA_MS 8000L
typedef struct { char *jpg; long pts; } Tile;
typedef struct { int n; Tile tiles[100]; } Job;
static Job storage, *job = &storage;
static int velho;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static GLuint texDe(Job *j, int k) { (void)j; return k + 1; }
typedef struct { const char *tipo, *imdb; } CatItem;
static int sheet, folhaTocarT, folhaTocarE, focusT = 1, focusE = 1;
static int stream_folha_aberta(void) { return sheet; }
static int detail_indice(void) { return 7; }
static int detail_ep_foco(int *t, int *e) { *t=focusT; *e=focusE; return 1; }
static void cat_id_stream(int idx, int t, int e, char *out, unsigned n) {
  assert(idx == 7); snprintf(out, n, "tt123:%d:%d", t, e);
}
'''

checks = r'''
static void tile(int k, long pts) { storage.tiles[k].jpg="jpeg"; storage.tiles[k].pts=pts; }
int main(void) {
  GLuint out[3]; double cues[3];
  storage.n=100;
  tile(0, 0); tile(6, 60000); tile(12, 120000);
  assert(pQuadros(60,3,out,cues));
  assert(out[0]==1 && out[1]==7 && out[2]==13);
  assert(cues[0]==0 && cues[1]==60 && cues[2]==120);
  // An approximate centre still has real neighbours during sparse generation.
  assert(pQuadros(80,3,out,cues) && velho);
  assert(out[0]==1 && out[1]==7 && out[2]==13);
  // Duplicate picture times cannot appear as earlier/later pictures.
  tile(5,60000); tile(7,60000);
  assert(pQuadros(60,3,out,cues));
  assert(cues[0]==0 && cues[2]==120);
  assert(pQuadros(60,1,out,cues) && out[0]==7 && cues[0]==60);
  assert(pQuadros(0,3,out,cues) && !out[0] && out[1]==1);
  memset(&storage,0,sizeof storage); storage.n=100;
  assert(!pQuadros(60,3,out,cues));
  assert(!out[0] && !out[1] && !out[2]);
  // Occupied nominal 60s slot displays a 62s keyframe without stale indication.
  tile(6,62000);
  assert(pQuadros(60,1,out,cues) && cues[0]==62 && !velho);
  printf("PTS CHECK: cursor=60s, displayed keyframe=%.1fs, stale=%d\n", cues[0], velho);
  char target[64]; CatItem series={"series","tt123"}, movie={"movie","tt456"};
  sheet=1; folhaTocarT=1; folhaTocarE=4;
  idDoAlvo(&series,target,sizeof target);
  assert(!strcmp(target,"tt123:1:4"));
  focusE=2; idDoAlvo(&series,target,sizeof target);
  assert(!strcmp(target,"tt123:1:4"));
  folhaTocarT=0; idDoAlvo(&series,target,sizeof target);
  assert(!strcmp(target,"tt123:0:4"));
  sheet=0; idDoAlvo(&series,target,sizeof target);
  assert(!strcmp(target,"tt123:1:2"));
  sheet=1; idDoAlvo(&movie,target,sizeof target);
  assert(!strcmp(target,"tt456"));
  idDoAlvo(NULL,target,sizeof target); assert(!target[0]);
  // An explicit metadata preference is independent of the interface.
  valor[AJ_TMDB_IDIOMA]=32;
  assert(!strcmp(ajustes_tmdb_idioma(),"ar-SA"));
  valor[AJ_TMDB_IDIOMA]=0;
  assert(!strcmp(ajustes_tmdb_idioma(),"en-US"));
  uiLanguage=IDIOMA_AR;
  assert(!strcmp(ajustes_tmdb_idioma(),"ar-SA"));
  puts("PASS: sparse strip, distinct picture times, edges, episode sheet target");
}
'''

with tempfile.TemporaryDirectory(prefix="nuvio-thumbs-regression-") as tmp:
    source = Path(tmp) / "regressions.c"
    executable = Path(tmp) / "regressions"
    source.write_text(prefix + function("src/miniatura_android.c", "static int pQuadros(")
                      + function("src/app.c", "static void idDoAlvo(")
                      + function("src/ajustes.c", "const char *ajustes_tmdb_idioma(") + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(source),
                    "-pthread", "-I", str(ROOT / "src"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
