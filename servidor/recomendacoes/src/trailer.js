// TRAILER NA SAMSUNG (#136). Duas rotas sem sessao, como /v1/noticias: nada
// da pessoa passa por aqui, so o id publico de um video.
//
// /v1/trailer/imdb?id=tt123  A API GraphQL do IMDb responde 403 sem
//   `Referer: https://www.imdb.com/` e nao manda CORS para a origem de um wgt
//   (file://, "null"): o fetch da TV morre antes de sair. O MP4 que ela
//   devolve, esse sim, toca de qualquer lugar sem Referer (206 medido do Mac
//   em 24/09/2026) — so a PERGUNTA precisa passar por aqui. A resposta vai
//   crua (o cliente ja sabe ler, trailerimdb.c) e fica 6 h na borda: a URL
//   assinada vale ~7 dias.
//
// /v1/trailer/yt?id=XXXX&mute=1  Pagina que embute o player do YouTube. O
//   embed direto de um wgt vai sem Referer e sem origem valida, e o YouTube
//   recusa com "Video player configuration error" (erro 153). Aqui o iframe
//   do player nasce dentro de uma pagina https deste worker, com `origin` e
//   `widget_referrer` desta origem e Referrer-Policy que manda a origem. A
//   pagina so repassa postMessage nos dois sentidos (player <-> app), para a
//   IFrame API continuar dizendo `onReady`/`onStateChange`/`onError` ao app.

const ID_IMDB = /^tt\d{5,10}$/;
const ID_YT = /^[A-Za-z0-9_-]{6,20}$/;

const CORS = {
  "access-control-allow-origin": "*",
  "cross-origin-resource-policy": "cross-origin",
};

export function consultaImdb(id) {
  // A mesma consulta de src/trailerimdb.c, para a resposta ter o mesmo formato.
  const q = `{title(id:"${id}"){primaryVideos(first:1){edges{node{name{value}playbackURLs{url videoMimeType videoDefinition}}}}}}`;
  return "https://api.graphql.imdb.com/?query=" + encodeURIComponent(q);
}

export async function rotaTrailerImdb(url, buscar = fetch, cache = null) {
  const id = url.searchParams.get("id") || "";
  if (!ID_IMDB.test(id)) return new Response('{"erro":"id invalido"}', { status: 400, headers: { "content-type": "application/json", ...CORS } });
  const alvo = consultaImdb(id);
  const chave = cache ? new Request(alvo) : null;
  let r = cache ? await cache.match(chave) : null;
  if (!r) {
    let up;
    try {
      up = await buscar(alvo, { headers: { accept: "application/json", referer: "https://www.imdb.com/" } });
    } catch {
      return new Response('{"erro":"imdb inalcancavel"}', { status: 502, headers: { "content-type": "application/json", ...CORS } });
    }
    if (!up.ok) return new Response(`{"erro":"imdb ${up.status}"}`, { status: 502, headers: { "content-type": "application/json", ...CORS } });
    const corpo = (await up.text()).slice(0, 64 * 1024);
    r = new Response(corpo, { status: 200, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "public, max-age=21600" } });
    if (cache) await cache.put(chave, r.clone());
  }
  return new Response(r.body, { status: 200, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "public, max-age=21600", ...CORS } });
}

export function paginaYoutube(id, mute, origem) {
  const o = encodeURIComponent(origem);
  const src = "https://www.youtube.com/embed/" + id + "?autoplay=1&mute=" + (mute ? 1 : 0) +
    "&controls=0&enablejsapi=1&rel=0&modestbranding=1&playsinline=1&iv_load_policy=3&fs=0&disablekb=1" +
    "&origin=" + o + "&widget_referrer=" + o;
  return `<!doctype html><html><head><meta charset="utf-8"><meta name="referrer" content="strict-origin-when-cross-origin">
<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}iframe{position:absolute;inset:0;border:0;width:100%;height:100%}</style></head>
<body><iframe id="p" src="${src}" allow="autoplay; encrypted-media" referrerpolicy="strict-origin-when-cross-origin" tabindex="-1"></iframe>
<script>(function(){var p=document.getElementById('p');
window.addEventListener('message',function(e){
if(e.source===p.contentWindow){try{parent.postMessage(e.data,'*')}catch(x){}return}
if(e.source===parent&&typeof e.data==='string'){try{p.contentWindow.postMessage(e.data,'https://www.youtube.com')}catch(x){}}
});})();</script></body></html>`;
}

export function rotaTrailerYoutube(url) {
  const id = url.searchParams.get("id") || "";
  if (!ID_YT.test(id)) return new Response("id invalido", { status: 400, headers: { "content-type": "text/plain", ...CORS } });
  const mute = url.searchParams.get("mute") !== "0";
  return new Response(paginaYoutube(id, mute, url.origin), {
    status: 200,
    headers: {
      "content-type": "text/html; charset=utf-8",
      "cache-control": "public, max-age=86400",
      "referrer-policy": "strict-origin-when-cross-origin",
      "content-security-policy": "default-src 'none'; frame-src https://www.youtube.com; script-src 'unsafe-inline'; style-src 'unsafe-inline'",
    },
  });
}
