// #205: the Tizen shell picks the wasm heap (256 -> 192 -> 160 MiB) and turns
// the pool's repeated memory errors into one action.
const fs = require('fs');
const assert = require('assert');
const html = fs.readFileSync(__dirname + '/../tools/tizen-shell.html', 'utf8');
const m = /\/\/ NV-MEM-INICIO\n([\s\S]*?)\/\/ NV-MEM-FIM/.exec(html);
assert(m, 'NV-MEM block missing from tizen-shell.html');
const { nvEscolherHeap, nvAcaoErroHeap, nvHeapAbaixo } = new Function(
  m[1] + '; return { nvEscolherHeap, nvAcaoErroHeap, nvHeapAbaixo };')();

// WebAssembly.Memory mock: throws above limitMiB, records every attempt.
function mockW(limitMiB, tries) {
  return { Memory: function (d) {
    tries.push(d.initial / 16);
    assert(d.shared === true && d.initial === d.maximum, 'shared, fixed-size memory');
    if (d.initial / 16 > limitMiB) throw new RangeError('WebAssembly.Memory(): could not allocate memory');
    this.pages = d.initial;
  } };
}
function pick(limit, dev, teto) { const t = []; const r = nvEscolherHeap(mockW(limit, t), dev, teto || 0); return { r, t }; }

let c = pick(1024, 2);                       // 1.5 GB+ TV: unchanged 256 MiB, pool 20
assert.deepStrictEqual([c.r.mib, c.r.pool, c.t], [256, 20, [256]]);
c = pick(200, 2);                            // 256 fails, 192 fits
assert.deepStrictEqual([c.r.mib, c.r.pool, c.t], [192, 12, [256, 192]]);
c = pick(170, 2);
assert.deepStrictEqual([c.r.mib, c.r.pool], [160, 8]);
c = pick(1024, 1);                           // 1 GB TV: never even tries 256
assert.deepStrictEqual([c.r.mib, c.r.pool, c.t], [192, 12, [192]]);
c = pick(1024, 1, 160);                      // stored ceiling after a worker failure
assert.deepStrictEqual([c.r.mib, c.r.pool, c.t], [160, 8, [160]]);
c = pick(100, 1);                            // nothing fits: no memory, error kept
assert(c.r.mib === 0 && !c.r.mem && /could not allocate/.test(c.r.erro.message));
assert.deepStrictEqual(c.t, [192, 160]);

// Worker storm: 17 identical errors -> exactly one action, rest ignored.
let st = { mib: 192, tratado: false };
const msg = 'Uncaught RangeError: WebAssembly.Memory(): could not allocate memory';
const acoes = [];
for (let i = 0; i < 17; i++) acoes.push(nvAcaoErroHeap(msg, st));
assert.strictEqual(acoes[0], 'baixar');
assert(acoes.slice(1).every((a) => a === 'dup'));
assert.strictEqual(nvHeapAbaixo(192), 160);
// Lowest step (or no heap at all): show the message once.
st = { mib: 160, tratado: false };
assert.deepStrictEqual([nvAcaoErroHeap(msg, st), nvAcaoErroHeap(msg, st)], ['mostrar', 'dup']);
st = { mib: 0, tratado: false };
assert.strictEqual(nvAcaoErroHeap('could not allocate memory', st), 'mostrar');
assert.strictEqual(nvAcaoErroHeap('Uncaught TypeError: x', { mib: 192, tratado: false }), null);
// Exercise the actual startup callback, including unavailable storage.
const vm = require('vm');
const startup = html.slice(html.indexOf('  var nvHeap ='), html.indexOf('\n  var Module ='));
for (const blocked of [false, true]) {
  const notices = [], saved = [];
  let reloads = 0;
  const ctx = {
    ...new Function(m[1] + '; return { nvEscolherHeap, nvAcaoErroHeap, nvHeapAbaixo };')(),
    window: {}, navigator: { deviceMemory: 1 }, console: { log() {} },
    WebAssembly: mockW(256, []),
    localStorage: {
      getItem() { return null; },
      setItem(key, value) {
        if (blocked) throw new Error('storage unavailable');
        saved.push([key, value]);
      }
    },
    location: { reload() { reloads++; } },
    document: {
      createElement() { return { style: {} }; },
      body: { appendChild(d) { notices.push(d.textContent); } },
      getElementById() { return null; }
    }
  };
  vm.runInNewContext(startup, ctx);
  for (let i = 0; i < 17; i++) ctx.window.__nvHeapErro(msg);
  assert.strictEqual(reloads, blocked ? 0 : 1, 'reload requires persisted ceiling');
  assert.strictEqual(notices.length, blocked ? 1 : 0, 'storage failure shows terminal error once');
  if (!blocked) assert.deepStrictEqual(saved, [['nv-heap-teto', '160']]);
}

// Compile real Emscripten glue/WASM with the production memory flags.
// Reusing initMemory alone misses an incompatible WASM import maximum.
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const build = fs.readFileSync(__dirname + '/../tools/tizen.sh', 'utf8');
// Evaluate the production variant setup and pool flag, including --leve.
const variant = build.slice(build.indexOf('VARIANTE=""'), build.indexOf('if [ "${1:-}" = "--alto-cache" ]'));
const poolFlag = build.match(/^  FIO_FLAGS="-pthread .*$/m)[0];
for (const leve of [false, true]) {
  const fioFlags = execFileSync('bash', ['-c', variant + '\n' + poolFlag + '\nprintf "%s" "$FIO_FLAGS"',
    'pool-test', ...(leve ? ['--leve'] : [])], { encoding: 'utf8' });
  const expression = fioFlags.match(/-sPTHREAD_POOL_SIZE='([^']+)'/)[1];
  for (const [limit, expected] of [[256, leve ? 12 : 20], [192, 12], [160, 8]]) {
    const Module = { nvPool: pick(limit, 2).r.pool };
    assert.strictEqual(vm.runInNewContext(expression, { Module }), expected,
      `${leve ? '--leve' : 'normal'} pool at ${limit} MiB`);
  }
  assert.strictEqual(vm.runInNewContext(expression, { Module: {} }), leve ? 12 : 20);
}
const flags = build.match(/^  -sINITIAL_MEMORY=.*$/m)[0].trim().replace(/\s*\\$/, '').split(/\s+/);
const incoming = build.match(/^  -sINCOMING_MODULE_JS_API=(.*?)(?:\s*\\)?$/m);
if (incoming) flags.push('-sINCOMING_MODULE_JS_API=' + incoming[1].trim().replace(/^'|'$/g, ''));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'tizen-heap-'));
try {
  fs.writeFileSync(path.join(tmp, 'main.c'), 'int main(void) { return 0; }\n');
  execFileSync('bash', ['-c', 'source "$1/emsdk_env.sh" >/dev/null 2>&1; shift; emcc "$@"',
    'heap-test', process.env.EMSDK_DIR || path.join(os.homedir(), 'emsdk'),
    path.join(tmp, 'main.c'), '-o', path.join(tmp, 'index.js'), '-pthread', '-O0', '-sASSERTIONS=0', ...flags],
    { stdio: 'inherit' });
  const glue = fs.readFileSync(path.join(tmp, 'index.js'), 'utf8');
  // VIDAA redirects MT to ST before navigation completes; glue must suppress main.
  const vidaa = fs.readFileSync(__dirname + '/../tools/vidaa-shell.html', 'utf8');
  const redirect = vidaa.slice(vidaa.indexOf('  var NV_MODO ='), vidaa.indexOf('  // DIAGNOSTICO DE ARRANQUE'));
  const noInitialRun = glue.match(/var noInitialRun = [^;]+;/)[0];
  for (const [mode, sab, isolated, suppressed] of [
    ['mt', false, true, true], ['mt', true, false, true],
    ['mt', true, true, false], ['st', false, false, false]
  ]) {
    const redirects = [];
    const ctx = {
      window: {}, self: { crossOriginIsolated: isolated }, Module: {},
      SharedArrayBuffer: sab ? SharedArrayBuffer : undefined,
      location: { pathname: '/vidaa/mt/index.html', search: '?test=1',
        replace(url) { redirects.push(url); } },
      localStorage: { setItem() {} }, console: { log() {} }
    };
    vm.runInNewContext(redirect.replace('@NUVIO_MODO@', mode) + '\n' + noInitialRun, ctx);
    assert.strictEqual(!!ctx.noInitialRun, suppressed,
      `VIDAA ${mode}: SAB=${sab}, isolated=${isolated} must preserve startup suppression`);
    assert.deepStrictEqual(redirects, suppressed ? ['/vidaa/st/index.html?test=1'] : []);
  }
  const init = glue.match(/function initMemory\(\) \{[\s\S]*?\n\}/)[0];
  const wasm = new WebAssembly.Module(fs.readFileSync(path.join(tmp, 'index.wasm')));
  for (const mib of [256, 192, 160]) {
    const selected = nvEscolherHeap(WebAssembly, 2, mib).mem;
    let allocations = 0;
    const ctx = { Module: { wasmMemory: selected }, ENVIRONMENT_IS_PTHREAD: false,
      WebAssembly: { Memory: function () { allocations++; } }, updateMemoryViews() {} };
    vm.runInNewContext(init + ';initMemory()', ctx);
    assert.strictEqual(ctx.wasmMemory, selected, 'generated glue must reuse shell heap');
    assert.strictEqual(allocations, 0, 'runtime must not allocate a second heap');
    const imports = {};
    for (const entry of WebAssembly.Module.imports(wasm)) {
      (imports[entry.module] ||= {})[entry.name] = entry.kind === 'memory' ? selected : () => 0;
    }
    new WebAssembly.Instance(wasm, imports);
    assert.strictEqual(selected.buffer.byteLength, mib * 1024 * 1024);
    assert.throws(() => selected.grow(1), RangeError, 'shell heap remains fixed-size');
  }
} finally {
  fs.rmSync(tmp, { recursive: true, force: true });
}
console.log('PASS tizen-heap-fallback (startup, generated glue, WASM imports: 256/192/160 MiB)');
