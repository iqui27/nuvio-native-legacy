// Exercises the production EM_JS bridge of src/video_html5.c (the VIDAA/HTML5
// video target) against a fake document/<video> written for this test, the
// same way tests/tizen-avplay-contract.cjs exercises video_tizen.c against a
// fake webapis.avplay. This covers the JS bridge logic and state machine, not
// the C parser, not real Chromium HLS/MSE behavior, and not TV compositing —
// nobody has a VIDAA TV to check any of that against.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const source = fs.readFileSync('src/video_html5.c', 'utf8');
const body = source.match(/EM_JS\(double, nv_av,[\s\S]*?int dstTam\), \{([\s\S]*?)\n\}\);/);
assert(body, 'production bridge not found');

// ============================================================================
// Fake DOM: just enough of document/<video>/<div>/<script> for the bridge to
// run against. Plain Node objects/functions, passed into the vm context the
// same way tests/tizen-avplay-contract.cjs passes console/setTimeout/etc.
// ============================================================================

function makeGenericEl(tag) {
  const el = {
    tagName: tag,
    style: {},
    attrs: {},
    children: [],
    parentNode: null,
    setAttribute(k, v) { el.attrs[k] = v; },
    getAttribute(k) { return el.attrs[k]; },
    appendChild(child) { child.parentNode = el; el.children.push(child); return child; },
    removeChild(child) {
      const i = el.children.indexOf(child);
      if (i >= 0) el.children.splice(i, 1);
      child.parentNode = null;
      return child;
    },
  };
  return el;
}

function makeVideoEl() {
  const listeners = {};
  const el = Object.assign(makeGenericEl('video'), {
    src: '',
    currentTime: 0,
    duration: NaN,
    videoWidth: 0,
    videoHeight: 0,
    buffered: { length: 0, start: () => 0, end: () => 0 },
    error: null,
    paused: true,
    volume: 1,
    muted: true,
    playsInline: false,
    preload: '',
    controls: true,
    audioTracks: undefined,
    textTracks: undefined,
    playCalls: 0,
    pauseCalls: 0,
    loadCalls: 0,
    removeAttrCalls: [],
    __canPlayTypeAnswer: '',
    addEventListener(type, fn) { (listeners[type] = listeners[type] || []).push(fn); },
    removeEventListener(type, fn) {
      if (!listeners[type]) return;
      const i = listeners[type].indexOf(fn);
      if (i >= 0) listeners[type].splice(i, 1);
    },
    __fire(type) { (listeners[type] || []).slice().forEach(fn => fn.call(el)); },
    play() { el.playCalls++; el.paused = false; return Promise.resolve(); },
    pause() { el.pauseCalls++; el.paused = true; },
    load() { el.loadCalls++; },
    removeAttribute(name) { el.removeAttrCalls.push(name); if (name === 'src') el.src = ''; },
    canPlayType() { return el.__canPlayTypeAnswer; },
  });
  return el;
}

function makeAudioTrack(language, label) {
  return { language, label, enabled: false };
}

function makeTextTrack(language, label) {
  const cueListeners = [];
  return {
    language, label, mode: 'disabled', activeCues: null,
    addEventListener(type, fn) { if (type === 'cuechange') cueListeners.push(fn); },
    __fireCue(texto) {
      this.activeCues = texto ? [{ text: texto }] : [];
      cueListeners.slice().forEach(fn => fn.call(this));
    },
  };
}

function FakeHls(config) {
  this.config = config;
  this._handlers = {};
  this.destroyed = false;
  this.loadSourceCalls = [];
  this.attachMediaCalls = [];
  FakeHls.ultimaInstancia = this;
}
FakeHls.prototype.on = function (ev, fn) { (this._handlers[ev] = this._handlers[ev] || []).push(fn); };
FakeHls.prototype.loadSource = function (url) { this.loadSourceCalls.push(url); };
FakeHls.prototype.attachMedia = function (v) { this.attachMediaCalls.push(v); };
FakeHls.prototype.destroy = function () { this.destroyed = true; };
FakeHls.prototype.__fire = function (ev, data) { (this._handlers[ev] || []).slice().forEach(fn => fn(ev, data)); };
FakeHls.Events = { ERROR: 'hlsError', MANIFEST_PARSED: 'hlsManifestParsed' };

const scripts = [];
const canvasEl = { getBoundingClientRect: () => ({ left: 100, top: 50, width: 1280, height: 720 }) };
const bodyEl = makeGenericEl('body');
const headEl = makeGenericEl('head');
const docEl = makeGenericEl('html');

const document = {
  createElement(tag) {
    if (tag === 'video') return makeVideoEl();
    return makeGenericEl(tag);
  },
  getElementById(id) { return id === 'canvas' ? canvasEl : null; },
  querySelector(sel) {
    if (sel === 'script[data-nv-hls]') {
      return scripts.find(s => s.attrs['data-nv-hls'] !== undefined) || null;
    }
    return null;
  },
  body: bodyEl,
  head: headEl,
  documentElement: docEl,
};
// Track every <script> created, for the "reuses the tag" assertion.
const realCreateScript = document.createElement.bind(document);
document.createElement = function (tag) {
  const el = realCreateScript(tag);
  if (tag === 'script') scripts.push(el);
  return el;
};

let clock = 0;
const outputs = new Map();
const context = vm.createContext({
  console,
  document,
  performance: { now: () => (clock += 1) },
  UTF8ToString: value => value,
  stringToUTF8: (text, address) => outputs.set(address, text),
  HEAPF64: new Float64Array(64),
});
vm.runInContext('window = globalThis', context);
vm.runInContext('bridge = function(cmd, txt, a, b, c, d, dst, dstTam) {' +
  body[1].replace(/\\\\/g, '\\') + '\n}', context);
const call = (op, text = '', a = 0, b = 0, c = 0, d = 0, dst = 0, size = 0) =>
  context.bridge(op, text, a, b, c, d, dst, size);

// EST_* offsets, must match the enum in src/video_html5.c.
const EST = { POS: 0, DUR: 1, TOCANDO: 2, PRONTO: 3, ABERTO: 4, LARG: 5, ALT: 6,
              ERRO: 7, FIM: 8, BUFFIM: 9, BUFMS: 10, ERRCODE: 11 };
function lerEstado() {
  outputs.delete(8);
  call('estado', '', 0, 0, 0, 0, 8, 96);
  const o = 1; // dst=8 bytes -> offset 1 double
  const h = context.HEAPF64;
  return {
    pos: h[o + EST.POS], dur: h[o + EST.DUR], tocando: h[o + EST.TOCANDO],
    pronto: h[o + EST.PRONTO], aberto: h[o + EST.ABERTO], larg: h[o + EST.LARG],
    alt: h[o + EST.ALT], erro: h[o + EST.ERRO], fim: h[o + EST.FIM],
    bufFim: h[o + EST.BUFFIM], bufMs: h[o + EST.BUFMS], erroCodigo: h[o + EST.ERRCODE],
  };
}

(async () => {
  let failures = 0;
  function check(name, fn) {
    try { fn(); console.log('PASS:', name); }
    catch (e) { failures++; console.error('FAIL:', name, e.message); }
  }

  check('disp reports a working <video> element', () => {
    assert.equal(call('disp'), 1);
  });

  // --- abertura progressiva (nao-HLS) ----------------------------------
  assert.equal(call('abrir', 'https://cdn.example.invalid/filme.mp4?tok=segredo'), 1);
  let v1 = context.__nvav.v;
  check('abrir creates a <video> wrapped in a div appended to the page', () => {
    assert.ok(v1, 'video element missing');
    assert.equal(v1.src, 'https://cdn.example.invalid/filme.mp4?tok=segredo');
    assert.equal(v1.muted, false, 'main player must not be muted, unlike trailer.c');
    assert.ok(bodyEl.children.length >= 1);
    assert.ok(v1.playCalls >= 1, 'video_tocar must call play()');
  });
  check('non-.m3u8 source takes the plain progressive path (no Hls instance)', () => {
    assert.equal(context.__nvav.hls, null);
  });

  v1.videoWidth = 1920; v1.videoHeight = 800; v1.duration = 7182.4;
  v1.__fire('loadedmetadata');
  check('loadedmetadata publishes dimensions and duration to estado', () => {
    const e = lerEstado();
    assert.equal(e.larg, 1920);
    assert.equal(e.alt, 800);
    assert.equal(e.pronto, 1);
  });

  v1.__fire('playing');
  check('playing sets tocando and clears any buffering clock', () => {
    assert.equal(lerEstado().tocando, 1);
  });

  v1.__fire('waiting');
  const antesDoBuffer = lerEstado();
  check('waiting starts a buffering clock (bufferando_ms > 0 while it runs)', () => {
    assert.ok(antesDoBuffer.bufMs > 0, 'bufMs should already be > 0 on the next read');
    assert.equal(antesDoBuffer.tocando, 1, 'tocando is not cleared by waiting alone');
  });
  v1.__fire('playing');
  check('playing again clears the buffering clock', () => {
    assert.equal(lerEstado().bufMs, 0);
  });

  v1.buffered = { length: 1, start: () => 0, end: () => 42.5 };
  v1.currentTime = 10;
  check('buffer_fim reads the buffered range containing currentTime', () => {
    assert.equal(lerEstado().bufFim, 42.5);
  });

  v1.__fire('ended');
  check('ended is exposed as terminou', () => {
    const e = lerEstado();
    assert.equal(e.fim, 1);
    assert.equal(e.tocando, 0);
  });

  // --- generation guard: stale events from a closed session must be ignored
  const v1Handlers = v1;
  assert.equal(call('abrir', 'https://cdn.example.invalid/segundo.mp4'), 1);
  check('a new abrir replaces the session (new <video> element)', () => {
    assert.notEqual(context.__nvav.v, v1Handlers);
  });
  v1Handlers.error = { code: 3, message: 'stale decode error' };
  v1Handlers.__fire('error');
  check('a stale error event from a previous session does not contaminate the current one', () => {
    assert.equal(lerEstado().erro, 0);
  });

  // --- parar: teardown removes the element from the page ----------------
  const v2 = context.__nvav.v;
  const wrap2 = context.__nvav.wrap;
  assert.equal(call('parar'), 1);
  check('parar pauses, clears src, calls load() and removes the element', () => {
    assert.ok(v2.pauseCalls >= 1);
    assert.ok(v2.removeAttrCalls.includes('src'));
    assert.ok(v2.loadCalls >= 1);
    assert.equal(wrap2.parentNode, null, 'wrapper must be detached from the page');
    assert.equal(context.__nvav.v, null);
  });

  // --- faixas: audio/text track listing and hidden-mode subtitle selection
  assert.equal(call('abrir', 'https://cdn.example.invalid/serie.mkv'), 1);
  const v3 = context.__nvav.v;
  v3.audioTracks = [makeAudioTrack('eng', 'English'), makeAudioTrack('por', 'Portugues')];
  v3.textTracks = [makeTextTrack('', ''), makeTextTrack('por', 'Forced')];
  call('faixas', '', 0, 0, 0, 0, 100, 4096);
  check('faixas lists both audioTracks and textTracks in the tizen-style row format', () => {
    const out = outputs.get(100);
    assert.match(out, /A\t0\teng\tEnglish\t-1/);
    assert.match(out, /A\t1\tpor\tPortugues\t-1/);
    assert.match(out, /T\t0\t\t\t-1/);
    assert.match(out, /T\t1\tpor\tForced\t-1/);
  });

  check('when the browser exposes no audioTracks/textTracks, faixas degrades to zero rows', () => {
    assert.equal(call('parar'), 1);
    assert.equal(call('abrir', 'https://cdn.example.invalid/sem-faixas.mp4'), 1);
    // v.audioTracks/textTracks stay undefined (never set), matching a real
    // browser that doesn't implement those APIs.
    const n = call('faixas', '', 0, 0, 0, 0, 150, 256);
    assert.equal(n, 0);
  });

  assert.equal(call('abrir', 'https://cdn.example.invalid/serie2.mkv'), 1);
  const v4 = context.__nvav.v;
  v4.audioTracks = [makeAudioTrack('eng', '')];
  v4.textTracks = [makeTextTrack('eng', 'English'), makeTextTrack('por', 'Portugues')];
  call('faixa', 'TEXT', 1);
  check('selecting a text track sets mode to hidden, never showing (the app draws it)', () => {
    assert.equal(v4.textTracks[0].mode, 'disabled');
    assert.equal(v4.textTracks[1].mode, 'hidden');
  });
  v4.textTracks[1].__fireCue('Ola <i>mundo</i>');
  check('the active cue text is exposed through leg_texto while it is current', () => {
    outputs.delete(200);
    const n = call('leg_texto', '', 0, 0, 0, 0, 200, 512);
    assert.equal(outputs.get(200), 'Ola <i>mundo</i>');
    assert.ok(n >= 2);
  });
  v4.textTracks[1].__fireCue('');
  check('cuechange with no active cue clears the text', () => {
    outputs.delete(201);
    call('leg_texto', '', 0, 0, 0, 0, 201, 512);
    assert.equal(outputs.get(201), '');
  });
  v4.textTracks[1].__fireCue('de volta');
  call('leg_mudo', '', 1);
  check('leg_mudo(1) disables every text track and clears the text immediately', () => {
    assert.equal(v4.textTracks[1].mode, 'disabled');
    outputs.delete(202);
    call('leg_texto', '', 0, 0, 0, 0, 202, 512);
    assert.equal(outputs.get(202), '');
  });

  // --- HLS decision: native canPlayType vs hls.js vs unsupported ---------
  assert.equal(call('parar'), 1);
  {
    const antes = context.__nvav.v;
    void antes;
  }
  // native support: canPlayType answers non-empty -> straight to v.src, no Hls.
  const origCreateElementNative = document.createElement;
  document.createElement = function (tag) {
    const el = origCreateElementNative(tag);
    if (tag === 'video') el.__canPlayTypeAnswer = 'probably';
    return el;
  };
  const rNativo = call('abrir', 'https://cdn.example.invalid/live/index.m3u8?sig=abc');
  check('an .m3u8 source with native canPlayType support uses the native path', () => {
    assert.equal(rNativo, 2);
    assert.equal(context.__nvav.hls, null);
    assert.equal(context.__nvav.v.src, 'https://cdn.example.invalid/live/index.m3u8?sig=abc');
  });
  document.createElement = origCreateElementNative;

  // no native support, MediaSource present, Hls already "loaded" (window.Hls set):
  // must go straight to hls.js without touching the <script> tag machinery.
  call('parar');
  context.MediaSource = function () {};
  context.Hls = FakeHls;
  const scriptsAntes = scripts.length;
  const rHlsJs = call('abrir', 'https://cdn.example.invalid/live/master.m3u8');
  check('an .m3u8 source without native support, with MediaSource and Hls already present, uses hls.js', () => {
    assert.equal(rHlsJs, 3);
    assert.ok(context.__nvav.hls, 'an Hls instance should have been created');
    assert.equal(scripts.length, scriptsAntes, 'hls.min.js must not be fetched again once window.Hls exists');
    // deepStrictEqual would compare prototypes too, and the config object was
    // built inside the vm context (a different realm, different Object.prototype)
    // — compare the three fields we actually care about instead.
    const cfg = context.__nvav.hls.config;
    assert.equal(cfg.maxBufferLength, 30);
    assert.equal(cfg.maxMaxBufferLength, 60);
    assert.equal(cfg.enableWorker, true);
    assert.equal(context.__nvav.hls.loadSourceCalls[0], 'https://cdn.example.invalid/live/master.m3u8');
    assert.equal(context.__nvav.hls.attachMediaCalls[0], context.__nvav.v);
  });
  context.__nvav.hls.__fire(FakeHls.Events.MANIFEST_PARSED);
  check('MANIFEST_PARSED triggers play()', () => {
    assert.ok(context.__nvav.v.playCalls >= 1);
  });
  const hlsAtivo = context.__nvav.hls;
  hlsAtivo.__fire(FakeHls.Events.ERROR, { fatal: true, type: 'networkError', details: 'manifestLoadError' });
  check('a fatal hls.js error is mapped to video_falhou (erro=1) and destroys the instance', () => {
    const e = lerEstado();
    assert.equal(e.erro, 1);
    assert.ok(hlsAtivo.destroyed);
    assert.equal(context.__nvav.hls, null);
    outputs.delete(300);
    call('erro_msg', '', 0, 0, 0, 0, 300, 256);
    assert.match(outputs.get(300), /hls\.js fatal: networkError\/manifestLoadError/);
    assert.doesNotMatch(outputs.get(300), /example\.invalid|https?:/);
  });

  // no native support, no window.Hls yet: must lazy-load tools/vendor/hls.min.js
  // by injecting exactly one <script data-nv-hls> tag and reusing it on a
  // second open, matching "ignore if already loaded" from the brief.
  call('parar');
  delete context.Hls;
  const scriptsAntesLazy = scripts.length;
  const rLazy1 = call('abrir', 'https://cdn.example.invalid/live/lazy1.m3u8');
  check('without window.Hls yet, abrir injects exactly one <script src=hls.min.js>', () => {
    assert.equal(rLazy1, 3);
    assert.equal(scripts.length, scriptsAntesLazy + 1);
    const sc = scripts[scripts.length - 1];
    assert.equal(sc.src, 'hls.min.js');
    assert.equal(sc.attrs['data-nv-hls'], '1');
    assert.ok(headEl.children.includes(sc) || docEl.children.includes(sc));
    assert.equal(context.__nvav.hls, null, 'must wait for the script to load before creating Hls');
  });
  context.Hls = FakeHls;
  const scLazy = scripts[scripts.length - 1];
  scLazy.onload();
  check('once the script "loads", the pending session initializes hls.js', () => {
    assert.ok(context.__nvav.hls, 'Hls instance should now exist');
  });
  call('parar');
  const scriptsAntesReuso = scripts.length;
  const rLazy2 = call('abrir', 'https://cdn.example.invalid/live/lazy2.m3u8');
  check('a second .m3u8 open reuses the existing <script> tag instead of injecting another', () => {
    assert.equal(rLazy2, 3);
    assert.equal(scripts.length, scriptsAntesReuso, 'no new <script> should have been created');
  });

  // no native support, no MediaSource at all: must fail outright with a logged reason.
  call('parar');
  delete context.MediaSource;
  delete context.Hls;
  const rSemMSE = call('abrir', 'https://cdn.example.invalid/live/sem-mse.m3u8');
  check('an .m3u8 source with neither native support nor MediaSource fails cleanly', () => {
    assert.equal(rSemMSE, 0);
    const e = lerEstado();
    assert.equal(e.erro, 1);
    outputs.delete(400);
    call('erro_msg', '', 0, 0, 0, 0, 400, 256);
    assert.match(outputs.get(400), /MediaSource/);
  });

  call('parar');
  process.exitCode = failures ? 1 : 0;
})();
