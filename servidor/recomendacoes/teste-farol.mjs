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
import { limparFarol, rotaFarolRegistro } from "./src/farol.js";
import { limitar } from "./src/amigos.js";
import { codigoRegistro } from "./src/codigo.js";

const MIGRACOES = ["schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql",
  "migracao-018-farol.sql", "migracao-019-farol-janela.sql"];

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
      // Versao sempre etiquetada, como o coletor manda (farolredator.c: a
      // numerica pura parseia como IPv4 WHATWG e o redator a recusa).
      plataforma: "webos", so: "4.9-webos", so_maior: 4, modelo: "OLED55C9PSA",
      ram_total_mb: 1870, ram_livre_mb: 212,
      tela: { w: 3840, h: 2160, hdr: 1, dv: 1, hevc: 1, av1: -1 },
      audio: { dts: 1, truehd: -1, eac3: 1, ac3: 1 },
      rede: { offline: false, hosts_com_falha: 0, vazao_mbps: 0 },
    },
    cliente: {
      versao: "2.1.0-ipk", commit: "c18c21756", build: "webos-ipk", backend: "starfish",
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
  assert.equal(linha.versao, "2.1.0-ipk");
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
    const p = pacote(); p.erro.fingerprint = `webos-4-starfish-ums_${210 + i}-mkv-hevc10-dv-eac3`;
    assert.equal((await post(p, { perfil: 1 })).status, 200, "envio " + i);
  }
  const setimo = pacote(); setimo.erro.fingerprint = "webos-4-starfish-ums_299-mkv-hevc10-dv-eac3";
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
  // A farol_dedup guarda o criado do registro vivo — envelhecer a linha exige
  // envelhecer o slot junto, como ocorre de verdade (o pai nao tinha tabela).
  try { sqlite.prepare("UPDATE farol_dedup SET criado = ?").run(agora() - 3600); } catch { /* sem a tabela */ }
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
  const c = pacote(); c.erro.fingerprint = "webos-4-starfish-ums_200-mkv-hevc-hlg-ac3";
  const r = await post(c);
  assert.notEqual(r.body.codigo, a.body.codigo);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 2);
});

test("setimo envio na hora da 429 e o diario tambem tem teto", async (t) => {
  const { sqlite, post } = cenario(t);
  const fpN = (i) => `webos-4-starfish-ums_${200 + i}-mkv-hevc10-dv-eac3`;
  for (let i = 0; i < 6; i++) {
    const p = pacote(); p.erro.fingerprint = fpN(i);
    assert.equal((await post(p)).status, 200, "envio " + i);
  }
  const setimo = pacote(); setimo.erro.fingerprint = fpN(90);
  assert.equal((await post(setimo)).status, 429);
  sqlite.exec("DELETE FROM limite WHERE chave LIKE 'farol:h:%'");
  sqlite.exec("UPDATE limite SET n = 20 WHERE chave LIKE 'farol:d:%'");
  const dia = pacote(); dia.erro.fingerprint = fpN(91);
  assert.equal((await post(dia)).status, 429);
  sqlite.exec("DELETE FROM limite WHERE chave LIKE 'farol:d:%'");
  sqlite.exec("UPDATE limite SET n = 500 WHERE chave = 'farol:dia'");
  const glob = pacote(); glob.erro.fingerprint = fpN(92);
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

// --- correcao farol5b ---------------------------------------------------------
// O contrato de texto e o do REDATOR (src/farolredator.c, src/farolnativo.h da
// issue 4): gramaticas fechadas por campo, nao um alfabeto generico. A fixture
// e o pacote que o redator C emite de verdade (versoes etiquetadas, enums
// fechados); o erro.fp e recalculado pela formula do indice.
const pacoteEmitido = () => {
  const p = JSON.parse(readFileSync(new URL("teste-farol-fixture-c.json", import.meta.url), "utf8"));
  p.erro.fp = fpDe(p.erro.fingerprint);
  return p;
};
// Chamada direta da rota com relogio e limitar injetaveis — e assim que a
// janela de dedupe e a concorrencia ficam deterministicas (padrao das sondas).
const chamadaFarol = (env, corpo, t, lim = limitar) =>
  rotaFarolRegistro(env, { id: "nuvio:qa" }, corpo,
    { json: (body) => ({ status: 200, body }), erro: (_, status) => ({ status }), agora: () => t },
    lim, codigoRegistro);

// Barreira de duas requisicoes sobre o D1 falso: segura a segunda consulta que
// casa `escolher` ate as duas terem lido — o cenario em que uma leitura
// separada da gravacao corria. No codigo corrigido nao ha consulta de dedupe
// nem de cota antes do UPSERT, entao a barreira nunca dispara: a atomicidade
// e estrutural. No codigo anterior ela forca a corrida real.
function barreira() {
  let liberar;
  let chamadas = 0;
  const espera = new Promise((resolve) => { liberar = resolve; });
  return async () => { if (++chamadas === 2) liberar(); await espera; };
}
function d1EmPar(DB, escolher, passo) {
  const original = DB.prepare.bind(DB);
  DB.prepare = (sql) => {
    const stm = original(sql);
    if (!escolher(sql)) return stm;
    const bind = stm.bind.bind(stm);
    stm.bind = (...args) => {
      const bound = bind(...args);
      const first = bound.first.bind(bound);
      bound.first = async () => { const result = await first(); await passo(args); return result; };
      return bound;
    };
    return stm;
  };
}

test("o pacote emitido pelo redator C grava; os formatos privados dao 422 sem gravar", async (t) => {
  const { sqlite, post } = cenario(t);
  assert.equal((await post(pacoteEmitido())).status, 200);
  let n = 0;
  const base = () => {
    const p = pacoteEmitido();
    p.erro.fingerprint = `webos-4-starfish-ums_${210 + (n++)}-mkv-hevc10-dv-eac3`;
    p.erro.fp = fpDe(p.erro.fingerprint);
    return p;
  };
  const casos = [
    ["h: com comprimento errado", (p) => { p.linha[0].k = "h:0123abc"; }],
    ["av_* fora dos 11 nomes AVPlay", (p) => { p.erro.nativo = "av_canario_sintetico"; }],
    ["tag de versao fora da lista", (p) => { p.aparelho.so = "4.9-7362194"; }],
    ["tag de versao-patch fora da lista", (p) => { p.cliente.versao = "2.1.0-canario"; }],
    ["versao numerica pura vira IPv4 WHATWG", (p) => { p.aparelho.so = "4.9"; }],
    ["IPv4 abreviado em token", (p) => { p.linha[0].k = "127.0.1"; }],
    ["IPv4 hexadecimal em token", (p) => { p.linha[0].k = "0xc633642a"; }],
    ["IPv4 octal em modelo", (p) => { p.aparelho.modelo = "0300.0063.052"; }],
    ["IPv4 decimal em token", (p) => { p.linha[0].k = "3325256746"; }],
    ["IPv4 em campo de versao", (p) => { p.aparelho.so = "127.0.1"; }],
    ["IPv6 em token", (p) => { p.linha[0].k = "2001:db8::1"; }],
    ["IPv6 mapeado em token", (p) => { p.linha[0].k = "::ffff:198.51.100.42"; }],
    ["PIN em token", (p) => { p.linha[0].k = "7362194"; }],
    ["PIN em modelo", (p) => { p.aparelho.modelo = "7362194"; }],
    ["PIN em commit", (p) => { p.cliente.commit = "7362194"; }],
    ["hex longo em token", (p) => { p.linha[0].k = "abcde0123456789abcdef0123"; }],
    ["URL em token", (p) => { p.linha[0].k = "https://example.test/qa"; }],
    ["arroba em token", (p) => { p.linha[0].k = "qa@example.test"; }],
    ["modelo fora da gramatica", (p) => { p.aparelho.modelo = "Canario Modelo"; }],
    ["fingerprint com sete componentes", (p) => {
      p.erro.fingerprint = "webos-4-starfish-ums_204-mkv-hevc10-eac3"; p.erro.fp = fpDe(p.erro.fingerprint); }],
    ["fingerprint com nativo livre", (p) => {
      p.erro.fingerprint = "webos-4-starfish-texto_livre-mkv-hevc10-dv-eac3"; p.erro.fp = fpDe(p.erro.fingerprint); }],
  ];
  for (const [nome, mudar] of casos) {
    const p = base();
    mudar(p);
    assert.equal((await post(p)).status, 422, nome);
  }
  // Os aceites da gramatica fechada seguem aceites — cada um com fp proprio.
  const positivos = [
    ["h:<8hex>", (p) => { p.linha[0].k = "h:0123abcd"; }],
    ["enum AVPlay publico", (p) => { p.erro.nativo = "av_invalid_uri"; }],
    ["codigo nativo m3_ na faixa", (p) => { p.linha[0].k = "m3_2004"; }],
    ["token de fonte composto", (p) => { p.linha[0].k = "720p_sdr_nodv_ac3"; }],
  ];
  for (const [nome, mudar] of positivos) {
    const p = base();
    mudar(p);
    assert.equal((await post(p)).status, 200, nome);
  }
  // 1 da fixture + 4 positivos; nenhum 422 gravou linha.
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 5);
});

test("o fingerprint longo emitido pelo redator C (72 bytes) e aceito", async (t) => {
  const { sqlite, post } = cenario(t);
  const emitido = JSON.parse(readFileSync(new URL("teste-farol-fixture-longo.json", import.meta.url), "utf8"));
  emitido.erro.fp = fpDe(emitido.erro.fingerprint);
  // O contrato de tamanho e o do emissor (ate 159 bytes), nao o 48 generico.
  assert.ok(emitido.erro.fingerprint.length > 48);
  const r = await post(emitido);
  assert.equal(r.status, 200, JSON.stringify(r.body));
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 1);
});

test("dedupe atomico cobre a janela inteira de 24 h, inclusive a virada UTC", async (t) => {
  const { sqlite, env } = cenario(t);
  const corpo = pacoteEmitido();
  const dia = 86400;
  // Dois envios a um segundo de distancia, um de cada lado de 00:00 UTC: a
  // chave (pessoa_h, fp) sem dia faz o ON CONFLICT decidir — um recibo so.
  const [a, b] = await Promise.all([chamadaFarol(env, corpo, dia - 1), chamadaFarol(env, corpo, dia)]);
  assert.equal(a.status, 200);
  assert.equal(b.status, 200);
  assert.equal(a.body.codigo, b.body.codigo);
  const linhas = q(sqlite, "SELECT id, criado, vezes FROM farol_registro");
  assert.equal(linhas.length, 1);
  assert.equal(linhas[0].vezes, 2);
  // Qualquer um dos dois pedidos paralelos pode vencer o UPSERT: o criado
  // persistido e o do vencedor — um dos dois instantes — e e exatamente o
  // que o slot de dedupe guarda (flake pre-existente da asserção fixa).
  assert.ok(linhas[0].criado === dia - 1 || linhas[0].criado === dia,
    `criado ${linhas[0].criado} fora de {${dia - 1}, ${dia}}`);
  assert.equal(q(sqlite, "SELECT criado, registro FROM farol_dedup")[0].criado, linhas[0].criado);
  // Passadas 24 h nao e mais reenvio: e OCORRENCIA NOVA — linha e codigo
  // novos, e a linha do recibo original fica intacta ate a retencao de 30
  // dias (o codigo antigo continua resolvendo o mesmo registro).
  const c = await chamadaFarol(env, corpo, dia + 86401);
  assert.equal(c.status, 200);
  const depois = q(sqlite, "SELECT criado, vezes FROM farol_registro ORDER BY criado");
  assert.equal(depois.length, 2);
  assert.equal(depois[0].criado, linhas[0].criado);
  assert.equal(depois[0].vezes, 2);
  assert.equal(depois[1].criado, dia + 86401);
  assert.equal(depois[1].vezes, 1);
  assert.notEqual(c.body.codigo, a.body.codigo);
  // O reenvio deduplica na ocorrencia NOVA (o slot vivo aponta para ela).
  const d = await chamadaFarol(env, corpo, dia + 86402);
  assert.equal(d.status, 200);
  assert.equal(d.body.codigo, c.body.codigo);
});

test("a cota de 6/h e atomica: o setimo envio simultaneo le o contador incrementado", async (t) => {
  const { sqlite, env } = cenario(t);
  const t0 = 86400100;
  const pacoteN = (i) => {
    const p = pacoteEmitido();
    p.erro.fingerprint = `webos-4-starfish-ums_${200 + i}-mkv-hevc10-dv-eac3`;
    p.erro.fp = fpDe(p.erro.fingerprint);
    return p;
  };
  for (let i = 0; i < 5; i++)
    assert.equal((await chamadaFarol(env, pacoteN(i), t0)).status, 200, "envio " + i);
  // No codigo antigo a leitura da cota era um SELECT separado da gravacao: a
  // barreira faz os dois envios lerem o mesmo contador. No corrigido a cota e
  // um UPSERT atomico — nao ha o que segurar.
  const leituras = barreira();
  d1EmPar(env.DB, (sql) => sql.startsWith("SELECT n FROM limite"), async ([chave]) => {
    if (String(chave).startsWith("farol:h:")) await leituras();
  });
  const res = (await Promise.all([chamadaFarol(env, pacoteN(5), t0), chamadaFarol(env, pacoteN(6), t0)]))
    .map((r) => r.status).sort();
  assert.deepEqual(res, [200, 429]);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 6);
});

test("reenvio deduplicado devolve o mesmo codigo sem debitar cota", async (t) => {
  const { sqlite, env } = cenario(t);
  const debitos = [];
  const limConta = async (e, chave, max, jan, tt) => { debitos.push(chave); return limitar(e, chave, max, jan, tt); };
  const corpo = pacoteEmitido();
  const t0 = 90000;
  // A barreira so dispara na consulta de dedupe que o codigo antigo fazia
  // antes de gravar; no corrigido o UPSERT e a primeira instrucao.
  const leituras = barreira();
  d1EmPar(env.DB, (sql) => sql.startsWith("SELECT id, criado FROM farol_registro"), leituras);
  const [a, b] = await Promise.all([chamadaFarol(env, corpo, t0, limConta), chamadaFarol(env, corpo, t0, limConta)]);
  assert.equal(a.status, 200);
  assert.equal(b.status, 200);
  assert.equal(a.body.codigo, b.body.codigo);
  const linhas = q(sqlite, "SELECT vezes FROM farol_registro");
  assert.equal(linhas.length, 1);
  assert.equal(linhas[0].vezes, 2);
  // Um registro novo debita os tres contadores uma vez; o dedupe, zero.
  assert.equal(debitos.length, 3);
});

test("migracao sobre a 018 antiga: funde so a mesma janela, nunca dias distintos", (t) => {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  // A 018 de 705a519ac: tabela SEM `janela` e sem dedupe — e ela que deixava
  // linhas repetidas do mesmo (pessoa_h, fp). Cenario: dois reenvios da MESMA
  // janela (T e T+100 s) MAIS uma ocorrencia legitima 48 h depois (a linha
  // 536870916 que a consolidacao cega apagava, junto do recibo dela) e um
  // segundo fingerprint intocado.
  sqlite.exec(`CREATE TABLE pessoa (id TEXT PRIMARY KEY, criado INTEGER NOT NULL, visto INTEGER NOT NULL);
    CREATE TABLE farol_registro (
      id INTEGER PRIMARY KEY AUTOINCREMENT, criado INTEGER NOT NULL,
      pessoa_h TEXT NOT NULL, versao TEXT NOT NULL, plataforma TEXT NOT NULL,
      so_maior INTEGER NOT NULL DEFAULT 0, codigo TEXT NOT NULL,
      fp TEXT NOT NULL, fingerprint TEXT NOT NULL,
      vezes INTEGER NOT NULL DEFAULT 1, json TEXT NOT NULL);
    CREATE INDEX farol_fp ON farol_registro (fp, criado);
    CREATE INDEX farol_criado ON farol_registro (criado);`);
  sqlite.exec(`INSERT INTO pessoa VALUES ('nuvio:qa-legado', 1700000000, 1700000000);
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json)
      VALUES (1700000000, 'h', '2.1.0-ipk', 'webos', 'TV_FORMATO', '0123456789abcdef', 'fp-a', '{"sintoma":"nao_comeca"}');
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, vezes, json)
      VALUES (1700000100, 'h', '2.1.0-ipk', 'webos', 'TV_FORMATO', '0123456789abcdef', 'fp-a', 3, '{"sintoma":"reenvio"}');
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json)
      VALUES (1700172900, 'h', '2.1.0-ipk', 'webos', 'REDE_FORA', '0123456789abcdef', 'fp-a', '{"sintoma":"48h"}');
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json)
      VALUES (1700000200, 'h', '2.1.0-ipk', 'webos', 'TV_FORMATO', 'fedcba9876543210', 'fp-b', '{"sintoma":"outro"}');
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json)
      VALUES (1700000000, 'h', '2.1.0-ipk', 'webos', 'TV_FORMATO', 'aaaaaaaabbbbcccc', 'fp-c', '{"sintoma":"corrente0"}');
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, vezes, json)
      VALUES (1700050000, 'h', '2.1.0-ipk', 'webos', 'TV_FORMATO', 'aaaaaaaabbbbcccc', 'fp-c', 2, '{"sintoma":"corrente1"}');
    INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json)
      VALUES (1700090000, 'h', '2.1.0-ipk', 'webos', 'TV_FORMATO', 'aaaaaaaabbbbcccc', 'fp-c', '{"sintoma":"corrente2"}');`);
  const aplica = (arquivo) => {
    for (const cmd of readFileSync(new URL(arquivo, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (c) sqlite.exec(c);
    }
  };
  aplica("migracao-018-farol.sql");
  aplica("migracao-019-farol-janela.sql");
  const colunas = sqlite.prepare("SELECT name FROM pragma_table_info('farol_registro')").all().map((r) => r.name);
  assert.ok(colunas.includes("janela"), "a 019 cria a coluna que faltava");
  const dedup = sqlite.prepare("SELECT sql FROM sqlite_schema WHERE name = 'farol_dedup'").get();
  assert.ok(dedup, "a tabela de dedupe existe");
  assert.match(dedup.sql, /PRIMARY KEY \(pessoa_h, fp\)/);
  // Os dois reenvios fundem na linha mais antiga (vezes somado, recibo e JSON
  // ORIGINAIS); a ocorrencia de 48 h depois e o outro fingerprint ficam.
  // A corrente fp-c prova a janela FIXA na ancora: T+50000 s funde em T, mas
  // T+90000 s (> ancora+86400) e registro NOVO mesmo estando a 40000 s do
  // anterior — uma fusao por "alguma linha em ate 24 h" o apagaria.
  const linhas = sqlite.prepare("SELECT criado, janela, fp, vezes, json FROM farol_registro ORDER BY criado, id").all();
  assert.equal(linhas.length, 5);
  assert.equal(linhas[0].criado, 1700000000);
  assert.equal(linhas[0].fp, "0123456789abcdef");
  assert.equal(linhas[0].vezes, 4);
  assert.equal(JSON.parse(linhas[0].json).sintoma, "nao_comeca");
  assert.equal(linhas[1].criado, 1700000000);
  assert.equal(linhas[1].fp, "aaaaaaaabbbbcccc");
  assert.equal(linhas[1].vezes, 3);
  assert.equal(JSON.parse(linhas[1].json).sintoma, "corrente0");
  assert.equal(linhas[2].criado, 1700000200);
  assert.equal(linhas[2].vezes, 1);
  assert.equal(linhas[3].criado, 1700090000);
  assert.equal(linhas[3].vezes, 1);
  assert.equal(JSON.parse(linhas[3].json).sintoma, "corrente2");
  assert.equal(linhas[4].criado, 1700172900);
  assert.equal(linhas[4].vezes, 1);
  assert.equal(JSON.parse(linhas[4].json).sintoma, "48h");
  assert.equal(linhas[0].janela, Math.floor(1700000000 / 86400));
  // O slot de dedupe aponta para a linha MAIS NOVA de cada par (a que ainda
  // recebe reenvio na janela movel).
  const slots = sqlite.prepare("SELECT criado, confirmado FROM farol_dedup ORDER BY criado").all();
  assert.deepEqual(slots.map((s) => [s.criado, s.confirmado]),
    [[1700000200, 1], [1700090000, 1], [1700172900, 1]]);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM pessoa").get().n, 1);
  // Reaplicavel: 018 E 019 rodam limpas de novo, sem excecao — a 019 nao
  // depende mais do "duplicate column" tolerado pelo preparo.
  aplica("migracao-018-farol.sql");
  aplica("migracao-019-farol-janela.sql");
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM farol_registro").get().n, 5);
});

test("a 019 em banco novo e idempotente: janela preenchida, reaplicacao limpa", (t) => {
  // Banco NOVO (schema.sql + todas as migracoes, como o banco() faz): a 019
  // roda de novo sem erro e `janela` continua igual a criado/86400.
  const { sqlite } = cenario(t);
  const aplica = (arquivo) => {
    for (const cmd of readFileSync(new URL(arquivo, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (c) sqlite.exec(c);
    }
  };
  sqlite.prepare("INSERT INTO farol_registro (criado, janela, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json) VALUES (1700000000, 0, 'h', 'v', 'p', 'c', 'f1', 'fp', '{}')").run();
  sqlite.prepare("INSERT INTO farol_registro (criado, janela, pessoa_h, versao, plataforma, codigo, fp, fingerprint, json) VALUES (1700172900, 0, 'h', 'v', 'p', 'c', 'f2', 'fp', '{}')").run();
  aplica("migracao-019-farol-janela.sql");
  aplica("migracao-019-farol-janela.sql");
  const linhas = sqlite.prepare("SELECT criado, janela FROM farol_registro ORDER BY criado").all();
  assert.equal(linhas.length, 2);
  for (const l of linhas) assert.equal(l.janela, Math.floor(l.criado / 86400));
  // A sequencia reservada nao desce abaixo do piso nem de id ja gravado.
  const seq = sqlite.prepare("SELECT seq FROM sqlite_sequence WHERE name = 'farol_registro'").all();
  assert.equal(seq.length, 1);
  assert.ok(seq[0].seq >= 536870912);
});

// --- correcao farol5c ---------------------------------------------------------
// Os quatro bloqueios da revisao r3: tokens do redator rejeitados, reenvio que
// efetivava registro acima da cota, ocorrencia apos 24 h que destruia o recibo
// anterior e migracao que fundia janelas distintas / nao reaplicavel.

test("a lista de token_seguro do Worker e a MESMA do redator C", async (t) => {
  // A lista fixa do token_seguro (b29addd8a:src/farolredator.c:259-271),
  // literal — o teste quebra se as duas divergirem de novo.
  const fonte = readFileSync(new URL("src/farol.js", import.meta.url), "utf8");
  const lista = fonte.match(/TOK_ROTULOS = new Set\(\s*"([^"]+)"/)[1].split(" ").sort();
  const doC = "x|starfish|avplay|tpk|media3|mpv|dns|tls|prazo|recusa|sem_plano|fora_cache|busca|proxima|mem_baixa|2160p_hdr_dv_eac3"
    .split("|").sort();
  assert.deepEqual(lista, doC);
  // E os quatro rotulos que o redator de fato emite em linha[].k e
  // erro.nativo (busca/addons.c, proxima/streams.c, mem_baixa/memlog.c,
  // mpv/player.c) passam 200 — em 3e2983dcd davam 422 nos oito pacotes. Oito
  // envios da mesma conta estourariam a cota de 6/h, entao cada um leva sua
  // hora no relogio injetado.
  const { sqlite, env } = cenario(t);
  let n = 0;
  const base = () => {
    const p = pacoteEmitido();
    p.erro.fingerprint = `webos-4-starfish-ums_${230 + (n++)}-mkv-hevc10-dv-eac3`;
    p.erro.fp = fpDe(p.erro.fingerprint);
    return p;
  };
  for (const tok of ["busca", "proxima", "mem_baixa", "mpv"]) {
    const linha = base(); linha.linha[0].k = tok;
    assert.equal((await chamadaFarol(env, linha, 1700000000 + n * 3600)).status, 200, `linha[].k ${tok}`);
    const nativo = base(); nativo.erro.nativo = tok;
    assert.equal((await chamadaFarol(env, nativo, 1700000000 + n * 3600)).status, 200, `erro.nativo ${tok}`);
  }
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 8);
});

test("reenvio concorrente com 429 nao efetiva registro acima da cota", async (t) => {
  const { sqlite, env } = cenario(t);
  const t0 = 86400100;
  const pacoteN = (i) => {
    const p = pacoteEmitido();
    p.erro.fingerprint = `webos-4-starfish-ums_${240 + i}-mkv-hevc10-dv-eac3`;
    p.erro.fp = fpDe(p.erro.fingerprint);
    return p;
  };
  for (let i = 0; i < 6; i++)
    assert.equal((await chamadaFarol(env, pacoteN(i), t0)).status, 200, "envio " + i);
  // O setimo reclama o slot e fica PENDENTE, travado na reserva da cota: e o
  // instante em que a versao anterior ja expunha a linha ao dedupe. O reenvio
  // do mesmo fingerprint nao pode virar 200 — espera e sai 409 para a TV
  // reenviar depois.
  let soltarCota;
  const trava = new Promise((r) => { soltarCota = r; });
  const limSegurado = async (e, chave, max, jan, tt) => {
    if (String(chave).startsWith("farol:h:")) await trava;
    return limitar(e, chave, max, jan, tt);
  };
  const primeiro = chamadaFarol(env, pacoteN(6), t0, limSegurado);
  const reenvio = await chamadaFarol(env, pacoteN(6), t0);
  soltarCota();
  const recusado = await primeiro;
  assert.equal(recusado.status, 429);
  assert.notEqual(reenvio.status, 200, "reenvio de linha pendente nao vira 200");
  // Seis linhas, cotas nos seis debitados: a recusa nao deixa linha, nao
  // incrementa vezes de ninguem e devolve o debito que tentou.
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 6);
  assert.deepEqual(
    q(sqlite, "SELECT n FROM limite WHERE chave LIKE 'farol:%' ORDER BY chave").map((r) => r.n),
    [6, 6, 6]);
  // E o reenvio DEPOIS do 429 continua recusado na cota, sem deixar linha.
  assert.equal((await chamadaFarol(env, pacoteN(6), t0)).status, 429);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 6);
});

test("ocorrencia apos 24 h gera registro novo e o 429 nunca toca o recibo anterior", async (t) => {
  const { sqlite, env } = cenario(t);
  const corpo = pacoteEmitido();
  const t0 = 1700000100;
  const original = await chamadaFarol(env, corpo, t0);
  assert.equal(original.status, 200);
  const idOriginal = q(sqlite, "SELECT id FROM farol_registro")[0].id;
  // 48 h depois com a cota DIARIA cheia: o 429 nao pode apagar nem
  // sobrescrever a linha do recibo original (ainda com 28 dias de retencao).
  sqlite.prepare("UPDATE limite SET n = 20, janela = ? WHERE chave LIKE 'farol:d:%'")
    .run(Math.floor((t0 + 48 * 3600) / 86400));
  const recusado = await chamadaFarol(env, corpo, t0 + 48 * 3600);
  assert.equal(recusado.status, 429);
  const depois = q(sqlite, "SELECT id, criado, vezes, json FROM farol_registro");
  assert.equal(depois.length, 1);
  assert.equal(depois[0].id, idOriginal);
  assert.equal(depois[0].criado, t0);
  assert.equal(depois[0].vezes, 1);
  assert.equal(JSON.parse(depois[0].json).erro.fingerprint, corpo.erro.fingerprint);
  // Com a cota livre, a mesma falha passadas 24 h e um REGISTRO NOVO: id e
  // codigo novos, a linha anterior intacta com o prazo original.
  sqlite.exec("DELETE FROM limite");
  const nova = await chamadaFarol(env, corpo, t0 + 49 * 3600);
  assert.equal(nova.status, 200);
  assert.notEqual(nova.body.codigo, original.body.codigo);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 2);
  const antiga = q(sqlite, "SELECT criado FROM farol_registro WHERE id = ?", idOriginal);
  assert.equal(antiga[0].criado, t0);
});

// --- correcao farol5d ---------------------------------------------------------
// Os quatro bloqueios da revisao r4: o schema.sql fundia janelas distintas e
// apagava recibos, a 019 rebaixava a sequencia depois da limpeza, uma falha
// transitoria na confirmacao prendia o reenvio em 409 e um POST atrasado
// reabria a cota da hora nova.
const aplicaSql = (sqlite, arquivo) => {
  for (const cmd of readFileSync(new URL(arquivo, import.meta.url), "utf8").split(/;\s*\n/)) {
    const c = cmd.replace(/^\s*--.*$/gm, "").trim();
    if (c) sqlite.exec(c);
  }
};

test("schema.sql no banco legado funde so a mesma janela: nenhum recibo perdido", (t) => {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  // A 018 de 705a519ac: sem `janela` e sem farol_dedup. As seis ocorrencias
  // da revisao no MESMO par — 0h, 12h, 24h-1s, 24h, 36h e 48h — mais um
  // segundo fingerprint e uma segunda pessoa que nao podem ser tocados. Na
  // ordem de preparar-local.sh o schema roda ANTES das migracoes, e era o
  // corte dele que apagava os recibos de 24 h e 48 h.
  sqlite.exec(`CREATE TABLE farol_registro (
      id INTEGER PRIMARY KEY AUTOINCREMENT, criado INTEGER NOT NULL,
      pessoa_h TEXT NOT NULL, versao TEXT NOT NULL, plataforma TEXT NOT NULL,
      so_maior INTEGER NOT NULL DEFAULT 0, codigo TEXT NOT NULL,
      fp TEXT NOT NULL, fingerprint TEXT NOT NULL,
      vezes INTEGER NOT NULL DEFAULT 1, json TEXT NOT NULL);
    CREATE INDEX farol_fp ON farol_registro (fp, criado);
    CREATE INDEX farol_criado ON farol_registro (criado);
    CREATE TABLE farol_conhecido (id INTEGER PRIMARY KEY AUTOINCREMENT,
      fp TEXT, codigo TEXT, plataforma TEXT, so_maior INTEGER,
      estado TEXT NOT NULL, versao TEXT NOT NULL DEFAULT '',
      nota_pt TEXT NOT NULL DEFAULT '', nota_en TEXT NOT NULL DEFAULT '');`);
  sqlite.exec(`INSERT INTO farol_registro
      (id,criado,pessoa_h,versao,plataforma,so_maior,codigo,fp,fingerprint,vezes,json) VALUES
      (536870913,172800000,'p1','1-ipk','webos',4,'NENHUM','a','fp-a',2,'{"caso":"ancora"}'),
      (536870914,172843200,'p1','1-ipk','webos',4,'NENHUM','a','fp-a',3,'{"caso":"reenvio12h"}'),
      (536870915,172886399,'p1','1-ipk','webos',4,'NENHUM','a','fp-a',5,'{"caso":"reenvio24h-1"}'),
      (536870916,172886400,'p1','1-ipk','webos',4,'NENHUM','a','fp-a',7,'{"caso":"novo24h"}'),
      (536870917,172929600,'p1','1-ipk','webos',4,'NENHUM','a','fp-a',11,'{"caso":"reenvio36h"}'),
      (536870918,172972800,'p1','1-ipk','webos',4,'NENHUM','a','fp-a',13,'{"caso":"novo48h"}'),
      (536870919,172972800,'p1','1-ipk','webos',4,'NENHUM','b','fp-b',17,'{"caso":"outrofp"}'),
      (536870920,172972800,'p2','1-ipk','webos',4,'NENHUM','a','fp-a',19,'{"caso":"outrapessoa"}');`);
  aplicaSql(sqlite, "schema.sql");
  // Ja no schema: a janela ancora em 0h, em 24h e em 48h — os recibos
  // 536870916 e 536870918 NAO podem sumir nem perder o JSON.
  const depoisSchema = q(sqlite, "SELECT id FROM farol_registro ORDER BY id").map((r) => r.id);
  assert.deepEqual(depoisSchema, [536870913, 536870916, 536870918, 536870919, 536870920]);
  aplicaSql(sqlite, "migracao-018-farol.sql");
  aplicaSql(sqlite, "migracao-019-farol-janela.sql");
  const linhas = q(sqlite, "SELECT id, vezes, json FROM farol_registro ORDER BY id");
  assert.deepEqual(linhas.map((r) => [r.id, r.vezes]),
    [[536870913, 10], [536870916, 18], [536870918, 13], [536870919, 17], [536870920, 19]]);
  assert.equal(JSON.parse(linhas[0].json).caso, "ancora");
  assert.equal(JSON.parse(linhas[2].json).caso, "novo48h");
  const slot = q(sqlite, "SELECT registro FROM farol_dedup WHERE pessoa_h = 'p1' AND fp = 'a'")[0];
  assert.equal(slot.registro, 536870918);
});

test("a 019 reaplicada apos a limpeza nao reemite id de recibo ja apagado", (t) => {
  const { sqlite } = cenario(t);
  // Como na retencao de 30 dias: o maior id emitido saiu do banco, mas a
  // sequencia ainda lembra dele. A 019 antiga a refazia so do MAX(id)
  // presente e o INSERT seguinte dava 536870924 para outra pessoa — mesmo
  // codigo de recibo (50WAN3), JSON diferente.
  sqlite.exec(`INSERT INTO farol_registro (id, criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, vezes, json)
    VALUES (536870923, 172800000, 'p1', 'v', 'p', 'NENHUM', 'a', 'fp', 1, '{}'),
           (536870924, 172972800, 'p2', 'v', 'p', 'NENHUM', 'b', 'fp', 1, '{}');
    DELETE FROM farol_registro WHERE id = 536870924;`);
  const antes = q(sqlite, "SELECT seq FROM sqlite_sequence WHERE name = 'farol_registro'")[0].seq;
  assert.equal(antes, 536870924);
  aplicaSql(sqlite, "migracao-019-farol-janela.sql");
  aplicaSql(sqlite, "migracao-019-farol-janela.sql");
  const seq = q(sqlite, "SELECT seq FROM sqlite_sequence WHERE name = 'farol_registro'")[0].seq;
  assert.ok(seq >= antes, `sequencia ${seq} ficou atras de ${antes}`);
  const novo = sqlite.prepare(
    "INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, codigo, fp, fingerprint, vezes, json) " +
    "VALUES (173000000, 'p3', 'v', 'p', 'NENHUM', 'c', 'fp', 1, '{}') RETURNING id").get();
  assert.ok(novo.id > 536870924, `o id ${novo.id} ja tinha sido emitido`);
});

test("falha transitoria na confirmacao: reenvios adotam a linha e devolvem o mesmo codigo", async (t) => {
  const { sqlite, env } = cenario(t);
  const corpo = pacoteEmitido();
  const t0 = 1700001000;
  const fp16 = fpDe(corpo.erro.fingerprint);
  // O gatilho reproduz a falha de escrita do D1: so o UPDATE de confirmacao
  // morre, depois do INSERT da linha e dos tres debitos de cota.
  sqlite.exec(`CREATE TRIGGER sonda_falha BEFORE UPDATE OF confirmado ON farol_dedup
    WHEN NEW.confirmado = 1 AND NEW.fp = '${fp16}'
    BEGIN SELECT RAISE(ABORT, 'sonda: confirmacao interrompida'); END;`);
  let interrompido;
  try {
    interrompido = await chamadaFarol(env, corpo, t0);
  } finally {
    sqlite.exec("DROP TRIGGER sonda_falha");
  }
  assert.equal(interrompido.status, 503);
  // O estado deixado e o da r4: linha gravada, slot pendente sem `registro`.
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 1);
  const pendente = q(sqlite, "SELECT registro, confirmado FROM farol_dedup")[0];
  assert.equal(pendente.registro, null);
  assert.equal(pendente.confirmado, 0);
  // Dois reenvios em paralelo: ambos adotam a linha orfa e saem com o MESMO
  // recibo que o envio original devolveria — nenhum 409, nenhum debito novo.
  const [r1, r2] = await Promise.all(
    [chamadaFarol(env, corpo, t0 + 5), chamadaFarol(env, corpo, t0 + 6)]);
  assert.equal(r1.status, 200);
  assert.equal(r2.status, 200);
  const linha = q(sqlite, "SELECT id, vezes FROM farol_registro")[0];
  assert.equal(r1.body.codigo, codigoRegistro(linha.id));
  assert.equal(r2.body.codigo, codigoRegistro(linha.id));
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 1);
  const slot = q(sqlite, "SELECT registro, confirmado FROM farol_dedup")[0];
  assert.equal(slot.registro, linha.id);
  assert.equal(slot.confirmado, 1);
  assert.deepEqual(
    q(sqlite, "SELECT n FROM limite WHERE chave LIKE 'farol:%' ORDER BY chave").map((r) => r.n),
    [1, 1, 1]);
  // E um fingerprint diferente continua gravando normal (o D1 "voltou").
  const outro = pacoteEmitido();
  outro.erro.fingerprint = "webos-4-starfish-ums_299-mkv-hevc10-dv-eac3";
  outro.erro.fp = fpDe(outro.erro.fingerprint);
  assert.equal((await chamadaFarol(env, outro, t0 + 7)).status, 200);
});

test("pedido da hora anterior em voo nao reabre a cota da hora nova", async (t) => {
  const { sqlite, env } = cenario(t);
  const horaNova = 1700002800;
  assert.equal(horaNova % 3600, 0);
  const pacoteN = (i) => {
    const p = pacoteEmitido();
    p.erro.fingerprint = `webos-4-starfish-ums_${250 + i}-mkv-hevc10-dv-eac3`;
    p.erro.fp = fpDe(p.erro.fingerprint);
    return p;
  };
  // O primeiro pedido captura `t` um segundo ANTES da virada e fica segurado
  // na reserva da cota ate os seis envios da hora nova completarem — a ordem
  // real de um POST atrasado no Workers.
  let soltar, chegou;
  const trava = new Promise((r) => { soltar = r; });
  const espera = new Promise((r) => { chegou = r; });
  const limSegurado = async (e, chave, max, jan, tt) => {
    if (String(chave).startsWith("farol:h:")) { chegou(); await trava; }
    return limitar(e, chave, max, jan, tt);
  };
  const antigo = chamadaFarol(env, pacoteN(0), horaNova - 1, limSegurado);
  await espera;
  for (let i = 1; i <= 6; i++)
    assert.equal((await chamadaFarol(env, pacoteN(i), horaNova)).status, 200, "envio " + i);
  soltar();
  const tardio = await antigo;
  // O atrasado conta na janela VIGENTE: e o setimo debito da hora nova —
  // recusado — e a janela nunca volta para a dele (na versao anterior ela
  // voltava e o setimo da hora nova gravava a setima linha).
  assert.equal(tardio.status, 429);
  assert.equal(
    q(sqlite, "SELECT janela FROM limite WHERE chave LIKE 'farol:h:%'")[0].janela,
    Math.floor(horaNova / 3600));
  const setimo = await chamadaFarol(env, pacoteN(7), horaNova + 1);
  assert.equal(setimo.status, 429);
  assert.equal(
    q(sqlite, "SELECT count(*) AS n FROM farol_registro WHERE criado >= ?", horaNova)[0].n, 6);
});

// --- correcao farol5e ---------------------------------------------------------
// O bloqueio da revisao r5 (sonda trocarSlot com barreiras no D1): um reenvio
// da janela anterior, pendurado na resposta do UPSERT, acordava depois de um
// POST legitimo retomar o slot e gravar a ocorrencia nova. A adocao com
// "criado >=" escolhia a linha NOVA e a confirmacao era forjada em memoria
// com o UPDATE descartado — 200 com o codigo alheio, vencimento 24 h antes
// do certo e vezes++ no recibo dos outros. No pai o reenvio sai 409.

test("troca de slot: reenvio da janela anterior nao adota o recibo da ocorrencia nova", async (t) => {
  const { sqlite, env } = cenario(t);
  const corpo = pacoteEmitido();
  const t0 = 20000 * 86400;
  // 1. O envio original grava a linha e morre na confirmacao (gatilho no D1
  //    falso, como a falha injetada da sonda): 503, linha orfa, slot pendente.
  sqlite.exec(`CREATE TRIGGER sonda_falha BEFORE UPDATE OF confirmado ON farol_dedup
    WHEN NEW.confirmado = 1 BEGIN SELECT RAISE(ABORT, 'sonda: confirmacao interrompida'); END;`);
  let primeiro;
  try { primeiro = await chamadaFarol(env, corpo, t0); }
  finally { sqlite.exec("DROP TRIGGER sonda_falha"); }
  assert.equal(primeiro.status, 503);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM farol_registro")[0].n, 1);
  // 2. O reenvio em t0+24h-1s reclama o slot pendente (toques=2, criado=t0)
  //    e fica com a RESPOSTA do UPSERT pendurada — a barreira da sonda.
  let seguraReenvio = true, marcouReenvio, soltaReenvio;
  const reenvioNoSlot = new Promise((r) => { marcouReenvio = r; });
  const travaReenvio = new Promise((r) => { soltaReenvio = r; });
  d1EmPar(env.DB, (sql) => sql.startsWith("INSERT INTO farol_dedup"), async () => {
    if (seguraReenvio) { seguraReenvio = false; marcouReenvio(); await travaReenvio; }
  });
  const emReenvio = chamadaFarol(env, corpo, t0 + 86399);
  await reenvioNoSlot;
  // 3. O POST legitimo em t0+24h retoma o slot (criado=t0+86400), grava a
  //    linha da ocorrencia nova e fica com a confirmacao pendurada.
  let seguraNovo = true, marcouNovo, soltaNovo;
  const novoNaConfirmacao = new Promise((r) => { marcouNovo = r; });
  const travaNovo = new Promise((r) => { soltaNovo = r; });
  const preparar = env.DB.prepare.bind(env.DB);
  env.DB.prepare = (sql) => {
    const stm = preparar(sql);
    if (!sql.startsWith("UPDATE farol_dedup")) return stm;
    const bind = stm.bind.bind(stm);
    stm.bind = (...args) => {
      const bound = bind(...args);
      const run = bound.run.bind(bound);
      bound.run = async () => {
        if (seguraNovo) { seguraNovo = false; marcouNovo(); await travaNovo; }
        return run();
      };
      return bound;
    };
    return stm;
  };
  const emNovo = chamadaFarol(env, corpo, t0 + 86400);
  await novoNaConfirmacao;
  // 4. Libera o reenvio antigo com o slot ja retomado e ainda pendente:
  //    NUNCA 200 com o recibo da ocorrencia nova nem vezes++ na linha dela.
  soltaReenvio();
  const reenvio = await emReenvio;
  const durante = q(sqlite, "SELECT id, criado, vezes FROM farol_registro ORDER BY criado");
  soltaNovo();
  const novo = await emNovo;
  assert.equal(reenvio.status, 409, JSON.stringify(reenvio));
  assert.equal(durante.length, 2);
  assert.equal(durante[0].criado, t0);
  assert.equal(durante[0].vezes, 1);
  assert.equal(durante[1].criado, t0 + 86400);
  assert.equal(durante[1].vezes, 1, "o reenvio nao incrementa o recibo alheio");
  // O POST legitimo conclui com o proprio recibo e o vencimento dele.
  assert.equal(novo.status, 200);
  assert.equal(novo.body.codigo, codigoRegistro(durante[1].id));
  assert.equal(novo.body.vence, t0 + 86400 + 30 * 86400);
  const slot = q(sqlite, "SELECT criado, registro, confirmado FROM farol_dedup")[0];
  assert.equal(slot.criado, t0 + 86400);
  assert.equal(slot.registro, durante[1].id);
  assert.equal(slot.confirmado, 1);
  // E o reenvio seguinte ja cai no dedup normal da ocorrencia confirmada.
  const depois = await chamadaFarol(env, corpo, t0 + 86400 + 5);
  assert.equal(depois.status, 200);
  assert.equal(depois.body.codigo, novo.body.codigo);
  assert.equal(depois.body.vence, novo.body.vence);
});
