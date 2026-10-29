// Exercise the production HTML5 crop/window path without a TV browser.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const source = fs.readFileSync('src/video_html5.c', 'utf8');
const start = source.indexOf('static int aplicarRect(');
const end = source.indexOf('\nconst char *video_url_atual(', start);
assert(start >= 0 && end > start, 'production HTML5 window path not found');
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'guialayout-html5-'));
try {
  const file = path.join(dir, 'test.c');
  const binary = path.join(dir, 'test');
  fs.writeFileSync(file, `
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int temVideo=1, ativo=1, janX, janY, janW, janH;
static int cropSX, cropSY, cropSW, cropSH, cropAtivo;
static int crop[4], rect[4];
static double avChamar(const char *op, const char *txt,
                      double a, double b, double c, double d, char *dst, int size) {
  int values[] = {a,b,c,d};
  if (!strcmp(op,"recorte")) memcpy(crop,values,sizeof crop);
  if (!strcmp(op,"rect")) memcpy(rect,values,sizeof rect);
  return 1;
}
${source.slice(start, end)}
int main(void) {
  // Grade cover of a 720x576 channel, then Cards' fitted preview.
  video_janela_fonte(0,98,720,380,978,96,894,472);
  assert(cropAtivo && crop[1]==98 && crop[3]==380);
  assert(rect[2]==894 && rect[3]==472);
  video_janela(1417,168,320,256);
  assert(!cropAtivo && crop[2]==0 && crop[3]==0);
  assert(rect[0]==1417 && rect[1]==168 && rect[2]==320 && rect[3]==256);
  video_recorte_reaplicar();
  assert(crop[2]==0 && crop[3]==0);
  // A repeated destination must still clear the source crop.
  video_janela_fonte(0,98,720,380,1417,168,320,256);
  video_recorte_reaplicar();
  assert(cropAtivo && crop[1]==98 && crop[3]==380);
  video_janela(1417,168,320,256);
  assert(!cropAtivo && crop[2]==0 && crop[3]==0);
  puts("PASS: HTML5 Grade-to-Cards resets source crop, including repeated destination and reapply");
  return 0;
}
`);
  const build = spawnSync('cc', [file, '-o', binary], { stdio: 'inherit' });
  assert.equal(build.status, 0, 'compile HTML5 window regression');
  const run = spawnSync(binary, [], { stdio: 'inherit' });
  assert.equal(run.status, 0, 'HTML5 Grade-to-Cards crop reset');
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}
