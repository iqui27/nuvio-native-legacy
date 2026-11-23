#!/usr/bin/env node
// Le um recibo do Farol pelo codigo de 6 caracteres e imprime o JSON formatado
// (PLANO.md secao 2.7, issue 13). NUNCA seleciona nem imprime `pessoa_h`.
//
//   node tools/farol-ler.mjs [--sqlite <arq> | --local | --remote] <codigo>
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { codigoRegistro, decodificarCodigo, FAROL_ID_MIN } from "../servidor/recomendacoes/src/codigo.js";
import { abrirBancoCli } from "./farol-conhecido.mjs";

const USO = "uso: farol-ler.mjs [--sqlite <arq> | --local | --remote] <codigo de 6>";

export async function lerFarolPorCodigo(db, codigoCru) {
  const limpo = typeof codigoCru === "string" ? codigoCru.trim() : "";
  const id = decodificarCodigo(limpo);
  if (id === null || id < FAROL_ID_MIN) throw new Error("codigo invalido");
  // `pessoa_h` fica fora do SELECT de proposito: a ferramenta nunca a le nem imprime.
  const linha = await db.prepare(
    "SELECT id, criado, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json " +
    "FROM farol_registro WHERE id = ?"
  ).bind(id).first();
  if (!linha) return null;
  let pacote = {};
  try { pacote = JSON.parse(linha.json || "{}"); } catch { pacote = {}; }
  if (pacote && typeof pacote === "object") delete pacote.pessoa_h;
  return {
    codigo: codigoRegistro(linha.id),
    id: Number(linha.id),
    criado: Number(linha.criado),
    vezes: Number(linha.vezes),
    ...pacote,
  };
}

export async function executarCliLer(argv, io = { out: console.log, err: console.error }, env = process.env) {
  let ctx;
  try {
    ctx = abrirBancoCli(argv, env);
  } catch (e) {
    io.err(e.message || String(e));
    return 1;
  }
  try {
    if (ctx.resto.length !== 1) { io.err(USO); return 1; }
    const doc = await lerFarolPorCodigo(ctx.db, ctx.resto[0]);
    if (!doc) { io.err("registro nao encontrado"); return 1; }
    io.out(JSON.stringify(doc, null, 2));
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
  const rc = await executarCliLer(process.argv.slice(2));
  process.exit(rc);
}
