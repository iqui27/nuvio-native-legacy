'use strict';
// Executa os EM_JS de producao do transporte WGT (nv_http_iniciar_bruto,
// nv_http_ttfb e nv_http_colher_bruto) como o navegador faria: fetch com
// cabecalhos. Na sonda (modo 2) o primeiro pedido usa redirect "manual" e os
// cabecalhos da fonte; um salto refaz a cadeia com "follow" levando so o
// Range, para nao atravessar cabecalho privado ate outra origem. Corpo
// contado no teto, abort e TTFB medido na chegada dos cabecalhos.
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
  const inits = [];
  const start = new Function('Module','UTF8ToString','HEAP32','HEAPU8','setTimeout','clearTimeout',
    'Date','AbortController','fetch','Uint8Array','XMLHttpRequest',
    'metodo','url','cabs','corpo','prazo','teto','modo', iniciar);
  const harvest = new Function('Module','UTF8ToString','HEAP32','HEAPU8','stringToUTF8','_malloc','lengthBytesUTF8',
    'id','tam','status','urlFinal','urlFinalTam','etag','etagTam','retryAfter','limitado','mime','mimeTam','tamanho','modo', colher);
  const ttfb = new Function('Module','id', ttfbJs);
  const fetch = async (url, o) => {
    inits.push(o);
    const resp = resposta.seq
      ? resposta.seq[Math.min(inits.length - 1, resposta.seq.length - 1)]
      : resposta;
    if (resp.throw) throw Error('network');
    const chunks = resp.chunks || [];
    return {status: resp.status, type: resp.type, url: resp.url || url,
      headers: {get: k => (resp.h || {})[k] || null},
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
    inits: () => inits};
}

(async () => {
  // 206 com Range: o primeiro pedido vai com todos os cabecalhos e redirect
  // manual; o corpo e contado ate o teto e nada e guardado.
  {
    const c = montar({status: 206, cabs: 'Range: bytes=0-1\nX-Probe: synthetic',
      teto: 2, chunks: [new Uint8Array([7, 8]), new Uint8Array(16)]});
    assert.equal(typeof c.Module.nvFioReqs[c.id].abortar, 'function');
    await c.done();
    assert.equal(c.inits().length, 1);
    assert.equal(c.inits()[0].redirect, 'manual');
    assert.equal(c.inits()[0].headers.Range, 'bytes=0-1');
    assert.equal(c.inits()[0].headers['X-Probe'], 'synthetic');
    assert.equal(c.ttfb(c.Module, c.id) >= 0, true);
    assert.equal(c.colhe(), 1);
    assert.equal(c.ints[5], 206);   // status
    assert.equal(c.ints[3], 2);     // bytes contados, cortados no teto
  }
  // Redirect: o Location fica opaco, a cadeia e refeita com follow levando
  // so o Range — X-Probe nao pode chegar a outra origem.
  {
    const c = montar({cabs: 'Range: bytes=0-1\nX-Probe: synthetic', teto: 2,
      seq: [{type: 'opaqueredirect', status: 0},
            {status: 206, chunks: [new Uint8Array([7, 8])]}]});
    await c.done();
    assert.equal(c.inits().length, 2);
    assert.equal(c.inits()[0].redirect, 'manual');
    assert.equal(c.inits()[0].headers['X-Probe'], 'synthetic');
    assert.equal(c.inits()[1].redirect, 'follow');
    assert.equal(c.inits()[1].headers.Range, 'bytes=0-1');
    assert.equal('X-Probe' in c.inits()[1].headers, false);
    assert.equal(c.colhe(), 1);
    assert.equal(c.ints[5], 206);
    assert.equal(c.ints[3], 2);
  }
  // 403 e resposta: o transporte devolve o status para a sonda, nao erro.
  {
    const c = montar({status: 403, chunks: []});
    await c.done();
    assert.equal(c.colhe(), 1);
    assert.equal(c.ints[5], 403);
  }
  // 403 cujo corpo estoura o prazo/aborta depois dos cabecalhos: o status
  // recebido e preservado (nao vira "nao testado").
  {
    const c = montar({status: 403, chunks: []});
    await new Promise(r => setTimeout(r, 0));
    c.Module.nvFioReqs[c.id].abortar();
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
  // Abort no meio do voo (antes dos cabecalhos): colhe falha.
  {
    const c = montar({status: 200});
    c.Module.nvFioReqs[c.id].abortar();
    await c.done();
    assert.equal(c.colhe(), 0);
  }
  console.log('farol_sonda_wgt: PASS (headers, redirect manual + follow com Range, contagem no teto, 403, falha, abort, ttfb)');
})().catch(e => { console.error(e); process.exitCode = 1; });
