// Casos para tests/regexjs_v8.sh. Imprime "modo<TAB>esperado<TAB>padrao<TAB>texto"
// (quebra de linha do texto escrita como \n), com o esperado vindo do V8:
//
//   modo "m" (motor): regexjs contra new RegExp(p, "i").test(t), padroes ALEATORIOS
//     (semente fixa) com o que costuma quebrar um motor de retrocesso: repeticao
//     que casa vazio, lookbehind (agora casado para tras), lookahead dentro de
//     lookbehind, preguicoso, quantificador aninhado, UTF-8 de 2 e 3 bytes.
//   modo "f" (filtro): fonteregra_regex_testar contra a logica do Nuvio OFICIAL
//     (NuvioTVSmart streamAutoPlaySelector.js: isRegexSelectionConfigured,
//     new RegExp(p, "i"), sem a exclusao extra do NuvioTVSmart) com regex de ~400 caracteres no
//     estilo da do dono e textos de fonte de verdade (Torrentio, AIOStreams,
//     MediaFusion) de 200 a 8000 bytes. Era aqui que o teto de passos estourava
//     e a fonte boa era recusada.
"use strict";
const R = String.raw;

// --- modo m: aleatorio --------------------------------------------------------
let seed = Number(process.env.NV_SEED) || 20261013;   // NV_SEED=n roda outra semente
function rnd() {   // mulberry32
  seed |= 0; seed = seed + 0x6D2B79F5 | 0;
  let t = Math.imul(seed ^ seed >>> 15, 1 | seed);
  t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
  return ((t ^ t >>> 14) >>> 0) / 4294967296;
}
const pick = (a) => a[Math.floor(rnd() * a.length)];
const ATOMOS = ["a", "b", "1", ".", "\\d", "\\w", "\\s", "\\W", "[ab]", "[^a]", "[ñx]", "ñ", "é", "x", "-",
                "\\b", "\\B", "^", "$", "D", "TS"];
function gera(d, fundo) {
  const r = rnd();
  if (d > fundo || r < 0.35) return pick(ATOMOS);
  if (r < 0.5) return gera(d + 1, fundo) + gera(d + 1, fundo);
  if (r < 0.6) return "(" + gera(d + 1, fundo) + "|" + gera(d + 1, fundo) + ")";
  if (r < 0.68) return "(?:" + gera(d + 1, fundo) + "|)";
  if (r < 0.76) return "(?=" + gera(d + 1, fundo) + ")";
  if (r < 0.83) return "(?!" + gera(d + 1, fundo) + ")";
  if (r < 0.88) return "(?<=" + gera(d + 1, fundo) + ")";
  if (r < 0.92) return "(?<!" + gera(d + 1, fundo) + ")";
  return "(?:" + gera(d + 1, fundo) + ")" + pick(["*", "+", "?", "*?", "+?", "??", "{2}", "{0,2}", "{1,}"]);
}
const ALFA = "aññbé1 x-™D\n.";
function texto(max) {
  let s = ""; const n = Math.floor(rnd() * max);
  for (let i = 0; i < n; i++) s += ALFA[Math.floor(rnd() * ALFA.length)];
  return s;
}
const out = [];
const esc = (t) => t.replace(/\n/g, "\\n");
for (const [n, fundo, max] of [[2500, 3, 14], [1500, 4, 40], [600, 5, 120]]) {
  for (let k = 0; k < n; ) {
    const p = gera(0, fundo);
    let re; try { re = new RegExp(p, "i"); } catch (_) { continue; }
    for (let j = 0; j < 5; j++) { const t = texto(max); out.push(["m", re.test(t) ? 1 : 0, p, esc(t)].join("\t")); }
    k++;
  }
}

// --- modo m: repeticao de um caractere (O_STAR) --------------------------------
// ".*", "[\s\S]*", "\w*?"... seguidos de continuacoes variadas, com e sem "^":
// o caminho em que o motor so tenta a continuacao nas posicoes cujo byte ela
// pode consumir (montaSeg) e so tenta a posicao 0 quando o padrao comeca com "^".
// Sem emoji no texto: o V8 casa \W com METADE de um par UTF-16, o motor (UTF-8)
// nao — diferenca conhecida e sem uso real.
const STAR = ["." + "*", ".*?", "[\\s\\S]*", "[\\s\\S]*?", "\\w*", "\\W*", "\\s*", "\\S+", "[ab]*", "[^a]*?",
              "a*", "b+?", "\\d*", "[^\\n]*", "é*", "[\\s.-]?"];
const SATOM = ["a", "b", "A", "é", "-", "\\.", " ", "\\n", "DTS", "x", "[ab]", "\\d", "\\w", "\\W", ".", "ñ"];
const SZW = ["\\b", "\\B", "^", "$", ""];
function scont(d) {
  const r = rnd();
  if (d > 2 || r < 0.3) return pick(SZW) + pick(SATOM);
  if (r < 0.5) return pick(SZW) + "(" + pick(SATOM) + "|" + pick(SATOM) + pick(SATOM) + ")" + pick(SZW);
  if (r < 0.6) return "(?=" + speca(d + 1) + ")" + pick(SATOM);
  if (r < 0.7) return "(?!" + speca(d + 1) + ")";
  if (r < 0.8) return "(?<=" + pick(SATOM) + pick(STAR) + ")" + pick(SATOM);
  if (r < 0.9) return "";
  return speca(d + 1);
}
function speca(d) { return pick(STAR) + scont(d); }
const SALFA = ["a", "b", "A", "B", "é", "ñ", "-", ".", " ", "\n", "D", "T", "S", "x", "1", "_", "DTS"];
for (let k = 0; k < 4000; ) {
  let p = "";
  for (let i = 0, n = 1 + Math.floor(rnd() * 3); i < n; i++) {
    const r = rnd();
    p += r < 0.2 ? "(?=" + speca(1) + ")" : r < 0.4 ? "(?!" + speca(1) + ")" : speca(0);
  }
  if (rnd() < 0.35) p = "^" + p;
  let re; try { re = new RegExp(p, "i"); } catch (_) { continue; }
  for (let j = 0; j < 5; j++) {
    let t = ""; for (let i = 0, n = Math.floor(rnd() * 40); i < n; i++) t += pick(SALFA);
    out.push(["m", re.test(t) ? 1 : 0, p, esc(t)].join("\t"));
  }
  k++;
}

// --- modo f: regex longas x textos de fonte longos ----------------------------
function oficial(pattern, text) {
  if (!/[a-z0-9]|[^\x00-\x7f]/i.test(pattern)) return -1;
  let inc; try { inc = new RegExp(pattern, "i"); } catch (_) { return -1; }
  return inc.test(text) ? 1 : 0;
}
const RES = R`(2160p|4K|UHD|1080p|FHD|BD|HD|720p|SDR)`;
const SRC = R`(SeaDex|Remux|BD-Remux|Dual[\s-]?Audio|Multi[\s-]?Audio|Multi|Dub|Eng|Jap|10bit|HEVC|x265|H265|Hi10P|BluRay|WEB-DL|WEBRip|BDRip|BRRip|x264|AVC|H264|8bit|HDTV|DVDRip|HDRip|Subs)`;
const DONO = R`(?=.*\b${RES}\b)(?=.*\b${SRC}\b)(?!.*(DTS|DTS-HD|DTS:X|.*DTS))(?!.*(?:\bDV\b|\bDolby\s?Vision\b)(?!.*\b(HDR10|HDR|Multi)\b))`;
const CERTO = R`^(?=.*\b${RES}\b)(?=.*\b${SRC}\b)(?!.*\b(DTS|DTS-HD|DTS:X)\b)(?!(?=.*\b(DV|Dolby\s?Vision)\b)(?!.*\b(HDR10|HDR|Multi)\b))`;
const pad400 = (p, fim) => { let s = p; while (s.length + fim.length < 400) s += "Q"; return s + fim; };
const LONGOS = [
  DONO,
  DONO + R`(?!.*(TrueHD|MLP|Atmos))`,
  CERTO + R`(?!.*\b(TrueHD|MLP|CAM|TS|HDCAM|TELESYNC|TC)\b)`,
  pad400(CERTO + R`(?!.*\b(TrueHD|MLP|CAM|HDCAM)\b)(?=.*(Torrentio|Comet|AIO|MediaFusion|`, "))"),
  "ESP SPA LAT LATAM Castellano Latino Spanish Dual Multi Subs Eng ITA FRE GER JAP KOR CHI RUS POR PTBR Dublado Legendado HINDI TAM TEL ARA TUR POL CZE HUN SWE NOR DAN FIN DUT GRE HEB THA VIE IND MAY FIL UKR BUL RON SRP HRV SLV SLK LIT"
    .split(" ").map((w) => R`\b${w}\b`).join("|"),
  [R`Dual\s?Audio`, R`Multi\s?Audio`, R`Dolby\s?Vision`, R`Blu\s?Ray`, R`WEB\s?DL`, R`DTS\s?HD\s?MA`, R`True\s?HD\s?Atmos`,
   R`DD\s?5\.1`, R`DDP\s?5\.1`, R`E\s?AC\s?3`, R`H\s?265`, R`10\s?bit`, R`HDR\s?10\s?Plus`, R`4K\s?UHD`, R`BD\s?Remux`].join("|"),
  R`^(?!.*\b(CAM|TS|HDCAM|TELESYNC|TELECINE|TC|SCR|SCREENER|DVDSCR|R5|WORKPRINT|HDTS|PDVD|HQCAM|HDTC|3D|SBS|Upscaled|DTS|DTS-HD|DTS:X|TrueHD|MLP)\b).*(2160p|4K|UHD|1080p|FHD).*(Remux|BluRay|WEB-DL|WEBRip|BDRip|HEVC|x265|H265|x264|AVC).*$`,
  [R`(?<=\b)2160p(?=\b)`, R`(?<!Dolby\s)Vision`, R`(?<=WEB-)DL`, R`(?<!x)265`, R`(?<=\.)HEVC`, R`(?<![A-Za-z])DV(?![A-Za-z])`,
   R`(?<=\s)HDR10\+?`, R`(?<!No\s)Atmos`, R`(?<=\d{3,4})p`, R`(?<=\[)Multi(?=\])`, R`(?<=-)FGT$`, R`(?<!\w)UHD(?!\w)`].join("|"),
  R`^(?=.*?\b(2160p|4K)\b.*?\b(Remux|BluRay)\b)(?=.*?(HDR10\+?|HDR|DV|Dolby\s?Vision))(?!.*?(DTS.*?MA|TrueHD.*?Atmos|.*CAM.*))(?=.*?\b(\d+(\.\d+)?\s?GB)\b)(?!.*?\b(0\.\d+|[1-4](\.\d+)?)\s?GB\b)`,
  R`(?<!.*DTS.*)2160p`, R`(?<=.*Remux.*)HEVC(?!.*DTS)`, R`\b(Remux|BluRay)\b.*\b2160p\b`, R`(?!.*CAM)1080p`, R`1080p(?!.*DTS)`,
  R`\bESP\b|\bSPA\b|Spanish|Espa(ñ|n)ol|Castellano|Latino|LATAM|🇪🇸|🇲🇽`,
  R`^(?!.{0,600}\bDTS\b)(?=.{0,300}\b2160p\b)`,   // {n,m} grande (antes: "pattern too large")
];
const TOR = "Torrentio\n4k DV | HDR Movie.Name.2024.2160p.UHD.BluRay.REMUX.DV.HDR10.HEVC.TrueHD.Atmos.7.1-FGT\n" +
  "👤 52 💾 61.4 GB ⚙️ TorrentGalaxy Movie.Name.2024.2160p.UHD.BluRay.REMUX.DV.HDR10.HEVC.TrueHD.Atmos.7.1-FGT.mkv " +
  "https://torrentio.strem.fun/realdebrid/" + "A".repeat(52) + "/1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d/null/0/Movie.Name.2024.2160p.mkv";
const AIO = "AIOStreams [RD+] Comet 2160p\n🎥 Movie Name (2024)\n📺 BluRay REMUX • HDR10+ • DV\n🎞️ HEVC • 10bit\n" +
  "🎧 Atmos | TrueHD | DD+ 7.1\n🔊 English | Spanish | Multi\n📦 58.21 GB / 💾 58.21 GB\n🏷️ FraMeSToR\n📡 RD+ • Cached\n🔍 Comet\n" +
  "Movie.Name.2024.2160p.BluRay.REMUX.HDR10Plus.DV.HEVC.Atmos.TrueHD.7.1.Multi-FraMeSToR.mkv " +
  "https://aiostreams.example.invalid/api/v1/debrid/playback/" + "eyJhbGciOiJIUzI1NiJ9".repeat(8) + "/Movie.Name.2024.2160p.mkv";
const MF = "MediaFusion 🧲 P2P 1080p\nShow.S02E07.1080p.WEB-DL.DDP5.1.H.264-NTb\n💾 3.2 GB 👤 120\n🌐 English, Español\n🔗 ThePirateBay " +
  "Show.S02E07.1080p.WEB-DL.DDP5.1.H.264-NTb.mkv magnet:?xt=urn:btih:1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d";
const UNI = "Movie.Name.2024.2160p.UHD.BluRay.REMUX.HDR10.HEVC.DDP.5.1-GRP ";
const TEXTOS = [
  TOR, TOR.replace("TrueHD.Atmos.7.1", "DDP.5.1").replace(/DV\./g, ""), TOR.replace("TrueHD.Atmos.7.1", "DTS-HD.MA.7.1"),
  TOR.replace(/2160p/g, "1080p").replace(/UHD\./g, "").replace(/DV\.HDR10\./g, "").replace("TrueHD.Atmos.7.1", "DD5.1").replace(/REMUX/g, "x265"),
  AIO, AIO.replace("Atmos | TrueHD | DD+ 7.1", "DD+ 5.1").replace(" • DV", ""), AIO.replace(/2160p/g, "1080p").replace("Spanish", "Español Latino 🇲🇽"),
  MF, MF.replace(/1080p/g, "2160p").replace(/H\.264/g, "x265"), "Film 1080p CAM HDCAM 1.4 GB",
  "Comet | TB+\n" + Array.from({ length: 25 }, (_, i) => `Line ${i}: ${UNI}extra info tags here`).join("\n") + "\nfinal 2160p Remux HEVC 45 GB",
  UNI.repeat(30) + "45.2 GB Torrentio", UNI.repeat(30).replace(/DDP/g, "DTS") + "45.2 GB Torrentio",
  "x".repeat(3000) + " 2160p Remux HEVC", UNI.repeat(130).slice(0, 8000), "",
];
for (const p of LONGOS) for (const t of TEXTOS) out.push(["f", oficial(p, t), p, esc(t)].join("\t"));
process.stdout.write(out.join("\n") + "\n");
