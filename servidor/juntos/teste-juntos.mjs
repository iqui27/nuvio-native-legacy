// nuvio-juntos de verdade (workerd via `wrangler dev`, Durable Object e
// websockets reais) com pares simulados em Node e REDE RUIM: cada direcao de
// cada par tem atraso de 20-300 ms com jitter de +-80 ms (fila FIFO, como o
// TCP: atrasa, nao embaralha). Mede o que o servidor promete: depois da
// sincronia de relogio (8 pings, fica a amostra de menor RTT), todos os pares
// calculam a MESMA posicao alvo no mesmo instante real.
//
//   cd servidor/juntos && npm install && node --test teste-juntos.mjs
// Sobe o wrangler dev numa porta propria e derruba no fim. Nada remoto.
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import http from "node:http";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test, { after, before } from "node:test";
import { assinar } from "./src/bilhete.js";

const PORTA = 8700 + Math.floor(Math.random() * 90);
const BASE = `http://127.0.0.1:${PORTA}`;
const SEGREDO = "segredo-de-teste";
let dev, estado, devLog = "";
import * as fsMod from "node:fs";
const require_fs = () => fsMod;

before(async () => {
  estado = mkdtempSync(join(process.env.TMPDIR || tmpdir(), "juntos-"));
  dev = spawn(new URL("./node_modules/.bin/wrangler", import.meta.url).pathname,
    ["dev", "--port", String(PORTA), "--ip", "127.0.0.1", "--var", `SALA_SEGREDO:${SEGREDO}`,
     "--var", "SALA_MAX:3", "--var", "JUNTOS_DEPURAR:1", "--persist-to", estado],
    { cwd: new URL(".", import.meta.url).pathname, env: { ...process.env, WRANGLER_SEND_METRICS: "false" },
      stdio: ["ignore", "pipe", "pipe"] });
  let log = "";
  dev.stdout.on("data", (d) => { log += d; devLog += d; });
  dev.stderr.on("data", (d) => { log += d; devLog += d; });
  for (let i = 0; i < 120; i++) {
    try { if ((await fetch(BASE + "/saude")).ok) return; } catch {}
    await dorme(250);
  }
  throw new Error("wrangler dev nao subiu:\n" + log.slice(-2000));
});
after(() => { if (process.env.JUNTOS_LOG) require_fs().writeFileSync(process.env.JUNTOS_LOG, devLog); dev?.kill("SIGINT"); try { rmSync(estado, { recursive: true, force: true }); } catch {} });

const dorme = (ms) => new Promise((r) => setTimeout(r, ms));
const novaSala = () => [...crypto.getRandomValues(new Uint8Array(16))].map((b) => b.toString(16).padStart(2, "0")).join("");
let nn = 0;
const bilhete = (sala, pessoa, papel, extra = {}) => assinar(SEGREDO,
  { sala, pessoa, nome: pessoa, av: "", papel, exp: Math.floor(Date.now() / 1000) + 60, n: "t" + ++nn, ...extra });

// Um par: websocket + rede ruim + relogio proprio (desviado de proposito).
class Par {
  constructor(nome, { atraso = [20, 300], jitter = 80, desvio = 0 } = {}) {
    this.nome = nome; this.atraso = atraso; this.jitter = jitter;
    this.desvio = desvio;            // o relogio desta "TV" esta errado em `desvio` ms
    this.caixa = []; this.esperas = []; this.ultIda = 0; this.ultVolta = 0;
    this.amostras = []; this.offset = 0; this.sala = null; this.fechou = null;
  }
  agora() { return Date.now() + this.desvio; }
  atrasoUm() {
    const [a, b] = this.atraso;
    return Math.max(0, a + Math.random() * (b - a) + (Math.random() * 2 - 1) * this.jitter);
  }
  async abrir(qs) {
    this.ws = new WebSocket(`ws://127.0.0.1:${PORTA}/ws?${qs}`);
    this.ws.addEventListener("message", (e) => {
      const quando = Math.max(Date.now() + this.atrasoUm(), this.ultVolta);
      this.ultVolta = quando;
      setTimeout(() => this.chegou(JSON.parse(e.data)), quando - Date.now());
    });
    this.ws.addEventListener("close", (e) => { this.fechou = e.code; });
    await new Promise((ok, erro) => {
      this.ws.addEventListener("open", ok, { once: true });
      this.ws.addEventListener("error", erro, { once: true });
    });
    return this;
  }
  chegou(m) {
    if (m.t === "pong") {
      const agora = this.agora();
      const rtt = agora - m.c;
      this.amostras.push({ rtt, off: m.s - (m.c + rtt / 2) });
    }
    if (m.t === "sala") this.sala = m;
    this.caixa.push(m);
    for (const w of this.esperas.slice()) if (w.f(m)) { this.esperas.splice(this.esperas.indexOf(w), 1); w.ok(m); }
  }
  mandar(m) {
    const quando = Math.max(Date.now() + this.atrasoUm(), this.ultIda);
    this.ultIda = quando;
    const txt = JSON.stringify(m);
    setTimeout(() => { if (this.ws.readyState === 1) this.ws.send(txt); }, quando - Date.now());
  }
  esperar(f, ms = 8000) {
    const ja = this.caixa.find(f);
    if (ja) return Promise.resolve(ja);
    return new Promise((ok, erro) => {
      const w = { f, ok };
      this.esperas.push(w);
      setTimeout(() => erro(new Error(`${this.nome}: tempo esgotado esperando ${String(f).slice(0, 90)}; ` +
        `chegou: ${this.caixa.map((m) => m.t + (m.k ? ":" + m.k : "")).join(",")}`)), ms);
    });
  }
  limpar() { this.caixa = []; }
  // 8 pings espacados de 150 ms; fica o offset da amostra de menor RTT (6.2).
  async sincronizar() {
    this.amostras = [];
    for (let i = 0; i < 8; i++) { this.mandar({ t: "ping", c: this.agora() }); await dorme(150); }
    await dorme(900);
    const melhor = this.amostras.sort((a, b) => a.rtt - b.rtt)[0];
    this.offset = melhor.off;
    this.rtt = melhor.rtt;
  }
  posAlvo() {
    const r = this.sala.rel;
    const s = this.agora() + this.offset;
    return r.estado === "tocando" ? r.pos0 + (s - r.t0) / 1000 : r.pos0;
  }
  fechar() { try { this.ws.close(); } catch {} }
  esperarFechar(ms = 3000) {
    if (this.fechou !== null) return Promise.resolve(this.fechou);
    return new Promise((ok, erro) => {
      this.ws.addEventListener("close", (e) => ok(e.code), { once: true });
      setTimeout(() => erro(new Error(this.nome + ": nao fechou")), ms);
    });
  }
}

// O fetch do Node recusa o cabecalho Upgrade; o pedido cru diz o status.
function statusUpgrade(qs) {
  return new Promise((ok, erro) => {
    const r = http.get(`${BASE}/ws?${qs}`, { headers: { connection: "Upgrade", upgrade: "websocket",
      "sec-websocket-version": "13", "sec-websocket-key": "dGhlIHNhbXBsZSBub25jZQ==" } });
    r.on("response", (res) => { res.resume(); ok(res.statusCode); });
    r.on("upgrade", (res, sock) => { sock.destroy(); ok(101); });
    r.on("error", erro);
  });
}

test("bilhete forjado, vencido e sem bilhete: 401; reusado: fecha 4401", async () => {
  const sala = novaSala();
  assert.equal(await statusUpgrade("b=x.y"), 401);
  const forjado = await assinar("outro", { sala, pessoa: "h", papel: "anfitriao", exp: 9e9, n: "f" });
  assert.equal(await statusUpgrade("b=" + forjado), 401);
  const vencido = await assinar(SEGREDO, { sala, pessoa: "h", papel: "anfitriao", exp: 1, n: "v" });
  assert.equal(await statusUpgrade("b=" + vencido), 401);
  assert.equal(await statusUpgrade("s=" + sala + "&k=nada"), 401);
  const b = await bilhete(sala, "h", "anfitriao", { cfg: { ep: "tt1", titulo: "Duna" } });
  const h = await new Par("h", { atraso: [0, 0], jitter: 0 }).abrir("b=" + b);
  await h.esperar((m) => m.t === "sala");
  const de = await new Par("h2", { atraso: [0, 0], jitter: 0 }).abrir("b=" + b);
  await de.esperar((m) => m.t === "erro");
  assert.equal(await de.esperarFechar(), 4401);
  h.fechar();
});

test("tres TVs com rede ruim e relogios errados: depois do play agendado, todas na mesma posicao", async () => {
  const sala = novaSala();
  const h = await new Par("h", { desvio: 7_000 }).abrir("b=" + await bilhete(sala, "h", "anfitriao",
    { cfg: { ep: "tt1160419", titulo: "Duna", imp: { hash: "b".repeat(40), idx: 1, url: "https://debrid/x" } } }));
  await h.esperar((m) => m.t === "sala");
  const r = await new Par("r", { atraso: [150, 300], desvio: -3_600_000 }).abrir("b=" + await bilhete(sala, "r", "convidado"));
  const f = await new Par("f", { atraso: [20, 60], jitter: 10, desvio: 123 }).abrir("b=" + await bilhete(sala, "f", "convidado"));
  await Promise.all([h, r, f].map((p) => p.sincronizar()));
  // A URL que veio na impressao nunca chega a ninguem.
  assert.ok(!JSON.stringify(f.sala).includes("debrid"));
  assert.equal(f.sala.cfg.imp.hash, "b".repeat(40));
  for (const p of [h, r, f]) p.mandar({ t: "ola", plat: "mac", nivel: "A", rtt: p.rtt });
  for (const p of [h, r, f]) p.mandar({ t: "estado", st: "pronto" });
  await dorme(800);
  r.mandar({ t: "cmd", op: "comecar", seqBase: r.sala.seq });
  await Promise.all([h, r, f].map((p) => p.esperar((m) => m.t === "sala" && m.rel.estado === "tocando")));
  // O t0 tem de estar no futuro para quem tem a pior rede tambem.
  assert.ok(r.sala.rel.t0 > Date.now() - 50, "t0 do play agendado ja passou");
  await dorme(1500);
  const pos = [h, r, f].map((p) => p.posAlvo());
  const spread = (Math.max(...pos) - Math.min(...pos)) * 1000;
  // Erro do relogio <= rtt/2 da melhor amostra: com 150-300 ms de ida, ~150 ms.
  assert.ok(spread < 200, `posicoes separadas por ${spread.toFixed(0)} ms`);
  assert.ok(pos[0] > 0.3 && pos[0] < 2.5, "posicao alvo plausivel: " + pos[0]);

  // Pausa de r, com a posicao local dele: todos param no mesmo pos0.
  for (const p of [h, r, f]) p.limpar();
  r.mandar({ t: "cmd", op: "pausa", pos: r.posAlvo(), seqBase: r.sala.seq });
  const ps = await Promise.all([h, r, f].map((p) => p.esperar((m) => m.t === "sala" && m.rel.estado === "pausado")));
  assert.equal(new Set(ps.map((s) => s.rel.pos0)).size, 1);
  assert.equal(ps[0].rel.autor, "r");

  // Membro trava >3 s: sala vai a "esperando" e volta quando ele fica pronto.
  for (const p of [h, r, f]) p.limpar();
  h.mandar({ t: "cmd", op: "play", seqBase: h.sala.seq });
  await f.esperar((m) => m.t === "sala" && m.rel.estado === "tocando");
  f.mandar({ t: "estado", st: "carregando", pct: 30 });
  const av = await h.esperar((m) => m.t === "aviso" && m.k === "esperando", 9000);
  assert.equal(av.quem, "f");
  await h.esperar((m) => m.t === "sala" && m.rel.estado === "esperando");
  f.mandar({ t: "estado", st: "pronto" });
  await h.esperar((m) => m.t === "sala" && m.rel.estado === "tocando" && m.rel.op === "pronto");

  // Sala cheia (SALA_MAX=3 neste teste): a quarta TV leva 4409.
  const d = await new Par("d", { atraso: [0, 0], jitter: 0 }).abrir("b=" + await bilhete(sala, "d", "convidado"));
  await d.esperar((m) => m.t === "erro" && m.k === "cheia");

  // Queda e volta de r: aviso "ausente" e depois "voltou".
  r.fechar();
  await h.esperar((m) => m.t === "aviso" && m.k === "ausente" && m.quem === "r");
  const r2 = await new Par("r2", { atraso: [0, 0], jitter: 0 }).abrir("b=" + await bilhete(sala, "r", "convidado"));
  await h.esperar((m) => m.t === "aviso" && m.k === "voltou" && m.quem === "r");
  const snap = await r2.esperar((m) => m.t === "sala");
  assert.equal(snap.membros.find((m) => m.id === "r").alc, true, "quem volta no meio segue alcancando");

  // Celular de h: primeiro uso pelo bilhete, depois pela chave de sessao.
  // O QR leva so a sala e um codigo de 8 caracteres que a TV pede a sala.
  h.mandar({ t: "celular", chat: 1 });
  const cod = (await h.esperar((m) => m.t === "celular")).codigo;
  assert.equal(await statusUpgrade(`s=${sala}&c=ZZZZ`), 401);
  const pag = await fetch(`${BASE}/j/${sala}${cod}`);
  assert.equal(pag.status, 200);
  assert.match(pag.headers.get("content-security-policy"), /default-src 'none'/);
  assert.match(await pag.text(), /Watch Together/);
  const c = await new Par("cel", { atraso: [0, 0], jitter: 0 }).abrir(`s=${sala}&c=${cod}`);
  const ses = await c.esperar((m) => m.t === "sessao");
  c.mandar({ t: "chat", txt: "pipoca pronta" });
  const chat = await f.esperar((m) => m.t === "chat");
  assert.equal(chat.txt, "pipoca pronta");
  assert.equal(chat.de, "h");
  c.fechar();
  await dorme(200);
  const reuso = await new Par("cel-reuso", { atraso: [0, 0], jitter: 0 }).abrir(`s=${sala}&c=${cod}`);
  assert.equal(await reuso.esperarFechar(), 4401, "codigo do QR e de uso unico");
  const c2 = await new Par("cel2", { atraso: [0, 0], jitter: 0 }).abrir(`s=${sala}&k=${ses.k}`);
  const hist = await c2.esperar((m) => m.t === "historico");
  assert.equal(hist.itens[0].txt, "pipoca pronta");

  // Mensagem acima de 2 KB derruba so quem mandou (1009).
  f.ws.send(JSON.stringify({ t: "chat", txt: "x".repeat(3000) }));
  assert.equal(await f.esperarFechar(), 1009);

  // Encerrar: todos recebem fim e a sala nao renasce com bilhete de anfitriao.
  h.mandar({ t: "cmd", op: "encerrar" });
  await r2.esperar((m) => m.t === "fim" && m.motivo === "anfitriao");
  const zumbi = await new Par("z", { atraso: [0, 0], jitter: 0 }).abrir("b=" + await bilhete(sala, "h", "anfitriao", { cfg: { ep: "tt1" } }));
  await zumbi.esperar((m) => m.t === "erro" && m.k === "fim");
  for (const p of [h, r2, c2, d, zumbi]) p.fechar();
});
