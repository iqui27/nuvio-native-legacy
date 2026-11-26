// Codigo de 6 digitos do login da TV (src/parear.js). Node 22+:
//   node --test servidor/recomendacoes/teste-parear.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { enderecoAceito, codigoDe, PAREAR_VALIDADE } from "./src/parear.js";

const BASE = "https://conta.exemplo/tv-login";

function cenario(t, extra = {}) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const m of extra.semTabela ? ["schema.sql", "migracao-005-amigos.sql"] : ["schema.sql", "migracao-005-amigos.sql", "migracao-021-parear.sql"])
    for (const cmd of readFileSync(new URL(m, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (c) sqlite.exec(c);
    }
  const stmt = (sql, v) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...v) ?? null,
      run: async () => { const r = st.run(...v);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
    };
  };
  const env = { PAREAR_BASES: BASE, ...extra.env,
    DB: { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) } };
  const pedir = async (rota, corpo, ip = "1.1.1.1", metodo = "POST") => {
    const res = await worker.fetch(new Request("https://w.test" + rota, {
      method: metodo, headers: { "cf-connecting-ip": ip },
      body: metodo === "POST" ? JSON.stringify(corpo) : undefined }), env);
    const txt = await res.text();
    let body = null; try { body = JSON.parse(txt); } catch {}
    return { status: res.status, body, txt, res };
  };
  return { sqlite, env, pedir };
}

test("enderecoAceito: so https dentro do prefixo, na fronteira", () => {
  const b = [BASE];
  assert.ok(enderecoAceito(BASE + "?code=abc", b));
  assert.ok(enderecoAceito(BASE + "/x?code=abc", b));
  assert.ok(enderecoAceito(BASE, b));
  assert.ok(!enderecoAceito(BASE + "-golpe?code=abc", b));
  assert.ok(!enderecoAceito("https://conta.exemplo.golpe.net/tv-login?code=a", b));
  assert.ok(!enderecoAceito("https://conta.exemplo.golpe.net/", ["https://conta.exemplo"]));
  assert.ok(!enderecoAceito("http://conta.exemplo/tv-login?code=a", ["http://conta.exemplo/tv-login"]));
  assert.ok(enderecoAceito("http://localhost:8080/tv?code=a", ["http://localhost:8080/tv"]));
  assert.ok(!enderecoAceito("https://u:p@conta.exemplo/tv-login", ["https://u:p@conta.exemplo"]));
  assert.ok(!enderecoAceito("javascript:alert(1)", ["javascript:"]));
  assert.ok(!enderecoAceito(BASE + "?code=" + "a".repeat(400), b));
  assert.ok(!enderecoAceito(BASE + "?a=<script>", b));
  assert.ok(!enderecoAceito(BASE + "?code=abc", []));
  assert.ok(!enderecoAceito(undefined, b));
});

test("codigoDe: sempre 6 digitos, com zeros a esquerda, sem vies de modulo", () => {
  for (let i = 0; i < 200; i++) assert.match(codigoDe(), /^\d{6}$/);
  assert.equal(codigoDe(() => [42]), "000042");
  const fila = [4294967295, 4294000000, 1234567];
  assert.equal(codigoDe(() => [fila.shift()]), "234567");   // os dois primeiros sao rejeitados
});

test("TV pede o codigo, quem ajuda abre: devolve o mesmo endereco", async (t) => {
  const { pedir } = cenario(t);
  const u = BASE + "?code=0123456789abcdef0123456789abcdef";
  const a = await pedir("/v1/parear", { url: u });
  assert.equal(a.status, 200);
  assert.match(a.body.codigo, /^\d{6}$/);
  assert.equal(a.body.validade, PAREAR_VALIDADE);
  assert.equal(a.body.pagina, "https://w.test/c");
  const b = await pedir("/v1/parear/abrir", { codigo: a.body.codigo }, "2.2.2.2");
  assert.deepEqual([b.status, b.body.url], [200, u]);
  // Quem digita com espaco ou traco chega igual.
  const c = await pedir("/v1/parear/abrir", { codigo: a.body.codigo.slice(0, 3) + " " + a.body.codigo.slice(3) }, "2.2.2.2");
  assert.equal(c.body.url, u);
  // A TV que reabre o login com o mesmo endereco recebe o MESMO codigo.
  const d = await pedir("/v1/parear", { url: u });
  assert.equal(d.body.codigo, a.body.codigo);
});

test("endereco fora do prefixo, codigo errado e codigo vencido", async (t) => {
  const { pedir, sqlite } = cenario(t);
  assert.equal((await pedir("/v1/parear", { url: "https://golpe.net/x" })).status, 400);
  assert.equal((await pedir("/v1/parear", {})).status, 400);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "12345" })).status, 400);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "000000" })).status, 404);
  const a = await pedir("/v1/parear", { url: BASE + "?code=aa" });
  sqlite.exec("UPDATE parear SET expira = 1");
  assert.equal((await pedir("/v1/parear/abrir", { codigo: a.body.codigo })).status, 404);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM parear").get().n, 1);
});

test("freio: 10 tentativas de abrir por IP em 10 minutos; 20 codigos por hora", async (t) => {
  const { pedir } = cenario(t);
  for (let i = 0; i < 10; i++) assert.equal((await pedir("/v1/parear/abrir", { codigo: "111111" }, "9.9.9.9")).status, 404);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "111111" }, "9.9.9.9")).status, 429);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "111111" }, "8.8.8.8")).status, 404);
  for (let i = 0; i < 20; i++) assert.equal((await pedir("/v1/parear", { url: BASE + "?code=" + i }, "7.7.7.7")).status, 200);
  assert.equal((await pedir("/v1/parear", { url: BASE + "?code=x" }, "7.7.7.7")).status, 429);
});

test("desligado sem PAREAR_BASES e sem a migracao: 503, nunca 500", async (t) => {
  const semVar = cenario(t, { env: { PAREAR_BASES: "" } });
  assert.equal((await semVar.pedir("/v1/parear", { url: BASE })).status, 503);
  assert.equal((await semVar.pedir("/v1/parear/abrir", { codigo: "123456" })).status, 503);
  const semTabela = cenario(t, { semTabela: true });
  assert.equal((await semTabela.pedir("/v1/parear", { url: BASE + "?code=a" })).status, 503);
});

test("PAREAR_PAGINA troca o endereco mostrado; /c serve a pagina sem depender do banco", async (t) => {
  const { pedir } = cenario(t, { env: { PAREAR_PAGINA: "https://nuvio.exemplo/c" } });
  assert.equal((await pedir("/v1/parear", { url: BASE + "?code=a" })).body.pagina, "https://nuvio.exemplo/c");
  const p = await pedir("/c", null, "1.1.1.1", "GET");
  assert.equal(p.status, 200);
  assert.match(p.res.headers.get("content-type"), /text\/html/);
  assert.match(p.res.headers.get("content-security-policy"), /default-src 'none'/);
  assert.match(p.txt, /\/v1\/parear\/abrir/);
  assert.match(p.txt, /replace\(\/\\D\/g/);   // o escape do modelo de texto saiu certo
});
