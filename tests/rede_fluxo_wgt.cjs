'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const src = fs.readFileSync('src/rede.c', 'utf8');
const found = src.match(/EM_ASYNC_JS\(int, nv_http_fluxo,[\s\S]*?, \{([\s\S]*?)\n\}\);/);
assert(found);
const body = found[1];
const run = new (Object.getPrototypeOf(async function() {}).constructor)(
  'fetch', 'ReadableStream', 'AbortController', 'UTF8ToString', 'HEAP32', 'HEAPU32',
  'HEAPU8', '_malloc', '_free', 'url', 'prazo', 'teto', 'callback', 'usuario', 'status', 'total', 'wasmTable', body);
async function check(chunks, status, cap, expected, accept = true) {
  const heap = new ArrayBuffer(32768), bytes = new Uint8Array(heap);
  const ints = new Int32Array(heap), uints = new Uint32Array(heap);
  let calls = 0, size = 0, allocations = 0, frees = 0, signal;
  const fetch = async (url, options) => {
    assert.equal(url, 'https://fixture.invalid/');
    assert.equal(options.redirect, 'error'); assert.equal(options.credentials, 'omit');
    signal = options.signal;
    return {status, body: {getReader: () => ({read: async () => chunks.length ?
      {value: chunks.shift(), done: false} : {done: true}})}};
  };
  const result = await run(fetch, ReadableStream, AbortController, () => 'https://fixture.invalid/',
    ints, uints, bytes, n => {allocations++; assert.equal(n, 16384); return 256;},
    p => {assert.equal(p, 256); frees++;}, 1, 1000, cap,
    (p, n, u) => {assert.equal(u, 99); assert(n <= 16384); calls++; size += n;
      assert.equal(bytes[p], 42); return accept ? 1 : 0;}, 99, 4, 8, {get: f => f});
  assert.equal(result, expected); assert.equal(allocations, 1); assert.equal(frees, 1);
  assert(signal.aborted); assert.equal(ints[1], status);
  return {calls, size, total: uints[2]};
}
(async () => {
  const data = n => new Uint8Array(n).fill(42);
  assert.deepEqual(await check([data(40000), data(7)], 200, 50000, 0),
    {calls: 4, size: 40007, total: 40007});
  assert.deepEqual(await check([data(33)], 200, 32, 8), {calls: 0, size: 0, total: 0});
  assert.deepEqual(await check([data(32)], 503, 64, 0), {calls: 0, size: 0, total: 32});
  assert.equal((await check([data(32)], 200, 64, 4, false)).calls, 1);
  assert.equal(await run(null, undefined, AbortController), 2);
  console.log('rede_fluxo_wgt: PASS (16 KiB buffer, chunks, cap, HTTP error, callback abort, unavailable)');
})().catch(e => {console.error(e); process.exitCode = 1;});
