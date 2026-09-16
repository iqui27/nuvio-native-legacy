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

async function idTrakt(token, env) {
  const r = await fetch("https://api.trakt.tv/users/settings", {
    headers: {
      authorization: `Bearer ${token}`,
      "trakt-api-version": "2",
      "trakt-api-key": env.TRAKT_CLIENT_ID,
    },
  });
  if (!r.ok) return null;
  const d = await r.json();
  const slug = d?.user?.ids?.slug || d?.user?.username;
  if (!slug) return null;
  return { id: `trakt:${slug}`, nome: limpar(d?.user?.name || d?.user?.username || slug, 64) };
}

async function idNuvio(token, env) {
  const r = await fetch(`${env.SUPABASE_URL}/auth/v1/user`, {
    headers: { apikey: env.SUPABASE_ANON_KEY, authorization: `Bearer ${token}` },
  });
  if (!r.ok) return null;
  const d = await r.json();
  if (!d?.id) return null;
  const nome = d?.user_metadata?.name || d?.user_metadata?.full_name || "";
  return { id: `nuvio:${d.id}`, nome: limpar(nome, 64) };
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
  if (cache) return { id: cache.id, nome: cache.nome };

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
  const ja = await env.DB.prepare("SELECT id, nome, codigo FROM pessoa WHERE id = ?")
    .bind(quem.id).first();
  if (ja) {
    let codigo = ja.codigo;
    if (!codigo) {
      codigo = await codigoLivre(env.DB);
      await env.DB.prepare("UPDATE pessoa SET codigo = ? WHERE id = ?").bind(codigo, quem.id).run();
    }
    const nome = quem.nome || ja.nome;
    await env.DB.prepare("UPDATE pessoa SET nome = ?, visto = ? WHERE id = ?")
      .bind(nome, t, quem.id).run();
    return { id: quem.id, nome, codigo };
  }
  const codigo = await codigoLivre(env.DB);
  await env.DB.prepare(
    "INSERT INTO pessoa (id, nome, codigo, criado, visto) VALUES (?, ?, ?, ?, ?)"
  ).bind(quem.id, quem.nome, codigo, t, t).run();
  return { id: quem.id, nome: quem.nome, codigo };
}

const saoContatos = (db, a, b) =>
  db.prepare("SELECT 1 FROM contato WHERE a = ? AND b = ?").bind(a, b).first();

// --- Rotas -------------------------------------------------------------------

async function rotaContatosLer(env, quem) {
  const r = await env.DB.prepare(
    "SELECT p.id AS id, p.nome AS nome FROM contato c JOIN pessoa p ON p.id = c.b " +
    "WHERE c.a = ? ORDER BY p.nome"
  ).bind(quem.id).all();
  return json({
    contatos: (r.results || []).map((x) => ({
      id: x.id,
      nome: x.nome,
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

  const r = await env.DB.prepare(
    "INSERT INTO rec (de, para, criado, imdb, tipo, titulo, poster, ano, modelo, texto) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
  ).bind(
    quem.id, para, t, imdb,
    limpar(corpo?.tipo, 8) || "movie",
    limpar(corpo?.titulo, 160),
    limpar(corpo?.poster, 512),
    limpar(corpo?.ano, 16),
    modelo, texto
  ).run();
  return json({ ok: 1, id: r.meta?.last_row_id || 0 });
}

async function rotaReceber(env, quem, url, req) {
  const desde = Math.max(0, parseInt(url.searchParams.get("desde") || "0", 10) || 0);
  const r = await env.DB.prepare(
    "SELECT r.id, r.de, p.nome AS deNome, r.criado, r.imdb, r.tipo, r.titulo, r.poster, " +
    "r.ano, r.modelo, r.texto, r.visto FROM rec r LEFT JOIN pessoa p ON p.id = r.de " +
    "WHERE r.para = ? AND r.id > ? ORDER BY r.id DESC LIMIT 50"
  ).bind(quem.id, desde).all();
  const itens = r.results || [];
  const maiorId = itens.reduce((m, x) => (x.id > m ? x.id : m), desde);
  const naoVistas = itens.filter((x) => !x.visto).length;

  // ETag barato: no caso comum (nada novo) a TV gasta um 304 e nenhum corpo.
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
