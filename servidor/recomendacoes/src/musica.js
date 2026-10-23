// MUSICAS DO TITULO. GET /musica/v1/<imdb>?t=<titulo>&y=<ano>[&s=<temporada>&country=us]
// Public iTunes by default; Apple Music catalog first only with all signing secrets.
// Canonical title/year prevent caller cache poisoning; films match year +-1.
// KV: 30 days for albums, seven days for confirmed misses, never for outages.
// /m/ serve a pagina do celular (QR da TV): links Spotify, Apple e YouTube Music.

const ID_IMDB = /^tt\d{5,10}$/;
const UA = "NuvioLegacy/2.1 (https://github.com/nuvio-legacy)";
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

// Deezer's public API needs no key and accepts Cloudflare egress.
async function deezer(buscar, titulo, ano, temp) {
  const term = encodeURIComponent(`${titulo}${temp ? ` season ${temp}` : ""} soundtrack`);
  const search = await json(buscar, `https://api.deezer.com/search/album?q=${term}&limit=25`);
  const restantes = (search.data || []).filter((a) => a.id && a.title &&
    !/\b(tribute|karaoke|cover|inspired)\b/i.test(a.artist?.name || ""));
  while (restantes.length) { // search hits carry no release year: match title, verify year per album
    const match = escolher(restantes.map((a) => ({ id: a.id, title: a.title, "secondary-types": ["Soundtrack"] })), titulo, 0, temp);
    if (!match) return null;
    const [alvo] = restantes.splice(restantes.findIndex((a) => a.id === match.id), 1);
    const d = await json(buscar, `https://api.deezer.com/album/${alvo.id}`);
    const anoAl = parseInt(d.release_date, 10);
    if (ano && !(Number.isFinite(anoAl) && Math.abs(anoAl - ano) <= (temp ? 15 : 1))) continue;
    const faixas = (d.tracks?.data || []).filter((k) => k.title).slice(0, FAIXAS_MAX)
      .map((k, i) => ({ n: k.track_position || i + 1, titulo: k.title,
        artista: k.artist?.name || d.artist?.name || "", dur: k.duration || 0,
        ...(https(k.preview) ? { previa: k.preview } : {}) }));
    if (!faixas.length) continue;
    return { album: { titulo: d.title, artista: d.artist?.name || alvo.artist?.name || "", ano: anoAl,
      genero: d.genres?.data?.[0]?.name || "Soundtrack", capa: https(d.cover_xl), deezer: https(d.link) }, faixas };
  }
  return null;
}

// MusicBrainz metadata is the last resort: no previews, 1 req/s (UA above).
let mbUntil = 0;
async function mbJson(buscar, url, agora) {
  const espera = Math.min(1100, Math.max(0, mbUntil - agora));
  mbUntil = Math.max(agora, mbUntil) + 1100;
  if (espera) await new Promise((r) => setTimeout(r, espera));
  return json(buscar, url);
}
async function musicbrainz(buscar, titulo, ano, temp, agora) {
  const q = encodeURIComponent(`"${titulo}${temp ? ` season ${temp}` : ""}" AND secondarytypes:Soundtrack`);
  const search = await mbJson(buscar, `https://musicbrainz.org/ws/2/release-group/?query=${q}&fmt=json&limit=25`, agora);
  const match = escolher((search["release-groups"] || []).map((g) => ({ id: g.id, title: g.title,
    "secondary-types": g["secondary-types"], "first-release-date": g["first-release-date"], score: g.score })), titulo, ano, temp);
  if (!match) return null;
  const rel = await mbJson(buscar, `https://musicbrainz.org/ws/2/release?release-group=${match.id}&fmt=json&inc=recordings+artist-credits`, agora);
  const release = (rel.releases || []).find((r) => r.media?.some((m) => m.tracks?.length));
  if (!release) return null;
  const artista = (c) => (c || []).map((a) => a.name).join("");
  const faixas = release.media.flatMap((m) => m.tracks || []).filter((k) => k.title).slice(0, FAIXAS_MAX)
    .map((k, i) => ({ n: Number(k.number) || i + 1, titulo: k.title,
      artista: artista(k["artist-credit"]) || artista(release["artist-credit"]),
      dur: Math.round((k.length || k.recording?.length || 0) / 1000) }));
  if (!faixas.length) return null;
  const capaUrl = `https://coverartarchive.org/release-group/${match.id}/front-500`;
  const capaRes = await buscar(capaUrl, { method: "HEAD", headers: { "user-agent": UA }, redirect: "manual" }).catch(() => null);
  const capa = capaRes && capaRes.status < 400 ? capaUrl : ""; // CAA answers 302 to archive.org when art exists.
  return { album: { titulo: match.title, artista: artista(release["artist-credit"]),
    ano: parseInt(match["first-release-date"], 10) || 0, genero: "Soundtrack", capa }, faixas };
}

// Provider search scores rank results; identity comes from title, type and year.
const nomeAlbum = (s) => {
  const n = norm(s).replace(/\b(?:temporada|series) (\d+)\b/g, "season $1");
  const season = n.match(/\bseason \d+\b/)?.[0];
  const title = n.replace(/\bseason \d+\b/g, "").replace(/\b(?:original (?:motion picture |television |tv |game )?(?:soundtrack|score)|music (?:from|of) (?:the |a )?(?:motion picture|film|television|tv|hbo series)|soundtrack|score|ost)\b.*$/, "").trim();
  return `${title}${season ? ` ${season}` : ""}`;
};
const https = (s) => typeof s === "string" && s.startsWith("https://") ? s : "";

// Exact soundtrack title (including season), year +-1 for films.
export function escolher(grupos, titulo, ano, temp) {
  const t = norm(titulo);
  let melhor = null;
  for (const g of grupos || []) {
    const n = norm(g.title);
    if (nomeAlbum(g.title) !== (temp ? `${t} season ${temp}` : t)) continue;
    if (!(g["secondary-types"] || []).some((type) => /^(soundtrack|score)$/.test(norm(type))) &&
        !/\b(soundtrack|score|ost|music from|music of)\b/.test(n)) continue;
    const a = parseInt(String(g["first-release-date"] || "").slice(0, 4), 10);
    if (ano && !(Number.isFinite(a) && Math.abs(a - ano) <= (temp ? 15 : 1))) continue;
    const eTemp = temp && new RegExp("(season|temporada|series) " + temp + "\\b").test(n);
    if (temp && !eTemp) continue;
    if (!melhor || (g.score ?? 0) > (melhor.score ?? 0)) melhor = g;
  }
  return melhor;
}

// ponytail: per-isolate budget; use a shared limiter if cross-isolate traffic requires it.
const states = new WeakMap();
function stateFor(env) {
  const owner = env.MUS || env;
  if (!states.has(owner)) states.set(owner, { pending: new Map(), start: 0, count: 0, until: 0 });
  return states.get(owner);
}
async function itunes(buscar, path, state, agora) {
  if (agora < state.until) throw new Error("itunes backoff");
  if (agora - state.start >= 60000) { state.start = agora; state.count = 0; }
  if (state.count >= 20) { state.until = state.start + 60000; throw new Error("itunes backoff"); }
  state.count++;
  const r = await buscar(`https://itunes.apple.com/${path}`, { headers: { accept: "application/json" } });
  if (r.status === 403 || r.status === 429) {
    const retry = r.headers.get("retry-after");
    const delay = /^\d+$/.test(retry || "") ? Number(retry) * 1000 : Date.parse(retry) - agora;
    state.until = agora + Math.max(60000, Number.isFinite(delay) ? delay : 0);
    throw new Error("itunes backoff");
  }
  if (!r.ok) throw new Error("http " + r.status);
  return r.json();
}

let developerToken;
const appleConfigured = (env) => env?.APPLE_MUSIC_TEAM_ID && env?.APPLE_MUSIC_KEY_ID && env?.APPLE_MUSIC_P8;
const base64url = (bytes) => btoa(String.fromCharCode(...bytes)).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");

async function tokenApple(env, agora) {
  const { APPLE_MUSIC_TEAM_ID: team, APPLE_MUSIC_KEY_ID: kid, APPLE_MUSIC_P8: pem } = env;
  if (developerToken?.team === team && developerToken.kid === kid && developerToken.pem === pem && developerToken.exp > agora + 60) return developerToken.token;
  const bytes = Uint8Array.from(atob(pem.replace(/-----[^-]+-----|\s/g, "")), (c) => c.charCodeAt(0));
  const key = await crypto.subtle.importKey("pkcs8", bytes, { name: "ECDSA", namedCurve: "P-256" }, false, ["sign"]);
  const encode = (value) => base64url(new TextEncoder().encode(JSON.stringify(value)));
  const exp = agora + 12 * 3600;
  const body = `${encode({ alg: "ES256", kid })}.${encode({ iss: team, iat: agora, exp })}`;
  const signature = await crypto.subtle.sign({ name: "ECDSA", hash: "SHA-256" }, key, new TextEncoder().encode(body));
  const token = `${body}.${base64url(new Uint8Array(signature))}`;
  developerToken = { team, kid, pem, exp, token };
  return token;
}

async function catalogoApple(buscar, env, titulo, ano, temp, country, agora) {
  if (!appleConfigured(env)) return null;
  try {
    const token = await tokenApple(env, Math.floor(agora / 1000));
    const get = async (path) => {
      const r = await buscar(`https://api.music.apple.com/v1/catalog/${country}/${path}`, {
        headers: { accept: "application/json", authorization: `Bearer ${token}` },
      });
      if (!r.ok) throw new Error("Apple catalog unavailable");
      return r.json();
    };
    const term = `${titulo}${temp ? ` season ${temp}` : ""} soundtrack`;
    const search = await get(`search?types=albums&limit=25&term=${encodeURIComponent(term)}`);
    const albums = search.results?.albums?.data || [];
    const match = escolher(albums.map(({ id, attributes: a }) => ({
      id, title: a?.name, "first-release-date": a?.releaseDate,
      "secondary-types": a?.genreNames?.some((g) => /soundtrack|score/i.test(g)) ? ["Soundtrack"] : [],
    })), titulo, ano, temp);
    if (!match) return null;
    const result = await get(`albums/${encodeURIComponent(match.id)}?include=tracks`);
    const album = result.data?.[0], a = album?.attributes;
    if (!a) return null;
    const faixas = (album.relationships?.tracks?.data || []).filter((k) => k.type === "songs" && k.attributes?.name)
      .slice(0, FAIXAS_MAX).map(({ attributes: k }, i) => ({
        n: i + 1, titulo: k.name, artista: k.artistName || a.artistName,
        dur: Math.round((k.durationInMillis || 0) / 1000),
        ...(https(k.previews?.[0]?.url) ? { previa: k.previews[0].url } : {}),
      }));
    if (!faixas.length) return null;
    const cores = Object.fromEntries(["bgColor", "textColor1", "textColor2", "textColor3", "textColor4"]
      .filter((key) => /^[a-f0-9]{6}$/i.test(a.artwork?.[key] || "")).map((key) => [key, a.artwork[key]]));
    return { album: { titulo: a.name, artista: a.artistName, ano: parseInt(a.releaseDate, 10),
      genero: (a.genreNames || []).join(", "), nota: a.editorialNotes?.short || "",
      capa: https(a.artwork?.url).replace(/\{w\}|\{h\}/g, "600"), cores,
      apple: https(a.url), appleUrl: https(a.url) }, faixas };
  } catch { return null; } // Optional provider: never log signing material or tokens.
}

export async function trilha(buscar, titulo, ano, temp, country = "br", state = stateFor(buscar), agora = Date.now()) {
  for (const store of new Set([country, "us"])) {
    const term = encodeURIComponent(`${titulo}${temp ? ` season ${temp}` : ""} soundtrack`);
    const search = await itunes(buscar, `search?term=${term}&entity=album&limit=25&country=${store}`, state, agora);
    const albums = (search.results || []).filter((a) => a.collectionId && !/\b(tribute|karaoke|cover|inspired)\b/i.test(a.artistName || ""));
    const match = escolher(albums.map((a) => ({ id: a.collectionId, title: a.collectionName,
      "first-release-date": a.releaseDate, "secondary-types": [a.primaryGenreName] })), titulo, ano, temp);
    if (!match) continue;
    const a = albums.find((a) => a.collectionId === match.id);
    const songs = await itunes(buscar, `lookup?id=${encodeURIComponent(a.collectionId)}&entity=song&limit=200&country=${store}`, state, agora);
    const faixas = (songs.results || []).filter((s) => s.kind === "song" && s.collectionId === a.collectionId && s.trackName)
      .sort((a, b) => (a.discNumber || 1) - (b.discNumber || 1) || a.trackNumber - b.trackNumber)
      .slice(0, FAIXAS_MAX).map((s, i) => ({ n: i + 1, titulo: s.trackName, artista: s.artistName || a.artistName,
        dur: Math.round((s.trackTimeMillis || 0) / 1000),
        ...(https(s.previewUrl) ? { previa: s.previewUrl } : {}),
        ...(https(s.trackViewUrl) ? { appleUrl: s.trackViewUrl } : {}),
      }));
    if (!faixas.length) continue;
    return { album: { titulo: a.collectionName, artista: a.artistName, ano: parseInt(a.releaseDate, 10),
      genero: a.primaryGenreName || "Soundtrack", capa: https(a.artworkUrl100).replace(/\d+x\d+bb/, "600x600bb"),
      apple: https(a.collectionViewUrl), appleUrl: https(a.collectionViewUrl) }, faixas };
  }
  return null;
}

export async function rotaMusica(url, env, buscar = fetch, limite = null, ip = "0.0.0.0", agora = Date.now()) {
  const m = /^\/musica\/v1\/(tt\d+)$/.exec(url.pathname);
  const titulo = (url.searchParams.get("t") || "").slice(0, 120).trim();
  const ano = parseInt(url.searchParams.get("y") || "", 10) || 0;
  const temp = parseInt(url.searchParams.get("s") || "", 10) || 0;
  if (!m || !ID_IMDB.test(m[1]) || !titulo || /[\u0000-\u001f]/.test(titulo)) return resp({ erro: "pedido invalido" }, 400);
  const requestedCountry = (url.searchParams.get("country") || "br").toLowerCase();
  const country = /^[a-z]{2}$/.test(requestedCountry) ? requestedCountry : "br";
  const chave = `m6:${m[1]}:${temp}:${country}:${appleConfigured(env) ? "apple" : "itunes"}`;
  const guardado = await env?.MUS?.get(chave);
  if (guardado) return resp(JSON.parse(guardado), 200, { "cache-control": "public, max-age=3600" });
  if (limite && !(await limite(env, "musica:" + ip, 30, 600, Math.floor(agora / 1000)))) return resp({ erro: "muitos pedidos" }, 429);
  const state = stateFor(env);
  if (!state.pending.has(chave)) state.pending.set(chave, (async () => {
    try {
      const tipo = temp > 0 ? "series" : "movie";
      const d = await json(buscar, `https://v3-cinemeta.strem.io/meta/${tipo}/${m[1]}.json`);
      const nome = String(d.meta?.name || "").trim();
      const anoId = parseInt(d.meta?.year || d.meta?.releaseInfo || "", 10);
      if (!nome || !Number.isFinite(anoId)) throw new Error("missing canonical metadata");
      let t = await catalogoApple(buscar, env, nome, anoId, temp, country, agora);
      let falhou = false; // A provider that errored or backed off answered nothing; its silence is not an empty catalog.
      if (!t) try { t = await trilha(buscar, nome, anoId, temp, country, state, agora); }
        catch (error) { if (error.message !== "itunes backoff") throw error; falhou = true; }
      if (!t) try { t = await deezer(buscar, nome, anoId, temp); } catch { falhou = true; }
      if (!t) try { t = await musicbrainz(buscar, nome, anoId, temp, agora); } catch { falhou = true; }
      if (falhou && !t) return { corpo: { vazio: true }, status: 200, cache: "no-store" };
      const corpo = t || { vazio: true };
      await env?.MUS?.put(chave, JSON.stringify(corpo), { expirationTtl: t ? CACHE_ACHOU : CACHE_VAZIO });
      return { corpo, status: 200, cache: "public, max-age=3600" };
    } catch (error) {
      return { corpo: { erro: "fonte indisponivel" }, status: 502, cache: "no-store" };
    }
  })());
  try {
    const { corpo, status, cache } = await state.pending.get(chave);
    return resp(corpo, status, { "cache-control": cache });
  } finally { state.pending.delete(chave); }
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
