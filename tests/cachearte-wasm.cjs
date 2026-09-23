#!/usr/bin/env node
const assert = require('assert');
const fs = require('fs');
const http = require('http');
const path = require('path');
const { chromium } = require('playwright');

const dir = process.argv[2];
if (!dir) throw new Error('usage: node tests/cachearte-wasm.cjs <compiled module directory>');
const html = `<!doctype html><meta charset="utf-8"><title>cachearte WASM bridge</title>
<script src="/cachearte.js"></script><script>
const stage = Number(new URLSearchParams(location.search).get('stage'));
const originalTransaction = IDBDatabase.prototype.transaction;
const originalPut = IDBObjectStore.prototype.put;
IDBObjectStore.prototype.put = function(...args) {
  const tx = this.transaction;
  const result = originalPut.apply(this, args);
  if (window.__abortNextCacheRead && tx.objectStoreNames.contains('meta') &&
      tx.objectStoreNames.contains('blob')) {
    window.__abortNextCacheRead = false;
    queueMicrotask(() => { try { tx.abort(); } catch (_) {} });
  }
  return result;
};
IDBDatabase.prototype.transaction = function(...args) {
  if (window.__quotaNextWrite && args[1] === 'readwrite' &&
      Array.isArray(args[0]) && args[0].includes('blob')) {
    window.__quotaNextWrite = false;
    throw new DOMException('injected storage quota exhaustion', 'QuotaExceededError');
  }
  const tx = originalTransaction.apply(this, args);
  if (window.__abortNextCacheWrite && args[1] === 'readwrite' &&
      Array.isArray(args[0]) && args[0].includes('blob')) {
    window.__abortNextCacheWrite = false;
    queueMicrotask(() => { try { tx.abort(); } catch (_) {} });
  }
  return tx;
};
createCacheModule().then(m => {
  window.Module = m;
  m._test_start(stage);
  const poll = () => {
    if (m._test_done()) {
      window.__cacheTest = { result: m._test_result(), stage };
      return;
    }
    setTimeout(poll, 10);
  };
  poll();
}).catch(e => { window.__cacheError = String(e && e.stack || e); });
</script>`;
const server = http.createServer((req, res) => {
  res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
  res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
  if (req.url.startsWith('/cachearte.js')) {
    res.setHeader('content-type', 'text/javascript');
    res.end(fs.readFileSync(path.join(dir, 'cachearte.js')));
  } else if (req.url.startsWith('/cachearte.wasm')) {
    res.setHeader('content-type', 'application/wasm');
    res.end(fs.readFileSync(path.join(dir, 'cachearte.wasm')));
  } else {
    res.setHeader('content-type', 'text/html; charset=utf-8');
    res.end(html);
  }
});

(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({
    headless: true,
    ...(fs.existsSync(process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')
      ? { executablePath: process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' } : {})
  });
  const page = await browser.newPage();
  page.on('pageerror', error => { throw error; });
  page.on('console', message => { if (message.type() === 'error') console.error(message.text()); });
  const base = `http://127.0.0.1:${server.address().port}`;
  await page.goto(`${base}/reset`);
  async function runStage(stage) {
    await page.goto(`${base}/?stage=${stage}`);
    await page.waitForFunction(() => window.__cacheTest || window.__cacheError, null, { timeout: 15000 });
    const state = await page.evaluate(() => ({ test: window.__cacheTest, error: window.__cacheError }));
    if (state.error) throw new Error(state.error);
    assert.deepStrictEqual(state.test, { result: 1, stage }, `production WASM bridge stage ${stage}`);
  }

  await page.evaluate(() => new Promise(resolve => {
    const r = indexedDB.deleteDatabase('nuvio-art-v1');
    r.onsuccess = r.onerror = r.onblocked = resolve;
  }));
  await runStage(1);
  const schema = await page.evaluate(async () => {
    const db = await new Promise((resolve, reject) => {
      const r = indexedDB.open('nuvio-art-v1', 1);
      r.onerror = () => reject(r.error); r.onsuccess = () => resolve(r.result);
    });
    const key = 'https://images.example/art.jpg?token=opaque&rev=1\n1';
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(['meta', 'blob'], 'readonly');
      const meta = tx.objectStore('meta').get(key), blob = tx.objectStore('blob').get(key);
      tx.oncomplete = () => resolve({ meta: meta.result, blob: blob.result });
      tx.onerror = () => reject(tx.error);
    });
  });
  assert(schema.meta && schema.meta.essential, 'essential refs live in metadata');
  assert.deepStrictEqual(Object.keys(schema.blob).sort(), ['bytes', 'key'], 'blob body has no pin flags');
  await runStage(2);
  await page.evaluate(() => new Promise((resolve, reject) => {
    window.__abortNextCacheWrite = true;
    Module._test_abort_write();
    const poll = () => {
      if (Module._test_abort_done()) {
        if (Module._test_abort_result() !== 10) reject(new Error(`aborted write must count one failure and release queue bytes once; delta=${Module._test_abort_result()}`));
        else resolve();
      } else setTimeout(poll, 10);
    };
    poll();
  }));
  await page.evaluate(async () => {
    const db = await new Promise((resolve, reject) => {
      const r = indexedDB.open('nuvio-art-v1', 1);
      r.onerror = () => reject(r.error); r.onsuccess = () => resolve(r.result);
    });
    await new Promise((resolve, reject) => {
      const tx = db.transaction(['meta', 'blob'], 'readwrite');
      const key = 'https://images.example/evictable.jpg?rev=1\n1';
      tx.objectStore('meta').put({ key, url: 'https://images.example/evictable.jpg?rev=1', variant: 1, bytes: 2048, essential: false, inUse: false, lastUsed: 1, scopes: {} });
      tx.objectStore('blob').put({ key, bytes: new Uint8Array(2048).buffer });
      tx.oncomplete = resolve; tx.onerror = () => reject(tx.error);
    });
    window.__quotaNextWrite = true;
  });
  await page.evaluate(() => new Promise((resolve, reject) => {
    Module._test_quota_write();
    const poll = () => {
      if (Module._test_quota_done()) {
        if (Module._test_quota_result() !== 101) reject(new Error(`quota retry should evict cold row and persist same bytes; result=${Module._test_quota_result()}`));
        else resolve();
      } else setTimeout(poll, 10);
    };
    poll();
  }));
  await page.evaluate(() => new Promise((resolve, reject) => {
    window.__abortNextCacheRead = true;
    Module._test_abort_read();
    const poll = () => {
      if (Module._test_read_abort_done()) {
        if (Module._test_read_abort_result() !== 1) reject(new Error('aborted read must release its allocated WASM buffer and resolve as a miss'));
        else resolve();
      } else setTimeout(poll, 10);
    };
    poll();
  }));
  const final = await page.evaluate(async () => {
    const db = await new Promise((resolve, reject) => {
      const r = indexedDB.open('nuvio-art-v1', 1);
      r.onerror = () => reject(r.error); r.onsuccess = () => resolve(r.result);
    });
    const prefix = 'https://images.example/art.jpg?token=opaque&rev=1\n';
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(['meta', 'blob'], 'readonly');
      const small = tx.objectStore('meta').get(prefix + '1');
      const medium = tx.objectStore('meta').get(prefix + '2');
      tx.oncomplete = () => resolve({ small: !!small.result, medium: !!medium.result });
      tx.onerror = () => reject(tx.error);
    });
  });
  assert.deepStrictEqual(final, { small: true, medium: false }, 'restart persists small variant and decoder-rejected medium invalidation');
  console.log('PASS production cachearte.c WASM bridge: IndexedDB persistence, variants, metadata pins, decoder invalidation, single-count aborts, read-buffer abort, quota eviction and retry');
  await browser.close();
  server.close();
})().catch(error => {
  console.error(error.stack || error);
  server.close();
  process.exitCode = 1;
});
