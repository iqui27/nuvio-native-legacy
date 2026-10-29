'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const src = fs.readFileSync('src/rede.c', 'utf8');
const found = src.match(/EM_JS\(int, nv_http_iniciar_assinc,[\s\S]*?, \{([\s\S]*?)\n\}\);/);
const collect = src.match(/EM_JS\(char \*, nv_http_colher_assinc,[\s\S]*?, \{([\s\S]*?)\n\}\);/);
assert(found, 'production async request body');
const heap = new ArrayBuffer(4096);
const HEAP32 = new Int32Array(heap), HEAPU8 = new Uint8Array(heap);
let header = null, noHeader = false, now = Date.UTC(2026, 9, 3, 12, 0, 0);
class Clock extends Date { static now() { return now; } }
const Module = {};
const run = new Function('Module', 'UTF8ToString', 'HEAP32', 'HEAPU8', '_malloc',
  'stringToUTF8', 'Date', 'fetch', 'setTimeout', 'clearTimeout',
  'metodo', 'url', 'cabs', 'corpo', 'prazo', 'teto', 'modo', found[1]);
async function check(value, expected, throws = false) {
  header = value; noHeader = throws; HEAP32[4] = 777;
  const id = run(Module, p => p === 1 ? 'GET' : 'https://fixture.invalid/', HEAP32,
    HEAPU8, () => 256, () => {}, Clock, async () => ({status:429,url:'final',headers:{get:key=>{
      if (throws) throw Error('not exposed'); return key==='retry-after'?header:null;}},
      body:{getReader(){return {read:async()=>({done:true})};}}}), setTimeout, clearTimeout,


    'GET', 'https://fixture.invalid/', 0, 0, 1000, 64, 2);
  const harvest = new Function('Module','UTF8ToString','HEAP32','HEAPU8','_malloc','stringToUTF8',
    'id','tam','status','urlFinal','urlFinalTam','etag','etagTam','retryAfter','limitado','mime','mimeTam','tamanho','modo',collect[1]);
  await new Promise(r=>setTimeout(r,10));
  harvest(Module,()=>0,HEAP32,HEAPU8,()=>256,()=>{},id,0,0,0,0,0,0,16,0,0,0,0,2);
  assert.equal(HEAP32[4], expected, JSON.stringify(header));
  delete Module.nvFioReqs[id];
}
(async()=>{
  await check('19', 19);
  await check(' 120 ', 120);
  await check('999999999999999999999999', 2147483647);
  await check('Sat, 03 Oct 2026 12:01:30 GMT', 90);
  await check('Sat, 03 Oct 2026 11:00:00 GMT', 0);
  await check('nonsense', 0);
  await check('12 garbage', 0);
  await check('-5', 0);
  await check('', 0);
  await check(null, 0);
  await check('19', 0, true);
  assert(src.includes('r->erro = REDE_INDISPONIVEL;\n  return 0; /* nenhum XHR'));
  console.log('rede_retry_wgt: PASS (exposed seconds/date; hidden/invalid keeps unknown; strict request unavailable)');
})().catch(e=>{console.error(e);process.exitCode=1});
