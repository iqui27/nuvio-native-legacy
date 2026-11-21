#!/usr/bin/env node
// Codigo de registro <-> id da linha (ver codigoRegistro em src/codigo.js e
// src/regcodigo.c no cliente). A MESMA bijecao serve `registro` e
// `farol_registro` (migracao-018): id >= 2^29 e do Farol.
//
//   node codigo-registro.mjs K7QM2X      -> id da linha (sempre codigo, mesmo so digitos)
//   node codigo-registro.mjs --id 9866   -> codigo que a TV mostrou
//
// Contrato da ferramenta de suporte (tests/regcodigo_suporte.sh): a saida
// padrao e so o id ou so o codigo, puro. A tabela de origem sai junto com a
// flag --tabela: `node codigo-registro.mjs --tabela K7QM2X` -> "9866\tregistro".
//
// Com o id e a tabela (faixa: < 2^29 e `registro`, >= 2^29 e `farol_registro`):
//   registro:       wrangler d1 execute <banco> --remote \
//     --command "SELECT id, pessoa, versao, plataforma, quando FROM registro WHERE id = <id>"
//   farol_registro: wrangler d1 execute <banco> --remote \
//     --command "SELECT id, criado, versao, plataforma, codigo, fp FROM farol_registro WHERE id = <id>"
import { codigoRegistro } from "./src/codigo.js";
const ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
const A_INV = 0x39331415n, B = 0x15a4e3c7n, M = 1n << 30n, FAROL_BASE = 1n << 29n;
const tabela = (id) => (BigInt(id) >= FAROL_BASE ? "farol_registro" : "registro");
const a = process.argv.slice(2), USO = "uso: codigo-registro.mjs [--tabela] <codigo de 6> | --id <id>";
const sai = (r) => { if (r == null) { console.error(USO); process.exit(1); } console.log(r); process.exit(0); };
// --tabela vale em qualquer posicao; aridade e flag ANTES de normalizar o
// codigo (I->1 viraria "--1d").
const comTabela = a.includes("--tabela");
const args = a.filter((x) => x !== "--tabela");
if (args.length === 2 && args[0] === "--id") {
  const c = /^\d+$/.test(args[1]) ? codigoRegistro(args[1]) : null;
  sai(c && comTabela ? `${c}\t${tabela(args[1])}` : c);
}
const arg = args.length === 1 ? args[0].trim().toUpperCase()
  .replace(/O/g, "0").replace(/[IL]/g, "1") : "";   // leitura de Crockford
if (!/^[0-9A-HJKMNP-TV-Z]{6}$/.test(arg)) sai(null);
let v = 0n;
for (const c of arg) v = v * 32n + BigInt(ALFA.indexOf(c));
const id = String((((v - B) % M + M) % M * A_INV) % M);
sai(comTabela ? `${id}\t${tabela(id)}` : id);
