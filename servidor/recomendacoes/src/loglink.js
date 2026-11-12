// DOWNLOAD DO REGISTRO PELA NUVEM (Matis94 adaptado, 11/2026). Quando a TV nao
// consegue abrir o endereco local do registro (celular.c motivo), o cartao
// "QR de diagnostico" (Azul/F10) pode, SO COM O OK/SETA DA PESSOA, mandar a
// copia mascarada e gzipada do registro (a mesma que a pagina local serviria —
// senhas, chaves e hosts ja vao cortados no cliente, em logexport.c) para cá.
// O worker guarda os bytes num KV com TTL de 72 h e devolve um link com token
// sorteado; a pessoa manda o link/QR a quem pediu o registro.
//
// Por que KV e nao D1: o corpo e binario ate 2 MiB, efemero por contrato (o KV
// ja apaga sozinho no TTL — sem cron e sem limpeza propria), e lido uma vez
// por quem recebe o link. O D1 guardaria um lixo expirado ate a varredura.
//
// Seguranca, no mesmo padrao do /v1/registro de diagnostico (#77):
//  - POST so com x-nuvio-auth: diagnostico + Bearer DIAG_TOKEN (segredo do
//    worker); o token de link e sorteado aqui (128 bits hex) — quem nao tem o
//    link nao advinha; quem tem o link so le bytes ja mascarados;
//  - GET/HEAD sem auth de proposito: e o link que a pessoa manda por fora;
//    sem CORS extra porque e link, nao API; cache-control: no-store para a
//    expiracao ser real e um intermediario nao ressuscitar registro morto;
//  - corpo limitado (413) e token validado por alfabeto antes de bater no KV.
//
// NADA AQUI foi publicado: o binding LOGS precisa existir no wrangler.toml do
// deploy (comentado abaixo) e o segredo DIAG_TOKEN ja existe para o #77.
const CORPO_MAX = 2 * 1024 * 1024;
const TTL_S = 72 * 3600;
const TOKEN_RX = /^[0-9a-f]{32}$/;

export async function rotaLoglinkPost(req, env, erro, json) {
  if (!env.LOGS) return erro("servico indisponivel", 503);
  const aut = req.headers.get("authorization") || "";
  const tok = aut.startsWith("Bearer ") ? aut.slice(7).trim() : "";
  if (!env.DIAG_TOKEN || !tok || tok !== env.DIAG_TOKEN) return erro("nao autenticado", 401);
  const reader = req.body?.getReader();
  if (!reader) return erro("corpo vazio", 400);
  const chunks = []; let size = 0;
  for (;;) {
    const { value, done } = await reader.read();
    if (done) break;
    size += value.byteLength;
    if (size > CORPO_MAX) { await reader.cancel(); return erro("corpo grande", 413); }
    chunks.push(value);
  }
  if (!size) return erro("corpo vazio", 400);
  const buf = new Uint8Array(size); let offset = 0;
  for (const chunk of chunks) { buf.set(chunk, offset); offset += chunk.byteLength; }
  const t = [...crypto.getRandomValues(new Uint8Array(16))]
    .map((x) => x.toString(16).padStart(2, "0")).join("");
  await env.LOGS.put("log:" + t, buf, { expirationTtl: TTL_S, metadata: { expires: Date.now() + TTL_S * 1000 } });
  const base = new URL(req.url).origin;
  return json({ ok: 1, url: base + "/v1/loglink/" + t, expira_s: TTL_S });
}

export async function rotaLoglinkGet(req, env, erro, token) {
  if (!env.LOGS) return erro("servico indisponivel", 503);
  if (!TOKEN_RX.test(token)) return erro("nao encontrado", 404);
  const { value: buf, metadata } = await env.LOGS.getWithMetadata("log:" + token, "arrayBuffer");
  if (!buf || !Number.isFinite(metadata?.expires) || Date.now() >= metadata.expires) return erro("nao encontrado", 404);
  if (req.method === "HEAD")
    return new Response(null, {
      status: 200,
      headers: {
        "content-type": "application/gzip",
        "content-length": String(buf.byteLength),
        "cache-control": "no-store",
      },
    });
  return new Response(buf, {
    status: 200,
    headers: {
      // gzipado de proposito: quem recebe o link baixa o .gz do registro.
      // Sem content-encoding: nao e resposta comprimida, e o arquivo em si.
      "content-type": "application/gzip",
      "content-disposition": 'attachment; filename="nuvio-registro.log.gz"',
      "cache-control": "no-store",
    },
  });
}
