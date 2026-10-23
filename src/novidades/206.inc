// Conteudo da 2.0.6: docs/releases/2.0.6/NOVIDADES.md.
// Sem cenas nem artes. Tetos de linhas para as traducoes; o motor mede antes de paginar.
static const NovItem ITENS_206[] = {
  { .grupo = "Correções", .icone = "aj_captions", .nome = "Legendas",
    .frase = "O atraso salvo da legenda foi corrigido uma vez: o sinal estava invertido.", .linhas = 2 },
  { .icone = "aj_monitor-play", .nome = "Android TV (TCL)",
    .frase = "O app não fecha mais por falta de memória durante a reprodução.", .linhas = 2,
    .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_tv-minimal-play", .nome = "LG webOS",
    .frase = "Telas mais fluidas, com vidro mais leve e menos sobreposição.", .linhas = 2,
    .plataformas = NOV_LG | NOV_OUTRAS },
  { .grupo = "Novidades", .icone = "aj_user-round", .nome = "Amigos",
    .frase = "A fileira de Amigos usa pôsteres horizontais e cartões de pessoas.", .linhas = 2 },
  { .icone = "aj_image", .nome = "Trailer do destaque",
    .frase = "Os Ajustes mostram por que o trailer do destaque está bloqueado.", .linhas = 2 },
  { .icone = "aj_sliders-horizontal", .nome = "Dicas do controle",
    .frase = "As dicas do controle aparecem uma vez por versão.", .linhas = 2 },
};
static const NovConteudo NOV_ATUAL = {
  .versao = "2.0.6",
  .arquivo = "novidades-206.txt",
  .subtitulo = "Legendas, memória e telas mais fluidas.",
  .itens = ITENS_206, .nItens = (int)(sizeof ITENS_206 / sizeof *ITENS_206),
};
