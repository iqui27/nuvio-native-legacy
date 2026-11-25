#!/usr/bin/env node
// Auditoria offline de registros FarolRegistro (PLANO §2.5).
//
// Le um ou mais arquivos JSON ou NDJSON locais (export offline do D1 feito
// separadamente pelo dono — NUNCA chama rede nem CLI remota) e aplica os
// padroes proibidos do redator:
//   - ://
//   - @
//   - IPv4
//   - IPv6
//   - 20+ hex seguidos fora de h:/fingerprint
//   - %2F
//   - Bearer
//   - JWT
//   - CANARIO
//
// Imprime SOMENTE contagens (por padrao e total), nunca o trecho nem o
// registro. Sai com codigo 0 se tudo limpo, 1 se houver qualquer ocorrencia,
// 2 em erro de uso/leitura/JSON.
import { readFileSync } from "node:fs";

const PADROES = [
  "url_esquema",
  "arroba",
  "ipv4",
  "ipv6",
  "hex_longo",
  "pct_2f",
  "bearer",
  "jwt",
  "canario",
];

const RX_URL = /:\/\//g;
const RX_AT = /@/g;
const RX_IPV4 = /(?<![0-9.])(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}(?![0-9.])/g;
const RX_HEX20 = /(?<![0-9a-fA-F])[0-9a-fA-F]{20,}(?![0-9a-fA-F])/g;
const RX_PCT2F = /%2f/gi;
const RX_BEARER = /\bBearer\b/gi;
const RX_JWT = /\beyJ[A-Za-z0-9_-]{4,}\.[A-Za-z0-9_-]{1,}\.[A-Za-z0-9_-]{1,}\b/g;
const RX_CANARIO = /canario/gi;

// Valida token candidato a endereco IPv6 (completo ou comprimido com "::",
// opcionalmente com sufixo IPv4 embutido como ::ffff:192.0.2.1).
function ehIpv6Token(tok) {
  let s = tok;
  if (s.startsWith("[") && s.endsWith("]")) s = s.slice(1, -1);
  if (!s.includes(":")) return false;
  // Remove zone id (%eth0) se presente
  const pct = s.indexOf("%");
  if (pct >= 0) s = s.slice(0, pct);
  if (!/^[0-9a-fA-F:.]+$/.test(s)) return false;

  const temDuplo = s.includes("::");
  if (temDuplo) {
    if (s.indexOf("::") !== s.lastIndexOf("::")) return false;
    const [esqStr, dirStr] = s.split("::");
    const esq = esqStr ? esqStr.split(":") : [];
    const dir = dirStr ? dirStr.split(":") : [];
    // Exige ao menos um grupo hex real (descarta "::" isolado sem digitos)
    if (esq.length === 0 && dir.length === 0) return false;
    let grupos = esq.length + dir.length;
    if (dir.length > 0 && dir[dir.length - 1].includes(".")) {
      const ultimo = dir.pop();
      if (!new RegExp(`^${RX_IPV4.source}$`).test(ultimo)) return false;
      grupos += 1; // IPv4 ocupa 2 grupos de 16 bits (ja contou 1)
    }
    if (grupos > 7) return false;
    for (const g of esq) if (!/^[0-9a-fA-F]{1,4}$/.test(g)) return false;
    for (const g of dir) if (!/^[0-9a-fA-F]{1,4}$/.test(g)) return false;
    return true;
  }

  const partes = s.split(":");
  if (partes.length === 8) {
    return partes.every((g) => /^[0-9a-fA-F]{1,4}$/.test(g));
  }
  if (partes.length === 7 && partes[6].includes(".")) {
    const ultimo = partes[6];
    if (!new RegExp(`^${RX_IPV4.source}$`).test(ultimo)) return false;
    return partes.slice(0, 6).every((g) => /^[0-9a-fA-F]{1,4}$/.test(g));
  }
  return false;
}

function contarIpv6(s) {
  if (!s.includes(":")) return 0;
  // Extrai trechos continuos de caracteres validos de IPv6
  const cands = s.match(/\[?[0-9a-fA-F:.]{2,}\]?/g);
  if (!cands) return 0;
  let n = 0;
  for (const c of cands) {
    // Limpa pontos ou dois-pontos soltos de pontuacao nas bordas, preservando "::"
    let t = c;
    while (t.length > 2 && t.startsWith(".") ) t = t.slice(1);
    while (t.length > 2 && t.endsWith(".")) t = t.slice(0, -1);
    if (ehIpv6Token(t)) n++;
  }
  return n;
}

function contarRegex(rx, s) {
  rx.lastIndex = 0;
  let n = 0;
  while (rx.exec(s) !== null) n++;
  return n;
}

function contarHexLongo(s, caminho) {
  // Fora de "h:" e fora de "fingerprint" (PLANO §2.5)
  const ehFingerprint =
    caminho === "fingerprint" || caminho.endsWith(".fingerprint");
  if (ehFingerprint) return 0;
  // Remove tokens com prefixo "h:" seguido de hex antes de procurar sequencias de 20+ hex
  const limpo = s.replace(/\bh:[0-9a-fA-F]+\b/g, "");
  return contarRegex(RX_HEX20, limpo);
}

function auditarTexto(s, caminho, contagens) {
  if (typeof s !== "string" || s.length === 0) return;
  contagens.url_esquema += contarRegex(RX_URL, s);
  contagens.arroba += contarRegex(RX_AT, s);
  contagens.ipv4 += contarRegex(RX_IPV4, s);
  contagens.ipv6 += contarIpv6(s);
  contagens.hex_longo += contarHexLongo(s, caminho);
  contagens.pct_2f += contarRegex(RX_PCT2F, s);
  contagens.bearer += contarRegex(RX_BEARER, s);
  contagens.jwt += contarRegex(RX_JWT, s);
  contagens.canario += contarRegex(RX_CANARIO, s);
}

function auditarValor(val, caminho, contagens) {
  if (val === null || val === undefined) return;
  if (typeof val === "string") {
    auditarTexto(val, caminho, contagens);
    return;
  }
  if (Array.isArray(val)) {
    const sub = caminho ? `${caminho}[]` : "[]";
    for (const item of val) auditarValor(item, sub, contagens);
    return;
  }
  if (typeof val === "object") {
    for (const k of Object.keys(val)) {
      const sub = caminho ? `${caminho}.${k}` : k;
      auditarTexto(k, sub, contagens);
      auditarValor(val[k], sub, contagens);
    }
  }
}

// Desembrulha linha de export D1 ({ id, ..., json: "{...}" }) para o objeto FarolRegistro.
function desembrulharRegistro(obj) {
  if (
    obj &&
    typeof obj === "object" &&
    !Array.isArray(obj) &&
    typeof obj.json === "string"
  ) {
    const t = obj.json.trim();
    if (t.startsWith("{")) {
      return JSON.parse(t);
    }
  }
  return obj;
}

// Extrai a lista de registros de JSON (objeto, array, envelope D1 results) ou NDJSON.
function extrairRegistros(conteudo) {
  const texto = conteudo.trim();
  if (!texto) return [];
  try {
    const parsed = JSON.parse(texto);
    if (Array.isArray(parsed)) {
      // Envelope de export D1: [{ results: [...], success: true }]
      if (
        parsed.length > 0 &&
        parsed.every(
          (it) => it && typeof it === "object" && Array.isArray(it.results)
        )
      ) {
        const out = [];
        for (const bloco of parsed) {
          for (const row of bloco.results) out.push(desembrulharRegistro(row));
        }
        return out;
      }
      return parsed.map(desembrulharRegistro);
    }
    if (parsed && typeof parsed === "object") {
      if (Array.isArray(parsed.results)) {
        return parsed.results.map(desembrulharRegistro);
      }
      return [desembrulharRegistro(parsed)];
    }
    return [parsed];
  } catch {
    // Tenta NDJSON (um JSON por linha nao vazia)
    const out = [];
    const linhas = texto.split(/\r?\n/);
    for (let i = 0; i < linhas.length; i++) {
      const l = linhas[i].trim();
      if (!l) continue;
      let obj;
      try {
        obj = JSON.parse(l);
      } catch {
        throw new Error(`JSON invalido na linha ${i + 1}`);
      }
      out.push(desembrulharRegistro(obj));
    }
    return out;
  }
}

function main() {
  const args = process.argv.slice(2);
  if (args.length === 0 || args.includes("-h") || args.includes("--help")) {
    process.stderr.write("Uso: node tools/farol-auditar.mjs <arquivo.json|ndjson|-> [...]\n");
    process.exit(args.length === 0 ? 2 : 0);
  }

  const contagens = Object.fromEntries(PADROES.map((p) => [p, 0]));
  let nRegistros = 0;

  for (const arg of args) {
    let raw;
    try {
      raw = arg === "-" ? readFileSync(0, "utf8") : readFileSync(arg, "utf8");
    } catch {
      process.stderr.write("erro: nao foi possivel ler o arquivo informado\n");
      process.exit(2);
    }
    let registros;
    try {
      registros = extrairRegistros(raw);
    } catch {
      process.stderr.write("erro: entrada JSON/NDJSON invalida\n");
      process.exit(2);
    }
    for (const reg of registros) {
      nRegistros++;
      auditarValor(reg, "", contagens);
    }
  }

  let total = 0;
  for (const p of PADROES) total += contagens[p];

  const saida = [`registros: ${nRegistros}`];
  for (const p of PADROES) saida.push(`${p}: ${contagens[p]}`);
  saida.push(`total: ${total}`);
  process.stdout.write(saida.join("\n") + "\n");

  process.exit(total > 0 ? 1 : 0);
}

main();
