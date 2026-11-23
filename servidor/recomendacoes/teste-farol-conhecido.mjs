// Farol issue 13: tabela `farol_conhecido` (migracao-020 + schema.sql), ordem
// de match fp -> (codigo, plataforma, so_maior) -> (codigo, plataforma),
// `corrigido` somente quando versao do cliente < versao da correcao, rota
// GET /v1/farol/conhecido, pagina publica GET /v1/farol/r/<codigo> (sem
// autenticacao, sem vazar nenhum campo do registro e com 404 identico para
// inexistente e vencido) e ferramentas tools/farol-{conhecido,ler,top}.mjs
// sem jamais imprimir `pessoa_h`.
//   node servidor/recomendacoes/teste-farol-conhecido.mjs
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import { fileURLToPath } from "node:url";
import worker from "./src/index.js";
import { codigoRegistro, decodificarCodigo } from "./src/codigo.js";
import { FAROL_RETENCAO, versaoMenor } from "./src/farol.js";

const MIGRACOES = [
  "schema.sql",
  "migracao-001-avatar-nota.sql",
  "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql",
  "migracao-004-triagem.sql",
  "migracao-005-amigos.sql",
  "migracao-006-social.sql",
  "migracao-007-resposta.sql",
  "migracao-008-identidade.sql",
  "migracao-018-farol.sql",
  "migracao-019-farol-janela.sql",
  "migracao-020-farol-conhecido.sql",
];

const CLI_CONHECIDO = fileURLToPath(new URL("../../tools/farol-conhecido.mjs", import.meta.url));
const CLI_LER = fileURLToPath(new URL("../../tools/farol-ler.mjs", import.meta.url));
const CLI_TOP = fileURLToPath(new URL("../../tools/farol-top.mjs", import.meta.url));
const TMP_BASE = fileURLToPath(new URL(".wrangler", import.meta.url));

const agora = () => Math.floor(Date.now() / 1000);
const fpDe = (s) => createHash("sha256").update(s).digest("hex").slice(0, 16);

function aplicarSqlArquivo(sqlite, nome) {
  for (const cmd of readFileSync(new URL(nome, import.meta.url), "utf8").split(/;\s*\n/)) {
    const c = cmd.replace(/^\s*--.*$/gm, "").trim();
    if (!c) continue;
    try { sqlite.exec(c); } catch (e) { if (!/duplicate column/.test(String(e.message))) throw e; }
  }
}

function prepararSqlite(sqlite) {
  for (const m of MIGRACOES) aplicarSqlArquivo(sqlite, m);
  sqlite.prepare("INSERT OR REPLACE INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update("nuvio:tok-farol").digest("hex"), "nuvio:pessoa-farol", "");
}

function d1DeSqlite(sqlite) {
  const stmt = (sql, valores) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...valores) ?? null,
      all: async () => ({ results: st.all(...valores) }),
      run: async () => {
        const r = st.run(...valores);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } };
      },
      _exec: () => st.run(...valores),
    };
  };
  return {
    prepare(sql) {
      return { ...stmt(sql, []), bind: (...v) => stmt(sql, v) };
    },
    async batch(lista) {
      sqlite.exec("BEGIN");
      try { for (const s of lista) s._exec(); sqlite.exec("COMMIT"); }
      catch (e) { sqlite.exec("ROLLBACK"); throw e; }
      return [];
    },
  };
}

function cenario(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  prepararSqlite(sqlite);
  const DB = d1DeSqlite(sqlite);
  const env = {
    DB,
    FAROL_SAL: "sal-sintetico-13",
    SUPABASE_URL: "https://sb.test",
    SUPABASE_ANON_KEY: "anon",
    TRAKT_CLIENT_ID: "tc",
  };
  const post = async (corpo, { token = "tok-farol", rota = "/v1/farol/registro" } = {}) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" };
    const res = await worker.fetch(new Request("https://w.test" + rota, {
      method: "POST",
      headers,
      body: typeof corpo === "string" ? corpo : JSON.stringify(corpo),
    }), env);
    return { status: res.status, body: await res.json().catch(() => null) };
  };
  const getConhecido = async (query, { token = "tok-farol", metodo = "GET" } = {}) => {
    const headers = token ? { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" } : {};
    const qs = query ? (query.startsWith("?") ? query : "?" + query) : "";
    const res = await worker.fetch(new Request("https://w.test/v1/farol/conhecido" + qs, {
      method: metodo,
      headers,
    }), env);
    return { status: res.status, body: await res.json().catch(() => null) };
  };
  const getPagina = async (codigo, { metodo = "GET" } = {}) => {
    const res = await worker.fetch(new Request("https://w.test/v1/farol/r/" + codigo, { method: metodo }), env);
    const headers = Object.fromEntries([...res.headers.entries()].sort(([a], [b]) => a.localeCompare(b)));
    return { status: res.status, headers, text: await res.text() };
  };
  return { sqlite, DB, env, post, getConhecido, getPagina };
}

function pacote({
  fingerprint = "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3",
  codigo = "TV_FORMATO_REPETE",
  plataforma = "webos",
  soMaior = 4,
  versao = "2.1.0-ipk",
  modelo = "OLED55C9PSA",
  commit = "c18c21756",
} = {}) {
  return {
    v: 1,
    gatilho: "falha_sem_conserto",
    sintoma: "nao_comeca",
    erro: {
      codigo,
      estagio: "tv",
      nativo: "ums_204",
      nativo_num: 204,
      http: 0,
      fingerprint,
      fp: fpDe(fingerprint),
      tentou: [{ acao: "proxima_fonte", ok: false, ms: 6400 }],
    },
    aparelho: {
      plataforma,
      so: "4.9-webos",
      so_maior: soMaior,
      modelo,
      ram_total_mb: 1870,
      ram_livre_mb: 212,
      tela: { w: 3840, h: 2160, hdr: 1, dv: 1, hevc: 1, av1: -1 },
      audio: { dts: 1, truehd: -1, eac3: 1, ac3: 1 },
      rede: { offline: false, hosts_com_falha: 0, vazao_mbps: 0 },
    },
    cliente: {
      versao,
      commit,
      build: "webos-ipk",
      backend: "starfish",
      idioma: "pt-BR",
      aberto_s: 1842,
      gpu: "auto:alto",
      ajustes: { glass_ui: 1, qualidade_max: "4k", tamanho_max_gb: 40, dts: "ac3", dv_mkv: 0, legenda_auto_sync: 1 },
      addons: { total: 12, ativos: 11, isolados: 1, tipos: ["catalogo", "fontes", "legendas"] },
      debrid: ["torbox"],
      contas: ["trakt", "simkl"],
    },
    midia: {
      container: "mkv", video: "hevc10", hdr: "dv", res: "2160p", fps: 23.976,
      mbps: 24.6, gb: 14.2, audio: "eac3_51", legenda: "srt_embutida",
      entrega: "debrid_cache", http: 206, ttfb_ms: 210,
    },
    cadeia: [{ passo: "internet", estado: "ok" }, { passo: "tv", estado: "falhou" }],
    linha: [{ t: -1950, m: "video", e: "erro_nativo", a: 204, b: 0, k: "ums_204" }],
    privacidade: { redator: 1, campos_recusados: 0 },
  };
}

function pastaTmp(t) {
  mkdirSync(TMP_BASE, { recursive: true });
  const dir = mkdtempSync(join(TMP_BASE, "farol13-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function rodarNode(script, args, envExtra = {}) {
  try {
    const stdout = execFileSync(process.execPath, [script, ...args], {
      encoding: "utf8",
      env: { ...process.env, ...envExtra },
    });
    return { status: 0, stdout, stderr: "" };
  } catch (e) {
    return {
      status: e.status ?? 1,
      stdout: String(e.stdout ?? ""),
      stderr: String(e.stderr ?? ""),
    };
  }
}

test("migracao-020 e schema.sql criam farol_conhecido e indices de forma idempotente", (t) => {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  aplicarSqlArquivo(sqlite, "migracao-020-farol-conhecido.sql");
  aplicarSqlArquivo(sqlite, "migracao-020-farol-conhecido.sql");
  aplicarSqlArquivo(sqlite, "schema.sql");
  const idx = sqlite.prepare("SELECT name FROM sqlite_schema WHERE type = 'index' AND tbl_name = 'farol_conhecido'")
    .all().map((r) => r.name);
  assert.ok(idx.includes("farol_conhecido_fp"));
  assert.ok(idx.includes("farol_conhecido_cod"));
});

test("ordem de match: fp exato -> (codigo, plataforma, so_maior) -> (codigo, plataforma)", async (t) => {
  const { sqlite, post, getConhecido } = cenario(t);
  const fpA = fpDe("webos-4-starfish-ums_204-mkv-hevc10-dv-eac3");
  const fpOutroSo4 = fpDe("webos-4-starfish-ums_205-mkv-hevc10-dv-eac3");
  const fpSo5 = fpDe("webos-5-starfish-ums_205-mkv-hevc10-dv-eac3");
  const fpAndroid = fpDe("android-14-media3-m3_2004-mkv-hevc10-dv-eac3");

  // Insere na ordem INVERSA da prioridade para provar que a prioridade nao vem
  // da ordem fisica nem do id maior, e sim de fp -> (cod,plat,so) -> (cod,plat).
  const ins = sqlite.prepare(
    "INSERT INTO farol_conhecido (fp, codigo, plataforma, so_maior, estado, versao, nota_pt, nota_en) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
  );
  ins.run(fpA, null, null, null, "corrigido", "2.1.1", "por fp", "by fp");
  ins.run(null, "TV_FORMATO_REPETE", "webos", 4, "contorno", "", "por cod+plat+so4", "by cod+plat+so4");
  ins.run(null, "TV_FORMATO_REPETE", "webos", null, "conhecido", "", "por cod+plat", "by cod+plat");

  // 1) Quando os tres casariam, vence `fp` exato (nivel 1).
  const g1 = await getConhecido(`fp=${fpA}&c=TV_FORMATO_REPETE&p=webos&so=4&v=2.1.0-ipk`);
  assert.equal(g1.status, 200);
  assert.deepEqual(g1.body.conhecido, {
    estado: "corrigido", versao: "2.1.1", nota_pt: "por fp", nota_en: "by fp",
  });
  const p1 = await post(pacote({
    fingerprint: "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3",
    soMaior: 4, versao: "2.1.0-ipk",
  }));
  assert.equal(p1.status, 200);
  assert.equal(p1.body.conhecido?.nota_pt, "por fp");
  // Reenvio deduplicado tambem devolve `conhecido`.
  const p1Dedup = await post(pacote({
    fingerprint: "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3",
    soMaior: 4, versao: "2.1.0-ipk",
  }));
  assert.equal(p1Dedup.body.dedup, 1);
  assert.equal(p1Dedup.body.conhecido?.nota_pt, "por fp");

  // 2) Outro fp no mesmo (codigo, plataforma=webos, so_maior=4): vence nivel 2.
  const g2 = await getConhecido(`fp=${fpOutroSo4}&c=TV_FORMATO_REPETE&p=webos&so_maior=4&v=2.1.0-ipk`);
  assert.equal(g2.body.conhecido?.nota_pt, "por cod+plat+so4");
  const p2 = await post(pacote({
    fingerprint: "webos-4-starfish-ums_205-mkv-hevc10-dv-eac3",
    soMaior: 4, versao: "2.1.0-ipk",
  }));
  assert.equal(p2.body.conhecido?.nota_pt, "por cod+plat+so4");

  // 3) Mesmo (codigo, plataforma=webos) mas so_maior=5 (ou sem so_maior): cai no nivel 3.
  const g3 = await getConhecido(`fp=${fpSo5}&c=TV_FORMATO_REPETE&p=webos&so=5&v=2.1.0-ipk`);
  assert.equal(g3.body.conhecido?.nota_pt, "por cod+plat");
  const g3SemSo = await getConhecido(`fp=${fpSo5}&c=TV_FORMATO_REPETE&p=webos&v=2.1.0-ipk`);
  assert.equal(g3SemSo.body.conhecido?.nota_pt, "por cod+plat");
  const p3 = await post(pacote({
    fingerprint: "webos-5-starfish-ums_205-mkv-hevc10-dv-eac3",
    soMaior: 5, versao: "2.1.0-ipk",
  }));
  assert.equal(p3.body.conhecido?.nota_pt, "por cod+plat");

  // 4) Outra plataforma (android) nao casa com regra de webos: sem `conhecido`.
  const g4 = await getConhecido(`fp=${fpAndroid}&c=TV_FORMATO_REPETE&p=android&so=14&v=2.1.0-apk`);
  assert.equal(g4.status, 200);
  assert.equal(g4.body.conhecido, null);
  const p4 = await post({
    ...pacote({
      fingerprint: "android-14-media3-m3_2004-mkv-hevc10-dv-eac3",
      plataforma: "android", soMaior: 14, versao: "2.1.0-apk", modelo: "TCL55C825",
    }),
    aparelho: {
      ...pacote().aparelho,
      plataforma: "android", so: "14-android", so_maior: 14, modelo: "TCL55C825",
    },
    cliente: {
      ...pacote().cliente,
      versao: "2.1.0-apk", build: "android-apk", backend: "media3",
    },
    erro: {
      ...pacote().erro,
      nativo: "m3_2004", nativo_num: 2004,
      fingerprint: "android-14-media3-m3_2004-mkv-hevc10-dv-eac3",
      fp: fpAndroid,
    },
  });
  assert.equal(p4.status, 200);
  assert.equal("conhecido" in p4.body, false);
});

test("corrigido so responde para versao estritamente menor; igual ou maior nao responde", async (t) => {
  assert.equal(versaoMenor("2.1.0-ipk", "2.1.2"), true);
  assert.equal(versaoMenor("2.1.2-ipk", "2.1.2"), false);
  assert.equal(versaoMenor("2.1.3-ipk", "2.1.2"), false);
  assert.equal(versaoMenor("2.2.0-ipk", "2.10.0"), true);
  assert.equal(versaoMenor("2.10.0-ipk", "2.2.0"), false);
  assert.equal(versaoMenor("2.1-webos", "2.1.0"), false);
  assert.equal(versaoMenor("2.1-webos", "2.1.1"), true);
  assert.equal(versaoMenor("", "2.1.2"), false);
  assert.equal(versaoMenor("dev", "2.1.2"), false);

  const { sqlite, post, getConhecido } = cenario(t);
  const fp = fpDe("webos-4-starfish-ums_204-mkv-hevc10-dv-eac3");
  // Mesmo com uma regra generica em 2.1.5, a regra exata de `fp` corrigida em
  // 2.1.2 governa aquele fp: cliente em 2.1.2 NAO recebe falso "corrigido".
  const ins = sqlite.prepare(
    "INSERT INTO farol_conhecido (fp, codigo, plataforma, so_maior, estado, versao, nota_pt) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?)"
  );
  ins.run(fp, null, null, null, "corrigido", "2.1.2", "fix 2.1.2");
  ins.run(null, "TV_FORMATO_REPETE", "webos", null, "corrigido", "2.1.5", "fix amplo");

  // Menor (2.1.1 < 2.1.2): responde corrigido.
  const menor = await getConhecido(`fp=${fp}&c=TV_FORMATO_REPETE&p=webos&v=2.1.1-ipk`);
  assert.equal(menor.body.conhecido?.estado, "corrigido");
  assert.equal(menor.body.conhecido?.versao, "2.1.2");

  // Igual (2.1.2 == 2.1.2): NAO responde corrigido.
  const igual = await getConhecido(`fp=${fp}&c=TV_FORMATO_REPETE&p=webos&v=2.1.2-ipk`);
  assert.equal(igual.body.conhecido, null);
  const postIgual = await post(pacote({ versao: "2.1.2-ipk" }));
  assert.equal(postIgual.status, 200);
  assert.equal("conhecido" in postIgual.body, false);

  // Maior (2.1.3 > 2.1.2) e sem versao: NAO responde corrigido.
  assert.equal((await getConhecido(`fp=${fp}&c=TV_FORMATO_REPETE&p=webos&v=2.1.3-ipk`)).body.conhecido, null);
  assert.equal((await getConhecido(`fp=${fp}&c=TV_FORMATO_REPETE&p=webos&v=`)).body.conhecido, null);

  // `conhecido` e `contorno` valem em qualquer versao.
  const fpContorno = fpDe("webos-4-starfish-ums_208-mkv-hevc10-dv-eac3");
  ins.run(fpContorno, null, null, null, "contorno", "", "Desligue o DV neste video");
  const cont = await getConhecido(`fp=${fpContorno}&v=2.9.9-ipk`);
  assert.equal(cont.body.conhecido?.estado, "contorno");
  assert.equal(cont.body.conhecido?.nota_pt, "Desligue o DV neste video");
});

test("GET /v1/farol/conhecido exige conta, valida parametros e nao grava nada", async (t) => {
  const { sqlite, getConhecido } = cenario(t);
  assert.equal((await getConhecido("fp=0123456789abcdef&v=2.1.0", { token: "" })).status, 401);
  assert.equal((await getConhecido("fp=0123456789abcdef&v=2.1.0", { token: "invalido" })).status, 401);
  assert.equal((await getConhecido("fp=0123456789abcdef&v=2.1.0", { metodo: "POST" })).status, 405);
  assert.equal((await getConhecido("")).status, 400);
  assert.equal((await getConhecido("fp=nao-e-hex")).status, 400);
  assert.equal((await getConhecido("c=TV_FORMATO")).status, 400); // sem plataforma e sem fp
  assert.equal((await getConhecido("c=TV_FORMATO&p=webos&so=abc")).status, 400);

  const ok = await getConhecido("fp=0123456789abcdef&c=TV_FORMATO&p=webos&v=2.1.0-ipk");
  assert.equal(ok.status, 200);
  assert.deepEqual(ok.body, { ok: 1, conhecido: null });
  // Zero efeitos colaterais: nem pessoa, nem farol_registro, nem limite.
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM pessoa").get().n, 0);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM farol_registro").get().n, 0);
  assert.equal(sqlite.prepare("SELECT count(*) AS n FROM limite").get().n, 0);
});

test("GET /v1/farol/r/<codigo>: sem auth, sem nenhum campo do registro e 404 identico para inexistente e vencido", async (t) => {
  const { sqlite, post, getPagina } = cenario(t);
  // Pacote com valores distintos em todos os blocos para provar ausencia total na pagina.
  const fpStr = "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3";
  const p = pacote({
    fingerprint: fpStr,
    codigo: "TV_FORMATO_REPETE",
    plataforma: "webos",
    soMaior: 4,
    versao: "2.1.0-ipk",
    modelo: "OLED77G3PSA",
    commit: "9a8b7c6d5",
  });
  const envio = await post(p);
  assert.equal(envio.status, 200);
  const cod = envio.body.codigo;
  const linha = sqlite.prepare("SELECT * FROM farol_registro").get();

  // Publica (sem Authorization): mostra codigo, vencimento e os 3 botoes.
  const pag = await getPagina(cod);
  assert.equal(pag.status, 200);
  assert.match(pag.headers["content-type"], /^text\/html/);
  assert.equal(pag.headers["cache-control"], "no-store");
  assert.equal(pag.headers["referrer-policy"], "no-referrer");
  assert.ok(pag.text.includes(cod));
  assert.ok(pag.text.includes("Vence em"));
  assert.ok(pag.text.includes(new Date(envio.body.vence * 1000).toISOString()));
  assert.ok(pag.text.includes("Copiar"));
  assert.ok(pag.text.includes("Abrir Discord"));
  assert.ok(pag.text.includes("Abrir issue"));

  // Normalizacao de Crockford (minusculas, O->0, I/L->1) abre a mesma pagina canonica.
  const apelido = cod.toLowerCase().replace(/0/g, "o").replace(/1/g, "l");
  const pagNorm = await getPagina(apelido);
  assert.equal(pagNorm.status, 200);
  assert.equal(pagNorm.text, pag.text);

  // Nenhum campo do registro nem metadado interno aparece no HTML ou cabecalhos.
  const proibidos = [
    linha.pessoa_h,
    fpStr,
    p.erro.fp,
    "TV_FORMATO_REPETE",
    "ums_204",
    "OLED77G3PSA",
    "4.9-webos",
    "2.1.0-ipk",
    "9a8b7c6d5",
    "webos-ipk",
    "starfish",
    "hevc10",
    "eac3_51",
    "srt_embutida",
    "debrid_cache",
    "torbox",
    "simkl",
    "falha_sem_conserto",
    "nao_comeca",
    String(linha.id),
    String(linha.criado),
  ];
  const tudo = pag.text + "\n" + JSON.stringify(pag.headers);
  for (const segredo of proibidos) {
    assert.equal(tudo.includes(segredo), false, `vazou '${segredo}' na pagina publica`);
  }

  // Registro de texto legado (id < 2^29) existente no banco tambem responde 404 igual.
  sqlite.prepare("INSERT INTO registro (id, pessoa, versao, plataforma, quando, texto, criado) VALUES (42, 'nuvio:a', '2.1.0', 'webos', 'hoje', 'x', ?)")
    .run(agora());
  const codLegado = codigoRegistro(42);
  const codInexistente = codigoRegistro(536870999);
  assert.ok(decodificarCodigo(codInexistente) >= 536870912);

  // Envelhece o registro do Farol alem dos 30 dias SEM rodar limparFarol: a
  // linha ainda existe na tabela, mas ja venceu.
  sqlite.prepare("UPDATE farol_registro SET criado = ? WHERE id = ?")
    .run(agora() - FAROL_RETENCAO - 1, linha.id);

  const rInexistente = await getPagina(codInexistente);
  const rVencido = await getPagina(cod);
  const rInvalido = await getPagina("INVALIDO");
  const rLegado = await getPagina(codLegado);

  assert.equal(rInexistente.status, 404);
  assert.equal(rVencido.status, 404);
  assert.equal(rInvalido.status, 404);
  assert.equal(rLegado.status, 404);

  // 404 identico em corpo e cabecalhos entre inexistente, vencido, invalido e legado.
  assert.equal(rVencido.text, rInexistente.text);
  assert.deepEqual(rVencido.headers, rInexistente.headers);
  assert.equal(rInvalido.text, rInexistente.text);
  assert.equal(rLegado.text, rInexistente.text);
  assert.equal(rVencido.text.includes(cod), false);
  assert.equal(rInexistente.text.includes(codInexistente), false);
});

test("tools/farol-{conhecido,ler,top}.mjs funcionam no CLI e nunca imprimem pessoa_h", (t) => {
  const dir = pastaTmp(t);
  const arqDb = join(dir, "farol.sqlite");
  const sqlite = new DatabaseSync(arqDb);
  prepararSqlite(sqlite);

  const canarioPessoaH = "canario_pessoa_h_secreto_deadbeefcafe1234567890abcdef";
  const t0 = 1760000000;
  const fp1 = fpDe("webos-4-starfish-ums_204-mkv-hevc10-dv-eac3");
  const fp2 = fpDe("android-14-media3-m3_2004-mkv-hevc10-dv-eac3");
  const fpVelho = fpDe("tizen_wgt-5-avplay-av_invalid_uri-mp4-h264-sdr-aac");

  const pkg1 = pacote({ fingerprint: "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3" });
  const insReg = sqlite.prepare(
    "INSERT INTO farol_registro (id, criado, janela, pessoa_h, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
  );
  insReg.run(536870920, t0 - 3600, Math.floor((t0 - 3600) / 86400), canarioPessoaH,
             "2.1.0-ipk", "webos", 4, "TV_FORMATO_REPETE", fp1, pkg1.erro.fingerprint, 5, JSON.stringify(pkg1));
  insReg.run(536870921, t0 - 7200, Math.floor((t0 - 7200) / 86400), canarioPessoaH,
             "2.1.0-ipk", "webos", 4, "TV_FORMATO_REPETE", fp1, pkg1.erro.fingerprint, 2, JSON.stringify(pkg1));
  insReg.run(536870922, t0 - 1800, Math.floor((t0 - 1800) / 86400), canarioPessoaH,
             "2.1.0-apk", "android", 14, "PLAYER_PAROU", fp2, "android-14-media3-m3_2004-mkv-hevc10-dv-eac3", 3, "{}");
  // Mais de 7 dias atras: fica fora da janela padrao de 7 dias do farol-top.mjs.
  insReg.run(536870923, t0 - 9 * 86400, Math.floor((t0 - 9 * 86400) / 86400), canarioPessoaH,
             "2.0.9-wgt", "tizen_wgt", 5, "FONTE_RECUSOU", fpVelho, "tizen_wgt-5-avplay-av_invalid_uri-mp4-h264-sdr-aac", 99, "{}");
  sqlite.close();

  const semCanario = (r, ctx) => {
    assert.equal(r.stdout.includes(canarioPessoaH), false, `${ctx}: vazou pessoa_h no stdout`);
    assert.equal(r.stderr.includes(canarioPessoaH), false, `${ctx}: vazou pessoa_h no stderr`);
    assert.equal(r.stdout.includes("pessoa_h"), false, `${ctx}: chave pessoa_h no stdout`);
  };

  // --- farol-conhecido.mjs (add/adicionar, list/listar, remove/remover) ---
  const add1 = rodarNode(CLI_CONHECIDO, [
    "--sqlite", arqDb, "add", "--fp", fp1, "--estado", "corrigido", "--versao", "2.1.2",
    "--nota-pt", "Corrigido na 2.1.2?", "--nota-en", "Fixed in 2.1.2?", "--json",
  ]);
  assert.equal(add1.status, 0, add1.stderr);
  semCanario(add1, "conhecido add1");
  const jAdd1 = JSON.parse(add1.stdout);
  assert.equal(jAdd1.id, 1);
  assert.equal(jAdd1.versao, "2.1.2");

  const add2 = rodarNode(CLI_CONHECIDO, [
    "--sqlite", arqDb, "adicionar", "--codigo", "PLAYER_PAROU", "--plataforma", "android",
    "--so-maior", "14", "--estado", "contorno", "--nota-pt", "Aumentar buffer",
  ]);
  assert.equal(add2.status, 0, add2.stderr);
  semCanario(add2, "conhecido add2");

  // `corrigido` sem versao falha com rc != 0.
  const addRuim = rodarNode(CLI_CONHECIDO, ["--sqlite", arqDb, "add", "--fp", fp2, "--estado", "corrigido"]);
  assert.notEqual(addRuim.status, 0);

  const lsJson = rodarNode(CLI_CONHECIDO, ["--sqlite", arqDb, "list", "--json"]);
  assert.equal(lsJson.status, 0);
  semCanario(lsJson, "conhecido list json");
  const lista = JSON.parse(lsJson.stdout);
  assert.equal(lista.length, 2);
  assert.equal(lista[0].fp, fp1);
  assert.equal(lista[1].so_maior, 14);

  const lsTxt = rodarNode(CLI_CONHECIDO, ["--sqlite", arqDb, "listar"]);
  assert.equal(lsTxt.status, 0);
  semCanario(lsTxt, "conhecido listar txt");
  assert.ok(lsTxt.stdout.includes("PLAYER_PAROU/android/14"));

  const rm1 = rodarNode(CLI_CONHECIDO, ["--sqlite", arqDb, "remove", "1"]);
  assert.equal(rm1.status, 0);
  assert.notEqual(rodarNode(CLI_CONHECIDO, ["--sqlite", arqDb, "remover", "1"]).status, 0);
  assert.equal(JSON.parse(rodarNode(CLI_CONHECIDO, ["--sqlite", arqDb, "list", "--json"]).stdout).length, 1);

  // --- farol-ler.mjs ---
  const cod20 = codigoRegistro(536870920);
  const lerOk = rodarNode(CLI_LER, ["--sqlite", arqDb, cod20.toLowerCase()]);
  assert.equal(lerOk.status, 0, lerOk.stderr);
  semCanario(lerOk, "farol-ler ok");
  const lido = JSON.parse(lerOk.stdout);
  assert.equal(lido.codigo, cod20);
  assert.equal(lido.id, 536870920);
  assert.equal(lido.vezes, 5);
  assert.equal(lido.erro.fingerprint, pkg1.erro.fingerprint);
  assert.equal("pessoa_h" in lido, false);

  // Codigo inexistente ou legado (< 2^29) falha sem nada no stdout.
  const lerAusente = rodarNode(CLI_LER, ["--sqlite", arqDb, codigoRegistro(536870999)]);
  assert.notEqual(lerAusente.status, 0);
  assert.equal(lerAusente.stdout, "");
  const lerLegado = rodarNode(CLI_LER, ["--sqlite", arqDb, codigoRegistro(42)]);
  assert.notEqual(lerLegado.status, 0);
  assert.equal(lerLegado.stdout, "");

  // --- farol-top.mjs ---
  const topJson = rodarNode(CLI_TOP, ["--sqlite", arqDb, "--agora", String(t0), "--json"]);
  assert.equal(topJson.status, 0, topJson.stderr);
  semCanario(topJson, "farol-top json");
  const top = JSON.parse(topJson.stdout);
  // O registro de 9 dias atras fica fora; fp1 soma vezes 5+2=7 (2 registros) na frente de fp2 (3).
  assert.equal(top.length, 2);
  assert.equal(top[0].fp, fp1);
  assert.equal(top[0].ocorrencias, 7);
  assert.equal(top[0].registros, 2);
  assert.equal(top[0].versao, "2.1.0-ipk");
  assert.equal(top[0].plataforma, "webos");
  assert.equal(top[1].fp, fp2);
  assert.equal(top[1].ocorrencias, 3);

  const topTxt = rodarNode(CLI_TOP, ["--sqlite", arqDb, "--agora", String(t0)]);
  assert.equal(topTxt.status, 0);
  semCanario(topTxt, "farol-top txt");
  assert.ok(topTxt.stdout.includes(fp1));
  assert.equal(topTxt.stdout.includes(fpVelho), false);

  // Caminho padrao do wrangler (NUVIO_WRANGLER falso, sem rede nem --remote):
  // prova a interpolacao SQL segura (inclusive com '?' dentro de nota_pt).
  const shimJs = join(dir, "fake-wrangler.mjs");
  const logArgs = join(dir, "wrangler-args.json");
  writeFileSync(shimJs, [
    'import { readFileSync, writeFileSync } from "node:fs";',
    'import { DatabaseSync } from "node:sqlite";',
    `writeFileSync(${JSON.stringify(logArgs)}, JSON.stringify(process.argv.slice(2)));`,
    'const idx = process.argv.indexOf("--command");',
    'const sql = process.argv[idx + 1];',
    `const db = new DatabaseSync(${JSON.stringify(arqDb)});`,
    'const results = db.prepare(sql).all();',
    'db.close();',
    'console.log(JSON.stringify([{ results, success: true, meta: { changes: 1, last_row_id: 1 } }]));',
  ].join("\n"));
  const shimBin = process.platform === "win32" ? shimJs : join(dir, "fake-wrangler.sh");
  if (process.platform !== "win32") {
    writeFileSync(shimBin, `#!/bin/sh\nexec "${process.execPath}" "${shimJs}" "$@"\n`, { mode: 0o755 });
  }
  const viaWrangler = rodarNode(CLI_LER, [cod20], { NUVIO_WRANGLER: shimBin, FAROL_SQLITE: "" });
  assert.equal(viaWrangler.status, 0, viaWrangler.stderr);
  semCanario(viaWrangler, "farol-ler wrangler");
  assert.equal(JSON.parse(viaWrangler.stdout).codigo, cod20);
  const argsUsados = JSON.parse(readFileSync(logArgs, "utf8"));
  assert.ok(argsUsados.includes("--local"), "padrao do CLI usa --local, nunca --remote");
  assert.equal(argsUsados.includes("--remote"), false);
});
