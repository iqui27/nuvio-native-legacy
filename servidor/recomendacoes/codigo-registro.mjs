#!/usr/bin/env node
// Codigo de registro <-> id da tabela `registro` (ver codigoRegistro em
// src/index.js e src/regcodigo.c no cliente).
//
//   node codigo-registro.mjs K7QM2X      -> id da linha (sempre codigo, mesmo so digitos)
//   node codigo-registro.mjs --id 9866   -> codigo que a TV mostrou
//
// Com o id: wrangler d1 execute <banco> --remote \
//   --command "SELECT id, pessoa, versao, plataforma, quando FROM registro WHERE id = <id>"
import { codigoRegistro } from "./src/codigo.js";
const ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
const A_INV = 0x39331415n, B = 0x15a4e3c7n, M = 1n << 30n;
const a = process.argv.slice(2), USO = "uso: codigo-registro.mjs <codigo de 6> | --id <id>";
const sai = (r) => { if (r == null) { console.error(USO); process.exit(1); } console.log(r); process.exit(0); };
// Aridade e flag ANTES de normalizar o codigo (I->1 viraria "--1d").
if (a.length === 2 && a[0] === "--id") sai(/^\d+$/.test(a[1]) ? codigoRegistro(a[1]) : null);
const arg = a.length === 1 ? a[0].trim().toUpperCase()
  .replace(/O/g, "0").replace(/[IL]/g, "1") : "";   // leitura de Crockford
if (!/^[0-9A-HJKMNP-TV-Z]{6}$/.test(arg)) sai(null);
let v = 0n;
for (const c of arg) v = v * 32n + BigInt(ALFA.indexOf(c));
sai(String((((v - B) % M + M) % M * A_INV) % M));
