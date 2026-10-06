// Diario (migracao 011, src/diario.js) contra `wrangler dev --local` (D1 local, sem rede).
//   bash servidor/recomendacoes/preparar-local.sh
//   npx wrangler@4 dev --local --port 8799 --config servidor/recomendacoes/wrangler.toml &
//   DIARIO_URL=http://127.0.0.1:8799 node servidor/recomendacoes/teste-diario.mjs
// tok-a/tok-b vem semeados por preparar-local.sh (nuvio:aaa / nuvio:bbb).
import assert from "node:assert/strict";

const BASE = process.env.DIARIO_URL || "http://127.0.0.1:8799";
let n = 0;
const ok = (nome, cond) => { assert.ok(cond, nome); n++; console.log("ok", nome); };
const eq = (nome, a, b) => { assert.deepEqual(a, b, nome); n++; console.log("ok", nome); };

const quem = (tok, perfil) => ({ authorization: "Bearer " + tok, "x-nuvio-auth": "nuvio",
  ...(perfil ? { "x-nuvio-perfil": String(perfil) } : {}) });
async function api(h, metodo, rota, corpo, cru) {
  const r = await fetch(BASE + rota, { method: metodo, headers: { ...h, ...(corpo !== undefined ? { "content-type": "application/json" } : {}) },
    body: cru ?? (corpo !== undefined ? JSON.stringify(corpo) : undefined) });
  const txt = await r.text();
  let j = null; try { j = JSON.parse(txt); } catch {}
  return { s: r.status, j, txt, h: r.headers };
}
const A = quem("tok-a"), A2 = quem("tok-a", 2), B = quem("tok-b");
const agora = Math.floor(Date.now() / 1000);
const dia = 86400;

// zera o estado (rodar duas vezes seguidas tem de passar)
for (const h of [A, A2, B]) { await api(h, "POST", "/v1/diario/cofre", { ligado: 0 }); }

// --- auth
{
  const r = await fetch(BASE + "/v1/diario/cofre"); ok("sem token: 401", r.status === 401);
  const r2 = await fetch(BASE + "/v1/diario?desde=0"); ok("sem token (lista): 401", r2.status === 401);
}

// --- cofre
eq("cofre: comeca desligado", (await api(A, "GET", "/v1/diario/cofre")).j, { ligado: 0 });
const item = (chave, quando, extra = {}) => ({ chave, quando, nota10: 8, flags: 4, tags: 3, cor: 123, seg: 7000, hora: 50,
  origem: "p", tmdb: 603, dormiu: 0, dur: 8160, com: 0, titulo: "Matrix", ano: 1999, ...extra });
{
  const r = await api(A, "POST", "/v1/diario/lote", { itens: [item("tt0133093", agora - dia)] });
  ok("lote sem cofre: 409 {erro:cofre}", r.s === 409 && r.j.erro === "cofre");
  eq("lote sem cofre: nada gravado", (await api(A, "GET", "/v1/diario")).j.itens, []);
  ok("letterboxd sem cofre: 409", (await api(A, "POST", "/v1/diario/letterboxd", { linhas: [] })).s === 409);
  ok("cofre invalido: 400", (await api(A, "POST", "/v1/diario/cofre", { ligado: 7 })).s === 400);
}
eq("cofre: liga", (await api(A, "POST", "/v1/diario/cofre", { ligado: 1 })).j, { ligado: 1 });
eq("cofre: GET depois de ligar", (await api(A, "GET", "/v1/diario/cofre")).j, { ligado: 1 });
eq("cofre: outro perfil da conta continua desligado", (await api(A2, "GET", "/v1/diario/cofre")).j, { ligado: 0 });

// --- validacao
{
  const mal = async (nome, it) => { const r = await api(A, "POST", "/v1/diario/lote", { itens: [it] }); ok("valida: " + nome, r.s === 400 && r.j.indice === 0); };
  await mal("chave ruim", item("tt 1", agora - 10));
  await mal("quando zero", item("tt1", 0));
  await mal("quando no futuro", item("tt1", agora + 3 * dia));
  await mal("nota10 11", item("tt1", agora - 10, { nota10: 11 }));
  await mal("origem x", item("tt1", agora - 10, { origem: "x" }));
  await mal("titulo 161", item("tt1", agora - 10, { titulo: "a".repeat(161) }));
  await mal("flags texto", item("tt1", agora - 10, { flags: "4" }));
  const grande = Array.from({ length: 201 }, (_, i) => item("tt" + i, agora - 10));
  ok("valida: 201 itens = 413", (await api(A, "POST", "/v1/diario/lote", { itens: grande })).s === 413);
  ok("valida: corpo > 64KB = 413", (await api(A, "POST", "/v1/diario/lote", undefined, JSON.stringify({ itens: [], x: "a".repeat(70000) }))).s === 413);
  ok("valida: json ruim = 400", (await api(A, "POST", "/v1/diario/lote", undefined, "{")).s === 400);
  ok("valida: quando +1 dia aceito", (await api(A, "POST", "/v1/diario/lote", { itens: [item("tt9", agora + dia)] })).s === 200);
  await api(A, "POST", "/v1/diario/lote", { apagar: [{ chave: "tt9", quando: agora + dia }] });
}

// --- lote + restauracao
const t1 = agora - 5 * dia, t2 = agora - 4 * dia, t3 = agora - 3 * dia;
{
  const r = await api(A, "POST", "/v1/diario/lote", { itens: [
    item("tt0133093", t1), item("tt0944947_s2e5", t2, { titulo: "GoT", nota10: 0, flags: 0 }), item("tt0111161", t3, { titulo: "Shawshank", ano: 1994, nota10: 10, flags: 1 })] });
  eq("lote: gravadas", r.j, { ok: 1, gravadas: 3, apagadas: 0 });
}
let todos;
{
  const r = await api(A, "GET", "/v1/diario");
  todos = r.j;
  eq("restore: 3 itens, mais 0", [todos.itens.length, todos.mais], [3, 0]);
  const m = todos.itens.find((x) => x.chave === "tt0133093");
  eq("restore: campos do item", [m.quando, m.nota10, m.flags, m.tags, m.cor, m.seg, m.hora, m.origem, m.tmdb, m.dormiu, m.dur, m.com, m.titulo, m.ano, m.link],
    [t1, 8, 4, 3, 123, 7000, 50, "p", 603, 0, 8160, 0, "Matrix", 1999, ""]);
  ok("restore: ate = ultimo atualizado", todos.ate === todos.itens[2].atualizado);
  ok("restore: ordenado por atualizado, estritamente crescente", todos.itens[0].atualizado < todos.itens[1].atualizado && todos.itens[1].atualizado < todos.itens[2].atualizado);
  const inc = await api(A, "GET", "/v1/diario?desde=" + todos.itens[1].atualizado);
  eq("restore: desde devolve so os mais novos", inc.j.itens.map((x) => x.chave), ["tt0111161"]);
  const pag = await api(A, "GET", "/v1/diario?limite=2");
  eq("restore: limite=2 pagina", [pag.j.itens.length, pag.j.mais, pag.j.ate === pag.j.itens[1].atualizado], [2, 1, true]);
  const pag2 = await api(A, "GET", "/v1/diario?limite=2&desde=" + pag.j.ate);
  eq("restore: segunda pagina", [pag2.j.itens.length, pag2.j.mais], [1, 0]);
  ok("restore: desde invalido 400", (await api(A, "GET", "/v1/diario?desde=abc")).s === 400);
}

// --- editar (upsert) e link preservado
{
  await api(A, "POST", "/v1/diario/lote", { itens: [item("tt0133093", t1, { nota10: 6, flags: 5 })] });
  const r = (await api(A, "GET", "/v1/diario")).j.itens;
  eq("editar: ainda 3 linhas", r.length, 3);
  const m = r.find((x) => x.chave === "tt0133093");
  eq("editar: nota e flags novas", [m.nota10, m.flags], [6, 5]);
  ok("editar: foi para o fim da fila incremental", r[r.length - 1].chave === "tt0133093");
}

// --- link
{
  const url = "https://letterboxd.com/fulano/film/the-matrix/";
  const r = await api(A, "POST", "/v1/diario/link", { chave: "tt0133093", url });
  ok("link valido: ok", r.s === 200 && r.j.ok === 1 && r.j.quando === t1);
  let m = (await api(A, "GET", "/v1/diario")).j.itens.find((x) => x.chave === "tt0133093");
  eq("link: guardado e flag 128", [m.link, m.flags & 128], [url, 128]);
  await api(A, "POST", "/v1/diario/lote", { itens: [item("tt0133093", t1, { nota10: 7, flags: 1 })] });
  m = (await api(A, "GET", "/v1/diario")).j.itens.find((x) => x.chave === "tt0133093");
  eq("link: reenvio da TV preserva link e 128", [m.link, m.flags & 128, m.nota10], [url, 128, 7]);
  ok("link boxd.it: ok com quando", (await api(A, "POST", "/v1/diario/link", { chave: "tt0111161", quando: t3, url: "https://boxd.it/AbC12" })).s === 200);
  for (const u of ["http://letterboxd.com/a/film/x/", "https://evil.com/a/film/x/", "https://letterboxd.com/a/film/x/?utm=1", "https://boxd.it/", "", 5])
    ok("link invalido recusado: " + u, (await api(A, "POST", "/v1/diario/link", { chave: "tt0133093", url: u })).s === 400);
  ok("link: chave ruim 400", (await api(A, "POST", "/v1/diario/link", { chave: "x y", url })).s === 400);
  ok("link: entrada inexistente 404", (await api(A, "POST", "/v1/diario/link", { chave: "tt404", url })).s === 404);
  ok("link: quando errado 404", (await api(A, "POST", "/v1/diario/link", { chave: "tt0133093", quando: 12345, url })).s === 404);
}

// --- apagar
{
  const r = await api(A, "POST", "/v1/diario/lote", { apagar: [{ chave: "tt0944947_s2e5", quando: t2 }, { chave: "tt000", quando: 5 }] });
  eq("apagar: so conta o que existia", r.j, { ok: 1, gravadas: 0, apagadas: 1 });
  eq("apagar: restou 2", (await api(A, "GET", "/v1/diario")).j.itens.map((x) => x.chave).sort(), ["tt0111161", "tt0133093"]);
  ok("apagar: item invalido 400", (await api(A, "POST", "/v1/diario/lote", { apagar: [{ chave: "tt1" }] })).s === 400);
}

// --- csv
{
  await api(A, "POST", "/v1/diario/lote", { itens: [
    item("tt0944947_s1e1", t1, { titulo: "Ep" }), item("tt0068646", t2, { titulo: 'Pai, "O" Poderoso', ano: 1972, nota10: 10, flags: 256 }),
    item("tt0110912", t2, { titulo: 'Pulp "Fiction", the', ano: 1994, nota10: 9, flags: 0, tags: 1 | (1 << 7), tmdb: 680 })] });
  const r = await fetch(BASE + "/v1/diario/letterboxd.csv", { headers: A });
  const csv = await r.text();
  ok("csv: content-type", /^text\/csv/.test(r.headers.get("content-type")));
  ok("csv: x-partes = 1", r.headers.get("x-partes") === "1");
  const L = csv.split("\r\n");
  eq("csv: cabecalho", L[0], "Title,Year,imdbID,tmdbID,WatchedDate,Rating10,Rewatch,Tags,Review");
  const d = (s) => new Date(s * 1000).toISOString().slice(0, 10);
  ok("csv: filme com tags em ingles e aspas", csv.includes(`"Pulp ""Fiction"", the",1994,tt0110912,680,${d(t2)},9,false,"visual, laughed a lot",`));
  ok("csv: rewatch true", csv.includes(`Matrix,1999,tt0133093,603,${d(t1)},7,true,"visual, soundtrack",`));
  ok("csv: sem episodio", !csv.includes("Ep,") && !csv.includes("tt0944947"));
  ok("csv: sem parcial (flag 256)", !csv.includes("Poderoso"));
  ok("csv: parte inexistente 404", (await fetch(BASE + "/v1/diario/letterboxd.csv?parte=2", { headers: A })).status === 404);
  ok("csv: parte invalida 400", (await fetch(BASE + "/v1/diario/letterboxd.csv?parte=x", { headers: A })).status === 400);
  ok("csv: sem token 401", (await fetch(BASE + "/v1/diario/letterboxd.csv")).status === 401);
  // divide em partes: 8000 linhas com titulo comprido passam de 1 MB
  const lote = (k) => Array.from({ length: 200 }, (_, i) => item("tt1" + String(k * 200 + i).padStart(6, "0"), t1 - i, { titulo: "T".repeat(80) }));
  for (let k = 0; k < 40; k++) ok("csv grande: lote " + k, (await api(A, "POST", "/v1/diario/lote", { itens: lote(k) })).s === 200);
  const r2 = await fetch(BASE + "/v1/diario/letterboxd.csv", { headers: A });
  const t2txt = await r2.text();
  const partes = parseInt(r2.headers.get("x-partes"), 10);
  ok("csv grande: dividido em mais de uma parte e cada uma < 1 MB", partes >= 2 && new TextEncoder().encode(t2txt).length < 1000000 + 1000);
  const rp = await fetch(BASE + `/v1/diario/letterboxd.csv?parte=${partes}`, { headers: A });
  ok("csv grande: ultima parte existe e traz cabecalho", rp.status === 200 && (await rp.text()).startsWith("Title,Year"));
  // limpa os 8000 itens de teste
  const tudo = [];
  let desde = 0;
  for (;;) { const p = (await api(A, "GET", `/v1/diario?desde=${desde}`)).j; tudo.push(...p.itens); desde = p.ate; if (!p.mais) break; }
  const lixo = tudo.filter((x) => x.chave.startsWith("tt1") && x.chave.length === 9);
  for (let i = 0; i < lixo.length; i += 200)
    await api(A, "POST", "/v1/diario/lote", { apagar: lixo.slice(i, i + 200).map((x) => ({ chave: x.chave, quando: x.quando })) });
  ok("csv grande: limpou", (await api(A, "GET", "/v1/diario?limite=500")).j.itens.length < 10);
}

// --- importacao do Letterboxd
{
  const linhas = [
    { titulo: "Heat", ano: 1995, data: "2026-02-27", nota10: 9, rever: 0, tags: "visual, crime", imdb: "tt0113277" },
    { titulo: "Alien", ano: 1979, data: "2026-03-02", nota10: 7, rever: 1, tags: "", tmdb: 348, link: "https://boxd.it/bbb" },
    { titulo: "Filme Sem Id", ano: 2001, data: "2026-03-03", nota10: 6, rever: 0, tags: "ending" },
    { titulo: "Outro Sem Id", ano: 2002, data: "2026-03-04", nota10: 0, rever: 0, tags: "" },
  ];
  eq("lb: casadas e pendentes", (await api(A, "POST", "/v1/diario/letterboxd", { linhas })).j, { casadas: 2, pendentes: 2 });
  const it = (await api(A, "GET", "/v1/diario?limite=500")).j.itens;
  const heat = it.find((x) => x.chave === "tt0113277"), alien = it.find((x) => x.chave === "tmdb:348");
  eq("lb: imdb vira chave, origem l, flag 16", [heat.origem, heat.flags, heat.nota10, heat.tags, heat.quando, heat.titulo], ["l", 16, 9, 1, Date.UTC(2026, 1, 27, 12) / 1000, "Heat"]);
  eq("lb: tmdb vira tmdb:<n>, rever |1, link |128", [alien.chave, alien.flags, alien.tmdb, alien.link], ["tmdb:348", 16 | 1 | 128, 348, "https://boxd.it/bbb"]);
  const p = (await api(A, "GET", "/v1/diario/pendentes")).j;
  eq("pendentes: total e titulos", [p.total, p.itens.map((x) => x.titulo).sort()], [2, ["Filme Sem Id", "Outro Sem Id"]]);
  eq("lb: reimportar nao duplica", (await api(A, "POST", "/v1/diario/letterboxd", { linhas })).j, { casadas: 2, pendentes: 2 });
  eq("pendentes: sem duplicar", (await api(A, "GET", "/v1/diario/pendentes")).j.total, 2);
  ok("lb: data invalida 400", (await api(A, "POST", "/v1/diario/letterboxd", { linhas: [{ titulo: "x", ano: 1, data: "2026-13-01" }] })).s === 400);
  ok("lb: imdb invalido 400", (await api(A, "POST", "/v1/diario/letterboxd", { linhas: [{ titulo: "x", ano: 1, data: "2026-01-01", imdb: "abc" }] })).s === 400);
  ok("lb: link invalido 400", (await api(A, "POST", "/v1/diario/letterboxd", { linhas: [{ titulo: "x", ano: 1, data: "2026-01-01", link: "https://evil.com/a" }] })).s === 400);
  ok("lb: 501 linhas = 413", (await api(A, "POST", "/v1/diario/letterboxd", { linhas: Array.from({ length: 501 }, () => ({ titulo: "x", ano: 1, data: "2026-01-01" })) })).s === 413);
}

// --- isolamento: outra conta e outro perfil da mesma conta
{
  eq("isolamento: B nao ve nada", (await api(B, "GET", "/v1/diario")).j.itens, []);
  ok("isolamento: B com cofre desligado leva 409", (await api(B, "POST", "/v1/diario/lote", { itens: [item("tt1", t1)] })).s === 409);
  await api(B, "POST", "/v1/diario/cofre", { ligado: 1 });
  await api(B, "POST", "/v1/diario/lote", { itens: [item("tt0133093", t1, { nota10: 1, titulo: "DoB" })] });
  eq("isolamento: mesma chave/quando em outra conta e linha separada", (await api(B, "GET", "/v1/diario")).j.itens.map((x) => [x.titulo, x.nota10]), [["DoB", 1]]);
  eq("isolamento: A nao mudou", (await api(A, "GET", "/v1/diario?limite=500")).j.itens.find((x) => x.chave === "tt0133093").nota10, 7);
  // perfil 2 da conta A: cofre proprio, diario proprio
  ok("perfil 2: sem cofre 409", (await api(A2, "POST", "/v1/diario/lote", { itens: [item("tt1", t1)] })).s === 409);
  await api(A2, "POST", "/v1/diario/cofre", { ligado: 1 });
  eq("perfil 2: comeca vazio", (await api(A2, "GET", "/v1/diario")).j.itens, []);
  await api(A2, "POST", "/v1/diario/lote", { itens: [item("tt777", t1, { titulo: "Perfil2" })] });
  eq("perfil 2: ve so o seu", (await api(A2, "GET", "/v1/diario")).j.itens.map((x) => x.titulo), ["Perfil2"]);
  ok("perfil 2: nao ve o do perfil principal", !(await api(A2, "GET", "/v1/diario?limite=500")).txt.includes("Matrix"));
  ok("perfil 1 nao ve o do perfil 2", !(await api(A, "GET", "/v1/diario?limite=500")).txt.includes("Perfil2"));
  eq("perfil 2: pendentes vazios", (await api(A2, "GET", "/v1/diario/pendentes")).j.total, 0);
  // DELETE /v1/diario so apaga a propria pessoa e mantem o cofre
  eq("DELETE: ok", (await api(A2, "DELETE", "/v1/diario")).j, { ok: 1 });
  eq("DELETE: perfil 2 vazio, cofre segue ligado", [(await api(A2, "GET", "/v1/diario")).j.itens.length, (await api(A2, "GET", "/v1/diario/cofre")).j.ligado], [0, 1]);
  ok("DELETE: nao tocou o perfil principal", (await api(A, "GET", "/v1/diario")).j.itens.length > 0);
  ok("DELETE: nao tocou a conta B", (await api(B, "GET", "/v1/diario")).j.itens.length === 1);
}

// --- desligar apaga tudo
{
  ok("A tem dados antes", (await api(A, "GET", "/v1/diario")).j.itens.length > 0 && (await api(A, "GET", "/v1/diario/pendentes")).j.total > 0);
  eq("desligar: responde ligado 0", (await api(A, "POST", "/v1/diario/cofre", { ligado: 0 })).j, { ligado: 0 });
  eq("desligar: diario apagado", (await api(A, "GET", "/v1/diario")).j.itens, []);
  eq("desligar: pendentes apagados", (await api(A, "GET", "/v1/diario/pendentes")).j.total, 0);
  eq("desligar: csv so cabecalho", (await (await fetch(BASE + "/v1/diario/letterboxd.csv", { headers: A })).text()), "Title,Year,imdbID,tmdbID,WatchedDate,Rating10,Rewatch,Tags,Review\r\n");
  ok("desligar: lote volta a 409", (await api(A, "POST", "/v1/diario/lote", { itens: [item("tt1", t1)] })).s === 409);
  ok("desligar A nao apagou B", (await api(B, "GET", "/v1/diario")).j.itens.length === 1);
}
for (const h of [A, A2, B]) await api(h, "POST", "/v1/diario/cofre", { ligado: 0 });
eq("rota desconhecida: 404", (await api(A, "GET", "/v1/diario/xyz")).s, 404);

console.log(`\n${n} verificacoes passaram`);
