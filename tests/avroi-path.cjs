// Compile the production C ROI/window path with controllable AVPlay results.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync('src/video_tizen.c', 'utf8');
const start = source.indexOf('static int aplicarRect(');
const end = source.indexOf('\nconst char *video_url_atual(', start);
assert(start >= 0 && end > start, 'production ROI/window path not found');
const pumpStart = source.indexOf('void video_bombear(void) {');
const pumpEnd = source.indexOf('  double est[EST_N];', pumpStart);
const retryStart = source.indexOf('void video_recorte_reaplicar(void) {');
const retryEnd = source.indexOf('\n// Tizen:', retryStart);
const stateStart = source.indexOf('static int roiAtivo;');
const stateEnd = source.indexOf('\n// AVANCO COM REPOUSO', stateStart);
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'avroi-path-'));
try {
  const fixture = `
#define main helper_main
#include "${path.resolve('tests/avroi.c')}"
#undef main
#include <string.h>
typedef unsigned Uint32;
static Uint32 now;
static Uint32 SDL_GetTicks(void) { return now; }
${source.slice(stateStart, stateEnd)}
static int temAvplay = 1, ativo = 1;
${source.slice(retryStart, retryEnd)}
${source.slice(pumpStart, pumpEnd)} }

static int janX, janY, janW, janH;
static int roiCalls, roi[4];
static double result = 1;
static double video_largura(void) { return 1920; }
static double video_altura(void) { return 1080; }
static double avChamar(const char *op, const char *txt,
                      double a, double b, double c, double d, char *dst, int size) {
  (void)txt; (void)dst; (void)size;
  if (strcmp(op, "roi")) return 1;
  roiCalls++;
  roi[0] = a; roi[1] = b; roi[2] = c; roi[3] = d;
  return result;
}
${source.slice(start, end)}
int main(void) {
  bad = helper_main();
  (void)SDL_GetTicks();
  EXPECT(roiAplicar(100, 0, 1600, 1080) == 1);
  EXPECT(roiAtivo == 1 && roiCalls == 1 && roi[0] == 100 && roi[2] == 1600);
  result = 0; // transient rejection while leaving zoom for PiP
  video_janela(20, 20, 640, 360);
  EXPECT(roiCalls == 2 && roiAtivo == 1);
  EXPECT(roi[0] == 0 && roi[1] == 0 && roi[2] == 1920 && roi[3] == 1080);
  result = 1;
  video_bombear(); // normal playback, no further UI geometry request
  EXPECT(roiCalls == 2 && roiAtivo == 1);
  now += 250;
  video_bombear();
  EXPECT(roiCalls == 3 && roiAtivo == 0);
  now += 1000;
  video_bombear();
  EXPECT(roiCalls == 3);
  video_janela(0, 0, 1920, 1080);
  EXPECT(roiCalls == 3);
  EXPECT(roiAplicar(100, 0, 1600, 1080) == 1);
  result = -1; // missing API is distinct from transient rejection
  video_janela(0, 0, 1920, 1080);
  EXPECT(roiAtivo == 0 && roiCalls == 5);
  video_janela(0, 0, 1920, 1080);
  EXPECT(roiCalls == 5);
  result = 1;
  EXPECT(roiAplicar(100, 0, 1600, 1080) == 1);
  result = 0;
  video_janela(0, 0, 1920, 1080);
  int calls = roiCalls;
  for (int i = 0; i < 20; ++i) {
    now += 250;
    video_bombear();
  }
  EXPECT(roiCalls == calls + 3 && roiAtivo == 1); // bounded rejection

  video_janela(20, 20, 640, 360); // replace exhausted reset request
  result = 1;
  now += 250;
  video_bombear();
  EXPECT(roiAtivo == 0);

  EXPECT(roiAplicar(100, 0, 1600, 1080) == 1);
  result = 0;
  video_janela(0, 0, 1920, 1080);
  // A newer crop cancels the reset even if AVPlay rejects that crop too.
  video_janela_fonte(200, 0, 1500, 1080, 0, 0, 1920, 1080);
  calls = roiCalls;
  result = 1;
  now += 1000;
  video_bombear();
  EXPECT(roiCalls == calls && roiAtivo == 1);

  result = 0;
  video_janela(0, 0, 1920, 1080);
  result = -1;
  now += 250;
  video_bombear();
  calls = roiCalls;
  now += 1000;
  video_bombear();
  EXPECT(roiCalls == calls && roiAtivo == 0);
  if (!bad) puts("PASS production ROI reset/retry");
  return bad;
}
`;
  const file = path.join(dir, 'test.c');
  const binary = path.join(dir, 'test');
  fs.writeFileSync(file, fixture);
  for (const [cmd, args] of [
    ['cc', ['-Wall', '-Wextra', '-Werror', file, '-o', binary]],
    [binary, []],
  ]) {
    const run = spawnSync(cmd, args, { stdio: 'inherit' });
    if (run.error) throw run.error;
    if (run.status !== 0) { process.exitCode = run.status || 1; break; }
  }
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}
