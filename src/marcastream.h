#ifndef NV_MARCASTREAM_H
#define NV_MARCASTREAM_H
#include <ctype.h>
#include <string.h>
// MARCA DOS SERVICOS DE STREAMING nas colecoes (pedido do dono, 13/10): o
// wordmark grande no lugar do titulo e o degrade do destaque na cor da marca.
// Esta parte e pura (tabela + casamento de nome); o logo do TMDB mora em
// marcastream_logo.c.
typedef struct {
  const char *chave;   // como o servico aparece no titulo da pasta
  long rede;           // id da rede no TMDB (/network/{id}/images); 0 = sem logo
  float r, g, b;       // cor do degrade, ja escura: o texto branco passa de 4,5:1
} MarcaStream;

// URL do wordmark ("" enquanto o TMDB nao respondeu, ou sem chave/rede).
// Dispara a busca na primeira chamada; do fio de desenho.
const char *marca_stream_logo(const MarcaStream *m);

// Cores: a cor da marca vezes ~0,55 (WCAG: branco sobre ela >= 4,5:1).
// Casa o nome do servico no titulo (palavra inteira, sem maiusculas), ou NULL.
// "HBO Max" e "Max" caem na mesma entrada. Tudo aqui e inline: os testes puros
// de colecoes nao precisam linkar nada novo.
static const MarcaStream MARCAS[] = {
  { "netflix",      213,  .52f, .035f, .065f },
  { "prime video",  1024, .025f, .32f, .58f },
  { "amazon",       1024, .025f, .32f, .58f },
  { "disney",       2739, .10f, .13f, .46f },
  { "hbo",          49,   .27f, .12f, .44f },
  { "max",          3186, .27f, .12f, .44f },
  { "apple tv",     2552, .16f, .17f, .20f },
  { "hulu",         453,  .03f, .28f, .17f },
  { "paramount",    4330, .0f, .20f, .60f },
  { "peacock",      3353, .10f, .10f, .13f },
  { "crunchyroll",  1112, .62f, .27f, .02f },
  { "globoplay",    0,    .55f, .08f, .10f },
};

static inline const MarcaStream *marca_stream(const char *titulo) {
  size_t i;
  if (!titulo) return NULL;
  for (i = 0; i < sizeof MARCAS / sizeof *MARCAS; i++) {
    const char *t = titulo;
    size_t n = strlen(MARCAS[i].chave);
    for (; *t; t++) {
      size_t j = 0;
      while (j < n && t[j] && tolower((unsigned char)t[j]) == MARCAS[i].chave[j]) j++;
      if (j == n && !isalnum((unsigned char)t[n]) && (t == titulo || !isalnum((unsigned char)t[-1])))
        return &MARCAS[i];
    }
  }
  return NULL;
}
#endif
