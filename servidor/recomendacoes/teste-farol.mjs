// Farol: POST /v1/farol/registro — pacote ANINHADO da secao 2.6 do PLANO,
// validacao por lista branca, codigo de 6 caracteres com ids >= 2^29, dedupe
// atomico por (pessoa_h, fp), cotas por CONTA e retencao de 30 dias. Worker
// inteiro com D1 falso em memoria (padrao teste-loglink.mjs); a sessao e
// semeada, sem rede. Node 22+:
//   node --test servidor/recomendacoes/teste-farol.mjs
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
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
const fpDe = (s) => createHash("sha256").update(s).digest("hex").slice(0, 16);

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

// O pacote ANINHADO do exemplo da secao 2.6 do PLANO, encurtado. `erro.fp` e
// gerado pela FORMULA (16 hex do SHA-256 do fingerprint), nao pelo literal
// ilustrativo do plano, que diverge.
function pacote() {
  const fingerprint = "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3";
  return {
    v: 1,
    gatilho: "falha_sem_conserto",
    sintoma: "nao_comeca",
    erro: {
      codigo: "TV_FORMATO_REPETE", estagio: "tv",
      nativo: "ums_204", nativo_num: 204, http: 0,
      fingerprint, fp: fpDe(fingerprint),
      tentou: [{ acao: "proxima_fonte", ok: false, ms: 6400 },
               { acao: "reconexao", ok: false, ms: 13000 }],
    },
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
  };
}

function cenario(t) {
  const b = banco(t);
  const env = { DB: b.DB, FAROL_SAL: "sal-sintetico",
                SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon", TRAKT_CLIENT_ID: "tc" };
  const post = async (corpo, { token = "tok-farol", perfil, rota = "/v1/farol/registro" } = {}) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" };
    if (perfil) headers["x-nuvio-perfil"] = String(perfil);
    const res = await worker.fetch(new Request("https://w.test" + rota, {
      method: "POST", headers,
      body: typeof corpo === "string" ? corpo : JSON.stringify(corpo),
    }), env);
    return { status: res.status, body: await res.json().catch(() => null) };
  };
  return { ...b, env, post };
}

const q = (sqlite, sql, ...v) => sqlite.prepare(sql).all(...v);

test("pacote aninhado da secao 2.6 grava e devolve codigo de 6 com id >= 2^29; pessoa_h nunca e o id", async (t) => {
  const { sqlite, post } = cenario(t);
  const r = await post(pacote());
  assert.equal(r.status, 200, JSON.stringify(r.body));
  assert.equal(r.body.ok, 1);
  assert.match(r.body.codigo, /^[0-9A-HJKMNP-TV-Z]{6}$/);
  assert.ok(r.body.vence > agora() + 29 * 86400);
  const linha = q(sqlite, "SELECT * FROM farol_registro")[0];
  assert.ok(linha.id >= 536870912);
  // O codigo do PLAYBOOK sai de erro.codigo, e o fp gravado e o da formula.
  assert.equal(linha.codigo, "TV_FORMATO_REPETE");
  assert.equal(linha.fp, fpDe(pacote().erro.fingerprint));
  assert.equal(linha.versao, "2.1.0");
  assert.equal(linha.plataforma, "webos");
  assert.equal(linha.so_maior, 4);
  assert.equal(linha.vezes, 1);
  assert.match(linha.pessoa_h, /^[0-9a-f]{64}$/);
  assert.notEqual(linha.pessoa_h, "nuvio:pessoa-farol");
  assert.equal(JSON.parse(linha.json).erro.fingerprint, pacote().erro.fingerprint);
});

test("chave herdada/desconhecida, enum fora da lista, tipo errado e dado proibido dao 422 sem gravar", async (t) => {
  const { sqlite, post } = cenario(t);
  // Chave propria desconhecida e chave HERDADA de Object.prototype.
  const titulo = pacote(); titulo.titulo = "filme x";
  assert.equal((await post(titulo)).status, 422);
  const ctor = pacote(); ctor.constructor = {};
  assert.equal((await post(ctor)).status, 422);
  const ctorAninhado = pacote(); ctorAninhado.erro.constructor = {};
  assert.equal((await post(ctorAninhado)).status, 422);
  // Enums fechados: debrid, modelo (sem ":"/"+"), rede.offline so booleano.
  const deb = pacote(); deb.cliente.debrid = ["debridgratis"];
  assert.equal((await post(deb)).status, 422);
  const mod = pacote(); mod.aparelho.modelo = "OLED:55+A";
  assert.equal((await post(mod)).status, 422);
  const off = pacote(); off.aparelho.rede.offline = 1;
  assert.equal((await post(off)).status, 422);
  // Dados proibidos em claro: PIN em cliente.commit, IPv6 e cadeia hex longa.
  const pin = pacote(); pin.cliente.commit = "12345678";
  assert.equal((await post(pin)).status, 422);
  const v6 = pacote(); v6.linha[0].k = "2001:db8::1";
  assert.equal((await post(v6)).status, 422);
  const hex = pacote(); hex.linha[0].k = "a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";
  assert.equal((await post(hex)).status, 422);
  // Sinais classicos de vazamento e tipo errado.
  const fp = pacote(); fp.erro.fingerprint = "https://vazou.com/x";
  assert.equal((await post(fp)).status, 422);
  const ar = pacote(); ar.erro.codigo = "dono@exemplo";
  assert.equal((await post(ar)).status, 422);
  const ipv4 = pacote(); ipv4.midia.entrega = "192.168.0.10";
  assert.equal((await post(ipv4)).status, 422);
  const tipo = pacote(); tipo.aparelho.so_maior = "quatro";
  assert.equal((await post(tipo)).status, 422);
  const urlNaLinha = pacote(); urlNaLinha.linha[0].k = "http://fonte/a.mkv";
  assert.equal((await post(urlNaLinha)).status, 422);
  assert.equal((await post({ v: 1 })).status, 422);          // sem erro.codigo/fingerprint
  assert.equal((await post("nao-json{")).status, 400);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 0);
});

test("corpo acima de 64 KB CRUS da 413 medido antes de materializar", async (t) => {
  const { post } = cenario(t);
  // 65.537 bytes crus com preenchimento de espacos: o JSON continua valido e
  // o trim() antigo o aceitaria — o teto tem de medir o corpo cru.
  const cru = JSON.stringify(pacote());
  const grande = cru + " ".repeat(65537 - cru.length);
  assert.equal(grande.length, 65537);
  assert.equal((await post(grande)).status, 413);
  // Multibyte tambem conta byte, nao unidade UTF-16.
  const mb = pacote(); mb.linha[0].k = "x";
  const cruMb = JSON.stringify(mb);
  const enc = new TextEncoder();
  const enche = "é".repeat(Math.ceil((65537 - enc.encode(cruMb).length) / 2));
  const mbGrande = cruMb.replace(/}$/, `,"enche":"${enche}"}`);
  assert.ok(enc.encode(mbGrande).length > 65536);
  assert.equal((await post(mbGrande)).status, 413);
  // E exatamente 64 KB crus ainda passa no teto.
  const cruOk = JSON.stringify(pacote());
  const justo = cruOk + " ".repeat(65536 - cruOk.length);
  assert.equal((await post(justo)).status, 200);
});

test("recusa nao cria pessoa: nada de id cru nem nome de conta gravados", async (t) => {
  const { sqlite, post } = cenario(t);
  const ruim = pacote(); ruim.erro.codigo = "INVENTADO";
  assert.equal((await post(ruim)).status, 422);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM pessoa")[0].n, 0);
  // Nem um pacote VALIDO cadastra: a conta ja foi verificada, so isso.
  assert.equal((await post(pacote())).status, 200);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM pessoa")[0].n, 0);
});

test("sem FAROL_SAL nao grava: responde 5xx no envelope JSON", async (t) => {
  const b = cenario(t);
  const envSemSal = { ...b.env, FAROL_SAL: undefined };
  const res = await worker.fetch(new Request("https://w.test/v1/farol/registro", {
    method: "POST",
    headers: { authorization: "Bearer tok-farol", "x-nuvio-auth": "nuvio" },
    body: JSON.stringify(pacote()),
  }), envSemSal);
  assert.equal(res.status, 503);
  assert.match((await res.json()).erro, /farol/);
  assert.equal(q(b.sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 0);
});

test("cota e dedupe sao por CONTA: o sufixo de perfil nao abre cota nova", async (t) => {
  const { sqlite, post } = cenario(t);
  // O MESMO fingerprint reenviado de outro perfil ganha o mesmo recibo:
  // a dedupe e da conta, nao da pessoa-perfil.
  const a = await post(pacote(), { perfil: 1 });
  assert.equal(a.status, 200);
  const b = await post(pacote(), { perfil: 2 });
  assert.equal(b.body.codigo, a.body.codigo);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro WHERE fingerprint = ?",
    "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3")[0].n, 1);
  // Seis fingerprints distintos esgotam a hora da CONTA (o envio acima ja
  // contou um); no OUTRO perfil da mesma conta o setimo continua recusado.
  for (let i = 0; i < 5; i++) {
    const p = pacote(); p.erro.fingerprint = "fp-" + i;
    assert.equal((await post(p, { perfil: 1 })).status, 200, "envio " + i);
  }
  const setimo = pacote(); setimo.erro.fingerprint = "fp-setimo";
  assert.equal((await post(setimo, { perfil: 1 })).status, 429);
  assert.equal((await post(setimo, { perfil: 2 })).status, 429);
});

test("dois POSTs da mesma falha viram uma linha; reenvio promete o vencimento certo", async (t) => {
  const { sqlite, post } = cenario(t);
  // Concorrencia: os dois pedidos juntos so podem gerar UMA linha e o MESMO
  // codigo (indice unico + ON CONFLICT).
  const [a, b] = await Promise.all([post(pacote()), post(pacote())]);
  assert.equal(a.status, 200); assert.equal(b.status, 200);
  assert.equal(a.body.codigo, b.body.codigo);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 1);
  assert.equal(q(sqlite, "SELECT vezes FROM farol_registro")[0].vezes, 2);
  // Reenvio sequencial de uma linha de 1 h atras: o vencimento prometido e
  // criado + 30 dias — o MESMO relogio que a limpeza usa — nao reenvio + 30.
  sqlite.prepare("UPDATE farol_registro SET criado = ?, vezes = 1").run(agora() - 3600);
  const r = await post(pacote());
  assert.equal(r.status, 200);
  const linha = q(sqlite, "SELECT id, criado, vezes FROM farol_registro")[0];
  assert.equal(r.body.vence, linha.criado + 30 * 86400);
  assert.equal(linha.vezes, 2);
});

test("mesmo fp da mesma conta em 24 h devolve o mesmo codigo; fp diferente, registro novo", async (t) => {
  const { sqlite, post } = cenario(t);
  const a = await post(pacote());
  const b = await post(pacote());
  assert.equal(a.body.codigo, b.body.codigo);
  const c = pacote(); c.erro.fingerprint = "webos-4-starfish-ums_200-mkv-hevc-dts-ac3";
  const r = await post(c);
  assert.notEqual(r.body.codigo, a.body.codigo);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 2);
});

test("setimo envio na hora da 429 e o diario tambem tem teto", async (t) => {
  const { sqlite, post } = cenario(t);
  for (let i = 0; i < 6; i++) {
    const p = pacote(); p.erro.fingerprint = "fp-" + i;
    assert.equal((await post(p)).status, 200, "envio " + i);
  }
  const setimo = pacote(); setimo.erro.fingerprint = "fp-setimo";
  assert.equal((await post(setimo)).status, 429);
  sqlite.exec("DELETE FROM limite WHERE chave LIKE 'farol:h:%'");
  sqlite.exec("UPDATE limite SET n = 20 WHERE chave LIKE 'farol:d:%'");
  const dia = pacote(); dia.erro.fingerprint = "fp-dia";
  assert.equal((await post(dia)).status, 429);
  sqlite.exec("DELETE FROM limite WHERE chave LIKE 'farol:d:%'");
  sqlite.exec("UPDATE limite SET n = 500 WHERE chave = 'farol:dia'");
  const glob = pacote(); glob.erro.fingerprint = "fp-global";
  assert.equal((await post(glob)).status, 429);
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

test("sqlite_sequence: faixa reservada e migracao repetivel sem baixar nem duplicar", async (t) => {
  const { sqlite, post } = cenario(t);
  assert.equal((await post(pacote())).status, 200);
  const seq0 = q(sqlite, "SELECT seq FROM sqlite_sequence WHERE name = 'farol_registro'");
  assert.equal(seq0.length, 1);
  assert.ok(seq0[0].seq >= 536870912);
  // Reaplicar a migracao nao cria segunda linha nem baixa a sequencia.
  for (const cmd of readFileSync(new URL("migracao-018-farol.sql", import.meta.url), "utf8").split(/;\s*\n/)) {
    const c = cmd.replace(/^\s*--.*$/gm, "").trim();
    if (c) sqlite.exec(c);
  }
  const seq1 = q(sqlite, "SELECT seq FROM sqlite_sequence WHERE name = 'farol_registro'");
  assert.equal(seq1.length, 1);
  assert.ok(seq1[0].seq >= seq0[0].seq);
});

test("envio manual identico a um automatico preserva o prazo de 30 dias", async (t) => {
  const { sqlite, env, post } = cenario(t);
  const texto = "REGISTRO DE TESTE " + "x".repeat(200);
  const a = await post({ versao: "2.1.0", plataforma: "webos", quando: "hoje (auto)", texto },
                       { rota: "/v1/registro" });
  assert.equal(a.status, 200);
  const m = await post({ versao: "2.1.0", plataforma: "webos", quando: "hoje", texto },
                       { rota: "/v1/registro" });
  assert.equal(m.status, 200);
  // Mesmo texto, origens diferentes: o manual NAO herda o recibo nem os 4
  // dias do automatico — e linha nova com prazo proprio.
  assert.notEqual(m.body.registro_id, a.body.registro_id);
  const ag = agora();
  sqlite.prepare("UPDATE registro SET criado = ?").run(ag - 5 * 86400);
  await limparRegistro(env, ag);
  const resto = q(sqlite, "SELECT id FROM registro").map((r) => r.id);
  assert.deepEqual(resto, [m.body.registro_id]);
});

test("codigo-registro.mjs preserva a saida legada pura; tabela so com --tabela", async () => {
  const cli = new URL("codigo-registro.mjs", import.meta.url).pathname;
  const run = (...args) => execFileSync("node", [cli, ...args], { encoding: "utf8" }).trim();
  // Legado: id puro no decode, codigo puro no --id. Sem tabela na saida.
  const codigo = run("--id", "536870912");
  assert.match(codigo, /^[0-9A-HJKMNP-TV-Z]{6}$/);
  assert.equal(run(codigo), "536870912");
  // Opt-in: a tabela sai so com --tabela, dos dois lados da bijecao.
  assert.equal(run("--tabela", codigo), "536870912\tfarol_registro");
  assert.equal(run("--id", "9866"), run("--id", "9866"));
  assert.equal(run("--id", "9866", "--tabela"), run("--id", "9866") + "\tregistro");
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
