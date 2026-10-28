// WGT production bridges under Chromium 63 capabilities; no live requests.
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {execFileSync} = require('node:child_process');
const network = fs.readFileSync(process.env.TIZEN_NETWORK_SOURCE || 'src/rede.c', 'utf8');
const request = network.match(/EM_JS\(int, nv_http_iniciar_bruto,[\s\S]*?int modo\), \{([\s\S]*?)\n\}\);/)[1];
const video = fs.readFileSync('src/video_tizen.c', 'utf8');
const bridge = video.match(/EM_JS\(double, nv_av,[\s\S]*?int dstTam\), \{([\s\S]*?)\n\}\);/)[1];
const calls = [];
const opened = [];
const context = vm.createContext({
  Module: {}, UTF8ToString: x => x, Uint8Array,
  setTimeout, clearTimeout, performance: {now: () => 1},
  webapis: {avplay: {
    open(url) { opened.push(url); },
    setListener() {}, setDisplayMethod() {}, prepareAsync() {},
  }},
  fetch: async (url, init) => {
    calls.push({url, init});
    if (url.includes('stream.')) throw new TypeError('TLS/CORS fixture');
    return {status: 200, url, headers: {get: () => null}, body: null};
  },
});
vm.runInContext('window = this; Promise.prototype.finally = undefined;', context);
vm.runInContext('request = function(metodo,url,cabs,corpo,prazo,teto,modo) {' +
  request.replace(/\\\\/g, '\\') + '\n};\nav = function(cmd,txt,a,b,c,d,dst,dstTam) {' +
  bridge.replace(/\\\\/g, '\\') + '\n};', context);
async function get(url, controller) {
  context.AbortController = controller;
  const id = context.request('GET', url, '', '', 100, 64, 0);
  for (let i = 0; i < 30 && !context.Module.nvFioReqs[id].pronto; i++)
    await new Promise(resolve => setTimeout(resolve, 1));
  assert(context.Module.nvFioReqs[id].pronto, 'request must settle on Chromium 63');
  return context.Module.nvFioReqs[id];
}
(async () => {
  process.stdout.write(execFileSync(process.argv[2]));
  // Direct manual selection has no browser preflight; AVPlay gets the URL.
  const url = 'https://stream.example.invalid/video.mkv';
  assert.equal(context.av('abrir', url), 1);
  assert.equal(opened.length, 1);
  assert.equal(opened[0], url);
  for (const controller of [undefined, AbortController]) {
    assert.equal((await get('https://metadata.example.invalid/meta', controller)).status, 200);
    const failed = await get(url, controller);
    assert.equal(failed.erro, true, 'TLS/CORS rejection is a transport failure');
    assert.equal(failed.status, 0);
    assert.equal((await get('https://subtitle.example.invalid/sub', controller)).status, 200);
  }
  assert.equal(calls.length, 6);
  assert(calls.every(x => x.init.redirect === 'follow'));
  context.AbortController = undefined;
  let cancelled = 0, reads = 0;
  context.fetch = async () => ({status: 206, headers: {get: () => null},
    body: {cancel: async () => { cancelled++; }, getReader: () => ({
      read: async () => { reads++; return {value: new Uint8Array(128)}; },
      cancel: async () => { cancelled++; },
    })}});
  const capped = await get('https://stream.example.invalid/range', undefined);
  assert(capped.limitado && capped.bytes.length === 64);
  assert.equal(reads, 1); assert.equal(cancelled, 1);
  const probe = context.request('GET', 'https://stream.example.invalid/probe', '', '', 100, 64, 1);
  await new Promise(resolve => setTimeout(resolve, 5));
  assert(context.Module.nvFioReqs[probe].pronto);
  assert.equal(reads, 1); assert.equal(cancelled, 2);
  let respond;
  context.fetch = () => new Promise(resolve => { respond = resolve; });
  const late = context.request('GET', 'https://stream.example.invalid/late', '', '', 1, 64, 0);
  await new Promise(resolve => setTimeout(resolve, 5));
  assert(context.Module.nvFioReqs[late].pronto && context.Module.nvFioReqs[late].erro);
  respond({body: {cancel: async () => { cancelled++; }}});
  await new Promise(resolve => setTimeout(resolve, 5));
  assert.equal(cancelled, 3, 'late response body is cancelled after deadline');
  console.log('PASS: WGT manual AVPlay handoff; Chromium 63 fetch settles; isolated TLS/CORS failure');
})().catch(error => { console.error(error.name + ': ' + error.message); process.exitCode = 1; });
