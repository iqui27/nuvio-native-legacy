// REGEX ENTRADA -> C: o que a pessoa digita/cola no campo "Padrao do filtro" passa
// primeiro pelo filtro de alfabeto do campo (teclado.c definirDoSistema) e pelo
// teto de tamanho, e SO ENTAO chega a fonteregra. Foi assim que o "(?=" virou
// "(?" (falta o "=" no alfabeto) e uma regex valida foi recusada. Este gerador
// aplica o MESMO filtro (alfabeto e teto lidos do codigo-fonte, passados por
// NV_ALFA e NV_MAX) a uma lista grande de regex de verdade e imprime
// "esperado<TAB>padrao-que-chegou-ao-C<TAB>texto", com o esperado vindo do
// Nuvio OFICIAL (new RegExp(p,"i"), sem a exclusao extra do NuvioTVSmart) sobre a regex ORIGINAL.
// Esperado -1 = o oficial recusa a regex (invalida). Qualquer caractere que o
// filtro perca tambem sai em stderr e faz o gerador falhar.
const ALFA = process.env.NV_ALFA || "";
const MAX = parseInt(process.env.NV_MAX || "500", 10);

// Espelho de definirDoSistema com teclado_aceitar_utf8(1) (o campo da regex): o
// caractere fora do ASCII entra inteiro se couber; o teto e em BYTES de UTF-8.
// (tests/regex_campo.sh passa pelo codigo de verdade.)
function filtro(t) {
  let w = "";
  for (const ch of t) {
    const c = ch.codePointAt(0), nb = Buffer.byteLength(w, "utf8");
    if (nb >= MAX) break;
    if (c >= 0x80) { if (nb + Buffer.byteLength(ch, "utf8") > MAX) break; w += ch; continue; }
    if (!c) continue;
    if (ALFA.includes(ch)) w += ch;
    else if (/[A-Z]/.test(ch) && ALFA.includes(ch.toLowerCase())) w += ch.toLowerCase();
    else if (/[a-z]/.test(ch) && ALFA.includes(ch.toUpperCase())) w += ch.toUpperCase();
  }
  return w;
}
// Todo padrao roda no motor de JS (fonteregra.c), com o ponto que NAO atravessa
// quebra de linha, como o oficial: todos sao comparados tambem em texto de varias
// linhas.
function oficial(pattern, text) {
  let inc;
  // O oficial so liga o filtro quando ha letra/digito ASCII (isRegexSelectionConfigured):
  // no C isso e -1 ("invalido ou nao configurado"). UMA DIFERENCA DE PROPOSITO: texto
  // nao ASCII ("[ñÑ]", "日本語") tambem liga o filtro aqui (ver temLetraOuDigito em
  // fonteregra.c): no oficial isso desliga o auto-play, aqui o filtro ficaria mudo.
  if (!/[a-z0-9]|[^\x00-\x7f]/i.test(pattern)) return -1;
  // "(?i)" no inicio: o C aceita (Java/Python); o V8 so aceita "(?i:...)". Compara
  // com o padrao sem o grupo de flags, que e o que o C executa.
  pattern = pattern.replace(/^\(\?[ims]+\)/, "");
  try { inc = new RegExp(pattern, "i"); } catch (_) { return -1; }
  return inc.test(text) ? 1 : 0;
}

const R = String.raw;
const RES = R`(2160p|4K|UHD|1080p|FHD|BD|HD|720p|SDR)`;
const SRC = R`(SeaDex|Remux|BD-Remux|Dual[\s-]?Audio|Multi[\s-]?Audio|Multi|Dub|Eng|Jap|10bit|HEVC|x265|H265|Hi10P|BluRay|WEB-DL|WEBRip|BDRip|BRRip|x264|AVC|H264|8bit|HDTV|DVDRip|HDRip|Subs)`;
const DONO = R`(?=.*\b${RES}\b)(?=.*\b${SRC}\b)(?!.*(DTS|DTS-HD|DTS:X|.*DTS))(?!.*(?:\bDV\b|\bDolby\s?Vision\b)(?!.*\b(HDR10|HDR|Multi)\b))`;

const padroes = [
  DONO,                                                   // o do dono, como colado
  DONO + R`(?!.*(TrueHD|MLP))`,
  R`SeaDex|Remux|BluRay|Blu[\s\-]?Ray|BDRip|BRRip|\bBD\b|WEB[\s\-]?DL|WEBRip|\bWEB\b|HDTV|DVDRip|\bDVD\b`,
  R`ESP|Latino|Castellano`,
  R`\b(ESP|LAT)\b`,
  R`^(?=.*1080p)(?!.*(DTS|CAM))`,
  R`^(?=.*\b1080p\b)(?=.*\bWEB-DL\b)`,
  R`(?=.*2160p)(?=.*HDR)`,
  R`^(?=.*(Remux|BluRay))(?!.*(CAM|TS))`,
  R`(?<=WEB-)DL`, R`(?<!x)265`, R`(?<=\b)HDR`, R`(?<!\d)4K`,
  R`^(?!.*\b(CAM|TS|HDCAM)\b)(?=.*\b(1080p|2160p)\b)`,
  R`(?=.*\bEng\b)(?!.*\b(Subs)\b)`,
  R`2160p.*HDR`, R`2160p.*?HDR`, R`^.*(1080p|2160p).*$`,
  R`(?:Remux|BluRay)\s+(?:HEVC|x265)`,
  R`S\d{2}E\d{2}`, R`\d{3,4}p`, R`\d+(\.\d+)?\s?GB`, R`[^a-z]DTS[^a-z]`, R`[A-Z]{3}`,
  R`DTS(-HD)?( MA)?`, R`DTS[-\s.]?(HD|X|MA)?`, R`TrueHD|MLP|DTS`, R`E-?AC-?3|DD\+`,
  R`H\.?26[45]`, R`x26[45]`, R`5\.1|7\.1`, R`Atmos`, R`Dolby\s?Vision|\bDV\b`,
  R`^(?!.*Dub).*Eng`, R`(Dual|Multi)[\s-]?Audio`,
  R`Don't`, R`Tom & Jerry`, R`50% off`, R`#1 hit`, R`user@host`, R`a;b`, R`~approx`, R`say "hi"`,
  R`a{2,3}`, R`a{2,}`, R`\(\d+\)`,
  // invalidas no JS (o C tambem tem de recusar):
  R`(`, R`[a-`, R`)`, R`?x`, R`(?<=a`, R`*a`, R`+a`, R`a|*`, R`[z-a]`, R`a{2,1}`, R`a\\`,
  // sem letra/digito: o oficial nao liga o filtro, o C devolve -1:
  R`.+`, R`^$`, R`\$|\^`, R`[\[\]]`,
  // (?i) no inicio, aceito de proposito:
  R`(?i)1080p`, R`(?i)^(?=.*remux)(?!.*dts)`,
  // UTF-8 (digitado ou colado pelo celular): antes o campo jogava fora e o modelo
  // de espanhol virava "Espa(|n)ol|...||", que casa com tudo.
  R`\bESP\b|\bSPA\b|Spanish|Espa(ñ|n)ol|Castellano|Latino|LATAM|🇪🇸|🇲🇽`,
  R`Español|Português|Français|Deutsch|日本語`, R`Subt[ií]tulos|Legendado`, R`(?<=🇲🇽 )1080p`,
  R`^(?=.*(Español|Latino))(?!.*\b(CAM|TS)\b)`, R`[ñÑ]`, R`Espa.ol`,
  // quantificador duplo: o JS recusa, o C tambem (antes o glibc aceitava):
  R`a**`, R`a+*`, R`a?*`, R`a{2}{3}`,
  // "{" sem quantificador valido e literal, nos dois:
  R`x{`, R`x{,2}`, R`1080p{`,
  // os que a traducao POSIX antiga errava (bordas no mesmo separador, lookahead
  // negativo sem grupo dentro):
  R`\b(Remux|BluRay)\b.*\b2160p\b`, R`2160p\b.*\bHDR`, R`\bWEB\b-\bDL\b`, R`\b1080p\b \bWEB-DL\b`,
  R`(?!.*CAM)1080p`, R`^(?!.*CAM).*1080p`, R`1080p(?!.*DTS)`, R`^(?!.*(DTS|TrueHD)).*2160p`,
  R`^.*(1080p|2160p).*$`, R`^Film`, R`WEB-DL$`,
];

// CONHECIDAS, nao comparadas (o C difere do V8 e nao vale mudar): \\p{L}, \\1 e
// \\k<n> o regexjs nao tem (o padrao e recusado, o oficial o aceitaria).
const conhecidas = [R`\p{L}`, R`(a)\1`, R`(?<n>a)\k<n>`];

const textos = [
  "Film 1080p BluRay x265", "Film 1080p BD-Remux", "Anime BD-Remux 1080p DTS-HD",
  "x 2160p Remux HEVC", "Film 1080p WEB-DL", "Film 1080p WEB-DL DTS",
  "Movie 2021 2160p BluRay Remux HEVC TrueHD 7.1", "Movie 1080p BluRay x265 DTS-HD MA 5.1",
  "Movie 2160p Remux DV HEVC Atmos", "Movie 2160p Remux DV HDR10 HEVC",
  "Film 2160p WEB-DL Dolby Vision", "Anime 2160p Dual Audio HEVC",
  "Film 1080p CAM", "Film TS 1080p", "Torrentio\n1080p WEB-DL", "Film 1080p\nDTS",
  "Show S01E02 Eng 1080p Subs", "Show Eng 1080p", "film x265 web-dl", "movie 1080P bluray",
  "Filme ESP Latino 1080p", "Castellano 720p", "Film H.264 5.1 E-AC-3", "Film DD+ 7.1 Atmos",
  "Tom & Jerry 1080p", "Don't Look 1080p", "50% off #1 hit", "user@host a;b ~approx say \"hi\"",
  "Film 4.5 GB", "(2021) [BD]", "aa aaa a", "",
  "Película Español 1080p", "Torrentio 🇲🇽 1080p Latino", "Filme Português Legendado", "Subtítulos ESP",
  "日本語 1080p", "Movie 1080p English", "Film Français 2160p CAM",
];

let ruim = 0;
const linhas = [];
for (const p of padroes) {
  const f = filtro(p);
  if (f !== p) { ruim++; process.stderr.write(`FILTRO ALTERA:\n  digitado: ${p}\n  chegou  : ${f}\n`); }
  for (const t of textos) {
    linhas.push([oficial(p, t), f, t.replace(/\n/g, "\\n")].join("\t"));
  }
}
process.stdout.write(linhas.join("\n") + "\n");
if (ruim) { process.stderr.write(`regex_entrada: ${ruim} padrao(oes) alterado(s) pelo filtro do campo\n`); process.exit(1); }
