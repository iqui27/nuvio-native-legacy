// Teste unitario puro de src/letterboxd.js (sem rede, sem wrangler). Node 22+:
//   node servidor/recomendacoes/teste-letterboxd.mjs
import assert from "node:assert/strict";
import { deflateRawSync } from "node:zlib";
import { CABECALHO_LB, csvLetterboxd, lerCsv, lerCsvObjetos, linhasDoExport, linkReviewOk, lerZip,
  tagsParaTexto, textoParaTags, dataValida, dataParaEpoch, epochParaData } from "./src/letterboxd.js";

let n = 0;
const ok = (nome, cond) => { assert.ok(cond, nome); n++; console.log("ok", nome); };
const eq = (nome, a, b) => { assert.deepEqual(a, b, nome); n++; console.log("ok", nome); };

// --- CSV
eq("csv: simples", lerCsv("a,b,c\n1,2,3\n"), [["a", "b", "c"], ["1", "2", "3"]]);
eq("csv: CRLF e BOM", lerCsv("﻿a,b\r\n1,2\r\n"), [["a", "b"], ["1", "2"]]);
eq("csv: aspas, virgula, aspas escapadas e quebra dentro do campo",
  lerCsv('x,"a, ""b""","l1\nl2"\n'), [["x", 'a, "b"', "l1\nl2"]]);
eq("csv: campo vazio no fim e entre virgulas", lerCsv("a,,\n,b,\n"), [["a", "", ""], ["", "b", ""]]);
eq("csv: sem \\n final", lerCsv("a,b\n1,2"), [["a", "b"], ["1", "2"]]);
eq("csv: aspas vazias contam como campo", lerCsv('""\nx\n'), [[""], ["x"]]);
eq("csv: objetos pelo cabecalho", lerCsvObjetos("A,B\n1,2\n"), [{ A: "1", B: "2" }]);

const linhas = [
  { titulo: 'O "Bom", o Mau', ano: 1966, imdb: "tt0060196", tmdb: 429, data: "2026-01-02", nota10: 9, rever: 1, tags: "visual, ending", review: "" },
  { titulo: "Linha\nQuebrada", ano: 2000, imdb: "", tmdb: "", data: "2026-02-03", nota10: 0, rever: 0, tags: "", review: "" },
];
const csv = csvLetterboxd(linhas);
ok("export: cabecalho exato", csv.split("\r\n")[0] === CABECALHO_LB && CABECALHO_LB === "Title,Year,imdbID,tmdbID,WatchedDate,Rating10,Rewatch,Tags,Review");
ok("export: aspas duplicadas e campo com virgula entre aspas", csv.includes('"O ""Bom"", o Mau",1966,tt0060196,429,2026-01-02,9,true,"visual, ending",'));
ok("export: sem nota fica vazio, rewatch false", csv.includes('"Linha\nQuebrada",2000,,,2026-02-03,,false,,'));
const volta = lerCsvObjetos(csv);
eq("round-trip: titulo com aspas e virgula", volta[0].Title, 'O "Bom", o Mau');
eq("round-trip: titulo com quebra de linha", volta[1].Title, "Linha\nQuebrada");
eq("round-trip: campos", [volta[0].Rating10, volta[0].Rewatch, volta[0].Tags, volta[1].Rating10], ["9", "true", "visual, ending", ""]);

// --- tags e datas
eq("tags: bit 0 e 11", tagsParaTexto(1 | (1 << 11)), "visual, underrated");
eq("tags: ida e volta", textoParaTags(tagsParaTexto(0xabc)), 0xabc);
eq("tags: desconhecida some, caixa ignorada", textoParaTags("Visual, bobagem , SLOW PACE"), 1 | (1 << 5));
ok("data: validas e invalidas", dataValida("2026-02-28") && !dataValida("2026-02-30") && !dataValida("26-1-1") && !dataValida(""));
eq("data: epoch ida e volta", epochParaData(dataParaEpoch("2025-12-31")), "2025-12-31");

// --- merge do export do Letterboxd
const diary = [
  "Date,Name,Year,Letterboxd URI,Rating,Rewatch,Tags,Watched Date",
  '2026-03-01,Heat,1995,https://boxd.it/aaa,4.5,,"crime, night",2026-02-27',
  "2026-03-02,Alien,1979,https://boxd.it/bbb,,Yes,,2026-03-02",
  "2026-03-03,Sem Data,2001,https://boxd.it/ccc,3,,,",
].join("\r\n");
const ratings = [
  "Date,Name,Year,Letterboxd URI,Rating",
  "2026-03-05,Heat,1995,https://boxd.it/aaa,5",
  "2026-03-06,Alien,1979,https://boxd.it/bbb,3.5",
  "2026-03-07,Solo Nota,2010,https://boxd.it/ddd,0.5",
  "2026-03-08,Outro,2011,https://boxd.it/eee,",
].join("\n");
const reviews = [
  "Date,Name,Year,Letterboxd URI,Rating,Rewatch,Review,Tags,Watched Date",
  'REV,Heat,1995,https://letterboxd.com/fulano/film/heat-1995/,4.5,,"Texto longo, privado",,2026-02-27',
  "2026-03-09,Perfect Days,2023,https://letterboxd.com/fulano/film/perfect-days/2/,4,,Gostei,,2026-03-08",
  "2026-03-09,Ruim,2020,https://evil.example/x,2,,txt,,2026-03-09",
].join("\r\n");
const L = linhasDoExport({ diary, ratings, reviews });
const por = (t) => L.find((l) => l.titulo === t);
eq("merge: Heat rating*2, data assistida, tags", [por("Heat").nota10, por("Heat").data, por("Heat").tags, por("Heat").rever], [9, "2026-02-27", "crime, night", 0]);
eq("merge: link vem so da review valida", por("Heat").link, "https://letterboxd.com/fulano/film/heat-1995/");
eq("merge: Rewatch Yes", por("Alien").rever, 1);
eq("merge: nota do ratings completa diario sem nota", por("Alien").nota10, 7);
eq("merge: sem Watched Date usa Date", por("Sem Data").data, "2026-03-03");
eq("merge: rating sem diario vira linha com a data do rating", [por("Solo Nota").data, por("Solo Nota").nota10], ["2026-03-07", 1]);
ok("merge: rating vazio sem diario vira linha sem nota", por("Outro") && por("Outro").nota10 === 0);
eq("merge: review sem diario vira linha com link", [por("Perfect Days").data, por("Perfect Days").link, por("Perfect Days").nota10], ["2026-03-08", "https://letterboxd.com/fulano/film/perfect-days/2/", 8]);
eq("merge: link invalido descartado", por("Ruim").link, "");
ok("merge: texto da review nunca sai", !JSON.stringify(L).includes("privado") && !JSON.stringify(L).includes("Gostei"));
eq("merge: sem duplicar Heat", L.filter((l) => l.titulo === "Heat").length, 1);
eq("merge: so diary", linhasDoExport({ diary }).length, 3);
eq("merge: vazio", linhasDoExport({}), []);

// --- link
for (const u of ["https://letterboxd.com/fulano/film/heat-1995/", "https://letterboxd.com/fulano/film/heat-1995/2/",
  "https://boxd.it/1AbC", "https://Letterboxd.com/Fu_lano/film/x/"]) ok("link ok: " + u, linkReviewOk(u));
for (const u of ["http://letterboxd.com/fulano/film/x/", "https://letterboxd.com/fulano/film/x", "https://letterboxd.com/fulano/",
  "https://letterboxd.com/fulano/film/x/?a=1", "https://letterboxd.com/fulano/film/x/#y", "https://evil.com/letterboxd.com/fulano/film/x/",
  "https://letterboxd.com.evil.com/fulano/film/x/", "https://boxd.it/", "https://boxd.it/a/b", "javascript:alert(1)", "", null, 5,
  "https://letterboxd.com/fulano/film/x/ "]) ok("link recusado: " + String(u), !linkReviewOk(u));

// --- ZIP montado aqui
function zip(arquivos) {
  const partes = [], central = [];
  let off = 0;
  const crc = (b) => { let c, t = [], r = 0xffffffff; for (let i = 0; i < 256; i++) { c = i; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[i] = c >>> 0; }
    for (const x of b) r = t[(r ^ x) & 255] ^ (r >>> 8); return (r ^ 0xffffffff) >>> 0; };
  for (const [nome, conteudo, metodo] of arquivos) {
    const nb = Buffer.from(nome), raw = Buffer.from(conteudo);
    const dados = metodo === 8 ? deflateRawSync(raw) : raw;
    const lh = Buffer.alloc(30); lh.writeUInt32LE(0x04034b50, 0); lh.writeUInt16LE(20, 4); lh.writeUInt16LE(metodo, 8);
    lh.writeUInt32LE(crc(raw), 14); lh.writeUInt32LE(dados.length, 18); lh.writeUInt32LE(raw.length, 22); lh.writeUInt16LE(nb.length, 26);
    const ch = Buffer.alloc(46); ch.writeUInt32LE(0x02014b50, 0); ch.writeUInt16LE(20, 4); ch.writeUInt16LE(20, 6); ch.writeUInt16LE(metodo, 10);
    ch.writeUInt32LE(crc(raw), 16); ch.writeUInt32LE(dados.length, 20); ch.writeUInt32LE(raw.length, 24); ch.writeUInt16LE(nb.length, 28); ch.writeUInt32LE(off, 42);
    partes.push(lh, nb, dados); central.push(ch, nb);
    off += 30 + nb.length + dados.length;
  }
  const cd = Buffer.concat(central);
  const eocd = Buffer.alloc(22); eocd.writeUInt32LE(0x06054b50, 0); eocd.writeUInt16LE(arquivos.length, 8); eocd.writeUInt16LE(arquivos.length, 10);
  eocd.writeUInt32LE(cd.length, 12); eocd.writeUInt32LE(off, 16);
  return new Uint8Array(Buffer.concat([...partes, cd, eocd]));
}
const grande = diary + "\r\n" + "2026-04-01,Acentuação ção,2000,https://boxd.it/z,3,,,2026-04-01\r\n".repeat(200);
const z = await lerZip(zip([["diary.csv", grande, 8], ["ratings.csv", ratings, 0], ["reviews.csv", reviews, 8],
  ["README.txt", "nao e csv", 8], ["pasta/", "", 0]]));
eq("zip: so os csv", Object.keys(z).sort(), ["diary.csv", "ratings.csv", "reviews.csv"]);
ok("zip: deflate integro com acentos", z["diary.csv"] === grande);
ok("zip: stored integro", z["ratings.csv"] === ratings);
eq("zip + merge de ponta a ponta", linhasDoExport({ diary: z["diary.csv"], ratings: z["ratings.csv"], reviews: z["reviews.csv"] })
  .find((l) => l.titulo === "Heat").link, "https://letterboxd.com/fulano/film/heat-1995/");
await assert.rejects(() => lerZip(new Uint8Array([1, 2, 3])), /zip invalido/); n++; console.log("ok zip: lixo e recusado");
const trunc = zip([["a.csv", "x,y\n1,2\n", 8]]).slice(0, 40);
await assert.rejects(() => lerZip(trunc), /zip invalido/); n++; console.log("ok zip: truncado e recusado");

console.log(`\n${n} verificacoes passaram`);
