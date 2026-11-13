/* Audio track labels in the DTS/Dolby Vision path (src/video_faixa_rotulo.h). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "video_faixa_rotulo.h"
int main(void) {
  char r[48];
  nv_rotulo_faixa(r, sizeof r, "Original DTS-ES Surround Mix", "Inglês", "DTS 5.1");
  assert(!strcmp(r, "Original DTS-ES Surround Mix \xc2\xb7 DTS 5.1"));
  nv_rotulo_faixa(r, sizeof r, "", "Inglês", "FLAC 2.0");
  assert(!strcmp(r, "Inglês \xc2\xb7 FLAC 2.0"));
  /* Long name: cut with an ellipsis so the codec line survives in 48 bytes. */
  nv_rotulo_faixa(r, sizeof r, "Commentary by director David Fincher and actors Brad Pitt", "Inglês", "FLAC 2.0");
  assert(strlen(r) <= 47 && strstr(r, "\xe2\x80\xa6 \xc2\xb7 FLAC 2.0") && !strncmp(r, "Commentary by director", 22));
  /* Never splits a UTF-8 character. */
  nv_rotulo_faixa(r, sizeof r, "Ścieżka dźwiękowa z komentarzem reżysera filmu", "Polski", "AC-3 5.1");
  assert(strlen(r) <= 47 && strstr(r, "\xc2\xb7 AC-3 5.1"));
  for (const unsigned char *p = (const unsigned char *)r; *p; p++)
    if (*p >= 0xc0) { int len = *p >= 0xe0 ? 3 : 2; for (int i = 1; i < len; i++) assert((p[i] & 0xc0) == 0x80); p += len - 1; }
  /* A "·" inside the name would become the menu split: it turns into "-". */
  nv_rotulo_faixa(r, sizeof r, "Remix \xc2\xb7 2026", "Inglês", "DTS 5.1");
  assert(!strcmp(r, "Remix - 2026 \xc2\xb7 DTS 5.1"));
  nv_rotulo_faixa(r, sizeof r, "Main", "Inglês", "");
  assert(!strcmp(r, "Main"));
  /* A title that is the release/file name shows the language instead. */
  { char g[112];
    nv_rotulo_faixa(g, sizeof g, "Game.of.Thrones.S01E01.2160p.BluRay.REMUX.HEVC.TrueHD.7.1.Atmos-FGT", "Angielski", "TrueHD 7.1");
    assert(!strcmp(g, "Angielski \xc2\xb7 TrueHD 7.1"));
    nv_rotulo_faixa(g, sizeof g, "Game of Thrones S01E01 2160p", "Francuski", "AC-3 5.1");
    assert(!strcmp(g, "Francuski \xc2\xb7 AC-3 5.1"));
    nv_rotulo_faixa(g, sizeof g, "Movie_2026_1080p_WEB-DL", "Inglês", "E-AC-3 5.1");
    assert(!strcmp(g, "Inglês \xc2\xb7 E-AC-3 5.1"));
    /* Real titles stay, including ones with dots, digits or an S-word. */
    nv_rotulo_faixa(g, sizeof g, "Surround 5.1", "Inglês", "DTS 5.1");
    assert(!strcmp(g, "Surround 5.1 \xc2\xb7 DTS 5.1"));
    nv_rotulo_faixa(g, sizeof g, "Polski lektor", "Polski", "AC-3 5.1");
    assert(!strcmp(g, "Polski lektor \xc2\xb7 AC-3 5.1"));
    nv_rotulo_faixa(g, sizeof g, "DTS-HD.MA", "Inglês", "DTS 7.1");
    assert(!strcmp(g, "DTS-HD.MA \xc2\xb7 DTS 7.1"));
    nv_rotulo_faixa(g, sizeof g, "Songs S1", "Inglês", "AC-3 2.0");
    assert(!strcmp(g, "Songs S1 \xc2\xb7 AC-3 2.0"));
    /* The 112-byte label keeps a long real title whole (the menu cuts by width). */
    nv_rotulo_faixa(g, sizeof g, "Commentary by director David Fincher and actors Brad Pitt", "Inglês", "FLAC 2.0");
    assert(!strcmp(g, "Commentary by director David Fincher and actors Brad Pitt \xc2\xb7 FLAC 2.0"));
    assert(!nv_titulo_e_arquivo("") && !nv_titulo_e_arquivo(NULL) && !nv_titulo_e_arquivo("Original DTS-ES Surround Mix")); }
  puts("PASS video_faixa_rotulo: file names, release names, language fallback, UTF-8 safe cut, menu separator");
  return 0;
}
