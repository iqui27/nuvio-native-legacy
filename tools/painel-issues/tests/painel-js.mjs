// Confere o polling do index.html sem navegador: DOM e fetch falsos, script real.
// Cobre: /api/respostas a cada ciclo mesmo com data.json igual, saida do modo somente
// leitura quando a API volta, rascunho nao enviado com precedencia sobre a resposta
// salva, e restauracao de scroll/foco abortada quando o dono mexe durante a espera.
// Uso: node tests/painel-js.mjs   (chamado por tests/api-respostas.sh)
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import vm from "node:vm";

const AQUI = dirname(fileURLToPath(import.meta.url));
const fonte = readFileSync(join(AQUI, "..", "index.html"), "utf8").match(/<script>([\s\S]*)<\/script>/);
if (!fonte) { console.log("FAIL nao achei o <script> do index.html"); process.exit(1); }

let FALHAS = 0;
const ok = (m) => console.log("PASS " + m);
const bad = (m) => { console.log("FAIL " + m); FALHAS++; };
const espera = (nome, esperado, obtido) =>
  (esperado === obtido ? ok : bad)(nome + " (esperado " + JSON.stringify(esperado) + ", veio " + JSON.stringify(obtido) + ")");

// ---- DOM falso -------------------------------------------------------------
const POR_ID = {};
function no(tag) {
  const n = {
    tagName: (tag || "div").toUpperCase(), children: [], attrs: {}, ouvintes: {},
    style: {}, value: "", className: "", hidden: false, id: "", parentNode: null, scrollTop: 0, _t: null,
    get textContent() { return this._t != null ? this._t : this.children.map((c) => c.textContent).join(""); },
    set textContent(v) { this._t = String(v); this.children = []; },
    appendChild(c) { this.children.push(c); c.parentNode = this; return c; },
    removeChild(c) { this.children = this.children.filter((x) => x !== c); return c; },
    replaceChild(novo, velho) {
      const i = this.children.indexOf(velho);
      if (i < 0) throw new Error("replaceChild: o filho nao esta na arvore");
      this.children[i] = novo; novo.parentNode = this; return velho;
    },
    setAttribute(k, v) { this.attrs[k] = v; if (k === "id") this.id = v; if (k === "value") this.value = v; },
    getAttribute(k) { return k in this.attrs ? this.attrs[k] : null; },
    addEventListener(ev, fn) { (this.ouvintes[ev] = this.ouvintes[ev] || []).push(fn); },
    disparar(ev) { (this.ouvintes[ev] || []).forEach((fn) => fn({ preventDefault() {}, stopPropagation() {}, target: this })); },
    focus() { DOC.activeElement = this; },
    setSelectionRange(a, b) { this.selectionStart = a; this.selectionEnd = b; },
    scrollIntoView() {},
  };
  return n;
}
const texto = (t) => { const n = no("#text"); n._t = String(t); return n; };

// so os nos ainda presos a arvore contam (cada redesenho joga fora os antigos)
const vivos = () => {
  const out = [], visita = (n) => { out.push(n); n.children.forEach(visita); };
  Object.values(POR_ID).forEach(visita);
  return out;
};
const dentroDe = (raiz, n) => { for (let x = n; x; x = x.parentNode) if (x === raiz) return true; return false; };
const areaDe = (id) => vivos().filter((n) => n.tagName === "TEXTAREA" && n.getAttribute("data-dec") === id).pop();
function cartaoDe(id) { let n = areaDe(id); while (n && !/(^|\s)dec(\s|$)/.test(n.className)) n = n.parentNode; return n; }
const botaoDe = (id, classe) => {
  const c = cartaoDe(id);
  if (!c) return null;
  return vivos().filter((n) => n.tagName === "BUTTON" && dentroDe(c, n) && new RegExp("(^|\\s)" + classe + "(\\s|$)").test(n.className)).pop();
};
const temAviso = () => vivos().filter((n) => n.className === "aviso").length > 0;

const WIN = {
  pageYOffset: 0, rolou: [], ouvintes: {},
  scrollTo(x, y) { this.rolou.push(y); },
  addEventListener(ev, fn) { (this.ouvintes[ev] = this.ouvintes[ev] || []).push(fn); },
  removeEventListener(ev, fn) { this.ouvintes[ev] = (this.ouvintes[ev] || []).filter((f) => f !== fn); },
};
const DOC = {
  documentElement: { scrollTop: 0 }, activeElement: null,
  createElement: no, createTextNode: texto, createDocumentFragment: () => no("#fragmento"),
  getElementById: (id) => (POR_ID[id] = POR_ID[id] || no("div")),
  querySelectorAll: (sel) => (sel === ".scroll" ? vivos().filter((n) => /(^|\s)scroll(\s|$)/.test(n.className)) : []),
  querySelector: (sel) => { const m = /\[data-dec="([^"]*)"\]/.exec(sel); return m ? areaDe(m[1]) : null; },
  addEventListener() {}, removeEventListener() {},
};
// o dono mexeu: dispara o que o painel escuta em preserva()
const mexer = () => (WIN.ouvintes.scroll || []).forEach((f) => f({}));
const ouvintesVivos = () => Object.keys(WIN.ouvintes).filter((k) => WIN.ouvintes[k].length);

// ---- fetch falso -----------------------------------------------------------
let MAPA = null, RESPOSTAS = {}, API_UP = true;
const CHAMADAS = [];
function fetch(u) {
  const url = String(u);
  CHAMADAS.push(url);
  if (url === "data.json") return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(MAPA) });
  if (url === "api/respostas") {
    if (!API_UP) return Promise.resolve({ ok: false, status: 503, json: () => Promise.resolve({ erro: "fora do ar" }) });
    return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ versao: 1, respostas: RESPOSTAS }) });
  }
  return Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve({}) });
}

const ctx = vm.createContext({
  document: DOC, window: WIN, fetch, console, setInterval: () => 0, clearInterval: () => {},
});

const resp = (id, resposta, nota) => ({ historico: [{ resposta, nota, quando: "2026-10-09T12:00:00-03:00" }] });
const mapa = (itens) => ({
  meta: { repo: "", commits: {}, branches: [] },
  itens: itens || [{ numero: 1, titulo: "issue de teste", estado_github: "aberta", plataforma: "todas", status: "aberta" }],
  decisoes: [{ id: "dec-um", pergunta: "Aceita?" }, { id: "dec-dois", pergunta: "Outra?" }],
  roadmap: [], validacoes: [], fechar_203: [], nao_fechar_ainda: [], fora_do_github: [],
});
const tick = (ms) => new Promise((r) => setTimeout(r, ms));
const ate = async (pred, ms = 3000) => {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) { if (pred()) return true; await tick(5); }
  return !!pred();
};

(async function () {
  MAPA = mapa();
  RESPOSTAS = { "dec-um": resp("dec-um", "sim", "nota salva") };
  vm.runInContext(fonte[1], ctx, { filename: "index.html" });
  await ate(() => ctx.API_OK === true && !!areaDe("dec-um"));
  espera("bootstrap: API no ar", true, ctx.API_OK);
  espera("bootstrap: cartao com textarea", true, !!areaDe("dec-um"));

  // 1) o polling consulta /api/respostas mesmo com data.json igual
  CHAMADAS.length = 0;
  await ctx.recarrega();
  espera("recarrega seek data.json", 1, CHAMADAS.filter((u) => u === "data.json").length);
  espera("recarrega seek api/respostas com o mapa igual", 1, CHAMADAS.filter((u) => u === "api/respostas").length);

  // 1b) API fora do ar: somente leitura; quando volta, o painel volta a responder
  API_UP = false;
  await ctx.recarrega();
  espera("API fora: somente leitura", false, ctx.API_OK);
  espera("API fora: aviso na tela", true, temAviso());
  API_UP = true;
  await ctx.recarrega();
  espera("API voltou: sai do somente leitura", true, ctx.API_OK);
  espera("API voltou: aviso some", false, temAviso());
  espera("API voltou: botoes voltaram", true, !!botaoDe("dec-um", "sim"));

  // 2) rascunho nao enviado tem precedencia sobre a resposta salva
  botaoDe("dec-um", "nao").disparar("click");        // o dono troca para Negar
  const ta = areaDe("dec-um");
  ta.value = "rascunho do dono";
  ta.disparar("input");
  MAPA = mapa([...mapa().itens, { numero: 2, titulo: "nova", estado_github: "aberta", plataforma: "todas", status: "aberta" }]);
  await ctx.recarrega();                              // redesenho com o mapa diferente
  espera("rascunho sobrevive ao redesenho", "rascunho do dono", areaDe("dec-um").value);
  espera("escolha do rascunho sobrevive (Negar marcado)", true,
    /(^|\s)on(\s|$)/.test(botaoDe("dec-um", "nao").className));

  // 5) restauracao de scroll/foco so quando o dono nao mexeu durante a espera
  WIN.pageYOffset = 700; WIN.rolou = [];
  await ctx.preserva(() => Promise.resolve());
  espera("sem interferencia: scroll restaurado", 700, WIN.rolou[WIN.rolou.length - 1]);
  WIN.rolou = [];
  await ctx.preserva(() => new Promise((r) => setTimeout(() => { mexer(); r(); }, 5)));
  espera("dono mexeu: nao restaura scroll", 0, WIN.rolou.length);
  espera("dono mexeu: ouvintes soltos", 0, ouvintesVivos().length);

  console.log(FALHAS ? FALHAS + " falha(s)" : "TUDO OK");
  process.exit(FALHAS ? 1 : 0);
})();