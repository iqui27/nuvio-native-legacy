// TRAVA DE SERIE (social & pessoal, Onda 1). Plano: docs/plans/social-pessoal/
// social-pessoal-plano.md secoes 5 e 10.2. Tabelas: migracao-012-trava.sql.
// Mesma autenticacao das demais rotas (index.js resolve `quem` antes).
//
//   GET  /v1/travas              minhas travas + membros + posicoes (ETag)
//   POST /v1/trava               criar {serie, titulo, poster, modo, pin, membros:[pessoa]}
//   POST /v1/trava/modo          {id, modo}       0 juntos, 1 separados
//   POST /v1/trava/pin           {id, pin}        0 | 1
//   POST /v1/trava/pausar        {id, pausada}    0 | 1
//   POST /v1/trava/membros       {id, membros:[pessoa]}   troca a lista (quem pede fica)
//   POST /v1/trava/sair          {id}
//   POST /v1/trava/apagar        {id}
//   POST /v1/trava/passo         {id, t, e, pessoa?, desfazer?}
//
// ONDA 1 = SO A MESMA CONTA. Todo membro tem de ser um perfil da conta de quem
// pede (`nuvio:<sub>` e `nuvio:<sub>:<n>`, mesmaConta de amigos.js) e entra
// direto com estado 1. A Onda 4 abre `membros` para amigos (estado 0 +
// /aceitar), sem mudar tabela.
//
// PRIVACIDADE: a posicao de um membro so sai para os OUTROS MEMBROS daquela
// trava, e so a desta serie. Independe do `alcance`: e um gesto explicito.
//
// O PASSO DE OUTRO PERFIL: no modo Juntos a TV marca o episodio para todos os
// membros que estavam no sofa, entao `pessoa` pode ser outro perfil DA MESMA
// CONTA que seja membro. Nunca de outra conta.

const MEMBROS_MAX = 8;
const TRAVAS_MAX = 30;           // ativas por pessoa
const CRIAR_HORA = 20;           // travas criadas por hora por pessoa
const SERIE_OK = /^[a-z0-9][a-z0-9_.-]{0,30}$/i;   // "tt123", "tmdb.55"... sem ':'

const hex = (n) => [...crypto.getRandomValues(new Uint8Array(n))]
  .map((b) => b.toString(16).padStart(2, "0")).join("");

async function revDe(env, pessoa) {
  try {
    const r = await env.DB.prepare("SELECT rev FROM trava_rev WHERE pessoa = ?").bind(pessoa).first();
    return r ? r.rev : 0;
  } catch {
    return 0;                    // migracao 012 ainda nao aplicada
  }
}

// /v1/rec chama isto (index.js). 0 sem a tabela.
export async function travaRev(env, pessoa) { return revDe(env, pessoa); }

function sobeRev(env, pessoas) {
  return [...new Set(pessoas)].map((p) => env.DB.prepare(
    "INSERT INTO trava_rev (pessoa, rev) VALUES (?, 1) " +
    "ON CONFLICT(pessoa) DO UPDATE SET rev = trava_rev.rev + 1").bind(p));
}

async function membrosDe(env, id) {
  const r = await env.DB.prepare(
    "SELECT pessoa, estado, temporada AS t, episodio AS e, passo_em AS em FROM trava_membro " +
    "WHERE trava = ? AND estado <> 2 ORDER BY rowid").bind(id).all();
  return r.results || [];
}

async function minhaTrava(env, quem, id) {
  if (typeof id !== "string" || !/^[0-9a-f]{32}$/.test(id)) return null;
  const t = await env.DB.prepare("SELECT * FROM trava WHERE id = ?").bind(id).first();
  if (!t) return null;
  const eu = await env.DB.prepare(
    "SELECT estado FROM trava_membro WHERE trava = ? AND pessoa = ?").bind(id, quem.id).first();
  if (!eu || eu.estado !== 1) return null;
  return t;
}

// Lista de membros pedida pelo cliente: strings, sem repetir, da MESMA CONTA,
// com quem pede incluido. null = recusada.
function membrosValidos(quem, lista, mesmaConta) {
  if (!Array.isArray(lista)) return null;
  const v = [quem.id];
  for (const p of lista) {
    if (typeof p !== "string" || p.length > 96) return null;
    if (!mesmaConta(p, quem.id)) return null;
    if (!/^nuvio:[^:]+(:\d{1,2})?$/.test(p)) return null;
    if (!v.includes(p)) v.push(p);
  }
  return v.length >= 2 && v.length <= MEMBROS_MAX ? v : null;
}

function saida(t, membros) {
  return {
    id: t.id, serie: t.serie, titulo: t.titulo, poster: t.poster, modo: t.modo,
    pin: t.pin, pausada: t.pausada, criador: t.criador, criado: t.criado,
    atualizado: t.atualizado,
    // `membros` POR ULTIMO: o leitor da TV (js.h) le os campos da trava antes
    // de entrar no array, e os nomes de dentro nao colidem com os de fora.
    membros: membros.map((m) => ({ pessoa: m.pessoa, estado: m.estado, t: m.t, e: m.e, em: m.em })),
  };
}

async function listar(env, quem) {
  const r = await env.DB.prepare(
    "SELECT t.* FROM trava t JOIN trava_membro m ON m.trava = t.id " +
    "WHERE m.pessoa = ? AND m.estado = 1 ORDER BY t.atualizado DESC LIMIT ?")
    .bind(quem.id, TRAVAS_MAX).all();
  const out = [];
  for (const t of r.results || []) out.push(saida(t, await membrosDe(env, t.id)));
  return out;
}

export async function rotaTrava(rota, metodo, env, quem, corpo, h, req, { limitar, mesmaConta }) {
  if (!rota.startsWith("/v1/trava")) return null;
  const { json, erro, agora, limpar } = h;
  const t = agora();
  const conta = /^nuvio:/.test(quem.id);

  if (rota === "/v1/travas" && metodo === "GET") {
    const rev = await revDe(env, quem.id);
    const etag = `"tv-${quem.id.length}-${rev}"`;
    if (req && req.headers.get("if-none-match") === etag)
      return new Response(null, { status: 304, headers: { etag, "access-control-allow-origin": "*",
        "access-control-expose-headers": "etag" } });
    return json({ rev, travas: await listar(env, quem) }, 200, { etag });
  }
  if (metodo !== "POST") return erro("rota desconhecida", 404);
  // Sem conta Nuvio nao ha "perfis da mesma conta" para travar.
  if (!conta) return erro("trava precisa da conta Nuvio", 403);

  if (rota === "/v1/trava") {
    const serie = typeof corpo?.serie === "string" ? corpo.serie : "";
    const modo = corpo?.modo === 0 ? 0 : 1;
    if (!SERIE_OK.test(serie)) return erro("serie invalida", 400);
    const membros = membrosValidos(quem, corpo?.membros, mesmaConta);
    if (!membros) return erro("membros invalidos (2 a 8 perfis desta conta)", 400);
    const ativas = await env.DB.prepare(
      "SELECT COUNT(*) AS n FROM trava_membro WHERE pessoa = ? AND estado = 1").bind(quem.id).first();
    if ((ativas?.n || 0) >= TRAVAS_MAX) return erro("travas demais", 429);
    if (!(await limitar(env, `trava:${quem.id}`, CRIAR_HORA, 3600, t))) return erro("devagar", 429);
    const id = hex(16);
    await env.DB.batch([
      env.DB.prepare(
        "INSERT INTO trava (id, serie, titulo, poster, modo, pin, criador, criado, atualizado, pausada) " +
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0)").bind(id, serie, limpar(corpo?.titulo, 160),
        /^https:\/\//.test(corpo?.poster || "") ? limpar(corpo.poster, 400) : "",
        modo, corpo?.pin ? 1 : 0, quem.id, t, t),
      ...membros.map((p) => env.DB.prepare(
        "INSERT INTO trava_membro (trava, pessoa, estado) VALUES (?, ?, 1)").bind(id, p)),
      ...sobeRev(env, membros),
    ]);
    const tv = await env.DB.prepare("SELECT * FROM trava WHERE id = ?").bind(id).first();
    return json({ id, trava: saida(tv, await membrosDe(env, id)) });
  }

  const tv = await minhaTrava(env, quem, corpo?.id);
  if (!tv) return erro("trava desconhecida", 404);
  const membros = await membrosDe(env, tv.id);
  const todos = membros.map((m) => m.pessoa);
  const mexer = async (sql, ...v) => {
    await env.DB.batch([
      env.DB.prepare(sql).bind(...v),
      env.DB.prepare("UPDATE trava SET atualizado = ? WHERE id = ?").bind(t, tv.id),
      ...sobeRev(env, todos),
    ]);
    const nova = await env.DB.prepare("SELECT * FROM trava WHERE id = ?").bind(tv.id).first();
    return json({ ok: 1, trava: nova ? saida(nova, await membrosDe(env, tv.id)) : null });
  };

  if (rota === "/v1/trava/modo")
    return mexer("UPDATE trava SET modo = ? WHERE id = ?", corpo?.modo === 0 ? 0 : 1, tv.id);
  if (rota === "/v1/trava/pin")
    return mexer("UPDATE trava SET pin = ? WHERE id = ?", corpo?.pin ? 1 : 0, tv.id);
  if (rota === "/v1/trava/pausar")
    return mexer("UPDATE trava SET pausada = ? WHERE id = ?", corpo?.pausada ? 1 : 0, tv.id);

  if (rota === "/v1/trava/membros") {
    const novos = membrosValidos(quem, corpo?.membros, mesmaConta);
    if (!novos) return erro("membros invalidos (2 a 8 perfis desta conta)", 400);
    const sai = todos.filter((p) => !novos.includes(p));
    await env.DB.batch([
      ...sai.map((p) => env.DB.prepare(
        "UPDATE trava_membro SET estado = 2 WHERE trava = ? AND pessoa = ?").bind(tv.id, p)),
      ...novos.map((p) => env.DB.prepare(
        "INSERT INTO trava_membro (trava, pessoa, estado) VALUES (?, ?, 1) " +
        "ON CONFLICT(trava, pessoa) DO UPDATE SET estado = 1").bind(tv.id, p)),
      env.DB.prepare("UPDATE trava SET atualizado = ? WHERE id = ?").bind(t, tv.id),
      ...sobeRev(env, [...todos, ...novos]),
    ]);
    const nova = await env.DB.prepare("SELECT * FROM trava WHERE id = ?").bind(tv.id).first();
    return json({ ok: 1, trava: saida(nova, await membrosDe(env, tv.id)) });
  }

  if (rota === "/v1/trava/sair") {
    // Sobrou um so: a trava acaba (uma trava de uma pessoa nao trava nada).
    const resto = todos.filter((p) => p !== quem.id);
    if (resto.length < 2) return apagar(env, tv, todos, json);
    return mexer("UPDATE trava_membro SET estado = 2 WHERE trava = ? AND pessoa = ?", tv.id, quem.id);
  }

  if (rota === "/v1/trava/apagar") {
    // Quem criou, ou (mesma conta) qualquer membro: na Onda 1 todos sao da
    // conta, e o controle de "adulto" e da TV (perfil infantil nao ve o botao).
    if (tv.criador !== quem.id && !mesmaConta(tv.criador, quem.id)) return erro("so quem criou", 403);
    return apagar(env, tv, todos, json);
  }

  if (rota === "/v1/trava/passo") {
    const te = Number(corpo?.t), ep = Number(corpo?.e);
    const pessoa = typeof corpo?.pessoa === "string" && corpo.pessoa ? corpo.pessoa : quem.id;
    // (0, 0) = "nao comecou", so pelo Desfazer (volta de quem nunca tinha visto).
    const zero = te === 0 && ep === 0 && !!corpo?.desfazer;
    if (!Number.isInteger(te) || !Number.isInteger(ep) || te > 999 || ep > 9999 ||
        (!zero && (te < 1 || ep < 1)))
      return erro("episodio invalido", 400);
    if (pessoa !== quem.id && !mesmaConta(pessoa, quem.id)) return erro("so perfis da sua conta", 403);
    const m = membros.find((x) => x.pessoa === pessoa && x.estado === 1);
    if (!m) return erro("nao e membro", 404);
    const avanca = te > m.t || (te === m.t && ep > m.e);
    // SO AVANCA, exceto o Desfazer da ilha (volta ao ponto exato pedido).
    if (!avanca && !corpo?.desfazer) return json({ ok: 1, mudou: 0 });
    await env.DB.batch([
      env.DB.prepare("UPDATE trava_membro SET temporada = ?, episodio = ?, passo_em = ? " +
        "WHERE trava = ? AND pessoa = ?").bind(te, ep, t, tv.id, pessoa),
      env.DB.prepare("UPDATE trava SET atualizado = ? WHERE id = ?").bind(t, tv.id),
      ...sobeRev(env, todos),
    ]);
    return json({ ok: 1, mudou: 1 });
  }

  return erro("rota desconhecida", 404);
}

async function apagar(env, tv, todos, json) {
  await env.DB.batch([
    env.DB.prepare("DELETE FROM trava_membro WHERE trava = ?").bind(tv.id),
    env.DB.prepare("DELETE FROM trava_licenca WHERE trava = ?").bind(tv.id),
    env.DB.prepare("DELETE FROM trava WHERE id = ?").bind(tv.id),
    ...sobeRev(env, todos),
  ]);
  return json({ ok: 1, apagada: 1 });
}
