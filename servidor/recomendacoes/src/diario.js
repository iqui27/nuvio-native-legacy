// DIARIO POR PERFIL (cofre opcional). Plano: social-pessoal-plano.md 4.3, 6.5, 10.1.
// Tabelas: migracao-011-diario.sql. Mesma autenticacao das demais rotas (index.js
// resolve `quem` antes de chamar; `quem.id` ja inclui o perfil: nuvio:<sub>:<n>).
//
//   GET    /v1/diario/cofre            -> {"ligado":0|1}
//   POST   /v1/diario/cofre            {"ligado":0|1}   desligar APAGA tudo da pessoa
//   POST   /v1/diario/lote             {"itens":[...],"apagar":[{chave,quando}]}  (409 sem cofre)
//   GET    /v1/diario?desde=<ms>&limite=<n<=500>   -> {"itens":[...],"mais":0|1,"ate":<ms>}
//   DELETE /v1/diario                  apaga tudo, mantem o consentimento
//   GET    /v1/diario/letterboxd.csv[?parte=N]     CSV no formato de importacao deles
//   POST   /v1/diario/letterboxd       {"linhas":[...]}  (409 sem cofre)
//   GET    /v1/diario/pendentes        linhas do Letterboxd sem id
//   POST   /v1/diario/link             {"chave","quando"?,"url"}
//
// NUNCA buscamos pagina nenhuma do Letterboxd: so guardamos o link que a pessoa
// colou. Nada aqui chama a rede.
import { csvLetterboxd, dataParaEpoch, dataValida, epochParaData, linkReviewOk,
  tagsParaTexto, textoParaTags } from "./letterboxd.js";

export const DIARIO_CORPO_MAX = 64 * 1024;
export const DIARIO_CORPO_MAX_LB = 128 * 1024; // 500 linhas de titulo longo nao cabem em 64 KB
const LOTE_MAX = 200;
const LB_MAX = 500;
const PAGINA_MAX = 500;
const PENDENTES_MAX = 5000;
const CSV_PARTE_MAX = 1000000; // bytes, abaixo do 1 MB do importador
const CHAVE = /^[A-Za-z0-9:._-]{1,40}(_s\d{1,4}e\d{1,5})?$/;
const ORIGENS = new Set(["p", "t", "c", "l", "m"]);

const inteiro = (v, min, max, padrao) => {
  if (v === undefined || v === null) return padrao;
  return Number.isInteger(v) && v >= min && v <= max ? v : NaN;
};
const texto = (v, max) => {
  if (v === undefined || v === null) return "";
  if (typeof v !== "string") return null;
  const s = v.replace(/[\u0000-\u001f\u007f]/g, " ").trim();
  return s.length <= max ? s : null;
};

// Valida um item do lote. Devolve o item limpo ou null.
function itemOk(it, t) {
  if (!it || typeof it !== "object") return null;
  if (typeof it.chave !== "string" || !CHAVE.test(it.chave)) return null;
  if (!Number.isInteger(it.quando) || it.quando <= 0 || it.quando >= t + 2 * 86400) return null;
  const o = { chave: it.chave, quando: it.quando };
  o.nota10 = inteiro(it.nota10, 0, 10, 0);
  o.flags = inteiro(it.flags, 0, 511, 0);
  o.tags = inteiro(it.tags, 0, 4095, 0);
  o.cor = inteiro(it.cor, 0, 16777215, 0);
  o.seg = inteiro(it.seg, 0, 2147483647, 0);
  o.hora = inteiro(it.hora, -1, 10000000, -1);
  o.tmdb = inteiro(it.tmdb, 0, 2000000000, 0);
  o.dormiu = inteiro(it.dormiu, 0, 2147483647, 0);
  o.dur = inteiro(it.dur, 0, 2147483647, 0);
  o.com = inteiro(it.com, 0, 2147483647, 0);
  o.ano = inteiro(it.ano, 0, 3000, 0);
  o.origem = it.origem === undefined ? "p" : it.origem;
  if (typeof o.origem !== "string" || !ORIGENS.has(o.origem)) return null;
  o.titulo = texto(it.titulo, 160);
  if (o.titulo === null) return null;
  for (const k of ["nota10", "flags", "tags", "cor", "seg", "hora", "tmdb", "dormiu", "dur", "com", "ano"])
    if (Number.isNaN(o[k])) return null;
  return o;
}

async function cofreLigado(env, pessoa) {
  const r = await env.DB.prepare("SELECT ligado FROM diario_cofre WHERE pessoa = ?").bind(pessoa).first();
  return !!(r && r.ligado === 1);
}

// `atualizado` e estritamente crescente por pessoa: a pagina de restauracao corta por
// `atualizado > desde`, e dois lotes no mesmo milissegundo furariam a paginacao.
async function proximoAtualizado(env, pessoa) {
  const r = await env.DB.prepare("SELECT MAX(atualizado) AS m FROM diario WHERE pessoa = ?").bind(pessoa).first();
  return Math.max(Date.now(), (r && r.m ? r.m : 0) + 1);
}

const COLUNAS = "chave, quando, nota10, flags, tags, cor, seg, hora, origem, tmdb, dormiu, dur, com, titulo, ano, link, atualizado";

export async function rotaDiario(rota, metodo, env, quem, corpo, url, h) {
  if (rota !== "/v1/diario" && !rota.startsWith("/v1/diario/")) return null;
  const { json, erro, agora } = h;
  const pessoa = quem.id;
  const t = agora();

  if (rota === "/v1/diario/cofre") {
    if (metodo === "GET") return json({ ligado: (await cofreLigado(env, pessoa)) ? 1 : 0 });
    if (metodo !== "POST") return erro("metodo", 405);
    const v = corpo?.ligado;
    const lig = v === 1 || v === true ? 1 : v === 0 || v === false ? 0 : -1;
    if (lig < 0) return erro("ligado invalido", 400);
    const grava = env.DB.prepare(
      "INSERT INTO diario_cofre (pessoa, ligado, quando) VALUES (?, ?, ?) " +
      "ON CONFLICT(pessoa) DO UPDATE SET ligado = excluded.ligado, quando = excluded.quando").bind(pessoa, lig, t);
    // DESLIGAR APAGA: o consentimento e a unica razao de os dados estarem aqui.
    if (lig === 0) await env.DB.batch([
      env.DB.prepare("DELETE FROM diario WHERE pessoa = ?").bind(pessoa),
      env.DB.prepare("DELETE FROM diario_lb_pendente WHERE pessoa = ?").bind(pessoa), grava]);
    else await grava.run();
    return json({ ligado: lig });
  }

  if (rota === "/v1/diario" && metodo === "DELETE") {
    await env.DB.batch([
      env.DB.prepare("DELETE FROM diario WHERE pessoa = ?").bind(pessoa),
      env.DB.prepare("DELETE FROM diario_lb_pendente WHERE pessoa = ?").bind(pessoa)]);
    return json({ ok: 1 });
  }

  if (rota === "/v1/diario" && metodo === "GET") {
    let desde = 0, lim = PAGINA_MAX;
    const d = url.searchParams.get("desde"), l = url.searchParams.get("limite");
    if (d !== null) { if (!/^\d{1,15}$/.test(d)) return erro("desde invalido", 400); desde = parseInt(d, 10); }
    if (l !== null) { if (!/^\d{1,4}$/.test(l)) return erro("limite invalido", 400); lim = Math.min(PAGINA_MAX, Math.max(1, parseInt(l, 10))); }
    const r = await env.DB.prepare(
      `SELECT ${COLUNAS} FROM diario WHERE pessoa = ? AND atualizado > ? ORDER BY atualizado LIMIT ?`)
      .bind(pessoa, desde, lim + 1).all();
    const rows = r.results || [];
    const mais = rows.length > lim ? 1 : 0;
    const itens = rows.slice(0, lim);
    return json({ itens, mais, ate: itens.length ? itens[itens.length - 1].atualizado : desde });
  }

  if (rota === "/v1/diario/lote" && metodo === "POST") {
    const itens = corpo?.itens === undefined ? [] : corpo.itens;
    const apagar = corpo?.apagar === undefined ? [] : corpo.apagar;
    if (!Array.isArray(itens) || !Array.isArray(apagar)) return erro("lote invalido", 400);
    if (itens.length > LOTE_MAX || apagar.length > LOTE_MAX) return erro("lote grande", 413);
    const limpos = [];
    for (let i = 0; i < itens.length; i++) {
      const o = itemOk(itens[i], t);
      if (!o) return json({ erro: "item invalido", indice: i }, 400);
      limpos.push(o);
    }
    const dels = [];
    for (let i = 0; i < apagar.length; i++) {
      const a = apagar[i];
      if (!a || typeof a.chave !== "string" || !CHAVE.test(a.chave) || !Number.isInteger(a.quando) || a.quando <= 0)
        return json({ erro: "apagar invalido", indice: i }, 400);
      dels.push(a);
    }
    if (!(await cofreLigado(env, pessoa))) return erro("cofre", 409);
    let at = await proximoAtualizado(env, pessoa);
    const cmds = [];
    // O link ja guardado nao some quando a TV reenvia a linha (a TV nao conhece o
    // link), e a flag 128 acompanha o link.
    for (const o of limpos) {
      cmds.push(env.DB.prepare(
        `INSERT INTO diario (pessoa, ${COLUNAS.replace(" link,", "")}) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ` +
        "ON CONFLICT(pessoa, chave, quando) DO UPDATE SET nota10 = excluded.nota10, " +
        "flags = excluded.flags | (CASE WHEN diario.link <> '' THEN 128 ELSE 0 END), tags = excluded.tags, " +
        "cor = excluded.cor, seg = excluded.seg, hora = excluded.hora, origem = excluded.origem, tmdb = excluded.tmdb, " +
        "dormiu = excluded.dormiu, dur = excluded.dur, com = excluded.com, titulo = excluded.titulo, " +
        "ano = excluded.ano, atualizado = excluded.atualizado")
        .bind(pessoa, o.chave, o.quando, o.nota10, o.flags, o.tags, o.cor, o.seg, o.hora, o.origem, o.tmdb,
          o.dormiu, o.dur, o.com, o.titulo, o.ano, at++));
    }
    for (const a of dels)
      cmds.push(env.DB.prepare("DELETE FROM diario WHERE pessoa = ? AND chave = ? AND quando = ?").bind(pessoa, a.chave, a.quando));
    const res = cmds.length ? await env.DB.batch(cmds) : [];
    let apagadas = 0;
    for (let i = limpos.length; i < cmds.length; i++) apagadas += res[i]?.meta?.changes || 0;
    return json({ ok: 1, gravadas: limpos.length, apagadas });
  }

  if (rota === "/v1/diario/letterboxd.csv" && metodo === "GET") return csvExport(env, pessoa, url, h);

  if (rota === "/v1/diario/letterboxd" && metodo === "POST") {
    const linhas = corpo?.linhas;
    if (!Array.isArray(linhas)) return erro("linhas invalidas", 400);
    if (linhas.length > LB_MAX) return erro("lote grande", 413);
    const ok = [];
    for (let i = 0; i < linhas.length; i++) {
      const l = linhaLbOk(linhas[i]);
      if (!l) return json({ erro: "linha invalida", indice: i }, 400);
      ok.push(l);
    }
    if (!(await cofreLigado(env, pessoa))) return erro("cofre", 409);
    let at = await proximoAtualizado(env, pessoa);
    const cmds = [];
    let casadas = 0, pendentes = 0;
    const jaPend = (await env.DB.prepare("SELECT COUNT(*) AS n FROM diario_lb_pendente WHERE pessoa = ?").bind(pessoa).first())?.n || 0;
    for (const l of ok) {
      const flags = 16 | (l.rever ? 1 : 0) | (l.link ? 128 : 0);
      if (l.chave) {
        casadas++;
        cmds.push(env.DB.prepare(
          "INSERT INTO diario (pessoa, chave, quando, nota10, flags, tags, origem, tmdb, titulo, ano, link, atualizado) " +
          "VALUES (?, ?, ?, ?, ?, ?, 'l', ?, ?, ?, ?, ?) " +
          "ON CONFLICT(pessoa, chave, quando) DO UPDATE SET " +
          "nota10 = CASE WHEN excluded.nota10 > 0 THEN excluded.nota10 ELSE diario.nota10 END, " +
          "flags = diario.flags | excluded.flags, tags = diario.tags | excluded.tags, " +
          "tmdb = CASE WHEN excluded.tmdb > 0 THEN excluded.tmdb ELSE diario.tmdb END, " +
          "titulo = CASE WHEN diario.titulo = '' THEN excluded.titulo ELSE diario.titulo END, " +
          "ano = CASE WHEN diario.ano = 0 THEN excluded.ano ELSE diario.ano END, " +
          "link = CASE WHEN diario.link <> '' THEN diario.link ELSE excluded.link END, atualizado = excluded.atualizado")
          .bind(pessoa, l.chave, l.quando, l.nota10, flags, l.tags, l.tmdb, l.titulo, l.ano, l.link, at++));
      } else {
        if (jaPend + pendentes >= PENDENTES_MAX) return erro("pendentes cheio", 409);
        pendentes++;
        cmds.push(env.DB.prepare(
          "INSERT INTO diario_lb_pendente (pessoa, titulo, ano, quando, nota10, flags, tags, link, criado) " +
          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(pessoa, titulo, ano, quando) DO UPDATE SET " +
          "nota10 = excluded.nota10, flags = excluded.flags, tags = excluded.tags, link = excluded.link")
          .bind(pessoa, l.titulo, l.ano, l.quando, l.nota10, flags, l.tags, l.link, t));
      }
    }
    if (cmds.length) await env.DB.batch(cmds);
    return json({ casadas, pendentes });
  }

  if (rota === "/v1/diario/pendentes" && metodo === "GET") {
    const r = await env.DB.prepare(
      "SELECT titulo, ano, quando, nota10, flags, tags, link FROM diario_lb_pendente WHERE pessoa = ? " +
      "ORDER BY quando DESC, titulo LIMIT ?").bind(pessoa, PAGINA_MAX).all();
    const n = (await env.DB.prepare("SELECT COUNT(*) AS n FROM diario_lb_pendente WHERE pessoa = ?").bind(pessoa).first())?.n || 0;
    return json({ total: n, itens: r.results || [] });
  }

  if (rota === "/v1/diario/link" && metodo === "POST") {
    const chave = corpo?.chave, u = corpo?.url;
    if (typeof chave !== "string" || !CHAVE.test(chave)) return erro("chave invalida", 400);
    if (!linkReviewOk(u)) return erro("link invalido", 400);
    let quando = corpo?.quando;
    if (quando !== undefined && (!Number.isInteger(quando) || quando <= 0)) return erro("quando invalido", 400);
    if (quando === undefined) {
      const r = await env.DB.prepare("SELECT quando FROM diario WHERE pessoa = ? AND chave = ? ORDER BY quando DESC LIMIT 1")
        .bind(pessoa, chave).first();
      if (!r) return erro("entrada desconhecida", 404);
      quando = r.quando;
    }
    const at = await proximoAtualizado(env, pessoa);
    const r = await env.DB.prepare(
      "UPDATE diario SET link = ?, flags = flags | 128, atualizado = ? WHERE pessoa = ? AND chave = ? AND quando = ?")
      .bind(u, at, pessoa, chave, quando).run();
    if (!(r?.meta?.changes > 0)) return erro("entrada desconhecida", 404);
    return json({ ok: 1, quando });
  }

  return erro("rota desconhecida", 404);
}

// Linha de importacao do Letterboxd. `chave` vazia = nao casou com id.
function linhaLbOk(l) {
  if (!l || typeof l !== "object") return null;
  const titulo = texto(l.titulo, 160);
  if (!titulo) return null;
  const ano = inteiro(l.ano, 0, 3000, 0);
  if (Number.isNaN(ano)) return null;
  if (!dataValida(l.data)) return null;
  const nota10 = inteiro(l.nota10, 0, 10, 0);
  if (Number.isNaN(nota10)) return null;
  const rever = l.rever === undefined ? 0 : l.rever === 1 || l.rever === true ? 1 : l.rever === 0 || l.rever === false ? 0 : -1;
  if (rever < 0) return null;
  if (l.tags !== undefined && (typeof l.tags !== "string" || l.tags.length > 400)) return null;
  let link = "";
  if (l.link !== undefined && l.link !== "") {
    if (!linkReviewOk(l.link)) return null;
    link = l.link;
  }
  let chave = "", tmdb = 0;
  if (l.imdb !== undefined && l.imdb !== "") {
    if (typeof l.imdb !== "string" || !/^tt\d{1,10}$/.test(l.imdb)) return null;
    chave = l.imdb;
  }
  if (l.tmdb !== undefined && l.tmdb !== 0 && l.tmdb !== "") {
    const n = typeof l.tmdb === "string" && /^\d{1,10}$/.test(l.tmdb) ? parseInt(l.tmdb, 10) : l.tmdb;
    if (!Number.isInteger(n) || n < 1 || n > 2000000000) return null;
    tmdb = n;
    if (!chave) chave = "tmdb:" + n;
  }
  return { titulo, ano, quando: dataParaEpoch(l.data), nota10, rever, tags: textoParaTags(l.tags), link, chave, tmdb };
}

async function csvExport(env, pessoa, url, h) {
  const { erro } = h;
  let parte = 1;
  const p = url.searchParams.get("parte");
  if (p !== null) { if (!/^\d{1,3}$/.test(p) || p === "0" || /^0\d/.test(p)) return erro("parte invalida", 400); parte = parseInt(p, 10); }
  const r = await env.DB.prepare(
    "SELECT chave, quando, nota10, flags, tags, tmdb, titulo, ano FROM diario " +
    "WHERE pessoa = ? AND chave NOT GLOB '*_s[0-9]*e[0-9]*' AND (flags & 256) = 0 ORDER BY quando, chave LIMIT 50000")
    .bind(pessoa).all();
  const linhas = (r.results || []).filter((x) => !/_s\d+e\d+$/.test(x.chave)).map((x) => {
    let imdb = "", tmdb = x.tmdb || "";
    if (/^tt\d+$/.test(x.chave)) imdb = x.chave;
    else if (/^tmdb:\d+$/.test(x.chave)) tmdb = parseInt(x.chave.slice(5), 10);
    return { titulo: x.titulo, ano: x.ano || "", imdb, tmdb, data: epochParaData(x.quando), nota10: x.nota10,
      rever: x.flags & 1, tags: tagsParaTexto(x.tags), review: "" };
  });
  // Cortes por tamanho em bytes: cada parte leva o cabecalho e fica sob 1 MB.
  const partes = [];
  let atual = [], bytes = 0;
  const enc = new TextEncoder();
  for (const l of linhas) {
    const b = enc.encode(csvLetterboxd([l], false)).length;
    if (atual.length && bytes + b > CSV_PARTE_MAX) { partes.push(atual); atual = []; bytes = 0; }
    atual.push(l); bytes += b;
  }
  if (atual.length || !partes.length) partes.push(atual);
  if (parte > partes.length) return erro("parte inexistente", 404);
  return new Response("﻿" + csvLetterboxd(partes[parte - 1]), { status: 200, headers: {
    "content-type": "text/csv; charset=utf-8",
    "content-disposition": `attachment; filename="nuvio-letterboxd-${parte}.csv"`,
    "x-partes": String(partes.length),
    "access-control-allow-origin": "*",
    "access-control-expose-headers": "x-partes, content-disposition",
    "cache-control": "no-store" } });
}
