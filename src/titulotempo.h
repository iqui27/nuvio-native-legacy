// Marcos da jornada do titulo, emitidos uma vez por campo e por foco.
#ifndef NV_TITULOTEMPO_H
#define NV_TITULOTEMPO_H
#include "marco.h"
#include "catalogo.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  char id[64];
  Uint32 desde;
  unsigned campos;
  int sinInicial, notaInicial, elencoInicial;
  unsigned long logo;
  unsigned pedidoLogo;
} TituloTempo;

static inline void titulo_tempo_marco(TituloTempo *t, const char *tela,
                                      const char *evento, const char *fonte,
                                      const char *url) {
  char msg[400], host[96] = "local";
  const char *p = url ? strstr(url, "://") : NULL;
  if (p) {
    p += 3;
    size_t n = strcspn(p, "/?#");
    // Nunca registra caminho, query ou credencial de addon.
    const char *at = memchr(p, '@', n);
    if (at) { n -= (size_t)(at + 1 - p); p = at + 1; }
    snprintf(host, sizeof host, "%.*s", (int)n, p);
  }
  snprintf(msg, sizeof msg, "%s: %s id=%s delta_ms=%u fonte=%s host=%s hash=%08lx",
           tela, evento, t->id, (unsigned)(SDL_GetTicks() - t->desde), fonte,
           url ? host : "-", url ? tex_hash_public(url) : 0UL);
  marco(msg);
}
static inline void titulo_tempo_inicio(TituloTempo *t, const char *tela,
                                       const CatItem *c) {
  memset(t, 0, sizeof *t);
  t->desde = SDL_GetTicks();
  if (c) {
    snprintf(t->id, sizeof t->id, "%s", c->imdb);
    t->sinInicial = c->sinopse[0] != 0;
    t->notaInicial = c->nota > 0;
    t->elencoInicial = c->nElenco > 0;
  }
  titulo_tempo_marco(t, tela, "foco/abertura", "catalogo-memoria", NULL);
}
static inline void titulo_tempo_logo(TituloTempo *t, const char *tela,
                                     const char *url, GLuint textura, int visivel) {
  if (!url || !*url) return;
  unsigned long hash = tex_hash_public(url);
  if (t->logo != hash) {
    t->logo = hash;
    t->pedidoLogo = 0;
    t->campos &= ~1u;
  }
  if (!t->pedidoLogo) {
    t->pedidoLogo = 1;
    titulo_tempo_marco(t, tela, "logo pedida", textura ? "memoria" : "pendente", url);
  }
  if (textura && visivel && !(t->campos & 1u)) {
    t->campos |= 1u;
    titulo_tempo_marco(t, tela, "logo na tela", "textura-pronta", url);
  }
}
static inline void titulo_tempo_info(TituloTempo *t, const char *tela,
                                     const CatItem *c, int elenco) {
  if (!c || strcmp(t->id, c->imdb)) return;
  if (c->sinopse[0] && !(t->campos & 2u)) {
    t->campos |= 2u;
    titulo_tempo_marco(t, tela, "sinopse na tela", t->sinInicial ? "catalogo-memoria" : "meta", NULL);
  }
  if (c->nota > 0 && !(t->campos & 4u)) {
    t->campos |= 4u;
    titulo_tempo_marco(t, tela, "nota na tela", t->notaInicial ? "catalogo-memoria" : "meta", NULL);
  }
  if (elenco && c->nElenco > 0 && !(t->campos & 8u)) {
    t->campos |= 8u;
    titulo_tempo_marco(t, tela, "elenco na tela", t->elencoInicial ? "catalogo-memoria" : "meta", NULL);
  }
}
#endif
