// Preserve the existing release card content and visuals for the version bump.
// Full 2.0.8 changes are documented in docs/releases/2.0.8/NOTAS.md.
static void visual208(GfxRect p, float t, float a, int c);
static void visual208a(GfxRect p, float t, float a) { visual208(p, t, a, 0); }
static void visual208b(GfxRect p, float t, float a) { visual208(p, t, a, 1); }
static void visual208c(GfxRect p, float t, float a) { visual208(p, t, a, 2); }
static void visual208d(GfxRect p, float t, float a) { visual208(p, t, a, 3); }
static const NovItem ITENS_208[] = {
  { .grupo = "Destaques", .icone = "aj_layout-dashboard", .nome = "Palco",
    .frase = "O título em foco aparece grande à esquerda, com arte, nota, sinopse e elenco; os cartazes da fileira correm à direita.", .linhas = 5 },
  { .icone = "aj_layout-dashboard", .nome = "Herói dividido",
    .frase = "O título em foco ganha logo grande e sinopse ao lado dos pôsteres, com o texto alinhado à fileira.", .linhas = 5 },
  { .icone = "aj_tv-minimal-play", .nome = "Guia de TV",
    .frase = "Adicione contas de TV e listas M3U com guias XMLTV e logos dos canais. A busca encontra programas enquanto você digita. OK abre a faixa sobre o vídeo; outro OK no canal atual abre o guia completo.", .linhas = 10 },
  { .icone = "aj_monitor-play", .nome = "Player externo",
    .frase = "Escolha qualquer player instalado e, se quiser, outro só para anime (Android).", .linhas = 6, .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_audio-lines", .nome = "Trailer dublado",
    .frase = "Áudio do trailer prioriza trailers dublados, e Original mantém o idioma do título. Automático segue sua preferência de áudio. Se o YouTube falhar, entra o trailer da Apple ou do IMDb.", .linhas = 9 },
  { .icone = "aj_play", .nome = "Continuar assistindo",
    .frase = "Até 40 títulos, e a fileira chega primeiro sem segurar os outros catálogos da Home.", .linhas = 4 },
  { .icone = "aj_tv-minimal-play", .nome = "TV ao vivo",
    .frase = "Prefira 4K sem repetir canais, mostre só 4K ou todas as versões. Escolha as seções, com sugestões para canais sem categoria e recuperação automática após falhas.", .linhas = 8 },
  { .grupo = "Mais novidades", .icone = "aj_droplet", .nome = "Vidro fosco",
    .frase = "As telas ficam mais fluidas, com vidro mais leve. Em TVs compatíveis, o app amplia a interface para 4K e permite escolher o algoritmo e a nitidez nos Ajustes.", .linhas = 8 },
  { .icone = "aj_layout-dashboard", .nome = "Fundo do Palco",
    .frase = "Cortina, Aura, Tríptico ou Monumento, com troca suave da arte ao mudar de título.", .linhas = 5 },
  { .icone = "aj_house", .nome = "Biblioteca",
    .frase = "Escolha nos Ajustes de onde vem a Biblioteca e para onde o + salva, por perfil. Ative uma fileira com seus títulos na Home. A fileira anterior continua visível ao rolar. Na página de um filme sem fontes, ative um aviso para quando houver uma disponível.", .linhas = 12 },
  { .icone = "aj_languages", .nome = "Idiomas",
    .frase = "Escolha espanhol latino-americano para os dados dos títulos; a sincronização mantém a variante regional da conta. A interface agora também está disponível em árabe.", .linhas = 8 },
  { .icone = "aj_smartphone", .nome = "Modo TV e Mobile",
    .frase = "O APK padrão escolhe sozinho entre TV e Mobile; dá para fixar em Modo da interface (Android).", .linhas = 6, .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_captions", .nome = "Legendas no trailer",
    .frase = "O trailer do YouTube mostra legendas no idioma das suas legendas (LG webOS).", .linhas = 6, .plataformas = NOV_LG | NOV_OUTRAS },
  { .icone = "aj_tv-minimal-play", .nome = "Trailer no cartão",
    .frase = "O trailer toca no cartão em foco ou expandido, mesmo com o destaque desligado.", .linhas = 4 },
  { .icone = "aj_languages", .nome = "Áudio da série",
    .frase = "A faixa de áudio que você escolher fica valendo para a série inteira.", .linhas = 4 },
  { .icone = "aj_memory-stick", .nome = "Tamanho do buffer",
    .frase = "Novo ajuste do player: Automático, 64, 128 ou 256 MB, dentro da memória livre.", .linhas = 4 },
  { .icone = "aj_hard-drive", .nome = "Cache no pendrive",
    .frase = "Agora no Android e na Samsung, com todos os detalhes do pendrive (Android, Samsung .tpk).", .linhas = 6, .plataformas = NOV_ANDROID | NOV_TPK | NOV_OUTRAS },
  { .icone = "aj_link", .nome = "Links externos",
    .frase = "Links nuvio-legacy://, stremio:// e do IMDb abrem o título, tocam ou buscam no Nuvio (Android).", .linhas = 6, .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_play", .nome = "Especiais",
    .frase = "Episódios especiais aparecem e podem ser reproduzidos (#556).", .linhas = 6 },
  { .icone = "aj_sliders-horizontal", .nome = "Escala de texto e componentes",
    .frase = "Texto de 85% a 130%, sem deslocar cartazes e controles.", .linhas = 4 },
  { .icone = "aj_layout-dashboard", .nome = "Espaçamento das fileiras",
    .frase = "Compacto, Normal ou Confortável na página do título.", .linhas = 4 },
  { .icone = "aj_droplet", .nome = "Foco na cor de destaque",
    .frase = "Realce o item selecionado com a sua cor em Aparência.", .linhas = 4 },
  { .grupo = "Correções", .icone = "aj_layout-dashboard", .nome = "Layout Apple TV",
    .frase = "Palco e Herói dividido mostram a arte inteira e escondem as fileiras acima, como na Moderna. O fundo padrão é Cinza, e Fundo do Palco passa a valer nesse layout.", .linhas = 8 },
  { .icone = "aj_play", .nome = "Trakt",
    .frase = "A lista mostra os mais recentes primeiro e carrega mesmo sem IMDb. Abra os comentários para ler as avaliações completas. Desconectar Trakt ou Simkl vale só para o perfil (#610). O histórico do Simkl volta a ser salvo corretamente.", .linhas = 10 },
  { .icone = "aj_link", .nome = "Modelo de URL dos pôsteres",
    .frase = "Serviços de pôster personalizados aceitam títulos sem TMDB quando esse campo é opcional (#566).", .linhas = 6 },
  { .icone = "aj_audio-lines", .nome = "Ajuste do áudio (ms)",
    .frase = "Positivo atrasa; negativo adianta. Só áudio convertido, a partir do próximo vídeo (Samsung .tpk) (#578).", .linhas = 6, .plataformas = NOV_TPK | NOV_OUTRAS },
  { .icone = "aj_droplet", .nome = "Episódios",
    .frase = "O painel mostra o nome completo e notas do IMDb, Trakt e TMDB como na página do título. Leia comentários e filtre só por notas que tenham comentários. O cartão flutua e ganha brilho no foco. A navegação fica mais fluida, sem deslocar a arte.", .linhas = 12 },
  { .icone = "aj_captions", .nome = "Legendas",
    .frase = "As legendas SRT mantêm negrito e itálico, e a seleção automática mantém o idioma (#604, #607). Na LG webOS, legendas PGS e ASS embutidas em arquivos MKV também são reconhecidas.", .linhas = 9 },
  { .icone = "aj_house", .nome = "Fileiras da Home",
    .frase = "Fileiras removidas somem, e a rolagem termina no lugar certo mesmo ao trocar de fileira (#545, #579).", .linhas = 6 },
  { .icone = "aj_puzzle", .nome = "Fontes",
    .frase = "Fontes de link direto voltaram, mesmo com o P2P desligado (#552). Veja os idiomas de áudio e legenda informados pelo add-on. A prioridade Do addon respeita a ordem das fontes sem priorizar qualidade ou escolhas anteriores.", .linhas = 10 },
  { .icone = "aj_search", .nome = "Busca",
    .frase = "O título exato vem primeiro, e séries com id do TMDB ganham os episódios (#555).", .linhas = 6 },
  { .icone = "aj_user-round", .nome = "PIN do perfil",
    .frase = "Digitar o PIN abre o perfil de primeira (#551).", .linhas = 6 },
  { .icone = "aj_users", .nome = "Elenco",
    .frase = "As fotos do elenco ficam mesmo quando o TMDB não responde.", .linhas = 3 },
  { .icone = "aj_monitor-play", .nome = "Player",
    .frase = "O vídeo abre mais rápido e as legendas ficam mais nítidas. No Android, ajuste o volume sem pausar e mude a velocidade com áudio. Na LG webOS, o protetor de tela fica desligado durante o filme. Vídeos interrompidos podem ser retomados, e Voltar fecha os painéis antes de sair.", .linhas = 12 },
  { .icone = "aj_sliders-horizontal", .nome = "Samsung .tpk",
    .frase = "O volume do trailer é mantido, o microfone volta na busca e o vídeo recupera a proporção ao sair do zoom (#603, #585). O player nativo mostra o selo HDR da fonte, nunca Dolby Vision.", .linhas = 9, .plataformas = NOV_TPK | NOV_OUTRAS },
};
static const NovCena CENAS_208[] = {
  { "Destaques", "Palco", visual208a, 5.5f, 0 },
  { "Destaques", "Trailer dublado", visual208b, 5.5f, 0 },
  { "Destaques", "Continuar assistindo", visual208c, 5.5f, 0 },
  { "Mais novidades", "Vidro fosco", visual208d, 5.5f, 0 },
};
static const int ARTES_208[] = { 2, 4, 7, 9 };
static const NovConteudo NOV_ATUAL = {
  .versao = "2.0.8", .arquivo = "novidades-208.txt",
  .subtitulo = NULL,
  .itens = ITENS_208, .nItens = (int)(sizeof ITENS_208 / sizeof *ITENS_208),
  .cenas = CENAS_208, .nCenas = 4,
  .artes = ARTES_208, .nArtes = 4,
};

// Small, static feature mockups. Only their entrance moves; no live video.
static void visual208(GfxRect p, float t, float a, int c) {
  float e = ajustes_animacoes_reduzidas() ? 1 : sai3(t / .45f);
  float x = p.x + 40, y = p.y + 100 + (1 - e) * 16;
  float w = p.w - 80;
  const char *icons[] = { "aj_layout-dashboard", "aj_audio-lines", "aj_play", "aj_droplet" };
  novArte(ARTES_208[c], (GfxRect){p.x, p.y, p.w, p.h}, PV_RAIO, .68f * a);
  gfx_rect(p, 0, GFX_VEU_CARD, 0, 0, 0, PV_RAIO / p.h, .025f, .03f, .045f, a);
  float vs = fminf(1, (p.h - 230) / 540);
  gfx_transformar(p.x, p.y, vs, 0, 0);
  gfx_icone((GfxRect){x, p.y + 36, 40, 40}, icons[c], .85f, .90f, 1, a);
  if (c == 0) {
    novArte(2, (GfxRect){x, y, w * .58f, 310}, 18, a * e);
    for (int j = 0; j < 2; j++) {
      GfxRect r = {x + w * .62f, y + j * 160, w * .38f, 145};
      novArte(ARTES_208[j + 1], r, 14, a * e);
    }
    gfx_cor((GfxRect){x + 14, y + 270, w * .45f, 4}, .5f, .9f, .9f, 1, .9f * a);
    for (int j = 0; j < 3; j++) gfx_cor((GfxRect){x, y + 340 + j * 16, w * (.8f - j * .12f), 5}, .5f, 1, 1, 1, .3f * a);
  } else if (c == 1) {
    novArte(4, (GfxRect){x, y, w, 310}, 20, a * e);
    gfx_icone((GfxRect){x + w / 2 - 36, y + 110, 72, 72}, "aj_play", 1, 1, 1, a);
    for (int j = 0; j < 17; j++) {
      float h = 12 + (j % 5) * 12;
      gfx_cor((GfxRect){x + j * (w / 17), y + 355 - h / 2, 8, h}, .4f, .75f, .84f, 1, .8f * a);
    }
    gfx_icone((GfxRect){x + w - 48, y + 250, 32, 32}, "aj_captions", 1, 1, 1, a);
  } else if (c == 2) {
    TxtLinha n = txt_linha(TXT_NOV_TITULO, "40", 245, 247, 255, 255);
    txt_desenhar_alpha(n, x, y, a * e);
    for (int j = 0; j < 3; j++) {
      float cw = (w - 32) / 3;
      GfxRect r = {x + j * (cw + 16), y + 100, cw, 240};
      novArte(ARTES_208[j], r, 14, a * e);
      gfx_cor((GfxRect){r.x + 12, r.y + r.h - 18, r.w - 24, 4}, .5f, 1, 1, 1, .3f * a);
      gfx_cor((GfxRect){r.x + 12, r.y + r.h - 18, (r.w - 24) * (.3f + .2f * j), 4}, .5f, .8f, .88f, 1, a);
    }
  } else {
    for (int j = 0; j < 3; j++) {
      GfxRect r = {x, y + j * 112, w, 90};
      gfx_vidro_folha(r, 18 / r.h, a * e);
      gfx_icone((GfxRect){x + 24, r.y + 25, 40, 40}, icons[j], .9f, .94f, 1, a);
      gfx_cor((GfxRect){x + 88, r.y + 38, w - 130, 6}, .5f, 1, 1, 1, .5f * a);
    }
  }
  gfx_sem_transformar();
}
