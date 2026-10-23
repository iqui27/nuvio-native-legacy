// WATCH TOGETHER ("Assistir Juntos") — a parte deste Worker: quem pode criar,
// convidar e entrar, e o BILHETE assinado que o nuvio-juntos confere. A sala em
// si (relogio, estados, chat) vive no Durable Object do nuvio-juntos, que nao
// tem banco. Plano: docs/plans/watch-together/assistir-juntos-plano.md, 7.2.
// Tabelas: migracao-010-juntos.sql.
//
//   POST /v1/sala               cria; convida amigos -> {id, codigo, bilhete, ws, web}
//   GET  /v1/sala/convites      convites pendentes para mim (salas vivas)
//   POST /v1/sala/entrar        {id} | {codigo} -> {id, bilhete, ws, resumo}
//   POST /v1/sala/recusar       {id}
//   POST /v1/sala/fim           {id} anfitriao: a sala acabou (melhor esforco)
//   POST /v1/sala/silenciar     {de|"*", sim} "nao receber convites de X"
//
// PERFIL INFANTIL (decisao 7): a TV manda `infantil: 1` (corpo, ou ?infantil=1
// no GET), porque o controle parental mora na TV — a mesma confianca no
// aparelho de hoje. Com ele: convite e entrada so de quem a conta conhece
// (outro perfil da mesma conta, ou amigo confirmado de um perfil ADULTO dela).
//
// PRIVACIDADE: a impressao do release (hash, nome do arquivo, tamanho) fica na
// linha da sala para o convite mostrar o veredito da fonte ANTES de entrar, e
// some com a sala. Nunca URL, chave de addon ou link de debrid.
import { assinar, nonce } from "../../juntos/src/bilhete.js";
import { limparImpressao } from "../../juntos/src/logica.js";

const SALA_TTL = 12 * 3600;        // o mesmo teto do Durable Object
const BILHETE_TV_S = 60;            // uso unico; reconectar pede outro
const CONVIDADOS_MAX = 10;
const COD_ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // o de codigo.js
const contaDe = (id) => String(id || "").replace(/^(nuvio:[^:]+):\d{1,2}$/, "$1");

function codigoNovo() {
  const b = crypto.getRandomValues(new Uint8Array(6));
  return [...b].map((x) => COD_ALFA[x & 31]).join("");
}
function idNovo() {
  return [...crypto.getRandomValues(new Uint8Array(16))].map((x) => x.toString(16).padStart(2, "0")).join("");
}
// "K7M-2QX", "k7m2qx", "K7M 2QX" -> "K7M2QX"; I/L viram 1, O vira 0 (Crockford).
export function codigoLimpo(s) {
  const c = String(s || "").toUpperCase().replace(/[\s-]/g, "").replace(/[IL]/g, "1").replace(/O/g, "0");
  return /^[0-9A-HJKMNP-TV-Z]{6}$/.test(c) ? c : "";
}

const juntosBase = (env) => String(env.JUNTOS_URL || "").replace(/\/+$/, "");
function enderecos(env) {
  const base = juntosBase(env);
  // O QR do celular (web + sala + codigo) e montado pela TV com o codigo que o
  // Durable Object da: endereco curto, porque o qr.c das TVs cabe ~134 bytes.
  return { ws: base.replace(/^http/, "ws") + "/ws", web: base + "/j/" };
}

async function marcarInfantil(env, quem, inf, t) {
  if (inf) await env.DB.prepare("INSERT INTO sala_infantil (pessoa, visto) VALUES (?, ?) " +
    "ON CONFLICT(pessoa) DO UPDATE SET visto = excluded.visto").bind(quem.id, t).run();
  else await env.DB.prepare("DELETE FROM sala_infantil WHERE pessoa = ?").bind(quem.id).run();
}

// `outro` e conhecido da CONTA de `eu`: perfil da mesma conta, ou amigo
// confirmado de um perfil adulto (sem marca de infantil) da mesma conta.
async function conhecido(env, eu, outro) {
  if (contaDe(eu) === contaDe(outro)) return true;
  const c = contaDe(eu);
  const r = await env.DB.prepare(
    "SELECT 1 FROM contato k WHERE k.b = ? AND (k.a = ? OR k.a GLOB ?) " +
    "AND NOT EXISTS (SELECT 1 FROM sala_infantil i WHERE i.pessoa = k.a) LIMIT 1"
  ).bind(outro, c, c + ":*").first();
  return !!r;
}

const bloqueado = (env, a, b) => env.DB.prepare(
  "SELECT 1 FROM bloqueio WHERE (quem = ? AND alvo = ?) OR (quem = ? AND alvo = ?)").bind(a, b, b, a).first();
const amigos = (env, a, b) => env.DB.prepare("SELECT 1 FROM contato WHERE a = ? AND b = ?").bind(a, b).first();

function lerImp(s) { try { return limparImpressao(JSON.parse(s || "{}")); } catch { return {}; } }

async function salaViva(env, onde, valor, t) {
  return env.DB.prepare(`SELECT * FROM sala WHERE ${onde} = ? AND expira > ?`).bind(valor, t).first();
}

async function pessoaPub(env, id) {
  const p = await env.DB.prepare("SELECT nome, avatar FROM pessoa WHERE id = ?").bind(id).first();
  return { id, nome: p?.nome || "", av: p?.avatar || "" };
}

export async function rotaSala(rota, metodo, env, quem, corpo, url, h, limitar) {
  if (rota !== "/v1/sala" && !rota.startsWith("/v1/sala/")) return null;
  const { json, erro, agora } = h;
  if (!env.SALA_SEGREDO || !juntosBase(env)) return erro("juntos indisponivel", 503);
  const t = agora();
  const inf = metodo === "GET" ? url.searchParams.get("infantil") === "1" : !!(corpo?.infantil === 1 || corpo?.infantil === true);
  await marcarInfantil(env, quem, inf, t);
  const eu = { pessoa: quem.id, nome: quem.nome || "", av: quem.avatar || "" };
  const bilheteTv = (sala, papel, extra = {}) => assinar(env.SALA_SEGREDO, { sala, ...eu, papel,
    exp: t + BILHETE_TV_S, n: nonce(), ...(inf ? { inf: 1 } : {}), ...extra });

  if (rota === "/v1/sala" && metodo === "POST") {
    if (!(await limitar(env, `sala:${quem.id}`, 10, 3600, t))) return erro("limite", 429);
    const ep = String(corpo?.ep || "");
    if (!/^tt\d{1,10}(:\d{1,4}:\d{1,5})?$/.test(ep)) return erro("ep invalido", 400);
    const titulo = h.limpar(corpo?.titulo, 120).replace(/[\u0000-\u001f]/g, " ");
    const poster = typeof corpo?.poster === "string" && /^https:\/\//.test(corpo.poster) ? corpo.poster.slice(0, 512) : "";
    const imp = limparImpressao(corpo?.imp);
    const controle = corpo?.controle === "anfitriao" ? "anfitriao" : "todos";
    const espera = [10, 20, 60].includes(corpo?.espera) ? corpo.espera : 20;
    const pos = Number.isFinite(corpo?.pos) ? Math.max(0, Math.min(86400, corpo.pos)) : 0;
    const id = idNovo();
    let codigo = "";
    for (let i = 0; i < 6 && !codigo; i++) {
      const c = codigoNovo();
      if (!(await env.DB.prepare("SELECT 1 FROM sala WHERE codigo = ? AND expira > ?").bind(c, t).first())) codigo = c;
    }
    if (!codigo) return erro("tente de novo", 503);
    await env.DB.prepare("DELETE FROM sala WHERE codigo = ?").bind(codigo).run();
    await env.DB.prepare("INSERT INTO sala (id, codigo, anfitriao, ep, titulo, poster, imp, controle, infantil, criado, expira) " +
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
      .bind(id, codigo, quem.id, ep, titulo, poster, JSON.stringify(imp), controle, inf ? 1 : 0, t, t + SALA_TTL).run();
    const convidados = await convidar(env, quem, id, Array.isArray(corpo?.convidados) ? corpo.convidados : [], t, limitar);
    const bilhete = await bilheteTv(id, "anfitriao", { cfg: { ep, titulo, poster, imp, controle, espera, pos, codigo,
      ...(inf ? { inf: 1 } : {}) } });
    return json({ id, codigo, bilhete, convidados, ...enderecos(env) });
  }

  if (rota === "/v1/sala/convidar" && metodo === "POST") {
    const s = await salaViva(env, "id", String(corpo?.id || ""), t);
    if (!s || s.anfitriao !== quem.id) return erro("sala desconhecida", 404);
    const convidados = await convidar(env, quem, s.id, Array.isArray(corpo?.convidados) ? corpo.convidados : [], t, limitar);
    return json({ convidados });
  }

  if (rota === "/v1/sala/convites" && metodo === "GET") {
    const r = await env.DB.prepare(
      "SELECT s.id, s.ep, s.titulo, s.poster, s.imp, s.controle, s.anfitriao, c.de, c.criado " +
      "FROM convite c JOIN sala s ON s.id = c.sala WHERE c.para = ? AND c.estado = 0 AND s.expira > ? " +
      "AND NOT EXISTS (SELECT 1 FROM bloqueio b WHERE (b.quem = c.para AND b.alvo = c.de) OR (b.quem = c.de AND b.alvo = c.para)) " +
      "AND NOT EXISTS (SELECT 1 FROM sala_silencio z WHERE z.pessoa = c.para AND (z.de = c.de OR z.de = '*')) " +
      "ORDER BY c.criado DESC LIMIT 10").bind(quem.id, t).all();
    const convites = [];
    for (const x of r.results || []) {
      if (inf && !(await conhecido(env, quem.id, x.de))) continue;
      convites.push({ id: x.id, ep: x.ep, titulo: x.titulo, poster: x.poster, imp: lerImp(x.imp), controle: x.controle,
        de: await pessoaPub(env, x.de), criado: x.criado });
    }
    return json({ convites });
  }

  if (rota === "/v1/sala/entrar" && metodo === "POST") {
    const porCodigo = corpo?.codigo !== undefined;
    let s;
    if (porCodigo) {
      const c = codigoLimpo(corpo.codigo);
      s = c ? await salaViva(env, "codigo", c, t) : null;
      if (!s) {
        // Errar codigo tem teto: 10 por hora. 32^6 ~ 10^9 nao se adivinha assim.
        if (!(await limitar(env, `salacod:${quem.id}`, 10, 3600, t))) return erro("limite", 429);
        return erro("sala desconhecida", 404);
      }
    } else {
      s = await salaViva(env, "id", String(corpo?.id || ""), t);
      if (!s) return erro("sala desconhecida", 404);
    }
    const anf = s.anfitriao;
    const resumo = { id: s.id, ep: s.ep, titulo: s.titulo, poster: s.poster, imp: lerImp(s.imp), controle: s.controle,
      anfitriao: await pessoaPub(env, anf) };
    if (anf === quem.id)
      return json({ id: s.id, codigo: s.codigo, bilhete: await bilheteTv(s.id, "anfitriao"), resumo, ...enderecos(env) });
    // Bloqueio nos dois sentidos parece "nao existe" — quem foi bloqueado nao
    // distingue uma coisa da outra.
    if (await bloqueado(env, quem.id, anf)) return erro("sala desconhecida", 404);
    // Perfil infantil, nos dois sentidos: a crianca so entra em sala de quem a
    // conta conhece, e a sala de uma crianca so recebe quem a conta dela conhece.
    if (inf && !(await conhecido(env, quem.id, anf))) return erro("infantil", 403);
    if (s.infantil && !(await conhecido(env, anf, quem.id))) return erro("infantil", 403);
    const conv = await env.DB.prepare("SELECT estado FROM convite WHERE sala = ? AND para = ?").bind(s.id, quem.id).first();
    let pend = 0;
    if (!conv) {
      if (!porCodigo) return erro("sem convite", 403);
      // Por codigo: amigo (ou a mesma conta) entra direto; estranho espera o
      // anfitriao aceitar, no Durable Object.
      if (!(await amigos(env, anf, quem.id)) && contaDe(anf) !== contaDe(quem.id)) pend = 1;
    }
    await env.DB.prepare("INSERT INTO convite (sala, para, de, criado, estado) VALUES (?, ?, ?, ?, 1) " +
      "ON CONFLICT(sala, para) DO UPDATE SET estado = 1").bind(s.id, quem.id, anf, t).run();
    return json({ id: s.id, bilhete: await bilheteTv(s.id, "convidado", pend ? { pend: 1 } : {}), pend, resumo,
      ...enderecos(env) });
  }

  if (rota === "/v1/sala/recusar" && metodo === "POST") {
    await env.DB.prepare("UPDATE convite SET estado = 2 WHERE sala = ? AND para = ? AND estado = 0")
      .bind(String(corpo?.id || ""), quem.id).run();
    return json({ ok: 1 });
  }

  if (rota === "/v1/sala/fim" && metodo === "POST") {
    const id = String(corpo?.id || "");
    const s = await env.DB.prepare("SELECT anfitriao FROM sala WHERE id = ?").bind(id).first();
    if (s && s.anfitriao === quem.id) await env.DB.batch([
      env.DB.prepare("DELETE FROM convite WHERE sala = ?").bind(id),
      env.DB.prepare("DELETE FROM sala WHERE id = ?").bind(id),
    ]);
    return json({ ok: 1 });
  }

  if (rota === "/v1/sala/silenciar" && metodo === "POST") {
    const de = corpo?.de === "*" ? "*" : String(corpo?.de || "").slice(0, 120);
    if (!de) return erro("de invalido", 400);
    if (corpo?.sim === 0 || corpo?.sim === false)
      await env.DB.prepare("DELETE FROM sala_silencio WHERE pessoa = ? AND de = ?").bind(quem.id, de).run();
    else await env.DB.prepare("INSERT OR IGNORE INTO sala_silencio (pessoa, de, criado) VALUES (?, ?, ?)")
      .bind(quem.id, de, t).run();
    return json({ ok: 1 });
  }
  return erro("rota desconhecida", 404);
}

// Convite so para contato confirmado, nao bloqueado e que nao silenciou quem
// convida. Uma crianca (marcada) so recebe de quem a conta dela conhece. O que
// nao passa e descartado em silencio: quem convida nao descobre o motivo.
async function convidar(env, quem, sala, lista, t, limitar) {
  const ok = [];
  for (const raw of lista.slice(0, CONVIDADOS_MAX)) {
    const para = String(raw || "").slice(0, 120);
    if (!para || para === quem.id) continue;
    if (!(await amigos(env, quem.id, para))) continue;
    if (await bloqueado(env, quem.id, para)) continue;
    if (await env.DB.prepare("SELECT 1 FROM sala_silencio WHERE pessoa = ? AND (de = ? OR de = '*')").bind(para, quem.id).first()) continue;
    if (await env.DB.prepare("SELECT 1 FROM sala_infantil WHERE pessoa = ?").bind(para).first()
        && !(await conhecido(env, para, quem.id))) continue;
    if (!(await limitar(env, `convite:${quem.id}`, 30, 3600, t))) break;
    await env.DB.prepare("INSERT OR IGNORE INTO convite (sala, para, de, criado, estado) VALUES (?, ?, ?, ?, 0)")
      .bind(sala, para, quem.id, t).run();
    ok.push(para);
  }
  return ok;
}

export function limpezaSala(env, t) {
  return [
    env.DB.prepare("DELETE FROM convite WHERE sala IN (SELECT id FROM sala WHERE expira < ?)").bind(t),
    env.DB.prepare("DELETE FROM sala WHERE expira < ?").bind(t),
  ];
}
