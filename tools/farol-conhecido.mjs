#!/usr/bin/env node
// Manutencao da tabela `farol_conhecido` pelo dono (PLANO.md secao 2.7, issue 13).
// Nunca consulta `farol_registro` nem imprime `pessoa_h`.
//
//   node tools/farol-conhecido.mjs add --fp <16hex> --estado corrigido --versao 2.1.2 [--nota-pt "..."] [--nota-en "..."]
//   node tools/farol-conhecido.mjs add --codigo TV_FORMATO_REPETE --plataforma webos [--so-maior 4] --estado contorno --nota-pt "..."
//   node tools/farol-conhecido.mjs list [--json]
//   node tools/farol-conhecido.mjs remove <id>
//
// Destino do banco (vale tambem para farol-ler.mjs e farol-top.mjs):
//   --sqlite <arquivo>   SQLite local direto (node:sqlite; ou env FAROL_SQLITE)
//   --local              wrangler d1 execute --local (padrao sem --sqlite)
//   --remote             wrangler d1 execute --remote (so quando o dono pede)
import { execFileSync } from "node:child_process";
import { resolve } from "node:path";
import { DatabaseSync } from "node:sqlite";
import { fileURLToPath } from "node:url";
import { ESTADOS_CONHECIDO, lerVersao } from "../servidor/recomendacoes/src/farol.js";

const RAIZ = resolve(fileURLToPath(new URL("..", import.meta.url)));
const CFG_PADRAO = resolve(RAIZ, "servidor/recomendacoes/wrangler.toml");
const FP_RX = /^[0-9a-f]{16}$/;
const TOK_RX = /^[A-Za-z0-9_.:+-]{1,48}$/;
const PLAT_OK = new Set(["webos", "tizen_wgt", "tizen_tpk", "android", "outra"]);

function sqlLiteral(v) {
  if (v === null || v === undefined) return "NULL";
  if (typeof v === "number") {
    if (!Number.isFinite(v)) throw new Error("numero invalido");
    return String(v);
  }
  const s = String(v);
  if (s.includes("\0")) throw new Error("texto invalido");
  return `'${s.replace(/'/g, "''")}'`;
}

export function interpolarSql(sql, valores) {
  let i = 0;
  const out = sql.replace(/\?/g, () => {
    if (i >= valores.length) throw new Error("faltam parametros SQL");
    return sqlLiteral(valores[i++]);
  });
  if (i !== valores.length) throw new Error("sobram parametros SQL");
  return out;
}

function criarDbSqlite(caminho) {
  const sqlite = new DatabaseSync(caminho);
  const stmt = (sql, valores) => ({
    first: async () => sqlite.prepare(sql).get(...valores) ?? null,
    all: async () => ({ results: sqlite.prepare(sql).all(...valores) }),
    run: async () => {
      const r = sqlite.prepare(sql).run(...valores);
      return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } };
    },
  });
  return {
    db: { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) },
    fechar: () => sqlite.close(),
  };
}

function resolverComandoWrangler(env) {
  if (env.NUVIO_WRANGLER) {
    if (/\.(m?js|cjs)$/i.test(env.NUVIO_WRANGLER))
      return [process.execPath, env.NUVIO_WRANGLER];
    return [env.NUVIO_WRANGLER];
  }
  if (process.platform === "win32") {
    const npxCli = resolve(process.execPath, "..", "node_modules/npm/bin/npx-cli.js");
    return [process.execPath, npxCli, "wrangler@4"];
  }
  return ["npx", "wrangler@4"];
}

function criarDbWrangler({ modo, banco, config, persistTo, env }) {
  const bin = resolverComandoWrangler(env);
  const rodar = (sql, valores) => {
    const cmd = interpolarSql(sql, valores);
    const args = [...bin.slice(1), "d1", "execute", banco, modo === "remote" ? "--remote" : "--local",
                  "--config", config, "--json", "--command", cmd];
    if (persistTo) args.push("--persist-to", persistTo);
    const bruto = execFileSync(bin[0], args, { encoding: "utf8", env });
    const ini = bruto.indexOf("[");
    const arr = JSON.parse(ini >= 0 ? bruto.slice(ini) : bruto);
    const item = Array.isArray(arr) ? arr[0] : arr;
    return {
      results: item?.results || [],
      meta: {
        changes: Number(item?.meta?.changes ?? 0),
        last_row_id: Number(item?.meta?.last_row_id ?? 0),
      },
    };
  };
  const stmt = (sql, valores) => ({
    first: async () => rodar(sql, valores).results[0] ?? null,
    all: async () => ({ results: rodar(sql, valores).results }),
    run: async () => ({ meta: rodar(sql, valores).meta }),
  });
  return {
    db: { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) },
    fechar: () => {},
  };
}

export function abrirBancoCli(argv, env = process.env) {
  let sqliteArq = env.FAROL_SQLITE || "";
  let modo = "local";
  let banco = "nuvio-recomendacoes";
  let config = env.NUVIO_D1_CFG || CFG_PADRAO;
  let persistTo = env.NUVIO_D1_PERSIST || "";
  const resto = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--sqlite") {
      if (i + 1 >= argv.length) throw new Error("falta caminho em --sqlite");
      sqliteArq = argv[++i];
    } else if (a === "--remote") modo = "remote";
    else if (a === "--local") modo = "local";
    else if (a === "--banco") {
      if (i + 1 >= argv.length) throw new Error("falta nome em --banco");
      banco = argv[++i];
    } else if (a === "--config") {
      if (i + 1 >= argv.length) throw new Error("falta arquivo em --config");
      config = argv[++i];
    } else if (a === "--persist-to") {
      if (i + 1 >= argv.length) throw new Error("falta pasta em --persist-to");
      persistTo = argv[++i];
    } else resto.push(a);
  }
  if (sqliteArq) return { ...criarDbSqlite(sqliteArq), resto };
  return { ...criarDbWrangler({ modo, banco, config, persistTo, env }), resto };
}

export async function adicionarConhecido(db, entrada = {}) {
  const estado = String(entrada.estado ?? "").trim();
  if (!ESTADOS_CONHECIDO.has(estado))
    throw new Error("estado invalido (use corrigido, conhecido ou contorno)");
  const versao = String(entrada.versao ?? "").trim();
  if (estado === "corrigido" && !lerVersao(versao))
    throw new Error("estado 'corrigido' exige --versao valida (ex.: 2.1.2)");
  if (versao && !lerVersao(versao))
    throw new Error("versao invalida");

  const fpCru = entrada.fp == null || entrada.fp === "" ? null : String(entrada.fp).trim().toLowerCase();
  if (fpCru !== null && !FP_RX.test(fpCru))
    throw new Error("fp invalido (16 hex)");

  const codigo = entrada.codigo == null || entrada.codigo === "" ? null : String(entrada.codigo).trim();
  if (codigo !== null && !TOK_RX.test(codigo))
    throw new Error("codigo invalido");

  const plataforma = entrada.plataforma == null || entrada.plataforma === "" ? null : String(entrada.plataforma).trim();
  if (plataforma !== null && !PLAT_OK.has(plataforma))
    throw new Error("plataforma invalida (webos|tizen_wgt|tizen_tpk|android|outra)");

  let soMaior = null;
  if (entrada.soMaior !== undefined && entrada.soMaior !== null && entrada.soMaior !== "") {
    const n = typeof entrada.soMaior === "number" ? entrada.soMaior
            : (/^\d{1,2}$/.test(String(entrada.soMaior).trim()) ? Number(entrada.soMaior) : NaN);
    if (!Number.isInteger(n) || n < 0 || n > 99) throw new Error("so_maior invalido");
    soMaior = n;
  }

  if (fpCru === null && !(codigo !== null && plataforma !== null))
    throw new Error("informe --fp <16hex> ou (--codigo <cod> --plataforma <plat> [--so-maior <n>])");
  if (fpCru === null && soMaior !== null && (codigo === null || plataforma === null))
    throw new Error("--so-maior exige --codigo e --plataforma");

  const notaPt = String(entrada.notaPt ?? entrada.nota_pt ?? "").trim().slice(0, 240);
  const notaEn = String(entrada.notaEn ?? entrada.nota_en ?? "").trim().slice(0, 240);

  const r = await db.prepare(
    "INSERT INTO farol_conhecido (fp, codigo, plataforma, so_maior, estado, versao, nota_pt, nota_en) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?) RETURNING id"
  ).bind(fpCru, codigo, plataforma, soMaior, estado, versao, notaPt, notaEn).first();
  return {
    id: Number(r?.id ?? 0),
    fp: fpCru,
    codigo,
    plataforma,
    so_maior: soMaior,
    estado,
    versao,
    nota_pt: notaPt,
    nota_en: notaEn,
  };
}

export async function listarConhecidos(db) {
  const r = await db.prepare(
    "SELECT id, fp, codigo, plataforma, so_maior, estado, versao, nota_pt, nota_en " +
    "FROM farol_conhecido ORDER BY id ASC"
  ).all();
  return (r?.results || []).map((l) => ({
    id: Number(l.id),
    fp: l.fp ?? null,
    codigo: l.codigo ?? null,
    plataforma: l.plataforma ?? null,
    so_maior: l.so_maior === null || l.so_maior === undefined ? null : Number(l.so_maior),
    estado: l.estado,
    versao: l.versao || "",
    nota_pt: l.nota_pt || "",
    nota_en: l.nota_en || "",
  }));
}

export async function removerConhecido(db, idCru) {
  const s = String(idCru ?? "").trim();
  if (!/^\d+$/.test(s)) throw new Error("id invalido");
  const id = Number(s);
  if (!Number.isSafeInteger(id) || id < 1) throw new Error("id invalido");
  const r = await db.prepare("DELETE FROM farol_conhecido WHERE id = ?").bind(id).run();
  return Number(r?.meta?.changes ?? 0) > 0;
}

function chaveAlvo(l) {
  if (l.fp) return `fp:${l.fp}`;
  return l.so_maior !== null
    ? `${l.codigo}/${l.plataforma}/${l.so_maior}`
    : `${l.codigo}/${l.plataforma}`;
}

const USO =
  "uso:\n" +
  "  farol-conhecido.mjs add (--fp <16hex> | --codigo <cod> --plataforma <plat> [--so-maior <n>]) " +
  "--estado <corrigido|conhecido|contorno> [--versao <v>] [--nota-pt <txt>] [--nota-en <txt>] [--json]\n" +
  "  farol-conhecido.mjs list [--json]\n" +
  "  farol-conhecido.mjs remove <id> [--json]";

export async function executarCliConhecido(argv, io = { out: console.log, err: console.error }, env = process.env) {
  let ctx;
  try {
    ctx = abrirBancoCli(argv, env);
  } catch (e) {
    io.err(e.message || String(e));
    return 1;
  }
  try {
    const args = ctx.resto;
    const sub = (args[0] || "").toLowerCase();
    const emJson = args.includes("--json");
    const limpos = args.slice(1).filter((x) => x !== "--json");

    if (sub === "list" || sub === "listar" || sub === "ls") {
      if (limpos.length) { io.err(USO); return 1; }
      const itens = await listarConhecidos(ctx.db);
      if (emJson) io.out(JSON.stringify(itens, null, 2));
      else for (const l of itens)
        io.out(`${l.id}\t${chaveAlvo(l)}\t${l.estado}\t${l.versao || "-"}\t${l.nota_pt || l.nota_en || ""}`);
      return 0;
    }

    if (sub === "remove" || sub === "remover" || sub === "rm" || sub === "del") {
      let idAlvo = "";
      if (limpos.length === 1) idAlvo = limpos[0];
      else if (limpos.length === 2 && limpos[0] === "--id") idAlvo = limpos[1];
      else { io.err(USO); return 1; }
      const ok = await removerConhecido(ctx.db, idAlvo);
      if (!ok) { io.err(`id ${idAlvo} nao encontrado`); return 1; }
      io.out(emJson ? JSON.stringify({ ok: 1, id: Number(idAlvo) }) : `removido ${idAlvo}`);
      return 0;
    }

    if (sub === "add" || sub === "adicionar") {
      const ent = {};
      for (let i = 0; i < limpos.length; i++) {
        const f = limpos[i], v = limpos[i + 1];
        if (v === undefined || v.startsWith("--")) { io.err(USO); return 1; }
        i++;
        if (f === "--fp") ent.fp = v;
        else if (f === "--codigo" || f === "-c") ent.codigo = v;
        else if (f === "--plataforma" || f === "-p") ent.plataforma = v;
        else if (f === "--so-maior" || f === "--so_maior" || f === "--so") ent.soMaior = v;
        else if (f === "--estado" || f === "-e") ent.estado = v;
        else if (f === "--versao" || f === "-v") ent.versao = v;
        else if (f === "--nota-pt" || f === "--nota_pt") ent.notaPt = v;
        else if (f === "--nota-en" || f === "--nota_en") ent.notaEn = v;
        else { io.err(USO); return 1; }
      }
      const criado = await adicionarConhecido(ctx.db, ent);
      io.out(emJson ? JSON.stringify(criado, null, 2)
                    : `${criado.id}\t${chaveAlvo(criado)}\t${criado.estado}\t${criado.versao || "-"}`);
      return 0;
    }

    io.err(USO);
    return 1;
  } catch (e) {
    io.err(e.message || String(e));
    return 1;
  } finally {
    ctx.fechar();
  }
}

const ehCli = process.argv[1] && resolve(process.argv[1]) === resolve(fileURLToPath(import.meta.url));
if (ehCli) {
  const rc = await executarCliConhecido(process.argv.slice(2));
  process.exit(rc);
}
