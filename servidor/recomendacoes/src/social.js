import { conquistasDe } from "./conquista.js";
// Redesenho do Social (02/10/2026): pessoa por perfil, nome de exibicao,
// nivel de atividade, eventos do player, feed, perfil do amigo.
// Contrato com a TV: docs/social-contrato.md. Tabelas: migracao-006-social.sql.
//
// A REGRA QUE TUDO AQUI OBEDECE: `pessoa.alcance` decide QUEM ve a atividade
// de alguem, e e conferido no servidor a cada escrita E a cada leitura.
//   -1 nao respondeu (vale 0) | 0 ninguem | 1 so amigos | 2 amigos de amigos
// Nada e gravado com alcance < 1, e baixar para 0 apaga o que havia.

const EVENTOS = new Set(["inicio", "progresso", "fim", "abandono", "reacao", "salvo", "junto"]);
// O FEED SO MOSTRA O QUE DIZ ALGO (dono, 06/10/2026: "muito sujo"): terminou,
// reagiu e — so na primeira vez de uma serie — comecou. "abandono" (saiu do
// player), "salvo" e o "inicio" de cada episodio/filme ficam fora: o "agora"
// ja diz quem esta vendo. As linhas antigas continuam na tabela, so nao saem.
const NO_FEED = ["inicio", "fim", "reacao", "junto"];
// Nao vira linha da tabela `evento`: e so sinal (fecha o "agora", soma tempo).
// "abandono" e o cliente antigo; o novo manda "progresso" com parou=1.
const SO_SINAL = new Set(["progresso", "abandono"]);
const FEED_BRUTO = 150;             // linhas lidas antes de agrupar (sai FEED_MAX)
const DEDUPE_FEED_S = 3600;          // junto: mesmo titulo na mesma hora = uma linha
const AGORA_S = 15 * 60;            // "assistindo agora" vence em 15 min
const RETENCAO = 90 * 86400;
const SEG_MAX = 4 * 3600;           // um trecho nao passa de 4 h
const FEED_MAX = 50;
const GOSTOU_MAX = 10;
const RECS_MAX = 20;
const DEDUPE_S = 600;               // mesmo evento do mesmo titulo em 10 min = um so
const NOME_MAX = 32;

// NOME: letras (qualquer alfabeto), numeros, espaco, apostrofo e hifen. Sem
// ponto, arroba, barra ou dois-pontos — nao cabe e-mail, link nem @usuario.
export function limparNome(s, max = NOME_MAX) {
  return String(s == null ? "" : s)
    .normalize("NFC")
    .replace(/[^\p{L}\p{N} '\-]+/gu, " ")
    .replace(/\s+/g, " ")
    .trim()
    .slice(0, max)
    .trim();
}

// FOTO DO PERFIL: so https e so de host conhecido. O host do Supabase do Nuvio
// e de onde saem os avatares do catalogo e as fotos enviadas
// (<url>/storage/v1/object/public/avatars/...; ver perfis.c); o Trakt pelo
// mesmo motivo de amigos.js. Qualquer outra URL vira "": uma URL livre
// deixaria alguem apontar a propria foto para um servidor dele e ler o IP de
// todo amigo que abrisse a lista.
export function avatarPerfilOk(url, env) {
  if (typeof url !== "string" || url.length > 512) return "";
  let u;
  try { u = new URL(url); } catch { return ""; }
  if (u.protocol !== "https:") return "";
  const hosts = new Set(["walter.trakt.tv", "media.trakt.tv", "trakt.tv"]);
  try { if (env?.SUPABASE_URL) hosts.add(new URL(env.SUPABASE_URL).host); } catch {}
  for (const h of String(env?.AVATAR_HOSTS || "").split(",")) if (h.trim()) hosts.add(h.trim());
  return hosts.has(u.host) ? u.toString() : "";
}

// CAPA NO FEED: mesma ideia, outra lista. Quem le o feed baixa a capa na TV
// dele; um host livre seria um rastreador de IP para amigos de amigos.
const POSTER_HOSTS = new Set([
  "image.tmdb.org", "images.metahub.space", "live.metahub.space",
  "episodes.metahub.space", "m.media-amazon.com", "artworks.thetvdb.com",
  "walter.trakt.tv", "media.trakt.tv",
]);
export function posterOk(url) {
  if (typeof url !== "string" || !url || url.length > 512) return "";
  try {
    const u = new URL(url);
    return u.protocol === "https:" && POSTER_HOSTS.has(u.host) ? u.toString() : "";
  } catch { return ""; }
}

// O NOME PRONTO, na ordem: o que a pessoa digitou; depois o do perfil (Nuvio)
// ou o da conta (Trakt, onde o nome e da pessoa e nao do aparelho); por fim
// "Amigo #<n>", com n = rowid da linha. NUNCA o id.
export function resolverNome(p, id) {
  const ex = p.exibicao || "";
  if (ex) return ex;
  const conta = p.nome_conta || "", perfil = p.nome_perfil || "";
  const n = String(id || "").startsWith("trakt:") ? (conta || perfil) : (perfil || conta);
  return n || `Amigo #${p.n || 0}`;
}

export const nivelDe = (p) => (p && Number.isInteger(p.alcance) && p.alcance > 0 ? p.alcance : 0);

const mesDe = (t) => new Date(t * 1000).toISOString().slice(0, 7);
const int = (v, a, b) => {
  const n = Number.isFinite(v) ? Math.round(v) : parseInt(v, 10);
  return Number.isFinite(n) ? Math.max(a, Math.min(b, n)) : a;
};

// --- nome e nivel ---------------------------------------------------------------

// `recalcular` e o registrar com a identidade VERIFICADA (quemBruto), e nao
// com a saida de um registrar anterior: aquela traz o nome JA RESOLVIDO, e
// grava-lo como nome da conta faria a exibicao apagada "grudar".
export async function rotaEuNome(env, quem, corpo, h, recalcular) {
  const nome = limparNome(corpo?.nome);
  await env.DB.prepare("UPDATE pessoa SET exibicao = ? WHERE id = ?").bind(nome, quem.id).run();
  const eu = await recalcular();                   // recalcula `pessoa.nome`
  return h.json({ ok: 1, nome: eu.nome, exibicao: nome });
}

export async function rotaAlcance(env, quem, corpo, h) {
  const n = Number.isInteger(corpo?.nivel) ? Math.max(0, Math.min(2, corpo.nivel)) : 0;
  const cmds = [env.DB.prepare("UPDATE pessoa SET alcance = ? WHERE id = ?").bind(n, quem.id)];
  // DESLIGAR APAGA. Nao basta esconder: "ninguem" tem de valer tambem para um
  // erro de consulta futuro.
  if (n === 0) cmds.push(
    env.DB.prepare("DELETE FROM evento WHERE pessoa = ?").bind(quem.id),
    env.DB.prepare("DELETE FROM agora WHERE pessoa = ?").bind(quem.id),
    env.DB.prepare("DELETE FROM agregado WHERE pessoa = ?").bind(quem.id),
    env.DB.prepare("DELETE FROM agregado_titulo WHERE pessoa = ?").bind(quem.id),
  );
  await env.DB.batch(cmds);
  return h.json({ ok: 1, alcance: n });
}

// --- POST /v1/atividade (corpo com "ev") ----------------------------------------

export async function rotaEvento(env, quem, corpo, h, limitar) {
  const t = h.agora();
  const ev = String(corpo?.ev || "");
  if (!EVENTOS.has(ev)) return h.erro("ev invalido", 400);
  const imdb = h.limpar(corpo?.imdb, 16);
  if (!/^tt\d{1,10}$/.test(imdb)) return h.erro("imdb invalido", 400);
  if (!(await limitar(env, `ev:${quem.id}`, 600, 3600, t))) return h.erro("limite", 429);
  const eu = await env.DB.prepare("SELECT alcance FROM pessoa WHERE id = ?").bind(quem.id).first();
  // CONSENTIMENTO AQUI, nao so na TV: cliente velho, com defeito ou de ma-fe
  // que mande evento com o nivel em 0 nao grava NADA — nem agregado, nem rec.
  if (nivelDe(eu) < 1) return h.json({ ok: 1, guardado: 0 });

  const midia = corpo?.midia === "series" ? "series" : "movie";
  const titulo = h.limpar(corpo?.titulo, 160).replace(/[\u0000-\u001f]/g, " ");
  const poster = posterOk(corpo?.poster);
  const temporada = int(corpo?.temporada, 0, 9999);
  const episodio = int(corpo?.episodio, 0, 99999);
  const pct = int(corpo?.pct, 0, 100);
  // "junto" nao e tempo de tela: os "progresso" da mesma sessao ja contaram.
  const seg = ev === "junto" ? 0 : int(corpo?.seg, 0, SEG_MAX);
  const reacao = ev === "reacao" ? int(corpo?.reacao, -1, 1) : 0;
  const rec = int(corpo?.rec, 0, 2 ** 31 - 1);
  const mes = mesDe(t);
  const cmds = [];

  if (seg > 0) cmds.push(env.DB.prepare(
    "INSERT INTO agregado (pessoa, mes, seg) VALUES (?, ?, ?) " +
    "ON CONFLICT(pessoa, mes) DO UPDATE SET seg = agregado.seg + excluded.seg"
  ).bind(quem.id, mes, seg));
  if (midia === "series" && (ev === "inicio" || ev === "progresso" || ev === "fim"))
    cmds.push(env.DB.prepare("INSERT OR IGNORE INTO agregado_titulo VALUES (?, ?, ?, 'serie')")
      .bind(quem.id, mes, imdb));
  if (midia === "movie" && ev === "fim")
    cmds.push(env.DB.prepare("INSERT OR IGNORE INTO agregado_titulo VALUES (?, ?, ?, 'filme')")
      .bind(quem.id, mes, imdb));

  // SAIU DO PLAYER: o cliente novo manda "progresso" com parou=1 (o antigo,
  // "abandono"). Fecha o "agora" em vez de renova-lo por mais 15 min.
  const parou = ev === "abandono" || (ev === "progresso" && (corpo?.parou === 1 || corpo?.parou === true));
  if ((ev === "inicio" || ev === "progresso") && !parou) cmds.push(env.DB.prepare(
    "INSERT INTO agora (pessoa, imdb, midia, titulo, poster, temporada, episodio, pct, atualizado) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(pessoa) DO UPDATE SET imdb = excluded.imdb, " +
    "midia = excluded.midia, titulo = excluded.titulo, poster = excluded.poster, " +
    "temporada = excluded.temporada, episodio = excluded.episodio, pct = excluded.pct, " +
    "atualizado = excluded.atualizado"
  ).bind(quem.id, imdb, midia, titulo, poster, temporada, episodio, pct, t));
  if (ev === "fim" || parou)
    cmds.push(env.DB.prepare("DELETE FROM agora WHERE pessoa = ? AND imdb = ?").bind(quem.id, imdb));

  let gravou = 0;
  if (!SO_SINAL.has(ev)) {
    // Retomar o mesmo episodio tres vezes em dez minutos nao sao tres "comecou".
    // Reacao so repete a ULTIMA: gostei -> nao gostei -> gostei precisa guardar
    // a mudanca final, mesmo que o primeiro gostei ainda esteja na janela.
    const ja = await env.DB.prepare(
      "SELECT reacao FROM evento WHERE pessoa = ? AND ev = ? AND imdb = ? AND midia = ? " +
      "AND temporada = ? AND episodio = ? AND criado > ? ORDER BY id DESC LIMIT 1"
    ).bind(quem.id, ev, imdb, midia, temporada, episodio, t - DEDUPE_S).first();
    if (!ja || (ev === "reacao" && ja.reacao !== reacao)) {
      cmds.push(env.DB.prepare(
        "INSERT INTO evento (pessoa, ev, imdb, midia, titulo, poster, temporada, episodio, pct, reacao, rec, criado) " +
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
      ).bind(quem.id, ev, imdb, midia, titulo, poster, temporada, episodio, pct, reacao, rec, t));
      gravou = 1;
    }
  }

  // A REC DE ORIGEM: so a que FOI MANDADA PARA MIM (para = quem.id). Um id de
  // rec alheio nao muda nada.
  if (rec > 0) {
    if (ev === "inicio" || ev === "progresso")
      cmds.push(env.DB.prepare("UPDATE rec SET comecou = 1 WHERE id = ? AND para = ?").bind(rec, quem.id));
    else if (ev === "fim")
      cmds.push(env.DB.prepare("UPDATE rec SET comecou = 1, terminou = 1 WHERE id = ? AND para = ?").bind(rec, quem.id));
    else if (ev === "reacao")
      cmds.push(env.DB.prepare("UPDATE rec SET reacao = ? WHERE id = ? AND para = ?").bind(reacao, rec, quem.id));
  }
  if (cmds.length) await env.DB.batch(cmds);
  return h.json({ ok: 1, guardado: 1, evento: gravou });
}

// --- quem eu posso ver -----------------------------------------------------------
//
// AMIGO (grau 1): contato com alcance >= 1. AMIGO DE AMIGO (grau 2): contato
// de um contato meu, que NAO e meu contato, com alcance = 2. Bloqueio entre
// mim e a pessoa (qualquer sentido) tira ela das duas listas.
const VISIVEIS =
  "WITH vis AS (" +
  " SELECT c.b AS pessoa, 1 AS grau, '' AS via FROM contato c JOIN pessoa p ON p.id = c.b " +
  "  WHERE c.a = ?1 AND p.alcance >= 1 " +
  " UNION ALL " +
  " SELECT c2.b AS pessoa, 2 AS grau, MIN(v.nome) AS via FROM contato c1 " +
  "  JOIN contato c2 ON c2.a = c1.b JOIN pessoa p ON p.id = c2.b JOIN pessoa v ON v.id = c1.b " +
  "  WHERE c1.a = ?1 AND c2.b <> ?1 AND p.alcance = 2 " +
  "  AND NOT EXISTS (SELECT 1 FROM contato x WHERE x.a = ?1 AND x.b = c2.b) " +
  "  GROUP BY c2.b" +
  "), visl AS (SELECT * FROM vis WHERE NOT EXISTS (SELECT 1 FROM bloqueio bq WHERE " +
  " (bq.quem = ?1 AND bq.alvo = vis.pessoa) OR (bq.quem = vis.pessoa AND bq.alvo = ?1))) ";

// Id que sai para FORA: o do contato como e; o do amigo de amigo vira handle
// opaco, como nas sugestoes (o `nuvio:<uuid>` nao se entrega a quem nao e amigo).
async function idPublico(env, x, garantirPerfil) {
  if (x.grau === 1 || String(x.pessoa).startsWith("trakt:")) return x.pessoa;
  const pub = x.pub || (await garantirPerfil(env, x.pessoa))?.pub || "";
  return `pub:${pub}`;
}

// --- GET /v1/feed ------------------------------------------------------------------

export async function rotaFeed(env, quem, url, req, h, garantirPerfil) {
  const desde = Math.max(0, parseInt(url.searchParams.get("desde") || "0", 10) || 0);
  const marcas = NO_FEED.map((_, i) => `?${i + 5}`).join(",");
  // "COMECOU" SO A PRIMEIRA VEZ DE UMA SERIE: um inicio de serie sem nenhum
  // inicio/fim anterior da mesma pessoa no mesmo titulo. Filme comecado nao e
  // noticia (o "agora" ja mostra), episodio 2, 3, 4... tambem nao.
  const r = await env.DB.prepare(
    VISIVEIS +
    "SELECT e.id, e.pessoa, e.ev, e.imdb, e.midia, e.titulo, e.poster, e.temporada, e.episodio, " +
    "e.pct, e.reacao, e.criado, visl.grau, visl.via, p.nome AS nome, p.avatar AS avatar, f.pub AS pub " +
    "FROM evento e JOIN visl ON visl.pessoa = e.pessoa JOIN pessoa p ON p.id = e.pessoa " +
    "LEFT JOIN perfil f ON f.pessoa = e.pessoa " +
    `WHERE e.id > ?2 AND e.criado > ?3 AND e.ev IN (${marcas}) ` +
    "AND (e.ev <> 'inicio' OR (e.midia = 'series' AND NOT EXISTS (SELECT 1 FROM evento o " +
    " WHERE o.pessoa = e.pessoa AND o.imdb = e.imdb AND o.id < e.id AND o.ev IN ('inicio', 'fim')))) " +
    "ORDER BY e.id DESC LIMIT ?4"
  ).bind(quem.id, desde, h.agora() - RETENCAO, FEED_BRUTO, ...NO_FEED).all();
  const linhas = r.results || [];
  const maior = linhas.reduce((m, x) => (x.id > m ? x.id : m), desde);
  // ETag pelo maior id E pela quantidade: alguem que baixa o nivel tira linhas
  // e muda a contagem, entao a TV nao fica com a lista velha num 304.
  const etag = `"f-${maior}-${linhas.length}"`;
  if (req.headers.get("if-none-match") === etag)
    return new Response(null, { status: 304, headers: { etag } });
  const itens = [];
  for (const g of agruparFeed(linhas)) {
    if (itens.length >= FEED_MAX) break;
    const x = g.linha;
    const item = {
      id: x.id, de: await idPublico(env, x, garantirPerfil),
      deNome: x.nome || "", deAvatar: x.grau === 1 ? (x.avatar || "") : "",
      grau: x.grau, via: x.grau === 2 ? (x.via || "") : "",
      ev: x.ev, imdb: x.imdb, midia: x.midia, titulo: x.titulo, poster: x.poster,
      temporada: x.temporada, episodio: x.episodio, pct: x.pct,
      reacao: x.reacao, criado: x.criado,
    };
    // Episodios distintos concluidos (serie): "assistiu 3 episodios de X".
    if (g.eps > 1) item.eps = g.eps;
    // A reacao que veio junto do "terminou" (a mesma pessoa no mesmo titulo):
    // uma linha "terminou e gostou" no lugar de duas.
    if (x.ev === "fim" && g.reac !== null) { item.reac = g.reac; item.reacao = g.reac; }
    itens.push(item);
  }
  // "ASSISTIRAM JUNTOS" (Watch Together, decisao 8): cada membro manda o
  // proprio evento `junto` no fim da sessao; aqui os do mesmo titulo/episodio
  // na mesma hora viram UMA linha com os nomes em `com`. So entra quem esta em
  // `linhas` — ou seja, quem tem alcance ligado e e visivel para quem le; os
  // outros simplesmente nao aparecem no "com".
  const juntos = [];
  for (const it of itens) {
    if (it.ev !== "junto") { juntos.push(it); continue; }
    const par = juntos.find((o) => o.ev === "junto" && o.imdb === it.imdb && o.temporada === it.temporada &&
      o.episodio === it.episodio && Math.abs(o.criado - it.criado) <= DEDUPE_FEED_S);
    if (par) { (par.com ||= []).push({ de: it.de, nome: it.deNome }); continue; }
    juntos.push(it);
  }
  return h.json({ cursor: maior, itens: juntos }, 200, { etag });
}

// UMA LINHA POR PESSOA E TITULO (dono, 06/10/2026). Entra `linhas` do mais
// novo para o mais velho; sai, na ordem da linha mais nova de cada grupo, a
// linha que mais diz: terminou (com a contagem de episodios e a reacao mais
// nova) > reagiu (a mais nova: mudar de ideia nao vira duas linhas) > comecou.
// A linha fica com o id e a hora da mais nova do grupo, para a ordem do feed.
export function agruparFeed(linhas) {
  const grupos = new Map();
  const ordem = [];
  for (const x of linhas) {
    // `junto` (Watch Together) nao agrupa: cada linha segue sozinha e e
    // fundida por titulo/hora em rotaFeed.
    if (x.ev === "junto") { ordem.push({ solta: x }); continue; }
    const k = `${x.pessoa}|${x.imdb}|${x.midia}`;
    let g = grupos.get(k);
    if (!g) { g = { fim: null, reacao: null, inicio: null, eps: new Set(), id: x.id, criado: x.criado }; grupos.set(k, g); ordem.push(g); }
    if (x.ev === "fim") { if (!g.fim) g.fim = x; g.eps.add(`${x.temporada}:${x.episodio}`); }
    else if (x.ev === "reacao") { if (!g.reacao) g.reacao = x; }
    else if (x.ev === "inicio") { if (!g.inicio) g.inicio = x; }
  }
  return ordem.map((g) => {
    if (g.solta) return { linha: g.solta, eps: 0, reac: null };
    const base = g.fim || g.reacao || g.inicio;
    return {
      linha: { ...base, id: g.id, criado: g.criado },
      eps: g.fim && g.fim.midia === "series" ? g.eps.size : 0,
      reac: g.reacao ? g.reacao.reacao : null,
    };
  });
}

// --- GET /v1/amigo?id= --------------------------------------------------------------

function estadoRec(x) {
  if (x.reacao !== null && x.reacao !== undefined) return "reacao";
  if (x.terminou) return "terminou";
  if (x.comecou) return "comecou";
  if (x.visto || x.aberto) return "aberta";
  return "entregue";
}

export async function rotaAmigo(env, quem, url, h, garantirPerfil) {
  const t = h.agora();
  const pedido = String(url.searchParams.get("id") || "").slice(0, 96);
  if (!pedido) return h.erro("sem id", 400);
  let alvo = pedido;
  if (pedido.startsWith("pub:")) {
    const f = await env.DB.prepare("SELECT pessoa FROM perfil WHERE pub = ?")
      .bind(pedido.slice(4).replace(/[^a-z0-9]/g, "").slice(0, 10)).first();
    if (!f) return h.erro("nao encontrado", 404);
    alvo = f.pessoa;
  }
  if (alvo === quem.id) return h.erro("nao encontrado", 404);
  const bloq = await env.DB.prepare(
    "SELECT 1 FROM bloqueio WHERE (quem = ? AND alvo = ?) OR (quem = ? AND alvo = ?)"
  ).bind(quem.id, alvo, alvo, quem.id).first();
  if (bloq) return h.erro("nao encontrado", 404);

  const p = await env.DB.prepare(
    "SELECT rowid AS n, id, nome, avatar, alcance FROM pessoa WHERE id = ?").bind(alvo).first();
  if (!p) return h.erro("nao encontrado", 404);
  const c = await env.DB.prepare("SELECT criado, via FROM contato WHERE a = ? AND b = ?")
    .bind(quem.id, alvo).first();
  let grau = c ? 1 : 0, via = "";
  if (!c) {
    // Amigo de amigo so e visivel a quem ELE autorizou (alcance 2) — e o mesmo
    // 404 de "nao existe" para quem nao pode ver.
    const v = await env.DB.prepare(
      "SELECT MIN(v.nome) AS via FROM contato c1 JOIN contato c2 ON c2.a = c1.b " +
      "JOIN pessoa v ON v.id = c1.b WHERE c1.a = ? AND c2.b = ?"
    ).bind(quem.id, alvo).first();
    if (!v?.via || nivelDe(p) !== 2) return h.erro("nao encontrado", 404);
    grau = 2; via = v.via;
  }
  const nivel = nivelDe(p);
  const compartilha = grau === 1 ? nivel >= 1 : nivel === 2;
  let origem = c?.via || "";
  if (c && !origem) origem = alvo.startsWith("trakt:") && quem.id.startsWith("trakt:") ? "trakt" : "";

  const saida = {
    id: grau === 1 ? alvo : await idPublico(env, { pessoa: alvo, grau }, garantirPerfil),
    nome: p.nome || `Amigo #${p.n}`,
    avatar: grau === 1 ? (p.avatar || "") : "",
    grau, via, desde: c?.criado || 0, origem,
    compartilha: compartilha ? 1 : 0,
    mes: null, agora: null, gostou: [], recs: [], gosto: null,
  };

  if (compartilha) {
    const mes = mesDe(t);
    const ag = await env.DB.prepare("SELECT seg FROM agregado WHERE pessoa = ? AND mes = ?")
      .bind(alvo, mes).first();
    const tt = await env.DB.prepare(
      "SELECT SUM(tipo = 'filme') AS filmes, SUM(tipo = 'serie') AS series " +
      "FROM agregado_titulo WHERE pessoa = ? AND mes = ?").bind(alvo, mes).first();
    saida.mes = { mes, seg: ag?.seg || 0, filmes: tt?.filmes || 0, series: tt?.series || 0 };
    const a = await env.DB.prepare(
      "SELECT imdb, midia, titulo, poster, temporada, episodio, pct, atualizado FROM agora " +
      "WHERE pessoa = ? AND atualizado > ?").bind(alvo, t - AGORA_S).first();
    saida.agora = a || null;
    // Ate 6 conquistas visiveis (conquista.js confere o alcance do dono).
    saida.conquistas = await conquistasDe(env, alvo);
    const g = await env.DB.prepare(
      "SELECT imdb, midia, titulo, poster, temporada, episodio, criado FROM evento " +
      "WHERE id IN (SELECT MAX(id) FROM evento WHERE pessoa = ?1 AND ev = 'reacao' " +
      "GROUP BY imdb, midia, temporada, episodio) AND reacao = 1 AND criado > ?2 " +
      "ORDER BY criado DESC, id DESC LIMIT ?3").bind(alvo, t - RETENCAO, GOSTOU_MAX).all();
    saida.gostou = g.results || [];
    // GOSTO PARECIDO so entre dois que compartilham: quem nao mostra as
    // proprias reacoes nao ganha uma janela para as dos outros.
    const eu = await env.DB.prepare("SELECT alcance FROM pessoa WHERE id = ?").bind(quem.id).first();
    if (nivelDe(eu) >= 1) {
      const ULT = (q) =>
        `SELECT imdb, midia, reacao FROM evento WHERE id IN (SELECT MAX(id) FROM evento ` +
        `WHERE pessoa = ${q} AND ev = 'reacao' AND criado > ?3 GROUP BY imdb)`;
      // POR MIDIA (F08): a midia da reacao mais nova de quem esta olhando.
      const r = await env.DB.prepare(
        `SELECT a.midia AS midia, COUNT(*) AS total, COALESCE(SUM(a.reacao = b.reacao), 0) AS iguais ` +
        `FROM (${ULT("?1")}) a JOIN (${ULT("?2")}) b ON a.imdb = b.imdb GROUP BY a.midia`
      ).bind(quem.id, alvo, t - RETENCAO).all();
      const por = { movie: { total: 0, iguais: 0 }, series: { total: 0, iguais: 0 } };
      for (const x of r.results || []) {
        const k = x.midia === "series" ? "series" : "movie";
        por[k].total += x.total || 0; por[k].iguais += x.iguais || 0;
      }
      const total = por.movie.total + por.series.total, iguais = por.movie.iguais + por.series.iguais;
      // COBERTURA: quantas reacoes cada um tem na janela. E o que separa "pouco
      // em comum" de "um dos dois quase nao reage".
      const cob = await env.DB.prepare(
        "SELECT pessoa, COUNT(DISTINCT imdb) AS n FROM evento WHERE pessoa IN (?1, ?2) AND ev = 'reacao' " +
        "AND criado > ?3 GROUP BY pessoa").bind(quem.id, alvo, t - RETENCAO).all();
      const nDe = (id) => (cob.results || []).find((x) => x.pessoa === id)?.n || 0;
      // EM COMUM: titulos que os dois concluiram na janela (filme visto; serie
      // com pelo menos um episodio concluido pelos dois).
      const FIM = (q) => `SELECT DISTINCT imdb, midia FROM evento WHERE pessoa = ${q} AND ev = 'fim' AND criado > ?3`;
      const cm = await env.DB.prepare(
        `SELECT a.midia AS midia, COUNT(*) AS n FROM (${FIM("?1")}) a JOIN (${FIM("?2")}) b ` +
        `ON a.imdb = b.imdb AND a.midia = b.midia GROUP BY a.midia`).bind(quem.id, alvo, t - RETENCAO).all();
      const comum = { filmes: 0, series: 0 };
      for (const x of cm.results || []) comum[x.midia === "series" ? "series" : "filmes"] += x.n || 0;
      saida.gosto = {
        total, iguais, pct: total ? Math.round((100 * iguais) / total) : 0,
        filmes: por.movie, series: por.series, comum,
        cobertura: { eu: nDe(quem.id), ele: nDe(alvo) },
        // Generos nao existem nos eventos: a TV mostra "sem dados", nunca um chute.
        generos: null,
      };
    }
  }

  if (grau === 1) {
    const r = await env.DB.prepare(
      "SELECT id, imdb, tipo, titulo, poster, criado, visto, aberto, comecou, terminou, reacao, " +
      "resposta, respondido FROM rec WHERE de = ? AND para = ? ORDER BY id DESC LIMIT ?").bind(quem.id, alvo, RECS_MAX).all();
    saida.recs = (r.results || []).map((x) => ({
      id: x.id, imdb: x.imdb, tipo: x.tipo, titulo: x.titulo, poster: x.poster, criado: x.criado,
      estado: estadoRec(x), reacao: x.reacao === null ? null : x.reacao,
      // Reagir e concluir sao fatos independentes. `estado` pode mostrar a
      // reacao sem esconder uma conclusao conhecida da TV que recebe.
      terminou: x.terminou ? 1 : 0,
      // A resposta direta de quem recebeu (POST /v1/rec/resposta, migracao 007).
      resposta: x.resposta || "", respondido: x.respondido || 0,
    }));
  }
  return h.json(saida);
}

// --- POST /v1/rec/resposta ------------------------------------------------------
//
// "JA ASSISTI" NUMA RECOMENDACAO RECEBIDA, com a reacao (1 | 0 | -1) e uma
// mensagem curta, as duas opcionais. Corpo:
//   {"id": <rec>, "reacao": 1|0|-1|null, "texto": "..."}
//
// NAO PASSA PELO ALCANCE, ao contrario de /v1/atividade: aquela rota carrega o
// que o player manda SOZINHO; esta e uma resposta que a pessoa deu de
// proposito, a quem lhe mandou o titulo, numa tela que diz que ele vai ver. So
// muda a rec mandada PARA quem responde (`para = quem.id`); id alheio = n 0.
//
// `reacao` ausente ou null NAO apaga uma reacao ja gravada (o "Ja assisti" sem
// resposta nao desfaz um "Gostei" dado nos creditos). Texto vazio idem.
export async function rotaRecResposta(env, quem, corpo, h, limitar, limparTexto) {
  const t = h.agora();
  const id = parseInt(corpo?.id, 10);
  if (!Number.isInteger(id) || id <= 0) return h.erro("sem id", 400);
  if (!(await limitar(env, `resp:${quem.id}`, 120, 3600, t))) return h.erro("limite", 429);
  const r = corpo?.reacao;
  const reacao = r === 1 || r === 0 || r === -1 ? r : null;
  const texto = limparTexto(corpo?.texto);
  const res = await env.DB.prepare(
    "UPDATE rec SET comecou = 1, terminou = 1, visto = 1, aberto = 1, " +
    "reacao = COALESCE(?, reacao), " +
    "resposta = CASE WHEN ? <> '' THEN ? ELSE resposta END, respondido = ? " +
    "WHERE id = ? AND para = ?"
  ).bind(reacao, texto, texto, t, id, quem.id).run();
  return h.json({ ok: 1, n: res?.meta?.changes ?? res?.changes ?? 0 });
}

export function limpezaSocial(env, t) {
  return [
    env.DB.prepare("DELETE FROM evento WHERE criado < ?").bind(t - RETENCAO),
    env.DB.prepare("DELETE FROM agora WHERE atualizado < ?").bind(t - AGORA_S),
    env.DB.prepare("DELETE FROM agregado WHERE mes < ?").bind(mesDe(t - RETENCAO)),
    env.DB.prepare("DELETE FROM agregado_titulo WHERE mes < ?").bind(mesDe(t - RETENCAO)),
  ];
}
