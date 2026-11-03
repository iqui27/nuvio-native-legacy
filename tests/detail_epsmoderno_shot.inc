// Identical fixtures for before/after captures, including menu edge cases.
static void epsModernoShots(const char *root) {
  extras_carregar("deploy/app/art");
  dados_gravar("aparencia-20.txt", "1\n");
  const int textScales[] = {100, 115, 130};
  for (int s = 0; s < 3; s++) {
    char out[900];
    snprintf(out, sizeof out, "%s/scale-%d", root, textScales[s]);
    assert(mkdir(out, 0755) == 0 || errno == EEXIST);
  for (int layout = 0; layout < 3; layout += 2) {
    char settings[400], path[1000];
    snprintf(settings, sizeof settings,
      "performanceAutoVersaoLocal 1\nidioma 1\nhomeLayoutLocal %d\nvidroLocal 1\n"
      "bordaFocoCartaz 1\nepTamanhoLocal 1\nescalaTextoVersaoLocal 1\nescalaTextoLocal 2\n", layout);
    dados_gravar("ajustes.txt", settings); ajustes_dir(dados_dir());
    gfx_escala_txt_definir(textScales[s] / 100.0f);
    montarCatalogo();
    for (int i = 0; i < T1_N; i++) {
      snprintf(episodios[i].thumb, sizeof episodios[i].thumb,
               "deploy/app/art/ep/27_1_%02d.jpg", i % 10 + 1);
      snprintf(episodios[i].nome, sizeof episodios[i].nome, "Episode title %d", i + 1);
      snprintf(episodios[i].sinopse, sizeof episodios[i].sinopse,
        "The family follows a new clue while an unexpected visitor changes their plans. "
        "Each discovery brings them closer to the truth and forces them to reconsider who they can trust.");
      snprintf(episodios[i].data, sizeof episodios[i].data, "March %d, 2024", i + 1);
    }
    cat_definir_episodios(0, episodios, T1_N + T2_N + T3_N);
    semear(1, T1_N, 3); abrir(0, 0); nosEpisodios(0);
    if (getenv("NV_EPSMODERNO_VERIFY") && layout == HOME_LAYOUT_MODERNA) {
      char label[32]; int col = colTemp(0);
      rotuloTemporada(col, label, sizeof label);
      float text = txt_largura(TXT_G22M, label), width = larguraTemporada(col);
      assert(width >= text + 70.0f && width <= text + 78.0f);
      assert(xItem(SEC_TEMPORADAS, col + 1) >= xItem(SEC_TEMPORADAS, col) + width);
    }
    for (int i = 0; i < 40; i++) { SDL_Delay(10); quadros(1); }
    snprintf(path, sizeof path, "%s/%s-row.png", out, layout ? "dynamic" : "modern"); gravar(path);
    foco.fileira = SEC_TEMPORADAS; foco.coluna = colTemp(temporada); quadros(120);
    snprintf(path, sizeof path, "%s/%s-tabs.png", out, layout ? "dynamic" : "modern"); gravar(path);
    const int columns[] = {0, 2, T1_N - 1};
    for (int k = 0; k < 3; k++) {
      nosEpisodios(columns[k]);
      const CatEp *ep = cat_episodio(idx, epAbsoluto(columns[k])); assert(ep);
      episodios_menu_visto(idx, ep->temporada, ep->episodio, ep->nome);
      quadros(1);
      if (getenv("NV_EPSMODERNO_VERIFY") && layout == HOME_LAYOUT_DINAMICA) {
        GfxRect summary, menu;
        float ui = gfx_escala_ui(), t = anim_clamp(episodios_menu_anim(), 0, 1);
        float x = (xItem(SEC_EPISODIOS, columns[k]) - scrollSec[SEC_EPISODIOS]) / ui;
        float y = (epRowY() - scrollY) / ui;
        assert(t < .2f && episodios_menu_resumo(&summary) && episodios_menu_caixa(&menu));
        assert(fabsf(summary.x - x) < 65.0f && fabsf(summary.y - y) < 65.0f);
        assert(fabsf(menu.x - (x + epCardW()/ui)) < 100.0f);
      }
      quadros(120);
      // Same source fixture reproduces the empty HDR separator defect.
      vaiTocar.tem = 1; vaiTocar.temporada=ep->temporada; vaiTocar.episodio=ep->episodio;
      vaiTocarIdx=idx; vaiTocarT=ep->temporada; vaiTocarE=ep->episodio; vaiTocarEm=fx_ticks();
      strcpy(vaiTocar.res, "1080p"); vaiTocar.hdr[0] = 0;
      strcpy(vaiTocar.audio, "DTS-HD MA 5.1");
      parado = 1; quadros(3);
      if (getenv("NV_EPSMODERNO_VERIFY") && layout == HOME_LAYOUT_MODERNA) {
        GfxRect summary, menu;
        assert(episodios_menu_resumo(&summary) && episodios_menu_caixa(&menu));
        float ui = gfx_escala_ui();
        assert(summary.x*ui >= 47.9f && (summary.x+summary.w)*ui+16 <= menu.x*ui);
        assert(summary.y*ui >= 47.9f && (summary.y+summary.h)*ui <= NV_LAYOUT_REAL_H-47.9f);
        // Observe the actual Detail transform, after conversion to real pixels.
        // Every title/info line is bounded by this same 420 px summary width.
        assert(fxSummaryRight <= menu.x * gfx_escala_ui());
        assert(fabsf(fxSummaryScale - 1.0f) < .001f);
        assert(fabsf(gfx_escala() - 1.0f) < .001f);
        assert(secA[SEC_TEMPORADAS] > .99f);
      }
      snprintf(path, sizeof path, "%s/%s-menu-%d.png", out, layout ? "dynamic" : "modern", k); gravar(path);
      assert(glGetError() == GL_NO_ERROR);
      SDL_Event ev = {0}; ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_ESCAPE;
      episodios_menu_evento(&ev); parado = 0; quadros(120);
    }
  }
  }
  puts("PASS: Modern and Dynamic menus at 100/115/130% text; summary stays clear of actions");
}
