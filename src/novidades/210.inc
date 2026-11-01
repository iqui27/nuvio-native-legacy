// Preserve the existing release card content and visuals for the version bump.
// Full 2.1.0 changes are documented in docs/releases/2.1.0/NOTAS.md.
static void visual210(GfxRect p, float t, float a, int c);
static void visual210a(GfxRect p, float t, float a) { visual210(p, t, a, 0); }
static void visual210b(GfxRect p, float t, float a) { visual210(p, t, a, 1); }
static void visual210c(GfxRect p, float t, float a) { visual210(p, t, a, 2); }
static void visual210d(GfxRect p, float t, float a) { visual210(p, t, a, 3); }
static const NovItem ITENS_210[] = {
  { .grupo = "Destaques", .icone = "aj_users", .nome = "Assistir Juntos",
    .frase = "Abra uma sala na página do título, convide quem você conhece e escolham a fonte juntos. Quando todos marcarem pronto, a sessão de cinema começa para a sala inteira.", .linhas = 6 },
  { .icone = "aj_tv-minimal-play", .nome = "Multiview",
    .frase = "Vários canais ao mesmo tempo na mesma tela, em grade 16:9. O seletor mostra os canais vivos de cada fonte, com limite por fonte e por conta.", .linhas = 6 },
  { .icone = "aj_book-open", .nome = "Diário",
    .frase = "O que você assistiu, por perfil: minutos do que tocou no Nuvio, importação do histórico do Trakt e cópia opcional na sua conta, só para você.", .linhas = 6 },
  { .icone = "aj_trophy", .nome = "Conquistas",
    .frase = "Trilhas e marcos pelo que você já viu: sagas, maratonas e recordes pessoais ganham medalhas que ficam no perfil.", .linhas = 4 },
  { .grupo = "Mais novidades", .icone = "aj_house", .nome = "Biblioteca",
    .frase = "A Biblioteca foi redesenhada e agora lê também os servidores Jellyfin e Emby, com a biblioteca inteira e as legendas do servidor.", .linhas = 6 },
  { .icone = "aj_layout-dashboard", .nome = "Coleções",
    .frase = "As páginas de gênero de uma coleção trazem os títulos daquele gênero (#700), e os pôsteres personalizados valem nas fileiras e na capa da lista (#708).", .linhas = 6 },
  { .icone = "aj_user-round", .nome = "Perfis",
    .frase = "Cada perfil tem estatísticas próprias e o PIN continua valendo mesmo depois de um perfil ser apagado.", .linhas = 4 },
  { .icone = "aj_sliders-horizontal", .nome = "Ajustes",
    .frase = "Os Ajustes viraram lâminas do Palco: cada seção desliza sobre o palco, com as técnicas agrupadas e o foco sempre visível.", .linhas = 5 },
  { .icone = "aj_puzzle", .nome = "AIOStreams",
    .frase = "Uma área dedicada ao add-on: painel com o estado, seções, serviços e catálogos, e pareamento seguro pelo celular para a configuração.", .linhas = 6 },
  { .icone = "aj_server", .nome = "Jellyfin e Emby",
    .frase = "Biblioteca completa do servidor no app, com as legendas hospedadas nele.", .linhas = 4 },
  { .grupo = "Correções", .icone = "aj_play", .nome = "Próximo episódio",
    .frase = "Em anime, o próximo episódio continua no mesmo anime, sem pular para outro título (#697).", .linhas = 4 },
  { .icone = "aj_captions", .nome = "Legendas",
    .frase = "A última linha de legendas volta a ser selecionável quando os add-ons devolvem o mesmo arquivo (#681), e os add-ons de legenda que não foram sondados continuam na busca de episódios (#692).", .linhas = 8 },
  { .icone = "aj_audio-lines", .nome = "Trailer no idioma",
    .frase = "O trailer do YouTube abre no idioma do app também na Samsung .tpk, pelo app do YouTube (#650, #687).", .linhas = 5, .plataformas = NOV_TPK | NOV_OUTRAS },
  { .icone = "aj_tv-minimal-play", .nome = "Protetor de tela",
    .frase = "O pedido de protetor de tela da TV é respondido durante o filme: ele não dispara no meio da sessão (LG webOS) (#706).", .linhas = 5, .plataformas = NOV_LG | NOV_OUTRAS },
};
static const NovCena CENAS_210[] = {
  { "Destaques", "Assistir Juntos", visual210a, 5.5f, 0 },
  { "Destaques", "Multiview", visual210b, 5.5f, 0 },
  { "Destaques", "Diário", visual210c, 5.5f, 0 },
  { "Mais novidades", "AIOStreams", visual210d, 5.5f, 0 },
};
static const int ARTES_210[] = { 2, 4, 7, 9 };
static const NovConteudo NOV_ATUAL = {
  .versao = "2.1.0", .arquivo = "novidades-210.txt",
  .subtitulo = NULL,
  .itens = ITENS_210, .nItens = (int)(sizeof ITENS_210 / sizeof *ITENS_210),
  .cenas = CENAS_210, .nCenas = 4,
  .artes = ARTES_210, .nArtes = 4,
};

// Small, static feature mockups. Only their entrance moves; no live video.
static void visual210(GfxRect p, float t, float a, int c) {
  float e = ajustes_animacoes_reduzidas() ? 1 : sai3(t / .45f);
  float x = p.x + 40, y = p.y + 100 + (1 - e) * 16;
  float w = p.w - 80;
  const char *icons[] = { "aj_users", "aj_tv-minimal-play", "aj_book-open", "aj_puzzle" };
  novArte(ARTES_210[c], (GfxRect){p.x, p.y, p.w, p.h}, PV_RAIO, .68f * a);
  gfx_rect(p, 0, GFX_VEU_CARD, 0, 0, 0, PV_RAIO / p.h, .025f, .03f, .045f, a);
  float vs = fminf(1, (p.h - 230) / 540);
  gfx_transformar(p.x, p.y, vs, 0, 0);
  gfx_icone((GfxRect){x, p.y + 36, 40, 40}, icons[c], .85f, .90f, 1, a);
  if (c == 0) {
    // Assistir Juntos: a sala, dois convidados prontos e o botão de começar.
    novArte(2, (GfxRect){x, y, w, 260}, 18, a * e);
    for (int j = 0; j < 3; j++) {
      GfxRect r = {x + j * (w / 3 + 8), y + 288, w / 3 - 12, 84};
      gfx_vidro_folha(r, 14 / r.h, a * e);
      gfx_icone((GfxRect){r.x + 18, r.y + 22, 40, 40}, j ? "aj_user-round" : "aj_users", .9f, .94f, 1, a);
      gfx_cor((GfxRect){r.x + 74, r.y + 38, r.w - 96, 6}, .5f, 1, 1, 1, .5f * a);
      if (j < 2) gfx_icone((GfxRect){r.x + r.w - 44, r.y + 28, 24, 24}, "aj_check", .55f, .95f, .6f, a);
    }
    gfx_cor((GfxRect){x + w * .32f, y + 400, w * .36f, 44}, .5f, .92f, .1f, .12f, .9f * a * e);
  } else if (c == 1) {
    // Multiview: grade 16:9 com quatro canais vivos.
    for (int j = 0; j < 4; j++) {
      float cw = (w - 16) / 2, ch = cw * 9 / 16;
      GfxRect r = {x + (j % 2) * (cw + 16), y + (j / 2) * (ch + 16), cw, ch};
      novArte(ARTES_210[j], r, 14, a * e);
      gfx_cor((GfxRect){r.x + 12, r.y + 12, 46, 20}, .5f, 0, 0, 0, .55f * a);
      gfx_cor((GfxRect){r.x + 18, r.y + 18, 10, 8}, .5f, .95f, .2f, .2f, .9f * a);
    }
  } else if (c == 2) {
    // Diário: a linha do tempo do que tocou, com os minutos em barras.
    TxtLinha n = txt_linha(TXT_NOV_TITULO, "142", 245, 247, 255, 255);
    txt_desenhar_alpha(n, x, y, a * e);
    gfx_cor((GfxRect){x + 170, y + 44, w - 200, 6}, .5f, 1, 1, 1, .25f * a);
    for (int j = 0; j < 7; j++) {
      float bw = (w - 60) / 7, bh = 30 + ((j * 37) % 90);
      GfxRect r = {x + j * (bw + 10), y + 210 - bh, bw, bh};
      gfx_cor(r, .45f, .75f, .82f, 1, (.35f + .5f * (j == 5)) * a * e);
    }
    for (int j = 0; j < 3; j++) {
      GfxRect r = {x, y + 260 + j * 88, w, 72};
      gfx_vidro_folha(r, 14 / r.h, a * e);
      novArte(ARTES_210[j + 1], (GfxRect){r.x + 10, r.y + 10, 92, 52}, 10, a * e);
      gfx_cor((GfxRect){r.x + 118, r.y + 24, w - 160, 6}, .5f, 1, 1, 1, .45f * a);
      gfx_cor((GfxRect){r.x + 118, r.y + 44, (w - 160) * .6f, 5}, .5f, 1, 1, 1, .25f * a);
    }
  } else {
    // AIOStreams: o painel com os cartões de estado e os serviços.
    for (int j = 0; j < 4; j++) {
      GfxRect r = {x + j * ((w - 48) / 4 + 16), y, (w - 48) / 4, 110};
      gfx_vidro_folha(r, 16 / r.h, a * e);
      gfx_icone((GfxRect){r.x + 20, r.y + 20, 32, 32}, icons[j % 4], .9f, .94f, 1, a);
      gfx_cor((GfxRect){r.x + 20, r.y + 66, r.w - 40, 6}, .5f, 1, 1, 1, .5f * a);
      gfx_cor((GfxRect){r.x + 20, r.y + 84, (r.w - 40) * .55f, 5}, .5f, .75f, .85f, 1, .5f * a);
    }
    for (int j = 0; j < 3; j++) {
      GfxRect r = {x, y + 150 + j * 104, w, 86};
      gfx_vidro_folha(r, 16 / r.h, a * e);
      gfx_cor((GfxRect){x + 24, r.y + 30, 26, 26}, .6f, .3f + .2f * j, .8f - .15f * j, .4f, .9f * a);
      gfx_cor((GfxRect){x + 74, r.y + 34, w * .5f, 6}, .5f, 1, 1, 1, .5f * a);
      gfx_cor((GfxRect){x + w - 190, r.y + 26, 140, 34}, .5f, 1, 1, 1, .12f * a);
    }
  }
  gfx_sem_transformar();
}
