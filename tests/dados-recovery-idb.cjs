const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'src/dados.c'), 'utf8');

function emAsyncBody(name) {
  const start = source.indexOf(`EM_ASYNC_JS(int, ${name},`);
  if (start < 0) throw new Error(`EM_ASYNC_JS ${name} not found`);
  const brace = source.indexOf('{', start);
  const end = source.indexOf('\n});', brace);
  if (brace < 0 || end < 0) throw new Error(`EM_ASYNC_JS ${name} is malformed`);
  return source.slice(brace + 1, end);
}

function emJsBody(name) {
  const start = source.indexOf(`EM_JS(void, ${name},`);
  if (start < 0) throw new Error(`EM_JS ${name} not found`);
  const brace = source.indexOf('{', start);
  const end = source.indexOf('\n});', brace);
  if (brace < 0 || end < 0) throw new Error(`EM_JS ${name} is malformed`);
  return source.slice(brace + 1, end);
}

function openDb(page, name, version, upgrade) {
  return page.evaluate(({ name, version, upgrade }) => new Promise((resolve, reject) => {
    const req = version ? indexedDB.open(name, version) : indexedDB.open(name);
    req.onupgradeneeded = () => { if (upgrade) req.result.createObjectStore('FILE_DATA'); };
    req.onsuccess = () => { req.result.close(); resolve(true); };
    req.onerror = () => reject(String(req.error));
  }), { name, version, upgrade });
}

async function readRecord(page, dbName, key) {
  return page.evaluate(({ dbName, key }) => new Promise((resolve, reject) => {
    const req = indexedDB.open(dbName);
    req.onerror = () => reject(String(req.error));
    req.onsuccess = () => {
      const db = req.result;
      const tx = db.transaction('FILE_DATA', 'readonly');
      const get = tx.objectStore('FILE_DATA').get(key);
      get.onsuccess = () => { const value = get.result; db.close(); resolve(value || null); };
      get.onerror = () => reject(String(get.error));
    };
  }), { dbName, key });
}

async function main() {
  const server = http.createServer((_req, res) => {
    res.writeHead(200, { 'content-type': 'text/html' });
    res.end('<!doctype html><title>dados recovery harness</title>');
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const chromePath = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
  const browser = await chromium.launch({
    ...(fs.existsSync(chromePath) ? { executablePath: chromePath } : {}),
    headless: true,
  });
  try {
    const page = await browser.newPage();
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await openDb(page, '/nuvio', 21, true);
    await page.evaluate(() => new Promise((resolve, reject) => {
      const dbReq = indexedDB.open('/nuvio');
      dbReq.onsuccess = () => {
        const db = dbReq.result;
        const tx = db.transaction('FILE_DATA', 'readwrite');
        const store = tx.objectStore('FILE_DATA');
        store.put({ timestamp: 1, mode: 32768, contents: new Uint8Array([83, 69, 83, 83, 65, 79]) }, '/nuvio/sessao.txt');
        store.put({ timestamp: 1, mode: 32768, contents: new Uint8Array([255, 216, 255]) }, '/nuvio/cache/01abcdef.jpg');
        store.put({ timestamp: 1, mode: 32768, contents: new Uint8Array([1, 2, 3]) }, '/nuvio/cache/estado.dat');
        tx.oncomplete = () => { db.close(); resolve(); };
        tx.onerror = () => reject(String(tx.error));
      };
      dbReq.onerror = () => reject(String(dbReq.error));
    }));

    const limparBody = emAsyncBody('nv_idbfs_limpar_cache_arte');
    await page.addScriptTag({ content: `window.UTF8ToString = function(p) { return p; }; window.nv_idbfs_limpar_cache_arte = async function(ponto) {${limparBody}\n}; window.__limparArte = window.nv_idbfs_limpar_cache_arte;` });
    const removed = await page.evaluate(() => window.__limparArte('/nuvio'));
    if (removed !== 1) throw new Error(`expected one legacy art record removed, got ${removed}`);
    if (!(await readRecord(page, '/nuvio', '/nuvio/sessao.txt'))) throw new Error('session record was lost');
    if (await readRecord(page, '/nuvio', '/nuvio/cache/01abcdef.jpg')) throw new Error('legacy image record remains');
    if (!(await readRecord(page, '/nuvio', '/nuvio/cache/estado.dat'))) throw new Error('non-image record under cache was removed');

    const mountBody = emAsyncBody('nv_idbfs_montar');
    const syncBody = emJsBody('nv_idbfs_gravar');
    await page.addScriptTag({ content: `
      window.Module = { nvRecoveryMode: true };
      window.__nvModoRecuperacao = true;
      window.IDBFS = {};
      window.out = function() {};
      window.FS = {
        mkdirTree: function() {}, mount: function() {},
        syncfs: function(populate, callback) {
          window.__populate = populate;
          setTimeout(function() { callback(null); }, 0);
        }
      };
      window.__montar = async function(ponto, nArte) {${mountBody}\n};
      window.__gravar = function(tipo) {${syncBody}\n};
    ` });
    const mounted = await page.evaluate(() => window.__montar('/nuvio', 0));
    if (mounted !== 1 || !(await page.evaluate(() => window.__populate))) throw new Error('IDBFS recovery mount failed');
    if (!(await readRecord(page, '/nuvio', '/nuvio/sessao.txt'))) throw new Error('session did not survive recovery mount');

    await page.evaluate(() => {
      let calls = 0;
      window.FS.syncfs = function(populate, callback) {
        if (populate) { callback(null); return; }
        calls++;
        if (calls === 1) { setTimeout(() => callback(new Error('injected quota failure')), 0); return; }
        const req = indexedDB.open('/nuvio');
        req.onsuccess = () => {
          const db = req.result;
          const tx = db.transaction('FILE_DATA', 'readwrite');
          tx.objectStore('FILE_DATA').put({ timestamp: 2, mode: 32768, contents: new Uint8Array([111, 107]) }, '/nuvio/retry.txt');
          tx.oncomplete = () => { db.close(); callback(null); };
          tx.onerror = () => callback(tx.error);
        };
        req.onerror = () => callback(req.error);
      };
      window.__gravar(1);
    });
    await page.waitForFunction(() => Module.nvSyncResultado === -1 && Module.nvSyncEmVoo === 0);
    const delay = await page.evaluate(() => Module.nvSyncPodeEm - Date.now());
    if (delay <= 0 || delay > 500) throw new Error(`unexpected retry backoff: ${delay}`);
    await page.waitForTimeout(delay + 50);
    const canRetry = await page.evaluate(() => Date.now() >= Module.nvSyncPodeEm);
    if (!canRetry) throw new Error('finite sync retry backoff did not expire');
    await page.evaluate(() => window.__gravar(1));
    await page.waitForFunction(() => Module.nvSyncResultado === 1 && Module.nvSyncEmVoo === 0);
    const retry = await readRecord(page, '/nuvio', '/nuvio/retry.txt');
    if (!retry) throw new Error('second write did not reach IndexedDB after retry');
    console.log('PASS recovery preserves session/non-image records, removes only legacy artwork, and syncfs retry persists the second write');
  } finally {
    await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
