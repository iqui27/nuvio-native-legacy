// Catalogo de apresentacao da Fase 1. Incluir depois de Item/SEC/ROT/OPC.
// A ordem do enum, CHAVE e valor[] nao participa deste mapa.
static const Item TELA[] = {
  SEC("Tela inicial", "Estilo, fileiras, barra lateral e Continuar assistindo", "aj_layout-dashboard"),
    ROT("Estilo"),
      OPC(AJ_HOME_LAYOUT), OPC(AJ_LANDSCAPE), OPC(AJ_HERO), OPC(AJ_HERO_CHEIO),
      OPC(AJ_HERO_CATALOGOS), OPC(AJ_HERO_TRANSICAO),
    ROT("Fileiras"),
      OPC(AJ_FIL_ORDEM), OPC(AJ_FIL_LIMITE), OPC(AJ_ITENS_FILEIRA),
      OPC(AJ_ESPACO_FILEIRAS), OPC(AJ_ESPACO_TITULOS),
      OPC(AJ_OCULTAR_NLANC), OPC(AJ_NOTAS_HOME), OPC(AJ_ROTULOS),
      OPC(AJ_NOME_ADDON), OPC(AJ_SUFIXO_TIPO), OPC(AJ_SELO_VISTO),
    ROT("Continuar assistindo"),
      OPC(AJ_CW_LIGADO), OPC(AJ_CW_OK), OPC(AJ_CW_ESTILO), OPC(AJ_CW_THUMB),
      OPC(AJ_CW_BLUR_PROX), OPC(AJ_CW_ORDEM), OPC(AJ_CW_NAO_EXIBIDOS),
      OPC(AJ_CW_FURTHEST), OPC(AJ_CW_RETIDO_TAMBEM),
    ROT("Barra lateral"),
      OPC(AJ_RAIL_MODERNA), OPC(AJ_RAIL), OPC(AJ_MENU_EXPLORAR),
      OPC(AJ_MENU_GUIA), OPC(AJ_MENU_AGENDA), OPC(AJ_MENU_PERFIL),
      OPC(AJ_RAIL_BLUR), OPC(AJ_DESCOBRIR),
    ROT("Escolha de perfil"), OPC(AJ_PS_FUNDO), OPC(AJ_PS_CONTINUAR),

  SEC("Página do título", "O que aparece, notas e dados do título", "aj_file-text"),
    ROT("O que aparece"),
      OPC(AJ_DET_VEU), OPC(AJ_DET_DATA_CHEIA), OPC(AJ_DET_BLUR_NAO_VISTOS),
    ROT("Notas"),
      OPC(AJ_MDB_CHAVE),
      OPC(AJ_NT_IMDB), OPC(AJ_NT_TOMATES), OPC(AJ_NT_AUDIENCIA), OPC(AJ_NT_META),
      OPC(AJ_NT_METAUSER), OPC(AJ_NT_TRAKT), OPC(AJ_NT_TMDB), OPC(AJ_NT_LETTER),
      OPC(AJ_NT_MAL), OPC(AJ_NT_EBERT), OPC(AJ_NT_SCORE), OPC(AJ_MDB_LIGADO),
      OPC(AJ_MDB_TRAKT), OPC(AJ_MDB_IMDB), OPC(AJ_MDB_TMDB), OPC(AJ_MDB_LETTER),
      OPC(AJ_MDB_TOMATES), OPC(AJ_MDB_AUDIENCIA), OPC(AJ_MDB_META), OPC(AJ_MDB_MAL),
    ROT("Dados do título"),
      OPC(AJ_TMDB_LIGADO), OPC(AJ_TMDB_ARTE), OPC(AJ_TMDB_BASICO),
      OPC(AJ_TMDB_FICHA), OPC(AJ_TMDB_DATAS), OPC(AJ_TMDB_ELENCO),
      OPC(AJ_TMDB_PROD), OPC(AJ_TMDB_REDES), OPC(AJ_TMDB_EPS),
      OPC(AJ_TMDB_TRAILERS), OPC(AJ_TMDB_MAIS), OPC(AJ_TMDB_COL), OPC(AJ_TMDB_CW),
      OPC(AJ_DET_META_EXT), OPC(AJ_DET_SO_CINEMETA),

  SEC("Cartazes e arte", "Cartaz em foco, formato e origem da arte", "aj_images"),
    ROT("Cartaz em foco"),
      OPC(AJ_EXPANDIR), OPC(AJ_EXPANDIR_ATRASO), OPC(AJ_BORDA_FOCO),
      OPC(AJ_GRAD_CLASSICO),
    ROT("Forma"),
      OPC(AJ_LARGURA_DP), OPC(AJ_RAIO_DP), OPC(AJ_PROF), OPC(AJ_PROF_BORDA),
      OPC(AJ_PROF_BRILHO), OPC(AJ_PROF_COBERTURA), OPC(AJ_PROF_POSTERS),
      OPC(AJ_PROF_CW), OPC(AJ_PROF_EPS), OPC(AJ_PROF_ELENCO),
      OPC(AJ_PROF_TRAILERS),
    ROT("De onde vem a arte"),
      OPC(AJ_HERO_FUNDO), OPC(AJ_HERO_ARTE_DIF), OPC(AJ_POSTER_PROV),
      OPC(AJ_FANART_CHAVE), OPC(AJ_ADDON_POSTER), OPC(AJ_ADDON_FUNDO), OPC(AJ_ADDON_LOGO),
      OPC(AJ_COL_ARTE_CONTA), OPC(AJ_POSTER_INST), OPC(AJ_POSTER_TOKEN),
      OPC(AJ_POSTER_EXTRA), OPC(AJ_POSTER_CHAVE), OPC(AJ_POSTER_MODELO),
      OPC(AJ_POSTER_TESTAR),

  SEC("Trailers", "Onde tocam, com que som e de qual fonte", "aj_clapperboard"),
    ROT("Onde toca"),
      OPC(AJ_HERO_TRAILER), OPC(AJ_FOCO_TRAILER), OPC(AJ_DET_TRAILER_AUTO),
      OPC(AJ_DET_TRAILER),
    ROT("Som e imagem"),
      OPC(AJ_HERO_TRAILER_SOM), OPC(AJ_DET_TRAILER_SOM), OPC(AJ_TRAILER_FONTE),
      OPC(AJ_HERO_TRAILER_ESPERA), OPC(AJ_TRAILER_QUAL), OPC(AJ_TRAILER_ASPECTO),
#ifdef NV_TPK
      OPC(AJ_TRAILER_ZOOM_TPK),
#endif
      OPC(AJ_LOGO_TRAILER),

  SEC("Aparência", "Cor, transparência, fonte, animações e relógio", "aj_palette"),
    ROT("Cores e vidro"),
      OPC(AJ_TEMA), OPC(AJ_FUNDO), OPC(AJ_COR_LOGO), OPC(AJ_VIDRO), OPC(AJ_VIDRO_CONTORNO),
      OPC(AJ_VIDRO_OPAC), OPC(AJ_VIDRO_FOSCO),
    ROT("Logo e abertura"),
      OPC(AJ_LOGO_APP), OPC(AJ_ABERTURA), OPC(AJ_ICONE_APP),
    ROT("Texto e tamanho"),
      OPC(AJ_FONTE_UI), OPC(AJ_TAMANHO_UI), OPC(AJ_TAMANHO_AJUSTES),
    ROT("Movimento e relógio"),
      OPC(AJ_ANIM), OPC(AJ_RELOGIO), OPC(AJ_RELOGIO_12H),
    ROT("Tela de descanso"),
      OPC(AJ_ESMAECER), OPC(AJ_DESCANSO_ESTILO), OPC(AJ_DESCANSO_FONTE),

  SEC("Reprodução", "Qualidade, som e o que o player faz", "aj_circle-play"),
    ROT("Imagem e som"), OPC(AJ_QUALIDADE), OPC(AJ_DV),
#if !defined(NV_TPK) && !defined(NV_ANDROID)
      OPC(AJ_DV_MKV),
#endif
      OPC(AJ_ATMOS), OPC(AJ_DTS_AC3), OPC(AJ_CACHE_SEEK),
    ROT("Durante e depois"),
      OPC(AJ_AUTO_CREDITOS),
      OPC(AJ_PAUSA_OVERLAY), OPC(AJ_PLR_CLASSIF), OPC(AJ_REACAO_CREDITOS), OPC(AJ_CW_CONCLUIDO), OPC(AJ_SAIDA_PLAYER),
      OPC(AJ_PROPORCAO_PADRAO), OPC(AJ_MANTER_VIDEO), OPC(AJ_BRILHO_PLAYER),
    ROT("Seekr"),
      OPC(AJ_SEEKR_LIGADO), OPC(AJ_SEEKR_CHAVE), OPC(AJ_SEEKR_TESTAR),
      OPC(AJ_SEEKR_FITA), OPC(AJ_SEEKR_AJUSTE),

  // Tudo o que decide DE ONDE vem o video: os addons, a escolha da fonte e os
  // servicos que entregam o arquivo. Antes ficava espalhado entre Reproducao
  // (escolha da fonte) e Contas e servicos (addons, debrid, P2P, servidores).
  SEC("Fontes e addons", "Addons, escolha da fonte, debrid e servidores", "aj_puzzle"),
    ROT("Addons"),
      OPC(AJ_ADDONS), OPC(AJ_PLUGINS), OPC(AJ_ADDONS_PRINCIPAL), OPC(AJ_BUSCA_CINEMETA),
    ROT("Escolha da fonte"),
      OPC(AJ_FONTE_MANUAL), OPC(AJ_FONTE_AUTO), OPC(AJ_FONTE_REPOR),
      OPC(AJ_FONTE_PRIORIDADE), OPC(AJ_FONTE_HDR), OPC(AJ_FONTE_PRAZO), OPC(AJ_FONTE_TOCAR_CONFERINDO), OPC(AJ_FONTE_AQUECER), OPC(AJ_FONTE_CONFERIR_VARIAS), OPC(AJ_FONTE_PREPARAR),
      // #202: o "Auto-play streams" do oficial (fonteregra.h).
      OPC(AJ_FONTE_ESCOPO), OPC(AJ_FONTE_ADDONS_PERM), OPC(AJ_FONTE_PLUGINS_PERM),
      OPC(AJ_FONTE_ORDEM), OPC(AJ_FONTE_ORDEM_USO),
      OPC(AJ_FONTE_OUTROS), OPC(AJ_FONTE_REGEX), OPC(AJ_FONTE_REGEX_PADRAO),
      OPC(AJ_FONTE_REGEX_MODELO),
      OPC(AJ_TAM_MAX), OPC(AJ_TAM_MIN), OPC(AJ_FONTE_TEXTO),
      OPC(AJ_SELOS_CORES),
      OPC(AJ_SELOS_PACOTE), OPC(AJ_SELOS_PACOTE_ADD), OPC(AJ_SELOS_PACOTE_REM),
    ROT("Debrid"),
      OPC(AJ_DEBRID_RD), OPC(AJ_DEBRID_TB), OPC(AJ_DEBRID_PM), OPC(AJ_DEBRID_AD),
      OPC(AJ_DEBRID_AD_TESTAR),
#ifndef __EMSCRIPTEN__
    // .wgt: sem socket TCP/UDP no navegador, sem motor; P2P escondido (a8e7685d).
    ROT("P2P"), OPC(AJ_P2P_LIGADO), OPC(AJ_P2P_URL), OPC(AJ_P2P_TESTAR),
#endif
    ROT("Servidores pessoais"),
      OPC(AJ_JF_LIGADO), OPC(AJ_JF_SERVIDOR), OPC(AJ_JF_ENTRAR), OPC(AJ_JF_SAIR),
      OPC(AJ_EM_SERVIDOR), OPC(AJ_EM_ENTRAR), OPC(AJ_EM_SAIR),
      OPC(AJ_PX_ENTRAR), OPC(AJ_PX_SERVIDOR), OPC(AJ_PX_SAIR),

  SEC("Idiomas e legendas", "Idioma da interface, áudio, legenda e metadados", "aj_languages"),
    ROT("Idiomas"),
      OPC(AJ_IDIOMA), OPC(AJ_AUD_LINGUA), OPC(AJ_LEG_LINGUA), OPC(AJ_LEG_FORCADA),
#ifdef NV_ANDROID
      // Sincronia pelo audio (F06): so o player do Android entrega o PCM. Na LG e
      // na Samsung a linha so diria "indisponivel nesta plataforma" (decisao do
      // dono, 04/10).
      OPC(AJ_LEG_SYNC_AUDIO),
#endif
      OPC(AJ_TMDB_IDIOMA),
    ROT("Segunda legenda"),
      OPC(AJ_LEG_LINGUA2),
      OPC(AJ_LEG2_POS), OPC(AJ_LEG2_TAMANHO), OPC(AJ_LEG2_COR), OPC(AJ_LEG2_FUNDO), OPC(AJ_LEG2_BORDA),

  SEC("TV ao vivo", "Provedor e solução de problemas ao assistir canais", "aj_tv-minimal-play"),
    ROT("Provedor"),
      OPC(AJ_XTREAM_SERVIDOR), OPC(AJ_XTREAM_USUARIO), OPC(AJ_XTREAM_SENHA),
      OPC(AJ_XTREAM_CONTA), OPC(AJ_XTREAM_LIMPAR), OPC(AJ_STALKER_PORTAL),
      OPC(AJ_STALKER_MAC), OPC(AJ_STALKER_LIMPAR), OPC(AJ_EPG_PAIS),
    ROT("Se o canal não abre"),
      OPC(AJ_LIVETV_DIAG), OPC(AJ_LIVETV_RES), OPC(AJ_LIVETV_ESPERA),
      OPC(AJ_LIVETV_FORMATO), OPC(AJ_LIVETV_MODO), OPC(AJ_LIVETV_PROXY),

  SEC("Conta e perfis", "Perfil, sincronização, Trakt, Simkl e avisos", "aj_user-round"),
    ROT("Perfil"),
      OPC(AJ_PERFIL_ATIVO), OPC(AJ_SYNC),
      OPC(AJ_PERFIL_PESQ), OPC(AJ_PERFIL_EDITAR), OPC(AJ_SAIR),
    ROT("Trakt e Simkl"),
      OPC(AJ_TRAKT), OPC(AJ_SIMKL), OPC(AJ_SALVOS_DEST), OPC(AJ_CW_FONTE), OPC(AJ_HIST_CONTA), OPC(AJ_SOCIAL), OPC(AJ_DISCORD),
    ROT("Notificações"), OPC(AJ_ENQUETES),

  SEC("Desempenho desta TV", "Resolução, imagens, efeitos e diagnóstico", "aj_monitor-cog"),
    OPC(AJ_RESOLUCAO), OPC(AJ_QUALIDADE_IMG),
#if defined(NV_TPK) || defined(NV_ANDROID)
    OPC(AJ_GPU_EFEITOS),
#endif
    OPC(AJ_NAV_RAPIDA), OPC(AJ_MEDIDOR), OPC(AJ_DIAGNOSTICO), OPC(AJ_VELOCIDADE), OPC(AJ_ESPACO),
    OPC(AJ_TEX_MB),

  SEC("Sobre e ajuda", "Guia de uso, versão, atualização e registros", "aj_info"),
    OPC(AJ_GUIA),
    OPC(AJ_NOVIDADES20),
    OPC(AJ_APOIAR),
    ROTS("App", ""), OPC(AJ_VERSAO_I), OPC(AJ_ATUALIZAR), OPC(AJ_AVANCADAS),
    ROTS("Registro", "para quem faz o app"),
      OPC(AJ_VER_REGISTRO), OPC(AJ_ENVIAR_LOG), OPC(AJ_ENVIO_AUTO),
};
#define AJ_N_TELA (int)(sizeof TELA / sizeof *TELA)
#define AJ_MAX_SECOES 16

// Texto exibido no painel ao selecionar uma categoria. Mesma ordem dos SECs.
static const char *SECAO_AJUDA[] = {
  "Estilo da home, fileiras, Continuar assistindo, barra lateral e escolha de perfil.",
  "Informações visíveis, notas e fontes de dados da página de cada título.",
  "Comportamento do cartaz em foco, formato dos cartões e origem das imagens.",
  "Configuração dos trailers no destaque, no cartaz em foco e na página do título.",
  "Cor de destaque, transparência, tipografia, animações e relógio na tela.",
  "Qualidade máxima, Dolby Vision e Atmos, e o que acontece durante e depois do vídeo.",
  "Addons e plugins, como a fonte é escolhida, debrid, P2P e servidores pessoais.",
  "Idioma da interface, preferências de áudio e legenda e idioma dos metadados.",
  "Conexão dos provedores e diagnóstico de reprodução de canais ao vivo.",
  "Perfil desta TV, sincronização, Trakt, Simkl e notificações.",
  "Recursos e diagnóstico deste aparelho.",
  "Versão instalada, atualização e envio de registros para a equipe do app.",
};

// Basic by default. Glass and its outline remain visible together in Appearance.
// Technical options below are revealed through the advanced control.
static int uxAvancada(int op) {
  switch (op) {
    case AJ_HERO_TRANSICAO:
    case AJ_ROTULOS: case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO:
    case AJ_RAIL_BLUR: case AJ_DESCOBRIR:
    case AJ_CW_FURTHEST:
    case AJ_GRAD_CLASSICO:
    case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA:
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
    case AJ_ADDON_POSTER: case AJ_ADDON_FUNDO: case AJ_ADDON_LOGO:
    case AJ_COL_ARTE_CONTA: case AJ_POSTER_INST: case AJ_POSTER_TOKEN:
    case AJ_POSTER_EXTRA: case AJ_POSTER_CHAVE: case AJ_POSTER_MODELO:
    case AJ_POSTER_TESTAR:
    case AJ_HERO_TRAILER_ESPERA: case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO:
    case AJ_DET_META_EXT: case AJ_DET_SO_CINEMETA:
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB: case AJ_MDB_LETTER:
    case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA: case AJ_MDB_META: case AJ_MDB_MAL:
    case AJ_TMDB_ARTE: case AJ_TMDB_BASICO: case AJ_TMDB_FICHA:
    case AJ_TMDB_DATAS: case AJ_TMDB_ELENCO:
    case AJ_TMDB_PROD: case AJ_TMDB_REDES: case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS:
    case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW:
    case AJ_SELOS_CORES:
    case AJ_SELOS_PACOTE: case AJ_SELOS_PACOTE_ADD: case AJ_SELOS_PACOTE_REM:
    case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO:
    case AJ_LIVETV_FORMATO: case AJ_LIVETV_MODO: case AJ_LIVETV_PROXY:
    case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR:
    case AJ_TEX_MB:
    case AJ_SEEKR_AJUSTE:
    case AJ_MANTER_VIDEO:
      return 1;
    default:
      return 0;
  }
}
