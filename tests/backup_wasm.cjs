// Real compiled production C/JS, Chromium IndexedDB and page-reload recovery.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawnSync } = require('node:child_process');
const { chromium } = require('playwright');
(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nuvio-backup-wasm-'));
  let server, browser;
  try {
    const emcc = process.env.EMCC || path.join(os.homedir(), 'emsdk/upstream/emscripten/emcc');
    const crypto = process.env.BACKUP_CRYPTO_ROOT;
    assert(crypto, 'set BACKUP_CRYPTO_ROOT to the compiled WASM primitives');
    const sources = ['dados', 'backupcrypto', 'backupdisco', 'backuprestauro', 'backupplano',
      'backuplocal', 'backupstores', 'backupcategories', 'backupconnections', 'backupfonts'];
    const build = spawnSync(emcc, [...sources.map(s => `src/${s}.c`), 'tests/backup_wasm.c',
      '-Isrc', `-I${crypto}/include`, `${crypto}/lib/libnuvio-backup-crypto.a`, '-DNV_BACKUP_MBEDTLS',
      '-O1', '-pthread', '-sASYNCIFY=1', '-sASYNCIFY_STACK_SIZE=131072', '-sSTACK_SIZE=8388608',
      '-lidbfs.js', '-sUSE_ZLIB=1', '-sUSE_SDL=2', '--no-entry', '-sALLOW_MEMORY_GROWTH=1',
      '-sEXPORTED_FUNCTIONS=["_dados_iniciar","_backup_wasm_seed","_backup_wasm_restore","_backup_wasm_recover","_backup_wasm_crypto"]',
      '-sEXPORTED_RUNTIME_METHODS=["ccall","FS"]', '-o', path.join(dir, 'backup.js')], { encoding: 'utf8' });
    if (build.status !== 0) throw new Error(build.stderr || build.error || 'WASM compile failed');
    server = http.createServer((req, res) => {
      res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
      res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
      const file = path.basename(req.url.split('?')[0]);
      if (/^backup(?:\.worker)?\.(js|wasm)$/.test(file)) {
        res.setHeader('Content-Type', file.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
        fs.createReadStream(path.join(dir, file)).pipe(res);
      } else { res.setHeader('Content-Type', 'text/html'); res.end('<!doctype html><title>Backup storage test</title>'); }
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    browser = await chromium.launch({ headless: true });
    const page = await browser.newPage();
    const errors = []; page.on('pageerror', e => errors.push(String(e)));
    const url = `http://127.0.0.1:${server.address().port}`;
    async function boot() {
      await page.goto(url);
      await page.evaluate(() => { window.Module = { onRuntimeInitialized() { window.ready = true; } }; });
      await page.addScriptTag({ url: '/backup.js' });
      await page.waitForFunction(() => window.ready);
      await page.evaluate(() => Module.ccall('dados_iniciar', null, ['string'], ['/unused'], { async: true }));
    }
    async function saved(name) {
      return page.evaluate(name => new Promise((resolve, reject) => {
        const request = indexedDB.open('/nuvio');
        request.onerror = () => reject(request.error);
        request.onsuccess = () => {
          const db = request.result;
          const read = db.transaction('FILE_DATA').objectStore('FILE_DATA').get(`/nuvio/${name}`);
          read.onsuccess = () => { db.close(); resolve(read.result ? new TextDecoder().decode(read.result.contents) : null); };
          read.onerror = () => { db.close(); reject(read.error); };
        };
      }), name);
    }
    await boot();
    assert.equal(await page.evaluate(() => Module.ccall('backup_wasm_crypto', 'number', [], [])), 1);
    assert.equal(await page.evaluate(() => Module.ccall('backup_wasm_seed', 'number', [], [], { async: true })), 1);
    assert.equal(await saved('player.txt'), 'old-player');
    const success = await page.evaluate(async () => {
      const original = Module.FS.syncfs.bind(Module.FS); let count = 0;
      Module.FS.syncfs = (populate, done) => { if (!populate) count++; original(populate, done); };
      const result = await Module.ccall('backup_wasm_restore', 'number', [], [], { async: true });
      Module.FS.syncfs = original; return { result, count };
    });
    assert.equal(success.result, 1); assert.equal(success.count, 4);
    assert.equal(await saved('player.txt'), 'new-player');
    assert.equal(await saved('backup-restore-journal.txt'), null);
    // A failed data-phase flush must not report success; rollback is durable.
    assert.equal(await page.evaluate(() => Module.ccall('backup_wasm_seed', 'number', [], [], { async: true })), 1);
    const failed = await page.evaluate(async () => {
      const original = Module.FS.syncfs.bind(Module.FS); let count = 0;
      Module.FS.syncfs = (populate, done) => {
        if (!populate && ++count === 2) setTimeout(() => done(new Error('fixture storage failure')), 0);
        else original(populate, done);
      };
      const result = await Module.ccall('backup_wasm_restore', 'number', [], [], { async: true });
      Module.FS.syncfs = original; return result;
    });
    assert.equal(failed, -3); assert.equal(await saved('player.txt'), 'old-player');
    assert.equal(await saved('backup-restore-journal.txt'), null);
    // Kill the page after PREPARED is in IndexedDB, before the data flush.
    await page.evaluate(() => {
      const original = Module.FS.syncfs.bind(Module.FS); let count = 0;
      Module.FS.syncfs = (populate, done) => {
        if (!populate && ++count === 2) { window.dataPhaseBlocked = true; return; }
        original(populate, done);
      };
      window.pendingRestore = Module.ccall('backup_wasm_restore', 'number', [], [], { async: true });
    });
    await page.waitForFunction(() => window.dataPhaseBlocked);
    assert.match(await saved('backup-restore-journal.txt'), /JOURNAL 1 P\n/);
    await boot();
    assert.equal(await page.evaluate(() => Module.ccall('backup_wasm_recover', 'number', [], [], { async: true })), 1);
    assert.equal(await saved('player.txt'), 'old-player');
    assert.equal(await saved('backup-restore-journal.txt'), null);
    assert.deepEqual(errors, []);
    console.log('PASS: actual WASM encryption, IndexedDB commit acknowledgements, rejected-flush rollback and recovery after page reload');
  } finally {
    if (browser) await browser.close();
    if (server) await new Promise(resolve => server.close(resolve));
    fs.rmSync(dir, { recursive: true, force: true });
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
