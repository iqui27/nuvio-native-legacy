// #293: tabela de nomes e canais do codec de audio.
#include "audioinfo.h"
#include <stdio.h>
#include <string.h>
static int falhas;
static void t(const char *c, int ch, int atmos, const char *quer) {
  char o[48];
  audioinfo_texto(c, ch, atmos, o, sizeof o);
  if (strcmp(o, quer)) { printf("FALHOU %s/%d/%d -> \"%s\" (queria \"%s\")\n", c ? c : "NULL", ch, atmos, o, quer); falhas++; }
}
int main(void) {
  t("A_EAC3", 6, 0, "E-AC-3 5.1");
  t("EAC3", 6, 1, "E-AC-3 5.1 Atmos");
  t("audio/eac3-joc", 6, 0, "E-AC-3 5.1 Atmos");
  t("audio/eac3", 8, 0, "E-AC-3 7.1");
  t("A_TRUEHD", 8, 1, "TrueHD 7.1 Atmos");
  t("audio/true-hd", 8, 0, "TrueHD 7.1");
  t("A_DTS/LOSSLESS", 6, 0, "DTS-HD MA 5.1");
  t("A_DTS/EXPRESS", 6, 0, "DTS-HD 5.1");
  t("A_DTS", 6, 0, "DTS 5.1");
  t("dts", 2, 0, "DTS 2.0");
  t("audio/vnd.dts.hd;profile=lbr", 6, 0, "DTS-HD 5.1");
  t("audio/vnd.dts.uhd;profile=p2", 8, 0, "DTS:X 7.1");
  t("A_AC3", 6, 0, "AC-3 5.1");
  t("ac3", 2, 0, "AC-3 2.0");
  t("A_AAC/MPEG4/LC", 2, 0, "AAC 2.0");
  t("audio/mp4a-latm", 6, 0, "AAC 5.1");
  t("A_OPUS", 6, 0, "Opus 5.1");
  t("opus", 2, 0, "Opus 2.0");
  t("A_FLAC", 2, 0, "FLAC 2.0");
  t("mp3", 2, 0, "MP3 2.0");
  t("pcm_s16le", 2, 0, "PCM 2.0");
  t("A_EAC3", 0, 0, "E-AC-3");
  t("A_EAC3", 13, 0, "E-AC-3");
  t("wmapro", 2, 0, "");
  t("", 6, 1, "");
  t(NULL, 6, 1, "");
  if (strcmp(audioinfo_canais(7), "6.1")) { puts("FALHOU 6.1"); falhas++; }
  { char p[6]; audioinfo_texto("A_EAC3", 6, 1, p, sizeof p); if (strlen(p) >= sizeof p) { puts("FALHOU truncar"); falhas++; } }
  puts(falhas ? "audioinfo: FALHOU" : "audioinfo: ok");
  return falhas != 0;
}
