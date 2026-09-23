// Contrato focalizado do trailer do hero.
//
// A parte importante deste teste executa os corpos EM_JS reais de
// src/trailer.c em um DOM minimo. Assim, os eventos sao disparados nos
// elementos que o app realmente cria; nao ha uma segunda maquina JS que possa
// divergir do callback compilado para a Samsung.
const assert = require('node:assert/strict');
const fs = require('node:fs');

const home = fs.readFileSync('src/home.c', 'utf8');
const layout = fs.readFileSync('src/layout.h', 'utf8');
const extras = fs.readFileSync('src/extras.c', 'utf8');
const trailer = fs.readFileSync('src/trailer.c', 'utf8');
const detail = fs.readFileSync('src/detail.c', 'utf8');
const apple = fs.readFileSync('src/trailerapple.c', 'utf8');

function check(name, condition) {
  assert.ok(condition, name);
  console.log(`ok ${name}`);
}

const number = (name) => {
  const m = layout.match(new RegExp(`#define ${name} ([0-9]+)`));
  assert.ok(m, `constante ${name}`);
  return Number(m[1]);
};

const appleWait = number('NV_TRAILER_HERO_ESPERA_MS');
const maxWait = number('NV_TRAILER_HERO_MAX_ESPERA_MS');
const prepare = number('NV_TRAILER_HERO_PREPARA_MS');
check('janela Apple curta e finita', appleWait > 0 && appleWait <= 1500 && maxWait > appleWait);
check('preparacao tem prazo finito', prepare > 0 && prepare <= 5000);
check('teto total explicito para duas fontes',
  layout.includes('NV_TRAILER_HERO_MAX_TOTAL_ESPERA_MS') &&
  layout.includes('NV_TRAILER_HERO_MAX_ESPERA_MS + 2 * NV_TRAILER_HERO_PREPARA_MS'));

// Extract the JavaScript between the real EM_JS markers. The delimiters are
// intentionally explicit so this test fails if the bridge is moved or the
// implementation stops being the one that is exercised below.
function emjsBody(startMarker, endMarker) {
  const start = trailer.indexOf(startMarker);
  assert.ok(start >= 0, `EM_JS ausente: ${startMarker}`);
  const open = trailer.indexOf('{', start);
  const end = trailer.indexOf(endMarker, open);
  assert.ok(open >= 0 && end > open, `corpo EM_JS ausente: ${startMarker}`);
  return trailer.slice(open + 1, end);
}

const openBody = emjsBody(
  'EM_JS(int, trailer_js_abrir',
  '\n});\nEM_JS(void, trailer_js_cmd');
const closeBody = emjsBody(
  'EM_JS(void, trailer_js_fechar',
  '\n});\nEM_JS(int, trailer_js_estado');
const stateBody = emjsBody(
  'EM_JS(int, trailer_js_estado',
  '\n});\nEM_JS(int, trailer_js_pausado');

function fakeElement(tag) {
  const e = {
    tagName: tag,
    listeners: Object.create(null),
    parentNode: null,
    children: [],
    style: {},
    attributes: {},
    paused: true,
    contentWindow: {
      messages: [],
      postMessage(message, origin) { this.messages.push({ message, origin }); },
    },
    addEventListener(name, fn) {
      (this.listeners[name] || (this.listeners[name] = [])).push(fn);
    },
    dispatch(name, event = {}) {
      for (const fn of this.listeners[name] || []) fn(event);
    },
    setAttribute(name, value) { this.attributes[name] = value; },
    removeAttribute(name) { delete this.attributes[name]; },
    appendChild(child) { child.parentNode = this; this.children.push(child); },
    removeChild(child) {
      const i = this.children.indexOf(child);
      if (i >= 0) this.children.splice(i, 1);
      child.parentNode = null;
    },
    play() {
      this.paused = false;
      return { catch() {} };
    },
    pause() { this.paused = true; },
    load() {},
  };
  return e;
}

const body = fakeElement('body');
const canvas = { getBoundingClientRect: () => ({ left: 10, top: 20, width: 1920, height: 1080 }) };
const messageListeners = [];
const diagLines = [];
const timers = [];
const fakeSetTimeout = (fn, ms) => { timers.push({ fn, ms }); return timers.length; };
const window = {
  __nvDiag(t) { diagLines.push(t); },
  addEventListener(name, fn) { if (name === 'message') messageListeners.push(fn); },
  dispatchMessage(source, message, origin = 'https://www.youtube.com') {
    for (const fn of messageListeners) fn({ source, data: message, origin });
  },
};
const document = {
  body,
  documentElement: body,
  getElementById(id) { return id === 'canvas' ? canvas : null; },
  createElement(tag) { return fakeElement(tag); },
};
const location = { origin: 'null' };
const Module = {};
const UTF8ToString = (value) => String(value);
const openJs = new Function(
  'Module', 'document', 'location', 'window', 'UTF8ToString', 'setTimeout', 'console',
  'fonte', 'x', 'y', 'w', 'h', 'som', 'zoom', openBody);
const quietConsole = { log() {} };
const closeJs = new Function('Module', 'document', 'location', 'window', 'UTF8ToString', closeBody);
const stateJs = new Function('Module', 'document', 'location', 'window', 'UTF8ToString', stateBody);
const abrir = (...args) => openJs(Module, document, location, window, UTF8ToString, fakeSetTimeout, quietConsole, ...args);
const fechar = () => closeJs(Module, document, location, window, UTF8ToString);
const estado = () => stateJs(Module, document, location, window, UTF8ToString);
const ultimo = () => body.children[body.children.length - 1];

function fonteVideo(url) {
  return [url, 0, 0, 1920, 1080, 0, 1];
}
function fonteYoutube(id) {
  return [id, 0, 0, 1920, 1080, 0, 1];
}
function estadoYoutube(f, value) {
  window.dispatchMessage(f.contentWindow, JSON.stringify({ event: 'onStateChange', info: value }));
}

// A -> B -> A, including reopening the same URL after close. Events emitted by
// the old video must not touch the current source.
abrir(...fonteVideo('https://media.test/a.mp4'));
const videoA1 = ultimo();
const gerA1 = Module.nvTrailer.geracao;
abrir(...fonteVideo('https://media.test/b.mp4'));
const videoB = ultimo();
check('video troca de fonte cria nova geracao', Module.nvTrailer.geracao === gerA1 + 1);
videoA1.dispatch('playing');
videoA1.dispatch('error');
check('eventos do video antigo nao alteram a fonte atual', estado() === -1);
videoB.dispatch('playing');
videoB.dispatch('waiting');
check('video atual publica playing e buffering', estado() === 3);
abrir(...fonteVideo('https://media.test/a.mp4'));
const videoA2 = ultimo();
videoA1.dispatch('error');
check('A antigo nao contamina A reaberto', estado() === -1 && videoA2 !== videoA1);
videoA2.dispatch('playing');
check('A reaberto toca normalmente', estado() === 1);
fechar();
videoA2.dispatch('error');
check('fechar invalida eventos pendentes', estado() === -2);
abrir(...fonteVideo('https://media.test/a.mp4'));
const videoA3 = ultimo();
check('mesma URL pode reabrir apos fechar', videoA3 !== videoA2 && estado() === -1);

// The iframe listener is shared for the session, so it must use the current
// source as well as the current generation before accepting postMessage.
abrir(...fonteYoutube('dQw4w9WgXcQ'));
const frame1 = ultimo();
estadoYoutube(frame1, 1);
check('iframe atual aceita estado do YouTube', estado() === 1);
abrir(...fonteYoutube('ZyX987abc12'));
const frame2 = ultimo();
estadoYoutube(frame1, 0);
check('postMessage do iframe antigo e ignorado', estado() === -1);
estadoYoutube(frame2, 1);
check('postMessage do iframe atual e aceito', estado() === 1);
const mesmaFrame = ultimo();
abrir(...fonteYoutube('ZyX987abc12'));
check('mesma URL aberta enquanto toca reutiliza o elemento atual', ultimo() === mesmaFrame);
fechar();
abrir(...fonteYoutube('ZyX987abc12'));
const frame3 = ultimo();
estadoYoutube(frame2, 0);
check('iframe antigo nao contamina reabertura da mesma URL', estado() === -1 && frame3 !== frame2);
estadoYoutube(frame3, 1);
check('iframe reaberto aceita playback', estado() === 1);

// DIAGNOSTICO do <video> (log 1646: HLS pedido e nenhuma linha depois). Uma
// linha por transicao, pelo canal do app (window.__nvDiag), com medida.
fechar();
diagLines.length = 0;
timers.length = 0;
abrir(...fonteVideo('https://vod.test/diag.m3u8'));
const vd = ultimo();
const ger = Module.nvTrailer.geracao;
vd.videoWidth = 1186; vd.videoHeight = 496; vd.duration = 116.1;
vd.dispatch('loadedmetadata');
vd.dispatch('waiting'); vd.dispatch('waiting'); vd.dispatch('stalled'); vd.dispatch('stalled');
check('loadedmetadata loga WxH e duracao', diagLines.some((l) => l.startsWith(`[trailer-js] g${ger} loadedmetadata +`) && l.includes('1186x496 dur=116.1')));
check('waiting e stalled so a primeira de cada', diagLines.filter((l) => / waiting /.test(l)).length === 1 && diagLines.filter((l) => / stalled /.test(l)).length === 1);
const prazo = timers.find((t) => t.ms === 8000);
check('prazo de 8 s armado no elemento', !!prazo);
prazo.fn();
check('sem playing em 8 s vira linha com readyState', diagLines.some((l) => l.includes('sem playing em 8 s') && l.includes('readyState=')));
vd.error = { code: 4, message: 'MEDIA_ERR_SRC_NOT_SUPPORTED' };
vd.dispatch('error');
check('error loga MediaError.code e message', diagLines.some((l) => l.includes(' error ') && l.includes('code=4 MEDIA_ERR_SRC_NOT_SUPPORTED')) && estado() === -3);
const doPlaying = diagLines.length;
abrir(...fonteVideo('https://vod.test/diag2.m3u8'));
const vd2 = ultimo();
vd2.dispatch('playing');
timers[timers.length - 1].fn();
check('playing loga e cala o aviso de 8 s', diagLines.slice(doPlaying).some((l) => / playing \+/.test(l)) &&
  !diagLines.slice(doPlaying).some((l) => l.includes('sem playing')));

// SESSAO NAO FICA PRESA (dono: "tocou um trailer e depois nenhum toca mais").
// Um trailer toca e termina, ou falha, e fecha; o de OUTRO titulo, aberto
// depois, tem de chegar a `playing` e ser visto pelo C como tocando.
fechar();
abrir(...fonteVideo('https://vod.test/tituloA.m3u8'));
const tA = ultimo();
tA.dispatch('playing');
check('titulo A toca', estado() === 1);
tA.dispatch('ended');
check('titulo A termina', estado() === 0);
fechar();
check('fechar volta ao estado sem elemento', estado() === -2 && !body.children.includes(tA));
abrir(...fonteVideo('https://vod.test/tituloB.m3u8'));
const tB = ultimo();
check('titulo B cria elemento novo e comeca preparando', tB !== tA && estado() === -1);
tA.dispatch('error');
check('eventos tardios de A nao contaminam B', estado() === -1);
tB.dispatch('playing');
check('titulo B toca depois de A', estado() === 1);
tB.error = { code: 2, message: 'rede' };
tB.dispatch('error');
fechar();
abrir(...fonteVideo('https://vod.test/tituloC.m3u8'));
const tC = ultimo();
tC.dispatch('playing');
check('depois de um erro, titulo C ainda toca', estado() === 1 && tC !== tB);
fechar();

// These checks protect the C state machine around the real bridge. They are
// deliberately small; the event behavior above is the executable evidence.
check('hero reseta fade ao trocar ou desligar', /heroTrailerFade = 0\.0f/.test(home) && /!ajustes_trailer_hero\(\)/.test(home));
check('hero atualiza fade mesmo aguardando fonte', home.includes('goto trailer_hero_fim;') && home.includes('trailer_hero_fim:'));
check('preparo respeita orçamento total', /heroTrailerPrazoPreparacao/.test(home) && /NV_TRAILER_HERO_MAX_ESPERA_MS/.test(home));
check('hero pede retry idempotente no mesmo titulo', /if \(!heroTrailerTentado\)[\s\S]*extras_hero_trailer_pedir/.test(home));
check('worker nao usa metadata ampla', /\/videos\?api_key=/.test(extras) && !/lacoHeroTrailer[\s\S]*extras_pedir\(/.test(extras));
check('retry vazio tem cooldown e nao bloqueio permanente', extras.includes('HERO_TRAILER_RETRY_S') && !/heroTrailerTentativas >= 2/.test(extras));

// Samsung: o <video> nunca recebe o master da Apple (o motor HLS da TV trava
// no ABR dele — trailerapple.c, varianteMidia), so a playlist de midia.
check('Samsung entrega a variante de midia, nao o master',
  /#ifdef __EMSCRIPTEN__\s*r = e->toca\[0\] \? e->toca : NULL;/.test(apple) && /varianteMidia\(m, /.test(apple));
// Pagina de titulo: prazo e degrau seguinte, com log.
const prep = number('NV_TRAILER_PREPARA_MS');
check('detalhe tem prazo finito de preparo (~8 s)', prep >= 5000 && prep <= 10000);
check('detalhe: sem playing no prazo ou erro sobe Apple -> YouTube e loga',
  /trailerPrazo = agora \+ NV_TRAILER_PREPARA_MS/.test(detail) &&
  /\[trailer\] detalhe: %s %d ms/.test(detail) && /trailerEtapa = 2;/.test(detail) && /trailerEtapa = 3;/.test(detail));
check('hero loga a desistencia de cada fonte', /\[trailer\] hero: sem playing em %d ms/.test(home));
check('hero nao abre YouTube antes de a Apple responder',
  /if \(!u && !trailerapple_respondeu\(ci->imdb\) &&\s*decorrido < NV_TRAILER_HERO_MAX_ESPERA_MS\)\s*goto trailer_hero_fim;/.test(home));
check('C loga cada transicao de estado e o estado a cada tentativa',
  /\[trailer\] estado %d -> %d/.test(trailer) && /\[trailer\] tentativa: aberto=%d/.test(trailer));

console.log('trailer-hero-contract: tudo ok');
