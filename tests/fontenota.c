// A ESCOLHA DE FONTE COM EVIDENCIA DE QUALIDADE (fontenota.h) e a fila do
// automatico (fonteauto.h), sem rede, sem SDL e sem a lista de streams.
//
// Os textos imitam o que o AIOStreams/TorBox manda de verdade (nome, descricao
// com os emojis do formatador, nome do arquivo). A pontuacao base e a MESMA
// conta de streams.c (qualidade(): degrau * 10000 + HDR + altura/10 + MP4 +
// Atmos); a fila e fonteauto_fila_o, a mesma de stream_primeira_boa e do
// failover. Cada caso confere QUEM vence e POR QUE (as parcelas da nota).
//
//   bash tests/fontenota.sh
#include "fontenota.h"
#include "fonteauto.h"
#include "fonteregra.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define GB(x) ((uint64_t)((x) * 1073741824.0))
#define MB(x) ((uint64_t)((x) * 1048576.0))

typedef struct {
  const char *nome, *descricao, *arquivo;
  int altura, hdr, mp4, atmos;   // hdr = nivelHdr de streams.c (0 SDR .. 4 DV)
  uint64_t bytes;
  int addonRank;                  // posicao do add-on na "Ordem dos add-ons"
  int excluida;                   // foraDoAuto: fora de cache, aviso, decoder, morta
} Fixture;

static double duracao;            // duracao conhecida do titulo (0 = nao)
static const char *idioma;        // preferencia de idioma (NULL = desligada)
static int prioridade = 1;        // 0 Equilibrio, 1 Qualidade maxima, 2 Comecar rapido

static void avaliar(const Fixture *f, FonteNota *n) {
  FonteNotaEntrada e;
  memset(&e, 0, sizeof e);
  e.rotulo = f->nome; e.descricao = f->descricao; e.arquivo = f->arquivo;
  e.altura = f->altura; e.bytes = f->bytes; e.duracaoSeg = duracao;
  e.idiomaPref = idioma; e.semHi10p = 1;
  fontenota_avaliar(&e, n);
}
static int degrau(int h) { return h >= 2160 ? 3 : h >= 1080 ? 2 : h >= 720 ? 1 : 0; }
// streams.c qualidade() (HDR "Preferir", Atmos ligado), sem as multas.
static long base(const Fixture *f) {
  long q = prioridade == 0 ? (degrau(f->altura) + (f->hdr > 0)) * 10000L + f->hdr * 1200L
                           : degrau(f->altura) * 10000L + f->hdr * 1500L;
  q += f->altura / 10;
  if (f->mp4) q += 300;
  if (f->atmos) q += 200;
  return q;
}
static long pontos(const Fixture *f, int comNota) {
  FonteNota n;
  if (!comNota) return base(f);
  avaliar(f, &n);
  return fontenota_somar(base(f), &n, prioridade != 2);
}

// A fila do automatico sobre as fixtures. Devolve o indice da vencedora;
// o tamanho da fila fica em ultimaFila.
static int ultimaFila;
static int fila(const Fixture *l, int total, int modo, int preferida, int ordemUso, int comNota,
                int *saida, int max) {
  long pts[16]; unsigned char excl[16]; int rk[16], f1[16], nf;
  int *dst = saida ? saida : f1;
  assert(total <= 16);
  for (int i = 0; i < total; i++) {
    pts[i] = pontos(&l[i], comNota);
    excl[i] = (unsigned char)l[i].excluida;
    rk[i] = l[i].addonRank;
  }
  nf = fonteauto_fila_o(modo, total, preferida, pts, NULL, excl, NULL, ordemUso ? rk : NULL, ordemUso,
                        max, dst);
  ultimaFila = nf;
  return nf > 0 ? dst[0] : -1;
}
static int melhor(const Fixture *l, int total) { return fila(l, total, FONTEAUTO_MELHOR, -1, FR_ORDEM_NAO, 1, NULL, 1); }
static void porque(const char *caso, const Fixture *f) {
  FonteNota n; char t[256];
  avaliar(f, &n); fontenota_explicar(&n, t, sizeof t);
  printf("  %-34s -> \"%s\" {%s} points=%ld\n", caso, f->nome, t, pontos(f, 1));
}

// ---------------------------------------------------------------- fixtures

// Code Geass S1E3, as duas linhas reais (AIOStreams + TorBox, em cache).
static const Fixture REMUX_CG = {
  "[TB⚡] AIOStreams 1080p",
  "🎞️ BluRay REMUX | AVC | 🎧 FLAC 2.0\n🌐 English | Japanese\n💾 6.57 GB 📊 36.2 Mbps\n🏷️ neoVzn · Knaben",
  "Code.Geass.S01E03.1080p.BluRay.REMUX.AVC.FLAC.2.0-neoVzn.mkv",
  1080, 0, 0, 0, GB(6.57), 0, 0 };
static const Fixture WEBDL_CG = {
  "[TB⚡] AIOStreams 1080p",
  "🎞️ WEB-DL | AVC | 🎧 AAC\n🌐 Japanese\n💾 421 MB 📊 2.3 Mbps\n🏷️ Lilith-Raws · mikan",
  "[Lilith-Raws] Code Geass - 03 [Baha][WEB-DL][1080p][AVC AAC][CHT][MP4].mp4",
  1080, 0, 1, 0, MB(421), 0, 0 };

static void codeGeass(void) {
  Fixture l[2] = { WEBDL_CG, REMUX_CG };   // o WEB-DL veio primeiro na lista
  FonteNota r, w;
  printf("Code Geass S1E3\n");
  // A CAUSA: sem a nota, as duas sao 1080p SDR e o +300 do MP4 decide.
  assert(pontos(&l[0], 0) == 20408 && pontos(&l[1], 0) == 20108);
  assert(fila(l, 2, FONTEAUTO_MELHOR, -1, FR_ORDEM_NAO, 0, NULL, 1) == 0);
  // DEPOIS: o REMUX vence, e por evidencia de verdade.
  for (prioridade = 0; prioridade < 2; prioridade++) assert(melhor(l, 2) == 1);
  prioridade = 1;
  avaliar(&l[1], &r); avaliar(&l[0], &w);
  assert(r.tier == FN_TIER_REMUX && r.kbps == 36200 && r.kbpsOrigem == FN_KBPS_DECLARADO);
  assert(r.audio == FN_AUD_SEM_PERDA && r.codec == FN_CODEC_AVC && !r.alertas);
  assert(w.tier == FN_TIER_WEBDL && w.kbps == 2300 && w.ptsBitrate == 0 && w.audio == FN_AUD_BASICO);
  assert(fontenota_evidencia(&r) == FN_PTS_REMUX + FN_PTS_BITRATE_MAX + FN_PTS_AUD_SEM_PERDA);
  // Memoria do automatico (stream_grupo_sem_perda): a diferenca passa da margem de 300.
  assert(fontenota_evidencia(&r) - fontenota_evidencia(&w) > 300);
  // Sem bitrate no texto, a duracao (24 min) faz o mesmo papel pelo tamanho.
  { Fixture a = REMUX_CG, b = WEBDL_CG; FonteNota na, nb;
    a.descricao = "BluRay REMUX | AVC | FLAC"; b.descricao = "WEB-DL | AVC | AAC";
    duracao = 24 * 60; avaliar(&a, &na); avaliar(&b, &nb); duracao = 0;
    assert(na.kbpsOrigem == FN_KBPS_TAMANHO && na.kbps > 35000 && na.kbps < 41000);
    assert(nb.kbpsOrigem == FN_KBPS_TAMANHO && nb.kbps < 2600); }
  // "Comecar rapido" nao usa a evidencia: continua preferindo o menor/MP4.
  prioridade = 2; assert(melhor(l, 2) == 0); prioridade = 1;
  porque("winner (Maximum quality)", &l[1]);
  porque("runner-up", &l[0]);
}

static void anime(void) {
  Fixture jp = { "AIOStreams 1080p", "BluRay REMUX | AVC | FLAC 2.0\n🇯🇵 Japanese | English Subs",
                 "Show.S01E01.1080p.BluRay.REMUX.AVC.FLAC.2.0-Grp.mkv", 1080, 0, 0, 0, GB(7), 0, 0 };
  Fixture dual = { "AIOStreams 1080p", "BluRay | HEVC 10bit | FLAC + AC3\n🌐 Dual Audio · English | Japanese",
                   "[Grp] Show - 01 (BD 1080p HEVC 10-bit) [Dual Audio].mkv", 1080, 0, 0, 0, GB(1.4), 0, 0 };
  Fixture dub = { "AIOStreams 1080p", "WEB-DL | AVC | AAC\n🇬🇧 English Dub", "Show.S01E01.1080p.CR.WEB-DL.ENG.DUB.mkv",
                  1080, 0, 0, 0, GB(1.1), 0, 0 };
  Fixture jp720 = { "AIOStreams 720p", "WEB-DL | 🇯🇵", "Show.01.720p.WEB-DL.mkv", 720, 0, 0, 0, MB(500), 0, 0 };
  Fixture l[2] = { jp, dual };
  FonteNota n;
  printf("Anime: dub / dual audio / Japanese only\n");
  idioma = NULL;  assert(melhor(l, 2) == 0);           // sem preferencia: a qualidade decide (REMUX)
  idioma = "en";  assert(melhor(l, 2) == 1);           // ingles preferido: a dual audio passa
  porque("pref=en winner", &l[1]);
  avaliar(&jp, &n); assert(n.idioma == FN_IDIOMA_NADA);  // "English Subs" e legenda, nao audio
  avaliar(&dual, &n); assert(n.idioma == FN_IDIOMA_EXPLICITO && n.bits10 && n.codec == FN_CODEC_HEVC && n.ptsCodec == FN_PTS_10BIT);
  { Fixture m[2] = { jp, dub }; assert(melhor(m, 2) == 1); }        // dub WEB-DL > REMUX japones
  { Fixture m[1] = { jp }; assert(melhor(m, 1) == 0); }             // so japones: toca (nao e filtro)
  { Fixture m[2] = { jp, jp720 }; assert(melhor(m, 2) == 0); }      // nenhuma em ingles: qualidade
  { Fixture m[2] = { jp, dub }; m[1].altura = 720; assert(melhor(m, 2) == 0); }  // dub 720p nao passa de 1080p
  // "Dual Audio" sem dizer quais conta menos que o idioma escrito.
  assert(fontenota_idioma("[Grp] Show [Dual-Audio]", "en") == FN_IDIOMA_DUAL);
  assert(fontenota_idioma("Show 🇬🇧 🇯🇵", "en") == FN_IDIOMA_EXPLICITO);
  assert(fontenota_idioma("Subs: 🇬🇧", "en") == FN_IDIOMA_NADA);
  assert(fontenota_idioma("Japanese", "ja") == FN_IDIOMA_EXPLICITO);
  assert(fontenota_idioma("Dublado PT-BR", "pt-BR") == FN_IDIOMA_EXPLICITO);
  assert(fontenota_idioma("English", "none") == FN_IDIOMA_NADA && fontenota_idioma("English", "*") == FN_IDIOMA_NADA);
  // AUDIO DECLARADO PELO ADDON (audioIdiomas) prevalece sobre o texto: "Dual
  // Audio" no nome com a lista so em japones nao ganha o bonus; a lista com
  // ingles ganha mesmo sem o texto dizer nada; sem preferencia, nada.
  { FonteNotaEntrada e; FonteNota m;
    memset(&e, 0, sizeof e);
    e.rotulo = dual.nome; e.descricao = dual.descricao; e.arquivo = dual.arquivo; e.altura = 1080;
    e.idiomaPref = "en"; e.semHi10p = 1;
    e.temIdiomaDeclarado = 1; e.idiomaDeclarado = FN_IDIOMA_NADA;
    fontenota_avaliar(&e, &m); assert(m.idioma == FN_IDIOMA_NADA && m.ptsIdioma == 0);
    e.rotulo = jp720.nome; e.descricao = jp720.descricao; e.arquivo = jp720.arquivo; e.altura = 720;
    e.idiomaDeclarado = FN_IDIOMA_EXPLICITO;
    fontenota_avaliar(&e, &m); assert(m.idioma == FN_IDIOMA_EXPLICITO && m.ptsIdioma == FN_PTS_IDIOMA);
    e.idiomaPref = "";
    fontenota_avaliar(&e, &m); assert(m.idioma == FN_IDIOMA_NADA && m.ptsIdioma == 0); }
  idioma = NULL;
}

static void genericDubIsNotEnglish(void) {
  Fixture l[2] = {
    { "1080p", "BluRay REMUX Hindi Dub", "", 1080, 0, 0, 0, 0, 0, 0 },
    { "1080p", "WEB-DL English Dub", "", 1080, 0, 0, 0, 0, 0, 0 }
  };
  idioma = "en";
  assert(melhor(l, 2) == 1);
  assert(fontenota_idioma("Hindi Dub", "en") == FN_IDIOMA_NADA);
  assert(fontenota_idioma("Tamil Dubbed", "en") == FN_IDIOMA_NADA);
  assert(fontenota_idioma("Dubs", "en") == FN_IDIOMA_NADA);
  assert(fontenota_idioma("English Dub", "en") == FN_IDIOMA_EXPLICITO);
  idioma = NULL;
}

static void supportedAudioLanguages(void) {
  static const struct { const char *code, *alias, *name, *flag; } cases[] = {
    { "pt", "pob", "Portuguese", "🇵🇹" }, { "en", "eng", "English", "🇬🇧" },
    { "es", "spa", "Spanish", "🇪🇸" }, { "de", "deu", "German", "🇩🇪" },
    { "ar", "ara", "Arabic", "🇸🇦" }, { "zh", "zho", "Chinese", "🇨🇳" },
    { "da", "dan", "Danish", "🇩🇰" }, { "ko", "kor", "Korean", "🇰🇷" },
    { "fr", "fra", "French", "🇫🇷" }, { "el", "ell", "Greek", "🇬🇷" },
    { "he", "heb", "Hebrew", "🇮🇱" }, { "nl", "nld", "Dutch", "🇳🇱" },
    { "hi", "hin", "Hindi", "🇮🇳" }, { "hu", "hun", "Hungarian", "🇭🇺" },
    { "id", "ind", "Indonesian", "🇮🇩" }, { "it", "ita", "Italian", "🇮🇹" },
    { "ja", "jpn", "Japanese", "🇯🇵" }, { "no", "nor", "Norwegian", "🇳🇴" },
    { "pl", "pol", "Polish", "🇵🇱" }, { "ro", "ron", "Romanian", "🇷🇴" },
    { "ru", "rus", "Russian", "🇷🇺" }, { "sv", "swe", "Swedish", "🇸🇪" },
    { "th", "tha", "Thai", "🇹🇭" }, { "cs", "ces", "Czech", "🇨🇿" },
    { "tr", "tur", "Turkish", "🇹🇷" }, { "uk", "ukr", "Ukrainian", "🇺🇦" },
    { "vi", "vie", "Vietnamese", "🇻🇳" }, { "fi", "fin", "Finnish", "🇫🇮" }
  };
  Fixture l[2] = {
    { "1080p", "WEB-DL Japanese", "", 1080, 0, 0, 0, 0, 0, 0 },
    { "1080p", "WEB-DL Swedish", "", 1080, 0, 0, 0, 0, 0, 0 }
  };
  idioma = "sv";
  assert(melhor(l, 2) == 1);
  idioma = NULL;
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    char subs[80], regional[16];
    assert(fontenota_idioma(cases[i].name, cases[i].code) == FN_IDIOMA_EXPLICITO);
    assert(fontenota_idioma(cases[i].flag, cases[i].code) == FN_IDIOMA_EXPLICITO);
    assert(fontenota_idioma("Dual Audio", cases[i].code) == FN_IDIOMA_DUAL);
    assert(fontenota_idioma(cases[i].name, cases[i].alias) == FN_IDIOMA_EXPLICITO);
    snprintf(regional, sizeof regional, "%s-XX", cases[i].code);
    assert(fontenota_idioma(cases[i].name, regional) == FN_IDIOMA_EXPLICITO);
    snprintf(subs, sizeof subs, "Subs: %s", cases[i].name);
    assert(fontenota_idioma(subs, cases[i].code) == FN_IDIOMA_NADA);
  }
  assert(fontenota_idioma("Dutch", "dut") == FN_IDIOMA_EXPLICITO);
  assert(fontenota_idioma("Czech", "cze") == FN_IDIOMA_EXPLICITO);
  assert(fontenota_idioma("Chinese", "chi") == FN_IDIOMA_EXPLICITO);
  assert(fontenota_idioma("Dual Audio", "none") == FN_IDIOMA_NADA);
  assert(fontenota_idioma("Dual Audio", "original") == FN_IDIOMA_NADA);
  assert(fontenota_idioma("Dual Audio", "") == FN_IDIOMA_NADA);
}

static void quatroK(void) {
  Fixture dv = { "AIOStreams 2160p", "BluRay REMUX | HEVC | DV | TrueHD Atmos 7.1", "Movie.2160p.UHD.BluRay.REMUX.DV.HDR.HEVC.TrueHD.Atmos.7.1-Grp.mkv",
                 2160, 4, 0, 1, GB(62), 0, 0 };
  Fixture sdr = { "AIOStreams 1080p", "BluRay REMUX | AVC | DTS-HD MA 5.1", "Movie.1080p.BluRay.REMUX.AVC.DTS-HD.MA.5.1-Grp.mkv",
                  1080, 0, 0, 0, GB(30), 0, 0 };
  Fixture dvSemDado = { "Torrentio 4k DV", "", "", 2160, 4, 0, 0, 0, 0, 0 };
  Fixture up = { "AIOStreams 2160p", "WEB | AI Upscale | HEVC", "Movie.2160p.AI.Upscale.HEVC-Grp.mkv", 2160, 0, 0, 0, GB(9), 0, 0 };
  Fixture up2 = { "AIOStreams 2160p", "HEVC 📊 3.1 Mbps", "Movie.2160p.HEVC-Grp.mkv", 2160, 0, 0, 0, GB(2.7), 0, 0 };
  Fixture bd = { "AIOStreams 1080p", "BluRay | x264 | DTS 5.1 📊 11 Mbps", "Movie.1080p.BluRay.x264.DTS-Grp.mkv", 1080, 0, 0, 0, GB(9.5), 0, 0 };
  Fixture web4k = { "AIOStreams 2160p", "WEB-DL | HEVC | DDP 5.1 📊 16 Mbps", "Movie.2160p.WEB-DL.DDP5.1.HEVC-Grp.mkv", 2160, 0, 0, 0, GB(14), 0, 0 };
  printf("4K, Dolby Vision and upscales\n");
  for (prioridade = 0; prioridade < 2; prioridade++) {
    { Fixture l[2] = { sdr, dv }; assert(melhor(l, 2) == 1); }        // 4K DV REMUX > 1080p SDR REMUX
    { Fixture l[2] = { sdr, dvSemDado }; assert(melhor(l, 2) == 1); } // 4K DV sem metadado: nenhuma multa
    { Fixture l[2] = { up, bd }; assert(melhor(l, 2) == 1); }         // upscale declarado: o Blu-ray 1080p
    { Fixture l[2] = { up2, bd }; assert(melhor(l, 2) == 1); }        // 4K de 3 Mbps: o Blu-ray 1080p
    { Fixture l[2] = { bd, web4k }; assert(melhor(l, 2) == 1); }      // 4K WEB-DL de verdade: o 4K
  }
  prioridade = 1;
  { FonteNota n; avaliar(&up2, &n); assert(n.alertas & FN_A_UPSCALE && n.ptsCompat == FN_PTS_UPSCALE); }
  { FonteNota n; avaliar(&dv, &n); assert(!n.alertas && n.tier == FN_TIER_REMUX && n.audio == FN_AUD_SEM_PERDA); }
  { Fixture l[2] = { sdr, dv }; porque("4K DV REMUX winner", &l[1]); }
  { Fixture l[2] = { up2, bd }; porque("1080p Blu-ray over 4K 3 Mbps", &l[1]); }
}

static void codecs(void) {
  Fixture avc = { "AIOStreams 1080p", "BluRay | x264 | AC3 📊 8 Mbps", "M.1080p.BluRay.x264.AC3-A.mkv", 1080, 0, 0, 0, 0, 0, 0 };
  Fixture hevc = { "AIOStreams 1080p", "BluRay | x265 10bit | AC3 📊 8 Mbps", "M.1080p.BluRay.x265.10bit.AC3-B.mkv", 1080, 0, 0, 0, 0, 0, 0 };
  Fixture hi10 = { "AIOStreams 1080p", "BD | Hi10P | FLAC 📊 9 Mbps", "[Old] Show - 01 (BD 1080p Hi10P FLAC).mkv", 1080, 0, 0, 0, 0, 0, 0 };
  Fixture remuxAvc = { "AIOStreams 1080p", "BluRay REMUX | AVC | AC3", "M.1080p.BluRay.REMUX.AVC-C.mkv", 1080, 0, 0, 0, 0, 0, 0 };
  FonteNota n;
  printf("Codecs\n");
  { Fixture l[2] = { avc, hevc }; assert(melhor(l, 2) == 1); }   // HEVC 10-bit genuino: desempate pequeno
  assert(pontos(&hevc, 1) - pontos(&avc, 1) == FN_PTS_10BIT);    // e so isso: nenhum vies de codec
  { Fixture l[2] = { hi10, avc }; assert(melhor(l, 2) == 1); }   // Hi10P nao decodifica na TV
  avaliar(&hi10, &n); assert(n.alertas & FN_A_HI10P);
  { Fixture l[2] = { hevc, remuxAvc }; assert(melhor(l, 2) == 1); } // AVC REMUX > HEVC encode
  { Fixture l[2] = { hi10, avc }; porque("AVC 8-bit over Hi10P", &l[1]); }
}

static void metadados(void) {
  Fixture semBitrate = { "AIOStreams 1080p", "BluRay REMUX | AVC", "M.1080p.BluRay.REMUX.AVC-A.mkv", 1080, 0, 0, 0, GB(25), 0, 0 };
  Fixture semTamanho = { "AIOStreams 1080p", "WEB-DL | AVC | AAC", "M.1080p.WEB-DL.AAC.H264-B.mkv", 1080, 0, 0, 0, 0, 0, 0 };
  Fixture mentira = { "AIOStreams 1080p", "REMUX | AVC", "Show.S01E01.1080p.REMUX-Fake.mkv", 1080, 0, 0, 0, GB(1.2), 0, 0 };
  Fixture honesto = { "AIOStreams 1080p", "WEB-DL | DDP 5.1", "Show.S01E01.1080p.WEB-DL.DDP5.1-Ok.mkv", 1080, 0, 0, 0, GB(1.3), 0, 0 };
  Fixture gordo = { "SomeAddon 1080p", "", "Show.S01E01.1080p.mkv", 1080, 0, 0, 0, GB(40), 0, 0 };
  Fixture absurdo = { "AIOStreams 1080p", "WEB-DL 📊 250 Mbps", "", 1080, 0, 0, 0, 0, 0, 0 };
  Fixture conflito = { "AIOStreams 1080p", "BluRay 📊 36 Mbps", "", 1080, 0, 0, 0, GB(0.5), 0, 0 };
  Fixture bytesB = { "x", "⚡ 12 MB/s", "", 1080, 0, 0, 0, 0, 0, 0 };
  FonteNota n;
  printf("Missing and misleading metadata\n");
  avaliar(&semBitrate, &n);
  assert(n.tier == FN_TIER_REMUX && n.kbps == 0 && n.ptsBitrate == 0);     // sem duracao, tamanho nao vira bitrate
  avaliar(&semTamanho, &n); assert(n.tier == FN_TIER_WEBDL && n.kbps == 0);
  { Fixture l[2] = { semTamanho, semBitrate }; assert(melhor(l, 2) == 1); }
  // Episodio de 24 min: "REMUX" de 1,2 GB = ~7 Mbps. Nome que mente nao ganha ponto de REMUX.
  duracao = 24 * 60;
  avaliar(&mentira, &n);
  assert(n.tierAnunciado == FN_TIER_REMUX && n.tier == FN_TIER_DESCONHECIDO && (n.alertas & FN_A_REMUX_IMPLAUSIVEL));
  { Fixture l[2] = { mentira, honesto }; assert(melhor(l, 2) == 1); porque("honest WEB-DL over fake REMUX", &l[1]); }
  duracao = 0;
  // Arquivo enorme sem nada que diga o que e: nao domina uma fonte descrita.
  { Fixture l[2] = { gordo, honesto }; assert(melhor(l, 2) == 1); }
  avaliar(&gordo, &n); assert(fontenota_evidencia(&n) == 0);
  avaliar(&absurdo, &n); assert(n.kbps == 0 && (n.alertas & FN_A_BITRATE_IMPLAUSIVEL));
  duracao = 2 * 3600;   // 0,5 GB em 2 h = ~0,6 Mbps contra 36 declarados
  avaliar(&conflito, &n); assert(n.kbps == 0 && (n.alertas & FN_A_BITRATE_CONFLITO));
  duracao = 0;
  avaliar(&bytesB, &n); assert(n.kbps == 0);                     // MB/s e velocidade, nao bitrate
  assert(fontenota_bitrate_declarado("📊 8,5 Mbps") == 8500);
  assert(fontenota_bitrate_declarado("2400 kbps") == 2400);
  assert(fontenota_bitrate_declarado("x264 1080p") == 0);
  // Log seguro: nenhum texto da fonte (nem URL) sai em fontenota_explicar.
  { Fixture u = REMUX_CG; char t[256];
    u.descricao = "https://api.torbox.app/v1/api/torrents/requestdl?token=SECRET123&torrent_id=9 BluRay REMUX";
    avaliar(&u, &n); fontenota_explicar(&n, t, sizeof t);
    assert(!strstr(t, "SECRET") && !strstr(t, "http") && !strstr(t, "torbox")); }
  // Dicas dos selos: o `title` do addon so sobrevive nos selos. Valem so
  // quando o texto guardado nao diz nada, e nunca contra ele.
  { FonteNotaEntrada e; memset(&e, 0, sizeof e);
    e.rotulo = "Torrentio 1080p"; e.altura = 1080;
    e.tierDica = FN_TIER_REMUX; e.audioDica = FN_AUD_SEM_PERDA; e.codecDica = FN_CODEC_AVC;
    fontenota_avaliar(&e, &n);
    assert(n.tier == FN_TIER_REMUX && n.audio == FN_AUD_SEM_PERDA && n.codec == FN_CODEC_AVC);
    e.descricao = "WEB-DL AAC x265"; fontenota_avaliar(&e, &n);
    assert(n.tier == FN_TIER_WEBDL && n.audio == FN_AUD_BASICO && n.codec == FN_CODEC_HEVC); }
}

static void filas(void) {
  Fixture l[4] = { WEBDL_CG, REMUX_CG, REMUX_CG, WEBDL_CG };
  int f[4], nf;
  printf("Queue semantics: cache, First in List, addon order, remembered, failover\n");
  // Fora de cache (ou aviso/decoder/morta): fora do automatico, mesmo sendo o REMUX.
  l[1].excluida = 1; l[2].excluida = 1;
  assert(melhor(l, 4) == 0);
  l[1].excluida = 0; l[2].excluida = 0;
  // PRIMEIRA DA LISTA: a ordem do addon e so ela; a qualidade nao reordena.
  assert(fila(l, 4, FONTEAUTO_PRIMEIRA, -1, FR_ORDEM_NAO, 1, NULL, 1) == 0);
  l[0].excluida = 1;   // a primeira nao toca: a seguinte NA ORDEM
  assert(fila(l, 4, FONTEAUTO_PRIMEIRA, -1, FR_ORDEM_NAO, 1, NULL, 1) == 1);
  l[0].excluida = 0;
  // Ordem dos add-ons: add-on 0 tem so o WEB-DL, add-on 1 tem o REMUX.
  l[0].addonRank = 0; l[1].addonRank = 1; l[2].addonRank = 1; l[3].addonRank = 0;
  assert(fila(l, 4, FONTEAUTO_MELHOR, -1, FR_ORDEM_ESTRITA, 1, NULL, 1) == 0);   // estrita: o add-on de cima
  // Desempate: mesma faixa (1080p) = mesma qualidade, vence o add-on de cima.
  assert(fila(l, 4, FONTEAUTO_MELHOR, -1, FR_ORDEM_DESEMPATE, 1, NULL, 1) == 0);
  // ...mas dentro do MESMO add-on a evidencia decide.
  l[1].addonRank = 0;
  assert(fila(l, 4, FONTEAUTO_MELHOR, -1, FR_ORDEM_ESTRITA, 1, NULL, 1) == 1);
  assert(fila(l, 4, FONTEAUTO_MELHOR, -1, FR_ORDEM_DESEMPATE, 1, NULL, 1) == 1);
  // Desempate nao segura resolucao: um 4K num add-on de baixo passa.
  { Fixture m[2] = { WEBDL_CG, REMUX_CG }; m[0].addonRank = 0; m[1].addonRank = 1; m[1].altura = 2160;
    assert(fila(m, 2, FONTEAUTO_MELHOR, -1, FR_ORDEM_DESEMPATE, 1, NULL, 1) == 1); }
  // LEMBRADA (escolha manual neste titulo): na frente nos dois modos.
  assert(fila(l, 4, FONTEAUTO_MELHOR, 3, FR_ORDEM_NAO, 1, NULL, 1) == 3);
  assert(fila(l, 4, FONTEAUTO_PRIMEIRA, 3, FR_ORDEM_NAO, 1, NULL, 1) == 3);
  // FAILOVER (falhaCandidatas): a mesma pontuacao ordena as alternativas; uma
  // candidata que o chamador exclui (excl) fica de fora da fila.
  { Fixture m[4] = { REMUX_CG, WEBDL_CG, REMUX_CG, REMUX_CG };
    m[2].descricao = "BluRay | AVC | FLAC 📊 12 Mbps"; m[2].arquivo = "Code.Geass.S01E03.1080p.BluRay.x264.FLAC-Enc.mkv";
    m[3].altura = 720; m[3].excluida = 1;   // excluida pelo chamador
    fila(m, 4, FONTEAUTO_MELHOR, 0, FR_ORDEM_NAO, 1, f, 4);
    nf = ultimaFila;
    assert(nf == 3 && f[0] == 0 && f[1] == 2 && f[2] == 1);   // falhou o REMUX: Blu-ray, depois WEB-DL
  }
}

int main(void) {
  codeGeass();
  anime();
  genericDubIsNotEnglish();
  supportedAudioLanguages();
  quatroK();
  codecs();
  metadados();
  filas();
  printf("fontenota: tudo ok\n");
  return 0;
}
