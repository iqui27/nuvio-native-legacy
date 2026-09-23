// Polyfills do build Tizen 4 (Chromium M56). Entra num <script> logo apos
// <head> do shell (tools/tizen.sh --tizen4), antes do glue do Emscripten.
// ES5 puro: este arquivo NAO passa pelo esbuild.
(function () {
  var g = typeof self !== "undefined" ? self : window;
  // globalThis (M71): o glue do Emscripten e o EM_JS de video_tizen.c usam.
  if (typeof g.globalThis === "undefined") g.globalThis = g;
  // padStart (M57): o relogio do registro em tizen-shell.html usa.
  if (!String.prototype.padStart) {
    String.prototype.padStart = function (n, c) {
      var s = String(this);
      c = c === undefined ? " " : String(c);
      if (s.length >= n || !c.length) return s;
      var f = "";
      while (f.length < n - s.length) f += c;
      return f.slice(0, n - s.length) + s;
    };
  }
  // Atomics (M60, junto com o SharedArrayBuffer). Neste build ha UM fio so, e
  // o JS nunca roda ao mesmo tempo que o wasm: a versao comum de cada operacao
  // e exatamente a atomica. dados.c e cachearte.c usam load/compareExchange/
  // notify no HEAP32; aqui eles continuam valendo sem mudar o C.
  if (typeof g.Atomics === "undefined") {
    g.Atomics = {
      load: function (a, i) { return a[i]; },
      store: function (a, i, v) { a[i] = v; return v; },
      exchange: function (a, i, v) { var o = a[i]; a[i] = v; return o; },
      compareExchange: function (a, i, e, v) { var o = a[i]; if (o === e) a[i] = v; return o; },
      add: function (a, i, v) { var o = a[i]; a[i] = o + v; return o; },
      sub: function (a, i, v) { var o = a[i]; a[i] = o - v; return o; },
      or: function (a, i, v) { var o = a[i]; a[i] = o | v; return o; },
      and: function (a, i, v) { var o = a[i]; a[i] = o & v; return o; },
      notify: function () { return 0; },
      wait: function () { return "timed-out"; }
    };
  }
})();
