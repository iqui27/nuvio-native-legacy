// CACA A FILMOGRAFIA E CONQUISTAS (plano social-pessoal, secoes 4.4, 8 e 10.3).
// Tabelas: migracao-013-conquista.sql. Mesma autenticacao das demais rotas
// (index.js resolve `quem` antes de chamar).
//
//   GET  /v1/filmografia?pessoa=<tmdb>&papel=dir|ator
//        A filmografia ja classificada (longa / documentario pelo genero 99 /
//        curta < 40 min / filme de TV pelo genero 10770) com o imdb de cada
//        filme. Guardada no D1 por 14 dias e COMPARTILHADA: a segunda TV que
//        caca Villeneuve nao custa nada ao TMDB. Precisa do segredo TMDB_KEY
//        (`wrangler secret put TMDB_KEY`); sem ele responde 503 e a TV pergunta
//        ao TMDB direto, como fazia antes.
//   GET  /v1/trilhas            o catalogo de trilhas (premios, a lista do
//                               Trakt dos "250 mais bem avaliados")
//   GET  /v1/trilha?id=<id>     os filmes de uma trilha
//   POST /v1/conquista          {chave, nivel, visivel, nome, vistos, total}
//                               so no DESBLOQUEIO (a TV nunca sonda)
//
// PRIVACIDADE: a conquista so sai para amigos com `visivel` = 1 (a TV manda 1
// so com alcance >= 1 e "Mostrar aos amigos" marcado; as engracadas nunca) E
// o alcance do dono >= 1 na hora da LEITURA — conferido em conquistasDe, nao
// confiado ao que foi gravado.

import { TRILHAS_PREMIOS } from "./trilhas-dados.js";

const DIA = 86400;
const TTL_FILMOGRAFIA = 14 * DIA;
const TMDB = "https://api.themoviedb.org/3";
const IMG = "https://image.tmdb.org/t/p/w185";
// Teto de /movie/<id> por pedido: o Worker tem limite de subpedidos, e uma
// filmografia de ator passa de 100 creditos. O que sobra sai sem imdb e a TV
// completa na fila lenta dela.
const DETALHES_MAX = 40;
const FILMES_MAX = 260;
const CHAVE_RE = /^(dir|ator|saga|top|trakt|trilha):[A-Za-z0-9._\/-]{1,80}$/;
export const CONQUISTAS_AMIGO = 6;

const hoje = () => {
  const d = new Date();
  return d.getUTCFullYear() * 10000 + (d.getUTCMonth() + 1) * 100 + d.getUTCDate();
};
const dataNum = (iso) => {
  const m = /^(\d{4})-(\d{2})-(\d{2})/.exec(iso || "");
  return m ? Number(m[1] + m[2] + m[3]) : 0;
};
const COMO_SI = /Self|Himself|Herself|Themselves|uncredited/;

// Mesma regra de src/cacadados.c (caca_classe): TV > documentario > curta.
export function classe(generos, duracao) {
  const g = generos || [];
  if (g.includes(10770)) return 3;
  if (g.includes(99)) return 1;
  if (duracao > 0 && duracao < 40) return 2;
  return 0;
}

// Os creditos de /person/<id>/movie_credits -> filmes, sem repeticao, so os
// lancados, em ordem de lancamento. Pura (testada em teste-conquista.mjs).
export function filmesDosCreditos(corpo, papel, dia = hoje()) {
  const lista = papel === "dir" ? (corpo?.crew || []).filter((c) => c.job === "Director")
                                : (corpo?.cast || []).filter((c) => !COMO_SI.test(c.character || ""));
  const vistos = new Set(), out = [];
  for (const c of lista) {
    const data = dataNum(c.release_date);
    if (!c.id || vistos.has(c.id) || c.adult || !data || data > dia || !c.title) continue;
    vistos.add(c.id);
    out.push({ tmdb: c.id, imdb: "", titulo: String(c.title).slice(0, 95),
               poster: c.poster_path || "", data, classe: classe(c.genre_ids, 0) });
  }
  out.sort((a, b) => a.data - b.data);
  return out.slice(0, FILMES_MAX);
}

async function tmdb(env, caminho, fetchFn) {
  const sep = caminho.includes("?") ? "&" : "?";
  const r = await fetchFn(`${TMDB}${caminho}${sep}api_key=${env.TMDB_KEY}`);
  if (!r.ok) return null;
  return r.json();
}

async function rotaFilmografia(env, url, h, fetchFn) {
  const pessoa = parseInt(url.searchParams.get("pessoa") || "", 10);
  const papel = url.searchParams.get("papel") === "ator" ? "ator" : "dir";
  if (!Number.isInteger(pessoa) || pessoa <= 0) return h.erro("pessoa invalida", 400);
  const t = h.agora();
  const guardada = await env.DB.prepare(
    "SELECT corpo, atualizado FROM filmografia WHERE pessoa = ? AND papel = ?").bind(pessoa, papel).first();
  if (guardada && t - guardada.atualizado < TTL_FILMOGRAFIA)
    return new Response(guardada.corpo, { status: 200, headers: { "content-type": "application/json; charset=utf-8" } });
  if (!env.TMDB_KEY) return h.erro("sem chave do tmdb", 503);
  const [creditos, ficha] = await Promise.all([
    tmdb(env, `/person/${pessoa}/movie_credits`, fetchFn),
    tmdb(env, `/person/${pessoa}`, fetchFn),
  ]);
  if (!creditos) return h.erro("tmdb indisponivel", 502);
  const filmes = filmesDosCreditos(creditos, papel);
  // imdb e duracao (curta) dos primeiros DETALHES_MAX, em paralelo.
  await Promise.all(filmes.slice(0, DETALHES_MAX).map(async (f) => {
    const d = await tmdb(env, `/movie/${f.tmdb}`, fetchFn).catch(() => null);
    if (!d) return;
    if (/^tt\d+$/.test(d.imdb_id || "")) f.imdb = d.imdb_id;
    if (f.classe === 0 && d.runtime > 0 && d.runtime < 40) f.classe = 2;
  }));
  const corpo = JSON.stringify({
    pessoa, papel, nome: String(ficha?.name || "").slice(0, 95),
    foto: ficha?.profile_path ? `${IMG}${ficha.profile_path}` : "",
    filmes: filmes.map((f) => ({ ...f, poster: f.poster ? `${IMG}${f.poster}` : "" })),
  });
  await env.DB.prepare(
    "INSERT INTO filmografia (pessoa, papel, corpo, atualizado) VALUES (?, ?, ?, ?) " +
    "ON CONFLICT(pessoa, papel) DO UPDATE SET corpo = excluded.corpo, atualizado = excluded.atualizado"
  ).bind(pessoa, papel, corpo, t).run();
  return new Response(corpo, { status: 200, headers: { "content-type": "application/json; charset=utf-8" } });
}

function catalogo(env) {
  const trilhas = Object.values(TRILHAS_PREMIOS).map((x) => ({ id: x.id, tipo: "trilha", nome: x.nome }));
  // A lista do Trakt dos "250 mais bem avaliados" e escolha do dono (P5): o id
  // numerico da lista vai em [vars] TRILHA_TRAKT_TOP250 no wrangler.toml. Sem
  // ele, a TV oferece so o top 250 do TMDB.
  if (env.TRILHA_TRAKT_TOP250)
    trilhas.unshift({ id: "trakt-top250", tipo: "trakt", lista: String(env.TRILHA_TRAKT_TOP250),
                      nome: { "pt-BR": "250 mais bem avaliados (Trakt)", pt: "250 mais bem avaliados (Trakt)",
                              en: "250 top rated (Trakt)" } });
  return { trilhas };
}

async function rotaConquista(env, quem, corpo, h) {
  const chave = String(corpo?.chave || "");
  const nivel = Number(corpo?.nivel);
  if (!CHAVE_RE.test(chave)) return h.erro("chave invalida", 400);
  if (!Number.isInteger(nivel) || nivel < 1 || nivel > 5) return h.erro("nivel invalido", 400);
  const visivel = corpo?.visivel === 1 || corpo?.visivel === true ? 1 : 0;
  const nome = h.limpar(corpo?.nome, 96);
  const vistos = Math.max(0, Math.min(9999, Number(corpo?.vistos) || 0));
  const total = Math.max(0, Math.min(9999, Number(corpo?.total) || 0));
  // O nivel so sobe (nada se perde); a visibilidade e a da ultima escolha.
  await env.DB.prepare(
    "INSERT INTO conquista (pessoa, chave, nivel, quando, visivel, nome, vistos, total) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(pessoa, chave) DO UPDATE SET " +
    "nivel = MAX(nivel, excluded.nivel), quando = CASE WHEN excluded.nivel > nivel THEN excluded.quando ELSE quando END, " +
    "visivel = excluded.visivel, nome = excluded.nome, vistos = excluded.vistos, total = excluded.total"
  ).bind(quem.id, chave, nivel, h.agora(), visivel, nome, vistos, total).run();
  return h.json({ ok: 1 });
}

// As conquistas recentes VISIVEIS de `alvo`, para /v1/amigo. Quem chama ja
// conferiu contato e bloqueio; aqui confere o alcance do dono. Antes da
// migracao 013 a tabela nao existe: lista vazia, nunca erro.
export async function conquistasDe(env, alvo) {
  try {
    const p = await env.DB.prepare("SELECT alcance FROM pessoa WHERE id = ?").bind(alvo).first();
    if (!p || !(Number(p.alcance) >= 1)) return [];
    const r = await env.DB.prepare(
      "SELECT chave, nivel, quando, nome, vistos, total FROM conquista " +
      "WHERE pessoa = ? AND visivel = 1 ORDER BY quando DESC LIMIT ?").bind(alvo, CONQUISTAS_AMIGO).all();
    return r.results || [];
  } catch {
    return [];
  }
}

export async function rotaCaca(rota, metodo, env, quem, corpo, url, h, fetchFn = fetch) {
  if (rota === "/v1/filmografia" && metodo === "GET") return rotaFilmografia(env, url, h, fetchFn);
  if (rota === "/v1/trilhas" && metodo === "GET") return h.json(catalogo(env));
  if (rota === "/v1/trilha" && metodo === "GET") {
    const id = String(url.searchParams.get("id") || "");
    const t = TRILHAS_PREMIOS[id];
    if (!t) return h.erro("trilha desconhecida", 404);
    return h.json({ id: t.id, nome: t.nome.en, ordem: t.ordem, filmes: t.filmes });
  }
  if (rota === "/v1/conquista" && metodo === "POST") return rotaConquista(env, quem, corpo, h);
  return null;
}
