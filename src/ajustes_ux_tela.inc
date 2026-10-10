// Catalogo de apresentacao. Incluir depois de Item/SEC/ROT/OPC/MAIS.
// A ordem do enum, CHAVE e valor[] nao participa deste mapa: reordenar aqui nao
// mexe em nada que esta no disco ou na conta.
//
// 2.0.3 (estudo ajustes-ux, aprovado pelo dono em 07/10): as categorias seguem
// com os NOMES que a comunidade conhece, mas na ordem da tarefa — Assistir
// (Reproducao, Idiomas e legendas, Fontes e addons, TV ao vivo), Telas (Tela
// inicial, Aparencia, Pagina do titulo, Cartazes e trailers) e Conta e sistema
// (Conta e servicos, Esta TV, Sobre e ajuda). O que e tecnico fica DEPOIS do
// MAIS() da categoria: uma linha "Mais opcoes · N" fechada no fim da lista, que
// o OK abre no lugar (sem terceira tela). A pilula "Avancadas" do indice abre
// todas de uma vez. uxAvancada() le esta tabela; nao ha outra lista.
static const Item TELA[] = {
  // ---- ASSISTIR ------------------------------------------------------------------
  SEC("Reprodução", "Qualidade, som e o que o player faz", "aj_circle-play"),
    ROT("Imagem e som"),
      OPC(AJ_QUALIDADE), OPC(AJ_FONTE_HDR), OPC(AJ_DV), OPC(AJ_ATMOS),
    ROT("Escolha da fonte"),
      OPC(AJ_FONTE_MANUAL), OPC(AJ_FONTE_PRIORIDADE), OPC(AJ_FONTE_PRAZO),
      OPC(AJ_FONTE_REPOR), OPC(AJ_TAM_MAX),
    ROT("Durante e depois"),
      OPC(AJ_PAUSA_OVERLAY), OPC(AJ_PLR_CLASSIF), OPC(AJ_REACAO_CREDITOS), OPC(AJ_CW_CONCLUIDO),
      OPC(AJ_SAIDA_PLAYER), OPC(AJ_PROPORCAO_PADRAO), OPC(AJ_BRILHO_PLAYER),
    ROT("Miniatura ao avançar"),
      OPC(AJ_SEEKR_LIGADO), OPC(AJ_SEEKR_CHAVE), OPC(AJ_SEEKR_TESTAR), OPC(AJ_SEEKR_FITA),
    MAIS(),
    ROT("Escolha da fonte"),
      // #310: o "Auto-play streams" do oficial (fonteregra.h).
      OPC(AJ_FONTE_REGEX), OPC(AJ_FONTE_REGEX_PADRAO), OPC(AJ_FONTE_REGEX_MODELO),
      OPC(AJ_FONTE_ESCOPO), OPC(AJ_FONTE_ADDONS_PERM), OPC(AJ_FONTE_PLUGINS_PERM),
      OPC(AJ_FONTE_ORDEM), OPC(AJ_FONTE_ORDEM_USO), OPC(AJ_FONTE_OUTROS),
      OPC(AJ_FONTE_AUTO), OPC(AJ_FONTE_TOCAR_CONFERINDO), OPC(AJ_FONTE_AQUECER),
      OPC(AJ_FONTE_CONFERIR_VARIAS), OPC(AJ_FONTE_PREPARAR),
      OPC(AJ_TAM_MIN), OPC(AJ_FONTE_TEXTO), OPC(AJ_FONTE_ORDEM_ADDON), OPC(AJ_SELOS_CORES),
      OPC(AJ_SELOS_PACOTE), OPC(AJ_SELOS_PACOTE_ADD), OPC(AJ_SELOS_PACOTE_REM),
    ROT("Som e vídeo desta TV"),
      OPC(AJ_DTS_AC3),
#if !defined(NV_TPK) && !defined(NV_ANDROID)
      OPC(AJ_DV_MKV),
#endif
      OPC(AJ_CACHE_SEEK), OPC(AJ_MANTER_VIDEO), OPC(AJ_SEEKR_AJUSTE),
#ifndef __EMSCRIPTEN__
    // .wgt: sem socket TCP/UDP no navegador, sem motor; P2P escondido (a8e7685d).
    ROT("P2P"), OPC(AJ_P2P_LIGADO), OPC(AJ_P2P_URL), OPC(AJ_P2P_TESTAR), OPC(AJ_P2P_LIMITE),
#endif

  SEC("Idiomas e legendas", "Idioma da interface, áudio, legenda e metadados", "aj_languages"),
    ROT("Idiomas"),
      OPC(AJ_IDIOMA), OPC(AJ_AUD_LINGUA), OPC(AJ_LEG_LINGUA), OPC(AJ_LEG_FORCADA),
      OPC(AJ_LEG_SYNC_AUTO),
#ifdef NV_ANDROID
      // Sincronia pelo audio (F06): so o player do Android entrega o PCM. Na LG e
      // na Samsung a linha so diria "indisponivel nesta plataforma" (decisao do
      // dono, 04/10).
      OPC(AJ_LEG_SYNC_AUDIO),
#endif
    ROT("Segunda legenda"),
      OPC(AJ_LEG_LINGUA2),
      OPC(AJ_LEG2_POS), OPC(AJ_LEG2_TAMANHO), OPC(AJ_LEG2_COR), OPC(AJ_LEG2_FUNDO), OPC(AJ_LEG2_BORDA),
    MAIS(),
    ROT("Idiomas"), OPC(AJ_TMDB_IDIOMA),

  // Tudo o que decide DE ONDE vem o video: os addons, os debrid e os servidores.
  // A escolha da fonte foi para Reproducao (mockup v3, tela 02).
  SEC("Fontes e addons", "Addons, debrid e servidores pessoais", "aj_puzzle"),
    ROT("Addons"),
      OPC(AJ_ADDONS), OPC(AJ_PLUGINS), OPC(AJ_ADDONS_PRINCIPAL), OPC(AJ_BUSCA_CINEMETA),
      OPC(AJ_BUSCA_NUVIO), OPC(AJ_BUSCA_ORIGEM),   // #311
    ROT("Debrid"),
      OPC(AJ_DEBRID_RD), OPC(AJ_DEBRID_TB), OPC(AJ_DEBRID_PM), OPC(AJ_DEBRID_AD),
      OPC(AJ_DEBRID_AD_TESTAR),
    ROT("Servidores pessoais"),
      OPC(AJ_JF_LIGADO), OPC(AJ_JF_SERVIDOR), OPC(AJ_JF_ENTRAR), OPC(AJ_JF_SAIR),
      OPC(AJ_EM_SERVIDOR), OPC(AJ_EM_ENTRAR), OPC(AJ_EM_SAIR),
      OPC(AJ_PX_ENTRAR), OPC(AJ_PX_SERVIDOR), OPC(AJ_PX_SAIR),

  SEC("TV ao vivo", "Provedor e solução de problemas ao assistir canais", "aj_tv-minimal-play"),
    ROT("Provedor"),
      OPC(AJ_XTREAM_SERVIDOR), OPC(AJ_XTREAM_USUARIO), OPC(AJ_XTREAM_SENHA),
      OPC(AJ_XTREAM_CONTA), OPC(AJ_XTREAM_LIMPAR), OPC(AJ_STALKER_PORTAL),
      OPC(AJ_STALKER_MAC), OPC(AJ_STALKER_LIMPAR), OPC(AJ_EPG_PAIS),
    ROT("Se o canal não abre"),
      OPC(AJ_LIVETV_DIAG), OPC(AJ_LIVETV_RES), OPC(AJ_LIVETV_ESPERA),
    MAIS(),
    ROT("Se o canal não abre"),
      OPC(AJ_LIVETV_FORMATO), OPC(AJ_LIVETV_MODO), OPC(AJ_LIVETV_PROXY),

  // ---- TELAS ---------------------------------------------------------------------
  SEC("Tela inicial", "Estilo, fileiras, barra lateral e Continuar assistindo", "aj_layout-dashboard"),
    ROT("Estilo"),
      OPC(AJ_HOME_LAYOUT), OPC(AJ_LANDSCAPE), OPC(AJ_HERO), OPC(AJ_HERO_CHEIO),
      OPC(AJ_HERO_CATALOGOS),
    ROT("Fileiras"),
      OPC(AJ_FIL_ORDEM), OPC(AJ_FIL_LIMITE), OPC(AJ_ITENS_FILEIRA),
      OPC(AJ_ESPACO_FILEIRAS), OPC(AJ_ESPACO_TITULOS),
      OPC(AJ_OCULTAR_NLANC), OPC(AJ_NOTAS_HOME), OPC(AJ_SELO_VISTO),
    ROT("Continuar assistindo"),
      OPC(AJ_CW_LIGADO), OPC(AJ_CW_OK), OPC(AJ_CW_ESTILO), OPC(AJ_CW_THUMB),
      OPC(AJ_CW_BLUR_PROX), OPC(AJ_CW_ORDEM), OPC(AJ_CW_NAO_EXIBIDOS),
      OPC(AJ_CW_RETIDO_TAMBEM),
    ROT("Barra lateral"),
      OPC(AJ_RAIL_MODERNA), OPC(AJ_RAIL), OPC(AJ_MENU_EXPLORAR),
      OPC(AJ_MENU_GUIA), OPC(AJ_MENU_AGENDA), OPC(AJ_MENU_PERFIL),
    ROT("Escolha de perfil"), OPC(AJ_PS_FUNDO), OPC(AJ_PS_CONTINUAR),
    MAIS(),
    ROT("Estilo"), OPC(AJ_HERO_TRANSICAO),
    ROT("Fileiras"), OPC(AJ_ROTULOS), OPC(AJ_NOME_ADDON), OPC(AJ_SUFIXO_TIPO),
    ROT("Continuar assistindo"), OPC(AJ_CW_FURTHEST),
    ROT("Barra lateral"), OPC(AJ_RAIL_BLUR),

  SEC("Aparência", "Cor, transparência, fonte, animações e relógio", "aj_palette"),
    ROT("Cores e vidro"),
      OPC(AJ_TEMA), OPC(AJ_FUNDO), OPC(AJ_COR_LOGO), OPC(AJ_VIDRO), OPC(AJ_VIDRO_CONTORNO),
    ROT("Logo e abertura"),
      OPC(AJ_LOGO_APP), OPC(AJ_ABERTURA), OPC(AJ_ICONE_APP),
    ROT("Texto e tamanho"),
      OPC(AJ_FONTE_UI), OPC(AJ_TAMANHO_UI), OPC(AJ_TAMANHO_AJUSTES), OPC(AJ_LAYOUT_AJUSTES),
    ROT("Movimento e relógio"),
      OPC(AJ_ANIM), OPC(AJ_RELOGIO), OPC(AJ_RELOGIO_12H),
    ROT("Tela de descanso"),
      OPC(AJ_ESMAECER), OPC(AJ_DESCANSO_ESTILO), OPC(AJ_DESCANSO_FONTE),
    MAIS(),
    ROT("Cores e vidro"), OPC(AJ_VIDRO_OPAC), OPC(AJ_VIDRO_FOSCO),

  // NOTAS (decisao do dono, 07/10): so as onze "Notas no titulo". Os oito
  // AJ_MDB_* (qual nota o MDBList baixa) sairam da tela e continuam no enum e na
  // conta: ligar uma nota liga o par dela e o MDBList por baixo (notaLigarPar).
  SEC("Página do título", "O que aparece, notas e dados do título", "aj_file-text"),
    ROT("O que aparece"),
      OPC(AJ_DET_VEU), OPC(AJ_DET_DATA_CHEIA), OPC(AJ_DET_BLUR_NAO_VISTOS),
    ROT("Notas"),
      OPC(AJ_NT_IMDB), OPC(AJ_NT_TOMATES), OPC(AJ_NT_AUDIENCIA), OPC(AJ_NT_META),
      OPC(AJ_NT_METAUSER), OPC(AJ_NT_TRAKT), OPC(AJ_NT_TMDB), OPC(AJ_NT_LETTER),
      OPC(AJ_NT_MAL), OPC(AJ_NT_EBERT), OPC(AJ_NT_SCORE), OPC(AJ_MDB_CHAVE),
    ROT("Dados do título"),
      OPC(AJ_TMDB_LIGADO),
    MAIS(),
    ROT("Notas"), OPC(AJ_MDB_LIGADO),
    ROT("Dados do título"),
      OPC(AJ_TMDB_ARTE), OPC(AJ_TMDB_BASICO),
      OPC(AJ_TMDB_FICHA), OPC(AJ_TMDB_DATAS), OPC(AJ_TMDB_ELENCO),
      OPC(AJ_TMDB_PROD), OPC(AJ_TMDB_REDES), OPC(AJ_TMDB_EPS),
      OPC(AJ_TMDB_TRAILERS), OPC(AJ_TMDB_MAIS), OPC(AJ_TMDB_COL), OPC(AJ_TMDB_CW),
      OPC(AJ_DET_META_EXT), OPC(AJ_DET_SO_CINEMETA),

  // Cartazes e arte + Trailers numa categoria so (mockup v3, tela 01).
  SEC("Cartazes e trailers", "Cartaz em foco, de onde vem a arte e trailers", "aj_clapperboard"),
    ROT("Cartaz em foco"),
      OPC(AJ_EXPANDIR), OPC(AJ_EXPANDIR_ATRASO), OPC(AJ_BORDA_FOCO),
    ROT("Forma"),
      OPC(AJ_LARGURA_DP), OPC(AJ_RAIO_DP), OPC(AJ_PROF),
    ROT("De onde vem a arte"),
      OPC(AJ_HERO_FUNDO), OPC(AJ_HERO_ARTE_DIF), OPC(AJ_POSTER_PROV), OPC(AJ_FANART_CHAVE),
    ROT("Trailers"),
      OPC(AJ_HERO_TRAILER), OPC(AJ_FOCO_TRAILER), OPC(AJ_DET_TRAILER_AUTO),
      OPC(AJ_DET_TRAILER),
    ROT("Som e imagem"),
      OPC(AJ_HERO_TRAILER_SOM), OPC(AJ_DET_TRAILER_SOM), OPC(AJ_TRAILER_FONTE),
#ifdef NV_TPK
      OPC(AJ_TRAILER_ZOOM_TPK),
#endif
      OPC(AJ_LOGO_TRAILER),
    MAIS(),
    ROT("Cartaz em foco"), OPC(AJ_GRAD_CLASSICO),
    ROT("Forma"),
      OPC(AJ_PROF_BORDA), OPC(AJ_PROF_BRILHO), OPC(AJ_PROF_COBERTURA), OPC(AJ_PROF_POSTERS),
      OPC(AJ_PROF_CW), OPC(AJ_PROF_EPS), OPC(AJ_PROF_ELENCO), OPC(AJ_PROF_TRAILERS),
    ROT("De onde vem a arte"),
      OPC(AJ_ADDON_POSTER), OPC(AJ_ADDON_FUNDO), OPC(AJ_ADDON_LOGO),
      OPC(AJ_COL_ARTE_CONTA), OPC(AJ_POSTER_INST), OPC(AJ_POSTER_TOKEN),
      OPC(AJ_POSTER_EXTRA), OPC(AJ_POSTER_CHAVE), OPC(AJ_POSTER_MODELO),
      OPC(AJ_POSTER_TESTAR),
    ROT("Trailers"),
      OPC(AJ_HERO_TRAILER_ESPERA), OPC(AJ_TRAILER_QUAL), OPC(AJ_TRAILER_ASPECTO),

  // ---- CONTA E SISTEMA -----------------------------------------------------------
  SEC("Conta e serviços", "Perfil, sincronização, Trakt, Simkl e avisos", "aj_user-round"),
    ROT("Perfil"),
      OPC(AJ_PERFIL_ATIVO), OPC(AJ_SYNC),
      OPC(AJ_PERFIL_PESQ), OPC(AJ_PERFIL_EDITAR), OPC(AJ_SAIR),
    ROT("Trakt e Simkl"),
      OPC(AJ_TRAKT), OPC(AJ_SIMKL), OPC(AJ_SALVOS_DEST), OPC(AJ_CW_FONTE), OPC(AJ_CW_PROXIMO), OPC(AJ_HIST_CONTA), OPC(AJ_SOCIAL), OPC(AJ_DISCORD),
    ROT("Notificações"), OPC(AJ_ENQUETES),

  SEC("Esta TV", "Resolução, imagens, efeitos e diagnóstico", "aj_monitor-cog"),
    OPC(AJ_RESOLUCAO), OPC(AJ_QUALIDADE_IMG),
#if defined(NV_TPK) || defined(NV_ANDROID)
    OPC(AJ_GPU_EFEITOS),
#endif
    OPC(AJ_NAV_RAPIDA), OPC(AJ_MEDIDOR), OPC(AJ_DIAGNOSTICO), OPC(AJ_VELOCIDADE), OPC(AJ_ESPACO),
    MAIS(),
    ROT("Memória"), OPC(AJ_TEX_MB),

  SEC("Sobre e ajuda", "Guia de uso, versão, atualização e registros", "aj_info"),
    OPC(AJ_GUIA),
    OPC(AJ_NOVIDADES20),
    OPC(AJ_APOIAR),
    ROTS("App", ""), OPC(AJ_VERSAO_I), OPC(AJ_ATUALIZAR),
    ROTS("Registro", "para quem faz o app"),
      OPC(AJ_VER_REGISTRO), OPC(AJ_ENVIAR_LOG), OPC(AJ_ENVIO_AUTO),
};
#define AJ_N_TELA (int)(sizeof TELA / sizeof *TELA)
#define AJ_MAX_SECOES 16

// Texto exibido no painel ao selecionar uma categoria. Mesma ordem dos SECs.
static const char *SECAO_AJUDA[] = {
  "Qualidade máxima, Dolby Vision e Atmos, e o que acontece durante e depois do vídeo.",
  "Idioma da interface, preferências de áudio e legenda e idioma dos metadados.",
  "Addons e plugins, debrid e servidores pessoais.",
  "Conexão dos provedores e diagnóstico de reprodução de canais ao vivo.",
  "Estilo da home, fileiras, Continuar assistindo, barra lateral e escolha de perfil.",
  "Cor de destaque, transparência, tipografia, animações e relógio na tela.",
  "Informações visíveis, notas e fontes de dados da página de cada título.",
  "Cartaz em foco, de onde vem a arte e os trailers da home e da página do título.",
  "Perfil desta TV, sincronização, Trakt, Simkl e notificações.",
  "Recursos e diagnóstico deste aparelho.",
  "Versão instalada, atualização e envio de registros para a equipe do app.",
};

// AVANCADA = vem depois do MAIS() da propria categoria. Calculado uma vez em
// montarTela (ajustes.c); a busca e os atalhos perguntam por aqui.
static unsigned char uxAvancadaTab[AJ_N];
static void montarTela(void);
static int uxAvancada(int op) {
  montarTela();
  return op >= 0 && op < AJ_N && uxAvancadaTab[op];
}
