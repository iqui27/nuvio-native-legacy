// "Entre amigos" alem do Trakt: perfil publico opcional, busca, pedidos de
// amizade, bloqueio e atividade dos amigos. Modelo de privacidade completo em
// docs/SOCIAL-PRIVACIDADE.md — este arquivo e onde cada regra dele e imposta.
//
// AS QUATRO REGRAS QUE TUDO AQUI OBEDECE, e o teste de ponta a ponta confere
// cada uma (servidor teste-amigos.sh):
//
//  1. NINGUEM APARECE SEM TER LIGADO. `pessoa.descobrivel` nasce 0 e so o
//     dono, com o proprio token, liga. Desligar apaga o perfil publico NA HORA
//     (nao ha copia da lista de descobriveis em lugar nenhum: toda consulta le
//     a coluna).
//  2. O QUE SAI DAQUI E UM PERFIL MINIMO, e o servidor e quem o monta a partir
//     de campos que a pessoa escolheu — apelido, bio, generos, foto opcional,
//     "vistos recentemente" opcional. Nunca e-mail, id de conta, addon, IP.
//     Ninguem de fora recebe o `id` da conta (`nuvio:<uuid>` e o sub do
//     Supabase): a porta entre estranhos e um HANDLE OPACO (`pub`).
//  3. AMIZADE PRECISA DOS DOIS LADOS. Achar alguem pela busca so gera um
//     PEDIDO; a outra pessoa aceita, recusa (em silencio) ou bloqueia.
//  4. ATIVIDADE SO PARA AMIGO MUTUO, e so de quem ligou. O consentimento e
//     conferido AQUI no servidor a cada escrita e a cada leitura, nao so na TV.

const PUB_ABC = "abcdefghijkmnpqrstuvwxyz23456789";
const APELIDO_MIN = 2;
const APELIDO_MAX = 20;
const BIO_MAX = 80;
const GEN_MAX = 5;
const BUSCA_MIN = 3;
const BUSCA_MAX = 10;
const ATIV_MAX = 30;          // titulos guardados por pessoa
const FEED_MAX = 30;
const AGORA_S = 600;          // "assistindo agora" vale 10 min, como social.c
const RETENCAO_ATIV = 30 * 86400;
const RETENCAO_PEDIDO = 30 * 86400;
const RETENCAO_RECUSA = 90 * 86400;
const PEDIDOS_RECEBIDOS_MAX = 100;
const PEDIDOS_ENVIADOS_MAX = 50;

// A lista fechada de generos: o servidor NUNCA guarda texto livre nessa coluna,
// so ids desta tabela. O cliente traduz o rotulo.
const GENEROS = new Set([
  "acao", "aventura", "animacao", "comedia", "crime", "documentario", "drama",
  "familia", "fantasia", "ficcao", "misterio", "romance", "suspense", "terror",
]);

// A FOTO SO SAI DE UM HOST CONHECIDO. O avatar do Trakt e uma URL de
// walter.trakt.tv, mas quem nao tem foto la recebe o Gravatar — e a URL do
// Gravatar e o MD5 do e-mail. Publicar isso para desconhecidos seria publicar
// um hash de e-mail que se reverte por dicionario. Fora da lista, vazio: a TV
// desenha a inicial num disco colorido.
const AVATAR_OK = /^https:\/\/(walter\.trakt\.tv|media\.trakt\.tv|trakt\.tv)\//;
const avatarPublico = (url) => (typeof url === "string" && AVATAR_OK.test(url) ? url : "");

const norm = (s) => String(s || "").toLowerCase().replace(/[^a-z0-9]+/g, "");

// --- conta, selo e nome ---------------------------------------------------------
//
// A CONTA de um id: `nuvio:<sub>:<n>` (perfil da casa) e `nuvio:<sub>` sao a
// MESMA conta. E por ela que a busca, a comunidade e as sugestoes deixam de fora
// os OUTROS PERFIS de quem pergunta: o dono viu o proprio segundo perfil listado
// como "uma pessoa". Adicionar um perfil da casa continua possivel pelo codigo
// de 6 letras (POST /v1/contatos), que e um gesto explicito.
const contaDe = (id) => String(id || "").replace(/^(nuvio:[^:]+):\d{1,2}$/, "$1");
const mesmaConta = (a, b) => contaDe(a) === contaDe(b);
// O mesmo corte em SQL: `p` e o parametro com contaDe(quem.id).
const foraDaConta = (col, p = "?") =>
  `AND ${col} <> ${p} AND substr(${col}, 1, length(${p}) + 1) <> ${p} || ':' `;

// A MESMA PESSOA PUBLICADA DUAS VEZES (#202, "nomes repetidos" na busca). Ate
// a 2.0.1 a TV guardava "Meu perfil" num arquivo do APARELHO e, ao ligar com
// outra identidade (outro perfil da casa, ou Trakt no lugar da conta Nuvio),
// republicava o mesmo apelido/bio/generos nela. No banco de 06/10/2026: 6 dos
// 22 apelidos publicados apareciam duas vezes, 5 deles com bio e generos
// identicos. O cliente parou de fazer isso; este corte esconde as copias que
// ja estao no banco, sem apagar nada.
//
// DUAS LINHAS SAO A MESMA PESSOA quando tem o mesmo apelido E:
//  - sao da MESMA CONTA Nuvio e publicaram exatamente o mesmo conteudo (bio e
//    generos). Perfis da casa com apelidos ou textos diferentes continuam
//    pessoas diferentes ("perfil = pessoa"); ou
//  - uma e `trakt:<slug>` com identidade Trakt VERIFICADA ligada a outra
//    (migracao 008). Identidade so declarada nunca funde.
// Fica a copia com quem eu ja sou amigo; empate, o menor id (o perfil
// principal `nuvio:<sub>` antes de `nuvio:<sub>:<n>`, e nuvio antes de trakt).
// Bloqueio numa das copias esconde as duas: e a mesma pessoa.
// `f` e o alias de `perfil` na consulta, `eu` o parametro com quem.id.
const contaSql = (c) =>
  `(CASE WHEN ${c} GLOB 'nuvio:*:*' THEN substr(${c}, 1, 5 + instr(substr(${c}, 7), ':')) ELSE ${c} END)`;
const traktLigado = (c) =>
  `(SELECT i.pessoa FROM identidade i WHERE i.provedor = 'trakt' AND i.verificado = 1 AND ${c} = 'trakt:' || i.sujeito)`;
function semCopia(f, eu) {
  const amigo = (c) => `EXISTS (SELECT 1 FROM contato k WHERE k.a = ${eu} AND k.b = ${c})`;
  const fp = `${f}.pessoa`;
  return "AND NOT EXISTS (SELECT 1 FROM perfil g JOIN pessoa gp ON gp.id = g.pessoa AND gp.descobrivel = 1 " +
    `WHERE g.apelido_norm = ${f}.apelido_norm AND g.apelido <> '' AND g.pessoa <> ${fp} ` +
    `AND ((${contaSql("g.pessoa")} = ${contaSql(fp)} AND g.bio = ${f}.bio AND g.generos = ${f}.generos) ` +
    `OR ${traktLigado(fp)} = g.pessoa OR ${traktLigado("g.pessoa")} = ${fp}) ` +
    `AND (${amigo("g.pessoa")} > ${amigo(fp)} OR (${amigo("g.pessoa")} = ${amigo(fp)} AND g.pessoa < ${fp}))) `;
}

// SELO DE CRIADOR: quem decide e o SERVIDOR, pela conta (lista em `CRIADORES`,
// wrangler.toml, ids canonicos separados por virgula), nunca o cliente por
// apelido ou nome — qualquer um pode escolher o apelido "iqui". Todos os perfis
// da conta levam o selo. O id nao e segredo e nao sai daqui: so sai o selo.
function seloDe(env, id) {
  const lista = String(env?.CRIADORES || "").split(",").map((s) => s.trim()).filter(Boolean);
  return id && lista.includes(contaDe(id)) ? "criador" : "";
}

// "Amigo #<rowid>" e a reserva de quem nao tem nome (resolverNome): nao e nome.
const nomeDeVerdade = (s) => (typeof s === "string" && !/^Amigo #\d+$/.test(s) ? s : "");

// --- limite de uso -------------------------------------------------------------
//
// Contador por (acao, pessoa) numa janela fixa. Um UPSERT so, atomico no D1: a
// virada de janela zera o contador na mesma instrucao que incrementa, entao
// duas requisicoes simultaneas nao passam do teto por corrida.
async function limitar(env, chave, max, janela, t) {
  const j = Math.floor(t / janela);
  const r = await env.DB.prepare(
    "INSERT INTO limite (chave, janela, n, expira) VALUES (?, ?, 1, ?) " +
    "ON CONFLICT(chave) DO UPDATE SET " +
    "n = CASE WHEN limite.janela = excluded.janela THEN limite.n + 1 ELSE 1 END, " +
    "janela = excluded.janela, expira = excluded.expira RETURNING n"
  ).bind(chave, j, (j + 1) * janela).first();
  return (r?.n || 1) <= max;
}

const bloqueado = (db, a, b) =>
  db.prepare("SELECT 1 FROM bloqueio WHERE (quem = ? AND alvo = ?) OR (quem = ? AND alvo = ?)")
    .bind(a, b, b, a).first();

// --- handle publico ------------------------------------------------------------

function novoPub() {
  const bytes = crypto.getRandomValues(new Uint8Array(10));
  return [...bytes].map((b) => PUB_ABC[b % PUB_ABC.length]).join("");
}

// Cria a linha de `perfil` se nao existe. O handle nasce AQUI e so aqui.
async function garantirPerfil(env, id) {
  const t = Math.floor(Date.now() / 1000);
  for (let i = 0; i < 6; i++) {
    try {
      await env.DB.prepare(
        "INSERT OR IGNORE INTO perfil (pessoa, pub, atualizado) VALUES (?, ?, ?)"
      ).bind(id, novoPub(), t).run();
      break;
    } catch { /* colisao rara do UNIQUE(pub): sorteia outro */ }
  }
  return env.DB.prepare("SELECT * FROM perfil WHERE pessoa = ?").bind(id).first();
}

const pubLimpo = (s) => norm(s).slice(0, 10);

// Handle -> pessoa. Devolve null tanto para "nao existe" quanto para "bloqueado":
// quem foi bloqueado nao pode distinguir uma coisa da outra.
async function achar(env, quem, pub) {
  const p = pubLimpo(pub);
  if (p.length !== 10) return null;
  const x = await env.DB.prepare(
    "SELECT f.pessoa AS id, f.pub AS pub, f.apelido AS apelido, f.bio AS bio, f.generos AS generos, " +
    "f.com_avatar AS comAvatar, f.recentes AS recentes, p.nome AS nome, p.avatar AS avatar, " +
    "p.descobrivel AS descobrivel FROM perfil f JOIN pessoa p ON p.id = f.pessoa WHERE f.pub = ?"
  ).bind(p).first();
  if (!x || x.id === quem.id) return null;
  if (await bloqueado(env.DB, quem.id, x.id)) return null;
  return x;
}

const publicado = (x) => !!(x.descobrivel && x.apelido);
const listaGeneros = (s) => (s ? s.split(",").filter((g) => GENEROS.has(g)) : []);

async function relacaoCom(env, quemId, outroId) {
  if (await env.DB.prepare("SELECT 1 FROM contato WHERE a = ? AND b = ?").bind(quemId, outroId).first())
    return "amigo";
  const p = await env.DB.prepare(
    "SELECT de, estado FROM pedido WHERE (de = ? AND para = ?) OR (de = ? AND para = ?)"
  ).bind(quemId, outroId, outroId, quemId).all();
  for (const r of p.results || []) {
    if (r.de === quemId) return "enviado";           // recusado tambem parece "enviado"
    if (r.estado === 0) return "recebido";
  }
  return "";
}

// O cartao que um ESTRANHO pode ver. Cada campo abaixo foi escolhido pela
// pessoa; o que ela nao preencheu sai vazio, nunca com um valor de reserva
// tirado da conta (o `nome` da conta pode ser o nome real).
//
// `nome` SO SAI PARA AMIGO. Quem ja e amigo recebe esse mesmo nome em
// /v1/contatos; para todo o resto (busca, comunidade, gosto, pedido) ele sai
// vazio e a TV mostra so o apelido. "Estranhos veem so o apelido" e o que a
// tela de Meu perfil promete, e teste-amigos.sh confere.
// `selo` so existe na resposta de quem tem um (ver seloDe).
function cartao(x, relacao, env) {
  const pub = publicado(x);
  const selo = seloDe(env, x.id);
  return {
    pub: x.pub,
    apelido: pub ? x.apelido : (x.apelido || x.nome || ""),
    nome: relacao === "amigo" ? nomeDeVerdade(x.nome) : "",
    avatar: x.comAvatar && pub ? avatarPublico(x.avatar) : "",
    bio: pub ? x.bio : "",
    generos: pub ? listaGeneros(x.generos) : [],
    relacao,
    ...(selo ? { selo } : {}),
  };
}

// --- meu perfil ------------------------------------------------------------------

async function rotaPerfilLer(env, quem, h) {
  const f = await garantirPerfil(env, quem.id);
  const p = await env.DB.prepare("SELECT descobrivel FROM pessoa WHERE id = ?").bind(quem.id).first();
  return h.json({
    publicado: p?.descobrivel && f.apelido ? 1 : 0,
    pesquisavel: p?.descobrivel ? 1 : 0,
    apelido: f.apelido, bio: f.bio, generos: listaGeneros(f.generos),
    avatar: f.com_avatar ? 1 : 0, recentes: f.recentes ? 1 : 0, ativ: f.ativ,
    pub: f.pub,
  });
}

async function rotaPerfilPublicar(env, quem, corpo, h) {
  const t = h.agora();
  if (!(await limitar(env, `perfil:${quem.id}`, 30, 3600, t))) return h.erro("limite de alteracoes", 429);
  // a-z0-9 e espaco, como o resto do servico: alem de casar com o teclado da
  // TV, isso torna IMPOSSIVEL colocar e-mail, link ou @usuario num apelido ou
  // numa bio (nao ha ponto, arroba nem dois-pontos no alfabeto).
  const apelido = h.limparTexto(corpo?.apelido).slice(0, APELIDO_MAX).trim();
  if (apelido.length < APELIDO_MIN || norm(apelido).length < APELIDO_MIN) return h.erro("apelido curto", 400);
  const bio = h.limparTexto(corpo?.bio).slice(0, BIO_MAX).trim();
  const gens = [...new Set((Array.isArray(corpo?.generos) ? corpo.generos : [])
    .map((g) => String(g)).filter((g) => GENEROS.has(g)))].slice(0, GEN_MAX);
  await garantirPerfil(env, quem.id);
  await env.DB.batch([
    env.DB.prepare(
      "UPDATE perfil SET apelido = ?, apelido_norm = ?, bio = ?, generos = ?, com_avatar = ?, " +
      "recentes = ?, atualizado = ? WHERE pessoa = ?"
    ).bind(apelido, norm(apelido), bio, gens.join(","), corpo?.avatar ? 1 : 0,
           corpo?.recentes ? 1 : 0, t, quem.id),
    env.DB.prepare("UPDATE pessoa SET descobrivel = 1 WHERE id = ?").bind(quem.id),
  ]);
  await podarAtividade(env, quem.id);
  const f = await env.DB.prepare("SELECT pub FROM perfil WHERE pessoa = ?").bind(quem.id).first();
  return h.json({ ok: 1, publicado: 1, pub: f?.pub || "" });
}

// Apaga o perfil PUBLICO e desliga o sinalizador. O `pub` e SORTEADO DE NOVO:
// qualquer link/lista/tela que ainda guarde o handle antigo passa a nao achar
// ninguem. As configuracoes so entre amigos (`ativ`) nao sao publicas e ficam.
export async function despublicar(env, id) {
  const t = Math.floor(Date.now() / 1000);
  await env.DB.batch([
    env.DB.prepare("UPDATE pessoa SET descobrivel = 0 WHERE id = ?").bind(id),
    env.DB.prepare(
      "UPDATE perfil SET apelido = '', apelido_norm = '', bio = '', generos = '', com_avatar = 0, " +
      "recentes = 0, pub = ?, atualizado = ? WHERE pessoa = ?"
    ).bind(novoPub(), t, id),
    // pedido PENDENTE feito por quem some nao fica pendurado mostrando um nome
    env.DB.prepare("DELETE FROM pedido WHERE de = ? AND estado = 0").bind(id),
  ]);
  await podarAtividade(env, id);
}

async function rotaPerfilDespublicar(env, quem, h) {
  await despublicar(env, quem.id);
  return h.json({ ok: 1, publicado: 0 });
}

// "Quero sumir": perfil, atividade, pedidos enviados. Contatos e recomendacoes
// nao sao tocados — sao vinculos que as duas pessoas aceitaram.
async function rotaPerfilApagar(env, quem, h) {
  await despublicar(env, quem.id);
  await env.DB.batch([
    env.DB.prepare("DELETE FROM atividade WHERE pessoa = ?").bind(quem.id),
    env.DB.prepare("DELETE FROM pedido WHERE de = ?").bind(quem.id),
    env.DB.prepare("UPDATE perfil SET ativ = 0 WHERE pessoa = ?").bind(quem.id),
  ]);
  return h.json({ ok: 1 });
}

// Atividade so cabe enquanto alguem a pediu: nem "amigos veem" nem "cartao
// publico". Sem nenhum dos dois, o que ja estava guardado e apagado.
async function podarAtividade(env, id) {
  const f = await env.DB.prepare("SELECT ativ, recentes FROM perfil WHERE pessoa = ?").bind(id).first();
  if (!f || (f.ativ < 1 && !f.recentes))
    await env.DB.prepare("DELETE FROM atividade WHERE pessoa = ?").bind(id).run();
  else if (f.ativ < 2)
    await env.DB.prepare("UPDATE atividade SET acao = 0 WHERE pessoa = ?").bind(id).run();
}

// 0 = nada; 1 = amigos veem o que assisti; 2 = e tambem "assistindo agora".
async function rotaPerfilAtividade(env, quem, corpo, h) {
  const n = Number.isInteger(corpo?.nivel) ? Math.max(0, Math.min(2, corpo.nivel)) : 0;
  await garantirPerfil(env, quem.id);
  await env.DB.prepare("UPDATE perfil SET ativ = ?, atualizado = ? WHERE pessoa = ?")
    .bind(n, h.agora(), quem.id).run();
  await podarAtividade(env, quem.id);
  return h.json({ ok: 1, ativ: n });
}

// --- busca e cartao ------------------------------------------------------------------

async function rotaBuscar(env, quem, corpo, h) {
  const t = h.agora();
  if (!(await limitar(env, `busca:${quem.id}`, 40, 3600, t))) return h.erro("limite de buscas", 429);
  const q = norm(corpo?.q);
  // MINIMO DE TRES: e o que impede "a" de listar meio banco. Tres caracteres
  // ainda deixam o teto de 10 resultados + 40 buscas/h tornar varredura inviavel.
  if (q.length < BUSCA_MIN) return h.erro("busca curta", 400);
  const achados = [];
  const vistos = new Set();
  const empurra = async (x) => {
    if (!x || x.id === quem.id || vistos.has(x.id)) return;
    if (mesmaConta(x.id, quem.id)) return;       // outro perfil da MINHA conta
    if (await bloqueado(env.DB, quem.id, x.id)) return;
    vistos.add(x.id);
    achados.push(cartao(x, await relacaoCom(env, quem.id, x.id), env));
  };
  const SEL =
    "SELECT f.pessoa AS id, f.pub AS pub, f.apelido AS apelido, f.bio AS bio, f.generos AS generos, " +
    "f.com_avatar AS comAvatar, f.recentes AS recentes, p.nome AS nome, p.avatar AS avatar, " +
    "p.descobrivel AS descobrivel FROM perfil f JOIN pessoa p ON p.id = f.pessoa ";
  // (a) codigo de pareamento EXATO. Quem dita o codigo ja escolheu ser achado,
  // entao aqui vale mesmo sem perfil publico; o pedido que sai daqui ainda
  // precisa de aceite.
  if (q.length === 6) {
    const c = await env.DB.prepare("SELECT id FROM pessoa WHERE codigo = ?").bind(q).first();
    if (c) {
      await garantirPerfil(env, c.id);
      await empurra(await env.DB.prepare(SEL + "WHERE f.pessoa = ?").bind(c.id).first());
    }
  }
  // (b) apelido por PREFIXO, so de quem ligou o perfil pesquisavel. Faixa em
  // vez de LIKE para usar o indice.
  const hi = q.slice(0, -1) + String.fromCharCode(q.charCodeAt(q.length - 1) + 1);
  const r = await env.DB.prepare(
    SEL + "WHERE f.apelido_norm >= ?1 AND f.apelido_norm < ?2 AND p.descobrivel = 1 AND f.apelido <> '' " +
    semCopia("f", "?3") +
    "ORDER BY f.apelido_norm LIMIT ?4"
  ).bind(q, hi, quem.id, BUSCA_MAX + 2).all();
  for (const x of r.results || []) {
    if (achados.length >= BUSCA_MAX) break;
    await empurra(x);
  }
  return h.json({ resultados: achados.slice(0, BUSCA_MAX) });
}

async function rotaVer(env, quem, corpo, h) {
  if (!(await limitar(env, `ver:${quem.id}`, 120, 3600, h.agora()))) return h.erro("limite", 429);
  const x = await achar(env, quem, corpo?.pub);
  if (!x) return h.erro("perfil nao encontrado", 404);
  const rel = await relacaoCom(env, quem.id, x.id);
  // Perfil NAO publicado so e visto por quem tem relacao com a pessoa (amigo ou
  // pedido) — um handle solto nao abre nada.
  if (!publicado(x) && !rel) return h.erro("perfil nao encontrado", 404);
  const c = cartao(x, rel, env);
  c.recentes = [];
  if (publicado(x) && x.recentes) {
    const r = await env.DB.prepare(
      "SELECT imdb, tipo, titulo, ano FROM atividade WHERE pessoa = ? AND acao = 0 " +
      "ORDER BY criado DESC LIMIT 6"
    ).bind(x.id).all();
    c.recentes = r.results || [];
  }
  return h.json(c);
}

// Gosto parecido. Antes so voltava gente com 3+ titulos em comum nos "vistos
// recentemente" PUBLICADOS — em 06/10/2026 isso eram 15 linhas de 2 pessoas em
// todo o banco, entao ninguem recebia nada. Agora tres sinais, cada resultado
// com o MOTIVO honesto:
//   titulos : n titulos que EU mandei na consulta (nao ficam guardados) tambem
//             estao na atividade publica (so de quem ligou "recentes") ou no
//             agregado de titulos da pessoa. SO O NUMERO sai, nunca quais.
//   generos : generos do "Meu perfil" em comum (ja sao publicos no cartao).
//   ativos  : perfil publicado mexido ha pouco — o que sobra quando faltam dados.
// So voltam pessoas pesquisaveis, sem relacao comigo e nunca bloqueadas. Exige
// que EU tambem seja pesquisavel: quem nao se mostra nao ganha uma janela para
// ver os outros. Limiar de titulos adaptativo: 1 se mandei poucos, 2 se muitos.
const SUG_MAX = 10;
const SUG_RECENTE_S = 45 * 86400;
async function rotaSugeridos(env, quem, corpo, h) {
  const t = h.agora();
  if (!(await limitar(env, `sug:${quem.id}`, 20, 3600, t))) return h.erro("limite", 429);
  const eu = await env.DB.prepare(
    "SELECT p.descobrivel AS d, f.generos AS generos FROM pessoa p LEFT JOIN perfil f ON f.pessoa = p.id WHERE p.id = ?"
  ).bind(quem.id).first();
  if (!eu?.d) return h.json({ sugeridos: [] });
  const imdbs = [...new Set((Array.isArray(corpo?.imdbs) ? corpo.imdbs : [])
    .map((s) => String(s)).filter((s) => /^tt\d{1,10}$/.test(s)))].slice(0, 60);
  const meus = new Set(listaGeneros(eu.generos));
  const r = await env.DB.prepare(
    "SELECT f.pessoa AS id, f.pub AS pub, f.apelido AS apelido, f.bio AS bio, f.generos AS generos, " +
    "f.com_avatar AS comAvatar, f.recentes AS recentes, f.atualizado AS atualizado, " +
    "p.avatar AS avatar, p.descobrivel AS descobrivel " +
    "FROM perfil f JOIN pessoa p ON p.id = f.pessoa AND p.descobrivel = 1 " +
    "WHERE f.apelido <> '' " +
    foraDaConta("f.pessoa") +
    semCopia("f", "?") +
    "AND NOT EXISTS (SELECT 1 FROM contato c WHERE c.a = ? AND c.b = f.pessoa) " +
    "AND NOT EXISTS (SELECT 1 FROM bloqueio b WHERE (b.quem = ? AND b.alvo = f.pessoa) OR (b.quem = f.pessoa AND b.alvo = ?)) " +
    "ORDER BY f.atualizado DESC LIMIT 200"
  ).bind(contaDe(quem.id), contaDe(quem.id), contaDe(quem.id),
         quem.id, quem.id, quem.id, quem.id,   // semCopia: os 4 `?` de "sou amigo"
         quem.id, quem.id, quem.id).all();
  const cands = new Map((r.results || []).map((x) => [x.id, x]));
  // titulos em comum: duas consultas pelo imdb (nunca por lista de pessoas — o D1
  // aceita 100 parametros) e o corte por candidato e feito aqui.
  const comuns = new Map();
  if (imdbs.length && cands.size) {
    const marcas = imdbs.map(() => "?").join(",");
    const soma = (rows, soRecentes) => {
      for (const x of rows || []) {
        const c = cands.get(x.pessoa);
        if (!c || (soRecentes && !c.recentes)) continue;
        if (!comuns.has(c.id)) comuns.set(c.id, new Set());
        comuns.get(c.id).add(x.imdb);
      }
    };
    soma((await env.DB.prepare(
      `SELECT pessoa, imdb FROM atividade WHERE acao = 0 AND imdb IN (${marcas})`).bind(...imdbs).all()).results, true);
    soma((await env.DB.prepare(
      `SELECT pessoa, imdb FROM agregado_titulo WHERE imdb IN (${marcas})`).bind(...imdbs).all()).results, false);
  }
  const minTit = imdbs.length >= 10 ? 2 : 1;
  const minGen = Math.min(2, meus.size);
  const lista = [];
  for (const c of cands.values()) {
    const n = comuns.get(c.id)?.size || 0;
    const gens = listaGeneros(c.generos).filter((g) => meus.has(g));
    const recente = t - c.atualizado <= SUG_RECENTE_S;
    let motivo;
    if (n >= minTit) motivo = { tipo: "titulos", n, ...(gens.length ? { generos: gens } : {}) };
    else if (minGen > 0 && gens.length >= minGen) motivo = { tipo: "generos", generos: gens };
    else if (recente) motivo = { tipo: "ativos" };
    else continue;
    const peso = motivo.tipo === "titulos" ? 1000 + n * 10 + gens.length
      : motivo.tipo === "generos" ? 100 + gens.length : 0;
    lista.push({ c, motivo, n, peso });
  }
  lista.sort((a, b) => b.peso - a.peso || b.c.atualizado - a.c.atualizado);
  return h.json({
    sugeridos: lista.slice(0, SUG_MAX).map(({ c, motivo, n }) => ({
      ...cartao({ ...c, nome: "" }, "", env), emComum: motivo.tipo === "titulos" ? n : 0, motivo,
    })),
  });
}

// COMUNIDADE NUVIO NATIVE: a lista de TODOS os perfis publicados, em paginas.
//
// QUEM ENTRA: so quem esta publicado (descobrivel = 1 E apelido escolhido) —
// exatamente o mesmo conjunto que a busca por apelido ja acha. Quem ligou so o
// antigo "aparecer para outras pessoas" sem escolher apelido NAO entra: o unico
// nome que haveria para mostrar e o da conta, e esse ninguem escolheu mostrar.
// Desligar tira da lista na hora (toda consulta le as colunas; nao ha copia).
//
// O QUE SAI: o mesmo cartao da busca, mais `vendo` = o ultimo titulo dos
// "vistos recentemente" SO de quem ligou esse interruptor (o cartao publico ja
// mostra esses mesmos titulos). "Assistindo agora" e coisa so de amigo e nunca
// sai aqui. Nenhum horario sai: a ordem usa a atividade publica, mas o numero
// fica no servidor.
//
// ORDEM: o mais recente entre "publicou/mexeu no perfil" e "ultimo visto
// recente publico". `pessoa.visto` (ultima vez que a TV falou com o servidor)
// NAO entra: e presenca, e presenca nunca foi publica.
//
// RECIPROCO, como o gosto parecido: quem nao se mostra nao ganha uma janela
// para ver todo mundo. Paginado (20 por vez, ate 50 paginas) e com limite por
// hora, para a lista nao virar um jeito barato de copiar o diretorio inteiro.
const COMUNIDADE_PAG = 20;
const COMUNIDADE_PAGS = 50;

async function rotaComunidade(env, quem, corpo, h) {
  if (!(await limitar(env, `comunidade:${quem.id}`, 60, 3600, h.agora()))) return h.erro("limite", 429);
  const eu = await env.DB.prepare("SELECT descobrivel FROM pessoa WHERE id = ?").bind(quem.id).first();
  if (!eu?.descobrivel) return h.json({ pessoas: [], mais: 0, fechado: 1 });
  const pag = Number.isInteger(corpo?.pagina) ? Math.max(0, Math.min(COMUNIDADE_PAGS - 1, corpo.pagina)) : 0;
  const PUBLICO = "a.pessoa = f.pessoa AND a.acao = 0 AND f.recentes = 1";
  const r = await env.DB.prepare(
    "SELECT f.pessoa AS id, p.nome AS nome, f.pub AS pub, f.apelido AS apelido, f.bio AS bio, f.generos AS generos, " +
    "f.com_avatar AS comAvatar, p.avatar AS avatar, p.descobrivel AS descobrivel, " +
    `(SELECT a.titulo FROM atividade a WHERE ${PUBLICO} ORDER BY a.criado DESC LIMIT 1) AS vendo, ` +
    `MAX(f.atualizado, COALESCE((SELECT MAX(a.criado) FROM atividade a WHERE ${PUBLICO}), 0)) AS ativo, ` +
    "CASE WHEN EXISTS (SELECT 1 FROM contato c WHERE c.a = ?1 AND c.b = f.pessoa) THEN 'amigo' " +
    "     WHEN EXISTS (SELECT 1 FROM pedido q WHERE q.de = ?1 AND q.para = f.pessoa) THEN 'enviado' " +
    "     WHEN EXISTS (SELECT 1 FROM pedido q WHERE q.de = f.pessoa AND q.para = ?1 AND q.estado = 0) THEN 'recebido' " +
    "     ELSE '' END AS relacao " +
    "FROM perfil f JOIN pessoa p ON p.id = f.pessoa " +
    "WHERE p.descobrivel = 1 AND f.apelido <> '' AND f.pessoa <> ?1 " +
    foraDaConta("f.pessoa", "?4") +
    semCopia("f", "?1") +
    "AND NOT EXISTS (SELECT 1 FROM bloqueio b WHERE (b.quem = ?1 AND b.alvo = f.pessoa) OR (b.quem = f.pessoa AND b.alvo = ?1)) " +
    "ORDER BY ativo DESC, f.pub LIMIT ?2 OFFSET ?3"
  ).bind(quem.id, COMUNIDADE_PAG + 1, pag * COMUNIDADE_PAG, contaDe(quem.id)).all();
  const linhas = r.results || [];
  return h.json({
    pessoas: linhas.slice(0, COMUNIDADE_PAG).map((x) => ({
      ...cartao(x, x.relacao || "", env), vendo: x.vendo || "",
    })),
    pagina: pag,
    mais: linhas.length > COMUNIDADE_PAG && pag + 1 < COMUNIDADE_PAGS ? 1 : 0,
  });
}

// --- pedidos de amizade -----------------------------------------------------------------

async function vincular(env, a, b) {
  const t = Math.floor(Date.now() / 1000);
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'pedido')").bind(a, b, t),
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'pedido')").bind(b, a, t),
    env.DB.prepare("DELETE FROM pedido WHERE (de = ? AND para = ?) OR (de = ? AND para = ?)").bind(a, b, b, a),
  ]);
}

async function rotaPedidoEnviar(env, quem, corpo, h) {
  const t = h.agora();
  if (!(await limitar(env, `pedido:${quem.id}`, 15, 86400, t))) return h.erro("limite diario", 429);
  const x = await achar(env, quem, corpo?.pub);
  if (!x) {
    // bloqueado ou inexistente respondem IGUAL para quem bloqueou-me nao saber
    // que foi bloqueado: sem esse silencio o bloqueio vira um aviso de "ele te
    // bloqueou".
    const p = pubLimpo(corpo?.pub);
    const existe = p.length === 10 && await env.DB.prepare("SELECT 1 FROM perfil WHERE pub = ?").bind(p).first();
    return existe ? h.json({ ok: 1, estado: "enviado" }) : h.erro("perfil nao encontrado", 404);
  }
  // QUEM PEDE PRECISA TER ESCOLHIDO UM APELIDO. O pedido chega mostrando quem
  // mandou, e sem esta regra o unico nome disponivel seria o da conta — que pode
  // ser o nome real, e a pessoa nunca escolheu mostra-lo a um desconhecido.
  const eu = await garantirPerfil(env, quem.id);
  if (!eu.apelido) return h.erro("defina um apelido antes de pedir amizade", 409);
  const rel = await relacaoCom(env, quem.id, x.id);
  if (rel === "amigo") return h.json({ ok: 1, estado: "amigo" });
  if (rel === "recebido") {            // ele ja tinha pedido a mim: dois "sim" = amigos
    await vincular(env, quem.id, x.id);
    return h.json({ ok: 1, estado: "amigo" });
  }
  if (rel === "enviado") return h.json({ ok: 1, estado: "enviado" });
  const env1 = await env.DB.prepare("SELECT COUNT(*) AS n FROM pedido WHERE de = ? AND estado = 0").bind(quem.id).first();
  if ((env1?.n || 0) >= PEDIDOS_ENVIADOS_MAX) return h.erro("muitos pedidos pendentes", 429);
  const rec1 = await env.DB.prepare("SELECT COUNT(*) AS n FROM pedido WHERE para = ? AND estado = 0").bind(x.id).first();
  // Caixa cheia do outro lado: aceita em silencio e nao grava (quem envia nao
  // aprende nada sobre a caixa de entrada alheia).
  if ((rec1?.n || 0) < PEDIDOS_RECEBIDOS_MAX) {
    await env.DB.prepare("INSERT OR IGNORE INTO pedido (de, para, criado, estado) VALUES (?, ?, ?, 0)")
      .bind(quem.id, x.id, t).run();
  }
  return h.json({ ok: 1, estado: "enviado" });
}

async function rotaPedidosLer(env, quem, h) {
  const r = await env.DB.prepare(
    "SELECT pe.de AS id, f.pub AS pub, f.apelido AS apelido, f.bio AS bio, f.generos AS generos, f.com_avatar AS comAvatar, " +
    "p.nome AS nome, p.avatar AS avatar, p.descobrivel AS descobrivel, pe.criado AS criado " +
    "FROM pedido pe JOIN perfil f ON f.pessoa = pe.de JOIN pessoa p ON p.id = pe.de " +
    "WHERE pe.para = ? AND pe.estado = 0 " +
    // Ja amigos (o vinculo veio por outro caminho: codigo, Trakt, fusao de
    // identidades) nao sao pedido: aceitar seria um botao sem efeito. Em
    // 06/10 eram 2 dos 21 pendentes. A linha nao e apagada aqui (GET).
    "AND NOT EXISTS (SELECT 1 FROM contato c WHERE c.a = pe.para AND c.b = pe.de) " +
    "AND NOT EXISTS (SELECT 1 FROM bloqueio b WHERE (b.quem = pe.para AND b.alvo = pe.de) OR (b.quem = pe.de AND b.alvo = pe.para)) " +
    "ORDER BY pe.criado DESC LIMIT 30"
  ).bind(quem.id).all();
  const n = await env.DB.prepare("SELECT COUNT(*) AS n FROM pedido WHERE de = ? AND estado = 0").bind(quem.id).first();
  return h.json({
    recebidos: (r.results || []).map((x) => ({ ...cartao(x, "recebido", env), criado: x.criado })),
    enviados: n?.n || 0,
  });
}

async function rotaPedidoAceitar(env, quem, corpo, h) {
  const x = await achar(env, quem, corpo?.pub);
  if (!x) return h.erro("perfil nao encontrado", 404);
  const ha = await env.DB.prepare("SELECT 1 FROM pedido WHERE de = ? AND para = ? AND estado = 0")
    .bind(x.id, quem.id).first();
  if (!ha) return h.erro("sem pedido desta pessoa", 404);
  await vincular(env, quem.id, x.id);
  return h.json({ ok: 1, estado: "amigo" });
}

// RECUSAR E EM SILENCIO: a linha vira estado 1 em vez de sumir, para quem
// mandou nao poder reenviar todo dia, e ele continua vendo "pedido enviado".
async function rotaPedidoRecusar(env, quem, corpo, h) {
  const x = await achar(env, quem, corpo?.pub);
  if (!x) return h.erro("perfil nao encontrado", 404);
  await env.DB.prepare("UPDATE pedido SET estado = 1, criado = ? WHERE de = ? AND para = ?")
    .bind(h.agora(), x.id, quem.id).run();
  return h.json({ ok: 1 });
}

async function rotaPedidoCancelar(env, quem, corpo, h) {
  const x = await achar(env, quem, corpo?.pub);
  if (!x) return h.json({ ok: 1 });
  await env.DB.prepare("DELETE FROM pedido WHERE de = ? AND para = ?").bind(quem.id, x.id).run();
  return h.json({ ok: 1 });
}

// --- bloqueio ------------------------------------------------------------------------------

// Aceita o handle (vindo de um cartao) OU o id (vindo da lista de amigos, onde
// o id ja e conhecido). Bloquear alguem sem perfil cria a linha so para o
// handle existir no desbloqueio.
async function alvoDe(env, quem, corpo) {
  const id = String(corpo?.id || "").slice(0, 96);
  if (id) {
    const p = await env.DB.prepare("SELECT id FROM pessoa WHERE id = ?").bind(id).first();
    if (!p || p.id === quem.id) return null;
    const f = await garantirPerfil(env, p.id);
    return { id: p.id, pub: f.pub };
  }
  const p = pubLimpo(corpo?.pub);
  if (p.length !== 10) return null;
  const f = await env.DB.prepare("SELECT pessoa, pub FROM perfil WHERE pub = ?").bind(p).first();
  return f && f.pessoa !== quem.id ? { id: f.pessoa, pub: f.pub } : null;
}

async function rotaBloquear(env, quem, corpo, h) {
  const alvo = await alvoDe(env, quem, corpo);
  if (!alvo) return h.erro("perfil nao encontrado", 404);
  const t = h.agora();
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO bloqueio (quem, alvo, criado) VALUES (?, ?, ?)").bind(quem.id, alvo.id, t),
    env.DB.prepare("DELETE FROM contato WHERE (a = ? AND b = ?) OR (a = ? AND b = ?)").bind(quem.id, alvo.id, alvo.id, quem.id),
    env.DB.prepare("DELETE FROM pedido WHERE (de = ? AND para = ?) OR (de = ? AND para = ?)").bind(quem.id, alvo.id, alvo.id, quem.id),
    env.DB.prepare("DELETE FROM rec WHERE de = ? AND para = ? AND visto = 0").bind(alvo.id, quem.id),
  ]);
  return h.json({ ok: 1 });
}

async function rotaDesbloquear(env, quem, corpo, h) {
  const alvo = await alvoDe(env, quem, corpo);
  if (alvo) await env.DB.prepare("DELETE FROM bloqueio WHERE quem = ? AND alvo = ?").bind(quem.id, alvo.id).run();
  return h.json({ ok: 1 });
}

async function rotaBloqueados(env, quem, h) {
  const r = await env.DB.prepare(
    "SELECT f.pub AS pub, COALESCE(NULLIF(f.apelido, ''), p.nome) AS nome FROM bloqueio b " +
    "JOIN perfil f ON f.pessoa = b.alvo JOIN pessoa p ON p.id = b.alvo WHERE b.quem = ? " +
    "ORDER BY b.criado DESC LIMIT 50"
  ).bind(quem.id).all();
  return h.json({ bloqueados: r.results || [] });
}

// --- atividade ------------------------------------------------------------------------------

// O CLIENTE ENVIA O TITULO; O SERVIDOR NAO GUARDA POSTER. A URL da capa e
// montada na TV de quem ve, a partir do imdb: aceitar a URL de quem escreve
// deixaria um amigo malicioso apontar a capa para um servidor dele e ler o IP
// de todo mundo que abrisse o feed.
async function rotaAtividadeEscrever(env, quem, corpo, h) {
  const t = h.agora();
  if (!(await limitar(env, `ativ:${quem.id}`, 120, 3600, t))) return h.erro("limite", 429);
  const imdb = h.limpar(corpo?.imdb, 16);
  if (!/^tt\d{1,10}$/.test(imdb)) return h.erro("imdb invalido", 400);
  const f = await garantirPerfil(env, quem.id);
  // CONSENTIMENTO CONFERIDO AQUI. Um cliente com o interruptor desligado que
  // mande atividade mesmo assim (versao antiga, erro, mao ma) nao grava nada.
  if (f.ativ < 1 && !f.recentes) return h.json({ ok: 1, guardado: 0 });
  const agoraMesmo = (corpo?.agora === 1 || corpo?.agora === true) && f.ativ >= 2 ? 1 : 0;
  const nota = Number.isFinite(corpo?.nota) ? Math.round(corpo.nota) : 0;
  await env.DB.batch([
    env.DB.prepare(
      "INSERT INTO atividade (pessoa, imdb, tipo, titulo, ano, nota, acao, criado) VALUES (?, ?, ?, ?, ?, ?, ?, ?) " +
      "ON CONFLICT(pessoa, imdb) DO UPDATE SET tipo = excluded.tipo, titulo = excluded.titulo, " +
      "ano = excluded.ano, nota = excluded.nota, acao = excluded.acao, criado = excluded.criado"
    ).bind(quem.id, imdb, h.limpar(corpo?.tipo, 8) === "series" ? "series" : "movie",
           h.limpar(corpo?.titulo, 160), h.limpar(corpo?.ano, 8),
           nota >= 0 && nota <= 100 ? nota : 0, agoraMesmo, t),
    env.DB.prepare(
      "DELETE FROM atividade WHERE pessoa = ? AND imdb NOT IN " +
      "(SELECT imdb FROM atividade WHERE pessoa = ? ORDER BY criado DESC LIMIT ?)"
    ).bind(quem.id, quem.id, ATIV_MAX),
  ]);
  return h.json({ ok: 1, guardado: 1 });
}

async function rotaAtividadeAmigos(env, quem, h) {
  const t = h.agora();
  const r = await env.DB.prepare(
    "SELECT p.id AS de, COALESCE(NULLIF(f.apelido, ''), p.nome) AS deNome, p.avatar AS deAvatar, " +
    "a.imdb AS imdb, a.tipo AS tipo, a.titulo AS titulo, a.ano AS ano, a.nota AS nota, " +
    "a.acao AS agora, a.criado AS criado " +
    "FROM contato c JOIN perfil f ON f.pessoa = c.b AND f.ativ >= 1 " +
    "JOIN atividade a ON a.pessoa = c.b JOIN pessoa p ON p.id = c.b " +
    // "ASSISTINDO AGORA" VENCE EM 10 MIN e SOME (nao vira "assistiu"): quem
    // largou o filme no meio nao assistiu nada, e dizer que sim a um amigo seria
    // uma afirmacao falsa sobre ela.
    "WHERE c.a = ? AND a.criado > ? AND (a.acao = 0 OR a.criado > ?) " +
    "AND EXISTS (SELECT 1 FROM contato v WHERE v.a = c.b AND v.b = c.a) " +          // os DOIS lados
    "AND NOT EXISTS (SELECT 1 FROM bloqueio b WHERE (b.quem = c.a AND b.alvo = c.b) OR (b.quem = c.b AND b.alvo = c.a)) " +
    "ORDER BY a.criado DESC LIMIT ?"
  ).bind(quem.id, t - RETENCAO_ATIV, t - AGORA_S, FEED_MAX).all();
  return h.json({ itens: r.results || [] });
}

// --- despacho e limpeza --------------------------------------------------------------------------

export async function rotaAmigos(rota, metodo, env, quem, corpo, h) {
  if (metodo === "GET") {
    if (rota === "/v1/perfil") return rotaPerfilLer(env, quem, h);
    if (rota === "/v1/pedidos") return rotaPedidosLer(env, quem, h);
    if (rota === "/v1/bloqueados") return rotaBloqueados(env, quem, h);
    if (rota === "/v1/amigos/atividade") return rotaAtividadeAmigos(env, quem, h);
    return null;
  }
  if (metodo !== "POST") return null;
  switch (rota) {
    case "/v1/perfil": return rotaPerfilPublicar(env, quem, corpo, h);
    case "/v1/perfil/despublicar": return rotaPerfilDespublicar(env, quem, h);
    case "/v1/perfil/apagar": return rotaPerfilApagar(env, quem, h);
    case "/v1/perfil/atividade": return rotaPerfilAtividade(env, quem, corpo, h);
    case "/v1/perfis/buscar": return rotaBuscar(env, quem, corpo, h);
    case "/v1/perfis/ver": return rotaVer(env, quem, corpo, h);
    case "/v1/perfis/sugeridos": return rotaSugeridos(env, quem, corpo, h);
    case "/v1/perfis/comunidade": return rotaComunidade(env, quem, corpo, h);
    case "/v1/pedidos/enviar": return rotaPedidoEnviar(env, quem, corpo, h);
    case "/v1/pedidos/aceitar": return rotaPedidoAceitar(env, quem, corpo, h);
    case "/v1/pedidos/recusar": return rotaPedidoRecusar(env, quem, corpo, h);
    case "/v1/pedidos/cancelar": return rotaPedidoCancelar(env, quem, corpo, h);
    case "/v1/bloquear": return rotaBloquear(env, quem, corpo, h);
    case "/v1/desbloquear": return rotaDesbloquear(env, quem, corpo, h);
    case "/v1/atividade": return rotaAtividadeEscrever(env, quem, corpo, h);
  }
  return null;
}

// Usado por index.js: garante que quem ja e "descobrivel" tem handle, e devolve
// o handle publico de um id de conta (nunca o id).
export { garantirPerfil, bloqueado, avatarPublico, norm, limitar, seloDe, mesmaConta };

export function limpezaAmigos(env, t) {
  return [
    env.DB.prepare("DELETE FROM atividade WHERE criado < ?").bind(t - RETENCAO_ATIV),
    env.DB.prepare("DELETE FROM atividade WHERE acao = 1 AND criado < ?").bind(t - AGORA_S),
    env.DB.prepare("DELETE FROM pedido WHERE estado = 0 AND criado < ?").bind(t - RETENCAO_PEDIDO),
    env.DB.prepare("DELETE FROM pedido WHERE estado = 1 AND criado < ?").bind(t - RETENCAO_RECUSA),
    env.DB.prepare("DELETE FROM limite WHERE expira < ?").bind(t),
  ];
}
