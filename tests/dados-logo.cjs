// dados_sincronizar_logo: a sessao gravada vai ao IndexedDB no primeiro
// dados_sincronizar, sem esperar os 700 ms desde a descarga anterior.
//
// O caso que importa: logo depois de uma descarga (a varredura de 700 ms ainda
// nao venceu) o login grava sessao.txt. Sem o pedido urgente ela so sai na
// janela seguinte; se a pagina morre antes, a proxima abertura pede QR.
// Compila o dados.c de producao com IDBFS de verdade e mede no Chrome.
//
//   node tests/dados-logo.cjs
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawnSync } = require('node:child_process');
const { chromium } = require('playwright');

(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nuvio-dados-logo-'));
  let server, browser;
  try {
    const emcc = process.env.EMCC || path.join(os.homedir(), 'emsdk/upstream/emscripten/emcc');
    const build = spawnSync(emcc, [path.resolve(__dirname, '../src/dados.c'), '-pthread',
      '-sASYNCIFY=1', '-lidbfs.js', '--no-entry', '-sALLOW_MEMORY_GROWTH=1',
      '-sEXPORTED_FUNCTIONS=["_dados_iniciar","_dados_gravar","_dados_sincronizar","_dados_sincronizar_logo","_dados_persistente"]',
      '-sEXPORTED_RUNTIME_METHODS=["ccall","FS"]', '-o', path.join(dir, 'dados.js')], { encoding: 'utf8' });
    if (build.status !== 0) throw new Error(build.stderr || build.error || 'emcc failed');
    server = http.createServer((req, res) => {
      res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
      res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
      const filename = path.basename(req.url.split('?')[0]);
      if (filename === 'dados.js' || filename === 'dados.wasm') {
        res.setHeader('Content-Type', filename.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
        res.end(fs.readFileSync(path.join(dir, filename)));
      } else { res.setHeader('Content-Type', 'text/html'); res.end('<!doctype html><title>dados logo</title>'); }
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const chrome = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch({ headless: true, ...(fs.existsSync(chrome) ? { executablePath: chrome } : {}) });
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(String(error)));
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.evaluate(() => { window.Module = { onRuntimeInitialized() { window.ready = true; } }; });
    await page.addScriptTag({ url: '/dados.js' });
    await page.waitForFunction(() => window.ready);
    const r = await page.evaluate(async () => {
      await Module.ccall('dados_iniciar', null, ['string'], ['/unused'], { async: true });
      if (!Module.ccall('dados_persistente', 'number', [], [])) throw new Error('IDBFS nao montou');
      const noIdb = (nome) => new Promise((resolve, reject) => {
        const q = indexedDB.open('/nuvio');
        q.onerror = () => reject(q.error);
        q.onsuccess = () => {
          const db = q.result, g = db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/nuvio/' + nome);
          g.onsuccess = () => { db.close(); resolve(g.result ? new TextDecoder().decode(g.result.contents) : null); };
          g.onerror = () => { db.close(); reject(g.error); };
        };
      });
      const esperarVoo = async () => { while (Module.nvSyncEmVoo) await new Promise(ok => setTimeout(ok, 5)); };
      const sinc = () => Module.ccall('dados_sincronizar', null, [], []);
      // 1. Uma descarga qualquer: zera o relogio dos 700 ms.
      Module.ccall('dados_gravar', 'number', ['string', 'string'], ['antes.txt', 'x']);
      sinc();
      await esperarVoo();
      // 2. CONTROLE: gravacao comum logo depois — o quadro seguinte NAO descarrega.
      Module.ccall('dados_gravar', 'number', ['string', 'string'], ['comum.txt', 'y']);
      sinc();
      const comumSaiu = !!Module.nvSyncEmVoo;
      await esperarVoo();
      const comumNoIdb = await noIdb('comum.txt');
      // 3. A sessao com o pedido urgente, ainda dentro dos mesmos 700 ms.
      const t0 = performance.now();
      Module.ccall('dados_gravar', 'number', ['string', 'string'], ['sessao.txt', 'tokens']);
      Module.ccall('dados_sincronizar_logo', null, [], []);
      sinc();
      const logoSaiu = !!Module.nvSyncEmVoo;
      await esperarVoo();
      const ms = performance.now() - t0;
      return { comumSaiu, comumNoIdb, logoSaiu, sessaoNoIdb: await noIdb('sessao.txt'), ms };
    });
    if (errors.length) throw new Error(errors.join('\n'));
    console.log(JSON.stringify(r));
    if (r.comumSaiu || r.comumNoIdb !== null) throw new Error('controle: a gravacao comum nao deveria sair antes dos 700 ms');
    if (!r.logoSaiu) throw new Error('dados_sincronizar_logo: a descarga nao saiu no primeiro dados_sincronizar');
    if (r.sessaoNoIdb !== 'tokens') throw new Error('sessao.txt nao chegou ao IndexedDB');
    console.log(`PASS sessao no IndexedDB ${r.ms.toFixed(0)} ms depois de gravar, sem esperar os 700 ms (a gravacao comum esperou)`);
  } finally {
    if (browser) await browser.close();
    if (server) await new Promise(resolve => server.close(resolve));
    fs.rmSync(dir, { recursive: true, force: true });
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
