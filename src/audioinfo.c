#include "audioinfo.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// strcasestr is a GNU extension the webOS toolchain does not declare.
static const char *achaSemCaixa(const char *h, const char *n) {
  size_t ln = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, ln)) return h;
  return ln ? NULL : h;
}

// Prefixos, comparados SEM maiusculas, o mais especifico primeiro
// ("audio/eac3" antes de "eac3" nao importa; "A_DTS/LOSSLESS" antes de "A_DTS").
static const struct { const char *pre, *nome; } T[] = {
  // Matroska
  { "A_EAC3", "E-AC-3" }, { "A_AC3", "AC-3" }, { "A_TRUEHD", "TrueHD" },
  { "A_DTS/LOSSLESS", "DTS-HD MA" }, { "A_DTS/EXPRESS", "DTS-HD" }, { "A_DTS", "DTS" },
  { "A_AAC", "AAC" }, { "A_OPUS", "Opus" }, { "A_FLAC", "FLAC" }, { "A_VORBIS", "Vorbis" },
  { "A_MPEG/L3", "MP3" }, { "A_MPEG/L2", "MP2" }, { "A_PCM", "PCM" }, { "A_MLP", "MLP" },
  // mime do ExoPlayer
  { "audio/eac3", "E-AC-3" }, { "audio/ac3", "AC-3" }, { "audio/true-hd", "TrueHD" },
  { "audio/vnd.dts.hd", "DTS-HD" }, { "audio/vnd.dts.uhd", "DTS:X" }, { "audio/vnd.dts", "DTS" },
  { "audio/mp4a-latm", "AAC" }, { "audio/opus", "Opus" }, { "audio/flac", "FLAC" },
  { "audio/vorbis", "Vorbis" }, { "audio/mpeg", "MP3" }, { "audio/raw", "PCM" },
  // uMS da LG e FFmpeg
  { "eac3", "E-AC-3" }, { "ec3", "E-AC-3" }, { "ac3", "AC-3" }, { "truehd", "TrueHD" },
  { "dts-hd", "DTS-HD" }, { "dtshd", "DTS-HD" }, { "dts", "DTS" }, { "aac", "AAC" },
  { "opus", "Opus" }, { "flac", "FLAC" }, { "vorbis", "Vorbis" }, { "mp3", "MP3" },
  { "mp2", "MP2" }, { "pcm", "PCM" }, { "lpcm", "PCM" },
};

const char *audioinfo_codec(const char *c) {
  size_t i;
  if (!c || !*c) return "";
  if (achaSemCaixa(c, "dts-hd ma") || achaSemCaixa(c, "dtshd_ma") || achaSemCaixa(c, "dts_hd_ma"))
    return "DTS-HD MA";
  for (i = 0; i < sizeof T / sizeof *T; i++)
    if (!strncasecmp(c, T[i].pre, strlen(T[i].pre))) return T[i].nome;
  return "";
}

const char *audioinfo_canais(int n) {
  switch (n) {
    case 1: return "1.0"; case 2: return "2.0"; case 3: return "3.0"; case 4: return "4.0";
    case 5: return "5.0"; case 6: return "5.1"; case 7: return "6.1"; case 8: return "7.1";
    case 10: return "9.1";
    default: return "";
  }
}

int audioinfo_codec_atmos(const char *c) { return c && !strncasecmp(c, "audio/eac3-joc", 14); }

int audioinfo_texto(const char *codec, int canais, int atmos, char *out, size_t n) {
  const char *nome = audioinfo_codec(codec), *ch = audioinfo_canais(canais);
  int w;
  if (!out || !n) return 0;
  out[0] = 0;
  if (!nome[0]) return 0;
  w = snprintf(out, n, "%s%s%s%s", nome, ch[0] ? " " : "", ch,
               (atmos || audioinfo_codec_atmos(codec)) ? " Atmos" : "");
  return w < 0 || (size_t)w >= n ? (int)strlen(out) : w;
}
