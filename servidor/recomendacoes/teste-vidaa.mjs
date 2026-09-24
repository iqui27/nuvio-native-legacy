// Teste da parte VIDAA do worker (#125): a hospedagem estatica da TV
// (rotaTv em index.js) e o proxy generico (rotaProxy em proxy.js). Em Node
// puro, sem wrangler e sem rede — o mesmo estilo de teste-xtream.mjs: um
// `buscar`/`env.ASSETS.fetch` de mentira que devolve o que o teste pede.
//
//   node servidor/recomendacoes/teste-vidaa.mjs
import assert from "node:assert/strict";
import worker from "./src/index.js";
import { rotaProxy } from "./src/proxy.js";

const logs = [];
console.log = (...a) => logs.push(a.join(" "));

// --- duble de env.ASSETS (Workers Static Assets) -----------------------------
function fakeAssets(arquivos) {
  return {
    async fetch(req) {
      const u = new URL(typeof req === "string" ? req : req.url);
      const f = arquivos[u.pathname];
      if (!f) return new Response("nao encontrado", { status: 404 });
      return new Response(f.body ?? "", { status: f.status ?? 200, headers: f.headers ?? {} });
    },
  };
}

// --- duble minimo de D1, so o suficiente para /v1/rec (sessao em cache) -----
function fakeDB() {
  const pessoa = { id: "nuvio:abc123", nome: "Ana", codigo: "az9x7q", avatar: "", descobrivel: 0 };
  return {
    prepare(sql) {
      const api = {
        bind(...a) { api._b = a; return api; },
        async first() {
          if (sql.includes("FROM sessao")) return { id: pessoa.id, nome: pessoa.nome };
          if (sql.includes("FROM pessoa WHERE id")) return { ...pessoa };
          return null;
        },
        async all() {
          if (sql.includes("FROM rec r LEFT JOIN pessoa")) return { results: [] };
          return { results: [] };
        },
        async run() { return { meta: { last_row_id: 1, changes: 1 } }; },
      };
      return api;
    },
    batch: async (arr) => Promise.all(arr.map((p) => p.run())),
  };
}

const W = "https://w.test";

// === /tv: redirecionamento para a versao atual ===============================
{
  const env = { ASSETS: fakeAssets({ "/tv/versao.txt": { body: "1.4.3\n" } }) };
  const r = await worker.fetch(new Request(W + "/tv"), env);
  assert.equal(r.status, 302);
  assert.equal(r.headers.get("location"), W + "/tv/1.4.3/mt/");
  const r2 = await worker.fetch(new Request(W + "/tv/"), env);
  assert.equal(r2.status, 302);
  console.error("ok  /tv: 302 para /tv/<versao>/mt/");
}

// === /tv sem ASSETS (ambiente sem [assets] configurado): 404 limpo ==========
{
  const r = await worker.fetch(new Request(W + "/tv/1.4.3/mt/index.html"), {});
  assert.equal(r.status, 404);
  console.error("ok  /tv sem binding ASSETS: 404 limpo, sem excecao");
}

// === /tv/<v>/mt/*: COOP/COEP/CORP + tipos + cache ============================
{
  const env = {
    ASSETS: fakeAssets({
      "/tv/1.4.3/mt/index.html": { body: "<html></html>", headers: { "content-type": "text/html" } },
      "/tv/1.4.3/mt/index.wasm": { body: "\0asm", headers: { "content-type": "application/octet-stream" } },
      "/tv/1.4.3/mt/index.data": { body: "dados", headers: { "content-type": "application/octet-stream" } },
    }),
  };
  let r = await worker.fetch(new Request(W + "/tv/1.4.3/mt/index.html"), env);
  assert.equal(r.status, 200);
  assert.equal(r.headers.get("cross-origin-opener-policy"), "same-origin");
  assert.equal(r.headers.get("cross-origin-embedder-policy"), "credentialless");
  assert.equal(r.headers.get("cross-origin-resource-policy"), "same-origin");
  assert.equal(r.headers.get("cache-control"), "no-cache", "index.html nunca e imutavel");

  r = await worker.fetch(new Request(W + "/tv/1.4.3/mt/index.wasm"), env);
  assert.equal(r.headers.get("content-type"), "application/wasm");
  assert.equal(r.headers.get("cache-control"), "public, max-age=31536000, immutable");
  assert.equal(r.headers.get("cross-origin-embedder-policy"), "credentialless");

  r = await worker.fetch(new Request(W + "/tv/1.4.3/mt/index.data"), env);
  assert.equal(r.headers.get("content-type"), "application/octet-stream");
  console.error("ok  /tv/<v>/mt/*: COOP/COEP/CORP, .wasm/.data certos, cache imutavel exceto index.html");
}

// === /tv/<v>/st/*: SEM COOP/COEP =============================================
{
  const env = {
    ASSETS: fakeAssets({
      "/tv/1.4.3/st/index.html": { body: "<html></html>", headers: { "content-type": "text/html" } },
      "/tv/1.4.3/st/index.wasm": { body: "\0asm", headers: { "content-type": "application/octet-stream" } },
    }),
  };
  const r = await worker.fetch(new Request(W + "/tv/1.4.3/st/index.html"), env);
  assert.equal(r.status, 200);
  assert.equal(r.headers.get("cross-origin-opener-policy"), null, "st/ nao leva COOP");
  assert.equal(r.headers.get("cross-origin-embedder-policy"), null, "st/ nao leva COEP");
  const r2 = await worker.fetch(new Request(W + "/tv/1.4.3/st/index.wasm"), env);
  assert.equal(r2.headers.get("content-type"), "application/wasm");
  console.error("ok  /tv/<v>/st/*: sem COOP/COEP, tipos ainda corretos");
}

// === /tv/icone-*.png repassado com cache longo ===============================
{
  const env = { ASSETS: fakeAssets({ "/tv/icone-220.png": { body: "png", headers: { "content-type": "image/png" } } }) };
  const r = await worker.fetch(new Request(W + "/tv/icone-220.png"), env);
  assert.equal(r.status, 200);
  assert.equal(r.headers.get("cache-control"), "public, max-age=31536000, immutable");
  console.error("ok  /tv/icone-*.png: repassado com cache longo");
}

// === /v1/rec: 304 agora leva CORS (o buraco do #125) =========================
{
  const env = { DB: fakeDB() };
  const cab = { authorization: "Bearer tok-a", "x-nuvio-auth": "nuvio" };
  let r = await worker.fetch(new Request(W + "/v1/rec", { headers: cab }), env);
  assert.equal(r.status, 200);
  const etag = r.headers.get("etag");
  assert.ok(etag);
  r = await worker.fetch(new Request(W + "/v1/rec", { headers: { ...cab, "if-none-match": etag } }), env);
  assert.equal(r.status, 304);
  assert.equal(r.headers.get("access-control-allow-origin"), "*", "304 sem isto e invisivel para o XHR em modo cors");
  assert.equal(r.headers.get("access-control-expose-headers"), "etag");
  console.error("ok  /v1/rec: 304 agora leva Access-Control-Allow-Origin e expõe etag");
}

// === /v1/proxy ================================================================
const painel = (resp) => async (u, op) => (typeof resp === "function" ? resp(u, op) : resp);
const proxy = (alvo, buscar) => {
  const u = new URL(W + "/v1/proxy?u=" + encodeURIComponent(alvo));
  return rotaProxy(new Request(u), u, {}, buscar);
};

// destino privado: recusado sem sair para rede nenhuma.
{
  let saiu = false;
  const r = await proxy("http://192.168.1.20/canal.json", async () => { saiu = true; return new Response("{}"); });
  assert.equal(r.status, 403);
  assert.equal(saiu, false, "destino privado nao deve nem tentar buscar");
  console.error("ok  proxy: IP privado recusado sem sair para a rede");
}

// content-type video/*: recusado (o proxy nunca entrega midia).
{
  const r = await proxy("https://cdn.exemplo.tv/segmento.ts",
    painel(new Response(new Uint8Array(4), { headers: { "content-type": "video/mp2t" } })));
  assert.equal(r.status, 415);
  console.error("ok  proxy: content-type video/* recusado (segmento de HLS)");
}

// redirect para IP privado: recusado, mesmo indo por 2 saltos.
{
  const r = await proxy("https://exemplo.tv/legenda.srt",
    painel((u) => u.includes("exemplo.tv")
      ? new Response(null, { status: 302, headers: { location: "http://127.0.0.1:8080/roubado" } })
      : new Response("nao devia chegar aqui")));
  assert.equal(r.status, 403);
  console.error("ok  proxy: redirect para IP privado recusado");
}

// teto de tamanho: pelo content-length declarado E pelo fluxo real.
{
  let r = await proxy("https://exemplo.tv/grande.json",
    painel(new Response("x", { headers: { "content-type": "application/json", "content-length": String(9 * 1024 * 1024) } })));
  assert.equal(r.status, 502, "recusado pelo content-length declarado");

  const grande = new Uint8Array(9 * 1024 * 1024);
  r = await proxy("https://exemplo.tv/grande2.json",
    painel(new Response(grande, { headers: { "content-type": "application/json" } })));
  assert.equal(r.status, 200, "sem content-length o corte acontece no fluxo, nao na resposta inicial");
  await assert.rejects(r.arrayBuffer(), "o corpo estoura o teto durante a leitura");
  console.error("ok  proxy: teto de 8 MB pelo cabecalho declarado e pelo fluxo real");
}

// caminho feliz: JSON passa com CORS e o endereco final exposto.
{
  const r = await proxy("https://exemplo.tv/api/dados.json",
    painel(new Response(JSON.stringify({ ok: 1 }), { headers: { "content-type": "application/json", etag: '"v1"' } })));
  assert.equal(r.status, 200);
  assert.equal(await r.text(), JSON.stringify({ ok: 1 }));
  assert.equal(r.headers.get("access-control-allow-origin"), "*");
  assert.equal(r.headers.get("access-control-expose-headers"), "etag, x-nuvio-url-final");
  assert.equal(r.headers.get("etag"), '"v1"');
  assert.equal(r.headers.get("x-nuvio-url-final"), "https://exemplo.tv/api/dados.json");
  console.error("ok  proxy: JSON passa com CORS, etag e endereco final expostos");
}

// 304 do destino tambem leva CORS (o cliente manda if-none-match).
{
  const r = await proxy("https://exemplo.tv/dados.json",
    painel(new Response(null, { status: 304 })));
  assert.equal(r.status, 304);
  assert.equal(r.headers.get("access-control-allow-origin"), "*");
  console.error("ok  proxy: 304 do destino tambem leva CORS");
}

// sem url: 400. destino sem protocolo http(s): 403.
{
  let r = await proxy("", painel(new Response("{}")));
  assert.equal(r.status, 400);
  r = await proxy("ftp://exemplo.tv/x", painel(new Response("{}")));
  assert.equal(r.status, 403);
  console.error("ok  proxy: sem url = 400, protocolo fora de http(s) = 403");
}

console.error(`\n${logs.length} linha(s) de log emitidas durante o teste (normal: falhas simuladas de rede/tipo).`);
console.error("TUDO OK");
