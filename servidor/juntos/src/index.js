// nuvio-juntos: as salas do Watch Together ("Assistir Juntos" em pt-BR).
//
// POR QUE UM WORKER SEPARADO (decisao 5 do dono, 06/10/2026): um deploy da API
// (nuvio-recomendacoes) feito da arvore errada derrubou o site da VIDAA. Aqui
// um deploy do social nao derruba sala aberta, e um deploy das salas nao toca
// no social. Este Worker NAO tem banco: so confere o bilhete que o
// nuvio-recomendacoes assina (bilhete.js) e encaminha ao Durable Object.
//
// Rotas:
//   GET /ws?b=<bilhete>          websocket da TV
//   GET /ws?s=<sala>&c=<codigo>  celular, primeiro uso (codigo do QR, 8 caracteres)
//   GET /ws?s=<sala>&k=<chave>   celular reconectando
//   GET /j/<sala><codigo>        pagina do celular (endereco curto: cabe no QR da TV)
//   GET /saude
//
// Publicar SO por tools/juntos-publicar.sh. Segredo: SALA_SEGREDO (o mesmo
// valor no nuvio-recomendacoes), por `wrangler secret put`.
import { conferir } from "./bilhete.js";
import { paginaCelular } from "./celular.js";
export { Sala } from "./sala.js";

const SEM_CACHE = { "cache-control": "no-store" };

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    const rota = url.pathname;
    if (rota === "/saude") return Response.json({ ok: 1, t: Date.now() }, { headers: SEM_CACHE });

    if (rota === "/ws") {
      if ((req.headers.get("upgrade") || "").toLowerCase() !== "websocket")
        return new Response("esperava websocket", { status: 426 });
      let sala = "";
      const b = url.searchParams.get("b");
      if (b) {
        const c = await conferir([env.SALA_SEGREDO, env.SALA_SEGREDO_ANTIGO], b, Math.floor(Date.now() / 1000));
        if (!c) return new Response("bilhete", { status: 401 });
        sala = c.sala;
      } else {
        sala = url.searchParams.get("s") || "";
        const k = url.searchParams.get("k") || "", c = url.searchParams.get("c") || "";
        if (!/^[0-9a-f]{32}$/.test(sala) || !(/^[0-9a-f]{36}$/.test(k) || /^[0-9A-Z]{8}$/.test(c)))
          return new Response("bilhete", { status: 401 });
      }
      const stub = env.SALA.get(env.SALA.idFromName(sala));
      return stub.fetch(req);
    }

    const pagina = rota.match(/^\/j\/[0-9a-f]{32}[0-9A-Z]{8}\/?$/);
    if (pagina && req.method === "GET") {
      return new Response(paginaCelular(), {
        headers: {
          "content-type": "text/html; charset=utf-8", ...SEM_CACHE,
          // O codigo do QR e de uso unico; mesmo assim nenhum link daqui manda Referer.
          "referrer-policy": "no-referrer",
          "content-security-policy": "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; " +
            "connect-src 'self' wss: ws:; img-src https: data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'",
          "x-content-type-options": "nosniff",
        },
      });
    }
    return new Response("nao encontrado", { status: 404 });
  },
};
