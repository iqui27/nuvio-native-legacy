// A ordem do audio automatico (audioescolha.h, dono 15/10/2026): escolhida a
// mao nesta reproducao > idioma preferido > segundo idioma > original > nada;
// em cada idioma, a que nao e DTS/TrueHD antes; o que o alvo nao toca, nunca.
//
//   bash tests/audioescolha.sh
#include "audioescolha.h"
#include "linguas.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }

#define A(i, cod, rot, ch, d, p) { i, cod, rot, ch, d, p, -1 }
int main(void) {
  // Ingles DTS-HD, Ingles AC3, Sueco TrueHD, Japones AAC (original), sem etiqueta.
  AudioCand f[] = {
    A("en", "A_DTS", "English · DTS-HD MA 7.1", 8, 1, 1),
    A("en", "A_AC3", "English · AC-3 5.1", 6, 0, 1),
    A("sv", "A_TRUEHD", "Svenska · TrueHD 7.1", 8, 1, 1),
    A("ja", "A_AAC", "Japanese · AAC 2.0", 2, 0, 1),
    A("", "A_EAC3", "Audio 5", 6, 0, 1),
  };
  int n = sizeof f / sizeof *f, o;
  AudioLembrada m;

  ok(audioesc_escolher_com(f, n, NULL, "en", "", "", &o) == 1 && o == AUDIOESC_PREF1,
     "preferred English: the AC-3 one, not the DTS one");
  ok(audioesc_escolher_com(f, n, NULL, "sv", "en", "", &o) == 2 && o == AUDIOESC_PREF1,
     "preferred Swedish exists only in TrueHD: TrueHD (it converts)");
  f[2].pode = 0;
  ok(audioesc_escolher_com(f, n, NULL, "sv", "en", "", &o) == 1 && o == AUDIOESC_PREF2,
     "Swedish TrueHD the target cannot play: the second language, English AC-3");
  f[2].pode = 1;
  ok(audioesc_escolher_com(f, n, NULL, "de", "fr", "ja", &o) == 3 && o == AUDIOESC_ORIGINAL,
     "neither preferred language in the file: the original (Japanese)");
  ok(audioesc_escolher_com(f, n, NULL, "de", "", "", &o) == -1 && o == AUDIOESC_NADA,
     "nothing matches: -1, the file's choice stays");
  ok(audioesc_escolher_com(f, n, NULL, "", "", "", &o) == -1, "no preference at all: -1");
  ok(audioesc_escolher_com(f, n, NULL, "~", "", "", &o) == -1, "a sentinel never matches a track");

  // Picked by hand: wins over every preference, matched by language+codec+channels.
  memset(&m, 0, sizeof m);
  snprintf(m.idioma, sizeof m.idioma, "en"); snprintf(m.codec, sizeof m.codec, "dts");
  snprintf(m.rotulo, sizeof m.rotulo, "English · DTS-HD MA 7.1 → Dolby Digital 5.1"); m.canais = 8;
  ok(audioesc_escolher_com(f, n, &m, "ja", "", "", &o) == 0 && o == AUDIOESC_LEMBRADA,
     "picked English DTS by hand earlier: English DTS again (label with the arrow still matches)");
  snprintf(m.idioma, sizeof m.idioma, "eng"); snprintf(m.codec, sizeof m.codec, "A_AC3");
  snprintf(m.rotulo, sizeof m.rotulo, "Inglês"); m.canais = 6;
  ok(audioesc_escolher_com(f, n, &m, "ja", "", "", &o) == 1, "another source, eng AC-3 picked: en AC-3 here");
  snprintf(m.idioma, sizeof m.idioma, "fr"); snprintf(m.codec, sizeof m.codec, "A_AC3");
  ok(audioesc_escolher_com(f, n, &m, "ja", "", "", &o) == 3 && o == AUDIOESC_PREF1,
     "picked French earlier, no French here: the preferences decide (Japanese)");
  memset(&m, 0, sizeof m);
  snprintf(m.rotulo, sizeof m.rotulo, "Audio 5");
  ok(audioesc_escolher_com(f, n, &m, "en", "", "", &o) == 4, "unlabelled track picked by hand: same label again");

  // THE SAME RELEASE (bingeGroup): the exact track by number, even when two
  // tracks look alike. Two Japanese AAC 2.0 (main and commentary); the second
  // was picked by hand.
  { AudioCand g[] = {
      A("en", "A_AAC", "English · AAC 2.0", 2, 0, 1),
      A("ja", "A_AAC", "Japanese · AAC 2.0", 2, 0, 1),
      A("ja", "A_AAC", "Japanese · AAC 2.0", 2, 0, 1),
    };
    int k;
    for (k = 0; k < 3; k++) g[k].numero = k + 1;
    memset(&m, 0, sizeof m);
    snprintf(m.idioma, sizeof m.idioma, "ja"); snprintf(m.codec, sizeof m.codec, "A_AAC");
    snprintf(m.rotulo, sizeof m.rotulo, "Japanese · AAC 2.0"); m.canais = 2; m.numero = 3;
    snprintf(m.binge, sizeof m.binge, "aio|SubsPlease|1080p");
    ok(audioesc_escolher_binge(g, 3, &m, "aio|SubsPlease|1080p", "en", "", "", &o) == 2 && o == AUDIOESC_LEMBRADA,
       "same bingeGroup: the exact track picked by hand (the second Japanese)");
    ok(audioesc_escolher_binge(g, 3, &m, "aio|Erai|1080p", "en", "", "", &o) == 1 && o == AUDIOESC_LEMBRADA,
       "another bingeGroup: track numbers mean nothing, the first Japanese");
    ok(audioesc_escolher_com(g, 3, &m, "en", "", "", &o) == 1, "no bingeGroup known: the first Japanese");
    g[2].idioma = "en";
    ok(audioesc_escolher_binge(g, 3, &m, "aio|SubsPlease|1080p", "en", "", "", &o) == 1 && o == AUDIOESC_LEMBRADA,
       "same bingeGroup but track 3 is now English: never the wrong language, the Japanese one");
    g[2].idioma = "ja"; g[2].pode = 0;
    ok(audioesc_escolher_binge(g, 3, &m, "aio|SubsPlease|1080p", "en", "", "", &o) == 1,
       "exact track the target cannot play: the playable Japanese");
    g[2].pode = 1; m.binge[0] = 0;
    ok(audioesc_escolher_binge(g, 3, &m, "", "en", "", "", &o) == 1,
       "remembered without bingeGroup, source without one: no exact track, the first Japanese");
  }

  // Detector.
  ok(audioesc_dts_truehd("A_DTS/LOSSLESS", "") && audioesc_dts_truehd("truehd", "") &&
     audioesc_dts_truehd("A_MLP", "") && audioesc_dts_truehd("", "Inglês · DTS 5.1") &&
     audioesc_dts_truehd("audio/vnd.dts.hd", "") && !audioesc_dts_truehd("A_EAC3", "English · E-AC-3 5.1"),
     "DTS/TrueHD detector");

  // Memory follows the title: a show is one title, so every episode keeps it.
  audioesc_titulo("tt1");
  audioesc_lembrar("en", "A_AC3", "English", 6);
  ok(audioesc_tem_lembrada(), "remembered on this title");
  audioesc_titulo("tt1");
  ok(audioesc_tem_lembrada(), "same title again (source switch, reopen, next episode): still remembered");
  audioesc_titulo("tt2");
  ok(!audioesc_tem_lembrada(), "another title: forgotten");

  // Second language from the account and Settings, same rule as the second subtitle.
  ling_conta_audio("en"); ling_conta_audio2("sv");
  ok(!strcmp(ling_audio2(), "sv"), "second audio language from the account");
  ling_local_audio("de");
  ok(!strcmp(ling_audio2(), ""), "first one chosen on this TV: the account's second does not apply");
  ling_local_audio2("fi");
  ok(!strcmp(ling_audio2(), "fi"), "second chosen on this TV");
  ling_conta_audio2("ORIGINAL");
  ling_local_audio2("");
  ling_local_audio("");
  ok(!strcmp(ling_audio2(), ""), "account sentinel ORIGINAL: no second language");
  printf(falhas ? "audioescolha: %d FAIL(S)\n" : "audioescolha: ok\n", falhas);
  return falhas != 0;
}
