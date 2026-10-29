// Offline regression: completed-card artwork and merged Next Up publication.
#include <assert.h>
#include "progresso.h"
#include "../src/home.c"

// Only persistence and rendering boundaries are stubbed; progress stays in memory.
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int dados_gravar_leve(const char *n, const char *c) { (void)n; (void)c; return 1; }
char *dados_caminho(char *d, unsigned t, const char *n) {
  (void)d; (void)t; (void)n; return NULL;
}
int perfis_ativo(void) { return 1; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int m, const char *s) { (void)m; return s; }
int tex_falhou(const char *u) { (void)u; return 0; }
float tex_aspecto(const char *u) { (void)u; return 0; }
int ajustes_hero_arte_diferente(void) { return 0; }
int ajustes_hero_fonte(void) { return 0; }
int ajustes_cw_thumb_episodio(void) { return 1; }
int ajustes_cw_concluido(void) { return 90; }
int main(void) {
  CatItem a = {0};
  snprintf(a.imdb, sizeof a.imdb, "tt200");
  snprintf(a.tipo, sizeof a.tipo, "series");
  // Finish locally while an older tracker card and a catalog copy coexist.
  // Artwork must advance in this session, then survive a merged Next Up publication.
  {
    static CatItem items[2], next;
    static CatEp eps[2];
    CatFileira row = {0};
    char expected[512];
    items[0] = a; items[0].temporada = 1; items[0].episodio = 1;
    items[0].progresso = 30; items[0].retomadoMs = 1000;
    snprintf(items[0].imdb, sizeof items[0].imdb, "tt200:1:1");
    items[1] = items[0];
    snprintf(items[1].imdb, sizeof items[1].imdb, "tt200");
    snprintf(row.chave, sizeof row.chave, "continue_watching"); row.n = 1;
    cat_definir_tudo(items, 2, &row, 1);
    eps[0].temporada = eps[1].temporada = 1;
    eps[0].episodio = 1; eps[1].episodio = 2;
    cat_definir_episodios(1, eps, 2);
    assert(cat_salvar_progresso_ep(1, 3000, 3000, 1, 1, 1));
    next = *cat_item(0); next.episodio = 2; next.progresso = 0;
    snprintf(next.imdb, sizeof next.imdb, "tt200:1:2");
    snprintf(expected, sizeof expected, "%s", artehero_url_episodio(&next));
    assert(!strcmp(arte_card_retomada(cat_item(0)), expected));
    // Nuvio/Trakt/Simkl seeds carry the completion instant of episode 1.
    { ProgRegistro regs[PROG_MAX];
      assert(prog_ler(regs, PROG_MAX) > 0);
      next.retomadoMs = regs[0].lastWatchedMs;
    }
    cat_trocar_continuar(&next, 1);
    assert(cat_item(0)->episodio == 2 && cat_item(0)->progresso == 0);
    assert(!strcmp(arte_card_retomada(cat_item(0)), expected));
    // Genuine playback of the offered episode must still update normally.
    cat_aplicar_progresso(0, 600, 3000, 1, 2);
    assert(cat_item(0)->episodio == 2 && cat_item(0)->progresso == 20);
  }
  puts("nextup_art: ok");
  return 0;
}
