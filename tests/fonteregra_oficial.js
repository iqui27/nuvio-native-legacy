// Gera os casos de tests/fonteregra_oficial.sh: roda a logica do Nuvio OFICIAL
// (new RegExp(p, "i"), a regex
// exatamente como escrita, sem a exclusao extra do NuvioTVSmart) sobre padroes com lookahead/lookbehind e imprime
// "esperado<TAB>padrao<TAB>texto" (quebra de linha do texto escrita como \n).
// O C (fonteregra.c) tem de dar a mesma resposta em todos.
function oficial(pattern, text) {
  const inc = new RegExp(pattern, "i");
  return inc.test(text) ? 1 : 0;
}

const R = String.raw;
const RES = R`(2160p|4K|UHD|1080p|FHD|BD|HD|720p|SDR)`;
const SRC = R`(SeaDex|Remux|BD-Remux|Dual[\s-]?Audio|Multi[\s-]?Audio|Multi|Dub|Eng|Jap|10bit|HEVC|x265|H265|Hi10P|BluRay|WEB-DL|WEBRip|BDRip|BRRip|x264|AVC|H264|8bit|HDTV|DVDRip|HDRip|Subs)`;

const padroes = [
  // O do dono, como escrito (com "." no lugar de ".*"):
  R`(?=.\b${RES}\b)(?=.\b${SRC}\b)(?!.(DTS|DTS-HD|DTS:X|.DTS))(?!.(?:\bDV\b|\bDolby\s?Vision\b)(?!.*\b(HDR10|HDR|Multi)\b))`,
  // A versao corrigida:
  R`^(?=.*\b${RES}\b)(?=.*\b${SRC}\b)(?!.*\b(DTS|DTS-HD|DTS:X)\b)(?!(?=.*\b(DV|Dolby\s?Vision)\b)(?!.*\b(HDR10|HDR|Multi)\b))`,
  R`^(?=.*1080p)(?!.*(DTS|CAM))`,
  R`^(?=.*\b1080p\b)(?=.*\bWEB-DL\b)`,
  R`(?=.*2160p)(?=.*HDR)`,
  R`^(?=.*(Remux|BluRay))(?!.*(CAM|TS))`,
  R`(?<=WEB-)DL`,
  R`(?<!x)265`,
  R`^(?!.*\b(CAM|TS)\b)(?=.*\b(1080p|2160p)\b)`,
  R`(?=.*\bEng\b)(?!.*\b(Subs)\b)`,
];

const textos = [
  "Film 1080p BluRay x265",
  "Film 1080p BD-Remux",
  "Anime BD-Remux 1080p DTS-HD",
  "x 2160p Remux HEVC",
  "Film 1080p WEB-DL",
  "Film 1080p WEB-DL DTS",
  "Film 2160p BluRay DV HDR10",
  "Film 2160p BluRay DV",
  "Film 2160p WEB-DL Dolby Vision",
  "Anime 2160p Dual Audio HEVC",
  "Film 1080p CAM",
  "Film TS 1080p",
  "Torrentio\n1080p WEB-DL",
  "Torrentio 1080p\nWEB-DL DTS",
  "Film 1080p\nDTS",
  "Film HDR 2160p",
  "Film 2160p",
  "Show Eng 1080p Subs",
  "Show Eng 1080p",
  "BD-Remux",
  "film x265 web-dl",
  "movie 1080P bluray",
];

const linhas = [];
for (const p of padroes)
  for (const t of textos)
    linhas.push([oficial(p, t), p, t.replace(/\n/g, "\\n")].join("\t"));
process.stdout.write(linhas.join("\n") + "\n");
