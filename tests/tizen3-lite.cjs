// One focused gate: transpiler preservation, ES5 syntax, unsigned package, no-WASM startup.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const os = require('node:os');
const http = require('node:http');
const { chromium } = require('playwright');
const emsdk = process.env.EMSDK_DIR || path.join(os.homedir(), 'emsdk');
const acorn = require(path.join(emsdk, 'upstream/emscripten/node_modules/acorn'));
const out = 'build/tizen3-lite';
const shots = process.env.NUVIO_LITE_SHOTS || fs.mkdtempSync(path.join(os.tmpdir(), 'lite-shots-'));

async function main() {
  cp.execFileSync('bash', ['-n', 'tools/tizen.sh', 'tools/tizen3-lite.sh']);
  for (const shell of ['tools/tizen-shell.html', 'tools/tizen3-lite-shell.html']) {
    cp.execFileSync('node', ['tests/wgt_voz_shell.js', shell], {stdio:'inherit'});
  }
  const shell = fs.readFileSync('tools/tizen3-lite-shell.html', 'utf8');
  acorn.parse(shell.match(/<script>([\s\S]*?)<\/script>/)[1], {ecmaVersion:5});
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'lite-es5-'));
  try {
    const body = 'function(info) { var braces = "}"; function wasm2js_trap() { throw Error("trap"); } return info; }';
    const fixture = `var WebAssembly={Instance:function(module,info){this.exports=(${body})(info);},instantiate:function(){}}; const f=async (x)=>{try{return x?.n ?? 1;}catch{return 0;}};`;
    fs.writeFileSync(path.join(tmp, 'in.js'), fixture);
    cp.execFileSync('python3', ['tools/tizen3-es5.py', path.join(tmp, 'in.js'), path.join(tmp, 'out.js')], {stdio:'inherit'});
    const lowered = fs.readFileSync(path.join(tmp, 'out.js'), 'utf8');
    assert(lowered.includes(body), 'asm body must stay byte-identical');
    acorn.parse(lowered, {ecmaVersion:5});
  } finally { fs.rmSync(tmp, {recursive:true, force:true}); }
  const js = fs.readFileSync(`${out}/index.js`, 'utf8');
  acorn.parse(js, {ecmaVersion:5});
  const html = fs.readFileSync(`${out}/index.html`, 'utf8');
  for (const m of html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g)) acorn.parse(m[1], {ecmaVersion:5});
  assert(!fs.existsSync(`${out}/index.wasm`));
  cp.execFileSync('python3', ['-c', `
import zipfile, xml.etree.ElementTree as ET
with zipfile.ZipFile('${out}/NuvioTV003.wgt') as z:
    assert set(z.namelist()) == {'index.html','index.js','index.data','config.xml','icon.png'}
    app = ET.fromstring(z.read('config.xml')).find('{http://tizen.org/ns/widgets}application')
    assert app.attrib == {'id':'NuvioTV003.NuvioLite','package':'NuvioTV003','required_version':'3.0'}
    assert z.read('index.js') == open('${out}/index.js','rb').read()
`]);
  fs.mkdirSync(shots, {recursive:true});
  // Stream large assets over HTTP; CDP route.fulfill exceeds its message limit.
  // Server lives and closes inside this foreground test process.
  const server = http.createServer((req, res) => {
    const name = new URL(req.url, 'http://localhost').pathname.slice(1) || 'index.html';
    if (!['index.html','index.js','index.data','icon.png'].includes(name)) {
      res.writeHead(404); return res.end();
    }
    const file = path.join(out, name);
    if (!fs.existsSync(file)) { res.writeHead(404); return res.end(); }
    res.setHeader('Content-Type', name.endsWith('.js') ? 'application/javascript' : name.endsWith('.html') ? 'text/html' : 'application/octet-stream');
    fs.createReadStream(file).pipe(res);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${server.address().port}`;
  const browser = await chromium.launch({headless:true, args:[
    '--js-flags=--noexpose_wasm', '--enable-unsafe-swiftshader', '--use-angle=swiftshader'
  ]});
  const lines = [], errors = [];
  let page;
  try {
    page = await browser.newPage({viewport:{width:1280,height:720}});
    page.on('console', m => lines.push(`${m.type()}: ${m.text()}`));
    page.on('pageerror', e => { errors.push(e.message); lines.push(`pageerror: ${e.message}`); });
    await page.addInitScript(() => {
      // Chromium 145 still exposes this API with --noexpose_wasm. Remove it
      // explicitly before any app script; wasm2js must supply its own JS shim.
      window.nvWasmAfterFlag = typeof WebAssembly;
      delete window.WebAssembly;
      window.nvWasmBefore = typeof WebAssembly;
      window.OffscreenCanvas = undefined;
      window.SharedArrayBuffer = undefined;
      window.Atomics = undefined;
      window.createImageBitmap = undefined;
      window.globalThis = undefined;
      window.nvFrames = 0;
      const clear = WebGLRenderingContext.prototype.clear;
      WebGLRenderingContext.prototype.clear = function (mask) { window.nvFrames++; return clear.call(this, mask); };
    });
    await page.route('**/*', async route => {
      const u = new URL(route.request().url());
      if (u.origin !== origin) return route.abort();
      return route.continue();
    });
    await page.goto(origin, {waitUntil:'load', timeout:120000});
    await page.waitForFunction(() => window.nvLiteRuntime && window.nvFrames >= 3, null, {timeout:120000});
    await page.waitForTimeout(5000);
    const framesBefore = await page.evaluate(() => window.nvFrames);
    await page.waitForFunction(n => window.nvFrames > n + 2, framesBefore, {timeout:30000});
    const state = await page.evaluate(() => ({
      nativeWasmAfterFlag:window.nvWasmAfterFlag,
      nativeWasmAtStart:window.nvWasmBefore, runtime:window.nvLiteRuntime,
      frames:window.nvFrames, shared:typeof SharedArrayBuffer,
      offscreen:typeof OffscreenCanvas, avplay:typeof webapis,
      size:[Module.canvas.width,Module.canvas.height]
    }));
    assert.equal(state.nativeWasmAtStart, 'undefined');
    assert.equal(state.shared, 'undefined');
    assert.equal(state.offscreen, 'undefined');
    assert.equal(state.avplay, 'undefined');
    assert.deepEqual(errors, []);
    assert(lines.some(s => s.includes('orcamento de texturas: 32 MB')));
    assert(!lines.some(s => /orcamento de texturas: (?:[4-9][0-9]|[1-9][0-9]{2,}) MB/.test(s)), 'settings must not raise lite texture budget');
    // Device-profile log reports its generic 1280 value; test actual lite cap
    // across all quality settings using the production C function.
    const textureSource = fs.readFileSync('src/tex_cache.c', 'utf8');
    const hero = textureSource.match(/static int tetoDoHeroiQualidade\(void\) \{[^]*?\n\}/)[0];
    const probe = `${out}/hero-cap.c`;
    fs.writeFileSync(probe, '#include <assert.h>\n#define NV_TIZEN3_LITE 1\n#define NV_TEX_HERO_LARG_MAX 640\nstatic int qualidadeImg;\n' + hero + '\nint main(void){for(int q=0;q<3;q++){qualidadeImg=q;assert(tetoDoHeroiQualidade()==640);}}\n');
    cp.execFileSync('cc', [probe, '-o', `${out}/hero-cap`]);
    cp.execFileSync(`${out}/hero-cap`);
    await page.screenshot({path:`${shots}/no-wasm.png`});
    fs.writeFileSync(`${shots}/smoke.json`, JSON.stringify({browser:browser.version(), args:'--js-flags=--noexpose_wasm', ...state},null,2)+'\n');
    console.log('PASS: ES5 parse, asm preservation, unsigned isolated package, WebGL frames without native WASM/SAB/OffscreenCanvas');
    console.log(JSON.stringify(state));
  } catch (e) {
    if (page) await page.screenshot({path:`${shots}/failure.png`}).catch(()=>{});
    throw e;
  } finally {
    fs.writeFileSync(`${out}/smoke.log`, lines.join('\n')+'\n');
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}
main().catch(e => { console.error(e); process.exitCode=1; });
