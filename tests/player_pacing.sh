#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
node <<'JS'
const fs = require('fs');
const assert = require('assert');
const source = fs.readFileSync('src/main.c', 'utf8');
const body = source.split('EM_ASYNC_JS(void, nv_ceder_quadro,')[1].split('\n});')[0].split('{\n').slice(1).join('{\n');
const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
async function check(ui, rafFirst) {
  let raf, timer, delay, cancelled = 0, cleared = 0, done = false;
  const frame = new AsyncFunction('ui', 'requestAnimationFrame', 'cancelAnimationFrame', 'setTimeout', 'clearTimeout', body);
  const pending = frame(ui, cb => { raf = cb; return 1; }, id => { assert.equal(id, 1); cancelled++; },
    (cb, ms) => { timer = cb; delay = ms; return 2; }, id => { assert.equal(id, 2); cleared++; }).then(() => { done = true; });
  await Promise.resolve();
  if (ui) {
    assert(timer, 'visible player UI must not wait indefinitely for browser rAF');
    assert(delay > 0 && delay <= 17, 'UI deadline must retain normal frame cadence');
    if (rafFirst) raf(); else timer();
  } else {
    assert.equal(timer, undefined, 'bare video retains browser pacing');
    assert.equal(done, false);
    raf();
  }
  await pending;
  if (ui) { assert.equal(cancelled, 1); assert.equal(cleared, 1); }
}
(async () => {
  await check(1, false);
  await check(1, true);
  await check(0, true);
  assert(source.includes('nv_ceder_quadro(playerUi)') && source.includes('player_ui_frame(player_aberto(), gfx_n_rect, gfx_n_outros)'),
    'player draws beyond the video hole must enable the UI deadline');
  console.log('PASS: delayed rAF cannot stall drawn player UI; bare video retains rAF pacing');
})().catch(e => { console.error(e); process.exitCode = 1; });
JS
