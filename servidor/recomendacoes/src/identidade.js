// IDENTIDADE SOCIAL UNIFICADA (F08, 1.8.0). Contrato:
// docs/releases/1.8.0/F08-SOCIAL-IDENTIDADE.md. Tabelas: migracao-008-identidade.sql.
//
// A PESSOA CANONICA e uma linha de `pessoa`. Para quem tem conta Nuvio ela e o
// PERFIL (`nuvio:<sub>` no principal, `nuvio:<sub>:<indice>` nos outros) — a
// decisao do dono de 02/10 ("cada perfil da casa = uma pessoa"). Trakt, Simkl e
// Letterboxd sao IDENTIDADES LIGADAS a ela, nao pessoas proprias.
//
// O QUE UMA IDENTIDADE VERIFICADA FAZ, e so isto:
//   1. Pedido autenticado pelo Trakt cujo slug esta ligado a uma pessoa e
//      atendido COMO essa pessoa (resolverCanonica). TV antiga, que prefere o
//      Trakt, cai no mesmo perfil que a TV nova sem saber de nada.
//   2. Ao ligar, os dados da pessoa `trakt:<slug>` que ja existia (contatos,
//      pedidos, bloqueios, recs, eventos, agregados, perfil publico) sao
//      FUNDIDOS na canonica (fundir). Desligar nao separa o que ja foi fundido.
//   3. Com `visivel = 1`, amigos diretos recebem o id em /v1/contatos para a TV
//      deles juntar o feed do Trakt com o nosso.
//
// O QUE NUNCA FAZ: fundir por nome, e-mail ou slug digitado. So com as DUAS
// provas no mesmo pedido: o token que autentica o pedido e o token da outra
// conta no corpo, os dois conferidos na hora contra o emissor. Nenhum token e
// guardado.

const PROVEDORES = new Set(["trakt", "simkl", "letterboxd"]);
const UA = "nuvio-recomendacoes/1.0 (+https://github.com/iqui27/nuvio-native-legacy)";
export const RECURSO = "identidade1";

// --- resolucao -------------------------------------------------------------------

// O pedido chegou autenticado pelo Trakt: se o slug esta ligado (verificado) a
// uma pessoa, e ELA quem fala. `bruto` guarda a identidade do token, que as
// rotas de vinculo precisam. Nome/foto vazios: os do Trakt nao sobrescrevem os
// do perfil Nuvio (registrar so troca o que vier preenchido).
export async function resolverCanonica(env, quem) {
  if (!quem || !String(quem.id).startsWith("trakt:")) return quem;
  const r = await env.DB.prepare(
    "SELECT pessoa FROM identidade WHERE provedor = 'trakt' AND sujeito = ? AND verificado = 1"
  ).bind(quem.id.slice(6)).first();
  if (!r?.pessoa || r.pessoa === quem.id) return quem;
  return { id: r.pessoa, nome: "", avatar: "", bruto: quem.id, viaTrakt: 1 };
}

// Id antigo -> id vivo. So ids de conta (trakt:/nuvio:) passam pela tabela.
export async function canonica(env, id) {
  if (typeof id !== "string" || !/^(trakt|nuvio):/.test(id)) return id;
  const r = await env.DB.prepare("SELECT para FROM fusao WHERE de = ?").bind(id).first();
  return r?.para || id;
}

// TVs antigas guardaram `trakt:fulano` como contato antes da fusao: o destino de
// uma rec, o contato a remover/bloquear e o perfil a abrir sao traduzidos aqui,
// uma vez, antes de qualquer rota.
export async function canonizarEntrada(env, corpo, url) {
  if (corpo && typeof corpo.para === "string") corpo.para = await canonica(env, corpo.para);
  if (corpo && typeof corpo.id === "string") corpo.id = await canonica(env, corpo.id);
  const q = url.searchParams.get("id");
  if (q) {
    const c = await canonica(env, q);
    if (c !== q) url.searchParams.set("id", c);
  }
}

export async function identidadesDe(env, pessoa) {
  const r = await env.DB.prepare(
    "SELECT provedor, sujeito, metodo, verificado, visivel, nome, criado FROM identidade " +
    "WHERE pessoa = ? ORDER BY provedor"
  ).bind(pessoa).all();
  return (r.results || []).map((x) => ({
    provedor: x.provedor, sujeito: x.sujeito, metodo: x.metodo,
    verificado: x.verificado ? 1 : 0, visivel: x.visivel ? 1 : 0, nome: x.nome || "",
    criado: x.criado || 0,
  }));
}

// --- fusao -----------------------------------------------------------------------
//
// `de` some e tudo dele passa a `para`, num batch so (D1 executa o batch numa
// transacao). As regras de conflito, todas no sentido de NAO abrir o que estava
// fechado:
//   alcance     quem nunca respondeu (-1) adota a resposta do outro; se os dois
//               responderam, fica a MENOR (mais fechada).
//   descobrivel a menor. O aparelho reconcilia a escolha dele no /v1/eu seguinte.
//   exibicao    a da canonica; vazia, a do outro.
//   perfil publico (handle) o da canonica; sem ele, o do outro muda de dono.
//   bloqueio    vence contato: par bloqueado depois da fusao perde o contato.
//   codigo      o da canonica; o do outro deixa de existir.
// Recs, pedidos e contatos ENTRE `de` e `para` (a pessoa consigo mesma) somem.
export function fundir(env, de, para, t) {
  const P = (sql, ...v) => env.DB.prepare(sql).bind(...v);
  return [
    P("UPDATE pessoa SET " +
      "alcance = CASE WHEN alcance < 0 THEN (SELECT alcance FROM pessoa WHERE id = ?1) " +
      "  WHEN (SELECT alcance FROM pessoa WHERE id = ?1) < 0 THEN alcance " +
      "  ELSE MIN(alcance, (SELECT alcance FROM pessoa WHERE id = ?1)) END, " +
      "descobrivel = MIN(descobrivel, (SELECT descobrivel FROM pessoa WHERE id = ?1)), " +
      "exibicao = CASE WHEN exibicao <> '' THEN exibicao ELSE (SELECT exibicao FROM pessoa WHERE id = ?1) END " +
      "WHERE id = ?2 AND EXISTS (SELECT 1 FROM pessoa WHERE id = ?1)", de, para),
    P("UPDATE perfil SET pessoa = ?2 WHERE pessoa = ?1 AND NOT EXISTS (SELECT 1 FROM perfil WHERE pessoa = ?2)", de, para),
    P("DELETE FROM perfil WHERE pessoa = ?", de),
    // contatos, nos dois sentidos
    P("INSERT OR IGNORE INTO contato (a, b, criado, via) SELECT ?2, b, criado, via FROM contato WHERE a = ?1 AND b <> ?2", de, para),
    P("INSERT OR IGNORE INTO contato (a, b, criado, via) SELECT a, ?2, criado, via FROM contato WHERE b = ?1 AND a <> ?2", de, para),
    P("DELETE FROM contato WHERE a = ?1 OR b = ?1", de),
    // pedidos
    P("INSERT OR IGNORE INTO pedido (de, para, criado, estado) SELECT ?2, para, criado, estado FROM pedido WHERE de = ?1 AND para <> ?2", de, para),
    P("INSERT OR IGNORE INTO pedido (de, para, criado, estado) SELECT de, ?2, criado, estado FROM pedido WHERE para = ?1 AND de <> ?2", de, para),
    P("DELETE FROM pedido WHERE de = ?1 OR para = ?1", de),
    // bloqueios, e o bloqueio vence o contato
    P("INSERT OR IGNORE INTO bloqueio (quem, alvo, criado) SELECT ?2, alvo, criado FROM bloqueio WHERE quem = ?1 AND alvo <> ?2", de, para),
    P("INSERT OR IGNORE INTO bloqueio (quem, alvo, criado) SELECT quem, ?2, criado FROM bloqueio WHERE alvo = ?1 AND quem <> ?2", de, para),
    P("DELETE FROM bloqueio WHERE quem = ?1 OR alvo = ?1", de),
    P("DELETE FROM contato WHERE (a = ?1 OR b = ?1) AND EXISTS (SELECT 1 FROM bloqueio bq WHERE " +
      "(bq.quem = contato.a AND bq.alvo = contato.b) OR (bq.quem = contato.b AND bq.alvo = contato.a))", para),
    // recomendacoes
    P("DELETE FROM rec WHERE (de = ?1 AND para = ?2) OR (de = ?2 AND para = ?1)", de, para),
    P("UPDATE rec SET de = ?2 WHERE de = ?1", de, para),
    P("UPDATE rec SET para = ?2 WHERE para = ?1", de, para),
    // atividade nova (migracao 006)
    P("UPDATE evento SET pessoa = ?2 WHERE pessoa = ?1", de, para),
    P("INSERT INTO agora (pessoa, imdb, midia, titulo, poster, temporada, episodio, pct, atualizado) " +
      "SELECT ?2, imdb, midia, titulo, poster, temporada, episodio, pct, atualizado FROM agora WHERE pessoa = ?1 " +
      "ON CONFLICT(pessoa) DO UPDATE SET imdb = excluded.imdb, midia = excluded.midia, titulo = excluded.titulo, " +
      "poster = excluded.poster, temporada = excluded.temporada, episodio = excluded.episodio, pct = excluded.pct, " +
      "atualizado = excluded.atualizado WHERE excluded.atualizado > agora.atualizado", de, para),
    P("DELETE FROM agora WHERE pessoa = ?", de),
    P("INSERT INTO agregado (pessoa, mes, seg) SELECT ?2, mes, seg FROM agregado WHERE pessoa = ?1 " +
      "ON CONFLICT(pessoa, mes) DO UPDATE SET seg = agregado.seg + excluded.seg", de, para),
    P("DELETE FROM agregado WHERE pessoa = ?", de),
    P("INSERT OR IGNORE INTO agregado_titulo (pessoa, mes, imdb, tipo) SELECT ?2, mes, imdb, tipo FROM agregado_titulo WHERE pessoa = ?1", de, para),
    P("DELETE FROM agregado_titulo WHERE pessoa = ?", de),
    // atividade antiga (migracao 005): fica a mais nova por titulo
    P("INSERT INTO atividade (pessoa, imdb, tipo, titulo, ano, nota, acao, criado) " +
      "SELECT ?2, imdb, tipo, titulo, ano, nota, acao, criado FROM atividade WHERE pessoa = ?1 " +
      "ON CONFLICT(pessoa, imdb) DO UPDATE SET tipo = excluded.tipo, titulo = excluded.titulo, ano = excluded.ano, " +
      "nota = excluded.nota, acao = excluded.acao, criado = excluded.criado WHERE excluded.criado > atividade.criado", de, para),
    P("DELETE FROM atividade WHERE pessoa = ?", de),
    P("UPDATE registro SET pessoa = ?2 WHERE pessoa = ?1", de, para),
    // identidades que a pessoa antiga ja tinha (Simkl/Letterboxd de um Trakt-so)
    P("UPDATE OR IGNORE identidade SET pessoa = ?2 WHERE pessoa = ?1", de, para),
    P("DELETE FROM identidade WHERE pessoa = ?", de),
    // ids antigos que apontavam para `de` passam a apontar para `para`
    P("UPDATE fusao SET para = ?2 WHERE para = ?1", de, para),
    P("INSERT OR REPLACE INTO fusao (de, para, criado) VALUES (?, ?, ?)", de, para, t),
    P("DELETE FROM pessoa WHERE id = ?", de),
  ];
}

// --- verificacao nos emissores ----------------------------------------------------

// SIMKL: `GET /users/settings` com o token do proprio usuario e o client id do
// app (doc: api.simkl.org/api-reference/users.md). NAO MEDIDO contra a API real
// — sem SIMKL_CLIENT_ID no worker a rota responde 501 e a TV mostra que o vinculo
// Simkl nao esta disponivel.
export async function idSimkl(token, env) {
  // A doc da Simkl exige client_id, app-name e app-version NA QUERY de todo
  // pedido (api.simkl.org, "Headers and required parameters"); o app da TV ja os
  // manda assim (simklauth.c). So o cabecalho simkl-api-key deixava a prova a
  // depender de a Simkl aceitar o id por ali. O cabecalho segue, por seguranca.
  const q = new URLSearchParams({ client_id: env.SIMKL_CLIENT_ID, "app-name": "nuvio", "app-version": "1.0" });
  let r;
  try {
    r = await fetch(`https://api.simkl.com/users/settings?${q}`, {
      headers: { authorization: `Bearer ${token}`, "simkl-api-key": env.SIMKL_CLIENT_ID,
                 "user-agent": UA, "content-type": "application/json" },
    });
  } catch { return { falha: 0 }; }
  if (!r.ok) {
    console.log(`simkl /users/settings -> ${r.status}`);
    // So 401/403 e o token recusado. 412 (client_id do SERVIDOR errado), 429 e
    // 5xx nao sao culpa do token: a TV nao deve mandar a pessoa entrar de novo.
    return r.status === 401 || r.status === 403 ? null : { falha: r.status };
  }
  const d = await r.json().catch(() => null);
  const id = d?.account?.id;
  if (id === undefined || id === null || !/^\d{1,12}$/.test(String(id))) return null;
  return { sujeito: String(id), nome: String(d?.user?.name || "").slice(0, 64) };
}

// O PERFIL E DA CONTA? A TV diz o indice; quem diz se ele existe e o Supabase,
// com o MESMO token: `sync_pull_profiles` e a RPC que o app usa para listar os
// perfis (perfis.c) e ja resolve conta compartilhada (dono). 1 existe, 0 nao
// existe, -1 nao deu para perguntar (o vinculo falha fechado).
export async function perfilExiste(token, env, indice) {
  let r;
  try {
    r = await fetch(`${env.SUPABASE_URL}/rest/v1/rpc/sync_pull_profiles`, {
      method: "POST",
      headers: { apikey: env.SUPABASE_ANON_KEY, authorization: `Bearer ${token}`,
                 "content-type": "application/json", "user-agent": UA },
      body: "{}",
    });
  } catch { return -1; }
  if (!r.ok) { console.log(`supabase sync_pull_profiles -> ${r.status}`); return -1; }
  let lista;
  try { lista = await r.json(); } catch { return -1; }
  if (!Array.isArray(lista)) return -1;
  return lista.some((p) => Number(p?.profile_index ?? p?.id) === indice) ? 1 : 0;
}

// --- rotas -------------------------------------------------------------------------

const tokenDe = (v) => (typeof v === "string" ? v.trim() : "").slice(0, 4096);

// Grava o vinculo verificado (e funde, quando ha pessoa antiga do provedor).
async function ligar(env, h, canonicaId, provedor, sujeito, nome, aliasId) {
  const t = h.agora();
  const dona = await env.DB.prepare(
    "SELECT pessoa FROM identidade WHERE provedor = ? AND sujeito = ? AND verificado = 1"
  ).bind(provedor, sujeito).first();
  if (dona && dona.pessoa === canonicaId) return h.json({ ok: 1, pessoa: canonicaId, ja: 1, fundiu: 0 });
  if (dona) return h.erro("identidade ja vinculada a outro perfil", 409);
  const outra = await env.DB.prepare(
    "SELECT sujeito, verificado FROM identidade WHERE pessoa = ? AND provedor = ?"
  ).bind(canonicaId, provedor).first();
  if (outra && outra.verificado) return h.erro("este perfil ja tem outra conta deste servico", 409);
  const cmds = [];
  // Uma declaracao antiga do mesmo provedor da lugar a prova.
  if (outra) cmds.push(env.DB.prepare("DELETE FROM identidade WHERE pessoa = ? AND provedor = ?")
    .bind(canonicaId, provedor));
  cmds.push(env.DB.prepare(
    "INSERT INTO identidade (provedor, sujeito, pessoa, metodo, verificado, visivel, nome, criado, verificado_em) " +
    "VALUES (?, ?, ?, 'token', 1, 1, ?, ?, ?)"
  ).bind(provedor, sujeito, canonicaId, nome || "", t, t));
  let fundiu = 0;
  if (aliasId && aliasId !== canonicaId) {
    const existe = await env.DB.prepare("SELECT 1 FROM pessoa WHERE id = ?").bind(aliasId).first();
    if (existe) { cmds.push(...fundir(env, aliasId, canonicaId, t)); fundiu = 1; }
  }
  await env.DB.batch(cmds);
  return h.json({ ok: 1, pessoa: canonicaId, ja: 0, fundiu });
}

// deps: { idTrakt, idNuvio, idSimkl, perfilExiste, registrar } — injetadas para o
// teste rodar sem rede.
export async function rotaIdentidades(rota, metodo, env, quem, corpo, h, deps) {
  if (rota === "/v1/identidades" && metodo === "GET")
    return h.json({ pessoa: quem.id, recursos: [RECURSO], identidades: await identidadesDe(env, quem.id) });

  if (rota === "/v1/identidades/vincular" && metodo === "POST") {
    const provedor = String(corpo?.provedor || "");
    const bruto = quem.bruto || quem.id;

    // O PEDIDO VEIO PELO TRAKT e traz a prova da conta Nuvio: a canonica e o
    // PERFIL Nuvio, e a pessoa trakt:<slug> e fundida nele.
    if (provedor === "nuvio") {
      if (!bruto.startsWith("trakt:")) return h.erro("use o token do Trakt para vincular a conta Nuvio", 400);
      const token = tokenDe(corpo?.token);
      if (!token) return h.erro("sem token", 400);
      const n = await deps.idNuvio(token, env);
      if (!n) return h.erro("token nuvio invalido", 401);
      let canon = n.id;
      const pf = corpo?.perfil;
      if (pf !== undefined && pf !== null && pf !== 0) {
        if (!Number.isInteger(pf) || pf < 1 || pf > 32) return h.erro("perfil invalido", 400);
        const ok = await deps.perfilExiste(token, env, pf);
        if (ok < 0) return h.erro("nao foi possivel conferir o perfil", 503);
        if (!ok) return h.erro("perfil nao pertence a conta", 403);
        canon = `${n.id}:${pf}`;
      }
      await deps.registrar(env, { id: canon, nome: n.nome || "", avatar: "" });
      return ligar(env, h, canon, "trakt", bruto.slice(6), quem.nome || "", bruto);
    }

    if (!PROVEDORES.has(provedor)) return h.erro("provedor invalido", 400);

    if (provedor === "trakt") {
      // O pedido veio pela conta Nuvio e traz o token do Trakt.
      if (!quem.id.startsWith("nuvio:")) return h.erro("vincule o Trakt a partir da conta Nuvio", 400);
      const token = tokenDe(corpo?.token);
      if (!token) return h.erro("sem token", 400);
      const tk = await deps.idTrakt(token, env);
      if (!tk) return h.erro("token trakt invalido", 401);
      return ligar(env, h, quem.id, "trakt", tk.id.slice(6), tk.nome || "", tk.id);
    }

    if (provedor === "simkl") {
      if (!env.SIMKL_CLIENT_ID) return h.erro("simkl indisponivel neste servidor", 501);
      const token = tokenDe(corpo?.token);
      if (!token) return h.erro("sem token", 400);
      const s = await deps.idSimkl(token, env);
      if (!s) return h.erro("token simkl invalido", 401);
      if (s.falha !== undefined) return h.erro("simkl nao respondeu (" + s.falha + ")", 502);
      return ligar(env, h, quem.id, "simkl", s.sujeito, s.nome, null);
    }

    // LETTERBOXD: nao ha API aberta para provar nada. Fica DECLARADO: aparece
    // para a propria pessoa e nunca funde, resolve ou deduplica.
    const usuario = String(corpo?.usuario || "").trim().toLowerCase();
    if (!/^[a-z0-9_]{2,30}$/.test(usuario)) return h.erro("usuario invalido", 400);
    const t = h.agora();
    const ja = await env.DB.prepare("SELECT verificado FROM identidade WHERE pessoa = ? AND provedor = 'letterboxd'")
      .bind(quem.id).first();
    if (ja && ja.verificado) return h.erro("este perfil ja tem outra conta deste servico", 409);
    await env.DB.batch([
      env.DB.prepare("DELETE FROM identidade WHERE pessoa = ? AND provedor = 'letterboxd'").bind(quem.id),
      env.DB.prepare(
        "INSERT INTO identidade (provedor, sujeito, pessoa, metodo, verificado, visivel, nome, criado, verificado_em) " +
        "VALUES ('letterboxd', ?, ?, 'declarado', 0, 0, '', ?, 0)").bind(usuario, quem.id, t),
    ]);
    return h.json({ ok: 1, pessoa: quem.id, ja: 0, fundiu: 0, verificado: 0 });
  }

  // DESLIGAR nao separa o passado: o que foi fundido continua na canonica. O
  // proximo pedido autenticado por aquele Trakt volta a ser uma pessoa nova.
  if (rota === "/v1/identidades/desvincular" && metodo === "POST") {
    const provedor = String(corpo?.provedor || "");
    if (!PROVEDORES.has(provedor)) return h.erro("provedor invalido", 400);
    const r = await env.DB.prepare("DELETE FROM identidade WHERE pessoa = ? AND provedor = ?")
      .bind(quem.id, provedor).run();
    return h.json({ ok: 1, n: r?.meta?.changes ?? r?.changes ?? 0 });
  }

  if (rota === "/v1/identidades/visivel" && metodo === "POST") {
    const provedor = String(corpo?.provedor || "");
    if (!PROVEDORES.has(provedor)) return h.erro("provedor invalido", 400);
    const v = corpo?.visivel ? 1 : 0;
    // Declarada nunca sai: ela nao prova nada sobre ninguem.
    const r = await env.DB.prepare(
      "UPDATE identidade SET visivel = ? WHERE pessoa = ? AND provedor = ? AND verificado = 1"
    ).bind(v, quem.id, provedor).run();
    return h.json({ ok: 1, n: r?.meta?.changes ?? r?.changes ?? 0, visivel: v });
  }
  return null;
}
