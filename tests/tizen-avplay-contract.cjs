// Exercises the production EM_JS bridge against the AVPlay fake. This covers
// JS calls and state transitions, not the C parser, codecs or TV compositing.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('src/video_tizen.c', 'utf8');
const body = source.match(/EM_JS\(double, nv_av,[\s\S]*?int dstTam\), \{([\s\S]*?)\n\}\);/);
assert(body, 'production bridge not found');
const outputs = new Map();
const diagnostics = [];
let clock = 0;
const context = vm.createContext({
  console, setTimeout, clearTimeout, document: {},
  performance: { now: () => ++clock },
  UTF8ToString: value => value,
  stringToUTF8: (text, address) => outputs.set(address, text),
  HEAPF64: new Float64Array(16),
});
vm.runInContext('window = globalThis', context);
context.__nvDiag = (message, level) => diagnostics.push({ message, level });
vm.runInContext(fs.readFileSync('tools/fake-avplay.js', 'utf8'), context);
// EM_JS stringification preserves C escapes; emulate the doubled backslashes
// used for JS strings/regexes in this source. The full emcc build checks syntax.
vm.runInContext('bridge = function(cmd, txt, a, b, c, d, dst, dstTam) {' +
  body[1].replace(/\\\\/g, '\\') + '\n}', context);
const call = (op, text = '', a = 0, b = 0, c = 0, d = 0, dst = 0, size = 0) =>
  context.bridge(op, text, a, b, c, d, dst, size);

(async () => {
  let failures = 0;
  function check(name, fn) {
    try { fn(); console.log('PASS:', name); }
    catch (e) { failures++; console.error('FAIL:', name, e.message); }
  }
  assert.equal(call('abrir', 'https://example.invalid/video.mkv'), 1);
  await new Promise(resolve => setTimeout(resolve, 30));
  call('estado', '', 0, 0, 0, 0, 8, 72);
  check('duration uses getDuration in milliseconds', () =>
    assert.equal(context.HEAPF64[2], 7200));
  check('video metadata is read in a valid state after async preparation', () => {
    assert.equal(context.HEAPF64[6], 3840);
    assert.equal(context.HEAPF64[7], 2160);
  });
  check('estado rejects an eight-double buffer before writing completion', () => {
    context.HEAPF64[9] = -12345;
    assert.equal(call('estado', '', 0, 0, 0, 0, 8, 64), 0);
    assert.equal(context.HEAPF64[9], -12345, 'the ninth slot is outside the supplied buffer');
    assert.equal(call('estado', '', 0, 0, 0, 0, 8, 72), 1);
    assert.equal(context.HEAPF64[9], 0);
  });
  check('native completion reaches the existing C state vector', () => {
    assert.equal(context.HEAPF64[9], 0);
    context.__avOuvinte().onstreamcompleted();
    call('estado', '', 0, 0, 0, 0, 8, 72);
    assert.equal(context.HEAPF64[9], 1);
    assert.equal(context.HEAPF64[3], 0);
    assert.match(source, /terminou\s*=\s*est\[EST_FIM\]/);
    assert.match(source, /video_terminou\(void\)\s*\{\s*return terminou;/);
    call('buscar', '', 60000);
    call('estado', '', 0, 0, 0, 0, 8, 72);
    assert.equal(context.HEAPF64[9], 0, 'a seek clears completion from the old position');
  });
  call('faixas', '', 0, 0, 0, 0, 100, 4096);
  check('bridge returns all audio and text rows', () => {
    assert.match(outputs.get(100), /A\t1\teng/);
    assert.match(outputs.get(100), /A\t2\tpor/);
    assert.match(outputs.get(100), /T\t3\tpor/);
  });
  check('track metadata is reused by the faixas operation', () => {
    const calls = context.__avReg().filter(entry => entry.startsWith('getTotalTrackInfo'));
    assert.equal(calls.length, 1);
  });
  check('repeated subtitle-menu reads reuse native metadata', () => {
    for (let i = 0; i < 1000; i++) call('faixas', '', 0, 0, 0, 0, 100, 4096);
    assert.equal(context.__avReg().filter(entry => entry.startsWith('getTotalTrackInfo')).length, 1);
  });
  check('AVPlay timing diagnostics are bounded and contain no URL', () => {
    const timing = diagnostics.filter(entry => / \d+ ms$/.test(entry.message));
    const metadata = diagnostics.filter(entry => /getTotalTrackInfo bruto/.test(entry.message));
    const names = timing.map(entry => entry.message.replace(/^\[video\] /, '').replace(/ \d+ ms$/, ''));
    assert.deepEqual(names, [
      'prepareAsync.retorno',
      'prepareAsync.ate-callback',
      'prepare.setDisplayRect',
      'prepare.play',
      'prepare.getTotalTrackInfo',
    ]);
    assert.equal(metadata.length, 1);
    assert.match(metadata[0].message, /^\[video\] getTotalTrackInfo bruto n=\d+ tipos=[A-Z?,]+$/);
    for (const entry of diagnostics) {
      assert.doesNotMatch(entry.message, /example\.invalid|https?:/);
      assert.equal(entry.level, 0);
    }
  });
  check('source logs correlate a source without printing its URL', () => {
    assert.doesNotMatch(source, /\[video\] URL: %s/);
    assert.doesNotMatch(source, /%.80s\\n", url/);
    assert.match(source, /fonte id=%08x/);
  });
  check('pausa confirmada requires real AVPlay PAUSED state', () => {
    assert.equal(call('pausa_confirmada'), 0);
    assert.equal(call('pausar', '', 1), 1);
    assert.equal(call('pausa_confirmada'), 1);
    const play = context.webapis.avplay;
    const getState = play.getState;
    play.getState = () => 'PLAYING';
    assert.equal(call('pausa_confirmada'), 0);
    play.getState = () => { throw new Error('resource gone'); };
    assert.equal(call('pausa_confirmada'), 0);
    play.getState = getState;
    assert.equal(call('pausar', '', 0), 1);
    assert.equal(call('pausa_confirmada'), 0);
  });
  check('app pumps video before any screen can return', () => {
    const app = fs.readFileSync('src/app.c', 'utf8');
    const start = app.indexOf('void app_atualizar(float dt, Uint32 agora)');
    const end = app.indexOf('void app_desenhar(Uint32 agora)', start);
    assert.ok(start >= 0 && end > start, 'app_atualizar range not found');
    const update = app.slice(start, end);
    const pump = update.indexOf('video_bombear();');
    const firstScreenHandler = update.indexOf('if (tela == TELA_LOGIN)');
    assert.ok(pump >= 0 && firstScreenHandler > pump,
      'video_bombear must run before conditional screen handlers');
    assert.equal((update.match(/video_bombear\(\);/g) || []).length, 1);
  });

  // #122: embedded text arrives only through onsubtitlechange; AVPlay does not
  // draw it (Samsung's PlayerAVPlaySubtitle sample sets innerHTML there). The
  // bridge must keep the text for the C overlay and expire/clear it.
  const legTexto = () => { outputs.delete(300); const n = call('leg_texto', '', 0, 0, 0, 0, 300, 512); return { n, t: outputs.get(300) }; };
  call('faixa', 'TEXT', 3);
  call('leg_mudo', '', 0);
  check('choosing embedded text unmutes and selects the absolute TEXT index', () => {
    const reg = context.__avReg();
    assert.ok(reg.includes('setSelectTrack("TEXT", 3)'));
    assert.ok(reg.includes('setSilentSubtitle(false)'));
  });
  context.__nvav.posMs = 10000;
  context.__avOuvinte().onsubtitlechange('2000', 'Olá <i>mundo</i>', '0', []);
  check('onsubtitlechange text is exposed to C while it is valid', () => {
    const r = legTexto();
    assert.equal(r.t, 'Olá <i>mundo</i>');
    assert.ok(r.n >= 2);
  });
  context.__nvav.posMs = 12500;
  check('embedded text expires after its duration', () => assert.equal(legTexto().t, ''));
  context.__avOuvinte().onsubtitlechange('0', 'Sem prazo', '0', []);
  call('buscar', '', 60000);
  check('seek clears the embedded text of the old position', () => assert.equal(legTexto().t, ''));
  context.__avOuvinte().onsubtitlechange('1000', 'Outra', '0', []);
  call('leg_mudo', '', 1);
  check('turning subtitles off clears the embedded text', () => assert.equal(legTexto().t, ''));
  const velho = context.__avOuvinte();
  call('parar');
  call('abrir', 'https://example.invalid/proximo.mkv');
  await new Promise(resolve => setTimeout(resolve, 30));
  velho.onsubtitlechange('5000', 'do titulo anterior', '0', []);
  check('stale onsubtitlechange from a previous open is ignored', () => assert.equal(legTexto().t, ''));

  // #147: o "estado" roda a cada quadro e nao pode chamar o servidor de midia
  // a cada vez. Com o evento de tempo chegando, nem getCurrentTime nem
  // getDuration; logo depois de um seek, getCurrentTime volta.
  {
    const av = context.webapis.avplay;
    const gct = av.getCurrentTime, gd = av.getDuration;
    let nTempo = 0, nDur = 0;
    call('estado', '', 0, 0, 0, 0, 8, 72);          // primeira leitura: guarda a duracao
    av.getCurrentTime = function () { nTempo++; return gct.call(this); };
    av.getDuration = function () { nDur++; return gd.call(this); };
    context.__nvav.tEvento = Date.now();            // oncurrentplaytime acabou de chegar
    context.__nvav.tSeek = 0;
    for (let i = 0; i < 20; i++) call('estado', '', 0, 0, 0, 0, 8, 72);
    check('estado does not call AVPlay getters every frame (#147)', () => {
      assert.equal(nTempo, 0);
      assert.equal(nDur, 0);
      assert.equal(context.HEAPF64[2], 7200);       // a duracao guardada continua valendo
    });
    call('buscar', '', 60000);
    call('estado', '', 0, 0, 0, 0, 8, 72);
    check('estado reads getCurrentTime right after a seek (#147)', () => assert.equal(nTempo, 1));
    // Entre dois eventos a posicao anda sozinha (tocando) e para (pausado):
    // sem isso a legenda externa andaria aos saltos.
    const S = context.__nvav;
    S.tSeek = 0; S.tEvento = Date.now(); S.posMs = 10000; S.tPos = Date.now() - 300; S.tocando = 1;
    call('estado', '', 0, 0, 0, 0, 8, 72);
    check('position advances between time events while playing (#147)', () => {
      assert.ok(context.HEAPF64[1] >= 10.25 && context.HEAPF64[1] <= 10.6, String(context.HEAPF64[1]));
    });
    S.tocando = 0; S.tEvento = Date.now();
    call('estado', '', 0, 0, 0, 0, 8, 72);
    check('position holds between events while paused (#147)', () => assert.equal(context.HEAPF64[1], 10));
    // Pausado, o evento para: a leitura de reserva nao pode virar uma por quadro.
    nTempo = 0; S.tEvento = Date.now() - 5000; S.tPos = Date.now();
    for (let i = 0; i < 20; i++) call('estado', '', 0, 0, 0, 0, 8, 72);
    check('paused fallback read is throttled (#147)', () => assert.ok(nTempo <= 1, String(nTempo)));
    av.getCurrentTime = gct; av.getDuration = gd;
  }

  // A failed prepare metadata read must be recoverable through `faixas`, and
  // that fallback must be cached just like the normal prepare path.
  const player = context.webapis.avplay;
  const originalTracks = player.getTotalTrackInfo;
  let failPrepareTracks = true;
  player.getTotalTrackInfo = function () {
    if (failPrepareTracks) {
      failPrepareTracks = false;
      throw new Error('metadata temporarily unavailable');
    }
    return originalTracks.apply(this, arguments);
  };
  call('parar');
  call('abrir', 'https://example.invalid/fallback.mkv');
  await new Promise(resolve => setTimeout(resolve, 30));
  const beforeFallback = context.__avReg().filter(entry => entry.startsWith('getTotalTrackInfo')).length;
  call('faixas', '', 0, 0, 0, 0, 200, 4096);
  call('faixas', '', 0, 0, 0, 0, 201, 4096);
  const afterFallback = context.__avReg().filter(entry => entry.startsWith('getTotalTrackInfo')).length;
  check('faixas fallback caches the first successful metadata read', () => {
    assert.equal(afterFallback - beforeFallback, 1);
    assert.match(outputs.get(200), /T\t3\tpor/);
    assert.equal(outputs.get(200), outputs.get(201));
  });
  check('fallback timing diagnostic is emitted without the source URL', () => {
    const fallback = diagnostics.filter(entry => entry.message.startsWith('[video] faixas.getTotalTrackInfo '));
    assert.equal(fallback.length, 1);
    assert.doesNotMatch(fallback[0].message, /example\.invalid|https?:/);
  });

  // Keep both the prepare callback and the listener from an old open. They
  // must be ignored after the second open has advanced the session generation.
  const originalPrepareAsync = player.prepareAsync;
  const originalSetListener = player.setListener;
  const pending = [];
  const listeners = [];
  player.prepareAsync = function (ok) { pending.push(ok); };
  player.setListener = function (listener) {
    listeners.push(listener);
    return originalSetListener.call(this, listener);
  };
  call('parar');
  call('abrir', 'https://example.invalid/stale-one.mkv');
  call('abrir', 'https://example.invalid/stale-two.mkv');
  check('two opens leave two pending prepare callbacks for the generation test', () => {
    assert.equal(pending.length, 2);
    assert.equal(listeners.length, 2);
  });
  listeners[0].oncurrentplaytime(7777);
  listeners[0].onerror('stale callback');
  listeners[0].onstreamcompleted();
  check('stale listener events do not contaminate the current session', () => {
    assert.equal(context.__nvav.posMs, 0);
    assert.equal(context.__nvav.erro, '');
    assert.equal(context.__nvav.fim, 0);
  });
  const beforeStaleCallback = context.__avReg().length;
  pending[0]();
  check('stale prepare callback is discarded', () => {
    assert.equal(context.__avReg().length, beforeStaleCallback);
    assert.equal(context.__nvav.pronto, 0);
  });
  pending[1]();
  check('current prepare callback still starts playback', () => {
    assert.equal(context.__nvav.pronto, 1);
    assert.equal(context.__nvav.tocando, 1);
  });
  check('no error text while the session is healthy', () => {
    outputs.delete(400);
    assert.equal(call('erro', '', 0, 0, 0, 0, 400, 200), 0);
    assert.equal(outputs.has(400), false);
  });
  listeners[1].onerror('PLAYER_ERROR_CONNECTION_FAILED');
  check('onerror reason reaches C through the erro operation', () => {
    outputs.delete(400);
    assert.equal(call('erro', '', 0, 0, 0, 0, 400, 200), 1);
    assert.equal(outputs.get(400), 'PLAYER_ERROR_CONNECTION_FAILED');
  });
  player.prepareAsync = originalPrepareAsync;
  player.setListener = originalSetListener;

  const originalPlay = player.play;
  player.play = function () { throw new Error('InvalidStateError: play'); };
  call('parar');
  call('abrir', 'https://example.invalid/play-throws.mkv');
  await new Promise(resolve => setTimeout(resolve, 30));
  call('estado', '', 0, 0, 0, 0, 8, 72);
  check('play failure never publishes a ready or open session', () => {
    assert.equal(context.HEAPF64[4], 0);
    assert.equal(context.HEAPF64[5], 0);
    assert.equal(context.HEAPF64[8], 1);
  });
  player.play = originalPlay;
  call('parar');
  process.exitCode = failures ? 1 : 0;
})();
