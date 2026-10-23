// MUSICAS DO TITULO (fase 1). GET /musica/v1/<imdb>?t=<titulo>&y=<ano>[&s=<temporada>]
// Sem sessao: so o id publico e o titulo. Trilha oficial = release-group
// "soundtrack" do MusicBrainz (CC0, 1 req/s, User-Agent obrigatorio); capa e
// link do album vem do iTunes Search. Resultado (inclusive "nao achei") fica em
// KV: 30 dias se achou, 7 se nao. Trilha errada e pior que nenhuma: so aceita
// nome que contem o titulo e ano a +-1 (o ano do titulo e obrigatorio).
// /m/ serve a pagina do celular (QR da TV): links Spotify, Apple e YouTube Music.

const ID_IMDB = /^tt\d{5,10}$/;
const UA = "NuvioLegacy/2.1 (https://github.com/nuvio-legacy)";
const MB = "https://musicbrainz.org/ws/2/";
export const CACHE_ACHOU = 30 * 86400, CACHE_VAZIO = 7 * 86400;
const FAIXAS_MAX = 60;

const resp = (o, status = 200, extra = {}) => new Response(JSON.stringify(o), {
  status, headers: { "content-type": "application/json; charset=utf-8", "access-control-allow-origin": "*", ...extra },
});

export const norm = (s) => String(s || "").normalize("NFD").replace(/[\u0300-\u036f]/g, "")
  .toLowerCase().replace(/[^a-z0-9]+/g, " ").trim();

const json = async (buscar, url) => {
  const r = await buscar(url, { headers: { accept: "application/json", "user-agent": UA } });
  if (!r.ok) throw new Error("http " + r.status);
  return r.json();
};

// Escolhe o release-group da trilha: titulo contem o do filme, ano +-1.
export function escolher(grupos, titulo, ano, temp) {
  const t = norm(titulo);
  let melhor = null;
  for (const g of grupos || []) {
    const n = norm(g.title);
    if (!n.includes(t) || (g.score ?? 0) < 90) continue;
    const a = parseInt(String(g["first-release-date"] || "").slice(0, 4), 10);
    if (ano && !(Number.isFinite(a) && Math.abs(a - ano) <= (temp ? 15 : 1))) continue;
    const eTemp = temp && new RegExp("(season|temporada|series) " + temp + "\\b").test(n);
    if (temp && !eTemp) continue;
    if (!melhor || (g.score ?? 0) > (melhor.score ?? 0)) melhor = g;
  }
  return melhor;
}

const artista = (ac) => (ac || []).map((c) => (c.name || "") + (c.joinphrase || "")).join("").trim();

async function procurar(buscar, titulo, ano, temp) {
  const termo = temp ? `${titulo} season ${temp}` : titulo;
  const q = `releasegroup:"${termo.replace(/["\\]/g, " ")}" AND secondarytype:soundtrack`;
  const d = await json(buscar, `${MB}release-group?query=${encodeURIComponent(q)}&limit=8&fmt=json`);
  return escolher(d["release-groups"], titulo, ano, temp);
}

async function capaApple(buscar, album, art) {
  try {
    const q = encodeURIComponent((album + " " + art).slice(0, 120));
    const d = await json(buscar, `https://itunes.apple.com/search?term=${q}&entity=album&limit=3`);
    const a = (d.results || []).find((x) => norm(x.collectionName).includes(norm(album).slice(0, 20)));
    if (!a) return {};
    const capa = String(a.artworkUrl100 || "").replace("100x100bb", "600x600bb");
    return { capa: capa.startsWith("https://") ? capa : "", apple: String(a.collectionViewUrl || "").startsWith("https://") ? a.collectionViewUrl : "" };
  } catch { return {}; }
}

export async function trilha(buscar, titulo, ano, temp) {
  const g = await procurar(buscar, titulo, ano, temp);
  if (!g) return null;
  const rel = await json(buscar, `${MB}release?release-group=${g.id}&inc=recordings+artist-credits&limit=1&fmt=json`);
  const r = rel.releases?.[0];
  const faixas = (r?.media || []).flatMap((m) => m.tracks || []).slice(0, FAIXAS_MAX).map((k, i) => ({
    n: i + 1, titulo: String(k.title || ""), artista: artista(k["artist-credit"]),
    dur: Math.round((k.length || 0) / 1000),
  })).filter((k) => k.titulo);
  if (!faixas.length) return null;
  const art = artista(g["artist-credit"]);
  const ap = await capaApple(buscar, g.title, art);
  return { album: { titulo: g.title, artista: art, ...ap }, faixas };
}

export async function rotaMusica(url, env, buscar = fetch, limite = null, ip = "0.0.0.0", agora = Date.now()) {
  const m = /^\/musica\/v1\/(tt\d+)$/.exec(url.pathname);
  const titulo = (url.searchParams.get("t") || "").slice(0, 120).trim();
  const ano = parseInt(url.searchParams.get("y") || "", 10) || 0;
  const temp = parseInt(url.searchParams.get("s") || "", 10) || 0;
  if (!m || !ID_IMDB.test(m[1]) || !titulo || /[\u0000-\u001f]/.test(titulo)) return resp({ erro: "pedido invalido" }, 400);
  const chave = `m2:${m[1]}:${temp}`;
  const guardado = await env?.MUS?.get(chave);
  if (guardado) return resp(JSON.parse(guardado), 200, { "cache-control": "public, max-age=3600" });
  if (limite && !(await limite(env, "musica:" + ip, 30, 600, Math.floor(agora / 1000)))) return resp({ erro: "muitos pedidos" }, 429);
  let t;
  try {
    const tipo = temp > 0 ? "series" : "movie";
    const d = await json(buscar, `https://v3-cinemeta.strem.io/meta/${tipo}/${m[1]}.json`);
    const nome = String(d.meta?.name || "").trim();
    const anoId = parseInt(d.meta?.year || d.meta?.releaseInfo || "", 10);
    if (!nome || !Number.isFinite(anoId)) throw new Error("missing canonical metadata");
    t = await trilha(buscar, nome, anoId, temp);
  } catch { return resp({ erro: "fonte indisponivel" }, 502); }
  const corpo = t || { vazio: true };
  await env?.MUS?.put(chave, JSON.stringify(corpo), { expirationTtl: t ? CACHE_ACHOU : CACHE_VAZIO });
  return resp(corpo, 200, { "cache-control": "public, max-age=3600" });
}

// Pagina do celular. O QR leva /m/#id=tt..&t=..&y=..&s=..; a pagina pergunta
// ao proprio worker e so monta links (open.spotify.com/search nao usa API).
export const PAGINA = `<!doctype html><html lang="pt"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Nuvio</title>
<style>body{margin:0;font:16px system-ui;background:#10131a;color:#eef}main{max-width:560px;margin:auto;padding:16px}h1{font-size:20px}li{list-style:none;padding:10px 0;border-bottom:1px solid #fff2}b{display:block}small{opacity:.7}a{color:#8cf;margin-right:14px;text-decoration:none;font-size:15px}img{width:120px;border-radius:12px}</style></head>
<body><main><h1 id="h">...</h1><div id="c"></div><ul id="l" style="padding:0"></ul></main>
<script>
var p=new URLSearchParams(location.hash.slice(1)),q=encodeURIComponent;
fetch("/musica/v1/"+q(p.get("id")||"x")+"?t="+q(p.get("t")||"")+"&y="+q(p.get("y")||"")+"&s="+q(p.get("s")||"")).then(function(r){return r.json()}).then(function(d){
var h=document.getElementById("h"),l=document.getElementById("l");
if(!d.faixas){h.textContent=p.get("t")||"";return}
h.textContent=d.album.titulo;
if(d.album.capa){var i=new Image();i.src=d.album.capa;document.getElementById("c").appendChild(i)}
d.faixas.forEach(function(k){var s=q(k.titulo+" "+k.artista),li=document.createElement("li"),b=document.createElement("b");
b.textContent=k.titulo;var sm=document.createElement("small");sm.textContent=k.artista;
li.appendChild(b);li.appendChild(sm);
[["Spotify","https://open.spotify.com/search/"+s],["Apple Music","https://music.apple.com/search?term="+s],["YouTube Music","https://music.youtube.com/search?q="+s]].forEach(function(x){var a=document.createElement("a");a.href=x[1];a.textContent=x[0];li.appendChild(a)});
l.appendChild(li)})}).catch(function(){document.getElementById("h").textContent="?"});
</script></body></html>`;

export const rotaPaginaMusica = () => new Response(PAGINA, {
  headers: { "content-type": "text/html; charset=utf-8", "cache-control": "public, max-age=3600", "referrer-policy": "no-referrer" },
});
