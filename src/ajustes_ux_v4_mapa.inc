// Presentation routes from approved v4 catalog; no serialized IDs or values.
static const char *const AJ4_GRUPOS[] = {
  "Imagem e som",
  "Durante e depois",
  "Escolha da fonte",
  "Lista de fontes",
  "P2P",
  "Miniatura ao avançar",
  "Compatibilidade da TV",
  "Regras das fontes",
  "Idiomas",
  "Segunda legenda",
  "Addons",
  "Debrid",
  "Servidores pessoais",
  "Provedor",
  "Se o canal não abre",
  "Estilo da tela inicial",
  "Fileiras",
  "Navegação",
  "Escolha de perfil",
  "Continuar assistindo",
  "Página do título",
  "Metadados",
  "Notas",
  "Fontes de arte",
  "Trailers",
  "Cartaz em foco",
  "Imagem e som dos trailers",
  "Formato dos cartazes",
  "Movimento e relógio",
  "Cor e material",
  "Texto e tamanho",
  "Identidade do app",
  "Proteção da tela",
  "Interface",
  "Contas conectadas",
  "Perfil",
  "Notificações",
  "Conquistas",
  "Desempenho",
  "App e atualizações",
  "Diagnóstico e registros",
  "Memória de imagens",
  "Ajuda",
  "Compatibilidade de reprodução",
};
enum { AJ4_N_GRUPOS = sizeof AJ4_GRUPOS / sizeof *AJ4_GRUPOS };
static const int AJ4_GRUPO_BASE[] = {0,8,10,13,15,23,34,38};
static int aj4MapaSecao(int op) {
  switch (op) {
    case AJ_QUALIDADE: case AJ_DV: case AJ_ATMOS: case AJ_PAUSA_OVERLAY: case AJ_FONTE_MANUAL: case AJ_FONTE_AUTO: case AJ_FONTE_REPOR: case AJ_FONTE_TEXTO: case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR: case AJ_CW_CONCLUIDO: case AJ_SELOS_CORES: case AJ_SEEKR_LIGADO: case AJ_SEEKR_CHAVE: case AJ_SEEKR_TESTAR: case AJ_SEEKR_FITA: case AJ_SEEKR_AJUSTE: case AJ_SAIDA_PLAYER: case AJ_REACAO_CREDITOS: case AJ_FONTE_PRAZO: case AJ_SELOS_PACOTE: case AJ_SELOS_PACOTE_ADD: case AJ_SELOS_PACOTE_REM: case AJ_FONTE_PRIORIDADE: case AJ_FONTE_HDR: case AJ_BRILHO_PLAYER: case AJ_TAM_MAX: case AJ_TAM_MIN: case AJ_PLR_CLASSIF: case AJ_FONTE_ESCOPO: case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: case AJ_FONTE_OUTROS: case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO: case AJ_PROPORCAO_PADRAO: case AJ_FONTE_ORDEM: case AJ_FONTE_ORDEM_USO: case AJ_FONTE_TOCAR_CONFERINDO: case AJ_FONTE_AQUECER: case AJ_FONTE_CONFERIR_VARIAS: case AJ_FONTE_PREPARAR: case AJ_P2P_LIMITE: case AJ_FONTE_ORDEM_ADDON: case AJ_HDR_UI: case AJ_TELA_ABERTURA: case AJ_CW_MINIMO: case AJ_PULAR_ABERTURA: case AJ_PULAR_RESUMO: case AJ_PROXIMO_AUTO: case AJ_PULAR_CREDITOS: case AJ_PROXIMO_CONTAGEM: return 0;
    case AJ_LEG_LINGUA: case AJ_AUD_LINGUA: case AJ_IDIOMA: case AJ_TMDB_IDIOMA: case AJ_LEG_LINGUA2: case AJ_LEG_SYNC_AUDIO: case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA: case AJ_LEG_FORCADA: case AJ_LEG_SYNC_AUTO: case AJ_LEG_MOSTRAR: return 1;
    case AJ_ADDONS: case AJ_DEBRID_AD: case AJ_DEBRID_AD_TESTAR: case AJ_DEBRID_RD: case AJ_DEBRID_TB: case AJ_DEBRID_PM: case AJ_ADDONS_PRINCIPAL: case AJ_PLUGINS: case AJ_JF_LIGADO: case AJ_JF_SERVIDOR: case AJ_JF_ENTRAR: case AJ_JF_SAIR: case AJ_EM_SERVIDOR: case AJ_EM_ENTRAR: case AJ_EM_SAIR: case AJ_PX_ENTRAR: case AJ_PX_SERVIDOR: case AJ_PX_SAIR: case AJ_BUSCA_CINEMETA: case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM: return 2;
    case AJ_STALKER_PORTAL: case AJ_STALKER_MAC: case AJ_STALKER_LIMPAR: case AJ_XTREAM_SERVIDOR: case AJ_XTREAM_USUARIO: case AJ_XTREAM_SENHA: case AJ_XTREAM_LIMPAR: case AJ_XTREAM_CONTA: case AJ_EPG_PAIS: case AJ_LIVETV_RES: case AJ_LIVETV_FORMATO: case AJ_LIVETV_ESPERA: case AJ_LIVETV_DIAG: case AJ_LIVETV_MODO: case AJ_LIVETV_PROXY: return 3;
    case AJ_LANDSCAPE: case AJ_HERO_CHEIO: case AJ_FIL_LIMITE: case AJ_FIL_ORDEM: case AJ_RAIL: case AJ_RAIL_MODERNA: case AJ_RAIL_BLUR: case AJ_HERO: case AJ_HERO_CATALOGOS: case AJ_PS_FUNDO: case AJ_ROTULOS: case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO: case AJ_OCULTAR_NLANC: case AJ_NOTAS_HOME: case AJ_CW_LIGADO: case AJ_CW_OK: case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_BLUR_PROX: case AJ_CW_FURTHEST: case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM: case AJ_DET_BLUR_NAO_VISTOS: case AJ_DET_META_EXT: case AJ_DET_DATA_CHEIA: case AJ_DET_VEU: case AJ_TMDB_LIGADO: case AJ_TMDB_ARTE: case AJ_TMDB_BASICO: case AJ_TMDB_FICHA: case AJ_TMDB_DATAS: case AJ_TMDB_ELENCO: case AJ_TMDB_PROD: case AJ_TMDB_REDES: case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS: case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW: case AJ_MDB_LIGADO: case AJ_MDB_CHAVE: case AJ_MENU_EXPLORAR: case AJ_MENU_GUIA: case AJ_MENU_AGENDA: case AJ_MENU_PERFIL: case AJ_ITENS_FILEIRA: case AJ_HOME_LAYOUT: case AJ_DET_SO_CINEMETA: case AJ_NT_IMDB: case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META: case AJ_NT_METAUSER: case AJ_NT_TRAKT: case AJ_NT_TMDB: case AJ_NT_LETTER: case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE: case AJ_HERO_TRANSICAO: case AJ_SELO_VISTO: case AJ_ESPACO_FILEIRAS: case AJ_ESPACO_TITULOS: case AJ_CW_RETIDO_TAMBEM: case AJ_PS_CONTINUAR: case AJ_MIC_BUSCA: case AJ_MENU_TOPO: return 4;
    case AJ_HERO_FUNDO: case AJ_HERO_ARTE_DIF: case AJ_HERO_TRAILER: case AJ_GRAD_CLASSICO: case AJ_DET_TRAILER: case AJ_DET_TRAILER_AUTO: case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO: case AJ_TRAILER_FONTE: case AJ_EXPANDIR: case AJ_EXPANDIR_ATRASO: case AJ_BORDA_FOCO: case AJ_PROF: case AJ_PROF_BRILHO: case AJ_LARGURA_DP: case AJ_RAIO_DP: case AJ_ANIM: case AJ_TEMA: case AJ_COR_LOGO: case AJ_FANART_CHAVE: case AJ_FOCO_TRAILER: case AJ_FONTE_UI: case AJ_VIDRO: case AJ_POSTER_PROV: case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA: case AJ_POSTER_CHAVE: case AJ_POSTER_MODELO: case AJ_POSTER_TESTAR: case AJ_VIDRO_CONTORNO: case AJ_HERO_TRAILER_SOM: case AJ_HERO_TRAILER_ESPERA: case AJ_ADDON_POSTER: case AJ_ADDON_FUNDO: case AJ_ADDON_LOGO: case AJ_COL_ARTE_CONTA: case AJ_DET_TRAILER_SOM: case AJ_RELOGIO: case AJ_TAMANHO_UI: case AJ_FUNDO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: case AJ_ICONE_APP: case AJ_TAMANHO_AJUSTES: case AJ_LOGO_TRAILER: case AJ_TRAILER_ZOOM_TPK: case AJ_LOGO_APP: case AJ_ABERTURA: case AJ_ESMAECER: case AJ_DESCANSO_ESTILO: case AJ_DESCANSO_FONTE: case AJ_RELOGIO_12H: case AJ_LAYOUT_AJUSTES: case AJ_INTERFACE_MODO: return 5;
    case AJ_CW_FONTE: case AJ_PERFIL_ATIVO: case AJ_SYNC: case AJ_SALVOS_DEST: case AJ_TRAKT: case AJ_SIMKL: case AJ_SAIR: case AJ_PERFIL_PESQ: case AJ_PERFIL_EDITAR: case AJ_DISCORD: case AJ_ENQUETES: case AJ_HIST_CONTA: case AJ_SOCIAL: case AJ_CW_PROXIMO: case AJ_CONQ_LIGADO: case AJ_CONQ_AVISO: case AJ_CONQ_DICAS: case AJ_PERFIL_INFANTIL: case AJ_DIARIO_NUVEM: return 6;
    case AJ_NAV_RAPIDA: case AJ_QUALIDADE_IMG: case AJ_RESOLUCAO: case AJ_VERSAO_I: case AJ_ATUALIZAR: case AJ_ENVIAR_LOG: case AJ_ENVIO_AUTO: case AJ_ESPACO: case AJ_TEX_MB: case AJ_DIAGNOSTICO: case AJ_VELOCIDADE: case AJ_GPU_EFEITOS: case AJ_VER_REGISTRO: case AJ_MEDIDOR: case AJ_GUIA: case AJ_CACHE_SEEK: case AJ_NOVIDADES20: case AJ_MANTER_VIDEO: case AJ_APOIAR: case AJ_DTS_AC3: case AJ_DV_MKV: case AJ_AVISO_LEGAL: case AJ_BUFFER: return 7;
    default: return -1;
  }
}
static int aj4MapaGrupo(int op) {
  switch (op) {
    case AJ_QUALIDADE: case AJ_DV: case AJ_ATMOS: case AJ_FONTE_HDR: return 0;
    case AJ_PAUSA_OVERLAY: case AJ_CW_CONCLUIDO: case AJ_SAIDA_PLAYER: case AJ_REACAO_CREDITOS: case AJ_BRILHO_PLAYER: case AJ_PLR_CLASSIF: case AJ_PROPORCAO_PADRAO: case AJ_HDR_UI: case AJ_TELA_ABERTURA: case AJ_CW_MINIMO: case AJ_PULAR_ABERTURA: case AJ_PULAR_RESUMO: case AJ_PROXIMO_AUTO: case AJ_PULAR_CREDITOS: case AJ_PROXIMO_CONTAGEM: return 1;
    case AJ_FONTE_MANUAL: case AJ_FONTE_AUTO: case AJ_FONTE_REPOR: case AJ_FONTE_PRAZO: case AJ_FONTE_PRIORIDADE: case AJ_TAM_MAX: case AJ_TAM_MIN: case AJ_FONTE_TOCAR_CONFERINDO: case AJ_FONTE_AQUECER: case AJ_FONTE_CONFERIR_VARIAS: case AJ_FONTE_PREPARAR: return 2;
    case AJ_FONTE_TEXTO: case AJ_SELOS_CORES: case AJ_SELOS_PACOTE: case AJ_SELOS_PACOTE_ADD: case AJ_SELOS_PACOTE_REM: case AJ_FONTE_ORDEM_ADDON: return 3;
    case AJ_P2P_LIGADO: case AJ_P2P_URL: case AJ_P2P_TESTAR: case AJ_P2P_LIMITE: return 4;
    case AJ_SEEKR_LIGADO: case AJ_SEEKR_CHAVE: case AJ_SEEKR_TESTAR: case AJ_SEEKR_FITA: return 5;
    case AJ_SEEKR_AJUSTE: return 6;
    case AJ_FONTE_ESCOPO: case AJ_FONTE_ADDONS_PERM: case AJ_FONTE_PLUGINS_PERM: case AJ_FONTE_OUTROS: case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO: case AJ_FONTE_ORDEM: case AJ_FONTE_ORDEM_USO: return 7;
    case AJ_LEG_LINGUA: case AJ_AUD_LINGUA: case AJ_IDIOMA: case AJ_TMDB_IDIOMA: case AJ_LEG_SYNC_AUDIO: case AJ_LEG_FORCADA: case AJ_LEG_SYNC_AUTO: case AJ_LEG_MOSTRAR: return 8;
    case AJ_LEG_LINGUA2: case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA: return 9;
    case AJ_ADDONS: case AJ_ADDONS_PRINCIPAL: case AJ_PLUGINS: case AJ_BUSCA_CINEMETA: case AJ_BUSCA_NUVIO: case AJ_BUSCA_ORIGEM: return 10;
    case AJ_DEBRID_AD: case AJ_DEBRID_AD_TESTAR: case AJ_DEBRID_RD: case AJ_DEBRID_TB: case AJ_DEBRID_PM: return 11;
    case AJ_JF_LIGADO: case AJ_JF_SERVIDOR: case AJ_JF_ENTRAR: case AJ_JF_SAIR: case AJ_EM_SERVIDOR: case AJ_EM_ENTRAR: case AJ_EM_SAIR: case AJ_PX_ENTRAR: case AJ_PX_SERVIDOR: case AJ_PX_SAIR: return 12;
    case AJ_STALKER_PORTAL: case AJ_STALKER_MAC: case AJ_STALKER_LIMPAR: case AJ_XTREAM_SERVIDOR: case AJ_XTREAM_USUARIO: case AJ_XTREAM_SENHA: case AJ_XTREAM_LIMPAR: case AJ_XTREAM_CONTA: case AJ_EPG_PAIS: return 13;
    case AJ_LIVETV_RES: case AJ_LIVETV_FORMATO: case AJ_LIVETV_ESPERA: case AJ_LIVETV_DIAG: case AJ_LIVETV_MODO: case AJ_LIVETV_PROXY: return 14;
    case AJ_LANDSCAPE: case AJ_HERO_CHEIO: case AJ_HERO: case AJ_HERO_CATALOGOS: case AJ_HOME_LAYOUT: case AJ_HERO_TRANSICAO: return 15;
    case AJ_FIL_LIMITE: case AJ_FIL_ORDEM: case AJ_ROTULOS: case AJ_NOME_ADDON: case AJ_SUFIXO_TIPO: case AJ_OCULTAR_NLANC: case AJ_NOTAS_HOME: case AJ_ITENS_FILEIRA: case AJ_SELO_VISTO: case AJ_ESPACO_FILEIRAS: case AJ_ESPACO_TITULOS: return 16;
    case AJ_RAIL: case AJ_RAIL_MODERNA: case AJ_RAIL_BLUR: case AJ_MENU_EXPLORAR: case AJ_MENU_GUIA: case AJ_MENU_AGENDA: case AJ_MENU_PERFIL: case AJ_MIC_BUSCA: case AJ_MENU_TOPO: return 17;
    case AJ_PS_FUNDO: case AJ_PS_CONTINUAR: return 18;
    case AJ_CW_LIGADO: case AJ_CW_OK: case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_BLUR_PROX: case AJ_CW_FURTHEST: case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM: case AJ_CW_RETIDO_TAMBEM: return 19;
    case AJ_DET_BLUR_NAO_VISTOS: case AJ_DET_DATA_CHEIA: case AJ_DET_VEU: return 20;
    case AJ_DET_META_EXT: case AJ_TMDB_LIGADO: case AJ_TMDB_ARTE: case AJ_TMDB_BASICO: case AJ_TMDB_FICHA: case AJ_TMDB_DATAS: case AJ_TMDB_ELENCO: case AJ_TMDB_PROD: case AJ_TMDB_REDES: case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS: case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW: case AJ_DET_SO_CINEMETA: return 21;
    case AJ_MDB_LIGADO: case AJ_MDB_CHAVE: case AJ_NT_IMDB: case AJ_NT_TOMATES: case AJ_NT_AUDIENCIA: case AJ_NT_META: case AJ_NT_METAUSER: case AJ_NT_TRAKT: case AJ_NT_TMDB: case AJ_NT_LETTER: case AJ_NT_MAL: case AJ_NT_EBERT: case AJ_NT_SCORE: return 22;
    case AJ_HERO_FUNDO: case AJ_HERO_ARTE_DIF: case AJ_FANART_CHAVE: case AJ_POSTER_PROV: case AJ_POSTER_INST: case AJ_POSTER_TOKEN: case AJ_POSTER_EXTRA: case AJ_POSTER_CHAVE: case AJ_POSTER_MODELO: case AJ_POSTER_TESTAR: case AJ_ADDON_POSTER: case AJ_ADDON_FUNDO: case AJ_ADDON_LOGO: case AJ_COL_ARTE_CONTA: return 23;
    case AJ_HERO_TRAILER: case AJ_DET_TRAILER: case AJ_DET_TRAILER_AUTO: case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO: case AJ_FOCO_TRAILER: case AJ_HERO_TRAILER_ESPERA: return 24;
    case AJ_GRAD_CLASSICO: case AJ_EXPANDIR: case AJ_EXPANDIR_ATRASO: case AJ_BORDA_FOCO: return 25;
    case AJ_TRAILER_FONTE: case AJ_HERO_TRAILER_SOM: case AJ_DET_TRAILER_SOM: case AJ_LOGO_TRAILER: case AJ_TRAILER_ZOOM_TPK: return 26;
    case AJ_PROF: case AJ_PROF_BRILHO: case AJ_LARGURA_DP: case AJ_RAIO_DP: return 27;
    case AJ_ANIM: case AJ_RELOGIO: case AJ_RELOGIO_12H: return 28;
    case AJ_TEMA: case AJ_COR_LOGO: case AJ_VIDRO: case AJ_VIDRO_CONTORNO: case AJ_FUNDO: case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: return 29;
    case AJ_FONTE_UI: case AJ_TAMANHO_UI: case AJ_TAMANHO_AJUSTES: case AJ_LAYOUT_AJUSTES: return 30;
    case AJ_ICONE_APP: case AJ_LOGO_APP: case AJ_ABERTURA: return 31;
    case AJ_ESMAECER: case AJ_DESCANSO_ESTILO: case AJ_DESCANSO_FONTE: return 32;
    case AJ_INTERFACE_MODO: return 33;
    case AJ_CW_FONTE: case AJ_SALVOS_DEST: case AJ_TRAKT: case AJ_SIMKL: case AJ_DISCORD: case AJ_HIST_CONTA: case AJ_SOCIAL: case AJ_CW_PROXIMO: case AJ_DIARIO_NUVEM: return 34;
    case AJ_PERFIL_ATIVO: case AJ_SYNC: case AJ_SAIR: case AJ_PERFIL_PESQ: case AJ_PERFIL_EDITAR: case AJ_PERFIL_INFANTIL: return 35;
    case AJ_ENQUETES: return 36;
    case AJ_CONQ_LIGADO: case AJ_CONQ_AVISO: case AJ_CONQ_DICAS: return 37;
    case AJ_NAV_RAPIDA: case AJ_QUALIDADE_IMG: case AJ_RESOLUCAO: case AJ_ESPACO: case AJ_DIAGNOSTICO: case AJ_VELOCIDADE: case AJ_GPU_EFEITOS: case AJ_MEDIDOR: return 38;
    case AJ_VERSAO_I: case AJ_ATUALIZAR: return 39;
    case AJ_ENVIAR_LOG: case AJ_ENVIO_AUTO: case AJ_VER_REGISTRO: return 40;
    case AJ_TEX_MB: return 41;
    case AJ_GUIA: case AJ_NOVIDADES20: case AJ_APOIAR: case AJ_AVISO_LEGAL: return 42;
    case AJ_CACHE_SEEK: case AJ_MANTER_VIDEO: case AJ_DTS_AC3: case AJ_DV_MKV: case AJ_BUFFER: return 43;
    default: return -1;
  }
}
