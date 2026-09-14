// Issue #31 ("It works now but it needs me to toggle the player menu to see
// it"): the parental-guide block in player_desenhar() drew its own alpha from
// pgDesde/entrada/saida (nothing to do with the OSD), but lived textually
// AFTER `if (a <= 0.005f) { ...; return; }` -- the same early return that
// `a` (anim * entrada, the OSD's own alpha) already trips a few seconds into
// clean playback. So the function never reached the block unless the OSD had
// just been shown, which only happens once the viewer opens the menu.
//
// This is a STRUCTURAL/ordering check, not a rendered-pixel test: proving the
// panel actually appears on a real screen needs a video pipeline and a
// decoded frame (inicioImagem, comVideo, video_pronto()) that this repo has
// no seam to drive from a unit test without a TV or a real source file. What
// is provable without either is that the block is reachable before the
// early-return gate -- which is the exact defect rawldon reported and the
// exact fix applied.
const assert = require('node:assert/strict');
const fs = require('node:fs');

const src = fs.readFileSync('src/player.c', 'utf8');

const fnStart = src.indexOf('void player_desenhar(Uint32 agora) {');
assert(fnStart >= 0, 'player_desenhar not found');
// Function is long; a generous slice covers it without needing a brace parser.
const fn = src.slice(fnStart, fnStart + 20000);

const idxPosplay = fn.indexOf('posplay_desenhar(agora');
const idxGuia = fn.indexOf('// GUIA PARENTAL');
// The exact code line, not the explanatory comment above the guide block
// (which quotes this same condition in prose and would otherwise match first).
const idxReturn = fn.indexOf('if (a <= 0.005f) { desenharAcoesEpisodio(); return; }');
const idxPgDesde = fn.indexOf('pgDesde = agora;');

let failures = 0;
function check(name, fn2) {
  try { fn2(); console.log('PASS:', name); }
  catch (e) { failures++; console.error('FAIL:', name, e.message); }
}

check('all four markers are present exactly once', () => {
  assert.notEqual(idxPosplay, -1, 'posplay_desenhar call missing');
  assert.notEqual(idxGuia, -1, 'GUIA PARENTAL block missing');
  assert.notEqual(idxReturn, -1, 'early-return gate missing');
  assert.notEqual(idxPgDesde, -1, 'pgDesde assignment missing');
  assert.equal((fn.match(/\/\/ GUIA PARENTAL/g) || []).length, 1,
    'GUIA PARENTAL block duplicated -- the old copy was not removed');
});

check('parental guide block sits between posplay_desenhar and the "tocando limpo" return', () => {
  assert.ok(idxPosplay < idxGuia,
    'guide block must come after posplay_desenhar (same reasoning applies to it, per the comment there)');
  assert.ok(idxGuia < idxReturn,
    'REGRESSION: guide block is after the early return again -- it will only ' +
    'draw once the OSD has been toggled, reproducing issue #31');
});

check('pgDesde is written before the early return (diagnostics stay reachable)', () => {
  assert.ok(idxPgDesde < idxReturn,
    'pgDesde = agora must run before the early return, or "[pg] abriu" never logs during clean playback');
});

process.exitCode = failures ? 1 : 0;
