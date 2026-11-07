"""Exercise production native thumbnail admission with a virtual player clock."""
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t Uint32;
typedef struct { double folgaTopo; Uint32 folgaTopoEm; int sharedHeadroom, parar; } Job;
typedef struct { double bufferSeg; } VideoStats;
#define FOLGA_MIN_S 4.0
static pthread_mutex_t muFolga = PTHREAD_MUTEX_INITIALIZER;
static Uint32 ticks, recoverAt, cancelAt;
static double buffered, position, duration;
static int headroom, bufferando, reconnecting, failed, scopes, begins, ends;
static Uint32 SDL_GetTicks(void) { return ticks; }
static void video_estatisticas(VideoStats *s) { s->bufferSeg = buffered; }
static double video_buffer_fim(void) { return 0; }
static double video_pos(void) { return position; }
static double video_duracao(void) { return duration; }
static int video_reconectando(void) { return reconnecting; }
static int video_falhou(void) { return failed; }
static int parou(const Job *j) { return j->parar; }
static int mHeadroom(const Job *j, double target) {
  (void)target;
  return !j->sharedHeadroom ? -1 : recoverAt && ticks >= recoverAt ? 1 : headroom;
}
static void mPipelineState(int state, int begin) {
  assert(state == 3);
  if (begin) { begins++; scopes++; }
  else { ends++; assert(scopes > 0); scopes--; }
}
static void dormir(const Job *j, int ms) {
  for (int t = 0; t < ms && !parou(j); t += 50) {
    ticks += 50;
    if (cancelAt && ticks >= cancelAt) ((Job *)j)->parar = 1;
  }
}
static Job fresh(int shared) {
  ticks = recoverAt = cancelAt = 0;
  buffered = 4.2; position = 0; duration = 1000;
  headroom = 1; bufferando = reconnecting = failed = scopes = begins = ends = 0;
  Job j = { .folgaTopo = -1, .sharedHeadroom = shared };
  return j;
}
static void balanced(void) { assert(!scopes && begins == ends); }
'''

checks = r'''
int main(void) {
  // A full byte-capped remux must not inherit an unrepresentative startup peak.
  Job shared = fresh(1);
  buffered = 20; assert(folgaSeg(&shared) == 20);
  buffered = 4.2;
  for (int n = 0; n < 3; n++) esperarFolga(&shared);
  assert(ticks == 0 && !folgaBaixa(&shared)); balanced();
  Job baseline = fresh(0);
  buffered = 20; folgaSeg(&baseline); buffered = 4.2;
  for (int n = 0; n < 3; n++) esperarFolga(&baseline);
  assert(ticks == 45000 && begins == 90); balanced();

  // Source-owned high-water marks cannot contaminate another source.
  Job next = fresh(0);
  assert(folgaSeg(&next) == 4.2);
  assert(folgaQuer(&next, 6, .5) == 2.1);
  assert(folgaQuer(&baseline, 6, .5) == 6);
  esperarFolga(&next); assert(ticks == 0); balanced();

  // Stale/unsafe shared health is BLOCKED, never legacy fallback.
  shared = fresh(1); headroom = 0; buffered = 20;
  esperarFolga(&shared);
  assert(ticks == 15000 && begins == 30 && folgaBaixa(&shared)); balanced();
  // Preserve the documented legacy timeout: waiting itself ends after 15 s.
  baseline = fresh(0); baseline.folgaTopo = 20;
  esperarFolga(&baseline); assert(ticks == 15000); balanced();

  // Player recovery and cancellation always balance their timeline scopes.
  shared = fresh(1); headroom = 0; recoverAt = 1500;
  esperarPlayer(&shared); assert(ticks == 2000 && begins == 2); balanced();
  shared = fresh(1); headroom = 0; recoverAt = 750;
  esperarFolga(&shared); assert(ticks == 1000 && begins == 2); balanced();
  shared = fresh(1); headroom = 0; cancelAt = 125;
  esperarFolga(&shared); assert(shared.parar && ticks == 150 && begins == 1); balanced();
  shared = fresh(1); headroom = 0; cancelAt = 125;
  esperarPlayer(&shared); assert(shared.parar && ticks == 150 && begins == 1); balanced();
  shared = fresh(1); shared.parar = 1;
  esperarFolga(&shared); esperarPlayer(&shared);
  assert(!ticks && !begins); balanced();

  // Near EOF the remaining source, not a two-second minimum, bounds demand.
  shared = fresh(1); headroom = 0; duration = 100; position = 98; buffered = 1.2;
  assert(!folgaBaixa(&shared));
  buffered = .5; assert(folgaBaixa(&shared));
  // Outside that EOF exception the shared emergency floor still blocks.
  duration = 1000; assert(folgaBaixa(&shared));
  puts("PASS: native headroom, source isolation, timeout, cancellation, timeline and EOF admission");
  return 0;
}
'''

functions = "\n\n".join(extract(name) for name in (
    "folgaSeg", "folgaQuer", "folgaBaixa", "dormirGate", "esperarPlayer", "esperarFolga"))
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "headroom.c").write_text(prefix + functions + checks)
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    str(path / "headroom.c"), "-pthread", "-o", str(path / "headroom")], check=True)
    subprocess.run([str(path / "headroom")], check=True, timeout=10)
