// Ponte e atalhos reais do tools/tizen-shell.html num DOM fingido, com
// captura antes do alvo. Sem TV: nao prova que o firmware abre/dita no IME.
//   node tests/wgt_voz_shell.js
"use strict";
const fs = require("fs");
const path = require("path");
const assert = require("assert");
const shell = process.argv[2] || "tools/tizen-shell.html";
const html = fs.readFileSync(path.join(__dirname, "..", shell), "utf8");
const ini = shell === "tools/tizen3-lite-shell.html" ? html.indexOf("<script>") + 8
  : html.indexOf("  // TECLADO DA SAMSUNG PARA DIGITAR E PARA O BOTAO DE MICROFONE DAS BUSCAS");
assert(ini > 0, "shell script missing");
// Inclui TODOS os tradutores de teclas que vem depois da ponte.
const codigo = html.slice(ini, html.indexOf("</script>", ini));

function montar(capacidade, foco = "normal") {
  const logs = [], recebidas = [];
  let janela, document;
  class Evento {
    constructor(type, args = {}) { this.type = type; Object.assign(this, args); }
    preventDefault() { this.defaultPrevented = true; }
    stopPropagation() { this.parou = true; }
    stopImmediatePropagation() { this.parou = this.imediato = true; }
  }
  class Alvo {
    constructor(id) { this.id = id; this.value = ""; this.hidden = true; this.ouv = {}; }
    addEventListener(t, f, captura = false) { (this.ouv[t] ||= []).push({ f, captura }); }
    emitir(ev, captura) {
      for (const o of this.ouv[ev.type] || []) {
        if (o.captura === captura) o.f(ev);
        if (ev.imediato) break;
      }
    }
    dispatchEvent(ev) {
      if (this !== janela) janela.emitir(ev, true);
      if (!ev.parou) { this.emitir(ev, true); if (!ev.imediato) this.emitir(ev, false); }
      if (this !== janela && ev.bubbles && !ev.parou) janela.emitir(ev, false);
      return !ev.defaultPrevented;
    }
    focus() {
      if (this.id === "nvTextoCampo" && foco === "recusa") return;
      if (this.id === "nvTextoCampo" && foco === "lanca") throw new Error("focus recusado");
      if (document.activeElement === this) return;
      if (document.activeElement) document.activeElement.blur();
      document.activeElement = this;
    }
    blur() {
      if (document.activeElement !== this) return;
      document.activeElement = null;
      this.dispatchEvent(new Evento("blur"));
    }
    setSelectionRange() {}
  }
  janela = new Alvo("window");
  const el = new Alvo("nvTextoCampo"), canvas = new Alvo("canvas"), diag = new Alvo("diag");
  let temCampo = true, pintou = 0;
  document = { activeElement: null,
    getElementById: id => id === "nvTextoCampo" ? (temCampo ? el : null)
      : id === "canvas" ? canvas : id === "diag" ? diag : null };
  const tizen = capacidade === undefined ? undefined
    : capacidade === "sem-api" ? {}
    : { systeminfo: { getCapability(k) {
      assert.strictEqual(k, "http://tizen.org/feature/microphone");
      if (capacidade === "lanca") throw new Error("NotSupported");
      return capacidade;
    } } };
  janela.tizen = tizen;
  janela.__nvPrimeiroQuadro = 1;
  janela.nvPlayerAberto = 1;
  janela.__nvPintar = () => pintou++;
  const console = { log: s => logs.push(s), warn: s => logs.push(s) };
  new Function("window", "document", "console", "tizen", "KeyboardEvent", "setInterval", "navigator", codigo)(
    janela, document, console, tizen, Evento, () => 0, { userAgent: "desktop" });
  janela.dispatchEvent(new Evento("load"));
  for (const tipo of ["keydown", "keyup"]) canvas.addEventListener(tipo, ev => recebidas.push([tipo, ev.keyCode]));
  const tecla = (alvo, keyCode, tipo = "keydown", key) => {
    const ev = new Evento(tipo, { keyCode, key, bubbles: true, isTrusted: true });
    (alvo === "janela" ? janela : alvo === "canvas" ? canvas : el).dispatchEvent(ev);
    return ev;
  };
  const digita = (v, tipo = "input") => { el.value = v; el.dispatchEvent(new Evento(tipo)); };
  return { t: janela.nvTexto, el, canvas, diag, janela, document, tecla, digita, logs, recebidas,
    tirarCampo() { temCampo = false; }, pintou: () => pintou };
}

let falhas = 0;
function teste(nome, f) {
  try { f(); console.log("PASS: " + nome); }
  catch (e) { falhas++; console.error("FAIL: " + nome + ": " + e.message); }
}

teste("Samsung Return and media reach SDL", () => {
  const m = montar(true);
  for (const [key, translated] of [[10009, 27], [10252, 19], [415, 19],
    [19, 19], [413, 27], [412, 37], [417, 39]]) {
    m.recebidas.length = 0;
    m.tecla("canvas", key);
    if (key === 10009) m.tecla("canvas", key, "keyup");
    assert.deepStrictEqual(m.recebidas, [["keydown", translated], ["keyup", translated]]);
  }
});

teste("sem Tizen (PC/VIDAA) preserva a ponte de texto da integracao", () => {
  const m = montar(undefined);
  assert.strictEqual(m.t.microfone, -1);
  m.t.abrir("matrix", 0);
  assert.strictEqual(m.document.activeElement, m.el);
  m.digita("duna");
  m.tecla("campo", 65376);
  assert.strictEqual(m.t.fim, 1);
  assert.strictEqual(m.t.valor, "duna");
  assert.strictEqual(m.document.activeElement, m.canvas);
});
teste("Samsung com capacidade ausente continua desconhecida", () => {
  assert.strictEqual(montar(true).t.microfone, 1);
  assert.strictEqual(montar(false).t.microfone, 0);
  for (const c of ["lanca", "sem-api"]) {
    const m = montar(c);
    assert.strictEqual(m.t.microfone, -1);
    m.t.abrir("", 1);
    assert.strictEqual(m.document.activeElement, m.el);
  }
});
for (const foco of ["ausente", "recusa", "lanca"]) teste("falha de campo/foco cancela: " + foco, () => {
  const m = montar(true, foco);
  if (foco === "ausente") m.tirarCampo();
  m.t.abrir("", 1);
  assert.strictEqual(m.t.fim, 2, "C must receive a startup failure");
  assert.strictEqual(m.t.aberto, false);
  assert.strictEqual(m.document.activeElement, m.canvas);
  m.t.fechar();
  assert.strictEqual(m.t.fim, 2);
});

// Cada tradutor de captura, incluindo keyup de CH+ e aliases do desktop.
const atalhos = [
  [403], [404], [427], [427, "keyup"], [428], [428, "keyup"],
  [112, "keydown", "F1"], [113, "keydown", "F2"],
  [114, "keydown", "F3"], [114, "keyup", "F3"],
  [10225], [405], [10252], [415], [19], [413], [412], [417]
];
for (const [k, tipo = "keydown", key] of atalhos) teste("IME isola atalho " + (key || k) + "/" + tipo, () => {
  const m = montar(true);
  m.t.abrir("matrix", 1);
  m.tecla("campo", k, tipo, key);
  assert.deepStrictEqual(m.recebidas, [], "nenhuma tecla sintetica no canvas");
  assert.strictEqual(m.diag.hidden, true, "nenhum painel de log aberto por baixo");
  assert.strictEqual(m.pintou(), 0);
  assert.strictEqual(m.t.aberto, true);
  assert.strictEqual(m.t.valor, "matrix");
  assert.strictEqual(m.document.activeElement, m.el);
});

teste("texto/composicao continuam chegando; captura nao bloqueia default", () => {
  const m = montar(true);
  m.t.abrir("", 1);
  const s = m.t.seq;
  for (const tipo of ["keydown", "keyup", "keypress"]) {
    const ev = m.tecla("campo", 65, tipo, "a");
    assert(!ev.defaultPrevented, "letra precisa continuar digitando");
  }
  const ev = m.tecla("campo", 229);
  assert(!ev.defaultPrevented, "composicao do IME precisa continuar");
  m.digita("ação é", "compositionend");
  assert.strictEqual(m.t.valor, "ação é");
  assert(m.t.seq > s);
  assert.deepStrictEqual(m.recebidas, []);
});
for (const k of [65376, 13, 65385, 27, 10009]) teste("Done/Cancel/Return continuam locais: " + k, () => {
  const m = montar(true);
  m.t.abrir("", 1); m.digita("duna");
  m.tecla("campo", k);
  assert.strictEqual(m.t.fim, k === 65376 || k === 13 ? 1 : 0);
  assert.strictEqual(m.t.aberto, false);
  assert.strictEqual(m.document.activeElement, m.canvas);
  assert.deepStrictEqual(m.recebidas, []);
});
teste("blur entrega texto; vazio cancela; fechar pelo app nao duplica fim", () => {
  const m = montar(true);
  m.t.abrir("", 1); m.digita("duna"); m.el.blur();
  assert.strictEqual(m.t.fim, 1);
  assert.strictEqual(m.t.valor, "duna");
  m.t.abrir("", 1); m.el.blur();
  assert.strictEqual(m.t.fim, 0);
  m.t.abrir("", 1); m.digita("x"); m.t.fechar();
  assert.strictEqual(m.t.fim, -1);
  assert.strictEqual(m.t.aberto, false);
  assert.strictEqual(m.document.activeElement, m.canvas);
});

teste("atalhos, log e aliases intactos fora do IME (tambem sem Tizen)", () => {
  for (const c of [true, undefined]) {
    const m = montar(c);
    function par(k, traduzida, key) {
      m.recebidas.length = 0;
      m.tecla("canvas", k, "keydown", key);
      if (k === 10009) m.tecla("canvas", k, "keyup", key);
      assert.deepStrictEqual(m.recebidas, [["keydown", traduzida], ["keyup", traduzida]]);
    }
    par(10009, 27); par(403, 120); par(112, 120, "F1");
    for (const [k, saida] of [[10225, 117], [405, 116], [10252, 19], [415, 19],
      [19, 19], [413, 27], [412, 37], [417, 39]]) par(k, saida);
    for (const [k, key] of [[427], [114, "F3"]]) {
      m.recebidas.length = 0;
      m.tecla("canvas", k, "keydown", key); m.tecla("canvas", k, "keyup", key);
      assert.deepStrictEqual(m.recebidas, [["keydown", 118], ["keyup", 118]]);
    }
    // CH- (428) vira F8 (119), o par do F7: o C o faz de zap ou de Spotlight.
    for (const tipo of ["keydown", "keyup"]) {
      m.recebidas.length = 0;
      m.tecla("canvas", 428, tipo);
      assert.deepStrictEqual(m.recebidas, [[tipo, 119]]);
    }
    m.tecla("canvas", 404);
    assert.strictEqual(m.diag.hidden, false); assert.strictEqual(m.pintou(), 1);
    m.tecla("canvas", 113, "keydown", "F2"); assert.strictEqual(m.diag.hidden, true);
    m.janela.__nvPrimeiroQuadro = 0;
    m.tecla("canvas", 403); assert.strictEqual(m.diag.hidden, false);
    m.janela.nvPlayerAberto = 0; m.recebidas.length = 0;
    m.tecla("canvas", 10252); assert.deepStrictEqual(m.recebidas, []);
    const antes = m.logs.length;
    m.tecla("canvas", 9999); m.tecla("canvas", 9999);
    assert.strictEqual(m.logs.length, antes + 1, "tecla desconhecida logada uma vez");
  }
});
process.exitCode = falhas ? 1 : 0;
