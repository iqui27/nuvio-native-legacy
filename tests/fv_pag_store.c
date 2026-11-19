// Issue #789 Phase-1 evidence: measure VtStore cost per stored CatItem and
// project the 8 MiB byte budget against the reported catalog sizes
// (~7,600 and ~14,000 items). No network: items are filled with realistic
// catalog payloads (as deMeta() produces for a paginated addon catalog),
// appended, and the store's own `used` counter is reported.
#include <assert.h>
#include <stdio.h>
#include "../src/vt_store.h"

static void fill(CatItem *it, int i, int longUrls) {
    memset(it, 0, sizeof *it);
    // TMDB-shaped URLs, ~100 chars each, as deMeta() leaves them after the
    // /original/ -> /w1280/ rewrite; longUrls=1 uses PostersPlus-scale
    // (#433: up to 1,173 bytes) poster URLs plus a filled sinopse.
    if (longUrls) {
        memset(it->poster, 'p', 1100); it->poster[1100] = 0;
    } else {
        snprintf(it->poster, sizeof it->poster,
                 "https://image.tmdb.org/t/p/w600_and_h900_bestv2/%d.jpg", i);
    }
    snprintf(it->backdrop, sizeof it->backdrop,
             "https://image.tmdb.org/t/p/w1280/%d.jpg", i);
    snprintf(it->backdropCatalogo, sizeof it->backdropCatalogo, "%s", it->backdrop);
    snprintf(it->backdropTmdb, sizeof it->backdropTmdb, "%s", it->backdrop);
    snprintf(it->titulo, sizeof it->titulo, "Um Titulo de Catalogo Numero %d", i);
    snprintf(it->genero, sizeof it->genero, "Filme  ·  Ação  ·  Drama");
    snprintf(it->meta, sizeof it->meta, "2024  ·  148 min");
    memset(it->sinopse, 's', longUrls ? 700 : 300);
    snprintf(it->pais, sizeof it->pais, "United States");
    snprintf(it->imdb, sizeof it->imdb, "tt%07d", 1000000 + i);
    snprintf(it->tipo, sizeof it->tipo, "movie");
    snprintf(it->origem, sizeof it->origem, "mdblist");
    it->tmdb = i; it->nota = 75; it->ranking = i;
}

int main(void) {
    CatItem in, out;
    const size_t LIMIT = 8u * 1024u * 1024u;
    const int targets[] = { 7600, 14000 };
    for (int lu = 0; lu < 2; lu++) {
        for (int t = 0; t < 2; t++) {
            VtStore s = {0};
            int n = 0;
            for (; n < targets[t]; n++) {
                fill(&in, n, lu);
                if (!vt_store_append(&s, &in, LIMIT)) break;
            }
            double per = n ? (double)s.used / n : 0;
            printf("urls=%s target=%d: stored=%d used=%zu bytes/item=%.0f "
                   "needed=%.0f budget=%zu fits=%s\n",
                   lu ? "long" : "tmdb", targets[t], n, s.used, per,
                   per * targets[t], LIMIT,
                   (n == targets[t]) ? "yes" : "no");
            vt_store_free(&s);
        }
    }
    // Round-trip check on one item.
    { VtStore s = {0}; fill(&in, 7, 1);
      assert(vt_store_append(&s, &in, LIMIT));
      assert(vt_store_get(&s, 0, &out) && !memcmp(&in, &out, sizeof in));
      vt_store_free(&s); }
    return 0;
}
