#include "fontenota.h"
#include "linguas.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// Fronteira de palavra: so letra/digito ASCII prende. Byte >= 0x80 (emoji,
// acento) conta como separador, para "🇬🇧English" casar "english".
static int prende(unsigned char c) { return c < 0x80 && isalnum(c); }

// Posicao do termo como palavra (sem caixa), a partir de `de`; NULL = nao ha.
static const char *palavraEm(const char *t, const char *de, const char *termo) {
  size_t n = strlen(termo);
  for (const char *q = de; *q; q++) {
    if (strncasecmp(q, termo, n)) continue;
    if (q > t && prende((unsigned char)q[-1])) continue;
    if (prende((unsigned char)q[n])) continue;
    return q;
  }
  return NULL;
}
static int palavra(const char *t, const char *termo) { return palavraEm(t, t, termo) != NULL; }
static int contem(const char *t, const char *termo) {
  size_t n = strlen(termo);
  for (const char *q = t; *q; q++) if (!strncasecmp(q, termo, n)) return 1;
  return 0;
}
static int algum(const char *t, const char *const *termos, int comoPalavra) {
  for (int i = 0; termos[i]; i++)
    if (comoPalavra ? palavra(t, termos[i]) : contem(t, termos[i])) return 1;
  return 0;
}

// ------------------------------------------------------------ tipo de lancamento

static const char *const CAM[] = { "cam", "camrip", "hdcam", "telesync", "hdts", "hd-ts", "telecine",
                                   "hdtc", "screener", "dvdscr", "scr", NULL };
static const char *const BLURAY[] = { "bluray", "blu-ray", "blu ray", "bdrip", "brrip", "bd", "bdmv", NULL };
static const char *const WEBDL[] = { "web-dl", "webdl", "web dl", "web", NULL };
static const char *const WEBRIP[] = { "webrip", "web-rip", "web rip", NULL };
static const char *const HDTV[] = { "hdtv", "pdtv", "tvrip", "dsr", NULL };

static int tierDe(const char *t) {
  if (algum(t, CAM, 1)) return FN_TIER_CAM;
  // "BDRemux", "Remux-1080p": substring, nao palavra.
  if (contem(t, "remux")) return FN_TIER_REMUX;
  if (algum(t, BLURAY, 1)) return FN_TIER_BLURAY;
  // WEBRip antes do "web" solto, que e o WEB-DL da cena ("1080p.WEB.h264").
  if (algum(t, WEBRIP, 1)) return FN_TIER_WEBRIP;
  if (algum(t, WEBDL, 1)) return FN_TIER_WEBDL;
  if (algum(t, HDTV, 1)) return FN_TIER_HDTV;
  return FN_TIER_DESCONHECIDO;
}
static long ptsTier(int tier) {
  switch (tier) {
    case FN_TIER_REMUX: return FN_PTS_REMUX;
    case FN_TIER_BLURAY: return FN_PTS_BLURAY;
    case FN_TIER_WEBDL: return FN_PTS_WEBDL;
    case FN_TIER_WEBRIP: return FN_PTS_WEBRIP;
    case FN_TIER_HDTV: return FN_PTS_HDTV;
    case FN_TIER_CAM: return FN_PTS_CAM;
    default: return 0;
  }
}

// ------------------------------------------------------------------- bitrate

// "36 Mbps", "36.2Mb/s", "8,5 Mbit/s", "2400 kbps". A CAIXA do B importa:
// "MB/s" e "MBps" sao bytes (velocidade de download), nao bitrate.
int fontenota_bitrate_declarado(const char *t) {
  static const struct { const char *u; int mult; } U[] = {
    { "Mbps", 1000 }, { "mbps", 1000 }, { "Mb/s", 1000 }, { "mb/s", 1000 },
    { "Mbit", 1000 }, { "mbit", 1000 },
    { "kbps", 1 }, { "Kbps", 1 }, { "kb/s", 1 }, { "Kb/s", 1 }, { "kbit", 1 }, { "Kbit", 1 },
  };
  if (!t) return 0;
  for (const char *p = t; *p; p++) {
    double v = 0, frac = 0, div = 1;
    const char *q = p;
    int temPonto = 0;
    if (!isdigit((unsigned char)*p) || (p > t && (prende((unsigned char)p[-1]) || p[-1] == '.' || p[-1] == ','))) continue;
    for (; isdigit((unsigned char)*q) || ((*q == '.' || *q == ',') && !temPonto && isdigit((unsigned char)q[1])); q++) {
      if (*q == '.' || *q == ',') { temPonto = 1; continue; }
      if (temPonto) { div *= 10; frac = frac * 10 + (*q - '0'); }
      else v = v * 10 + (*q - '0');
    }
    v += frac / div;
    if (*q == ' ') q++;
    for (size_t k = 0; k < sizeof U / sizeof U[0]; k++) {
      size_t n = strlen(U[k].u);
      if (!strncmp(q, U[k].u, n) && !prende((unsigned char)q[n]) && v > 0 && v < 1e6) {
        long kbps = (long)(v * U[k].mult + 0.5);
        return kbps > 0 && kbps < 1000000 ? (int)kbps : 0;
      }
    }
    p = q - 1;   // nao reanalisar os digitos do mesmo numero
  }
  return 0;
}

// Teto do possivel para o arquivo inteiro (video + audio). Acima disto o dado
// e erro ou e o tamanho de um pacote de temporada.
static int kbpsTeto(int h) {
  return h >= 2160 ? 180000 : h >= 1440 ? 100000 : h >= 1080 ? 80000 : h >= 720 ? 40000 : h > 0 ? 20000 : 180000;
}
// Referencia de um bom encode na resolucao (a parcela cresce acima dela).
static int kbpsReferencia(int h) {
  return h >= 2160 ? 16000 : h >= 1440 ? 10000 : h >= 1080 ? 6000 : h >= 720 ? 3000 : 1500;
}
// Abaixo disto um "REMUX" nao e remux (Blu-ray 1080p tem 20-40 Mbps).
static int kbpsRemuxMin(int h) {
  return h >= 2160 ? 30000 : h >= 1080 ? 15000 : h >= 720 ? 8000 : h > 0 ? 3000 : 0;
}
static long ptsBitrate(int kbps, int h) {
  long r;
  if (kbps <= 0) return 0;
  r = (long)kbps * 100 / kbpsReferencia(h);
  return r >= 200 ? FN_PTS_BITRATE_MAX : r >= 141 ? FN_PTS_BITRATE_MAX * 3 / 4
       : r >= 100 ? FN_PTS_BITRATE_MAX / 2 : r >= 70 ? FN_PTS_BITRATE_MAX / 4 : 0;
}

// ------------------------------------------------------------- codec e audio

static const char *const HEVC[] = { "hevc", "x265", "h265", "h.265", "h 265", NULL };
static const char *const AVC[] = { "avc", "x264", "h264", "h.264", "h 264", NULL };
static const char *const DEZ_BITS[] = { "10bit", "10-bit", "10 bit", "10bits", "hi10", "main10", NULL };
static const char *const SEM_PERDA[] = { "truehd", "true-hd", "true hd", "dts-hd ma", "dts-hd.ma", "dts-hdma",
                                         "dtshd-ma", "dtshd ma", "dts-ma", "dts ma", "dts:x", "dts-x", "dtsx",
                                         "flac", "pcm", NULL };
static const char *const SURROUND[] = { "ddp", "dd+", "eac3", "e-ac-3", "e-ac3", "ac3", "ac-3", "dd5", "dd 5",
                                        "dd7", "dts", "atmos", NULL };
static const char *const BASICO[] = { "aac", "opus", "mp3", "vorbis", "mp2", NULL };

static int codecDe(const char *t) {
  if (algum(t, HEVC, 1)) return FN_CODEC_HEVC;
  if (palavra(t, "av1")) return FN_CODEC_AV1;
  if (algum(t, AVC, 1) || contem(t, "hi10")) return FN_CODEC_AVC;
  return FN_CODEC_DESCONHECIDO;
}
static int audioDe(const char *t) {
  if (algum(t, SEM_PERDA, 0)) return FN_AUD_SEM_PERDA;
  if (algum(t, SURROUND, 0)) return FN_AUD_SURROUND;
  if (algum(t, BASICO, 0)) return FN_AUD_BASICO;
  return FN_AUD_DESCONHECIDO;
}

// ------------------------------------------------------------------- idioma

typedef struct { const char *cod; const char *palavras[9]; const char *bandeiras[4]; } Lingua;
static const Lingua LINGUAS[] = {
  { "en", { "english", "eng", "ingles", "inglês", NULL }, { "GB", "US", NULL } },
  { "ja", { "japanese", "jpn", "japones", "japonês", NULL }, { "JP", NULL } },
  { "pt", { "portuguese", "português", "portugues", "pt-br", "ptbr", "brazilian", "nacional", "dublado", NULL },
          { "BR", "PT", NULL } },
  { "es", { "spanish", "español", "espanol", "castellano", "latino", "doblado", "spa", NULL }, { "ES", "MX", NULL } },
  { "fr", { "french", "français", "francais", "truefrench", "vff", "fre", NULL }, { "FR", NULL } },
  { "de", { "german", "deutsch", "ger", NULL }, { "DE", NULL } },
  { "it", { "italian", "italiano", "ita", NULL }, { "IT", NULL } },
  { "ko", { "korean", "kor", NULL }, { "KR", NULL } },
  { "zh", { "chinese", "mandarin", "cantonese", NULL }, { "CN", "TW", "HK", NULL } },
  { "ru", { "russian", "rus", NULL }, { "RU", NULL } },
  { "hi", { "hindi", "hin", NULL }, { "IN", NULL } },
  { "ar", { "arabic", "ara", NULL }, { "SA", "AE", "EG", NULL } },
  { "da", { "danish", "dansk", "dan", NULL }, { "DK", NULL } },
  { "el", { "greek", "ell", "gre", NULL }, { "GR", NULL } },
  { "he", { "hebrew", "heb", NULL }, { "IL", NULL } },
  { "nl", { "dutch", "nederlands", "nld", "dut", NULL }, { "NL", NULL } },
  { "hu", { "hungarian", "magyar", "hun", NULL }, { "HU", NULL } },
  { "id", { "indonesian", "ind", NULL }, { "ID", NULL } },
  { "no", { "norwegian", "norsk", "nor", "nob", "nno", NULL }, { "NO", NULL } },
  { "pl", { "polish", "polski", "pol", NULL }, { "PL", NULL } },
  { "ro", { "romanian", "română", "romana", "ron", "rum", NULL }, { "RO", NULL } },
  { "sv", { "swedish", "svenska", "swe", NULL }, { "SE", NULL } },
  { "th", { "thai", "tha", NULL }, { "TH", NULL } },
  { "cs", { "czech", "čeština", "cestina", "ces", "cze", NULL }, { "CZ", NULL } },
  { "tr", { "turkish", "türkçe", "turkce", "tur", NULL }, { "TR", NULL } },
  { "uk", { "ukrainian", "ukr", NULL }, { "UA", NULL } },
  { "vi", { "vietnamese", "vie", NULL }, { "VN", NULL } },
  { "fi", { "finnish", "suomi", "fin", NULL }, { "FI", NULL } },
};

// A MENCAO E DE LEGENDA? "English Subs", "Subs: English", "ENG-SUB": a palavra
// fala da legenda, nao do audio. Janela na MESMA linha, poucos bytes para cada
// lado — e heuristica, e por isso o idioma so desempata e nunca filtra.
static int deLegenda(const char *t, const char *p, size_t n) {
  const char *a = p, *b = p + n;
  for (int k = 0; k < 14 && a > t && a[-1] != '\n'; k++) a--;
  for (int k = 0; k < 12 && *b && *b != '\n'; k++) b++;
  for (const char *q = a; q + 3 <= b; q++)
    if (!strncasecmp(q, "sub", 3) || !strncasecmp(q, "leg", 3)) return 1;
  return 0;
}
static int palavraDeAudio(const char *t, const char *termo) {
  for (const char *p = palavraEm(t, t, termo); p; p = palavraEm(t, p + 1, termo))
    if (!deLegenda(t, p, strlen(termo))) return 1;
  return 0;
}
// Bandeira = dois "regional indicator" (F0 9F 87 A6+x): 🇬🇧 -> "GB".
static int bandeiraDeAudio(const char *t, const char *pais) {
  const unsigned char *u = (const unsigned char *)t;
  for (size_t i = 0; u[i]; i++) {
    if (u[i] != 0xF0 || u[i + 1] != 0x9F || u[i + 2] != 0x87 || u[i + 3] < 0xA6 || u[i + 3] > 0xBF) continue;
    if (u[i + 4] != 0xF0 || u[i + 5] != 0x9F || u[i + 6] != 0x87 || u[i + 7] < 0xA6 || u[i + 7] > 0xBF) continue;
    if ('A' + (u[i + 3] - 0xA6) == pais[0] && 'A' + (u[i + 7] - 0xA6) == pais[1] && !deLegenda(t, t + i, 8))
      return 1;
    i += 7;
  }
  return 0;
}

int fontenota_idioma(const char *t, const char *pref) {
  const Lingua *l = NULL;
  if (!t || !pref || !pref[0]) return FN_IDIOMA_NADA;
  for (size_t k = 0; k < sizeof LINGUAS / sizeof LINGUAS[0]; k++)
    if (ling_casa(LINGUAS[k].cod, pref)) { l = &LINGUAS[k]; break; }
  if (!l) return FN_IDIOMA_NADA;
  for (int i = 0; l->palavras[i]; i++) if (palavraDeAudio(t, l->palavras[i])) return FN_IDIOMA_EXPLICITO;
  for (int i = 0; l->bandeiras[i]; i++) if (bandeiraDeAudio(t, l->bandeiras[i])) return FN_IDIOMA_EXPLICITO;
  if (palavraDeAudio(t, "dual audio") || palavraDeAudio(t, "dual-audio") || palavraDeAudio(t, "dualaudio") ||
      palavraDeAudio(t, "dual") || palavraDeAudio(t, "multi audio") || palavraDeAudio(t, "multi-audio") ||
      palavraDeAudio(t, "multi"))
    return FN_IDIOMA_DUAL;
  return FN_IDIOMA_NADA;
}

// -------------------------------------------------------------------- nota

void fontenota_avaliar(const FonteNotaEntrada *e, FonteNota *n) {
  char t[192 + 2048 + 512 + 8];
  int dec, der = 0, teto, h;
  memset(n, 0, sizeof *n);
  if (!e) return;
  h = e->altura;
  snprintf(t, sizeof t, "%s\n%s\n%s", e->rotulo ? e->rotulo : "", e->descricao ? e->descricao : "",
           e->arquivo ? e->arquivo : "");

  n->tier = n->tierAnunciado = tierDe(t);
  if (n->tier == FN_TIER_DESCONHECIDO && e->tierDica > 0) n->tier = n->tierAnunciado = e->tierDica;

  // BITRATE: o declarado pelo addon, ou o tamanho dividido pela duracao. Sem
  // duracao o tamanho NAO vira bitrate (um episodio de 6 GB e um filme de 6 GB
  // sao coisas diferentes).
  teto = kbpsTeto(h);
  dec = fontenota_bitrate_declarado(t);
  if (e->duracaoSeg > 60) {
    double bytes = e->bytes ? (double)e->bytes : (double)e->tamanhoMB * 1048576.0;
    if (bytes > 0) der = (int)(bytes * 8.0 / e->duracaoSeg / 1000.0);
  }
  if (dec > teto) { dec = 0; n->alertas |= FN_A_BITRATE_IMPLAUSIVEL; }
  if (der > teto) { der = 0; n->alertas |= FN_A_BITRATE_IMPLAUSIVEL; }
  if (dec && der && (dec > der ? dec * 2 > der * 5 : der * 2 > dec * 5)) {
    n->alertas |= FN_A_BITRATE_CONFLITO;      // declarado contradiz o tamanho: nenhum vale
  } else if (dec) { n->kbps = dec; n->kbpsOrigem = FN_KBPS_DECLARADO; }
  else if (der) { n->kbps = der; n->kbpsOrigem = FN_KBPS_TAMANHO; }

  // NOME QUE MENTE: "REMUX" com bitrate de encode nao recebe ponto de remux.
  if (n->tier == FN_TIER_REMUX && n->kbps && n->kbps < kbpsRemuxMin(h)) {
    n->tier = FN_TIER_DESCONHECIDO;
    n->alertas |= FN_A_REMUX_IMPLAUSIVEL;
  }

  n->codec = codecDe(t);
  if (n->codec == FN_CODEC_DESCONHECIDO && e->codecDica > 0) n->codec = e->codecDica;
  n->bits10 = algum(t, DEZ_BITS, 0);
  n->audio = audioDe(t);
  if (n->audio == FN_AUD_DESCONHECIDO && e->audioDica > 0) n->audio = e->audioDica;

  n->ptsTier = ptsTier(n->tier);
  n->ptsBitrate = ptsBitrate(n->kbps, h);
  n->ptsAudio = n->audio == FN_AUD_SEM_PERDA ? FN_PTS_AUD_SEM_PERDA : n->audio == FN_AUD_SURROUND ? FN_PTS_AUD_SURROUND
              : n->audio == FN_AUD_BASICO ? FN_PTS_AUD_BASICO : 0;
  n->ptsCodec = n->bits10 && (n->codec == FN_CODEC_HEVC || n->codec == FN_CODEC_AV1) ? FN_PTS_10BIT : 0;

  // COMPATIBILIDADE: AVC 10-bit (Hi10P, comum em anime antigo) nao tem
  // decodificador de hardware em TV; 4K com cara de upscale compete como 1080p.
  if (e->semHi10p && (contem(t, "hi10") || (n->codec == FN_CODEC_AVC && n->bits10))) {
    n->ptsCompat += FN_PTS_HI10P;
    n->alertas |= FN_A_HI10P;
  }
  if (h >= 2160 && (contem(t, "upscal") || (n->kbps && n->kbps < 5000))) {
    n->ptsCompat += FN_PTS_UPSCALE;
    n->alertas |= FN_A_UPSCALE;
  }

  n->idioma = !e->idiomaPref || !e->idiomaPref[0] ? FN_IDIOMA_NADA
            : e->temIdiomaDeclarado ? e->idiomaDeclarado : fontenota_idioma(t, e->idiomaPref);
  n->ptsIdioma = n->idioma == FN_IDIOMA_EXPLICITO ? FN_PTS_IDIOMA : n->idioma == FN_IDIOMA_DUAL ? FN_PTS_IDIOMA_DUAL : 0;
}

long fontenota_evidencia(const FonteNota *n) {
  long p;
  if (!n) return 0;
  p = n->ptsTier + n->ptsBitrate + n->ptsAudio + n->ptsCodec;
  return p > FN_MAX_EVIDENCIA ? FN_MAX_EVIDENCIA : p;
}
long fontenota_ajuste(const FonteNota *n) { return n ? n->ptsIdioma + n->ptsCompat : 0; }

// Ultimo ponto do degrau de `p` (divisao para baixo, tambem para negativo:
// "Evitar HDR" pode deixar a base de uma fonte SD abaixo de zero).
static long topoDoDegrau(long p) {
  long d = p >= 0 ? p / FN_DEGRAU : -((-p + FN_DEGRAU - 1) / FN_DEGRAU);
  return (d + 1) * FN_DEGRAU - 1;
}

long fontenota_somar(long base, const FonteNota *n, int comEvidencia) {
  long q = base, topo;
  if (!n) return base;
  if (comEvidencia) {
    topo = topoDoDegrau(base);
    q += fontenota_evidencia(n);
    if (q > topo) q = topo;
  }
  // O idioma tambem fica no degrau: preferencia escolhe entre fontes da mesma
  // resolucao, nunca troca 4K por 1080p.
  if (n->ptsIdioma) {
    topo = topoDoDegrau(q);
    q += n->ptsIdioma;
    if (q > topo) q = topo;
  }
  return q + n->ptsCompat;
}

void fontenota_explicar(const FonteNota *n, char *dst, size_t tam) {
  static const char *const TIER[] = { "cam", "unknown", "hdtv", "webrip", "web-dl", "bluray", "remux" };
  static const char *const KO[] = { "none", "declared", "size/runtime" };
  static const char *const AU[] = { "unknown", "basic", "surround", "lossless" };
  static const char *const CO[] = { "unknown", "avc", "hevc", "av1" };
  static const char *const ID[] = { "none", "dual/multi", "preferred" };
  char al[96] = "";
  if (!dst || !tam) return;
  if (!n) { dst[0] = 0; return; }
  if (n->alertas & FN_A_REMUX_IMPLAUSIVEL) strcat(al, " remux-implausible");
  if (n->alertas & FN_A_BITRATE_IMPLAUSIVEL) strcat(al, " bitrate-implausible");
  if (n->alertas & FN_A_BITRATE_CONFLITO) strcat(al, " bitrate-conflict");
  if (n->alertas & FN_A_UPSCALE) strcat(al, " upscale");
  if (n->alertas & FN_A_HI10P) strcat(al, " hi10p");
  snprintf(dst, tam, "tier=%s kbps=%d(%s) audio=%s codec=%s%s lang=%s evidence=%+ld adjust=%+ld alerts=%s",
           TIER[n->tier + 1], n->kbps, KO[n->kbpsOrigem], AU[n->audio], CO[n->codec], n->bits10 ? "/10bit" : "",
           ID[n->idioma], fontenota_evidencia(n), fontenota_ajuste(n), al[0] ? al + 1 : "none");
}
