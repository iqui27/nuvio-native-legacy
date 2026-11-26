// Direct household connections. Profile membership comes from the same
// authenticated Nuvio RPC as the profile picker, never from client IDs/names.
export async function rotaPerfisContatos(req, env, quem, quemToken, corpo, h, registrar, bloqueado) {
  const rota = new URL(req.url).pathname;
  if (rota !== "/v1/contatos/perfis") return null;
  if (!["GET", "POST"].includes(req.method)) return h.erro("metodo invalido", 405);
  const conta = /^nuvio:([^:]+)(?::\d{1,2})?$/.exec(quemToken.id || "");
  if (!conta) return h.erro("conta Nuvio necessaria", 403);
  const base = `nuvio:${conta[1]}`;
  let resposta, perfis;
  try {
    resposta = await fetch(`${env.SUPABASE_URL}/rest/v1/rpc/sync_pull_profiles`, {
      method: "POST",
      headers: { apikey: env.SUPABASE_ANON_KEY, authorization: req.headers.get("authorization"),
        "content-type": "application/json", "user-agent": "nuvio-recomendacoes/1.0" },
      body: "{}",
    });
    if (!resposta.ok) return h.erro("nao foi possivel consultar os perfis", 503);
    perfis = await resposta.json();
  } catch { return h.erro("nao foi possivel consultar os perfis", 503); }
  if (!Array.isArray(perfis) || perfis.length > 32) return h.erro("lista de perfis invalida", 503);
  const vistos = new Set();
  perfis = perfis.flatMap((p) => {
    const indice = Number(p?.profile_index ?? p?.id);
    if (!Number.isInteger(indice) || indice < 1 || indice > 32 || vistos.has(indice)) return [];
    vistos.add(indice);
    const primario = p.is_primary === true || p.is_primary === 1 || (p.is_primary == null && indice === 1);
    return [{ indice, id: primario ? base : `${base}:${indice}`,
      nome: typeof p.name === "string" ? p.name.slice(0, 64) : `Perfil ${indice}`,
      avatar: typeof p.avatar_url === "string" ? p.avatar_url.slice(0, 256) : "" }];
  });
  if (!perfis.some((p) => p.id === quem.id)) return h.erro("perfil ativo invalido", 403);
  if (req.method === "GET") {
    const saida = [];
    for (const p of perfis) {
      if (p.id === quem.id) continue;
      const amigo = await env.DB.prepare("SELECT 1 FROM contato WHERE a = ? AND b = ?").bind(quem.id, p.id).first();
      const fechado = await bloqueado(env.DB, quem.id, p.id);
      saida.push({ ...p, estado: fechado ? "bloqueado" : amigo ? "amigo" : "disponivel" });
    }
    return h.json({ perfis: saida });
  }
  const indice = corpo?.perfil;
  if (!Number.isInteger(indice)) return h.erro("perfil invalido", 400);
  const alvo = perfis.find((p) => p.indice === indice && p.id !== quem.id);
  if (!alvo || await bloqueado(env.DB, quem.id, alvo.id)) return h.erro("perfil indisponivel", 404);
  await registrar(env, { id: alvo.id, nome: quemToken.nome }, { nome: alvo.nome, avatar: alvo.avatar });
  const t = h.agora();
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'perfil')").bind(quem.id, alvo.id, t),
    env.DB.prepare("INSERT OR IGNORE INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'perfil')").bind(alvo.id, quem.id, t),
  ]);
  return h.json({ ok: 1, contato: { id: alvo.id, nome: alvo.nome } });
}
