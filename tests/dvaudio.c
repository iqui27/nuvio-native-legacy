#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/video_dvaudio.h"
static int casa(const char *a, const char *b) { return !strncmp(a, b, 2); }
int main(void) {
  // Arquivo real: 1 video, 2 TrueHD Atmos en, 3 E-AC3 en (faixas de audio: 0 e 1).
  char cod[4][16] = {"A_TRUEHD", "A_EAC3"};
  char idi[4][8] = {"en", "en"};
  char rot[4][48] = {"Dolby TrueHD Atmos 7.1", "Dolby Digital Plus 7.1"};
  assert(nv_dvaudio_escolher(2, 0, cod, idi, rot, casa) == 1);
  assert(nv_dvaudio_escolher(2, 1, cod, idi, rot, casa) == -1);   // atual E-AC3: TrueHD nao alimenta
  // Idioma diferente: nao troca.
  strcpy(idi[1], "pt"); assert(nv_dvaudio_escolher(2, 0, cod, idi, rot, casa) == -1);
  strcpy(idi[1], "en");
  // Vazio so casa com vazio.
  idi[0][0] = 0; assert(nv_dvaudio_escolher(2, 0, cod, idi, rot, casa) == -1);
  idi[1][0] = 0; assert(nv_dvaudio_escolher(2, 0, cod, idi, rot, casa) == 1);
  strcpy(idi[0], "en"); strcpy(idi[1], "en");
  // E-AC3 vence AC3 mesmo sem Atmos; entre dois E-AC3, o Atmos/JOC.
  { char c[4][16] = {"A_TRUEHD", "A_AC3", "A_EAC3", "A_EAC3"};
    char i[4][8] = {"en", "en", "en", "en"};
    char r[4][48] = {"TrueHD", "Dolby Digital 5.1 Atmos", "DD+ 5.1", "DD+ 7.1 JOC"};
    assert(nv_dvaudio_escolher(4, 0, c, i, r, casa) == 3);
    strcpy(r[3], "DD+ 7.1"); assert(nv_dvaudio_escolher(4, 0, c, i, r, casa) == 2);   // empate: primeira
    strcpy(c[2], "A_AC3"); strcpy(c[3], "A_AC3"); strcpy(c[1], "A_DTS-HD");
    assert(nv_dvaudio_escolher(4, 0, c, i, r, casa) == 2);   // AC3 > DTS
  }
  // So TrueHD: sem faixa possivel.
  { char c[2][16] = {"A_TRUEHD", "A_TRUEHD"}; char i[2][8] = {"en", "en"}; char r[2][48] = {"a", "b"};
    assert(nv_dvaudio_escolher(2, 0, c, i, r, casa) == -1); }
  assert(nv_dvaudio_tem_atmos("DD+ ATMOS") && nv_dvaudio_tem_atmos("x joc") && !nv_dvaudio_tem_atmos("DD+ 7.1"));
  // C9 (log real): o pipeline da TV lista UMA faixa de audio, o MKV tem duas.
  // A escolha vem so do MKV; a lista da TV nao pode limitar.
  { char c[2][16] = {"A_TRUEHD", "A_EAC3"}; char i[2][8] = {"eng", "eng"};
    char r[2][48] = {"Dolby TrueHD Atmos 7.1", "Dolby Digital Plus 7.1"};
    assert(nv_dvaudio_decidir(1, 2, 0, c, i, r, casa) == 1); }
  puts("dvaudio ok");
  return 0;
}
