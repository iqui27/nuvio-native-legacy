// LETTERBOXD, PARTE PURA (ESM, sem dependencias). Roda no Worker (src/diario.js)
// e, depois, na pagina do celular, no navegador da pessoa. Nada aqui faz rede:
// o dado e o export que a PROPRIA pessoa baixou do Letterboxd.

// As 12 tags fechadas, na ordem dos bits (bit 0 = "visual"). Nomes em ingles
// porque e o que vai para o Letterboxd.
export const TAGS_EN = ["visual", "soundtrack", "acting", "screenplay", "ending", "slow pace",
  "cried", "laughed a lot", "scary", "want to rewatch", "overrated", "underrated"];

export function tagsParaTexto(mascara) {
  const o = [];
  for (let i = 0; i < TAGS_EN.length; i++) if ((mascara >> i) & 1) o.push(TAGS_EN[i]);
  return o.join(", ");
}

// "Visual, ending, qualquer coisa" -> bits; o que nao esta na lista fechada some.
export function textoParaTags(texto) {
  let m = 0;
  for (const t of String(texto || "").split(",")) {
    const i = TAGS_EN.indexOf(t.trim().toLowerCase());
    if (i >= 0) m |= 1 << i;
  }
  return m;
}

// --- CSV (RFC 4180) --------------------------------------------------------

// Devolve uma matriz de linhas de campos. Aceita BOM, CRLF/LF/CR, aspas e "" dentro
// de campo entre aspas. Linhas totalmente vazias (ex.: o ultimo \n) sao descartadas.
export function lerCsv(texto) {
  let s = String(texto ?? "");
  if (s.charCodeAt(0) === 0xfeff) s = s.slice(1);
  const linhas = [];
  let campo = "", linha = [], aspas = false, tocou = false;
  const fimCampo = () => { linha.push(campo); campo = ""; tocou = true; };
  const fimLinha = () => {
    if (!tocou && !linha.length && campo === "") return;
    linha.push(campo); linhas.push(linha);
    campo = ""; linha = []; tocou = false;
  };
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (aspas) {
      if (c === '"') { if (s[i + 1] === '"') { campo += '"'; i++; } else aspas = false; }
      else campo += c;
    } else if (c === '"') { aspas = true; tocou = true; }
    else if (c === ",") fimCampo();
    else if (c === "\r") { if (s[i + 1] === "\n") i++; fimLinha(); }
    else if (c === "\n") fimLinha();
    else campo += c;
  }
  fimLinha();
  return linhas;
}

// Primeira linha vira cabecalho: [{Date:..., Name:...}, ...].
export function lerCsvObjetos(texto) {
  const m = lerCsv(texto);
  if (!m.length) return [];
  const cab = m[0].map((c) => c.trim());
  return m.slice(1).map((l) => {
    const o = {};
    cab.forEach((k, i) => { o[k] = l[i] ?? ""; });
    return o;
  });
}

const campoCsv = (v) => {
  const s = String(v ?? "");
  return /[",\r\n]|^\s|\s$/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
};
export const linhaCsv = (campos) => campos.map(campoCsv).join(",");

export const CABECALHO_LB = "Title,Year,imdbID,tmdbID,WatchedDate,Rating10,Rewatch,Tags,Review";

// linhas: [{titulo, ano, imdb, tmdb, data:"YYYY-MM-DD", nota10, rever, tags:"a, b", review}]
export function csvLetterboxd(linhas, comCabecalho = true) {
  const o = comCabecalho ? [CABECALHO_LB] : [];
  for (const l of linhas) {
    o.push(linhaCsv([l.titulo || "", l.ano || "", l.imdb || "", l.tmdb || "", l.data || "",
      l.nota10 > 0 ? l.nota10 : "", l.rever ? "true" : "false", l.tags || "", l.review || ""]));
  }
  return o.join("\r\n") + "\r\n";
}

// --- datas -----------------------------------------------------------------

export function dataValida(d) {
  const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(String(d || ""));
  if (!m) return false;
  const t = new Date(Date.UTC(+m[1], +m[2] - 1, +m[3]));
  return t.getUTCFullYear() === +m[1] && t.getUTCMonth() === +m[2] - 1 && t.getUTCDate() === +m[3];
}
// meio-dia UTC, para o dia nao escorregar com fuso.
export const dataParaEpoch = (d) => { const m = d.split("-"); return Date.UTC(+m[0], +m[1] - 1, +m[2], 12) / 1000; };
export const epochParaData = (s) => new Date(s * 1000).toISOString().slice(0, 10);

// --- link de review --------------------------------------------------------

// So estas duas formas, https, sem query nem fragmento:
//   https://letterboxd.com/<usuario>/film/<slug>/[<n>/]
//   https://boxd.it/<codigo>
export function linkReviewOk(url) {
  if (typeof url !== "string" || url.length > 200) return false;
  return /^https:\/\/letterboxd\.com\/[A-Za-z0-9_-]{1,40}\/film\/[a-z0-9-]{1,100}\/(\d{1,4}\/)?$/i.test(url)
      || /^https:\/\/boxd\.it\/[A-Za-z0-9]{1,12}$/i.test(url);
}

// --- export do proprio Letterboxd -> linhas de importacao -------------------

const nota10De = (r) => {
  const n = parseFloat(String(r || "").replace(",", "."));
  if (!(n >= 0.5 && n <= 5)) return 0;
  return Math.round(n * 2);
};
const norm = (s) => String(s || "").trim().toLowerCase();
const chaveFilme = (nome, ano) => norm(nome) + "|" + String(ano || "").trim();

// diary / ratings / reviews: TEXTO dos CSVs (qualquer um pode faltar).
// Linha de saida: {titulo, ano, data, nota10, rever, tags, link}. O TEXTO da review
// nunca entra: de reviews.csv so se aproveita a URI, e so se for um link aceito.
export function linhasDoExport({ diary, ratings, reviews } = {}) {
  const saida = [];
  const porDia = new Map();   // nome|ano|data -> linha
  const porFilme = new Set(); // nome|ano com alguma linha
  const poe = (o, data) => {
    if (!o.Name || !dataValida(data)) return null;
    const k = chaveFilme(o.Name, o.Year) + "|" + data;
    let l = porDia.get(k);
    if (!l) {
      l = { titulo: o.Name.trim(), ano: parseInt(o.Year, 10) || 0, data, nota10: 0, rever: 0, tags: "", link: "" };
      porDia.set(k, l); saida.push(l); porFilme.add(chaveFilme(o.Name, o.Year));
    }
    return l;
  };
  for (const o of lerCsvObjetos(diary)) {
    const l = poe(o, o["Watched Date"] || o.Date);
    if (!l) continue;
    l.nota10 = l.nota10 || nota10De(o.Rating);
    if (/^yes$/i.test((o.Rewatch || "").trim())) l.rever = 1;
    if (o.Tags && !l.tags) l.tags = o.Tags.trim();
  }
  for (const o of lerCsvObjetos(reviews)) {
    const l = poe(o, o["Watched Date"] || o.Date);
    if (!l) continue;
    l.nota10 = l.nota10 || nota10De(o.Rating);
    if (/^yes$/i.test((o.Rewatch || "").trim())) l.rever = 1;
    if (o.Tags && !l.tags) l.tags = o.Tags.trim();
    if (!l.link && linkReviewOk((o["Letterboxd URI"] || "").trim())) l.link = o["Letterboxd URI"].trim();
  }
  for (const o of lerCsvObjetos(ratings)) {
    const nota = nota10De(o.Rating);
    if (porFilme.has(chaveFilme(o.Name, o.Year))) {
      // Filme com diario: a nota do ratings.csv so completa linha sem nota.
      for (const l of saida) if (!l.nota10 && chaveFilme(l.titulo, l.ano) === chaveFilme(o.Name, o.Year)) l.nota10 = nota;
      continue;
    }
    poe(o, o.Date);
    const l = porDia.get(chaveFilme(o.Name, o.Year) + "|" + o.Date);
    if (l) l.nota10 = nota;
  }
  return saida;
}

// --- ZIP minimo (so leitura) ----------------------------------------------

const u16 = (b, o) => b[o] | (b[o + 1] << 8);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const LIMITE_CSV = 32 * 1024 * 1024;

async function inflar(dados) {
  const ds = new DecompressionStream("deflate-raw");
  const w = ds.writable.getWriter();
  w.write(dados).catch(() => {}); w.close().catch(() => {});
  return new Uint8Array(await new Response(ds.readable).arrayBuffer());
}

// bytes: Uint8Array do .zip. Devolve {nome: texto} so dos .csv (metodo 0 stored e
// 8 deflate). Sem ZIP64, sem criptografia: o export do Letterboxd nao usa.
export async function lerZip(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  let eocd = -1;
  for (let i = b.length - 22; i >= Math.max(0, b.length - 22 - 65535); i--)
    if (u32(b, i) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error("zip invalido");
  const n = u16(b, eocd + 10);
  let p = u32(b, eocd + 16);
  const dec = new TextDecoder("utf-8");
  const saida = {};
  for (let i = 0; i < n; i++) {
    if (p + 46 > b.length || u32(b, p) !== 0x02014b50) throw new Error("zip invalido");
    const metodo = u16(b, p + 10), comp = u32(b, p + 20), real = u32(b, p + 24);
    const nl = u16(b, p + 28), el = u16(b, p + 30), cl = u16(b, p + 32), lo = u32(b, p + 42);
    const nome = dec.decode(b.subarray(p + 46, p + 46 + nl));
    p += 46 + nl + el + cl;
    if (!/\.csv$/i.test(nome) || nome.endsWith("/")) continue;
    if (real > LIMITE_CSV) continue;
    if (lo + 30 > b.length || u32(b, lo) !== 0x04034b50) throw new Error("zip invalido");
    const ini = lo + 30 + u16(b, lo + 26) + u16(b, lo + 28);
    if (ini + comp > b.length) throw new Error("zip invalido");
    const cru = b.subarray(ini, ini + comp);
    let dados;
    if (metodo === 0) dados = cru;
    else if (metodo === 8) dados = await inflar(cru);
    else continue;
    saida[nome] = dec.decode(dados);
  }
  return saida;
}
