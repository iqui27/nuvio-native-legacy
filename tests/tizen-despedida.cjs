// Issue #120: no Tizen, o cartao "O app fechou sozinho" em todo arranque.
// dados.c de producao em WASM, IDBFS real no Chrome headless, e o "processo
// morre" simulado cortando o IndexedDB da pagina e recarregando: nada do que
// ainda nao tinha chegado ao banco chega depois — o pior caso do
// tizen...exit(). Recarregar sozinho NAO serve: a navegacao dispara pagehide,
// a rede de seguranca de dados.c descarrega e o Chrome de mesa deixa a
// transacao terminar.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawnSync } = require('node:child_process');
const { chromium } = require('playwright');

const MARCA = '/nuvio/sessao-viva.txt';

(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nuvio-despedida-'));
  let server, browser;
  try {
    const emcc = process.env.EMCC || path.join(os.homedir(), 'emsdk/upstream/emscripten/emcc');
    const build = spawnSync(emcc, [path.resolve(__dirname, '../src/dados.c'), '-pthread',
      '-sASYNCIFY=1', '-lidbfs.js', '--no-entry', '-sALLOW_MEMORY_GROWTH=1',
      '-sEXPORTED_FUNCTIONS=["_dados_iniciar","_dados_gravar","_dados_apagar","_dados_sincronizar",' +
        '"_dados_persistente","_dados_despedida_ler","_dados_despedida_fim","_dados_descarregar_e_sair"]',
      '-sEXPORTED_RUNTIME_METHODS=["ccall","FS"]', '-o', path.join(dir, 'dados.js')], { encoding: 'utf8' });
    if (build.status !== 0) throw new Error(build.stderr || build.error || 'emcc failed');
    server = http.createServer((req, res) => {
      res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
      res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
      const f = path.basename(req.url.split('?')[0]);
      if (f === 'dados.js' || f === 'dados.wasm') {
        res.setHeader('Content-Type', f.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
        res.end(fs.readFileSync(path.join(dir, f)));
      } else { res.setHeader('Content-Type', 'text/html'); res.end('<!doctype html><title>despedida</title>'); }
    });
    await new Promise(r => server.listen(0, '127.0.0.1', r));
    const chrome = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch({ headless: true, ...(fs.existsSync(chrome) ? { executablePath: chrome } : {}) });
    const page = await browser.newPage();
    const erros = [];
    page.on('pageerror', e => { if (!String(e).includes('processo morto')) erros.push(String(e)); });
    // O processo morre AGORA: daqui em diante nada desta pagina chega ao
    // IndexedDB nem ao localStorage — nem o que o pagehide e o
    // visibilitychange da navegacao disparariam, que um processo morto nao tem.
    const morrer = () => page.evaluate(() => {
      const morto = () => { throw new Error('processo morto'); };
      IDBDatabase.prototype.transaction = morto;
      IDBFactory.prototype.open = morto;
      Storage.prototype.setItem = morto;
      Storage.prototype.removeItem = morto;
    });
    const url = `http://127.0.0.1:${server.address().port}/`;

    // Um "arranque": carrega o modulo e monta o IDBFS. Devolve a marca lida e
    // a despedida da sessao anterior, como avisos_iniciar as consulta.
    async function arrancar() {
      await page.goto(url);
      await page.evaluate(() => { window.Module = { onRuntimeInitialized() { window.pronto = true; } }; });
      await page.addScriptTag({ url: '/dados.js' });
      await page.waitForFunction(() => window.pronto);
      return page.evaluate(async (marca) => {
        await Module.ccall('dados_iniciar', null, ['string'], ['/unused'], { async: true });
        if (!Module.ccall('dados_persistente', 'number', [], [])) throw new Error('IDBFS nao montou');
        const temMarca = Module.FS.analyzePath(marca).exists;
        const despedida = Module.ccall('dados_despedida_ler', 'number', [], []);
        return { temMarca, despedida };
      }, MARCA);
    }
    // Grava a marca e espera ela chegar ao IndexedDB (a sessao "de pe").
    async function gravarMarcaDuravel() {
      await page.evaluate(() => {
        Module.ccall('dados_gravar', 'number', ['string', 'string'], ['sessao-viva.txt', '1.4.2 2026-09-23 18:00\nultimo=arranque t=0s\n']);
        window.nvPoll = setInterval(() => Module.ccall('dados_sincronizar', null, [], []), 30);
      });
      // (waitForFunction nao espera Promise: um Promise e "verdadeiro".)
      await page.evaluate(async () => {
        const temNoBanco = () => new Promise(r => {
          const q = indexedDB.open('/nuvio');
          q.onerror = () => r(false);
          q.onsuccess = () => {
            const db = q.result;
            if (!db.objectStoreNames.contains('FILE_DATA')) { db.close(); return r(false); }
            const g = db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/nuvio/sessao-viva.txt');
            g.onsuccess = () => { db.close(); r(!!g.result); };
            g.onerror = () => { db.close(); r(false); };
          };
        });
        for (let i = 0; i < 100; i++) {
          if (!Module.nvSyncEmVoo && await temNoBanco()) return;
          await new Promise(r => setTimeout(r, 50));
        }
        throw new Error('a marca nunca chegou ao IndexedDB');
      });
      await page.evaluate(() => clearInterval(window.nvPoll));
    }
    const confere = (nome, obtido, esperado) => {
      if (JSON.stringify(obtido) !== JSON.stringify(esperado))
        throw new Error(`${nome}: esperado ${JSON.stringify(esperado)}, veio ${JSON.stringify(obtido)}`);
      console.log(`ok  ${nome}`);
    };

    await page.goto(url);
    await page.evaluate(() => new Promise(r => { const q = indexedDB.deleteDatabase('/nuvio'); q.onsuccess = q.onerror = () => r(); }));
    await page.evaluate(() => localStorage.clear());

    // 0. O DEFEITO, reproduzido: saida limpa como era ate a 1.4.2 — apagar a
    //    marca e sair na mesma tarefa. A remocao nao chega ao IndexedDB.
    await arrancar();
    await gravarMarcaDuravel();
    await page.evaluate(() => Module.ccall('dados_apagar', 'number', ['string'], ['sessao-viva.txt']));
    await morrer();
    confere('saida antiga: a marca apagada volta no arranque seguinte', (await arrancar()).temMarca, true);

    // 1. Saida limpa corrigida: despedida + descarga antes do exit.
    await gravarMarcaDuravel();
    await page.evaluate(() => new Promise(r => {
      Module.nvSair = r;
      Module.ccall('dados_despedida_fim', null, [], []);
      Module.ccall('dados_apagar', 'number', ['string'], ['sessao-viva.txt']);
      Module.ccall('dados_descarregar_e_sair', null, [], []);
    }));
    await morrer();
    confere('saida pelo app: sem marca e com despedida=fim', await arrancar(), { temMarca: false, despedida: 1 });

    // 2. A despedida vale uma sessao so: esta caiu (recarga sem nada).
    await gravarMarcaDuravel();
    await morrer();
    confere('queda depois de uma saida limpa: marca e sem despedida', await arrancar(), { temMarca: true, despedida: 0 });

    // 3. Pagina escondida e a TV mata o app (Home/Exit): marca fica, mas
    //    a despedida diz "oculto" — nao e o app caindo.
    await gravarMarcaDuravel();
    await page.evaluate(() => {
      Object.defineProperty(document, 'visibilityState', { configurable: true, get: () => 'hidden' });
      document.dispatchEvent(new Event('visibilitychange'));
    });
    await morrer();
    confere('morte em segundo plano: despedida=oculto', (await arrancar()).despedida, 2);

    // 4. Escondeu e VOLTOU a tela: a despedida some; cair depois disso e crash.
    await gravarMarcaDuravel();
    await page.evaluate(() => {
      let v = 'hidden';
      Object.defineProperty(document, 'visibilityState', { configurable: true, get: () => v });
      document.dispatchEvent(new Event('visibilitychange'));
      v = 'visible';
      document.dispatchEvent(new Event('visibilitychange'));
    });
    await morrer();
    confere('escondeu, voltou e caiu: sem despedida', await arrancar(), { temMarca: true, despedida: 0 });

    if (erros.length) throw new Error(erros.join('\n'));
    console.log('PASS despedida da sessao no Tizen (issue #120)');
  } finally {
    if (browser) await browser.close();
    if (server) await new Promise(r => server.close(r));
    fs.rmSync(dir, { recursive: true, force: true });
  }
})().catch(e => { console.error(e); process.exitCode = 1; });
