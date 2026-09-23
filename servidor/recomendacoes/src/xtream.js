// PROXY DO XTREAM PARA A SAMSUNG (#112, rota POST /v1/xtream, url do painel
// no CORPO).
//
// POR QUE EXISTE: no Tizen TODA requisicao `http://` que o app faz morre antes
// de sair, e `https://` passa (log D1 id 1647, Samsung 1.4.1: "servidor nao
// respondeu a lista de canais" e os icones http de 24horas.cc, com o mesmo
// servidor respondendo CORS). E o Chromium do Tizen barrando http puro vindo
// do app, e a maioria dos paineis Xtream so fala http. Este worker e https, e
// daqui para o painel e servidor falando com servidor: o bloqueio nao existe.
// A LG (libcurl) continua indo direto — so o ramo __EMSCRIPTEN__ usa isto.
//
// O QUE ELA VE, e por isso as regras abaixo: a url do player_api.php leva
// usuario e senha do Xtream NA QUERY. Elas passam por aqui em transito e nao
// sao guardadas: sem D1, sem cache (nem o da borda — resposta com
// `cache-control: no-store` e nenhum cache.put), e o log so tem host e action.
// NUNCA logar `u`, a url montada, nem e.message de fetch (a mensagem pode
// trazer a url).
//
// POR QUE POST E NAO GET: a primeira versao (deploy 748f61f7) era GET ?u=, e
// o `wrangler tail` imprimiu a url de ENTRADA inteira — com a credencial
// dentro do u= — antes de qualquer linha nossa. Sanear o console.log nao
// adianta para o que a propria plataforma registra. No corpo, a url nao
// aparece nem no tail nem no Workers Logs. O corpo e a url crua em
// text/plain (tipo "simples" do CORS: o XHR nem faz preflight) ou JSON
// {"u": "..."}. O GET ficou SO para imagem sem credencial — e o que um
// cliente antigo ou um teste manual manda —, e um u= com username/password
// e recusado sem log (o vazamento ja aconteceu na plataforma, mas ao menos
// o painel nao recebe e ninguem acostuma a usar assim). O VIDEO NAO PASSA POR AQUI: a url do .m3u8 vai direto para o
// AVPlay, que nao e o XHR do navegador e nao sofre o bloqueio.
//
// DOIS MODOS, pelo caminho da url:
//  - LISTA: `/player_api.php` (e `/xmltv.php` NAO: o app le o EPG por outras
//    fontes, e caminho que ninguem usa e superficie sem motivo). So as actions
//    de LISTA_ACOES, e a query e REMONTADA so com os parametros conhecidos —
//    nada do que o cliente mandar alem disso chega ao painel.
//  - IMAGEM: qualquer outro caminho, mas a resposta so volta se o painel
//    disser `image/*`. E o icone de canal (stream_icon), que vem http do mesmo
//    painel ou de um CDN dele. Nao e proxy aberto de pagina: HTML, JSON e
//    video sao recusados pelo tipo, e o teto de tamanho e menor.
//
// SSRF: o destino tem de ser host PUBLICO. Recusa localhost, nome sem ponto,
// .local/.internal/.lan/.home.arpa, IPv6 literal e IPv4 privado, loopback,
// CGNAT, link-local (169.254 = metadados de nuvem) e multicast. O parser de
// URL normaliza 2130706433 e 0x7f.1 para 127.0.0.1 antes do teste. Redirect e
// seguido A MAO (ate 3) para o destino de cada salto passar pelo mesmo crivo.
// Nome publico que resolve para IP privado (rebinding) nao e resolvido aqui:
// o fetch do Workers sai da rede da Cloudflare, que nao roteia para a rede
// privada de ninguem.

const LISTA_ACOES = new Set([
  "get_live_categories", "get_live_streams",
  "get_vod_categories", "get_vod_streams", "get_vod_info",
  "get_series_categories", "get_series", "get_series_info",
  "get_short_epg",
]);
// Parametros que o player_api.php conhece e o app (ou um app Xtream qualquer)
// usa. Tudo fora disto e descartado na remontagem.
const LISTA_PARAMS = ["username", "password", "action", "category_id", "stream_id",
                      "vod_id", "series_id", "limit"];
const LISTA_MAX = 20 * 1024 * 1024;  // 20 mil canais dao ~5 MB; 20 e folga
const IMAGEM_MAX = 2 * 1024 * 1024;  // icone de canal; 2 MB ja e absurdo
const PRAZO_MS = 20000;              // o mesmo XT_PRAZO_S do cliente
const SALTOS = 3;
// UA de player: ha painel que recusa o UA de datacenter/navegador.
const UA = "VLC/3.0.20 LibVLC/3.0.20";

const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-methods": "GET, POST, OPTIONS",
  "cross-origin-resource-policy": "cross-origin",
};

function falha(msg, status) {
  return new Response(JSON.stringify({ erro: msg }), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store", ...CORS },
  });
}

// IPv4 em dotted-quad (o parser de URL ja normalizou) -> privado/reservado?
function ipv4Reservado(h) {
  const m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(h);
  if (!m) return false;
  const [a, b] = [Number(m[1]), Number(m[2])];
  return a === 0 || a === 10 || a === 127 || a >= 224 ||
    (a === 100 && b >= 64 && b <= 127) ||   // CGNAT
    (a === 169 && b === 254) ||             // link-local, metadados
    (a === 172 && b >= 16 && b <= 31) ||
    (a === 192 && b === 168) ||
    (a === 192 && b === 0) ||               // 192.0.0.0/24 e 192.0.2.0/24
    (a === 198 && (b === 18 || b === 19));  // benchmark
}

// Devolve a URL validada, ou null. So http/https, sem usuario:senha@ na
// autoridade (seria outra credencial viajando), host publico.
export function destinoPublico(texto) {
  let u;
  try { u = new URL(texto); } catch { return null; }
  if (u.protocol !== "http:" && u.protocol !== "https:") return null;
  if (u.username || u.password) return null;
  const h = u.hostname.toLowerCase();
  if (!h || h.startsWith("[") || h.includes(":")) return null;   // IPv6 literal
  if (!h.includes(".")) return null;                            // localhost, intranet
  if (/\.(localhost|local|internal|lan|home\.arpa|intranet|corp)$/.test(h)) return null;
  if (ipv4Reservado(h)) return null;
  return u;
}

// Modo da url: "lista" (com a url remontada), "imagem", ou null (recusa).
export function classificar(u) {
  if (u.pathname === "/player_api.php") {
    const acao = u.searchParams.get("action");
    if (acao !== null && !LISTA_ACOES.has(acao)) return null;
    const limpa = new URL(u.origin + u.pathname);
    for (const k of LISTA_PARAMS) {
      const v = u.searchParams.get(k);
      if (v !== null) limpa.searchParams.set(k, v);
    }
    return { modo: "lista", url: limpa, acao: acao || "info" };
  }
  // O resto do painel (live/, movie/, series/ = VIDEO, e o proprio
  // xmltv/get.php/panel_api) nao e imagem: recusa sem nem buscar.
  if (/^\/(live|movie|series|timeshift|hls|streaming)\//.test(u.pathname)) return null;
  if (/\.(php|m3u8?|ts|mp4|mkv|avi)$/i.test(u.pathname)) return null;
  return { modo: "imagem", url: u, acao: "imagem" };
}

// Corta o corpo no teto sem guardar tudo na memoria do worker.
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

const CORPO_MAX = 4096;   // uma url; o cliente monta no maximo ~3,3 KB

// A url do painel: do corpo (POST) ou do u= (GET, so imagem). null + resposta
// de erro quando nao da.
async function lerAlvo(req, url) {
  if (req.method === "POST") {
    const cl = Number(req.headers.get("content-length") || 0);
    if (cl > CORPO_MAX) return { erro: falha("corpo grande demais", 413) };
    const cru = (await req.text()).trim();
    if (cru.length > CORPO_MAX) return { erro: falha("corpo grande demais", 413) };
    let u = cru;
    if (cru.startsWith("{")) {
      try { u = String(JSON.parse(cru)?.u || ""); } catch { return { erro: falha("json invalido", 400) }; }
    }
    return { u, get: false };
  }
  const u = url.searchParams.get("u") || "";
  // Credencial na url de entrada: 403 SEM log, nem de host.
  if (/username|password/i.test(u)) return { erro: falha("credencial na url: use POST", 403) };
  return { u, get: true };
}

export async function rotaXtream(req, url, buscar = fetch) {
  const lido = await lerAlvo(req, url);
  if (lido.erro) return lido.erro;
  const bruto = lido.u;
  if (!bruto) return falha("sem url", 400);
  let alvo = destinoPublico(bruto);
  if (!alvo) return falha("destino recusado", 403);
  let tipo = classificar(alvo);
  if (!tipo) return falha("caminho recusado", 403);
  if (lido.get && tipo.modo !== "imagem") return falha("lista so por POST", 405);
  const host = alvo.host;   // host[:porta] — e o que vai para o log, e so

  const ctl = new AbortController();
  const relogio = setTimeout(() => ctl.abort(), PRAZO_MS);
  let r;
  try {
    for (let salto = 0; ; salto++) {
      r = await buscar(tipo.url.toString(), {
        method: "GET", redirect: "manual", signal: ctl.signal,
        headers: { "user-agent": UA, "accept": "*/*" },
      });
      if (r.status < 300 || r.status >= 400) break;
      const loc = r.headers.get("location");
      if (!loc || salto >= SALTOS) { console.log(`xtream ${host} ${tipo.acao} -> redirect demais`); return falha("redirect demais", 502); }
      // O salto passa pelo mesmo crivo, e o modo nao pode mudar: um
      // player_api que redireciona para /live/ nao vira proxy de video.
      const prox = destinoPublico(new URL(loc, tipo.url).toString());
      const t2 = prox && classificar(prox);
      if (!t2 || t2.modo !== tipo.modo) { console.log(`xtream ${host} ${tipo.acao} -> redirect recusado`); return falha("redirect recusado", 403); }
      tipo = t2;
    }
  } catch (e) {
    clearTimeout(relogio);
    const nome = e && e.name === "AbortError" ? "prazo" : "rede";
    console.log(`xtream ${host} ${tipo.acao} -> falha ${nome}`);
    return falha(nome === "prazo" ? "painel nao respondeu no prazo" : "painel inacessivel", 504);
  }

  console.log(`xtream ${host} ${tipo.acao} -> ${r.status}`);
  const ct = r.headers.get("content-type") || "";
  const max = tipo.modo === "lista" ? LISTA_MAX : IMAGEM_MAX;
  if (tipo.modo === "imagem" && r.ok && !/^image\//i.test(ct)) {
    clearTimeout(relogio);
    return falha("nao e imagem", 415);
  }
  const cl = Number(r.headers.get("content-length") || 0);
  if (cl > max) { clearTimeout(relogio); return falha("resposta grande demais", 502); }
  // O status do painel passa como esta (401/403/404/5xx): o cliente distingue
  // "credencial recusada" de "servidor fora" pelo corpo e pelo status, como
  // quando ia direto. O relogio continua valendo durante o corpo.
  const corpo = r.body ? limitar(r.body, max) : null;
  if (!corpo) clearTimeout(relogio);
  return new Response(corpo ? corpoComPrazo(corpo, relogio) : null, {
    status: r.status,
    headers: {
      "content-type": ct || (tipo.modo === "lista" ? "application/json" : "application/octet-stream"),
      "cache-control": "no-store",
      ...CORS,
    },
  });
}

// Solta o relogio quando o corpo termina (ou falha), para o abort nao
// disparar depois de a resposta ja ter saido inteira.
function corpoComPrazo(corpo, relogio) {
  return corpo.pipeThrough(new TransformStream({
    flush() { clearTimeout(relogio); },
  }));
}
