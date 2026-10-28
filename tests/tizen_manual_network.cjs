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
const calls = [], opened = [], active = new Set();
let aborted = 0;
const stream = 'https://stream.example.invalid/video.mkv';
let fixture = {status: 200, bytes: Buffer.from('héllo'), headers: {}};
class XMLHttpRequest {
  open(method, url) { this.method = method; this.url = url; this.headers = {}; }
  setRequestHeader(name, value) { this.headers[name] = value; }
  overrideMimeType(value) { assert.equal(value, 'text/plain; charset=x-user-defined'); }
  getResponseHeader(name) { return fixture.headers[name] ?? null; }
  abort() {
    if (active.delete(this)) aborted++;
    if (this.onabort) this.onabort();
  }
  send(body) {
    calls.push({url: this.url, init: {method: this.method, headers: this.headers, body, redirect: 'follow'}});
    active.add(this);
    if (fixture.never) return;
    queueMicrotask(() => {
      if (!active.has(this)) return;
      if (this.url === stream) {
        active.delete(this); this.onerror(); return;
      }
      this.status = fixture.status;
      this.responseURL = this.url + '/redirected';
      this.readyState = 2;
      this.onreadystatechange();
      if (!active.has(this)) return;
      this.responseText = '';
      for (const byte of fixture.bytes) this.responseText += String.fromCharCode(byte < 128 ? byte : 0xf700 + byte);
      this.onprogress({loaded: fixture.bytes.length});
      if (!active.delete(this)) return;
      this.readyState = 4;
      this.onload();
    });
  }
}
const context = vm.createContext({
  Module: {}, UTF8ToString: x => x, Uint8Array, XMLHttpRequest,
  setTimeout, clearTimeout, performance: {now: () => 1},
  webapis: {avplay: {
    open(url) { opened.push(url); },
    setListener() {}, setDisplayMethod() {}, prepareAsync() {},
  }},
  fetch: (url, init) => {
    calls.push({url, init});
    const token = {};
    active.add(token);
    return new Promise((resolve, reject) => {
      if (init.signal) init.signal.addEventListener('abort', () => {
        if (active.delete(token)) aborted++;
        reject(new Error('aborted'));
      });
      if (fixture.never) return;
      active.delete(token);
      if (url === stream) { reject(new TypeError('TLS/CORS fixture')); return; }
      let read = false;
      resolve({status: fixture.status, url: url + '/redirected',
        headers: {get: name => fixture.headers[name] ?? null},
        body: {cancel: async () => {}, getReader: () => ({
          read: async () => read ? {done: true} : (read = true, {value: fixture.bytes}),
          cancel: async () => {},
        })}});
    });
  },
});
// Promise.finally shipped in Chrome 63; bridge must not require it either.
vm.runInContext('window = this; Promise.prototype.finally = undefined;', context);
vm.runInContext('request = function(metodo,url,cabs,corpo,prazo,teto,modo) {' +
  request.replace(/\\\\/g, '\\') + '\n};\nav = function(cmd,txt,a,b,c,d,dst,dstTam) {' +
  bridge.replace(/\\\\/g, '\\') + '\n};', context);
async function get(url, controller, mode = 0, deadline = 100, body = '') {
  context.AbortController = controller;
  const id = context.request(body ? 'POST' : 'GET', url, 'X-Test: fixture', body, deadline, 64, mode);
  for (let i = 0; i < 200 && !context.Module.nvFioReqs[id].pronto; i++)
    await new Promise(resolve => setTimeout(resolve, 1));
  assert(context.Module.nvFioReqs[id].pronto, 'request must settle on Chromium 63');
  return context.Module.nvFioReqs[id];
}
(async () => {
  const failures = [];
  for (const controller of [undefined, AbortController]) {
    fixture = {status: 200, bytes: Buffer.from('héllo'), headers: {
      etag: 'fixture', 'content-type': 'Text/Plain; charset=utf-8', 'retry-after': '7',
    }};
    const text = await get('https://metadata.example.invalid/meta', controller, 0, 100, 'request body');
    assert.equal(calls[calls.length - 1].init.method, 'POST');
    assert.equal(calls[calls.length - 1].init.body, 'request body');
    assert.equal(text.status, 200);
    assert.equal(Buffer.from(text.bytes).toString(), 'héllo');
    assert(text.url.endsWith('/redirected'));
    assert.equal(text.etag, 'fixture'); assert.equal(text.mime, 'text/plain');
    assert.equal(text.retry, 7);
    const failed = await get(stream, controller);
    assert.equal(failed.erro, true, 'TLS/CORS rejection is a transport failure');
    assert.equal(failed.status, 0);
    fixture = {status: 206, bytes: Buffer.from([0, 127, 128, 255]), headers: {}};
    assert.deepEqual(Array.from((await get('https://stream.example.invalid/binary', controller)).bytes), [0, 127, 128, 255]);
    fixture.bytes = Buffer.alloc(128, 255);
    const capped = await get('https://stream.example.invalid/range', controller);
    assert(capped.limitado && capped.bytes.length === 64);
    const count = await get('https://stream.example.invalid/count', controller, 2);
    assert(count.limitado && count.n === 64 && count.bytes.length === 0);
    const probe = await get('https://stream.example.invalid/probe', controller, 1);
    assert.equal(probe.erro, false); assert.equal(probe.bytes.length, 0);
    fixture = {never: true};
    for (let i = 0; i < 3; i++) {
      const before = aborted;
      const timed = await get(stream, controller, 0, 5);
      assert(timed.pronto && timed.erro && timed.status === 0);
      assert.equal(active.size, 0, 'no active request remains after no-headers timeout');
      assert.equal(aborted, before + 1, 'deadline aborts transport before headers');
      failures.push(timed.status || 6);
    }
  }
  assert(calls.every(x => x.init.redirect === 'follow' && x.init.headers['X-Test'] === 'fixture'));
  // Feed actual timeout outcomes through production manual handoff and health.
  assert.equal(context.av('abrir', stream), 1);
  assert.deepEqual(opened, [stream]);
  process.stdout.write(execFileSync(process.argv[2], failures.map(String)));
  console.log('PASS: abort before headers; bounded text/binary transports; manual handoff stays online');
})().catch(error => { console.error(error.name + ': ' + error.message); process.exitCode = 1; });
