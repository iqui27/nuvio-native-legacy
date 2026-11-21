'use strict';
// Executa os EM_JS de producao do transporte WGT (nv_http_iniciar_bruto,
// nv_http_ttfb e nv_http_colher_bruto) como o navegador faria: fetch com
// cabecalhos, redirect seguido pelo runtime, corpo contado no teto (modo 2,
// o que a sonda usa), abort e TTFB medido na chegada dos cabecalhos.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const src = fs.readFileSync('src/rede.c', 'utf8');
const corpoDe = nome => src.match(
  new RegExp('EM_JS\\([^)]+, ' + nome + ',[\\s\\S]*?, \\{([\\s\\S]*?)\\n\\}\\);')
)[1].replace(/\\\\/g, '\\');
const iniciar = corpoDe('nv_http_iniciar_bruto');
const ttfbJs = corpoDe('nv_http_ttfb');
const colher = corpoDe('nv_http_colher_bruto');

function montar(resposta) {
  const ints = new Int32Array(64), bytes = new Uint8Array(4096);
  const Module = {};
  let init;
  const start = new Function('Module','UTF8ToString','HEAP32','HEAPU8','setTimeout','clearTimeout',
    'Date','AbortController','fetch','Uint8Array','XMLHttpRequest',
    'metodo','url','cabs','corpo','prazo','teto','modo', iniciar);
  const harvest = new Function('Module','UTF8ToString','HEAP32','HEAPU8','stringToUTF8','_malloc','lengthBytesUTF8',
    'id','tam','status','urlFinal','urlFinalTam','etag','etagTam','retryAfter','limitado','mime','mimeTam','tamanho','modo', colher);
  const ttfb = new Function('Module','id', ttfbJs);
  const fetch = async (url, o) => {
    init = o;
    if (resposta.throw) throw Error('network');
    const chunks = resposta.chunks || [];
    return {status: resposta.status, url: resposta.url || url,
      headers: {get: k => (resposta.h || {})[k] || null},
      body: {getReader(){ let i = 0;
        return {read: async () => i < chunks.length
          ? {value: chunks[i++], done: false} : {done: true}}; }}};
  };
  const id = start(Module, x=>x, ints, bytes, setTimeout, clearTimeout, Date,
    AbortController, fetch, Uint8Array, function () {},
    'GET', 'https://fonte.invalid/v.mkv', resposta.cabs, 0, 2000,
    resposta.teto || 512, resposta.modo === undefined ? 2 : resposta.modo);
  return {id, Module, harvest, ttfb, ints,
    done: () => new Promise(r => setTimeout(r, 20)),
    colhe() {
      // tam=12, status=20, retryAfter=24, limitado=28, tamanho=32 (offsets HEAP32)
      return harvest(Module, x=>x, ints, bytes, ()=>0, ()=>256, x=>Buffer.byteLength(x),
        this.id, 12, 20, 0, 0, 0, 0, 24, 28, 0, 0, 32, 2);
    },
    init: () => init};
}

(async () => {
  // 206 com Range: o corpo e contado ate o teto e nada e guardado.
  {
    const c = montar({status: 206, cabs: 'Range: bytes=0-1\nX-Probe: synthetic',
      teto: 2, chunks: [new Uint8Array([7, 8]), new Uint8Array(16)]});
    assert.equal(typeof c.Module.nvFioReqs[c.id].abortar, 'function');
    await c.done();
    assert.equal(c.init().redirect, 'follow');
    assert.equal(c.init().headers.Range, 'bytes=0-1');
    assert.equal(c.init().headers['X-Probe'], 'synthetic');
    assert.equal(c.ttfb(c.Module, c.id) >= 0, true);
    assert.equal(c.colhe(), 1);
    assert.equal(c.ints[5], 206);   // status
    assert.equal(c.ints[3], 2);     // bytes contados, cortados no teto
  }
  // 403 e resposta: o transporte devolve o status para a sonda, nao erro.
  {
    const c = montar({status: 403, chunks: []});
    await c.done();
    assert.equal(c.colhe(), 1);
    assert.equal(c.ints[5], 403);
  }
  // Falha de fetch (sem resposta): colhe falha, status 0 -> "nao testado".
  {
    const c = montar({throw: true});
    await c.done();
    assert.equal(c.colhe(), 0);
    assert.equal(c.ints[5], 0);
  }
  // Abort no meio do voo: o registro marca erro e a colheita falha.
  {
    let resolver;
    const c = montar({status: 200});
    c.Module.nvFioReqs[c.id].abortar();
    await c.done();
    assert.equal(c.colhe(), 0);
  }
  console.log('farol_sonda_wgt: PASS (headers, redirect follow, contagem no teto, 403, falha, abort, ttfb)');
})().catch(e => { console.error(e); process.exitCode = 1; });
