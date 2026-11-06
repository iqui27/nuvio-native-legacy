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
  puts("PASS video_faixa_rotulo: file names, language fallback, UTF-8 safe cut, menu separator");
  return 0;
}
