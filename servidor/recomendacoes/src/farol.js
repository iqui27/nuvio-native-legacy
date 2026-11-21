// FAROL — RECEPCAO DO REGISTRO ESTRUTURADO (PLANO.md secao 2.7, issue 5).
// O pacote `FarolRegistro v1` e MONTADO na TV por lista branca (farolpacote.c):
// so enums, numeros e tokens do alfabeto de `farolanel_token`. Aqui a defesa e
// a mesma: chave fora do esquema ou valor com sinal de vazamento ("://", "@",
// IPv4) e 422 e nada e gravado. O servidor nao sabe quem mandou: `pessoa_h` e
// HMAC(FAROL_SAL, id) so para os tetos de abuso, nunca o id cru.
//
// O codigo de 6 caracteres do recibo e a MESMA bijecao de `registro`
// (codigo.js); para os dois espacos nao colidirem os ids do Farol comecam em
// 2^29 (ver migracao-018-farol.sql e codigo-registro.mjs).
//
// Match de correcao conhecida (tabela farol_conhecido), GET /v1/farol/conhecido
// e a pagina /v1/farol/r/<codigo> sao da issue 13 — a tabela ja nasce aqui
// porque mora no mesmo arquivo de migracao.

export const FAROL_CORPO_MAX = 64 * 1024;
const FAROL_RETENCAO = 30 * 24 * 3600;
const FAROL_HORA = 6;            // envios por pessoa por hora
const FAROL_DIA = 20;            // envios por pessoa por dia
const FAROL_DIA_GLOBAL = 500;    // envios no total por dia
const FAROL_DEDUP_JANELA = 24 * 3600;
const LINHA_MAX = 256;
const LISTA_MAX = 64;

// O alfabeto seguro: o que o redator da TV ja produz, mais espaco e maiusculas
// (modelo "OLED55C9PSA", versao "2.1.0"). Um valor que nao cabe aqui e texto
// livre — e texto livre e o que o pacote nao pode ter.
const STR_RX = /^[A-Za-z0-9_.:+ \-]*$/;
const IPV4_RX = /\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/;

function strOk(v, max) {
  return typeof v === "string" && v.length <= max && STR_RX.test(v) &&
    !v.includes("://") && !v.includes("@") && !IPV4_RX.test(v);
}
const numOk = (v) => typeof v === "number" && Number.isFinite(v);
const intOk = (v) => Number.isInteger(v) && Math.abs(v) <= 0x7fffffff;
const boolOk = (v) => v === true || v === false || v === 0 || v === 1;

// Esquema de lista branca do FarolRegistro v1 (secao 2.6 do PLANO). Falta de
// chave e tolerado (TV velha manda menos); chave FORA daqui recusa o pacote.
const ESQUEMA = {
  v: "int",
  fingerprint: "s",
  codigo: "s",
  aparelho: {
    plataforma: "s", so: "s", so_maior: "int", modelo: "s32",
    ram_total_mb: "int", ram_livre_mb: "int",
    tela: { w: "int", h: "int", hdr: "int", dv: "int", hevc: "int", av1: "int" },
    audio: { dts: "int", truehd: "int", eac3: "int", ac3: "int" },
    rede: { offline: "bool", hosts_com_falha: "int", vazao_mbps: "num" },
  },
  cliente: {
    versao: "s", commit: "s", build: "s", backend: "s", idioma: "s",
    aberto_s: "int", gpu: "s",
    ajustes: {
      glass_ui: "int", qualidade_max: "s", tamanho_max_gb: "num",
      dts: "s", dv_mkv: "int", legenda_auto_sync: "int",
    },
    addons: { total: "int", ativos: "int", isolados: "int", tipos: ["s"] },
    debrid: ["s"],
    contas: ["s"],
  },
  midia: {
    container: "s", video: "s", hdr: "s", res: "s", fps: "num", mbps: "num",
    gb: "num", audio: "s", legenda: "s", entrega: "s", http: "int", ttfb_ms: "int",
  },
  cadeia: [{ passo: "s", estado: "s", ms: "int", http: "int" }],
  linha: [{ t: "int", m: "s", e: "s", a: "int", b: "int", k: "s" }],
  privacidade: { redator: "int", campos_recusados: "int" },
};

// true = valor dentro do esquema. `maxArr` vale quando `spec` e uma lista.
function valido(v, spec) {
  if (typeof spec === "string") {
    switch (spec) {
      case "s": return strOk(v, 48);
      case "s32": return strOk(v, 32);
      case "num": return numOk(v);
      case "int": return intOk(v);
      case "bool": return boolOk(v);
      default: return false;
    }
  }
  if (Array.isArray(spec)) {
    if (!Array.isArray(v)) return false;
    const lim = spec === ESQUEMA.linha ? LINHA_MAX : LISTA_MAX;
    if (v.length > lim) return false;
    return v.every((it) => valido(it, spec[0]));
  }
  if (typeof v !== "object" || v === null || Array.isArray(v)) return false;
  for (const k of Object.keys(v)) {
    if (!(k in spec)) return false;
    if (!valido(v[k], spec[k])) return false;
  }
  return true;
}

async function hmacHex(sal, texto) {
  const enc = new TextEncoder();
  const chave = await crypto.subtle.importKey(
    "raw", enc.encode(sal), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = await crypto.subtle.sign("HMAC", chave, enc.encode(texto));
  return [...new Uint8Array(sig)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

async function sha256Hex(s) {
  const b = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s));
  return [...new Uint8Array(b)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

// `quem` ja passou por quemE/registrar (conta autenticada, a mesma de
// /v1/registro). `corpo` e o JSON lido pelo index.js (teto cru la).
export async function rotaFarolRegistro(env, quem, corpo, h, limitar, codigoRegistro) {
  const { json, erro, agora } = h;
  if (typeof corpo !== "object" || corpo === null || Array.isArray(corpo) || corpo.v !== 1)
    return erro("pacote invalido", 422);
  if (!valido(corpo, ESQUEMA)) return erro("pacote fora do esquema", 422);
  const fingerprint = corpo.fingerprint;
  const codigo = corpo.codigo;
  if (!fingerprint || !codigo) return erro("pacote invalido", 422);

  const t = agora();
  // O HMAC e o que limita abuso sem guardar quem mandou (DECISOES.md item 4).
  // Sem FAROL_SAL configurado ainda nao vaza id: cai num SHA-256 com prefixo.
  const pessoa_h = env.FAROL_SAL
    ? await hmacHex(env.FAROL_SAL, quem.id)
    : await sha256Hex("farol:" + quem.id);
  const fp = (await sha256Hex(fingerprint)).slice(0, 16);

  // Dedupe ANTES dos tetos: reenvio da mesma falha nao gasta a cota.
  const ja = await env.DB.prepare(
    "SELECT id FROM farol_registro WHERE pessoa_h = ? AND fp = ? AND criado > ? LIMIT 1"
  ).bind(pessoa_h, fp, t - FAROL_DEDUP_JANELA).first();
  if (ja?.id) {
    await env.DB.prepare("UPDATE farol_registro SET vezes = vezes + 1 WHERE id = ?").bind(ja.id).run();
    return json({ ok: 1, codigo: codigoRegistro(ja.id), vence: t + FAROL_RETENCAO, dedup: 1 });
  }

  if (!(await limitar(env, "farol:h:" + pessoa_h, FAROL_HORA, 3600, t))) return erro("limite", 429);
  if (!(await limitar(env, "farol:d:" + pessoa_h, FAROL_DIA, 86400, t))) return erro("limite diario", 429);
  if (!(await limitar(env, "farol:dia", FAROL_DIA_GLOBAL, 86400, t))) return erro("limite diario", 429);

  const r = await env.DB.prepare(
    "INSERT INTO farol_registro (criado, pessoa_h, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1, ?)"
  ).bind(
    t, pessoa_h,
    String(corpo?.cliente?.versao || "").slice(0, 48),
    String(corpo?.aparelho?.plataforma || "").slice(0, 48),
    intOk(corpo?.aparelho?.so_maior) ? corpo.aparelho.so_maior : 0,
    codigo, fp, fingerprint,
    JSON.stringify(corpo)
  ).run();
  const id = r?.meta?.last_row_id ?? null;
  return json({ ok: 1, codigo: codigoRegistro(id), vence: t + FAROL_RETENCAO });
}

// Retencao de 30 dias — a promessa que a TV faz na tela de enviados. Em lotes
// como `registro` (#203): DELETE empedrado segura o unico escritor do D1.
const FAROL_LOTE = 200, FAROL_LOTES_MAX = 20;
export async function limparFarol(env, t) {
  for (let n = 0; n < FAROL_LOTES_MAX; n++) {
    const r = await env.DB.prepare(
      "DELETE FROM farol_registro WHERE id IN (SELECT id FROM farol_registro WHERE criado < ? LIMIT ?)"
    ).bind(t - FAROL_RETENCAO, FAROL_LOTE).run();
    if ((r.meta?.changes || 0) < FAROL_LOTE) break;
  }
}
