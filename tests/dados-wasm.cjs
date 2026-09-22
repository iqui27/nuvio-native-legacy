// Exercises the compiled production C/JS boundary and real IDBFS.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawnSync } = require('node:child_process');
const { chromium } = require('playwright');

(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nuvio-dados-wasm-'));
  let server, browser;
  try {
    const emcc = process.env.EMCC || path.join(os.homedir(), 'emsdk/upstream/emscripten/emcc');
    const build = spawnSync(emcc, [path.resolve(__dirname, '../src/dados.c'), '-pthread',
      '-sASYNCIFY=1', '-lidbfs.js', '--no-entry', '-sALLOW_MEMORY_GROWTH=1',
      '-sEXPORTED_FUNCTIONS=["_dados_iniciar","_dados_gravar","_dados_sincronizar","_dados_persistente"]',
      '-sEXPORTED_RUNTIME_METHODS=["ccall","FS"]', '-o', path.join(dir, 'dados.js')], { encoding: 'utf8' });
    if (build.status !== 0) throw new Error(build.stderr || build.error || 'emcc failed');
    server = http.createServer((req, res) => {
      res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
      res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
      const filename = path.basename(req.url.split('?')[0]);
      if (filename === 'dados.js' || filename === 'dados.wasm') {
        res.setHeader('Content-Type', filename.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
        res.end(fs.readFileSync(path.join(dir, filename)));
      } else { res.setHeader('Content-Type', 'text/html'); res.end('<!doctype html><title>dados wasm</title>'); }
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const chrome = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch({ headless: true, ...(fs.existsSync(chrome) ? { executablePath: chrome } : {}) });
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(String(error)));
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.evaluate(() => new Promise((resolve, reject) => {
      const r = indexedDB.open('/nuvio', 21);
      r.onupgradeneeded = () => {
        const s = r.result.createObjectStore('FILE_DATA');
        s.createIndex('timestamp', 'timestamp');
      };
      r.onerror = () => reject(r.error);
      r.onsuccess = () => {
        const db = r.result, tx = db.transaction('FILE_DATA', 'readwrite'), s = tx.objectStore('FILE_DATA');
        for (const name of ['/nuvio', '/nuvio/cache']) s.put({ timestamp: new Date(), mode: 16895 }, name);
        s.put({ timestamp: new Date(), mode: 33206, contents: new TextEncoder().encode('fixture-session') }, '/nuvio/sessao.txt');
        s.put({ timestamp: new Date(), mode: 33206, contents: new Uint8Array([255,216,255]) }, '/nuvio/cache/fixture.jpg');
        tx.oncomplete = () => { db.close(); resolve(); };
        tx.onerror = () => reject(tx.error);
      };
    }));
    await page.evaluate(() => {
      window.__nvModoRecuperacao = true;
      window.Module = { onRuntimeInitialized() { window.ready = true; } };
    });
    await page.addScriptTag({ url: '/dados.js' });
    await page.waitForFunction(() => window.ready);
    await page.evaluate(async () => {
      await Module.ccall('dados_iniciar', null, ['string'], ['/unused'], { async: true });
      if (!Module.ccall('dados_persistente', 'number', [], [])) throw new Error('IDBFS did not mount');
      if (Module.FS.readFile('/nuvio/sessao.txt', { encoding: 'utf8' }) !== 'fixture-session') throw new Error('session lost');
      if (Module.FS.analyzePath('/nuvio/cache/fixture.jpg').exists) throw new Error('legacy image hydrated');
      const original = Module.FS.syncfs.bind(Module.FS);
      let first = true;
      Module.FS.syncfs = (populate, done) => {
        if (first && !populate) { first = false; setTimeout(() => done(new Error('fixture failure')), 0); }
        else original(populate, done);
      };
      Module.ccall('dados_gravar', 'number', ['string', 'string'], ['retry.txt', 'persisted']);
      window.poll = setInterval(() => Module.ccall('dados_sincronizar', null, [], []), 50);
    });
    await page.waitForFunction(() => Module.nvSyncFalhasTentativa === 1);
    // A new write during backoff must survive the successful retry as well.
    await page.evaluate(() => Module.ccall('dados_gravar', 'number', ['string', 'string'], ['second.txt', 'newer']));
    await page.waitForFunction(() => Module.nvSyncFalhasTentativa === 0 && !Module.nvSyncEmVoo);
    await page.evaluate(() => clearInterval(window.poll));
    await page.evaluate(() => {
      Module.ccall('dados_gravar', 'number', ['string', 'string'], ['exit.txt', 'last-write']);
      dispatchEvent(new Event('pagehide'));
    });
    await page.waitForFunction(() => !Module.nvSyncEmVoo);
    await page.evaluate(() => new Promise((resolve, reject) => {
      const r = indexedDB.open('/nuvio');
      r.onsuccess = () => {
        const db = r.result, tx = db.transaction('FILE_DATA'), store = tx.objectStore('FILE_DATA');
        for (const [key, text] of [['retry.txt', 'persisted'], ['second.txt', 'newer'], ['sessao.txt', 'fixture-session'], ['exit.txt', 'last-write']]) {
          const g = store.get('/nuvio/' + key);
          g.onsuccess = () => {
            if (!g.result || new TextDecoder().decode(g.result.contents) !== text) reject(new Error('missing durable ' + key));
          };
        }
        tx.oncomplete = () => { db.close(); resolve(); };
        tx.onerror = () => reject(tx.error);
      };
      r.onerror = () => reject(r.error);
    }));
    if (errors.length) throw new Error(errors.join('\n'));
    console.log('PASS production WASM recovery preserves session and dirty writes retry into real IDBFS');
  } finally {
    if (browser) await browser.close();
    if (server) await new Promise(resolve => server.close(resolve));
    fs.rmSync(dir, { recursive: true, force: true });
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
