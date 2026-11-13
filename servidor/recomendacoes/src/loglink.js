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
// Seguranca (escopo proprio, 12/11/2026): o envio NAO usa o DIAG_TOKEN do
// /v1/registro (#77). Um token embutido na build pode ser extraido; por isso o
// envio tem credencial PROPRIA e so de envio:
//  - POST so com x-nuvio-auth: loglink + Bearer LOGLINK_TOKEN (segredo do
//    worker, configurado por nome no deploy). Esse token so cria link: nao
//    le, nao lista, nao apaga, nao grava recibo. O DIAG_TOKEN aqui e 401, e o
//    LOGLINK_TOKEN no /v1/registro tambem e 401 (la so vale DIAG_TOKEN);
//  - se os dois segredos forem iguais o envio recusa (503): configuracao
//    errada nunca alarga o privilegio do token de envio;
//  - teto por IP (LIM_IP por hora) e teto global (LIM_DIA por dia) no
//    contador atomico `limite` do D1: uma janela movel de 72 h pode cruzar
//    quatro dias fixos (~800 MiB de dados vivos no pior caso);
//  - o token de link e sorteado aqui (128 bits hex): quem nao tem o link nao
//    advinha; quem tem o link so le bytes ja mascarados na TV (logexport.c);
//  - GET/HEAD sem auth de proposito: e o link que a pessoa manda por fora;
//    expiracao conferida no GET (nao confia so no TTL do KV), no-store;
//  - nada aqui registra URL ou token de link em console.
const CORPO_MAX = 2 * 1024 * 1024;
const TTL_S = 72 * 3600;
const TOKEN_RX = /^[0-9a-f]{32}$/;
const LIM_IP = 6;
const LIM_DIA = 100;

export function loglinkAutorizado(req, env) {
  const aut = req.headers.get("authorization") || "";
  const tok = aut.startsWith("Bearer ") ? aut.slice(7).trim() : "";
  if (!env.LOGLINK_TOKEN || !tok || tok !== env.LOGLINK_TOKEN) return false;
  return true;
}

export async function rotaLoglinkPost(req, env, erro, json, limitar) {
  if (!env.LOGS || !env.DB || !env.LOGLINK_TOKEN) return erro("servico indisponivel", 503);
  if (env.DIAG_TOKEN && env.LOGLINK_TOKEN === env.DIAG_TOKEN) return erro("servico indisponivel", 503);
  if (!loglinkAutorizado(req, env)) return erro("nao autenticado", 401);
  const declarado = Number(req.headers.get("content-length") || 0);
  if (declarado > CORPO_MAX) return erro("corpo grande", 413);
  const t = Math.floor(Date.now() / 1000);
  const ip = req.headers.get("cf-connecting-ip") || "0.0.0.0";
  if (!(await limitar(env, "loglink:ip:" + ip, LIM_IP, 3600, t))) return erro("limite", 429);
  if (!(await limitar(env, "loglink:dia", LIM_DIA, 86400, t))) return erro("limite diario", 429);
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
  const id = [...crypto.getRandomValues(new Uint8Array(16))]
    .map((x) => x.toString(16).padStart(2, "0")).join("");
  await env.LOGS.put("log:" + id, buf, { expirationTtl: TTL_S, metadata: { expires: Date.now() + TTL_S * 1000 } });
  const base = new URL(req.url).origin;
  return json({ ok: 1, url: base + "/v1/loglink/" + id, expira_s: TTL_S });
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
