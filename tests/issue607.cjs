'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const cp = require('node:child_process');
const vm = require('node:vm');

// Exercise the C stringification used by EM_JS, including compiler escapes.
function bridge(file, name) {
  const source = fs.readFileSync(file, 'utf8');
  const match = source.match(new RegExp('EM_JS\\([^,]+, ' + name + ',[\\s\\S]*?\\n\\}\\);'));
  assert(match, name);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'issue607-'));
  try {
    fs.writeFileSync(path.join(dir, 'bridge.c'), '#include <stdio.h>\n' +
      '#define EM_JS(ret,name,args,...) const char *body = #__VA_ARGS__;\n' +
      match[0] + '\nint main(void) { puts(body); }\n');
    cp.execFileSync('cc', [path.join(dir, 'bridge.c'), '-o', path.join(dir, 'bridge')], {stdio:'pipe'});
    let out = cp.execFileSync(path.join(dir, 'bridge'), {encoding:'utf8'}).trim();
    return out.trim();
  } finally { fs.rmSync(dir, {recursive:true, force:true}); }
}

async function probe() {
  for (const [raw, expected] of [['9',9], [' 64 ',64], ['65',-1], ['ddd',-1], ['9x',-1], [null,-1]]) {
    let aborted = false;
    const module = {};
    const run = vm.runInNewContext('(function(metodo,url,cabs,corpo,prazo,teto,modo){' +
      bridge('src/rede.c','nv_http_iniciar_assinc') + '})', {
      Module:module,UTF8ToString:x=>x,setTimeout,clearTimeout,Uint8Array,
      AbortController:class {abort(){aborted=true;}},
      fetch:async()=>({status:200,url:'url',headers:{get:key=>key==='content-length'?raw:null},
        body:{getReader(){throw Error('probe must not read media');}}})
    });
    run('GET','url',0,0,1000,64,1);
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(module.nvFioReqs[1].tamanho,expected,'Content-Length '+raw);
    assert(aborted); assert(!module.nvFioReqs[1].erro);
  }
  console.log('PASS: generated probe validates Content-Length and aborts before media');
}

function avplay() {
  const heap = Buffer.alloc(1024);
  function pointer(text) { heap.fill(0); heap.write(text, 16); return 16; }
  const context = vm.createContext({console, document:{}, UTF8ToString:p=>{
    assert.equal(typeof p,'number','EM_JS strings must be WASM pointers');
    return heap.toString('utf8',p,heap.indexOf(0,p));
  },
    performance:{now:()=>0}, setTimeout() {}, clearTimeout() {}, stringToUTF8() {}});
  vm.runInContext('window = globalThis', context);
  vm.runInContext(fs.readFileSync('tools/fake-avplay.js','utf8'), context);
  const run = vm.runInContext('(function(cmd,txt,a,b,c,d,dst,dstTam) {' +
    bridge('src/video_tizen.c','nv_av') + '})', context);
  context.nv_av = run;
  const call = op => run(pointer(op),0,0,0,0,0,0,0);
  assert.equal(call('abrir'),1);
  assert.equal(call('abrir'),1,'healthy reopen must close then open');

  const player = context.webapis.avplay;
  const stop = player.stop, close = player.close;
  let closed = 0;
  player.stop = () => { throw Error('stop failed'); };
  player.close = () => { closed++; close(); };
  assert.equal(call('encerrar'),0,'shutdown must report stop failure');
  assert.equal(closed,1,'close must run despite stop failure');
  assert.match(context.__nvav.erro,/stop/);
  assert.equal(call('abrir'),1);
  assert.equal(call('abrir'),0,'reopen must report stop failure');
  assert.equal(closed,2);
  assert.equal(context.__avReg().filter(x=>x.startsWith('open(')).length,3);
  player.stop = stop;
  player.close = () => { throw Error('close failed'); };
  assert.equal(call('abrir'),1);
  assert.equal(call('encerrar'),0);
  assert.equal(context.__nvav.aberto,1,'failed close must remain retryable');
  assert.match(context.__nvav.erro,/close/);
  player.close = close;
  assert.equal(call('encerrar'),1);
  assert.equal(call('abrir'),1);
  console.log('PASS: AVPlay always closes, reports failures, retries cleanup');
}

function decoder() {
  const source = fs.readFileSync('src/webp.c','utf8');
  const body = source.match(/void navegador_iniciar\(void\)[\s\S]*?EM_ASM\(\{([\s\S]*?)\n  \},/)[1];
  const heap = new Int32Array(new SharedArrayBuffer(1024));
  const workers = [];
  class Worker {
    constructor() { workers.push(this); }
    postMessage() {}
    terminate() { assert.equal(heap[2] & 4,0,'publish death only after termination'); this.dead = true; }
  }
  const context = {Module:{},HEAP32:heap,wasmMemory:{buffer:heap.buffer},SharedArrayBuffer,
    OffscreenCanvas:function(){},Worker,MessageChannel:class {constructor(){this.port1={};this.port2={};}},console};
  vm.runInNewContext('(function($0,$1){' + body + '})(4,8)',context);
  heap[2] = 3;
  workers[0].onerror({message:'decoder failure'});
  assert(workers.every(w=>w.dead),'both workers must stop before jobs can be freed');
  assert.equal(heap[2],7);
  assert(context.Module.nvDec.morto);
  const declarations = source.slice(source.indexOf('enum { J_EST'),source.indexOf('// printf SEM ESPERAR'));
  const functions = source.slice(source.indexOf('static void soltarJob'),source.indexOf('int navegador_abandonados_vivos'));
  const dir = fs.mkdtempSync(path.join(os.tmpdir(),'issue607-gc-'));
  try {
    fs.writeFileSync(path.join(dir,'gc.c'), '#include <stdint.h>\n#include <stdlib.h>\n#include <pthread.h>\n#include <assert.h>\n' + declarations + functions + `
int main(void) {
  int32_t job[J_INTS] = {0}; job[J_ORIGEM] = 2;
  canalEstado = 3; largarCanalMorto(job); assert(job[J_EST] == 0);
  canalEstado = 7; largarCanalMorto(job); assert(job[J_EST] == 1);
  job[J_EST] = 4; largarCanalMorto(job); assert(job[J_EST] == 5);
  job[J_EST] = 0; job[J_ORIGEM] = 1; largarCanalMorto(job); assert(job[J_EST] == 0);
  lixo = calloc(J_INTS,sizeof(int32_t)); nLixo = 1;
  lixo[J_EST] = 4; lixo[J_ORIGEM] = 2;
  assert(varrer() == 0 && !lixo);
  return 0;
}\n`);
    cp.execFileSync('cc',[path.join(dir,'gc.c'),'-pthread','-o',path.join(dir,'gc')]);
    cp.execFileSync(path.join(dir,'gc'));
  } finally { fs.rmSync(dir,{recursive:true,force:true}); }
  console.log('PASS: decoder death fails active jobs; GC frees abandoned jobs');
}

function fatal() {
  for (const file of ['tools/tizen-shell.html','tools/vidaa-shell.html']) {
    const source = fs.readFileSync(file,'utf8');
    const block = source.slice(source.indexOf('    window.addEventListener("error"'),source.indexOf('    setTimeout(function ()',source.indexOf('    window.addEventListener("error"')));
    const functions = source.match(/    function fatal\([\s\S]*?\n    \}/);
    const listeners = {}, lines = [], panel = {hidden:true};
    let paints = 0;
    vm.runInNewContext((functions ? functions[0] : '') + block, {
      window:{addEventListener:(event,fn)=>listeners[event]=fn}, console:{log(){}},
      mostrar:text=>lines.push(text), painel:panel, pintar:()=>paints++,
      document:{getElementById:()=>panel}
    });
    for (const event of ['error','unhandledrejection']) {
      panel.hidden = true;
      listeners[event]({message:'fatal failure',reason:Error('fatal rejection')});
      assert.equal(panel.hidden,false,file + ': ' + event);
      assert.match(lines.join('\n'),/reopen|restart|reload/i);
    }
    assert(paints >= 2);
  }
  console.log('PASS: both web shells reveal fatal errors with recovery guidance');
}

async function deadlines() {
  const module = {};
  let aborted = false, elapsed;
  const run = vm.runInNewContext('(function(metodo,url,cabs,corpo,prazo,teto,modo){' +
    bridge('src/rede.c','nv_http_iniciar_assinc') + '})', {
    Module:module,UTF8ToString:x=>x,AbortController:class {abort(){aborted=true;}},
    setTimeout:(fn,ms)=>{elapsed=ms;fn();return 1;},clearTimeout(){},
    fetch:()=>new Promise(()=>{})
  });
  run('GET','url',0,0,37,64,0);
  assert.equal(elapsed,37); assert(aborted);
  assert.equal(module.nvFioReqs[1].pronto,true,'deadline must release C caller');
  console.log('PASS: shared fetch transport enforces requested deadline');
}

async function limits() {
  const source = fs.readFileSync('src/rede.c','utf8');
  const body = source.match(/EM_JS\(int, nv_http_iniciar_assinc,[\s\S]*?, \{([\s\S]*?)\n\}\);/)[1];
  const module = {};
  let reads = 0, aborted = false;
  const response = {status:200,url:'url',headers:{get:()=>null},
    body:{getReader:()=>({read:async()=>{reads++;return {done:false,value:new Uint8Array(16)};}})},
    arrayBuffer:async()=>{reads=1000;return new ArrayBuffer(16000);}};
  vm.runInNewContext('(function(metodo,url,cabs,corpo,prazo,teto,modo,pronto){' + body + '})("GET","url",0,0,1000,32,0,0)', {
    Module:module,UTF8ToString:x=>x,HEAP32:new Int32Array(8),AbortController:class {abort(){aborted=true;}},
    setTimeout,clearTimeout,fetch:async()=>response,Uint8Array
  });
  await new Promise(resolve=>setTimeout(resolve,20));
  assert(reads <= 3,'must stop reading at limit; reads=' + reads);
  assert(aborted,'oversized transfer must abort');
  assert.equal(module.nvFioReqs[1].pronto,true);
  console.log('PASS: ignored Range/chunked response aborts at byte limit');
}

const cases = {probe, avplay, decoder, fatal, deadlines, limits};
(async () => {
  const selected = process.argv.slice(2);
  for (const name of selected.length ? selected : Object.keys(cases)) await cases[name]();
})().catch(error => { console.error(error); process.exitCode = 1; });
