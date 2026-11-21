#!/usr/bin/env node
// Codigo de registro <-> id da linha (ver codigoRegistro em src/codigo.js e
// src/regcodigo.c no cliente). A MESMA bijecao serve `registro` e
// `farol_registro` (migracao-018): id >= 2^29 e do Farol.
//
//   node codigo-registro.mjs K7QM2X      -> id e tabela (sempre codigo, mesmo so digitos)
//   node codigo-registro.mjs --id 9866   -> codigo que a TV mostrou e tabela
//
// Com o id: wrangler d1 execute <banco> --remote \
//   --command "SELECT id, pessoa, versao, plataforma, quando FROM <tabela> WHERE id = <id>"
import { codigoRegistro } from "./src/codigo.js";
const ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
const A_INV = 0x39331415n, B = 0x15a4e3c7n, M = 1n << 30n, FAROL_BASE = 1n << 29n;
const tabela = (id) => (BigInt(id) >= FAROL_BASE ? "farol_registro" : "registro");
const a = process.argv.slice(2), USO = "uso: codigo-registro.mjs <codigo de 6> | --id <id>";
const sai = (r) => { if (r == null) { console.error(USO); process.exit(1); } console.log(r); process.exit(0); };
// Aridade e flag ANTES de normalizar o codigo (I->1 viraria "--1d").
if (a.length === 2 && a[0] === "--id") {
  sai(/^\d+$/.test(a[1]) && codigoRegistro(a[1]) ? `${codigoRegistro(a[1])}\t${tabela(a[1])}` : null);
}
const arg = a.length === 1 ? a[0].trim().toUpperCase()
  .replace(/O/g, "0").replace(/[IL]/g, "1") : "";   // leitura de Crockford
if (!/^[0-9A-HJKMNP-TV-Z]{6}$/.test(arg)) sai(null);
let v = 0n;
for (const c of arg) v = v * 32n + BigInt(ALFA.indexOf(c));
const id = String((((v - B) % M + M) % M * A_INV) % M);
sai(`${id}\t${tabela(id)}`);
