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
 * O nome e cortado (com "…", sem partir UTF-8) para o rotulo inteiro caber;
 * um "·" dentro do nome viraria a divisao do menu, entao vira "-". */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static inline void nv_rotulo_faixa(char *out, size_t n, const char *titulo,
                                   const char *idioma_nome, const char *sub) {
  char nome[96];
  size_t k = 0, room;
  const char *src = titulo && *titulo ? titulo : (idioma_nome ? idioma_nome : "");
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
