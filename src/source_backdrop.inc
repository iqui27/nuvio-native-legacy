// Source-sheet backdrop only: reuse the RGB snapshot allocated at startup.
// The sheet/cards/clock remain live. Do not capture any video plane, nested
// snapshot, moving title page, glass or continuously animated theme.
#include "corviva.h"
static void fontes_fundo(int podeParar, unsigned rev, void (*fundo)(void *), void *ctx) {
#if defined(NV_SOURCE_BACKDROP_TEST) || (!defined(NV_ANDROID) && !defined(NV_TPK) && \
    !defined(__EMSCRIPTEN__) && !defined(NV_LINUX_DESKTOP) && !defined(__APPLE__))
  typedef struct {
    uint64_t profile;
    unsigned catalog, content, episodes, color;
    int title, language;
    float w, h, scale;
  } BackdropKey;
  static struct {
    BackdropKey key;
    unsigned since, painted, owner, artRetries;
    int active, ready, missing, failed, settling;
  } cache;
  unsigned now = SDL_GetTicks();
  int allowed = stream_folha_aberta() && detail_aberto() && detail_cobre_tela() &&
    !strcmp(detail_transicao(), "-") && !player_aberto() && !player_mini_ativo() && !player_retido() &&
    !trailer_aberto() && !trailer_tocando() && !spainel_visivel() && !ctx_aberto() && !menu_aberto() && !vertudo_aberta() &&
    !registro_aberto() && !spot_cheio() && !ajustes_vidro() && !gfx_hdr_output &&
    !nv_ambiente_forca && (!nv_grad_ativo || anim_politica_reduzida) &&
    gfx_snap_ok() && !gfx_snap_ativo();
  if (!allowed) {
    memset(&cache, 0, sizeof cache);
  } else {
    // All padding is initialized: revisions never compare stack garbage.
    BackdropKey key;
    memset(&key, 0, sizeof key);
    key.profile = nv_perfil_sessao_ler(); key.catalog = rev;
    key.content = cat_revisao_itens(); key.episodes = cat_versao_episodios();
    key.color = (unsigned)corviva_retargets(); key.title = detail_indice();
    key.language = ajustes_idioma(); key.w = NV_TELA_W; key.h = NV_TELA_H;
    key.scale = gfx_escala_ui();
    if (!cache.active || memcmp(&key, &cache.key, sizeof key)) {
      memset(&cache, 0, sizeof cache);
      cache.active = 1; cache.since = now; memcpy(&cache.key, &key, sizeof key);
    }
    // Metadata revisions invalidate the snapshot; an unchanged, complete
    // background needs no periodic repaint while the source list scrolls.
    // Keep retrying missing art, and let a newly available logo's fade finish
    // live before taking one final, settled copy (detail.c: 260 ms).
    if (!cache.failed && now - cache.since >= 700) {
      if (cache.owner != gfx_snap_geracao()) cache.ready = 0;
      unsigned interval = cache.missing && cache.artRetries < 80 ? 150 : 2000;
      unsigned elapsed = now - cache.painted;
      if (cache.ready && !cache.settling && (!cache.missing || elapsed < interval)) {
        gfx_snap_desenhar(); return;
      }
      if (cache.ready && cache.settling && elapsed < 350) {
        spainel_fundo(podeParar, rev, fundo, ctx); return;
      }
      gfx_snap_comecar();
      if (gfx_snap_ativo()) {
        gfx_sem_recorte();
        glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        unsigned missing = tex_n_falta;
        if (fundo) fundo(ctx);
        int stillMissing = tex_n_falta != missing;
        cache.settling = cache.missing && !stillMissing;
        cache.missing = stillMissing;
        cache.artRetries = cache.missing ? cache.artRetries + 1 : 0;
        gfx_sem_recorte();
        int maxChannel = 0;
        int empty = !cache.ready && gfx_snap_vazio(&maxChannel);
        gfx_snap_terminar();
        cache.owner = gfx_snap_geracao();
        if (!empty) {
          cache.ready = 1; cache.painted = now;
          gfx_snap_desenhar(); return;
        }
        cache.failed = 1; cache.ready = 0;
      }
    }
  }
#endif
  spainel_fundo(podeParar, rev, fundo, ctx);
}
