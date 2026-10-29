// Run the real Return translator; feed its SDL-equivalent events to the C test.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const shell = process.argv[2] || 'tools/tizen-shell.html';
const html = fs.readFileSync(shell, 'utf8');
const start = html.indexOf('  function imeAberto()');
const end = html.indexOf(shell.includes('lite') ? '  var MEDIA_TECLA' :
  '// TECLAS DE MIDIA DO CONTROLE SAMSUNG', start);
assert(start >= 0 && end > start);
const listeners = {};
const events = [];
const canvas = { dispatchEvent: ev => events.push(ev) };
vm.runInNewContext(html.slice(start, end), {
  window: { addEventListener: (type, fn) => { listeners[type] = fn; } },
  document: { getElementById: () => canvas },
  imeAberto: () => false,
  KeyboardEvent: function (type, options) { return { type, ...options }; },
});
for (const repeat of [false, true, false, true]) {
  listeners.keydown({ keyCode: 10009, repeat, preventDefault() {}, stopPropagation() {} });
}
if (listeners.keyup) {
  listeners.keyup({ keyCode: 10009, repeat: false, preventDefault() {}, stopPropagation() {} });
}
for (const ev of events) {
  assert.equal(ev.keyCode, 27);
  console.log(`${ev.type === 'keyup' ? 1 : 0} ${ev.repeat ? 1 : 0}`);
}
assert.deepEqual(events.map(ev => ev.type), ['keydown', 'keydown', 'keydown', 'keydown', 'keyup'],
  'Return must release only on physical keyup');
assert.deepEqual(events.map(ev => !!ev.repeat), [false, true, false, true, false]);
