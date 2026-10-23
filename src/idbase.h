// Identidade de um titulo: onde acaba o id "base" e comeca o episodio.
//
// POR QUE ISTO EXISTE. O app inteiro assumia "tt1234567" (IMDb) e cortava no
// PRIMEIRO ':' para separar o id do ":temporada:episodio". Vale para o IMDb e
// so para ele: um titulo de catalogo de addon de anime chega como "kitsu:123",
// "mal:456", "anilist:789" ou "xperience:abc", e o corte no primeiro ':' os
// reduzia a "kitsu"/"mal"/"anilist" — a mesma chave de progresso, de preferencia
// de fonte e de arte para TODOS os animes do mesmo addon (o vazamento que o #37
// ja tinha achado nos canais, "cs:channel:<hash>").
//
// A regra, uma so e com nome: id do IMDb ("tt...") corta no primeiro ':'; todo
// outro id e "<prefixo>:<id>" e corta no SEGUNDO ':' ("tmdb:t1399", "kitsu:41370").
// Mesma regra que arteesc_chave (arteescolha.c) ja usava sozinha.
//
// LIMITE, dito: um id com mais de dois segmentos que NAO seja episodio
// ("cs:channel:hash") seria cortado em "cs:channel". Canal nao passa por aqui
// (ehCanal), e nenhum addon de catalogo conhecido usa id de titulo assim.
//
// So funcoes estaticas no cabecalho: quem precisa da regra nao precisa linkar
// mais um modulo (alguns testes compilam pedacos do app sem o resto).
#ifndef NV_IDBASE_H
#define NV_IDBASE_H
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// 1 quando o id e do IMDb.
static inline int idbase_e_imdb(const char *id) {
  return id && id[0] == 't' && id[1] == 't';
}

// Tamanho do id BASE (sem ":temporada:episodio"). 0 para id nulo.
static inline size_t idbase_len(const char *id) {
  size_t n;
  if (!id) return 0;
  n = strcspn(id, ":");
  if (!idbase_e_imdb(id) && id[n] == ':') n += 1 + strcspn(id + n + 1, ":");
  return n;
}

// O id base copiado para `dst`.
static inline void idbase_copiar(const char *id, char *dst, size_t tam) {
  size_t n = idbase_len(id);
  if (!tam) return;
  if (n >= tam) n = tam - 1;
  if (n) memcpy(dst, id, n);
  dst[n] = 0;
}

// O episodio que o id carrega, se carrega. `*t`/`*e` ficam 0 sem episodio.
//   "tt1:2:5"       -> t=2 e=5
//   "kitsu:41370:5" -> t=-1 e=5  (absent season; explicit season 0 is Specials)
//   "kitsu:1:2:5"   -> t=2 e=5
static inline void idbase_episodio(const char *id, int *t, int *e) {
  const char *p;
  int a = -1, b = -1, n;
  if (t) *t = 0;
  if (e) *e = 0;
  if (!id) return;
  p = id + idbase_len(id);
  if (*p != ':') return;
  n = sscanf(p + 1, "%d:%d", &a, &b);
  if (n == 2) { if (t) *t = a; if (e) *e = b; }
  else if (n == 1) {
    if (idbase_e_imdb(id)) { if (t) *t = a; }     // como sempre foi para o IMDb
    else { if (t) *t = -1; if (e) *e = a; }
  }
}

// O id ja carrega episodio?
static inline int idbase_tem_episodio(const char *id) {
  return id && id[idbase_len(id)] == ':';
}

#endif
