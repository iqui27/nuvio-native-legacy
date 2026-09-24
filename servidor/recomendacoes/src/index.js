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
import { rotaProxy } from "./proxy.js";

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
  "access-control-allow-headers": "authorization, content-type, x-nuvio-auth, if-none-match",
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
  if (cache) return { id: cache.id, nome: cache.nome, avatar: "" };

  const quem = via === "trakt" ? await idTrakt(token, env) : await idNuvio(token, env);
  if (!quem) return null;
  await env.DB.prepare(
    "INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, ?) " +
    "ON CONFLICT(hash) DO UPDATE SET id = excluded.id, nome = excluded.nome, expira = excluded.expira"
  ).bind(hash, quem.id, quem.nome, t + SESSAO_TTL).run();
  return quem;
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
async function registrar(env, quem) {
  const t = agora();
  // `descobrivel` SAI DAQUI E NAO ENTRA. Esta funcao roda em TODA requisicao
  // autenticada: se ela escrevesse a coluna, qualquer sondagem de 60 s poderia
  // desfazer a escolha da pessoa por omissao do cliente. Quem escreve e so
  // `/v1/descobrivel`, que existe para isso e nao faz mais nada.
  const ja = await env.DB.prepare(
    "SELECT id, nome, codigo, avatar, descobrivel FROM pessoa WHERE id = ?"
  ).bind(quem.id).first();
  if (ja) {
    let codigo = ja.codigo;
    if (!codigo) {
      codigo = await codigoLivre(env.DB);
      await env.DB.prepare("UPDATE pessoa SET codigo = ? WHERE id = ?").bind(codigo, quem.id).run();
    }
    const nome = quem.nome || ja.nome;
    // MESMA REGRA DO NOME: o que veio vazio nao apaga o que estava guardado.
    // Uma verificacao servida pelo cache da sessao chega sem foto, e uma pessoa
    // que troca de foto no Trakt tem a nova na proxima verificacao de verdade.
    const avatar = quem.avatar || ja.avatar || "";
    await env.DB.prepare("UPDATE pessoa SET nome = ?, avatar = ?, visto = ? WHERE id = ?")
      .bind(nome, avatar, t, quem.id).run();
    return { id: quem.id, nome, codigo, avatar, descobrivel: ja.descobrivel ? 1 : 0 };
  }
  const codigo = await codigoLivre(env.DB);
  const avatar = quem.avatar || "";
  // A COLUNA `descobrivel` NAO APARECE NO INSERT de proposito: o DEFAULT 0 do
  // esquema e quem responde, e escreve-la aqui seria dar ao codigo a chance de
  // um dia inserir 1 sem ninguem ter respondido a pergunta.
  await env.DB.prepare(
    "INSERT INTO pessoa (id, nome, codigo, avatar, criado, visto) VALUES (?, ?, ?, ?, ?, ?)"
  ).bind(quem.id, quem.nome, codigo, avatar, t, t).run();
  return { id: quem.id, nome: quem.nome, codigo, avatar, descobrivel: 0 };
}

const saoContatos = (db, a, b) =>
  db.prepare("SELECT 1 FROM contato WHERE a = ? AND b = ?").bind(a, b).first();

// --- Rotas -------------------------------------------------------------------

async function rotaContatosLer(env, quem) {
  const r = await env.DB.prepare(
    "SELECT p.id AS id, p.nome AS nome, p.avatar AS avatar FROM contato c " +
    "JOIN pessoa p ON p.id = c.b WHERE c.a = ? ORDER BY p.nome"
  ).bind(quem.id).all();
  return json({
    contatos: (r.results || []).map((x) => ({
      id: x.id,
      nome: x.nome,
      avatar: x.avatar || "",
      origem: x.id.startsWith("trakt:") ? "trakt" : "nuvio",
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
  const t = agora();
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado) VALUES (?, ?, ?)")
      .bind(quem.id, outro.id, t),
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado) VALUES (?, ?, ?)")
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
  if (!quem.id.startsWith("trakt:")) return erro("so para conta trakt", 400);
  const slugs = Array.isArray(corpo?.slugs) ? corpo.slugs.slice(0, 200) : [];
  const ids = slugs.map((s) => `trakt:${limparTexto(s).replace(/ /g, "-")}`)
    .filter((s) => s.length > 6 && s !== quem.id);
  if (!ids.length) return json({ vinculados: 0 });
  const marcas = ids.map(() => "?").join(",");
  const r = await env.DB.prepare(`SELECT id FROM pessoa WHERE id IN (${marcas})`).bind(...ids).all();
  const t = agora();
  const cmds = [];
  for (const x of r.results || []) {
    cmds.push(env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado) VALUES (?, ?, ?)")
      .bind(quem.id, x.id, t));
    cmds.push(env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado) VALUES (?, ?, ?)")
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
  await env.DB.prepare("UPDATE pessoa SET descobrivel = ? WHERE id = ?")
    .bind(v, quem.id).run();
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

  const slugs = Array.isArray(corpo?.slugs) ? corpo.slugs.slice(0, 200) : [];
  const ids = [...new Set(
    slugs.map((s) => `trakt:${limparTexto(s).replace(/ /g, "-")}`)
         .filter((s) => s.length > 6 && s !== quem.id)
  )];
  if (ids.length) {
    const marcas = ids.map(() => "?").join(",");
    const r = await env.DB.prepare(
      `SELECT p.id AS id, p.nome AS nome, p.avatar AS avatar FROM pessoa p ` +
      `WHERE p.id IN (${marcas}) AND p.descobrivel = 1 ` +
      // JA E CONTATO NAO E SUGESTAO. Sem este NOT EXISTS a aba abriria pedindo
      // para adicionar quem ja esta na lista de contatos logo acima.
      `AND NOT EXISTS (SELECT 1 FROM contato c WHERE c.a = ? AND c.b = p.id) ` +
      `ORDER BY p.nome LIMIT ?`
    ).bind(...ids, quem.id, SUG_MAX).all();
    for (const x of r.results || []) {
      if (vistos.has(x.id)) continue;
      vistos.add(x.id);
      saida.push({ id: x.id, nome: x.nome || "", avatar: x.avatar || "",
                   origem: "trakt", viaNome: "" });
    }
  }

  const r2 = await env.DB.prepare(
    `SELECT p.id AS id, p.nome AS nome, p.avatar AS avatar, ` +
    // O NOME DO INTERMEDIARIO, e so ele: e o que a linha da TV mostra ("amigo
    // de Gustavo"). MIN() porque o GROUP BY colapsa varios caminhos ate a mesma
    // pessoa num so, e mostrar "amigo de Gustavo, Marina e mais 3" contaria a
    // quem recebe quantos contatos em comum existem — que e informacao sobre os
    // OUTROS dois, nao sobre ela. COALESCE/NULLIF porque quem nunca preencheu o
    // nome no Trakt tem `nome` vazio e "amigo de" sozinho nao diz nada.
    `MIN(COALESCE(NULLIF(v.nome, ''), v.id)) AS viaNome ` +
    `FROM contato c1 ` +
    `JOIN contato c2 ON c2.a = c1.b ` +
    `JOIN pessoa  p  ON p.id = c2.b ` +
    `JOIN pessoa  v  ON v.id = c1.b ` +
    `WHERE c1.a = ? AND c2.b <> ? AND p.descobrivel = 1 ` +
    `AND NOT EXISTS (SELECT 1 FROM contato x WHERE x.a = ? AND x.b = p.id) ` +
    `GROUP BY p.id, p.nome, p.avatar ORDER BY p.nome LIMIT ?`
  ).bind(quem.id, quem.id, quem.id, SUG_MAX).all();
  for (const x of r2.results || []) {
    if (vistos.has(x.id)) continue;
    if (saida.length >= SUG_MAX) break;
    vistos.add(x.id);
    saida.push({ id: x.id, nome: x.nome || "", avatar: x.avatar || "",
                 origem: "amigo", viaNome: x.viaNome || "" });
  }
  return saida.slice(0, SUG_MAX);
}

const rotaSugestoes = async (env, quem, corpo) =>
  json({ sugestoes: await sugestoesDe(env, quem, corpo) });

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
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado) VALUES (?, ?, ?)")
      .bind(quem.id, alvo, t),
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado) VALUES (?, ?, ?)")
      .bind(alvo, quem.id, t),
  ]);
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

async function rotaReceber(env, quem, url, req) {
  const desde = Math.max(0, parseInt(url.searchParams.get("desde") || "0", 10) || 0);
  const r = await env.DB.prepare(
    // `deAvatar` sai do JOIN e nao da linha de `rec`: a foto e de QUEM MANDOU,
    // nao da recomendacao, e copia-la para dentro de `rec` deixaria a TV
    // mostrando a foto velha de um amigo que trocou a dele.
    // COALESCE porque o LEFT JOIN nao acha ninguem quando a pessoa que mandou
    // foi apagada — a linha continua valida, so fica sem nome e sem foto.
    "SELECT r.id, r.de, COALESCE(p.nome, '') AS deNome, " +
    "COALESCE(p.avatar, '') AS deAvatar, r.criado, r.imdb, r.tipo, r.titulo, " +
    "r.poster, r.ano, r.modelo, r.texto, r.nota, r.visto " +
    "FROM rec r LEFT JOIN pessoa p ON p.id = r.de " +
    "WHERE r.para = ? AND r.id > ? ORDER BY r.id DESC LIMIT 50"
  ).bind(quem.id, desde).all();
  const itens = r.results || [];
  const maiorId = itens.reduce((m, x) => (x.id > m ? x.id : m), desde);
  const naoVistas = itens.filter((x) => !x.visto).length;

  // ETag barato: no caso comum (nada novo) a TV gasta um 304 e nenhum corpo.
  // Ele conta ID e NAO VISTAS, e nao o conteudo: trocar a foto de perfil (ou o
  // nome) de quem mandou nao invalida o 304, entao a cara nova so aparece na
  // proxima recomendacao. E o preco combinado de uma sondagem por minuto por
  // TV, e nao um esquecimento.
  const etag = `"${quem.id.length}-${maiorId}-${naoVistas}"`;
  if (req.headers.get("if-none-match") === etag) {
    // SEM CORS AQUI, o XHR da TV via `mode: "cors"` nunca via ESTE 304 —
    // via um erro de rede generico, porque a resposta sem
    // access-control-allow-origin e recusada pelo navegador antes de chegar
    // ao codigo que compara o status. A sondagem de 304 (o caso comum, "nada
    // novo") era exatamente o caminho sem CORS; so o 200 com corpo tinha.
    return new Response(null, { status: 304, headers: { etag, ...CORS } });
  }
  return json({ cursor: maiorId, novas: naoVistas, itens }, 200, { etag });
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

// --- Hospedagem estatica da TV VIDAA -----------------------------------------
//
// build/vidaa-site/ e um site gerado por outro script (tools/vidaa-site.sh),
// nao por este worker: tv/versao.txt com a versao corrente e
// tv/<versao>/{mt,st}/{index.html,index.js,index.wasm,index.data,
// decodificador.js,hls.min.js}, mais tv/icone-*.png. O worker so serve isto
// (wrangler.toml: [assets] + run_worker_first = ["/tv/*"]) porque tres coisas
// exigem codigo no meio do caminho: resolver /tv para a versao atual sem o
// cliente saber qual e, ligar COOP/COEP so no mt/ (pthreads exige isolamento
// cross-origin; SharedArrayBuffer nao existe sem isso) e por cache longo nos
// arquivos versionados. O binding falta em ambiente de teste sem `[assets]`
// configurado (ex.: teste-xtream.mjs chamando rotaXtream direto) — por isso
// toda funcao aqui comeca conferindo `env.ASSETS`.
let versaoCache = null, versaoCacheAte = 0;
async function versaoAtual(env) {
  const t = Date.now();
  if (versaoCache && t < versaoCacheAte) return versaoCache;
  const r = await env.ASSETS.fetch(new Request("https://tv.interna/tv/versao.txt"));
  if (!r.ok) return null;
  const v = (await r.text()).trim();
  if (!v) return null;
  versaoCache = v;
  versaoCacheAte = t + 60000;   // 60s: o mesmo isolate nao bate no ASSETS a cada pedido de /tv
  return v;
}

// Extensao -> content-type. SO as que build/vidaa-site produz; o resto sai
// como o Asset Worker ja serviu (ele acerta html/js/png sozinho pela mesma
// tabela de mimes do navegador — o que ele NAO acerta e o que este mapa
// cobre).
const TIPOS_TV = {
  ".wasm": "application/wasm",
  ".data": "application/octet-stream",
};
function tipoTvDe(caminho) {
  const i = caminho.lastIndexOf(".");
  return i < 0 ? null : TIPOS_TV[caminho.slice(i)] || null;
}

async function rotaTv(req, url, env) {
  if (!env.ASSETS) return erro("hospedagem da tv indisponivel", 404);
  const rota = url.pathname;

  // /tv e /tv/ -> a versao atual, sempre mt/ primeiro (a shell la dentro pula
  // sozinha para ../st/ quando falta SharedArrayBuffer — ver tools/tizen.sh).
  if (rota === "/tv" || rota === "/tv/") {
    const v = await versaoAtual(env);
    if (!v) return erro("versao indisponivel", 404);
    return Response.redirect(new URL(`/tv/${v}/mt/`, url).toString(), 302);
  }

  if (/^\/tv\/icone-\d+\.png$/.test(rota)) {
    const r = await env.ASSETS.fetch(req);
    if (!r.ok) return r;
    const h = new Headers(r.headers);
    h.set("cache-control", "public, max-age=31536000, immutable");
    return new Response(r.body, { status: r.status, headers: h });
  }

  // /tv/versao.txt DIRETO (nao so a leitura interna de versaoAtual): quem
  // testar uma TV manda esta URL para conferir qual versao esta no ar sem
  // seguir o redirect inteiro. Achado testando o deploy real (24/09): a
  // regex de baixo exige /mt//st, entao este caminho caia em 404 mesmo
  // com o arquivo publicado e o redirect de /tv/ funcionando (ele le por
  // fetch interno, que nao passa por aqui).
  if (rota === "/tv/versao.txt") {
    const r = await env.ASSETS.fetch(req);
    if (!r.ok) return r;
    const h = new Headers(r.headers);
    h.set("cache-control", "no-cache");
    return new Response(r.body, { status: r.status, headers: h });
  }

  const m = /^\/tv\/([^/]+)\/(mt|st)\/(.*)$/.exec(rota);
  if (!m) return erro("rota da tv desconhecida", 404);
  const modo = m[2], resto = m[3];

  const r = await env.ASSETS.fetch(req);
  if (!r.ok) return r;
  const h = new Headers(r.headers);

  const tipo = tipoTvDe(resto);
  if (tipo) h.set("content-type", tipo);

  // versao.txt (fora de /tv/<v>/) ja e tratado acima; aqui dentro so o
  // index.html do bundle pode mudar sem trocar de caminho (a pessoa fica na
  // MESMA versao enquanto uma TV vieja ainda a usa). Tudo o mais no caminho
  // versionado e imutavel: o nome do arquivo so muda quando o conteudo muda.
  const ehIndex = resto === "" || resto === "index.html";
  h.set("cache-control", ehIndex ? "no-cache" : "public, max-age=31536000, immutable");

  // COOP/COEP SO NO mt/: e o modo com pthreads, que so arranca com
  // SharedArrayBuffer, que so existe com a origem isolada. O st/ compila sem
  // -pthread (fio1.c cooperativo) e nao precisa disto — e exigir credentialless
  // ali quebraria, sem motivo, qualquer sub-recurso que o st/ venha a buscar
  // sem CORP/CORS proprio.
  if (modo === "mt") {
    h.set("cross-origin-opener-policy", "same-origin");
    h.set("cross-origin-embedder-policy", "credentialless");
    h.set("cross-origin-resource-policy", "same-origin");
  }

  return new Response(r.body, { status: r.status, headers: h });
}

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    const rota = url.pathname;

    if (req.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });
    if (rota === "/v1/saude") return json({ ok: 1, t: agora() });

    // SITE ESTATICO DA TV VIDAA. Sem sessao, sem D1: e so o Asset Worker com
    // cabecalhos por cima (ver rotaTv). Fica antes de tudo porque nao e /v1 e
    // nao deve competir com nenhuma rota da API.
    if (rota === "/tv" || rota === "/tv/" || rota.startsWith("/tv/"))
      return rotaTv(req, url, env);

    // PROXY GENERICO PARA A VIDAA (#125): sem sessao, como /v1/xtream e
    // /v1/noticias — a pagina inteira e https e qualquer chamada http:// do
    // cliente (nao so o painel Xtream) precisa deste desvio. Ver proxy.js.
    if (rota === "/v1/proxy" && req.method === "GET")
      return rotaProxy(req, url, env);

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

    const quemBruto = await quemE(req, env);
    if (!quemBruto) return erro("nao autenticado", 401);

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
    if (rota === "/v1/eu" && req.method === "POST") return json(await registrar(env, quemBruto));

    const quem = await registrar(env, quemBruto);

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
    if (rota === "/v1/registro" && req.method === "POST")   return rotaRegistro(env, quem, corpo);

    return erro("rota desconhecida", 404);
  },

  async scheduled(_evt, env) {
    const t = agora();
    await env.DB.batch([
      env.DB.prepare("DELETE FROM rec WHERE criado < ?").bind(t - RETENCAO),
      env.DB.prepare("DELETE FROM sessao WHERE expira < ?").bind(t),
      env.DB.prepare("DELETE FROM registro WHERE criado < ?").bind(t - REGISTRO_RETENCAO),
    ]);
  },
};
