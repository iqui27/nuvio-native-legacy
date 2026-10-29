// Exercises the key-translation script embedded in tools/tizen-shell.html
// against a minimal DOM stub. Covers only the browser-side keyCode ->
// synthetic-key mapping; it does not prove the Samsung firmware actually
// delivers MediaPlayPause to a registered app (no TV available here).
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const html = fs.readFileSync('tools/tizen-shell.html', 'utf8');
const scriptMatch = html.match(/<script>([\s\S]*?)<\/script>/);
assert(scriptMatch, 'inline script not found in tizen-shell.html');

// Minimal EventTarget: enough for addEventListener/dispatchEvent used by the
// shell script. Capture-phase semantics are irrelevant here because every
// dispatch below targets the same element that registered the listener.
class Alvo {
  constructor(id) { this.id = id; this.hidden = false; this._l = {}; }
  addEventListener(type, fn) { (this._l[type] = this._l[type] || []).push(fn); }
  dispatchEvent(ev) { (this._l[ev.type] || []).forEach(fn => fn(ev)); return true; }
  getContext() { return null; }  // WebGL probe: irrelevant to key mapping
  focus() {}
  set innerHTML(_v) {}
}
class FakeKeyboardEvent {
  constructor(type, opts) {
    this.type = type;
    Object.assign(this, opts);
  }
  preventDefault() {}
  stopPropagation() {}
}

const canvas = new Alvo('canvas');
const diag = new Alvo('diag');
const elems = { canvas, diag };

const context = {
  console,
  setTimeout: () => 0, clearTimeout: () => {},
  performance: { now: () => 1 },
  // The 3s PThread-pool meter and the (disabled) log-batch timer are not
  // relevant to key mapping; a real timer would keep the test process alive.
  setInterval: () => 0, clearInterval: () => {},
  Date,
  navigator: { userAgent: 'test', hardwareConcurrency: 1 },
  screen: { width: 1920, height: 1080 },
  devicePixelRatio: 1,
  KeyboardEvent: FakeKeyboardEvent,
  document: {
    addEventListener: () => {},
    getElementById: id => elems[id] || null,
    createElement: () => new Alvo('tmp'),
  },
};
// window IS the context's global object (window = globalThis, below), so
// addEventListener/dispatchEvent set here become what window.addEventListener
// resolves to inside the script.
const windowTarget = new Alvo('window');
context.addEventListener = windowTarget.addEventListener.bind(windowTarget);
context.dispatchEvent = windowTarget.dispatchEvent.bind(windowTarget);

vm.createContext(context);
vm.runInContext('window = globalThis', context);
vm.runInContext(scriptMatch[1], context);

(function () {
  let failures = 0;
  function check(name, fn) {
    try { fn(); console.log('PASS:', name); }
    catch (e) { failures++; console.error('FAIL:', name, e.message); }
  }

  function fireMediaKey(keyCode) {
    const received = [];
    canvas.addEventListener('keydown', ev => received.push(ev));
    context.dispatchEvent(new FakeKeyboardEvent('keydown', { keyCode, isTrusted: true }));
    return received;
  }

  // OUTSIDE THE PLAYER THE KEY IS SWALLOWED, NOT TRANSLATED. player.c sets
  // window.nvPlayerAberto on open/close; without it, play/pause on the home
  // or on a context menu would become an OK on whatever is focused.
  check('with the player closed, MediaPlayPause produces no synthetic event', () => {
    context.window.nvPlayerAberto = 0;
    assert.equal(fireMediaKey(10252).length, 0);
  });

  context.window.nvPlayerAberto = 1;

  check('MediaPlayPause (10252, QN90D single play/pause key) maps to Pause', () => {
    const got = fireMediaKey(10252);
    assert.equal(got.length, 1, 'expected exactly one synthetic keydown on canvas');
    assert.equal(got[0].keyCode, 19, 'expected Pause (19), independent of focused action');
  });

  check('MediaPlay (415) maps to Pause', () => {
    assert.equal(fireMediaKey(415)[0].keyCode, 19);
  });

  check('MediaPause (19) maps to Pause', () => {
    assert.equal(fireMediaKey(19)[0].keyCode, 19);
  });

  check('Info (457) maps to the playback panel key', () => {
    const got = fireMediaKey(457);
    assert.equal(got.length, 1);
    assert.equal(got[0].keyCode, 73);
    assert.equal(got[0].key, 'i');
  });

  check('MediaStop (413) maps to Escape', () => {
    assert.equal(fireMediaKey(413)[0].keyCode, 27);
  });

  check('MediaRewind (412) maps to ArrowLeft', () => {
    assert.equal(fireMediaKey(412)[0].keyCode, 37);
  });

  check('MediaFastForward (417) maps to ArrowRight', () => {
    assert.equal(fireMediaKey(417)[0].keyCode, 39);
  });

  check('an unregistered keyCode produces no synthetic event', () => {
    assert.equal(fireMediaKey(999999).length, 0);
  });

  process.exitCode = failures ? 1 : 0;
})();
