// Offline fixtures only. Run from the worktree root:
// node servidor/recomendacoes/teste-musica.mjs
import assert from "node:assert/strict";
import { webcrypto } from "node:crypto";
globalThis.crypto ||= webcrypto;
import { rotaMusica, rotaPaginaMusica, escolher, CACHE_ACHOU, CACHE_VAZIO } from "./src/musica.js";

const J = (body) => new Response(JSON.stringify(body));
const U = (query = "", id = "tt0816692") => new URL(`https://fixture.test/musica/v1/${id}?t=Interstellar&y=2014${query}`);
const kv = () => {
  const m = new Map(), ttl = [];
  return { m, ttl, get: async (k) => m.get(k) ?? null, put: async (k, v, o) => { m.set(k, v); ttl.push(o.expirationTtl); } };
};
const album = { collectionId: 10, collectionName: "Interstellar (Original Motion Picture Soundtrack)",
  artistName: "Hans Zimmer", releaseDate: "2014-11-17", primaryGenreName: "Soundtrack",
  artworkUrl100: "https://is1.mzstatic.com/art/100x100bb.jpg", collectionViewUrl: "https://music.apple.com/br/album/10" };
const preview = "https://audio-ssl.itunes.apple.com/Cornfield.m4a";
const song = { collectionId: 10, kind: "song", trackName: "Cornfield Chase", artistName: "Hans Zimmer",
  trackNumber: 2, trackTimeMillis: 126000, previewUrl: preview, trackViewUrl: "https://music.apple.com/br/song/12" };
const dzAlbum = { id: 10, title: album.collectionName, artist: { name: "Hans Zimmer" } };
const dzTrack = { title: "Cornfield Chase", track_position: 1, duration: 126,
  preview: "https://cdns-preview.dzcdn.net/preview.mp3", artist: { name: "Hans Zimmer" } };
const fixture = (mode = "ok", calls = []) => async (target, options) => {
  const u = new URL(target); calls.push({ u, options });
  if (u.hostname === "v3-cinemeta.strem.io") return J({ meta: { name: "Interstellar", year: "2014" } });
  if (u.hostname === "api.deezer.com") {
    assert.ok(!options?.headers?.authorization, "Deezer needs no authorization");
    if (mode === "dzfail") return new Response("bad gateway", { status: 502 });
    if (u.pathname === "/search/album") return J({ data: mode === "dz" ? [dzAlbum] : [] });
    assert.equal(u.pathname, "/album/10");
    return J({ ...dzAlbum, release_date: album.releaseDate, cover_xl: "https://e-cdns.dzcdn.net/cover_xl.jpg",
      link: "https://www.deezer.com/album/10", genres: { data: [{ name: "Soundtrack" }] },
      tracks: { data: [dzTrack, { ...dzTrack, title: "No Preview", preview: "javascript:bad", track_position: 2 }] } });
  }
  if (u.hostname === "musicbrainz.org") {
    assert.match(options?.headers?.["user-agent"] || "", /^NuvioLegacy\//, "MusicBrainz requires a User-Agent");
    if (u.pathname === "/ws/2/release-group/") return J({ "release-groups": mode === "mb" || mode === "dzfail" ? [
      { id: "rg1", title: album.collectionName, "secondary-types": ["Soundtrack"],
        "first-release-date": album.releaseDate, score: 100 }] : [] });
    assert.match(u.pathname, /^\/ws\/2\/release$/);
    return J({ releases: [{ "artist-credit": [{ name: "Hans Zimmer" }], media: [{ tracks: [
      { number: "1", title: "Dreaming of the Crash", length: 180000, "artist-credit": [{ name: "Hans Zimmer" }] }] }] }] });
  }
  if (u.hostname === "coverartarchive.org") return new Response("found", { status: 302 });
  assert.equal(u.hostname, "itunes.apple.com", "no secrets: only public iTunes catalog");
  assert.ok(!options?.headers?.authorization, "iTunes needs no authorization");
  if (mode === "offline") throw new Error("offline");
  if (mode === "403" || mode === "429" || mode === "dz" || mode === "mb" || mode === "dzfail") return new Response("throttled", { status: 403, headers: { "retry-after": "120" } });
  if (u.pathname === "/search") return J({ results: mode === "empty" || (mode === "us" && u.searchParams.get("country") !== "us") ? [] : [
    { ...album, collectionId: 9, artistName: "Tribute Orchestra" },
    { ...album, collectionId: 8, releaseDate: "1999-01-01" },
    { ...album, collectionId: 7, collectionName: "Not Interstellar (Score)" }, album,
  ] });
  assert.equal(u.pathname, "/lookup");
  assert.equal(u.searchParams.get("id"), "10");
  return J({ results: [album, song,
    { ...song, trackNumber: 1, trackName: "Dreaming of the Crash", previewUrl: "https://audio-ssl.itunes.apple.com/Crash.m4a" },
    { ...song, trackNumber: 3, trackName: "Dust", previewUrl: "javascript:bad" },
    { ...song, trackNumber: 4, trackName: "Day One", previewUrl: undefined },
    { ...song, collectionId: 99, trackName: "Wrong album", previewUrl: preview },
  ] });
};
const clock = 1800000000000;
const calls = [], env = { MUS: kv() };
env.MUS.m.set("m4:tt0816692:0:br:mb", JSON.stringify({ vazio: true }));
let response = await rotaMusica(U(), env, fixture("ok", calls), null, "fixture", clock);
assert.equal(response.status, 200);
const data = await response.json();
assert.equal(data.album.capa, "https://is1.mzstatic.com/art/600x600bb.jpg");
assert.equal(data.album.artista, "Hans Zimmer");
assert.equal(data.album.ano, 2014);
assert.equal(data.album.genero, "Soundtrack");
assert.equal(data.album.appleUrl, album.collectionViewUrl);
assert.equal(data.faixas.length, 4);
assert.equal(data.faixas[0].titulo, "Dreaming of the Crash");
assert.match(data.faixas[0].previa, /Crash\.m4a$/);
assert.equal(data.faixas[1].previa, preview, "preview belongs to same returned track");
assert.equal(data.faixas[1].dur, 126);
assert.equal(data.faixas[1].appleUrl, song.trackViewUrl);
assert.ok(data.faixas.slice(2).every((s) => !s.previa));
assert.equal(calls[1].u.searchParams.get("country"), "br");
assert.deepEqual(env.MUS.ttl, [CACHE_ACHOU]);
await rotaMusica(U(), env, fixture("ok", calls), null, "fixture", clock);
assert.equal(calls.length, 3, "KV hit makes no upstream calls");
console.log("ok public iTunes default, Interstellar, track identity, safe URLs, 600px cover, 30-day cache");

const groups = [{ id: "score", title: "Interstellar", "secondary-types": ["Score"], "first-release-date": "2015" }];
assert.ok(escolher(groups, "Interstellar", 2014, 0));
for (const year of [2013, 2015]) assert.ok(escolher([{ ...groups[0], "first-release-date": String(year) }], "Interstellar", 2014, 0));
for (const title of ["Interstellar: Original Motion Picture Soundtrack", "Interstellar (Original Score)", "Interstellar (Music from the Motion Picture)"]) {
  assert.ok(escolher([{ ...groups[0], title }], "Interstellar", 2014, 0));
}
for (const group of [{ ...groups[0], title: "Not Interstellar" }, { ...groups[0], "first-release-date": "2012" }]) {
  assert.equal(escolher([group], "Interstellar", 2014, 0), null);
}
assert.ok(escolher([{ title: "Show (Music from the HBO Series, Season 2)", "first-release-date": "2020" }], "Show", 2019, 2));
assert.equal(escolher([{ title: "Show (Music from the HBO Series, Season 1)", "first-release-date": "2020" }], "Show", 2019, 2), null);

const usCalls = [], usEnv = { MUS: kv() };
assert.ok((await (await rotaMusica(U(), usEnv, fixture("us", usCalls))).json()).faixas);
assert.deepEqual(usCalls.filter((c) => c.u.hostname === "itunes.apple.com").map((c) => c.u.searchParams.get("country")), ["br", "us", "us"]);
const explicitCalls = [];
await rotaMusica(U("&country=GB"), { MUS: kv() }, fixture("ok", explicitCalls));
assert.equal(explicitCalls[1].u.searchParams.get("country"), "gb");
const emptyEnv = { MUS: kv() };
assert.deepEqual(await (await rotaMusica(U(), emptyEnv, fixture("empty"))).json(), { vazio: true });
assert.deepEqual(emptyEnv.MUS.ttl, [CACHE_VAZIO]);
const hostile = U(); hostile.searchParams.set("t", "Unrelated"); hostile.searchParams.set("y", "1999");
assert.equal((await (await rotaMusica(hostile, { MUS: kv() }, fixture())).json()).album.titulo, album.collectionName);
const concurrentCalls = [], concurrentEnv = { MUS: kv() }, source = fixture("ok", concurrentCalls);
const concurrent = await Promise.all(Array.from({ length: 8 }, () => rotaMusica(U(), concurrentEnv, source)));
assert.equal(concurrentCalls.length, 3, "same lookup coalesces concurrent requests");
assert.equal(concurrentEnv.MUS.ttl.length, 1);
for (const result of concurrent) assert.ok((await result.json()).faixas);
console.log("ok title/year/season matching, canonical metadata, storefront fallback, seven-day miss, concurrent deduplication");

for (const status of ["403", "429"]) {
  const throttleEnv = { MUS: kv() }, throttleCalls = [], throttled = fixture(status, throttleCalls);
  const first = await rotaMusica(U(), throttleEnv, throttled, null, "fixture", clock);
  assert.deepEqual(await first.json(), { vazio: true });
  assert.equal(first.headers.get("cache-control"), "no-store");
  await rotaMusica(U(), throttleEnv, throttled, null, "fixture", clock + 1000);
  await rotaMusica(U(), throttleEnv, throttled, null, "fixture", clock + 61000);
  assert.equal(throttleCalls.filter((c) => c.u.hostname === "itunes.apple.com").length, 1, "Retry-After respected");
  assert.deepEqual(throttleEnv.MUS.ttl, [], "transient throttle never cached as seven-day miss");
  const recovered = await rotaMusica(U(), throttleEnv, fixture(), null, "fixture", clock + 120001);
  assert.ok((await recovered.json()).faixas);
  throttleEnv.MUS.m.set("m6:tt0816693:0:br:itunes", JSON.stringify(data));
  await rotaMusica(U("", "tt0816694"), throttleEnv, throttled, null, "fixture", clock + 120002);
  const cached = await rotaMusica(U("", "tt0816693"), throttleEnv, throttled, null, "fixture", clock + 120003);
  assert.deepEqual(await cached.json(), data, "cached response remains available during backoff");
}
// Fallback chain: iTunes 403 backoff, Deezer answers with preview mp3s.
const dzEnv = { MUS: kv() }, dzCalls = [];
const dzResp = await rotaMusica(U(), dzEnv, fixture("dz", dzCalls), null, "fixture", clock);
const dzData = await dzResp.json();
assert.equal(dzData.album.titulo, album.collectionName);
assert.equal(dzData.album.capa, "https://e-cdns.dzcdn.net/cover_xl.jpg");
assert.equal(dzData.faixas.length, 2);
assert.match(dzData.faixas[0].previa, /preview\.mp3$/);
assert.ok(!dzData.faixas[1].previa, "unsafe preview dropped");
assert.deepEqual(dzEnv.MUS.ttl, [CACHE_ACHOU], "Deezer hit caches 30 days");
assert.ok(dzCalls.some((c) => c.u.hostname === "api.deezer.com"));
assert.equal(dzCalls.filter((c) => c.u.hostname === "musicbrainz.org").length, 0, "no MusicBrainz when Deezer answers");
console.log("ok iTunes backoff -> Deezer fallback, previews, 30-day cache");

// Deezer outage: MusicBrainz still answers, metadata only, rate-limited.
const mbEnv = { MUS: kv() }, mbCalls = [];
const mbData = await (await rotaMusica(U(), mbEnv, fixture("mb", mbCalls), null, "fixture", clock)).json();
assert.equal(mbData.album.titulo, album.collectionName);
assert.equal(mbData.album.capa, "https://coverartarchive.org/release-group/rg1/front-500");
assert.equal(mbData.faixas[0].titulo, "Dreaming of the Crash");
assert.equal(mbData.faixas[0].dur, 180);
assert.ok(!mbData.faixas[0].previa && !mbData.faixas[0].appleUrl);
assert.deepEqual(mbEnv.MUS.ttl, [CACHE_ACHOU]);

// Deezer HTTP error + iTunes backoff + MusicBrainz answers: still tracks.
const dzfEnv = { MUS: kv() };
const dzfData = await (await rotaMusica(U(), dzfEnv, fixture("dzfail"), null, "fixture", clock)).json();
assert.equal(dzfData.faixas[0].titulo, "Dreaming of the Crash");
assert.deepEqual(dzfEnv.MUS.ttl, [CACHE_ACHOU]);

// Every provider answered empty: confirmed miss still caches seven days.
const allEmptyEnv = { MUS: kv() };
assert.deepEqual(await (await rotaMusica(U(), allEmptyEnv, fixture("empty"))).json(), { vazio: true });
assert.deepEqual(allEmptyEnv.MUS.ttl, [CACHE_VAZIO]);
console.log("ok MusicBrainz last resort, provider outage chain, confirmed miss cache");

const budgetEnv = { MUS: kv() }, budgetCalls = [], budgetSource = fixture("ok", budgetCalls);
for (let i = 0; i < 11; i++) await rotaMusica(U("", `tt0816${String(i).padStart(3, "0")}`), budgetEnv, budgetSource, null, "fixture", clock);
assert.equal(budgetCalls.filter((c) => c.u.hostname === "itunes.apple.com").length, 20, "20 upstream requests per minute per isolate");
assert.equal((await rotaMusica(U(), { MUS: kv() }, fixture("offline"))).status, 502);
assert.equal((await rotaMusica(U(), { MUS: kv() }, fixture(), async () => false)).status, 429);
for (const path of ["/musica/v1/x?t=x", "/musica/v1/tt1?t=x", "/musica/v1/tt0816692", "/musica/v1/tt0816692?t=%00x"]) {
  assert.equal((await rotaMusica(new URL("https://fixture.test" + path), { MUS: kv() }, () => { throw new Error("unexpected request"); })).status, 400);
}
console.log("ok 403/429 backoff, cache during throttle, budget, outages, validation and caller limit");

const keys = await crypto.subtle.generateKey({ name: "ECDSA", namedCurve: "P-256" }, true, ["sign", "verify"]);
const p8 = Buffer.from(await crypto.subtle.exportKey("pkcs8", keys.privateKey)).toString("base64");
const appleEnv = { MUS: kv(), APPLE_MUSIC_TEAM_ID: "fixture-team", APPLE_MUSIC_KEY_ID: "fixture-key",
  APPLE_MUSIC_P8: `-----BEGIN PRIVATE KEY-----\n${p8}\n-----END PRIVATE KEY-----` };
const tokens = [], stores = [], appleCalls = [];
const catalog = (mode = "ok") => async (target, options) => {
  const u = new URL(target); appleCalls.push(u.hostname);
  if (u.hostname !== "api.music.apple.com") return fixture()(target, options);
  if (mode === "outage") return new Response("unavailable", { status: 503 });
  const token = options.headers.authorization.replace("Bearer ", ""); tokens.push(token); stores.push(u.pathname.split("/")[3]);
  const [header, payload, signature] = token.split(".");
  assert.deepEqual(JSON.parse(Buffer.from(header, "base64url")), { alg: "ES256", kid: "fixture-key" });
  const claims = JSON.parse(Buffer.from(payload, "base64url"));
  assert.equal(claims.iss, "fixture-team"); assert.equal(claims.exp - claims.iat, 12 * 3600);
  assert.ok(await crypto.subtle.verify({ name: "ECDSA", hash: "SHA-256" }, keys.publicKey,
    Buffer.from(signature, "base64url"), new TextEncoder().encode(`${header}.${payload}`)));
  const attributes = { name: album.collectionName, artistName: album.artistName, releaseDate: album.releaseDate,
    genreNames: ["Soundtrack"], url: album.collectionViewUrl,
    artwork: { url: "https://is1.mzstatic.com/art/{w}x{h}bb.jpg", bgColor: "152c38", textColor1: "ffffff" },
    editorialNotes: { short: "A journey beyond the stars." } };
  if (u.pathname.endsWith("/search")) return J({ results: { albums: { data: [
    { id: "wrong", attributes: { ...attributes, name: "Not Interstellar (Soundtrack)" } }, { id: "10", attributes },
  ] } } });
  assert.match(u.pathname, /\/albums\/10$/);
  return J({ data: [{ attributes, relationships: { tracks: { data: mode === "empty" ? [] : [
    { type: "songs", attributes: { name: "Cornfield Chase", artistName: "Hans Zimmer", durationInMillis: 126000, previews: [{ url: preview }] } },
  ] } } }] });
};
const apple = await (await rotaMusica(U(), appleEnv, catalog(), null, "fixture", clock)).json();
assert.equal(apple.album.capa, data.album.capa); assert.equal(apple.album.cores.bgColor, "152c38");
assert.equal(apple.album.nota, "A journey beyond the stars."); assert.equal(apple.faixas[0].previa, preview);
assert.deepEqual(appleCalls, ["v3-cinemeta.strem.io", "api.music.apple.com", "api.music.apple.com"], "secrets: Apple catalog first, no iTunes on success");
assert.ok(!JSON.stringify(apple).includes(tokens[0]));
await rotaMusica(U("&country=us"), appleEnv, catalog(), null, "fixture", clock + 1000);
assert.deepEqual(stores, ["br", "br", "us", "us"]); assert.equal(tokens[0], tokens[2]);
appleEnv.MUS = kv();
await rotaMusica(U("&country=invalid"), appleEnv, catalog(), null, "fixture", clock + 13 * 3600000);
assert.equal(stores.at(-1), "br"); assert.notEqual(tokens.at(-1), tokens[0]);
for (const mode of ["outage", "empty"]) {
  const fallback = await rotaMusica(U(), { ...appleEnv, MUS: kv() }, catalog(mode));
  assert.equal((await fallback.json()).faixas.length, 4, `${mode} falls back to public iTunes`);
}
const badKey = await rotaMusica(U(), { ...appleEnv, MUS: kv(), APPLE_MUSIC_P8: "invalid" }, fixture());
assert.equal((await badKey.json()).faixas.length, 4);
for (const partial of [{ APPLE_MUSIC_TEAM_ID: "team" }, { APPLE_MUSIC_KEY_ID: "key", APPLE_MUSIC_P8: "invalid" }]) {
  assert.ok((await (await rotaMusica(U(), { MUS: kv(), ...partial }, fixture())).json()).faixas);
}
console.log("ok dormant Apple API, secret-gated priority, ES256, token renewal, storefront isolation, iTunes fallback");
const page = rotaPaginaMusica();
assert.equal(page.headers.get("referrer-policy"), "no-referrer");
const html = await page.text();
assert.ok(["open.spotify.com/search", "music.apple.com/search", "music.youtube.com/search"].every((s) => html.includes(s)));
console.log("ok phone page store links; all music checks passed");
