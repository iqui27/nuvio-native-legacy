#ifndef NV_VIDEO_FAIXA_ROTULO_H
#define NV_VIDEO_FAIXA_ROTULO_H
/* ROTULO DE UMA FAIXA DE AUDIO DO ARQUIVO no caminho DTS/Dolby Vision:
 * "<nome da faixa> · <codec e canais>", como o menu de audio parte
 * (partirRotulo em faixas.c: antes do "·" a linha forte, depois a de baixo).
 *
 * POR QUE (dono, 06/11/2026): no menu as faixas DTS apareciam com o nome do
 * arquivo ("Original DTS-ES Surround Mix") e as FLAC so como "Inglês / flac",
 * embora o MKV de cada comentario diga de quem e. Agora toda faixa com nome no
 * arquivo mostra o nome; sem nome, o idioma, como antes. O codec vem legivel
 * ("DTS 5.1", "FLAC 2.0") quando conhecido.
 *
 * Title that is the release/file name: the language (nv_titulo_e_arquivo).
 * O nome e cortado (com "…", sem partir UTF-8) para o rotulo inteiro caber;
 * um "·" dentro do nome viraria a divisao do menu, entao vira "-". */
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* A track title that is really the release/file name
 * ("Game.of.Thrones.S01E01.2160p.BluRay.REMUX...") says nothing about the
 * track: the menu shows the language instead. Real titles ("Surround 5.1",
 * "Commentary", "Polski lektor") stay. */
static inline int nv_titulo_e_arquivo(const char *t) {
  static const char *const marcas[] = { "2160p", "1080p", "1080i", "720p", "bluray", "blu-ray",
                                        "remux", "web-dl", "webrip", "hdtv", "x264", "x265", "h.264", "h.265" };
  size_t n = t ? strlen(t) : 0, pontos = 0;
  int espaco = 0;
  if (!n) return 0;
  for (size_t i = 0; i < n; i++) {
    if (t[i] == ' ') espaco = 1;
    else if (t[i] == '.' || t[i] == '_') pontos++;
    /* An episode tag: S01E01 (any case), not inside a word. */
    if ((t[i] == 'S' || t[i] == 's') && (!i || !isalpha((unsigned char)t[i - 1])) &&
        i + 4 < n && isdigit((unsigned char)t[i + 1]) && isdigit((unsigned char)t[i + 2]) &&
        (t[i + 3] == 'E' || t[i + 3] == 'e') && isdigit((unsigned char)t[i + 4])) return 1;
  }
  if (!espaco && pontos >= 2 && n >= 12) return 1;
  for (size_t k = 0; k < sizeof marcas / sizeof *marcas; k++) {
    size_t m = strlen(marcas[k]);
    for (size_t i = 0; i + m <= n; i++) {
      size_t j = 0;
      while (j < m && tolower((unsigned char)t[i + j]) == marcas[k][j]) j++;
      if (j == m && (!i || !isalnum((unsigned char)t[i - 1])) && (i + m == n || !isalnum((unsigned char)t[i + m]))) return 1;
    }
  }
  return 0;
}

/* A track title that only repeats what the menu already says ("FR AC3 5.1",
 * "Hun AAC 2.0", "English DD+ 5.1 640kbps", "ENG E-AC3 Atmos"): languages,
 * codecs, channels, bitrates. `e_idioma` says whether a short token is a
 * language code (the app passes ling_tem_nome); NULL: only the names below.
 * "Dubbing", "Lektor", "Commentary" and anything else make it a real title. */
static inline int nv_titulo_tecnico(const char *t, int (*e_idioma)(const char *)) {
  static const char *const palavras[] = {
    "ac3", "eac3", "e", "ac", "dd", "ddp", "dd+", "dts", "hd", "ma", "es", "x", "aac", "lc", "he",
    "truehd", "thd", "atmos", "flac", "opus", "mp3", "mp2", "pcm", "lpcm", "dolby", "digital", "plus",
    "surround", "stereo", "mono", "audio", "track", "ch", "channels", "kbps", "kbit", "kbs", "khz",
    "english", "french", "german", "spanish", "italian", "polish", "hungarian", "russian", "portuguese",
    "czech", "dutch", "swedish", "danish", "norwegian", "finnish", "turkish", "greek", "romanian",
    "bulgarian", "ukrainian", "croatian", "serbian", "slovak", "slovenian", "japanese", "korean",
    "chinese", "mandarin", "cantonese", "arabic", "hindi", "thai", "vietnamese", "hebrew", "persian",
    "latino", "castellano", "castilian", "magyar", "polski", "deutsch", "francais", "espanol", "italiano" };
  static const char *const sufixos[] = { "", "kbps", "kbs", "kb", "k", "ch", "khz", "hz", "bit" };
  const char *p = t;
  int tokens = 0;
  if (!t) return 0;
  while (*p) {
    char tok[32];
    size_t k = 0;
    int ok = 0;
    while (*p && !(isalnum((unsigned char)*p) || *p == '+' || (unsigned char)*p >= 0x80)) p++;
    if (!*p) break;
    /* A dot joins digits only ("5.1"); "DTS-HD.MA" is three words. */
    while (*p && (isalnum((unsigned char)*p) || *p == '+' || (unsigned char)*p >= 0x80 ||
                  (*p == '.' && k && isdigit((unsigned char)tok[k - 1]) && isdigit((unsigned char)p[1])))) {
      if (k + 1 < sizeof tok) tok[k++] = (char)tolower((unsigned char)*p);
      p++;
    }
    while (k && tok[k - 1] == '.') k--;
    tok[k] = 0;
    if (!k) continue;
    tokens++;
    /* Numbers: 5.1, 2.0, 640kbps, 6ch, 48khz, 24bit. */
    { size_t d = 0;
      while (d < k && (isdigit((unsigned char)tok[d]) || tok[d] == '.')) d++;
      if (d && isdigit((unsigned char)tok[0]))
        for (size_t s = 0; s < sizeof sufixos / sizeof *sufixos && !ok; s++) if (!strcmp(tok + d, sufixos[s])) ok = 1; }
    for (size_t w = 0; w < sizeof palavras / sizeof *palavras && !ok; w++) if (!strcmp(tok, palavras[w])) ok = 1;
    if (!ok && k >= 2 && k <= 3 && e_idioma && e_idioma(tok)) ok = 1;
    /* "DD+5.1", "AAC2.0", "AC3.5.1": a codec glued to its channels. */
    if (!ok) {
      size_t d = k;
      while (d && (isdigit((unsigned char)tok[d - 1]) || tok[d - 1] == '.')) d--;
      if (d && d < k) {
        char base[32];
        memcpy(base, tok, d); base[d] = 0;
        for (size_t w = 0; w < sizeof palavras / sizeof *palavras && !ok; w++) if (!strcmp(base, palavras[w])) ok = 1;
      }
    }
    if (!ok) return 0;
  }
  return tokens > 0;
}

static inline void nv_rotulo_faixa(char *out, size_t n, const char *titulo,
                                   const char *idioma_nome, const char *sub) {
  char nome[160];
  size_t k = 0, room;
  const char *src = titulo && *titulo && !nv_titulo_e_arquivo(titulo) ? titulo : (idioma_nome ? idioma_nome : "");
  const char *sep = sub && *sub ? " \xc2\xb7 " : "";
  if (!out || !n) return;
  out[0] = 0;
  for (const char *p = src; *p && k + 1 < sizeof nome; p++) {
    if ((unsigned char)p[0] == 0xc2 && (unsigned char)p[1] == 0xb7) { nome[k++] = '-'; p++; continue; }
    nome[k++] = *p;
  }
  while (k && nome[k - 1] == ' ') k--;
  nome[k] = 0;
  room = n - 1;
  { size_t fixed = strlen(sep) + (sub ? strlen(sub) : 0);
    room = fixed < room ? room - fixed : 0; }
  if (strlen(nome) > room) {
    size_t cut = room >= 3 ? room - 3 : 0;   /* 3 bytes for the ellipsis */
    while (cut && ((unsigned char)nome[cut] & 0xc0) == 0x80) cut--;
    while (cut && nome[cut - 1] == ' ') cut--;
    if (room >= 3) { memcpy(nome + cut, "\xe2\x80\xa6", 3); nome[cut + 3] = 0; }
    else nome[cut] = 0;
  }
  snprintf(out, n, "%s%s%s", nome, sep, sub ? sub : "");
}
#endif
