// Recomendacoes entre amigos do Nuvio nativo.
//
// POR QUE ESTE SERVICO EXISTE: o Supabase do Nuvio nao tem nada social. Medido
// em 15/09/2026 no indice OpenAPI do PostgREST (`GET /rest/v1/` com a anon
// key): 13 tabelas e 52 RPC, nenhuma de amigo, recomendacao ou aviso. E este
// repositorio e um fork nao oficial — criar tabela la nao e uma decisao nossa.
// Ver PLANO-SOCIAL-RECOMENDACOES.md.
//
// O QUE ELE NAO GUARDA, de proposito: token (so o SHA-256, e por 10 min),
// e-mail, IP. A identidade e um identificador estavel e um nome de exibicao.

import { rotaXtream } from "./xtream.js";
import { codigoRegistro } from "./codigo.js";
import { rotaTrailerImdb, rotaTrailerYoutube } from "./trailer.js";
import { rotaNoticia, rotaNoticiaImg } from "./noticia.js";
import { rotaAmigos, limpezaAmigos, despublicar, garantirPerfil, avatarPublico, limitar, seloDe, mesmaConta } from "./amigos.js";
import { rotaEuNome, rotaAlcance, rotaEvento, rotaFeed, rotaAmigo, limpezaSocial,
         limparNome, avatarPerfilOk, resolverNome, rotaRecResposta } from "./social.js";
import { rotaEnquete } from "./enquete.js";
import { rotaTrava, travaRev } from "./trava.js";
import { resolverCanonica, canonizarEntrada, identidadesDe, rotaIdentidades, idSimkl,
         perfilExiste, RECURSO } from "./identidade.js";

const DIA = 86400;
const RETENCAO = 90 * DIA;
const SESSAO_TTL = 600;          // 10 min de cache da verificacao de identidade
const LIM_DIA = 20;              // recomendacoes enviadas por pessoa por dia
const LIM_PAR = 5;               // ... para a MESMA pessoa
const TEXTO_MAX = 60;
// Teto da lista de sugestoes. Vinte porque a TV desenha uma linha por sugestao
// numa coluna de 688px e ninguem rola quarenta nomes com um controle remoto; e
// porque a consulta de amigo-de-amigo cresce com o QUADRADO do tamanho da roda
// de contatos, e um teto no SQL e mais barato que um teto no cliente.
const SUG_MAX = 20;
const CODIGO_ABC = "abcdefghijkmnpqrstuvwxyz23456789"; // sem l/o/0/1

const agora = () => Math.floor(Date.now() / 1000);

// CORS aberto: o cliente e o app na TV (o .wgt dispensa a checagem, mas o
// mesmo codigo roda no Chrome de mesa nos testes) e nenhuma rota responde
// nada sem o Bearer — a origem nao e o que protege aqui.
const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-headers": "authorization, content-type, x-nuvio-auth, x-nuvio-perfil, if-none-match",
  "access-control-allow-methods": "GET, POST, OPTIONS",
  "cross-origin-resource-policy": "cross-origin",
  // O 304 de /v1/rec so manda `etag` (ver rotaReceber): sem isto exposto o
  // XHR da TV enxerga o 304 mas nunca le o cabecalho para comparar com o que
  // ja tinha guardado, e cai numa sondagem que nunca acerta o cache.
  "access-control-expose-headers": "etag",
};
function json(dados, status = 200, extra = {}) {
  return new Response(JSON.stringify(dados), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", ...CORS, ...extra },
  });
}
const erro = (msg, status) => json({ erro: msg }, status);

async function sha256(s) {
  const b = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s));
  return [...new Uint8Array(b)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

// O teclado da TV e `a-z0-9` minusculo (busca.c:106). Tudo que nao cabe nele
// tambem nao precisa entrar no banco.
function limparTexto(s) {
  return String(s || "")
    .toLowerCase()
    .replace(/[^a-z0-9 ]+/g, " ")
    .replace(/\s+/g, " ")
    .trim()
    .slice(0, TEXTO_MAX);
}
const limpar = (s, n) => String(s == null ? "" : s).slice(0, n);

// --- Identidade --------------------------------------------------------------
//
// O cliente diz quem acha que e; o servidor confirma com quem emitiu o token.
// Nunca se confia no corpo do pedido para isso.

// O USER-AGENT NAO E ENFEITE, e foi o que custou a primeira TV real: o fetch
// do Workers nao manda User-Agent nenhum, e o Trakt responde 403 a requisicao
// SEM ele — o mesmo 403 que ele daria a um client id errado. Medido com curl:
// token invalido COM user-agent da 401, o MESMO pedido com `-A ""` da 403.
const UA = "nuvio-recomendacoes/1 (+https://github.com/iqui27/nuvio-native-legacy)";

// `?extended=full` NAO E ENFEITE: sem ele o Trakt devolve o usuario sem o bloco
// `images`, e a foto de perfil simplesmente nao vem. trakt.c ja pede assim pelo
// mesmo motivo (ver a chamada de users/settings em trakt.c, "o avatar pode ser
// WebP no Trakt novo"). O slug, que e a identidade, vem nos dois casos — entao
// a falta do parametro custava so a foto, calada.
async function idTrakt(token, env) {
  const r = await fetch("https://api.trakt.tv/users/settings?extended=full", {
    headers: {
      authorization: `Bearer ${token}`,
      "trakt-api-version": "2",
      "trakt-api-key": env.TRAKT_CLIENT_ID,
      "user-agent": UA,
    },
  });
  if (!r.ok) {
    // O CODIGO DO TRAKT VAI PARA O LOG, e so ele. Sem isto o cliente ve um 401
    // nosso e nao ha como saber se o token venceu, se o client id e de outro
    // aplicativo ou se o Trakt estava fora — tres consertos diferentes.
    console.log(`trakt /users/settings -> ${r.status}`);
    return null;
  }
  const d = await r.json();
  const slug = d?.user?.ids?.slug || d?.user?.username;
  if (!slug) return null;
  return {
    id: `trakt:${slug}`,
    nome: limpar(d?.user?.name || d?.user?.username || slug, 64),
    // `full` e a maior das tres (full/medium/thumb) e e a unica que o Trakt
    // preenche sempre. A TV reduz na textura; pedir a `thumb` daria uma foto de
    // 64px esticada num disco de 56 na tela de 1080p.
    avatar: limpar(d?.user?.images?.avatar?.full || "", 512),
  };
}

async function idNuvio(token, env) {
  const r = await fetch(`${env.SUPABASE_URL}/auth/v1/user`, {
    headers: { apikey: env.SUPABASE_ANON_KEY, authorization: `Bearer ${token}`,
               "user-agent": UA },
  });
  if (!r.ok) {
    console.log(`supabase /auth/v1/user -> ${r.status}`);
    return null;
  }
  const d = await r.json();
  if (!d?.id) return null;
  const nome = d?.user_metadata?.name || d?.user_metadata?.full_name || "";
  // SEM FOTO, E ISSO E DEFINITIVO AQUI. `/auth/v1/user` devolve a identidade da
  // conta, nao o perfil escolhido na TV — e a foto do Nuvio mora em `profiles`,
  // que este servico nao tem permissao (nem motivo) para ler. Chutar
  // `user_metadata.avatar_url` seria inventar um campo que nao foi medido. Quem
  // resolve este caso e o cliente, com a inicial num disco colorido.
  return { id: `nuvio:${d.id}`, nome: limpar(nome, 64), avatar: "" };
}

async function quemE(req, env) {
  const aut = req.headers.get("authorization") || "";
  const token = aut.startsWith("Bearer ") ? aut.slice(7).trim() : "";
  const via = (req.headers.get("x-nuvio-auth") || "").toLowerCase();
  if (!token || (via !== "trakt" && via !== "nuvio")) return null;

  const hash = await sha256(`${via}:${token}`);
  const t = agora();
  const cache = await env.DB.prepare("SELECT id, nome FROM sessao WHERE hash = ? AND expira > ?")
    .bind(hash, t).first();
  // A SESSAO EM CACHE NAO CARREGA FOTO, e nao falta coluna nenhuma para isso:
  // `avatar: ""` aqui quer dizer "nao perguntei agora", e `registrar` so
  // sobrescreve a foto guardada quando ela vem de uma verificacao DE VERDADE.
  // Sem essa regra, os 10 min de cache apagariam a foto de todo mundo.
  if (cache) return comPerfil(req, via, { id: cache.id, nome: cache.nome, avatar: "" });

  const quem = via === "trakt" ? await idTrakt(token, env) : await idNuvio(token, env);
  if (!quem) return null;
  await env.DB.prepare(
    "INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, ?) " +
    "ON CONFLICT(hash) DO UPDATE SET id = excluded.id, nome = excluded.nome, expira = excluded.expira"
  ).bind(hash, quem.id, quem.nome, t + SESSAO_TTL).run();
  return comPerfil(req, via, quem);
}

// CADA PERFIL NUVIO E UMA PESSOA (decisao do dono, 02/10/2026). O token prova a
// CONTA; o perfil vem no cabecalho `X-Nuvio-Perfil: <profile_index>` e so
// escolhe ENTRE as pessoas daquela conta — nao ha valor que leve a outra conta.
// O PRINCIPAL nao manda o cabecalho e continua `nuvio:<sub>`: e assim que tudo
// o que foi gravado antes (contatos, recs, perfil publico) continua dele sem
// migrar linha nenhuma. Trakt ignora: o slug ja e de uma pessoa.
function comPerfil(req, via, quem) {
  if (via !== "nuvio") return quem;
  const v = (req.headers.get("x-nuvio-perfil") || "").trim();
  if (!/^\d{1,2}$/.test(v)) return quem;
  const n = parseInt(v, 10);
  if (n < 1 || n > 32) return quem;
  return { ...quem, id: `${quem.id}:${n}`, perfil: n };
}

async function codigoLivre(db) {
  for (let tentativa = 0; tentativa < 8; tentativa++) {
    const bytes = crypto.getRandomValues(new Uint8Array(6));
    const c = [...bytes].map((b) => CODIGO_ABC[b % CODIGO_ABC.length]).join("");
    const ja = await db.prepare("SELECT 1 FROM pessoa WHERE codigo = ?").bind(c).first();
    if (!ja) return c;
  }
  return null;
}

// Registra ou atualiza a pessoa. E o unico ponto que cria linha em `pessoa`.
//
// `extra` so vem de POST /v1/eu: o nome e a foto do PERFIL ativo na TV. As
// outras rotas chamam sem ele e nao mexem nesses campos.
//
// O NOME QUE SAI (`pessoa.nome`) E RECALCULADO AQUI, sempre: exibicao digitada >
// nome do perfil (ou da conta, no Trakt) > "Amigo #<rowid>". Todas as consultas
// antigas leem `pessoa.nome`, entao nenhuma delas volta a mostrar UUID.
async function registrar(env, quem, extra) {
  const t = agora();
  // `descobrivel` e `alcance` SAEM DAQUI E NAO ENTRAM: so as rotas proprias
  // (/v1/descobrivel, /v1/alcance) escrevem — uma sondagem nunca desfaz a
  // escolha da pessoa por omissao do cliente.
  let ja = await env.DB.prepare(
    "SELECT rowid AS n, id, nome, codigo, avatar, descobrivel, nome_conta, nome_perfil, " +
    "exibicao, avatar_perfil, alcance FROM pessoa WHERE id = ?"
  ).bind(quem.id).first();
  if (!ja) {
    const codigo = await codigoLivre(env.DB);
    // `descobrivel` e `alcance` ficam no DEFAULT do esquema (0 e -1).
    const r = await env.DB.prepare(
      "INSERT INTO pessoa (id, nome, codigo, avatar, criado, visto, nome_conta) VALUES (?, '', ?, '', ?, ?, ?)"
    ).bind(quem.id, codigo, t, t, quem.nome || "").run();
    ja = { n: r.meta?.last_row_id || 0, id: quem.id, nome: "", codigo, avatar: "", descobrivel: 0,
           nome_conta: quem.nome || "", nome_perfil: "", exibicao: "", avatar_perfil: "", alcance: -1 };
  }
  let codigo = ja.codigo;
  if (!codigo) {
    codigo = await codigoLivre(env.DB);
    await env.DB.prepare("UPDATE pessoa SET codigo = ? WHERE id = ?").bind(codigo, quem.id).run();
  }
  // O QUE VEIO VAZIO NAO APAGA O GUARDADO: verificacao servida pelo cache da
  // sessao chega sem foto e, na conta Nuvio, quase sempre sem nome.
  const nomeConta = quem.nome || ja.nome_conta || "";
  const nomePerfil = extra && extra.nome !== undefined ? limparNome(extra.nome, 64) : (ja.nome_perfil || "");
  const avatarPerfil = extra && extra.avatar !== undefined ? avatarPerfilOk(extra.avatar, env) : (ja.avatar_perfil || "");
  // FOTO: no Trakt e a da verificacao (como sempre foi); na conta Nuvio e a do
  // perfil, que so a TV conhece — filtrada por host em avatarPerfilOk.
  const avatar = quem.id.startsWith("trakt:")
    ? (quem.avatar || ja.avatar || avatarPerfil || "")
    : (avatarPerfil || "");
  const p = { ...ja, nome_conta: nomeConta, nome_perfil: nomePerfil };
  const nome = resolverNome(p, quem.id);
  await env.DB.prepare(
    "UPDATE pessoa SET nome = ?, avatar = ?, visto = ?, nome_conta = ?, nome_perfil = ?, avatar_perfil = ? WHERE id = ?"
  ).bind(nome, avatar, t, nomeConta, nomePerfil, avatarPerfil, quem.id).run();
  return {
    id: quem.id, nome, codigo, avatar, descobrivel: ja.descobrivel ? 1 : 0,
    exibicao: ja.exibicao || "", alcance: Number.isInteger(ja.alcance) ? ja.alcance : -1,
    perfil: quem.perfil || 0,
  };
}

const bloqueadoPar = (db, a, b) =>
  db.prepare("SELECT 1 FROM bloqueio WHERE (quem = ? AND alvo = ?) OR (quem = ? AND alvo = ?)")
    .bind(a, b, b, a).first();

const saoContatos = (db, a, b) =>
  db.prepare("SELECT 1 FROM contato WHERE a = ? AND b = ?").bind(a, b).first();

// --- Rotas -------------------------------------------------------------------

async function rotaContatosLer(env, quem) {
  const r = await env.DB.prepare(
    "SELECT p.id AS id, p.nome AS nome, p.avatar AS avatar, c.criado AS desde, c.via AS via, " +
    // O APELIDO que o amigo escolheu (vazio se nunca escolheu): a linha dele
    // mostra o nome e, embaixo, o @apelido.
    "COALESCE((SELECT f.apelido FROM perfil f WHERE f.pessoa = p.id), '') AS apelido, " +
    // IDENTIDADES LIGADAS (migracao 008), so as verificadas que a pessoa deixou
    // amigos verem: a TV junta o feed do Trakt dela com o nosso por elas.
    "(SELECT GROUP_CONCAT(i.provedor || ':' || i.sujeito, ' ') FROM identidade i " +
    " WHERE i.pessoa = p.id AND i.verificado = 1 AND i.visivel = 1) AS ids FROM contato c " +
    "JOIN pessoa p ON p.id = c.b WHERE c.a = ? ORDER BY p.nome"
  ).bind(quem.id).all();
  return json({
    contatos: (r.results || []).map((x) => ({
      id: x.id,
      nome: x.nome,
      apelido: x.apelido || "",
      // selo de criador, decidido aqui pela conta (amigos.js: seloDe)
      ...(seloDe(env, x.id) ? { selo: seloDe(env, x.id) } : {}),
      avatar: x.avatar || "",
      origem: x.id.startsWith("trakt:") ? "trakt" : "nuvio",
      // desde quando e contato e por onde (codigo|trakt|sugestao|pedido|"")
      desde: x.desde || 0, via: x.via || "",
      ids: x.ids ? String(x.ids).split(" ").filter(Boolean) : [],
    })),
  });
}

async function rotaContatosVincular(env, quem, corpo) {
  const codigo = limparTexto(corpo?.codigo).replace(/ /g, "");
  if (codigo.length !== 6) return erro("codigo invalido", 400);
  const outro = await env.DB.prepare("SELECT id, nome FROM pessoa WHERE codigo = ?")
    .bind(codigo).first();
  if (!outro) return erro("codigo nao encontrado", 404);
  if (outro.id === quem.id) return erro("esse codigo e seu", 400);
  // BLOQUEIO VALE PARA O CODIGO TAMBEM, nos dois sentidos, e responde IGUAL a
  // "codigo inexistente": quem foi bloqueado nao pode descobrir isso tentando.
  if (await bloqueadoPar(env.DB, quem.id, outro.id)) return erro("codigo nao encontrado", 404);
  const t = agora();
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'codigo')")
      .bind(quem.id, outro.id, t),
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'codigo')")
      .bind(outro.id, quem.id, t),
  ]);
  return json({ ok: 1, contato: { id: outro.id, nome: outro.nome } });
}

async function rotaContatoRemover(env, quem, corpo) {
  const outro = limpar(corpo?.id, 96);
  if (!outro) return erro("sem id", 400);
  await env.DB.batch([
    env.DB.prepare("DELETE FROM contato WHERE a = ? AND b = ?").bind(quem.id, outro),
    env.DB.prepare("DELETE FROM contato WHERE a = ? AND b = ?").bind(outro, quem.id),
    env.DB.prepare("DELETE FROM rec WHERE de = ? AND para = ? AND visto = 0").bind(outro, quem.id),
  ]);
  return json({ ok: 1 });
}

// O amigo do Trakt ja e contato por construcao: os dois se seguem la. Em vez de
// obrigar a parear de novo, o cliente manda a lista de slugs que o Trakt
// respondeu e o servidor vincula os que ja usam este servico. Quem nunca abriu
// o app nao vira contato — nao ha ninguem para receber.
async function rotaContatosTrakt(env, quem, corpo) {
  // Conta Nuvio com Trakt LIGADO (migracao 008) tambem pode: a pessoa e a mesma.
  if (!quem.id.startsWith("trakt:") && !String(quem.bruto || "").startsWith("trakt:")) {
    const ligado = await env.DB.prepare(
      "SELECT 1 FROM identidade WHERE pessoa = ? AND provedor = 'trakt' AND verificado = 1").bind(quem.id).first();
    if (!ligado) return erro("so para conta trakt", 400);
  }
  const slugs = Array.isArray(corpo?.slugs) ? corpo.slugs.slice(0, 200) : [];
  const ids = slugs.map((s) => `trakt:${limparTexto(s).replace(/ /g, "-")}`)
    .filter((s) => s.length > 6 && s !== quem.id);
  if (!ids.length) return json({ vinculados: 0 });
  const marcas = ids.map(() => "?").join(",");
  // O seguido que ja ligou o Trakt ao perfil Nuvio e achado pela identidade.
  const r = await env.DB.prepare(
    `SELECT id FROM pessoa WHERE (id IN (${marcas}) OR id IN (SELECT pessoa FROM identidade ` +
    `WHERE provedor = 'trakt' AND verificado = 1 AND 'trakt:' || sujeito IN (${marcas}))) AND id <> ? ` +
    `AND NOT EXISTS (SELECT 1 FROM bloqueio b WHERE (b.quem = ? AND b.alvo = pessoa.id) OR (b.quem = pessoa.id AND b.alvo = ?))`
  ).bind(...ids, ...ids, quem.id, quem.id, quem.id).all();
  const t = agora();
  const cmds = [];
  for (const x of r.results || []) {
    cmds.push(env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'trakt')")
      .bind(quem.id, x.id, t));
    cmds.push(env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'trakt')")
      .bind(x.id, quem.id, t));
  }
  if (cmds.length) await env.DB.batch(cmds);
  return json({ vinculados: (r.results || []).length });
}

// --- "Posso aparecer para outras pessoas?" ------------------------------------
//
// A ROTA NAO ACEITA UM ID, e essa ausencia e o mecanismo inteiro. Quem escreve
// e sempre `quem.id`, que saiu da verificacao do token contra o Trakt ou o
// Supabase — nao ha campo no corpo que aponte para outra pessoa, entao nao
// existe pedido malformado, falsificado ou bem-intencionado que ligue o
// sinalizador de terceiro. Um `{"id":"...","descobrivel":1}` e aceito e o `id`
// e ignorado: o efeito recai sobre quem mandou.
//
// DESLIGAR TEM DE SER TAO BARATO QUANTO LIGAR. E o mesmo POST com 0, sem
// confirmacao e sem periodo de carencia — e, como a consulta de sugestoes le a
// coluna a cada pedido, no instante seguinte a pessoa sumiu da sugestao de todo
// mundo. Nao ha copia da lista de "descobriveis" em lugar nenhum para
// envelhecer.
async function rotaDescobrivel(env, quem, corpo) {
  const v = corpo?.descobrivel ? 1 : 0;
  // DESLIGAR = DESPUBLICAR DE VERDADE. Antes desta rota so mexer na coluna
  // bastava; agora ha um perfil publico (apelido, bio, foto) e deixa-lo gravado
  // com o sinalizador em 0 seria uma copia esquecida. `despublicar` limpa tudo
  // e sorteia o handle de novo.
  if (!v) await despublicar(env, quem.id);
  else {
    await env.DB.prepare("UPDATE pessoa SET descobrivel = 1 WHERE id = ?").bind(quem.id).run();
    await garantirPerfil(env, quem.id);
  }
  return json({ ok: 1, descobrivel: v });
}

// --- Sugestoes de gente para adicionar ---------------------------------------
//
// AS DUAS FONTES, e nada alem delas. Nenhuma das duas precisa de um dado novo
// saindo da TV:
//
//   (a) QUEM ELA JA SEGUE NO TRAKT e tambem usa este servico. A lista de slugs
//       e a MESMA que `/v1/contatos/trakt` ja recebe hoje — nao e informacao
//       nova, e o proprio Trakt a publica. A diferenca e o que se faz com ela:
//       la vira contato na hora, aqui vira uma SUGESTAO que a pessoa aceita.
//
//   (b) AMIGO DE UM AMIGO: um JOIN de `contato` com ele mesmo. O cliente nao
//       manda nada para isto, e nem poderia — ele nao conhece a lista de
//       contatos dos contatos dele, e nunca vai conhecer: o que volta daqui e
//       "fulano, alcancavel por Gustavo", nunca a lista de amigos do Gustavo.
//
// O FILTRO `descobrivel = 1` VALE PARA AS DUAS. Ele podia valer so para (b) —
// em (a) a pessoa ja segue o outro no Trakt e portanto ja sabe que ele existe.
// Vale para as duas assim mesmo, porque a pergunta que a tela de consentimento
// faz e "voce aceita aparecer nas sugestoes dos outros?" e uma excecao
// silenciosa faria daquela frase uma mentira.
//
// CUSTO: (a) e um `IN (...)` sobre a chave primaria de `pessoa`, ou seja uma
// busca por linha pedida, no maximo 200. (b) percorre os contatos DELA e, para
// cada um, os contatos DELE — as duas pernas pela PK de `contato` — e corta em
// SUG_MAX. Para uma roda de 20 contatos com 20 contatos cada, sao 400 linhas
// examinadas por indice. Nao ha varredura de tabela em nenhuma das duas.
async function sugestoesDe(env, quem, corpo) {
  const vistos = new Set([quem.id]);
  const saida = [];
  // O QUE SAI PARA FORA DESTA LISTA, e o que nao. O `id` interno (`_id`) so
  // serve ao servidor; quem recebe a lista ve `id` = handle opaco
  // ("pub:<handle>") quando a conta e do Nuvio, porque `nuvio:<uuid>` e o
  // identificador da conta e nao se entrega a quem so e amigo de um amigo. Ids
  // do Trakt continuam como eram: o slug ja e publico no proprio Trakt. O nome
  // e o APELIDO que a pessoa escolheu, quando ha; a foto so sai se ela
  // marcou "mostrar minha foto" e a URL e de um host sem hash de e-mail.
  const empurra = async (x, origem, viaNome) => {
    if (vistos.has(x.id) || saida.length >= SUG_MAX) return;
    // Outro perfil da MINHA conta nao e "gente que talvez voce conheca".
    if (mesmaConta(x.id, quem.id)) return;
    vistos.add(x.id);
    let id = x.id;
    if (!id.startsWith("trakt:")) {
      const f = x.pub ? { pub: x.pub } : await garantirPerfil(env, x.id);
      id = `pub:${f.pub}`;
    }
    saida.push({
      _id: x.id, id,
      nome: x.apelido || x.nome || "",
      avatar: x.comAvatar ? avatarPublico(x.avatar) : "",
      origem, viaNome: viaNome || "",
      ...(seloDe(env, x.id) ? { selo: seloDe(env, x.id) } : {}),
    });
  };
  const SEL = "p.id AS id, p.nome AS nome, p.avatar AS avatar, f.pub AS pub, f.apelido AS apelido, " +
              "COALESCE(f.com_avatar, 0) AS comAvatar";
  const SEM_BLOQUEIO = (col) =>
    `AND NOT EXISTS (SELECT 1 FROM bloqueio bq WHERE (bq.quem = ? AND bq.alvo = ${col}) OR (bq.quem = ${col} AND bq.alvo = ?)) `;

  const slugs = Array.isArray(corpo?.slugs) ? corpo.slugs.slice(0, 200) : [];
  const ids = [...new Set(
    slugs.map((s) => `trakt:${limparTexto(s).replace(/ /g, "-")}`)
         .filter((s) => s.length > 6 && s !== quem.id)
  )];
  if (ids.length) {
    const marcas = ids.map(() => "?").join(",");
    const r = await env.DB.prepare(
      `SELECT ${SEL} FROM pessoa p LEFT JOIN perfil f ON f.pessoa = p.id ` +
      `WHERE (p.id IN (${marcas}) OR p.id IN (SELECT pessoa FROM identidade WHERE provedor = 'trakt' ` +
      `AND verificado = 1 AND 'trakt:' || sujeito IN (${marcas}))) AND p.id <> ? AND p.descobrivel = 1 ` +
      // JA E CONTATO NAO E SUGESTAO. Sem este NOT EXISTS a aba abriria pedindo
      // para adicionar quem ja esta na lista de contatos logo acima.
      `AND NOT EXISTS (SELECT 1 FROM contato c WHERE c.a = ? AND c.b = p.id) ` +
      SEM_BLOQUEIO("p.id") +
      `ORDER BY p.nome LIMIT ?`
    ).bind(...ids, ...ids, quem.id, quem.id, quem.id, quem.id, SUG_MAX).all();
    for (const x of r.results || []) await empurra(x, "trakt", "");
  }

  const r2 = await env.DB.prepare(
    `SELECT ${SEL}, ` +
    // O NOME DO INTERMEDIARIO, e so ele: e o que a linha da TV mostra ("amigo
    // de Gustavo"). MIN() porque o GROUP BY colapsa varios caminhos ate a mesma
    // pessoa num so, e mostrar "amigo de Gustavo, Marina e mais 3" contaria a
    // quem recebe quantos contatos em comum existem — que e informacao sobre os
    // OUTROS dois, nao sobre ela. COALESCE/NULLIF porque quem nunca preencheu o
    // nome no Trakt tem `nome` vazio e "amigo de" sozinho nao diz nada.
    `MIN(COALESCE(NULLIF(vf.apelido, ''), NULLIF(v.nome, ''), v.id)) AS viaNome ` +
    `FROM contato c1 ` +
    `JOIN contato c2 ON c2.a = c1.b ` +
    `JOIN pessoa  p  ON p.id = c2.b ` +
    `LEFT JOIN perfil f  ON f.pessoa = p.id ` +
    `JOIN pessoa  v  ON v.id = c1.b ` +
    `LEFT JOIN perfil vf ON vf.pessoa = v.id ` +
    `WHERE c1.a = ? AND c2.b <> ? AND p.descobrivel = 1 ` +
    `AND NOT EXISTS (SELECT 1 FROM contato x WHERE x.a = ? AND x.b = p.id) ` +
    SEM_BLOQUEIO("p.id") +
    `GROUP BY p.id, p.nome, p.avatar, f.pub, f.apelido, f.com_avatar ORDER BY p.nome LIMIT ?`
  ).bind(quem.id, quem.id, quem.id, quem.id, quem.id, SUG_MAX).all();
  for (const x of r2.results || []) await empurra(x, "amigo", x.viaNome);
  return saida.slice(0, SUG_MAX);
}

const rotaSugestoes = async (env, quem, corpo) =>
  json({ sugestoes: (await sugestoesDe(env, quem, corpo)).map(({ _id, ...pub }) => pub) });

// Adiciona UMA sugestao como contato.
//
// ELA RECALCULA A LISTA EM VEZ DE CONFIAR NO ID QUE CHEGOU, e e a mesma funcao
// que desenhou a tela — nao uma segunda regra parecida. Sem isto a rota seria
// "vincule-me a qualquer id", e quem soubesse (ou adivinhasse) o `nuvio:<sub>`
// de alguem viraria contato dele sem passar por codigo, por Trakt, nem pelo
// consentimento. Custa uma consulta a mais num pedido que acontece quando
// alguem aperta OK, e nao por sondagem.
async function rotaContatoSugerido(env, quem, corpo) {
  const alvo = limpar(corpo?.id, 96);
  if (!alvo) return erro("sem id", 400);
  const lista = await sugestoesDe(env, quem, corpo);
  const achado = lista.find((x) => x.id === alvo);
  if (!achado) return erro("nao esta nas suas sugestoes", 403);
  const t = agora();
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'sugestao')")
      .bind(quem.id, achado._id, t),
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'sugestao')")
      .bind(achado._id, quem.id, t),
  ]);
  // A resposta devolve o handle, nao o id da conta: e o que o cliente ja tinha.
  return json({ ok: 1, contato: { id: alvo, nome: achado.nome } });
}

async function rotaEnviar(env, quem, corpo) {
  const para = limpar(corpo?.para, 96);
  const imdb = limpar(corpo?.imdb, 16);
  if (!para || !/^tt\d+$/.test(imdb)) return erro("para/imdb invalidos", 400);
  if (!(await saoContatos(env.DB, quem.id, para))) return erro("voces nao sao contatos", 403);

  const t = agora();
  const desde = t - DIA;
  const nDia = await env.DB.prepare("SELECT COUNT(*) AS n FROM rec WHERE de = ? AND criado > ?")
    .bind(quem.id, desde).first();
  if ((nDia?.n || 0) >= LIM_DIA) return erro("limite diario", 429);
  const nPar = await env.DB.prepare(
    "SELECT COUNT(*) AS n FROM rec WHERE de = ? AND para = ? AND criado > ?"
  ).bind(quem.id, para, desde).first();
  if ((nPar?.n || 0) >= LIM_PAR) return erro("limite para este amigo", 429);

  const modelo = Number.isInteger(corpo?.modelo) ? corpo.modelo : 0;
  const texto = modelo === -1 ? limparTexto(corpo?.texto) : "";
  if (modelo === -1 && !texto) return erro("texto vazio", 400);

  // A NOTA VEM DE QUEM MANDA, em centesimos (83 = 8,3), porque so ele tem o
  // titulo na mao. Fora da faixa vira 0 em vez de 400: o cliente desenha "0,0"
  // para qualquer numero que chegue, e um envio nao deve morrer por causa de um
  // campo decorativo.
  const nota = Number.isFinite(corpo?.nota) ? Math.round(corpo.nota) : 0;
  const notaOk = nota >= 0 && nota <= 100 ? nota : 0;

  const r = await env.DB.prepare(
    "INSERT INTO rec (de, para, criado, imdb, tipo, titulo, poster, ano, modelo, texto, nota) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
  ).bind(
    quem.id, para, t, imdb,
    limpar(corpo?.tipo, 8) || "movie",
    limpar(corpo?.titulo, 160),
    limpar(corpo?.poster, 512),
    limpar(corpo?.ano, 16),
    modelo, texto, notaOk
  ).run();
  return json({ ok: 1, id: r.meta?.last_row_id || 0 });
}

export async function rotaReceber(env, quem, url, req) {
  const desde = Math.max(0, parseInt(url.searchParams.get("desde") || "0", 10) || 0);
  const r = await env.DB.prepare(
    // `deAvatar` sai do JOIN e nao da linha de `rec`: a foto e de QUEM MANDOU,
    // nao da recomendacao, e copia-la para dentro de `rec` deixaria a TV
    // mostrando a foto velha de um amigo que trocou a dele.
    // COALESCE porque o LEFT JOIN nao acha ninguem quando a pessoa que mandou
    // foi apagada — a linha continua valida, so fica sem nome e sem foto.
    "SELECT r.id, r.de, COALESCE(p.nome, '') AS deNome, " +
    "COALESCE(p.avatar, '') AS deAvatar, r.criado, r.imdb, r.tipo, r.titulo, " +
    "r.poster, r.ano, r.modelo, r.texto, r.nota, r.visto, " +
    "r.terminou, r.reacao, r.resposta, r.respondido " +
    "FROM rec r LEFT JOIN pessoa p ON p.id = r.de " +
    "WHERE r.para = ? AND r.id > ? ORDER BY r.id DESC LIMIT 50"
  ).bind(quem.id, desde).all();
  const itens = r.results || [];
  // O ESTADO DE "ASSISTIDA"/RESPOSTA DE TODAS AS RECS, nao so das novas: `desde`
  // so entrega ids novos, e uma rec respondida em OUTRA TV tem id velho. Vai
  // sem o cartaz (a TV ja tem), limitado ao que o cliente guarda (120).
  const rs = await env.DB.prepare(
    "SELECT id, terminou, reacao, resposta, respondido FROM rec " +
    "WHERE para = ? AND (terminou = 1 OR respondido > 0 OR reacao IS NOT NULL) " +
    "ORDER BY id DESC LIMIT 120"
  ).bind(quem.id).all();
  const respostas = (rs.results || []).map((x) => ({
    id: x.id, terminou: x.terminou ? 1 : 0,
    reacao: x.reacao === null || x.reacao === undefined ? null : x.reacao,
    resposta: x.resposta || "", respondido: x.respondido || 0,
  }));
  const maiorId = itens.reduce((m, x) => (x.id > m ? x.id : m), desde);
  const naoVistas = itens.filter((x) => !x.visto).length;

  // ETag barato: no caso comum (nada novo) a TV gasta um 304 e nenhum corpo.
  // Ele conta ID e NAO VISTAS, e nao o conteudo: trocar a foto de perfil (ou o
  // nome) de quem mandou nao invalida o 304, entao a cara nova so aparece na
  // proxima recomendacao. E o preco combinado de uma sondagem por minuto por
  // TV, e nao um esquecimento.
  // O estado das respostas entra no ETag (quantas, soma dos instantes e das
  // reacoes): responder em outra TV tem de invalidar o 304 desta.
  const sig = respostas.reduce((a, x) => a + x.respondido + (x.reacao === null ? 0 : x.reacao + 2) + x.terminou, 0);
  // TRAVA DE SERIE (migracao 012): a revisao das travas desta pessoa pega
  // carona nesta sondagem. Mudou = a TV faz GET /v1/travas. Sem a tabela, 0.
  const tRev = await travaRev(env, quem.id);
  const etag = `"${quem.id.length}-${maiorId}-${naoVistas}-${respostas.length}.${sig}.${tRev}"`;
  if (req.headers.get("if-none-match") === etag) {
    // SEM CORS AQUI, o XHR da TV via `mode: "cors"` nunca via ESTE 304 —
    // via um erro de rede generico, porque a resposta sem
    // access-control-allow-origin e recusada pelo navegador antes de chegar
    // ao codigo que compara o status. A sondagem de 304 (o caso comum, "nada
    // novo") era exatamente o caminho sem CORS; so o 200 com corpo tinha.
    return new Response(null, { status: 304, headers: { etag, ...CORS } });
  }
  return json({ cursor: maiorId, novas: naoVistas, itens, respostas, travaRev: tRev }, 200, { etag });
}

async function rotaVisto(env, quem, corpo) {
  const ids = (Array.isArray(corpo?.ids) ? corpo.ids : [])
    .map((x) => parseInt(x, 10)).filter(Number.isInteger).slice(0, 100);
  if (!ids.length) return json({ ok: 1, n: 0 });
  const marcas = ids.map(() => "?").join(",");
  const r = await env.DB.prepare(
    `UPDATE rec SET visto = 1 WHERE para = ? AND id IN (${marcas})`
  ).bind(quem.id, ...ids).run();
  return json({ ok: 1, n: r.meta?.changes || 0 });
}

// REGISTRO DE UMA SESSAO QUE MORREU (avisos.h no cliente). So chega quando a
// pessoa aperta "Enviar registro": o app nunca manda sozinho. O texto ja vem
// sem credencial (rede_url_publica no cliente) e e cortado aqui em 200 KB de
// qualquer jeito; fica 30 dias e sai na limpeza diaria.
const REGISTRO_MAX = 200 * 1024;
// codigoRegistro (o codigo de seis caracteres do recibo): ver src/codigo.js.
const REGISTRO_RETENCAO = 30 * 24 * 3600;
async function rotaRegistro(env, quem, corpo) {
  const versao = String(corpo?.versao || "").slice(0, 32);
  const plataforma = String(corpo?.plataforma || "").slice(0, 16);
  const quando = String(corpo?.quando || "").slice(0, 40);
  let texto = String(corpo?.texto || "");
  if (texto.length > REGISTRO_MAX) texto = texto.slice(texto.length - REGISTRO_MAX);
  const res = await env.DB.prepare(
    "INSERT INTO registro (pessoa, versao, plataforma, quando, texto, criado) VALUES (?, ?, ?, ?, ?, ?)"
  ).bind(quem.id, versao, plataforma, quando, texto, agora()).run();
  // RECIBO: a TV (avisos_enviar_diagnostico -> extrairRegistroId) so da o
  // envio por concluido se o corpo trouxer o id da linha gravada; sem ele o
  // registro dizia "HTTP 200 (sem recibo desta execucao)" com o envio feito.
  // execucao_id volta ecoado para a TV casar o recibo com a execucao dela.
  const recibo = { ok: 1, bytes: texto.length, registro_id: res?.meta?.last_row_id ?? null };
  if (recibo.registro_id != null) recibo.codigo = codigoRegistro(recibo.registro_id);
  const exec = String(corpo?.execucao_id ?? "").replace(/[^\w-]/g, "").slice(0, 64);
  if (exec) recibo.execucao_id = exec;
  return json(recibo);
}

async function rotaApagar(env, quem, corpo) {
  const id = parseInt(corpo?.id, 10);
  if (!Number.isInteger(id)) return erro("sem id", 400);
  await env.DB.prepare("DELETE FROM rec WHERE id = ? AND para = ?").bind(id, quem.id).run();
  return json({ ok: 1 });
}

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    const rota = url.pathname;

    if (req.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });
    if (rota === "/v1/saude") return json({ ok: 1, t: agora() });

    // TV VIDAA: o site (/tv/*) e o proxy dela (/v1/proxy) moram em OUTRO
    // worker, nuvio-tv (servidor/tv). Estavam aqui, com [assets] no
    // wrangler.toml deste worker, e todo deploy feito de uma arvore sem o site
    // montado apagava a pagina da TV (24/09 e de 29/09 a 05/10, issue #135).
    // Aqui fica so o repasse, para o endereco antigo continuar valendo; sem o
    // binding (teste, deploy de arvore antiga) responde 404 e o site segue no
    // ar no endereco proprio dele.
    if (rota === "/tv" || rota.startsWith("/tv/") || rota === "/v1/proxy") {
      if (!env.TV) return erro("site da tv em outro endereco", 404);
      return env.TV.fetch(req);
    }

    // BUILD DE DIAGNOSTICO (#77, 20/09/2026): uma TV que nao chega nem ao
    // login nao tem sessao nem Trakt para assinar o envio, e o dono pediu uma
    // build que manda o registro sozinha. Ela vem com o token DIAG_TOKEN
    // (segredo do worker) e so pode fazer ISTO: gravar registro sob a pessoa
    // "diag:<marca da TV>". Nenhuma outra rota aceita esse token.
    if (rota === "/v1/registro" && req.method === "POST" &&
        (req.headers.get("x-nuvio-auth") || "").toLowerCase() === "diagnostico") {
      const aut = req.headers.get("authorization") || "";
      const token = aut.startsWith("Bearer ") ? aut.slice(7).trim() : "";
      if (!env.DIAG_TOKEN || !token || token !== env.DIAG_TOKEN) return erro("nao autenticado", 401);
      let corpo = {};
      try { corpo = JSON.parse((await req.text()).trim() || "{}"); } catch { return erro("json invalido", 400); }
      const tv = String(corpo?.tv || "?").replace(/[^\w.:-]/g, "").slice(0, 64);
      return rotaRegistro(env, { id: "diag:" + tv }, corpo);
    }

    // PROXY DO XTREAM PARA A SAMSUNG (#112): sem sessao, como /v1/noticias,
    // porque quem autentica e o painel do Xtream com a credencial que passa
    // na url. Regras (allowlist, SSRF, sem log nem cache de credencial) e o
    // porque da rota em xtream.js.
    // O preflight (OPTIONS) ja e respondido acima com CORS * e content-type
    // permitido; o cliente manda text/plain, que nem pede preflight.
    if (rota === "/v1/xtream" && (req.method === "POST" || req.method === "GET"))
      return rotaXtream(req, url);

    // TRAILER NA SAMSUNG (#136): a pergunta ao IMDb (que exige Referer) e a
    // pagina que embute o YouTube com origem valida. Regras em trailer.js.
    if (rota === "/v1/trailer/imdb" && req.method === "GET") return rotaTrailerImdb(url, fetch, caches.default);
    if (rota === "/v1/trailer/yt" && req.method === "GET") return rotaTrailerYoutube(url);

    // NOTICIA COMPLETA (modal de noticia da Agenda na Samsung): o link do RSS
    // -> pagina do veiculo -> texto e capa; e o repasse da capa (so image/*).
    // Sem sessao, como /v1/noticias. Limites (SSRF, tetos, prazo, cache de
    // 1 h) em noticia.js.
    if (rota === "/v1/noticia" && req.method === "GET") return rotaNoticia(url, fetch, caches.default);
    if (rota === "/v1/noticia/img" && req.method === "GET") return rotaNoticiaImg(url, fetch, caches.default);

    // NOTICIAS DE UM TITULO (Agenda, 1.3.11). O RSS de busca do Google News
    // nao manda CORS, e na Samsung (wgt em file://) o fetch morre antes de
    // sair — 12 de 12 "rede falhou" no registro de 21/09. Este worker so
    // repassa o XML com CORS, sem chave e sem sessao: a consulta e um titulo
    // de serie, nada da pessoa. Cache de 1 h na borda; consulta limitada a
    // 200 caracteres e so os quatro parametros que o cliente usa.
    if (rota === "/v1/noticias" && req.method === "GET") {
      const q = (url.searchParams.get("q") || "").slice(0, 200);
      if (!q) return erro("sem consulta", 400);
      const lp = (k, padrao) => (url.searchParams.get(k) || padrao).replace(/[^\w:-]/g, "").slice(0, 12);
      const alvo = "https://news.google.com/rss/search?q=" + encodeURIComponent(q) +
        "&hl=" + lp("hl", "pt-BR") + "&gl=" + lp("gl", "BR") + "&ceid=" + lp("ceid", "BR:pt-419");
      const cache = caches.default;
      const chave = new Request(alvo);
      let r = await cache.match(chave);
      if (!r) {
        // O Google devolve 503 a rede da Cloudflare (medido no deploy de
        // 21/09). Tenta com UA de navegador; se recusar, o Bing News tem o
        // mesmo RSS (title/pubDate; a fonte vem como <News:Source>, que aqui
        // vira <source> para o cliente ler um formato so).
        const UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0 Safari/537.36";
        let xml = null;
        try {
          const up = await fetch(alvo, { headers: { "user-agent": UA, "accept": "application/rss+xml,text/xml;q=0.9,*/*;q=0.8" } });
          if (up.ok) xml = await up.text();
        } catch {}
        if (!xml) {
          const lang = lp("hl", "pt-BR"), cc = lp("gl", "BR");
          const bing = "https://www.bing.com/news/search?q=" + encodeURIComponent(q) + "&format=rss&setlang=" + lang + "&cc=" + cc;
          const up2 = await fetch(bing, { headers: { "user-agent": UA } });
          if (!up2.ok) return erro("noticias indisponiveis (" + up2.status + ")", 502);
          xml = (await up2.text()).replace(/<News:Source>/g, "<source>").replace(/<\/News:Source>/g, "</source>");
        }
        r = new Response(xml, { status: 200, headers: {
          "content-type": "application/rss+xml; charset=utf-8", "cache-control": "public, max-age=3600" } });
        await cache.put(chave, r.clone());
      }
      return new Response(r.body, { status: 200, headers: {
        "content-type": "application/rss+xml; charset=utf-8", "cache-control": "public, max-age=3600", ...CORS } });
    }

    const quemToken = await quemE(req, env);
    if (!quemToken) return erro("nao autenticado", 401);
    // IDENTIDADE CANONICA (migracao 008): um Trakt ligado a um perfil Nuvio fala
    // como o perfil. Sem vinculo, nada muda.
    const quemBruto = await resolverCanonica(env, quemToken);

    // Corpo VAZIO e legitimo: `/v1/eu` nao tem nada a dizer alem de quem manda,
    // e o cliente em C nao vai montar um "{}" so para agradar o parser.
    let corpo = {};
    if (req.method === "POST") {
      const cru = (await req.text()).trim();
      if (cru) {
        try { corpo = JSON.parse(cru); } catch { return erro("json invalido", 400); }
      }
    }

    // `/v1/eu` tambem e o registro: a primeira chamada de uma TV cria a pessoa.
    // O corpo pode trazer {"nome","avatar"} do PERFIL ativo; corpo vazio (cliente
    // antigo) nao mexe neles.
    if (rota === "/v1/eu" && req.method === "POST") {
      const extra = {};
      if (typeof corpo?.nome === "string") extra.nome = corpo.nome;
      if (typeof corpo?.avatar === "string") extra.avatar = corpo.avatar;
      const eu = await registrar(env, quemBruto, Object.keys(extra).length ? extra : undefined);
      // `recursos` e a deteccao de recurso do cliente: TV nova so oferece unir
      // contas e so espera `ids` nos contatos quando o servidor diz que sabe.
      return json({ ...eu, recursos: [RECURSO], autenticado: quemToken.id.startsWith("trakt:") ? "trakt" : "nuvio",
                    identidades: await identidadesDe(env, eu.id) });
    }

    const quem = { ...(await registrar(env, quemBruto)), bruto: quemBruto.bruto || quemBruto.id };
    const h = { json, erro, agora, limparTexto, limpar };
    // Ids antigos (de antes de uma fusao) que TVs guardaram viram o id vivo.
    await canonizarEntrada(env, corpo, url);

    const ident = await rotaIdentidades(rota, req.method, env, quem, corpo, h,
      { idTrakt, idNuvio, idSimkl, perfilExiste, registrar });
    if (ident) return ident;

    if (rota === "/v1/eu/nome" && req.method === "POST") return rotaEuNome(env, quem, corpo, h, () => registrar(env, quemBruto));
    if (rota === "/v1/alcance" && req.method === "POST") return rotaAlcance(env, quem, corpo, h);
    // MESMA ROTA, DOIS CONTRATOS: com "ev" e o evento do player (social.js); sem
    // ele e o corpo antigo de amigos.js, que TVs ja no ar continuam mandando.
    if (rota === "/v1/atividade" && req.method === "POST" && typeof corpo?.ev === "string")
      return rotaEvento(env, quem, corpo, h, limitar);
    if (rota === "/v1/feed" && req.method === "GET") return rotaFeed(env, quem, url, req, h, garantirPerfil);
    if (rota === "/v1/amigo" && req.method === "GET") return rotaAmigo(env, quem, url, h, garantirPerfil);

    if (rota === "/v1/contatos" && req.method === "GET")  return rotaContatosLer(env, quem);
    if (rota === "/v1/contatos" && req.method === "POST") return rotaContatosVincular(env, quem, corpo);
    if (rota === "/v1/contatos/trakt" && req.method === "POST") return rotaContatosTrakt(env, quem, corpo);
    if (rota === "/v1/contatos/remover" && req.method === "POST") return rotaContatoRemover(env, quem, corpo);
    if (rota === "/v1/contatos/sugerido" && req.method === "POST") return rotaContatoSugerido(env, quem, corpo);
    if (rota === "/v1/sugestoes" && req.method === "POST") return rotaSugestoes(env, quem, corpo);
    if (rota === "/v1/descobrivel" && req.method === "POST") return rotaDescobrivel(env, quem, corpo);
    if (rota === "/v1/rec" && req.method === "POST")      return rotaEnviar(env, quem, corpo);
    if (rota === "/v1/rec" && req.method === "GET")       return rotaReceber(env, quem, url, req);
    if (rota === "/v1/rec/visto" && req.method === "POST") return rotaVisto(env, quem, corpo);
    if (rota === "/v1/rec/apagar" && req.method === "POST") return rotaApagar(env, quem, corpo);
    // Resposta direta a uma rec recebida (social.js; exige migracao-007).
    if (rota === "/v1/rec/resposta" && req.method === "POST")
      return rotaRecResposta(env, quem, corpo, h, limitar, limparTexto);
    if (rota === "/v1/registro" && req.method === "POST")   return rotaRegistro(env, quem, corpo);

    // Enquete na ilha (enquete.js; exige migracao-009).
    const enq = await rotaEnquete(rota, req.method, env, quem, corpo, h);
    if (enq) return enq;

    // Trava de serie (trava.js; exige migracao-012).
    const trv = await rotaTrava(rota, req.method, env, quem, corpo, h, req, { limitar, mesmaConta });
    if (trv) return trv;

    // Perfil publico, busca, pedidos, bloqueio e atividade (amigos.js).
    const amigos = await rotaAmigos(rota, req.method, env, quem, corpo, h);
    if (amigos) return amigos;

    return erro("rota desconhecida", 404);
  },

  async scheduled(_evt, env) {
    const t = agora();
    await env.DB.batch([
      env.DB.prepare("DELETE FROM rec WHERE criado < ?").bind(t - RETENCAO),
      env.DB.prepare("DELETE FROM sessao WHERE expira < ?").bind(t),
      env.DB.prepare("DELETE FROM registro WHERE criado < ?").bind(t - REGISTRO_RETENCAO),
      ...limpezaAmigos(env, t),
      ...limpezaSocial(env, t),
    ]);
  },
};
