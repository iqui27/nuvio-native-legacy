#!/usr/bin/env node
// Top fingerprints do Farol por versao e plataforma em 7 dias (PLANO.md secao
// 2.7, issue 13). NUNCA seleciona, agrupa nem imprime `pessoa_h`.
//
//   node tools/farol-top.mjs [--sqlite <arq> | --local | --remote] [--dias 7] [--limite 20] [--plataforma <p>] [--versao <v>] [--json]
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { abrirBancoCli } from "./farol-conhecido.mjs";

const USO =
  "uso: farol-top.mjs [--sqlite <arq> | --local | --remote] [--dias <n>] [--limite <n>] " +
  "[--plataforma <plat>] [--versao <ver>] [--json]";

export async function consultarFarolTop(db, {
  dias = 7,
  limite = 20,
  plataforma = null,
  versao = null,
  agora = Math.floor(Date.now() / 1000),
} = {}) {
  if (!Number.isFinite(dias) || dias <= 0 || dias > 365) throw new Error("dias invalido");
  if (!Number.isInteger(limite) || limite < 1 || limite > 200) throw new Error("limite invalido");
  const desde = Math.floor(agora - dias * 86400);
  const plat = plataforma ? String(plataforma).trim() : null;
  const ver = versao ? String(versao).trim() : null;
  const r = await db.prepare(
    "SELECT fp, fingerprint, codigo, versao, plataforma, " +
    "COUNT(*) AS registros, COALESCE(SUM(vezes), 0) AS ocorrencias, MAX(criado) AS ultimo " +
    "FROM farol_registro " +
    "WHERE criado >= ? AND (? IS NULL OR plataforma = ?) AND (? IS NULL OR versao = ?) " +
    "GROUP BY fp, fingerprint, codigo, versao, plataforma " +
    "ORDER BY ocorrencias DESC, registros DESC, ultimo DESC, fp ASC " +
    "LIMIT ?"
  ).bind(desde, plat, plat, ver, ver, limite).all();
  return (r?.results || []).map((l) => ({
    ocorrencias: Number(l.ocorrencias),
    registros: Number(l.registros),
    versao: l.versao,
    plataforma: l.plataforma,
    codigo: l.codigo,
    fp: l.fp,
    fingerprint: l.fingerprint,
    ultimo: Number(l.ultimo),
  }));
}

export async function executarCliTop(argv, io = { out: console.log, err: console.error }, env = process.env) {
  let ctx;
  try {
    ctx = abrirBancoCli(argv, env);
  } catch (e) {
    io.err(e.message || String(e));
    return 1;
  }
  try {
    const args = ctx.resto;
    let dias = 7;
    let limite = 20;
    let plataforma = null;
    let versao = null;
    let agora = Math.floor(Date.now() / 1000);
    let emJson = false;
    for (let i = 0; i < args.length; i++) {
      const f = args[i];
      if (f === "--json") { emJson = true; continue; }
      const v = args[++i];
      if (v === undefined) { io.err(USO); return 1; }
      if (f === "--dias" || f === "-d") {
        dias = Number(v);
        if (!Number.isFinite(dias) || dias <= 0) { io.err(USO); return 1; }
      } else if (f === "--limite" || f === "-n") {
        if (!/^\d+$/.test(v)) { io.err(USO); return 1; }
        limite = Number(v);
      } else if (f === "--plataforma" || f === "-p") plataforma = v;
      else if (f === "--versao" || f === "-v") versao = v;
      else if (f === "--agora") {
        if (!/^\d+$/.test(v)) { io.err(USO); return 1; }
        agora = Number(v);
      } else { io.err(USO); return 1; }
    }
    const linhas = await consultarFarolTop(ctx.db, { dias, limite, plataforma, versao, agora });
    if (emJson) {
      io.out(JSON.stringify(linhas, null, 2));
    } else {
      io.out("ocorrencias\tregistros\tversao\tplataforma\tcodigo\tfp\tfingerprint");
      for (const l of linhas)
        io.out(`${l.ocorrencias}\t${l.registros}\t${l.versao}\t${l.plataforma}\t${l.codigo}\t${l.fp}\t${l.fingerprint}`);
    }
    return 0;
  } catch (e) {
    io.err(e.message || String(e));
    return 1;
  } finally {
    ctx.fechar();
  }
}

const ehCli = process.argv[1] && resolve(process.argv[1]) === resolve(fileURLToPath(import.meta.url));
if (ehCli) {
  const rc = await executarCliTop(process.argv.slice(2));
  process.exit(rc);
}
