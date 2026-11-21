// Farol: POST /v1/farol/registro — validacao por lista branca, codigo de 6
// caracteres com ids >= 2^29, dedupe por (pessoa_h, fp), tetos e retencao de
// 30 dias. Worker inteiro com D1 falso em memoria (padrao teste-loglink.mjs);
// a sessao e semeada, sem rede. Node 22+:
//   node --test servidor/recomendacoes/teste-farol.mjs
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { limparRegistro } from "./src/index.js";
import { limparFarol } from "./src/farol.js";

const MIGRACOES = ["schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql",
  "migracao-018-farol.sql"];

const agora = () => Math.floor(Date.now() / 1000);

function banco(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const m of MIGRACOES) {
    for (const cmd of readFileSync(new URL(m, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (!c) continue;
      try { sqlite.exec(c); } catch (e) { if (!/duplicate column/.test(String(e.message))) throw e; }
    }
  }
  const stmt = (sql, valores) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...valores) ?? null,
      all: async () => ({ results: st.all(...valores) }),
      run: async () => { const r = st.run(...valores);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
      _exec: () => st.run(...valores),
    };
  };
  const DB = {
    prepare(sql) {
      const base = stmt(sql, []);
      return { ...base, bind: (...v) => stmt(sql, v) };
    },
    async batch(lista) {
      sqlite.exec("BEGIN");
      try { for (const s of lista) s._exec(); sqlite.exec("COMMIT"); }
      catch (e) { sqlite.exec("ROLLBACK"); throw e; }
      return [];
    },
  };
  sqlite.prepare("INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update("nuvio:tok-farol").digest("hex"), "nuvio:pessoa-farol", "");
  return { sqlite, DB };
}

// O pacote do exemplo da secao 2.6 do PLANO, encurtado.
function pacote(extra = {}) {
  return {
    v: 1,
    fingerprint: "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3_51",
    codigo: "ums_204",
    aparelho: {
      plataforma: "webos", so: "4.9", so_maior: 4, modelo: "OLED55C9PSA",
      ram_total_mb: 1870, ram_livre_mb: 212,
      tela: { w: 3840, h: 2160, hdr: 1, dv: 1, hevc: 1, av1: -1 },
      audio: { dts: 1, truehd: -1, eac3: 1, ac3: 1 },
      rede: { offline: false, hosts_com_falha: 0, vazao_mbps: 0 },
    },
    cliente: {
      versao: "2.1.0", commit: "c18c21756", build: "webos-ipk", backend: "starfish",
      idioma: "pt-BR", aberto_s: 1842, gpu: "auto:alto",
      ajustes: { glass_ui: 1, qualidade_max: "4k", tamanho_max_gb: 40, dts: "ac3", dv_mkv: 0, legenda_auto_sync: 1 },
      addons: { total: 12, ativos: 11, isolados: 1, tipos: ["catalogo", "fontes", "legendas"] },
      debrid: ["torbox"], contas: ["trakt", "simkl"],
    },
    midia: { container: "mkv", video: "hevc10", hdr: "dv", res: "2160p", fps: 23.976,
             mbps: 24.6, gb: 14.2, audio: "eac3_51", legenda: "srt_embutida",
             entrega: "debrid_cache", http: 206, ttfb_ms: 210 },
    cadeia: [{ passo: "internet", estado: "ok" }, { passo: "tv", estado: "falhou" }],
    linha: [{ t: -1950, m: "video", e: "erro_nativo", a: 204, b: 0, k: "ums_204" }],
    privacidade: { redator: 1, campos_recusados: 0 },
    ...extra,
  };
}

function cenario(t) {
  const b = banco(t);
  const env = { DB: b.DB, FAROL_SAL: "sal-sintetico",
                SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon", TRAKT_CLIENT_ID: "tc" };
  const post = async (corpo, token = "tok-farol") => {
    const res = await worker.fetch(new Request("https://w.test/v1/farol/registro", {
      method: "POST",
      headers: { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" },
      body: typeof corpo === "string" ? corpo : JSON.stringify(corpo),
    }), env);
    return { status: res.status, body: await res.json().catch(() => null) };
  };
  return { ...b, env, post };
}

const q = (sqlite, sql, ...v) => sqlite.prepare(sql).all(...v);

test("registro valido grava e devolve codigo de 6 com id >= 2^29; pessoa_h nunca e o id", async (t) => {
  const { sqlite, post } = cenario(t);
  const r = await post(pacote());
  assert.equal(r.status, 200);
  assert.equal(r.body.ok, 1);
  assert.match(r.body.codigo, /^[0-9A-HJKMNP-TV-Z]{6}$/);
  assert.ok(r.body.vence > agora() + 29 * 86400);
  const linha = q(sqlite, "SELECT * FROM farol_registro")[0];
  assert.ok(linha.id >= 536870912);
  assert.equal(linha.codigo, "ums_204");
  assert.equal(linha.versao, "2.1.0");
  assert.equal(linha.plataforma, "webos");
  assert.equal(linha.so_maior, 4);
  assert.equal(linha.vezes, 1);
  assert.match(linha.pessoa_h, /^[0-9a-f]{64}$/);
  assert.notEqual(linha.pessoa_h, "nuvio:pessoa-farol");
  assert.equal(JSON.parse(linha.json).fingerprint, pacote().fingerprint);
});

test("chave desconhecida, '://', '@', IPv4 e tipo errado dao 422 sem gravar", async (t) => {
  const { sqlite, post } = cenario(t);
  assert.equal((await post({ ...pacote(), titulo: "filme x" })).status, 422);
  assert.equal((await post(pacote({ fingerprint: "https://vazou.com/x" }))).status, 422);
  assert.equal((await post(pacote({ codigo: "dono@exemplo" }))).status, 422);
  const ipv4 = pacote(); ipv4.midia.entrega = "192.168.0.10";
  assert.equal((await post(ipv4)).status, 422);
  const tipo = pacote(); tipo.aparelho.so_maior = "quatro";
  assert.equal((await post(tipo)).status, 422);
  const longo = pacote(); longo.codigo = "x".repeat(49);
  assert.equal((await post(longo)).status, 422);
  const urlNaLinha = pacote(); urlNaLinha.linha[0].k = "http://fonte/a.mkv";
  assert.equal((await post(urlNaLinha)).status, 422);
  assert.equal((await post({ v: 1 })).status, 422);          // sem fingerprint/codigo
  assert.equal((await post("nao-json{")).status, 400);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 0);
});

test("corpo acima de 64 KB da 413", async (t) => {
  const { post } = cenario(t);
  const p = pacote(); p.fingerprint = "f".repeat(70 * 1024);
  assert.equal((await post(p)).status, 413);
});

test("mesmo fp da mesma pessoa em 24 h devolve o mesmo codigo e soma vezes", async (t) => {
  const { sqlite, post } = cenario(t);
  const a = await post(pacote());
  const b = await post(pacote());
  assert.equal(a.body.codigo, b.body.codigo);
  assert.equal(q(sqlite, "SELECT vezes FROM farol_registro")[0].vezes, 2);
  // fp diferente = registro novo.
  const c = await post(pacote({ fingerprint: "webos-4-starfish-ums_200-mkv-hevc-dts-ac3" }));
  assert.notEqual(c.body.codigo, a.body.codigo);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 2);
});

test("setimo envio na hora da 429 e o diario tambem tem teto", async (t) => {
  const { sqlite, post } = cenario(t);
  for (let i = 0; i < 6; i++)
    assert.equal((await post(pacote({ fingerprint: "fp-" + i }))).status, 200, "envio " + i);
  assert.equal((await post(pacote({ fingerprint: "fp-setimo" }))).status, 429);
  sqlite.exec("DELETE FROM limite WHERE chave LIKE 'farol:h:%'");
  sqlite.exec("UPDATE limite SET n = 20 WHERE chave LIKE 'farol:d:%'");
  assert.equal((await post(pacote({ fingerprint: "fp-dia" }))).status, 429);
  sqlite.exec("DELETE FROM limite WHERE chave LIKE 'farol:d:%'");
  sqlite.exec("UPDATE limite SET n = 500 WHERE chave = 'farol:dia'");
  assert.equal((await post(pacote({ fingerprint: "fp-global" }))).status, 429);
});

test("limpeza apaga farol_registro com mais de 30 dias", async (t) => {
  const { sqlite, env, post } = cenario(t);
  await post(pacote());
  sqlite.prepare("UPDATE farol_registro SET criado = ?").run(agora() - 31 * 86400);
  await limparFarol(env, agora());
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 0);
  // Recente fica.
  await post(pacote());
  await limparFarol(env, agora());
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 1);
});

test("retencao do registro de texto: manual 30 dias, automatico 4", async (t) => {
  const { sqlite, env } = cenario(t);
  const ag = agora();
  const ins = (pessoa, quando, criado) => sqlite.prepare(
    "INSERT INTO registro (pessoa, versao, plataforma, quando, texto, criado) VALUES (?, '2.1.0', 'webos', ?, 'x', ?)"
  ).run(pessoa, quando, criado);
  ins("nuvio:a", "hoje", ag - 29 * 86400);           // manual recente: fica
  ins("nuvio:b", "hoje", ag - 31 * 86400);           // manual velho: sai
  ins("nuvio:c", "hoje (auto)", ag - 5 * 86400);     // auto velho: sai
  ins("nuvio:d", "hoje (auto)", ag - 3 * 86400);     // auto recente: fica
  ins("arranque:tv", "arranque", ag - 29 * 86400);   // arranque recente: fica
  await limparRegistro(env, ag);
  const resto = q(sqlite, "SELECT pessoa FROM registro ORDER BY pessoa").map((r) => r.pessoa);
  assert.deepEqual(resto, ["arranque:tv", "nuvio:a", "nuvio:d"]);
});
