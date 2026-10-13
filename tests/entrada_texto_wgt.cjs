// Ponte do IME no .wgt: o <input> escondido e o window.nvTexto de
// tools/tizen-shell.html (copiados do arquivo de verdade), com
// src/entrada_texto.c em WASM. No Chrome nao ha IME da Samsung: o teste faz o
// papel dele (insertText, composicao, Done 65376, Return 10009, blur).
// O <input> serve a digitacao no teclado da TV e ao botao de microfone
// (TS_TECLADO | TS_VOZ). Sem emsdk, tests/wgt_voz.sh cobre o mesmo com um
// emscripten.h fingido.
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const http = require('node:http');
const { spawnSync } = require('node:child_process');
let pw; try { pw = require('playwright'); } catch (e) { pw = require('playwright-core'); }
const { chromium } = pw;

const raiz = path.resolve(__dirname, '..');
const shell = fs.readFileSync(path.join(raiz, 'tools/tizen-shell.html'), 'utf8');
const campo = shell.match(/<input id="nvTextoCampo"[\s\S]*?>/)[0];
const ini = shell.indexOf('  // TECLADO DA SAMSUNG PARA DIGITAR E PARA O BOTAO DE MICROFONE DAS BUSCAS');
const fim = shell.indexOf('})();', ini) + 5;
if (ini < 0 || fim < 5) throw new Error('bloco nvTexto nao achado no tizen-shell.html');
const ponte = shell.slice(ini, fim);

function ok(c, m) { if (!c) throw new Error('FALHOU: ' + m); console.log('ok - ' + m); }

(async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nuvio-texto-wgt-'));
  let server, browser;
  try {
    const emcc = process.env.EMCC || path.join(os.homedir(), 'emsdk/upstream/emscripten/emcc');
    const b = spawnSync(emcc, [path.join(raiz, 'tests/entrada_texto_wgt.c'), path.join(raiz, 'src/entrada_texto.c'),
      '-sUSE_SDL=2', '-sEXPORTED_RUNTIME_METHODS=["ccall","UTF8ToString"]', '-o', path.join(dir, 't.js')], { encoding: 'utf8' });
    if (b.status !== 0) throw new Error(b.stderr || String(b.error));
    const html = `<!doctype html><meta charset="utf-8"><canvas id="canvas" tabindex="0"></canvas>${campo}
<script>
// Chrome nao tem Tizen: emula a Samsung explicitamente antes de carregar a ponte.
window.tizen = { systeminfo: { getCapability(k) {
  if (k !== 'http://tizen.org/feature/microphone') throw new Error('capacidade inesperada');
  return true;
} } };
${ponte}
window.chegouNoWindow = []; window.addEventListener('keydown', e => chegouNoWindow.push(e.keyCode));
window.capturaDepois = []; window.addEventListener('keydown', e => capturaDepois.push(e.keyCode), true);
window.Module = { onRuntimeInitialized() { window.pronto = true; } };</script><script src="t.js"></script>`;
    server = http.createServer((req, res) => {
      const f = path.basename(req.url.split('?')[0]);
      if (f === 't.js' || f === 't.wasm') {
        res.setHeader('Content-Type', f.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
        res.end(fs.readFileSync(path.join(dir, f)));
      } else { res.setHeader('Content-Type', 'text/html; charset=utf-8'); res.end(html); }
    });
    await new Promise(r => server.listen(0, '127.0.0.1', r));
    const chrome = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    browser = await chromium.launch({ headless: true, ...(fs.existsSync(chrome) ? { executablePath: chrome } : {}) });
    const page = await browser.newPage();
    const erros = [];
    page.on('pageerror', e => erros.push(String(e)));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.pronto);
    const C = (fn, ret, tipos, args) => page.evaluate(([fn, ret, tipos, args]) =>
      Module.ccall(fn, ret, tipos, args), [fn, ret, tipos || [], args || []]);
    const quadro = () => C('t_quadro', null);

    await C('t_iniciar', 'number');
    ok(await C('t_disponivel', 'number') === 3, 'disponivel = TS_TECLADO | TS_VOZ com window.nvTexto');
    await C('t_mic_modo', null, ['number'], [2]);
    ok(await C('t_disponivel', 'number') === 1, 'ajuste Desligado: teclado da TV continua disponivel');
    await C('t_mic_modo', null, ['number'], [0]);
    await C('t_abrir', null, ['string', 'number'], ['ab', 0]);
    ok(await C('t_aberto', 'number') === 1, 'OK no campo abre o teclado da TV para digitar');
    ok(await page.evaluate(() => document.activeElement.id) === 'nvTextoCampo', 'abrir foca o input escondido');
    ok(await page.evaluate(() => document.getElementById('nvTextoCampo').value) === 'ab', 'o IME comeca do texto atual');
    await page.keyboard.insertText('c');
    await quadro();
    ok(await C('t_valor', 'string') === 'abc', 'texto do IME chega como valor inteiro');

    await page.evaluate(() => {
      const el = document.getElementById('nvTextoCampo');
      el.value = 'ação é';
      el.dispatchEvent(new CompositionEvent('compositionend', { data: 'é' }));
    });
    await quadro();
    ok(await C('t_valor', 'string') === 'ação é', 'composicao (UTF-8) chega certa');

    await page.evaluate(() => { window.chegouNoWindow.length = 0; });
    await page.keyboard.press('KeyX');
    await quadro();
    ok((await page.evaluate(() => window.chegouNoWindow)).length === 0, 'tecla no campo nao sobe ao window (o SDL nao a ve)');
    ok(await C('t_valor', 'string') === 'ação éx', 'tecla fisica no campo entra uma vez so, pelo valor');

    await page.evaluate(() => document.getElementById('nvTextoCampo').dispatchEvent(
      new KeyboardEvent('keydown', { keyCode: 65376, bubbles: true })));
    await quadro();
    ok(await C('t_n_fim', 'number') === 1 && await C('t_fim_confirmou', 'number') === 1, 'Done (65376) fecha confirmando');
    ok(await C('t_aberto', 'number') === 0, 'modulo marca fechado');
    ok(await page.evaluate(() => document.activeElement.id) === 'canvas', 'foco volta ao canvas');

    await C('t_abrir', null, ['string', 'number'], ['x', 1]);
    await page.evaluate(() => { window.capturaDepois.length = 0; document.getElementById('nvTextoCampo').dispatchEvent(
      new KeyboardEvent('keydown', { keyCode: 10009, bubbles: true })); });
    await quadro();
    ok(await C('t_n_fim', 'number') === 2 && await C('t_fim_confirmou', 'number') === 0, 'Return (10009) fecha sem confirmar');
    ok((await page.evaluate(() => window.capturaDepois)).length === 0, 'Return com o IME aberto nao chega aos outros ouvintes (nao fecha o Spotlight)');

    await C('t_abrir', null, ['string', 'number'], ['', 1]);
    await page.evaluate(() => document.getElementById('nvTextoCampo').blur());
    await quadro();
    ok(await C('t_n_fim', 'number') === 3 && await C('t_fim_confirmou', 'number') === 0, 'IME que some (blur) sem nada dito: fim sem confirmar');

    await C('t_abrir', null, ['string', 'number'], ['', 1]);
    await page.keyboard.insertText('duna');
    await page.evaluate(() => document.getElementById('nvTextoCampo').blur());
    await quadro();
    ok(await C('t_n_fim', 'number') === 4 && await C('t_fim_confirmou', 'number') === 1, 'IME que some depois da fala: o que foi dito vale');

    await C('t_abrir', null, ['string', 'number'], ['', 1]);
    await C('t_fechar', null);
    await quadro();
    ok(await page.evaluate(() => document.activeElement.id) !== 'nvTextoCampo', 'fechar pelo app tira o foco do campo');
    ok(await C('t_n_fim', 'number') === 5 && await C('t_aberto', 'number') === 0, 'fechar pelo app avisa o fim uma vez');
    await quadro();
    ok(await C('t_n_fim', 'number') === 5, 'sem fim duplicado depois do blur');

    await page.evaluate(() => { document.getElementById('nvTextoCampo').focus = () => {}; });
    await C('t_abrir', null, ['string', 'number'], ['', 1]);
    await quadro();
    ok(await C('t_n_fim', 'number') === 6 && await C('t_fim_confirmou', 'number') === 0 &&
       await C('t_aberto', 'number') === 0, 'focus recusado cancela a sessao C');
    await page.evaluate(() => document.getElementById('nvTextoCampo').remove());
    await C('t_abrir', null, ['string', 'number'], ['', 1]);
    await quadro();
    ok(await C('t_n_fim', 'number') === 7 && await C('t_fim_confirmou', 'number') === 0 &&
       await C('t_aberto', 'number') === 0, 'campo ausente cancela a sessao C');
    ok(erros.length === 0, 'sem erro de pagina ' + erros.join(' | '));
  } finally {
    if (browser) await browser.close();
    if (server) server.close();
    fs.rmSync(dir, { recursive: true, force: true });
  }
})().catch(e => { console.error(e.message || e); process.exit(1); });
