// --pre-js de tests/fonteurl_wasm.c: simula o fetch do navegador para as
// urls https://fixture.invalid/*, com ReadableStream e AbortController de
// verdade (Node >= 18). Confere tambem as opcoes que o rede.c exige:
// redirect:"error", credentials:"omit" e signal sempre presente.
globalThis.fetch = new Proxy(globalThis.fetch, {
  apply(original, receiver, args) {
    const [url, options] = args;
    if (!String(url).startsWith('https://fixture.invalid/')) return Reflect.apply(original, receiver, args);
    if (options.redirect !== 'error' || options.credentials !== 'omit' || !options.signal) throw Error('unexpected fetch options');
    const path = new URL(url).pathname;
    if (path === '/cors') return Promise.reject(new TypeError('CORS fixture'));
    if (path === '/no-reader') return Promise.resolve({status: 200});
    if (path === '/timeout') return new Promise((resolve, reject) => {
      options.signal.addEventListener('abort', () => reject(new TypeError('timer aborted')));
    });
    if (path === '/oversized' || path === '/large') {
      // Corpo grande chega em fatias por pull(), como a rede de verdade —
      // enfileirar os 32 MiB de uma vez no start() mediria a alocacao da
      // fixture dentro de um passo do escalonador, nao a drenagem.
      let bloco = 0;
      return Promise.resolve({status: 200, body: new ReadableStream({
        pull(controller) {
          if (bloco < 512) { controller.enqueue(new Uint8Array(65536)); bloco++; }
          else if (path === '/oversized' && bloco === 512) { controller.enqueue(new Uint8Array(1)); bloco++; }
          else controller.close();
        }
      })});
    }
    const stream = new ReadableStream({
      start(controller) {
        if (path === '/reject' || path === '/reject-close') {
          controller.enqueue(new Uint8Array(20000).fill(7));
          const close = path === '/reject-close' ? setTimeout(() => controller.close(), 50) : null;
          options.signal.addEventListener('abort', () => {
            if (close !== null) clearTimeout(close);
            globalThis.fixtureCancels = (globalThis.fixtureCancels || 0) + 1;
            controller.error(new TypeError('reader aborted'));
          });
          return;
        }
        const delay = path === '/delayed' ? 250 : path === '/cancel' ? 170 : 0;
        if (path === '/cancel') globalThis.fixtureInflight = 1;
        setTimeout(() => {
          if (path === '/cancel') globalThis.fixtureInflight = 0;
          controller.enqueue(new Uint8Array(16));
          controller.close();
        }, delay);
      }
    });
    return Promise.resolve({status: path === '/http404' ? 404 : 200, body: stream});
  }
});

// Acelera so o prazo de 30 s do downloader da fonteurl (vira 500 ms) — o
// prazo de 100 ms das sondas de recusa passa intacto.
const actualSetTimeout = globalThis.setTimeout;
globalThis.setTimeout = (fn, ms, ...args) => {
  if (ms === 30000) return actualSetTimeout(fn, 500, ...args);
  return actualSetTimeout(fn, ms, ...args);
};
