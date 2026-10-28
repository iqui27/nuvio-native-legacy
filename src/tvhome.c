#include "tvhome.h"
#include "perfis.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int tvhome_select(const ProgRegistro *records, int count, const CatItem *cards, int cardCount,
                  int profile, int minimum, int completed, TvHomeItem *out, int capacity) {
  int n = 0;
  if (!out || capacity <= 0 || profile != perfis_ativo()) return 0;
  for (int i = 0; i < count; i++) {
    const ProgRegistro *r = &records[i];
    int newer = 0;
    if (r->perfil != profile || !isfinite(r->durSeg) || !isfinite(r->posSeg) || r->durSeg < 60) continue;
    /* A finished newer episode must suppress an older unfinished episode. */
    for (int j = 0; j < count; j++)
      if (j != i && records[j].perfil == profile && !strcmp(records[j].contentId, r->contentId) &&
          (records[j].lastWatchedMs > r->lastWatchedMs ||
           (records[j].lastWatchedMs == r->lastWatchedMs && strcmp(records[j].chave, r->chave) < 0))) newer = 1;
    if (newer || prog_removido_vence(r->contentId, r->lastWatchedMs) ||
        prog_oculto_vence(r->contentId, r->lastWatchedMs)) continue;
    const CatItem *c = NULL;
    for (int j = 0; j < cardCount; j++)
      if (!strcmp(cards[j].imdb, r->contentId) && !strcmp(cards[j].tipo, r->tipo)) { c = &cards[j]; break; }
    if (!c || c->rewatch || !c->titulo[0]) continue;
    int next = r->posSeg * 100.0 >= r->durSeg * completed;
    if (next) {
      /* Home already resolved the next episode, including season boundaries. */
      if (strcmp(r->tipo, "series") || c->progresso != 0 || c->temporada < 1 || c->episodio < 1 ||
          c->temporada < r->temporada || (c->temporada == r->temporada && c->episodio <= r->episodio)) continue;
    } else if (!prog_inicio_continuar(r, minimum)) continue;
    if (strlen(r->contentId) >= sizeof out[0].id) continue;
    TvHomeItem item = {0};
    snprintf(item.id, sizeof item.id, "%s", r->contentId);
    snprintf(item.type, sizeof item.type, "%s", r->tipo);
    snprintf(item.title, sizeof item.title, "%s", c->titulo);
    snprintf(item.episodeTitle, sizeof item.episodeTitle, "%s", c->nomeEpisodio);
    snprintf(item.poster, sizeof item.poster, "%s", c->poster);
    snprintf(item.backdrop, sizeof item.backdrop, "%s", c->backdrop);
    item.season = next ? c->temporada : r->temporada;
    item.episode = next ? c->episodio : r->episodio;
    item.next = next;
    item.watchedMs = r->lastWatchedMs;
    /* Provider columns use signed 32-bit milliseconds. */
    item.positionMs = next ? 0 : (long long)(fmin(r->posSeg, 2147483.0) * 1000);
    item.durationMs = next ? 0 : (long long)(fmin(r->durSeg, 2147483.0) * 1000);
    int k = 0;
    while (k < n && (out[k].watchedMs > item.watchedMs ||
           (out[k].watchedMs == item.watchedMs && strcmp(out[k].id, item.id) < 0))) k++;
    if (k >= capacity) continue;
    if (n < capacity) n++;
    memmove(out + k + 1, out + k, (size_t)(n - k - 1) * sizeof *out);
    out[k] = item;
  }
  return n;
}
