// Codigo de 6 digitos do login da TV (src/parear.js). Node 22+:
//   node --test servidor/recomendacoes/teste-parear.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { enderecoAceito, codigoDe, PAREAR_VALIDADE } from "./src/parear.js";

const BASE = "https://conta.exemplo/tv-login";
// O login da TV manda ?code= com 32 hexadecimais (src/qr.h).
const hex = (i) => i.toString(16).padStart(32, "0");
const U = (i = 0) => BASE + "?code=" + hex(i);

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
    const run = () => {
      const r = st.run(...v);
      return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } };
    };
    return {
      first: async () => st.get(...v) ?? null,
      all: async () => ({ results: st.all(...v) }),
      run,
    };
  };
  const env = { PAREAR_BASES: BASE, ...extra.env,
    DB: {
      prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }),
      batch: async (stmts) => {
        sqlite.exec("BEGIN IMMEDIATE");
        try {
          const results = stmts.map((st) => st.run());
          sqlite.exec("COMMIT");
          return results;
        } catch (err) {
          sqlite.exec("ROLLBACK");
          throw err;
        }
      },
    } };
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

test("enderecoAceito: origem e caminho exatos, query canonica, sem prefixo", () => {
  const b = [BASE];
  const c = hex(0xabc);
  assert.ok(enderecoAceito(BASE + "?code=" + c, b));
  assert.ok(enderecoAceito(BASE + "?code=" + c.toUpperCase(), b));
  // Caminho: so a rota exata. Subcaminho, sufixo e barra a mais ficam fora.
  assert.ok(!enderecoAceito(BASE + "/x?code=" + c, b));
  assert.ok(!enderecoAceito(BASE + "-golpe?code=" + c, b));
  assert.ok(!enderecoAceito(BASE + "/?code=" + c, b));
  assert.ok(!enderecoAceito("https://conta.exemplo/tv-login/../tv-login?code=" + c, b));   // nao canonico
  assert.ok(!enderecoAceito("https://conta.exemplo/%74v-login?code=" + c, b));
  // Origem inteira: host, porta e esquema.
  assert.ok(!enderecoAceito("https://conta.exemplo.golpe.net/tv-login?code=" + c, b));
  assert.ok(!enderecoAceito("https://conta.exemplo:8443/tv-login?code=" + c, b));
  assert.ok(!enderecoAceito("https://conta.exemplo.golpe.net/?code=" + c, ["https://conta.exemplo"]));
  assert.ok(!enderecoAceito("http://conta.exemplo/tv-login?code=" + c, ["http://conta.exemplo/tv-login"]));
  assert.ok(enderecoAceito("http://localhost:8080/tv?code=" + c, ["http://localhost:8080/tv"]));
  assert.ok(!enderecoAceito("http://localhost:8081/tv?code=" + c, ["http://localhost:8080/tv"]));
  // Query: so code=<32 hex>, uma vez, sem # e sem nada a mais.
  assert.ok(!enderecoAceito(BASE, b));
  assert.ok(!enderecoAceito(BASE + "?code=abc", b));
  assert.ok(!enderecoAceito(BASE + "?code=" + c + "&next=https://golpe.net", b));
  assert.ok(!enderecoAceito(BASE + "?code=" + c + "&code=" + c, b));
  assert.ok(!enderecoAceito(BASE + "?code=" + c + "#x", b));
  assert.ok(!enderecoAceito(BASE + "?code=" + c + "#", b));   // fragmento vazio tambem
  assert.ok(!enderecoAceito(BASE + "?Code=" + c, b));
  assert.ok(!enderecoAceito(BASE + "?code=" + "g".repeat(32), b));
  // Base suja nao libera nada; credencial e esquema estranho tambem nao.
  assert.ok(!enderecoAceito(BASE + "?code=" + c, [BASE + "?code="]));
  assert.ok(!enderecoAceito("https://u:p@conta.exemplo/tv-login?code=" + c, ["https://u:p@conta.exemplo/tv-login"]));
  assert.ok(!enderecoAceito("javascript:alert(1)", ["javascript:"]));
  assert.ok(!enderecoAceito(BASE + "?code=" + "a".repeat(400), b));
  assert.ok(!enderecoAceito(BASE + "?a=<script>", b));
  assert.ok(!enderecoAceito(BASE + "?code=" + c, []));
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
  // Nada disto pode ir para cache.
  const a = await pedir("/v1/parear", { url: u });
  assert.equal(a.status, 200);
  assert.match(a.body.codigo, /^\d{6}$/);
  assert.equal(a.body.validade, PAREAR_VALIDADE);
  assert.equal(a.body.pagina, "https://w.test/c");
  assert.equal(a.res.headers.get("cache-control"), "no-store");
  // A TV que reabre o login com o mesmo endereco recebe o MESMO codigo.
  const d = await pedir("/v1/parear", { url: u });
  assert.equal(d.body.codigo, a.body.codigo);
  // Quem digita com espaco ou traco chega igual.
  const b = await pedir("/v1/parear/abrir", { codigo: a.body.codigo.slice(0, 3) + " " + a.body.codigo.slice(3) }, "2.2.2.2");
  assert.deepEqual([b.status, b.body.url], [200, u]);
  assert.equal(b.res.headers.get("cache-control"), "no-store");
  // USO UNICO: a segunda tentativa com o mesmo codigo e recusada.
  const c = await pedir("/v1/parear/abrir", { codigo: a.body.codigo }, "3.3.3.3");
  assert.equal(c.status, 404);
  assert.equal(c.res.headers.get("cache-control"), "no-store");
});

test("Novo codigo: a TV pede outro para o MESMO endereco e recebe na hora; o antigo morre", async (t) => {
  const { pedir, sqlite } = cenario(t);
  const u = U(0x5eed);
  const a = await pedir("/v1/parear", { url: u });
  assert.equal(a.status, 200);
  // Sem "novo" o mesmo endereco devolve o mesmo codigo (a TV que so reabriu a tela).
  assert.equal((await pedir("/v1/parear", { url: u })).body.codigo, a.body.codigo);
  // Com "novo": codigo valido, diferente, validade cheia, na mesma resposta.
  const b = await pedir("/v1/parear", { url: u, novo: 1 });
  assert.equal(b.status, 200);
  assert.match(b.body.codigo, /^\d{6}$/);
  assert.notEqual(b.body.codigo, a.body.codigo);
  assert.equal(b.body.validade, PAREAR_VALIDADE);
  assert.equal(b.res.headers.get("cache-control"), "no-store");
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM parear WHERE url = ?").get(u).n, 1);
  // O antigo nao abre mais; o novo abre o mesmo endereco, uma vez.
  assert.equal((await pedir("/v1/parear/abrir", { codigo: a.body.codigo }, "6.6.6.1")).status, 404);
  const c = await pedir("/v1/parear/abrir", { codigo: b.body.codigo }, "6.6.6.2");
  assert.deepEqual([c.status, c.body.url], [200, u]);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: b.body.codigo }, "6.6.6.3")).status, 404);
  // Renovar seguidas vezes: sempre um codigo que abre, nunca repete o anterior.
  let ant = null;
  for (let i = 0; i < 5; i++) {
    const r = await pedir("/v1/parear", { url: u, novo: true }, "6.6.6.9");
    assert.equal(r.status, 200);
    assert.notEqual(r.body.codigo, ant);
    ant = r.body.codigo;
  }
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM parear WHERE url = ?").get(u).n, 1);
});

test("Novo codigo gasta a mesma cota de 20 por hora", async (t) => {
  const { pedir } = cenario(t);
  for (let i = 0; i < 20; i++) assert.equal((await pedir("/v1/parear", { url: U(3), novo: 1 }, "7.7.7.8")).status, 200);
  assert.equal((await pedir("/v1/parear", { url: U(3), novo: 1 }, "7.7.7.8")).status, 429);
});

test("oito colisoes na renovacao preservam o codigo anterior", async (t) => {
  const { pedir, sqlite } = cenario(t);
  const inserir = sqlite.prepare("INSERT INTO parear VALUES (?, ?, ?)");
  const expira = Math.floor(Date.now() / 1000) + PAREAR_VALIDADE;
  inserir.run("111111", U(1), expira);
  inserir.run("222222", U(2), expira);
  const sorteio = t.mock.method(crypto, "getRandomValues", (dados) => {
    dados[0] = 222222;
    return dados;
  });

  assert.equal((await pedir("/v1/parear", { url: U(1), novo: 1 })).status, 503);
  assert.equal(sorteio.mock.callCount(), 8);
  const antigo = sqlite.prepare("SELECT codigo, expira FROM parear WHERE url = ?").get(U(1));
  assert.deepEqual({ ...antigo }, { codigo: "111111", expira });
});

test("renovacoes concorrentes deixam um unico codigo ativo por URL", async (t) => {
  const { pedir, sqlite } = cenario(t);
  const url = U(1);
  let codigo = 111110;
  t.mock.method(crypto, "getRandomValues", (dados) => {
    dados[0] = ++codigo;
    return dados;
  });
  assert.equal((await pedir("/v1/parear", { url })).body.codigo, "111111");

  const respostas = await Promise.all([1, 2].map((i) =>
    pedir("/v1/parear", { url, novo: 1 }, `4.4.4.${i}`)));
  assert.deepEqual(respostas.map((r) => r.status), [200, 200]);
  const ativos = sqlite.prepare("SELECT codigo FROM parear WHERE url = ? AND expira > ?")
    .all(url, Math.floor(Date.now() / 1000));
  assert.equal(ativos.length, 1);
  assert.ok(respostas.some((r) => r.body.codigo === ativos[0].codigo));
});

test("criacao concorrente reutiliza o codigo emitido para a mesma URL", async (t) => {
  const { pedir, sqlite } = cenario(t);
  const url = U(1);
  let codigo = 111110;
  t.mock.method(crypto, "getRandomValues", (dados) => {
    dados[0] = ++codigo;
    return dados;
  });
  const respostas = await Promise.all([1, 2].map((i) => pedir("/v1/parear", { url }, `4.4.4.${i}`)));
  assert.deepEqual(respostas.map((r) => r.status), [200, 200]);
  assert.equal(respostas[0].body.codigo, respostas[1].body.codigo);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM parear WHERE url = ?").get(url).n, 1);
});

test("a renovacao exclui o codigo anterior ja consumido por /abrir", async (t) => {
  const { pedir, sqlite } = cenario(t);
  const url = U(1);
  sqlite.prepare("INSERT INTO parear VALUES (?, ?, ?)")
    .run("000042", url, Math.floor(Date.now() / 1000) + PAREAR_VALIDADE);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "000042" })).status, 200);
  const fila = [42, 43];
  t.mock.method(crypto, "getRandomValues", (dados) => {
    dados[0] = fila.shift();
    return dados;
  });

  const renovado = await pedir("/v1/parear", { url, novo: 1, anterior: "000042" });
  assert.equal(renovado.status, 200);
  assert.equal(renovado.body.codigo, "000043");
});

test("uso unico mesmo com dois pedidos ao mesmo tempo", async (t) => {
  const { pedir, sqlite } = cenario(t);
  const a = await pedir("/v1/parear", { url: U(7) });
  const r = await Promise.all([1, 2, 3, 4].map((i) => pedir("/v1/parear/abrir", { codigo: a.body.codigo }, "4.4.4." + i)));
  assert.deepEqual(r.map((x) => x.status).sort(), [200, 404, 404, 404]);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM parear").get().n, 0);
});

test("corpo acima do teto: 413 antes de ler tudo e sem gastar cota", async (t) => {
  const { env } = cenario(t);
  const grande = async (rota, n, chunked) => {
    const corpo = JSON.stringify({ url: U(1), lixo: "x".repeat(n) });
    const body = chunked
      ? new ReadableStream({ start(c) { c.enqueue(new TextEncoder().encode(corpo)); c.close(); } })
      : corpo;
    const res = await worker.fetch(new Request("https://w.test" + rota, {
      method: "POST", headers: { "cf-connecting-ip": "5.5.5.5" }, body, duplex: "half" }), env);
    return res.status;
  };
  assert.equal(await grande("/v1/parear", 5000, false), 413);
  assert.equal(await grande("/v1/parear", 5000, true), 413);      // sem Content-Length
  assert.equal(await grande("/v1/parear/abrir", 500, false), 413);
  assert.equal(await grande("/v1/parear/abrir", 500, true), 413);
  // Nenhum dos 413 contou na cota: as 10 tentativas de abrir continuam la.
  for (let i = 0; i < 10; i++) {
    const res = await worker.fetch(new Request("https://w.test/v1/parear/abrir", {
      method: "POST", headers: { "cf-connecting-ip": "5.5.5.5" }, body: JSON.stringify({ codigo: "111111" }) }), env);
    assert.equal(res.status, 404);
  }
});

test("endereco fora do prefixo, codigo errado e codigo vencido", async (t) => {
  const { pedir, sqlite } = cenario(t);
  assert.equal((await pedir("/v1/parear", { url: "https://golpe.net/x" })).status, 400);
  assert.equal((await pedir("/v1/parear", {})).status, 400);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "12345" })).status, 400);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "000000" })).status, 404);
  const a = await pedir("/v1/parear", { url: U(0xaa) });
  sqlite.exec("UPDATE parear SET expira = 1");
  assert.equal((await pedir("/v1/parear/abrir", { codigo: a.body.codigo })).status, 404);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM parear").get().n, 1);
});

test("freio: 10 tentativas de abrir por IP em 10 minutos; 20 codigos por hora", async (t) => {
  const { pedir } = cenario(t);
  for (let i = 0; i < 10; i++) assert.equal((await pedir("/v1/parear/abrir", { codigo: "111111" }, "9.9.9.9")).status, 404);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "111111" }, "9.9.9.9")).status, 429);
  assert.equal((await pedir("/v1/parear/abrir", { codigo: "111111" }, "8.8.8.8")).status, 404);
  for (let i = 0; i < 20; i++) assert.equal((await pedir("/v1/parear", { url: U(i) }, "7.7.7.7")).status, 200);
  assert.equal((await pedir("/v1/parear", { url: U(99) }, "7.7.7.7")).status, 429);
});

test("desligado sem PAREAR_BASES e sem a migracao: 503, nunca 500", async (t) => {
  const semVar = cenario(t, { env: { PAREAR_BASES: "" } });
  assert.equal((await semVar.pedir("/v1/parear", { url: U() })).status, 503);
  assert.equal((await semVar.pedir("/v1/parear/abrir", { codigo: "123456" })).status, 503);
  const semTabela = cenario(t, { semTabela: true });
  assert.equal((await semTabela.pedir("/v1/parear", { url: U(0xa) })).status, 503);
});

test("PAREAR_PAGINA troca o endereco mostrado; /c serve a pagina sem depender do banco", async (t) => {
  const { pedir } = cenario(t, { env: { PAREAR_PAGINA: "https://nuvio.exemplo/c" } });
  assert.equal((await pedir("/v1/parear", { url: U(0xa) })).body.pagina, "https://nuvio.exemplo/c");
  const p = await pedir("/c", null, "1.1.1.1", "GET");
  assert.equal(p.status, 200);
  assert.match(p.res.headers.get("content-type"), /text\/html/);
  assert.match(p.res.headers.get("content-security-policy"), /default-src 'none'/);
  assert.match(p.txt, /\/v1\/parear\/abrir/);
  assert.match(p.txt, /replace\(\/\\D\/g/);   // o escape do modelo de texto saiu certo
});
