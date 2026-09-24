// PROXY GENERICO DE CORS / CONTEUDO MISTO PARA A VIDAA (#125, rota
// GET /v1/proxy?u=<url>).
//
// POR QUE EXISTE: a pagina da TV e servida em https (o proprio worker, ver
// rotaTv em index.js), e o Chromium da Hisense bloqueia toda requisicao
// http:// feita de dentro dela (conteudo misto) — o MESMO bloqueio que o
// Tizen tem para http (ver a nota no topo de xtream.js), so que na VIDAA a
// pagina INTEIRA e https, entao vale para qualquer chamada de rede do app,
// nao so para o painel Xtream. E tambem cobre https sem CORS (metadados de
// addon, legenda externa, icone de CDN) que src/xtream.js nao tenta resolver
// porque so conhece o formato do Xtream.
//
// O QUE NUNCA PASSA: video/audio — o worker nao tem CPU nem banda para
// reproxy de midia, e seria uma forma gratuita de sair da conta da
// Cloudflare por um app que nao e nosso; ver tipoAceito. Playlist HLS
// (texto, poucos KB) passa; SEGMENTO de video (.ts, video/*) nao. Qualquer
// destino privado/loopback/link-local — mesmo crivo de xtream.js
// (destinoPublico), reaproveitado e nao duplicado. Resposta acima de 8 MB é
// abortada em pleno streaming, nao so recusada pelo content-length
// declarado (que o servidor pode nem mandar).
//
// SEM CACHE, DE PROPOSITO: o corpo pode ser uma legenda ou um JSON que muda,
// e o cliente ja manda if-none-match quando tem um etag guardado — o 304
// economiza o corpo sem o worker guardar nada.
import { destinoPublico } from "./xtream.js";

const SALTOS = 5;
const TAM_MAX = 8 * 1024 * 1024;   // 8 MB: folga generosa para legenda, playlist, json de addon
const PRAZO_MS = 20000;            // o mesmo prazo do proxy do Xtream
// UA de navegador: alguns servidores de addon/CDN recusam (403) o fetch do
// Workers por ele nao mandar User-Agent nenhum — a MESMA licao de
// idTrakt/idNuvio em index.js, medida ali com o Trakt.
const UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0 Safari/537.36 nuvio-vidaa-proxy/1";

const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-methods": "GET, OPTIONS",
  // So os dois cabecalhos que este proxy repassa (ver rotaProxy). "accept" e
  // seguro por padrao no CORS e nao precisa entrar aqui.
  "access-control-allow-headers": "if-none-match",
  "access-control-expose-headers": "etag, x-nuvio-url-final",
  "cross-origin-resource-policy": "cross-origin",
};

// Extensoes cujo caminho aceita application/octet-stream — servidor de
// legenda ou playlist as vezes nao manda content-type nenhum, ou manda o
// generico; recusar so pelo tipo perderia um arquivo valido que o path ja
// identifica.
const EXT_OCTET = /\.(srt|vtt|ass|ssa|sub|json|m3u|m3u8)$/i;

// Decide pelo content-type da RESPOSTA, nao pela extensao pedida: um CDN pode
// devolver imagem para uma url sem extensao. video/* e audio/* SEMPRE
// recusados, mesmo com extensao "de texto" — o content-type e quem manda.
function tipoAceito(ct, pathname) {
  const c = (ct || "").split(";")[0].trim().toLowerCase();
  if (!c) return true;   // sem content-type: decide pelo tamanho, nao pelo tipo
  if (c.startsWith("video/") || c.startsWith("audio/")) return false;
  if (c.startsWith("image/")) return true;
  if (c === "application/json" || c.startsWith("text/")) return true;
  if (c.endsWith("/xml") || c.endsWith("+xml")) return true;
  if (c === "application/x-subrip") return true;
  // Playlist HLS: e a LISTA de segmentos (poucos KB de texto), nao o
  // segmento. O segmento em si vem como video/* do CDN e cai na regra acima.
  if (c === "application/vnd.apple.mpegurl" || c === "application/x-mpegurl") return true;
  if (c === "application/octet-stream") return EXT_OCTET.test(pathname);
  return false;
}

function falha(msg, status) {
  return new Response(JSON.stringify({ erro: msg }), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store", ...CORS },
  });
}

// Corta o corpo no teto sem guardar tudo na memoria do worker (mesmo
// mecanismo de xtream.js): aborta em pleno streaming, nao so pelo
// content-length declarado.
function limitar(corpo, max) {
  let n = 0;
  return corpo.pipeThrough(new TransformStream({
    transform(bloco, ctl) {
      n += bloco.byteLength;
      if (n > max) ctl.error(new Error("resposta grande demais"));
      else ctl.enqueue(bloco);
    },
  }));
}
function corpoComPrazo(corpo, relogio) {
  return corpo.pipeThrough(new TransformStream({ flush() { clearTimeout(relogio); } }));
}

// LIMITE DE TAXA POR IP, EM MEMORIA DO ISOLATE — BEST-EFFORT, nao defesa de
// verdade. Um isolate novo (deploy, cold start, outra borda) zera o contador,
// e isolates concorrentes na MESMA borda nao compartilham este Map. Serve so
// para segurar um cliente com bug em loop, nao um ataque deliberado. 300/min
// (~1 a cada 200 ms) tem folga sobre o uso normal (icone, legenda, playlist).
const JANELA_MS = 60000;
const LIMITE_JANELA = 300;
const contadores = new Map();
function limiteTaxa(ip) {
  const t = Date.now();
  let c = contadores.get(ip);
  if (!c || t - c.inicio > JANELA_MS) { c = { inicio: t, n: 0 }; contadores.set(ip, c); }
  c.n++;
  // Poda oportunista: sem isto o Map cresce por isolate ate o worker reciclar
  // (isolates de worker nao tem um "fim" previsivel para um clearInterval).
  if (contadores.size > 5000) {
    for (const [k, v] of contadores) if (t - v.inicio > JANELA_MS) contadores.delete(k);
  }
  return c.n <= LIMITE_JANELA;
}

export async function rotaProxy(req, url, env, buscar = fetch) {
  const ip = req.headers.get("cf-connecting-ip") || "0.0.0.0";
  if (!limiteTaxa(ip)) return falha("muitos pedidos, tente de novo em instantes", 429);

  const bruto = url.searchParams.get("u") || "";
  if (!bruto) return falha("sem url", 400);
  let alvo = destinoPublico(bruto);
  if (!alvo) return falha("destino recusado", 403);

  // So os cabecalhos que o alvo pode precisar para responder certo (accept) ou
  // para o 304 economizar corpo (if-none-match). Nada de Cookie, Authorization
  // ou Referer: o cliente que quisesse vazar credencial de outro dominio por
  // aqui nao pode.
  const cabecalhos = { "user-agent": UA, "accept": req.headers.get("accept") || "*/*" };
  const inm = req.headers.get("if-none-match");
  if (inm) cabecalhos["if-none-match"] = inm;

  const ctl = new AbortController();
  const relogio = setTimeout(() => ctl.abort(), PRAZO_MS);
  let r;
  try {
    for (let salto = 0; ; salto++) {
      r = await buscar(alvo.toString(), {
        method: "GET", redirect: "manual", signal: ctl.signal, headers: cabecalhos,
      });
      // 304 (Not Modified, resposta ao if-none-match que repassamos) NAO e
      // redirecionamento: nao vem com Location, e tratado como redirect cairia
      // na recusa de baixo por "sem Location". So os 3xx com Location entram
      // no laco; o 304 sai direto para a resposta final, como o 200.
      if (r.status < 300 || r.status >= 400 || r.status === 304) break;
      const loc = r.headers.get("location");
      if (!loc || salto >= SALTOS) { clearTimeout(relogio); return falha("redirect demais", 502); }
      // O salto passa pelo MESMO crivo do destino original — um redirect para
      // IP privado nao pode ser o jeito de contornar a checagem de cima.
      const prox = destinoPublico(new URL(loc, alvo).toString());
      if (!prox) { clearTimeout(relogio); return falha("redirect recusado", 403); }
      alvo = prox;
    }
  } catch (e) {
    clearTimeout(relogio);
    const nome = e && e.name === "AbortError" ? "prazo" : "rede";
    console.log(`proxy ${alvo.host} -> falha ${nome}`);
    return falha(nome === "prazo" ? "destino nao respondeu no prazo" : "destino inacessivel", 504);
  }

  const ct = r.headers.get("content-type") || "";
  if (r.status < 300 && !tipoAceito(ct, alvo.pathname)) {
    clearTimeout(relogio);
    console.log(`proxy ${alvo.host} -> tipo recusado (${ct.split(";")[0] || "?"})`);
    return falha("tipo de conteudo recusado", 415);
  }
  const cl = Number(r.headers.get("content-length") || 0);
  if (cl > TAM_MAX) { clearTimeout(relogio); return falha("resposta grande demais", 502); }

  const corpo = r.body ? limitar(r.body, TAM_MAX) : null;
  if (!corpo) clearTimeout(relogio);

  const headersSaida = {
    "content-type": ct || "application/octet-stream",
    "cache-control": "no-store",
    // O CAMINHO FINAL, exposto: quando o cliente pediu uma url que redirecionou,
    // e assim que ele sabe para onde foi sem seguir o redirect ele mesmo.
    "x-nuvio-url-final": alvo.toString(),
    ...CORS,
  };
  const etag = r.headers.get("etag");
  if (etag) headersSaida["etag"] = etag;

  return new Response(corpo ? corpoComPrazo(corpo, relogio) : null, {
    status: r.status,
    headers: headersSaida,
  });
}

export const rotaProxyCors = CORS;
