// XMLHttpRequest FALSO para tests/texgif-tizen.sh (Node). Serve o arquivo de
// NV_TEXGIF_GIF para URLs com "gif" e o de NV_TEXGIF_JPG para o resto, e CONTA
// os pedidos: e a contagem que o teste compara. So o que nv_http (src/rede.c)
// usa: open/overrideMimeType/setRequestHeader/send, status, responseURL,
// getResponseHeader e responseText em latin-1 (o truque x-user-defined).
if (typeof XMLHttpRequest === 'undefined' && typeof require === 'function') {
  var nvFs = require('fs');
  globalThis.nvXhrN = globalThis.nvXhrN || 0;
  globalThis.XMLHttpRequest = function () {};
  XMLHttpRequest.prototype.open = function (m, u) { this.u = u; };
  XMLHttpRequest.prototype.overrideMimeType = function () {};
  XMLHttpRequest.prototype.setRequestHeader = function () {};
  XMLHttpRequest.prototype.getResponseHeader = function () { return null; };
  XMLHttpRequest.prototype.send = function () {
    var arq = this.u.indexOf('gif') >= 0 ? process.env.NV_TEXGIF_GIF : process.env.NV_TEXGIF_JPG;
    globalThis.nvXhrN++;
    this.status = 200;
    this.responseURL = this.u;
    this.responseText = nvFs.readFileSync(arq).toString('latin1');
  };
}
