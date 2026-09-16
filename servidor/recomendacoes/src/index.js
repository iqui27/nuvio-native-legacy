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

function json(dados, status = 200, extra = {}) {
  return new Response(JSON.stringify(dados), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", ...extra },
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
    return new Response(null, { status: 304, headers: { etag } });
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

    if (rota === "/v1/saude") return json({ ok: 1, t: agora() });

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

    return erro("rota desconhecida", 404);
  },

  async scheduled(_evt, env) {
    const t = agora();
    await env.DB.batch([
      env.DB.prepare("DELETE FROM rec WHERE criado < ?").bind(t - RETENCAO),
      env.DB.prepare("DELETE FROM sessao WHERE expira < ?").bind(t),
    ]);
  },
};
