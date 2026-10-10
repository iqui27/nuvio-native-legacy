// Glifos complementares: SVG Lucide embarcado, rasterizado no pacote para OpenGL.
static const char *ajVisualIcone(int op) {
  switch (op) {
    case AJ_HOME_LAYOUT: return "aj_layout-dashboard";
    case AJ_LANDSCAPE: return "aj_layout-dashboard";
    case AJ_HERO: return "aj_layout-dashboard";
    case AJ_HERO_CHEIO: return "aj_layout-dashboard";
    case AJ_HERO_CATALOGOS: return "aj_layout-dashboard";
    case AJ_HERO_TRANSICAO: return "aj_layout-dashboard";
    case AJ_FIL_ORDEM: return "aj_rows-3";
    case AJ_FIL_LIMITE: return "aj_rows-3";
    case AJ_ITENS_FILEIRA: return "aj_rows-3";
    case AJ_ESPACO_FILEIRAS: return "aj_rows-3";
    case AJ_ESPACO_TITULOS: return "aj_rows-3";
    case AJ_OCULTAR_NLANC: return "aj_rows-3";
    case AJ_NOTAS_HOME: return "aj_rows-3";
    case AJ_ROTULOS: return "aj_rows-3";
    case AJ_NOME_ADDON: return "aj_rows-3";
    case AJ_SUFIXO_TIPO: return "aj_rows-3";
    case AJ_SELO_VISTO: return "aj_rows-3";
    case AJ_CW_LIGADO: return "aj_rotate-ccw-clock";
    case AJ_CW_OK: return "aj_rotate-ccw-clock";
    case AJ_CW_ESTILO: return "aj_rotate-ccw-clock";
    case AJ_CW_THUMB: return "aj_rotate-ccw-clock";
    case AJ_CW_BLUR_PROX: return "aj_rotate-ccw-clock";
    case AJ_CW_ORDEM: return "aj_rotate-ccw-clock";
    case AJ_CW_NAO_EXIBIDOS: return "aj_rotate-ccw-clock";
    case AJ_CW_FURTHEST: return "aj_rotate-ccw-clock";
    case AJ_CW_RETIDO_TAMBEM: return "aj_rotate-ccw-clock";
    case AJ_RAIL_MODERNA: return "aj_panel-left";
    case AJ_RAIL: return "aj_panel-left";
    case AJ_MENU_EXPLORAR: return "aj_panel-left";
    case AJ_MENU_GUIA: return "aj_panel-left";
    case AJ_MENU_AGENDA: return "aj_panel-left";
    case AJ_MENU_PERFIL: return "aj_panel-left";
    case AJ_RAIL_BLUR: return "aj_panel-left";
    case AJ_DESCOBRIR: return "aj_panel-left";
    case AJ_PS_FUNDO: return "aj_users";
    case AJ_PS_CONTINUAR: return "aj_users";
    case AJ_EXPANDIR: return "aj_maximize-2";
    case AJ_EXPANDIR_ATRASO: return "aj_clock";
    case AJ_BORDA_FOCO: return "aj_maximize-2";
    case AJ_GRAD_CLASSICO: return "aj_maximize-2";
    case AJ_LARGURA_DP: return "aj_images";
    case AJ_RAIO_DP: return "aj_images";
    case AJ_PROF: return "aj_images";
    case AJ_PROF_BORDA: return "aj_images";
    case AJ_PROF_BRILHO: return "aj_images";
    case AJ_PROF_COBERTURA: return "aj_images";
    case AJ_PROF_POSTERS: return "aj_images";
    case AJ_PROF_CW: return "aj_images";
    case AJ_PROF_EPS: return "aj_images";
    case AJ_PROF_ELENCO: return "aj_images";
    case AJ_PROF_TRAILERS: return "aj_images";
    case AJ_HERO_FUNDO: return "aj_image";
    case AJ_HERO_ARTE_DIF: return "aj_image";
    case AJ_POSTER_PROV: return "aj_image";
    case AJ_ADDON_POSTER: return "aj_image";
    case AJ_ADDON_FUNDO: return "aj_image";
    case AJ_ADDON_LOGO: return "aj_image";
    case AJ_COL_ARTE_CONTA: return "aj_image";
    case AJ_POSTER_INST: return "aj_image";
    case AJ_POSTER_TOKEN: return "aj_image";
    case AJ_POSTER_EXTRA: return "aj_image";
    case AJ_POSTER_CHAVE: return "aj_image";
    case AJ_POSTER_MODELO: return "aj_image";
    case AJ_POSTER_TESTAR: return "aj_image";
    case AJ_HERO_TRAILER: return "aj_clapperboard";
    case AJ_FOCO_TRAILER: return "aj_clapperboard";
    case AJ_DET_TRAILER_AUTO: return "aj_clapperboard";
    case AJ_TRAILER_ZOOM_TPK: return "aj_clapperboard";
    case AJ_DET_TRAILER: return "aj_clapperboard";
    case AJ_HERO_TRAILER_SOM: return "aj_audio-lines";
    case AJ_DET_TRAILER_SOM: return "aj_audio-lines";
    case AJ_TRAILER_FONTE: return "aj_server";
    case AJ_HERO_TRAILER_ESPERA: return "aj_clock";
    case AJ_TRAILER_QUAL: return "aj_monitor-play";
    case AJ_TRAILER_ASPECTO: return "aj_maximize-2";
    case AJ_DET_VEU: return "aj_file-text";
    case AJ_DET_DATA_CHEIA: return "aj_file-text";
    case AJ_DET_BLUR_NAO_VISTOS: return "aj_file-text";
    case AJ_NT_IMDB: return "aj_star";
    case AJ_NT_TOMATES: return "aj_star";
    case AJ_NT_AUDIENCIA: return "aj_star";
    case AJ_NT_META: return "aj_star";
    case AJ_NT_METAUSER: return "aj_star";
    case AJ_NT_TRAKT: return "aj_star";
    case AJ_NT_TMDB: return "aj_star";
    case AJ_NT_LETTER: return "aj_star";
    case AJ_NT_MAL: return "aj_star";
    case AJ_NT_EBERT: return "aj_star";
    case AJ_NT_SCORE: return "aj_star";
    case AJ_MDB_LIGADO: return "aj_star";
    case AJ_MDB_TRAKT: return "aj_star";
    case AJ_MDB_IMDB: return "aj_star";
    case AJ_MDB_TMDB: return "aj_star";
    case AJ_MDB_LETTER: return "aj_star";
    case AJ_MDB_TOMATES: return "aj_star";
    case AJ_MDB_AUDIENCIA: return "aj_star";
    case AJ_MDB_META: return "aj_star";
    case AJ_MDB_MAL: return "aj_star";
    case AJ_TMDB_LIGADO: return "aj_database";
    case AJ_TMDB_ARTE: return "aj_database";
    case AJ_TMDB_BASICO: return "aj_database";
    case AJ_TMDB_FICHA: return "aj_database";
    case AJ_TMDB_DATAS: return "aj_database";
    case AJ_TMDB_ELENCO: return "aj_database";
    case AJ_TMDB_PROD: return "aj_database";
    case AJ_TMDB_REDES: return "aj_database";
    case AJ_TMDB_EPS: return "aj_database";
    case AJ_TMDB_TRAILERS: return "aj_database";
    case AJ_TMDB_MAIS: return "aj_database";
    case AJ_TMDB_COL: return "aj_database";
    case AJ_TMDB_CW: return "aj_database";
    case AJ_DET_META_EXT: return "aj_database";
    case AJ_DET_SO_CINEMETA: return "aj_database";
    case AJ_FONTE_MANUAL: return "aj_list-video";
    case AJ_FONTE_AUTO: return "aj_list-video";
    case AJ_FONTE_REPOR: return "aj_list-video";
    case AJ_FONTE_PREPARAR: return "aj_download";
    case AJ_FONTE_CONFERIR_VARIAS: return "aj_layers";
    case AJ_FONTE_AQUECER: return "aj_wifi";
    case AJ_FONTE_TOCAR_CONFERINDO: return "aj_zap";
    case AJ_FONTE_PRAZO: return "aj_list-video";
    case AJ_FONTE_PRIORIDADE: case AJ_FONTE_HDR: return "aj_list-video";
    case AJ_FONTE_TEXTO: case AJ_FONTE_ORDEM_ADDON: return "aj_list-video";
    case AJ_FONTE_ESCOPO: case AJ_FONTE_OUTROS: return "aj_list-video";
    case AJ_FONTE_ADDONS_PERM: return "aj_puzzle";
    case AJ_FONTE_PLUGINS_PERM: return "aj_plug";
    case AJ_FONTE_ORDEM: return "aj_arrow-up-down";
    case AJ_FONTE_ORDEM_USO: return "aj_list-video";
    case AJ_FONTE_REGEX: case AJ_FONTE_REGEX_PADRAO: case AJ_FONTE_REGEX_MODELO: return "aj_search";
    case AJ_SELOS_CORES: return "aj_list-video";
    case AJ_SELOS_PACOTE: return "aj_list-video";
    case AJ_SELOS_PACOTE_ADD: return "aj_list-video";
    case AJ_SELOS_PACOTE_REM: return "aj_list-video";
    case AJ_QUALIDADE: return "aj_monitor-play";
    case AJ_DV: return "aj_monitor-play";
    case AJ_DV_MKV: return "aj_monitor-play";
    case AJ_ATMOS: return "aj_monitor-play";
    case AJ_PAUSA_OVERLAY: return "aj_circle-play";
    case AJ_PLR_CLASSIF: return "aj_shield";
    case AJ_REACAO_CREDITOS: return "aj_circle-play";
    case AJ_CW_CONCLUIDO: return "aj_circle-play";
    case AJ_SAIDA_PLAYER: return "aj_circle-play";
    case AJ_SEEKR_LIGADO: return "aj_film";
    case AJ_SEEKR_CHAVE: return "aj_film";
    case AJ_SEEKR_TESTAR: return "aj_film";
    case AJ_SEEKR_FITA: return "aj_film";
    case AJ_SEEKR_AJUSTE: return "aj_film";
    case AJ_IDIOMA: return "aj_languages";
    case AJ_AUD_LINGUA: return "aj_audio-lines";
    case AJ_LEG_LINGUA: case AJ_LEG_LINGUA2: case AJ_LEG_FORCADA: return "aj_captions";
    case AJ_CACHE_SEEK: return "aj_database";
    case AJ_TAM_MAX: case AJ_TAM_MIN: return "aj_database";
    case AJ_TMDB_IDIOMA: return "aj_file-text";
    case AJ_TEMA: return "aj_palette";
    case AJ_FUNDO: return "aj_images";
    case AJ_COR_LOGO: return "aj_palette";
    case AJ_VIDRO: return "aj_layers";
    case AJ_FONTE_UI: return "aj_type";
    case AJ_TAMANHO_UI: return "aj_maximize-2";
    case AJ_TAMANHO_AJUSTES: return "aj_sliders-horizontal";
    case AJ_LAYOUT_AJUSTES: return "aj_layout-list";
    case AJ_AVANCADAS: return "aj_eye";
    case AJ_LOGO_TRAILER: return "aj_eye-off";
    case AJ_LEG_SYNC_AUTO: case AJ_LEG_SYNC_AUDIO: return "aj_audio-lines";
    case AJ_LEG2_POS: case AJ_LEG2_TAMANHO: case AJ_LEG2_COR: case AJ_LEG2_FUNDO: case AJ_LEG2_BORDA: return "aj_captions";
    case AJ_ICONE_APP: return "aj_image-plus";
    case AJ_LOGO_APP: return "aj_image";
    case AJ_ABERTURA: return "aj_clapperboard";
    case AJ_DISCORD: return "aj_message-square";
    case AJ_ANIM: return "aj_activity";
    case AJ_ENQUETES: return "aj_bell";
    case AJ_BUSCA_CINEMETA: return "aj_list-x";
    case AJ_BUSCA_NUVIO: return "aj_arrow-down";
    case AJ_BUSCA_ORIGEM: return "aj_tag";
    case AJ_ESMAECER: return "aj_moon";
    case AJ_DESCANSO_ESTILO: return "aj_image";
    case AJ_DESCANSO_FONTE: return "aj_film";
    case AJ_PROPORCAO_PADRAO: return "aj_maximize-2";
    case AJ_DTS_AC3: return "aj_audio-lines";
    case AJ_BRILHO_PLAYER: return "aj_sun-dim";
    case AJ_MANTER_VIDEO: return "aj_memory-stick";
    case AJ_RELOGIO: case AJ_RELOGIO_12H: return "aj_clock";
    case AJ_VIDRO_CONTORNO: return "aj_layers";
    case AJ_VIDRO_OPAC: return "aj_layers";
    case AJ_VIDRO_FOSCO: return "aj_layers";
    case AJ_XTREAM_SERVIDOR: return "aj_server";
    case AJ_XTREAM_USUARIO: return "aj_server";
    case AJ_XTREAM_SENHA: return "aj_server";
    case AJ_XTREAM_CONTA: return "aj_server";
    case AJ_XTREAM_LIMPAR: return "aj_server";
    case AJ_STALKER_PORTAL: return "aj_server";
    case AJ_STALKER_MAC: return "aj_server";
    case AJ_STALKER_LIMPAR: return "aj_server";
    case AJ_EPG_PAIS: return "aj_server";
    case AJ_LIVETV_DIAG: return "aj_stethoscope";
    case AJ_LIVETV_RES: return "aj_stethoscope";
    case AJ_LIVETV_ESPERA: return "aj_stethoscope";
    case AJ_LIVETV_FORMATO: return "aj_stethoscope";
    case AJ_LIVETV_MODO: return "aj_stethoscope";
    case AJ_LIVETV_PROXY: return "aj_stethoscope";
    case AJ_PERFIL_ATIVO: return "aj_user-round";
    case AJ_SYNC: return "aj_cloud";
    case AJ_ADDONS: return "aj_user-round";
    case AJ_PLUGINS: return "aj_puzzle";
    case AJ_ADDONS_PRINCIPAL: return "aj_user-round";
    case AJ_PERFIL_PESQ: return "aj_user-round";
    case AJ_PERFIL_EDITAR: return "aj_user-round";
    case AJ_SAIR: return "aj_log-out";
    case AJ_TRAKT: return "aj_plug";
    case AJ_SIMKL: return "aj_plug";
    case AJ_SALVOS_DEST: return "aj_plug";
    case AJ_CW_FONTE: return "aj_plug";
    case AJ_CW_PROXIMO: return "aj_plug";
    case AJ_HIST_CONTA: return "aj_cloud";
    case AJ_SOCIAL: return "aj_user-round";
    case AJ_DEBRID_RD: return "aj_key-round";
    case AJ_DEBRID_TB: return "aj_key-round";
    case AJ_DEBRID_PM: return "aj_key-round";
    case AJ_DEBRID_AD: return "aj_key-round";
    case AJ_DEBRID_AD_TESTAR: return "aj_key-round";
    case AJ_MDB_CHAVE: return "aj_key-round";
    case AJ_FANART_CHAVE: return "aj_key-round";
    case AJ_P2P_LIGADO: return "aj_user-round";
    case AJ_P2P_URL: return "aj_user-round";
    case AJ_P2P_TESTAR: return "aj_user-round";
    case AJ_P2P_LIMITE: return "aj_database";
    case AJ_JF_LIGADO: return "aj_server";
    case AJ_JF_SERVIDOR: return "aj_server";
    case AJ_JF_ENTRAR: return "aj_log-in";
    case AJ_JF_SAIR: return "aj_log-out";
    case AJ_EM_SERVIDOR: case AJ_PX_SERVIDOR: return "aj_server";
    case AJ_EM_ENTRAR: case AJ_PX_ENTRAR: return "aj_log-in";
    case AJ_EM_SAIR: case AJ_PX_SAIR: return "aj_log-out";
    case AJ_RESOLUCAO: return "aj_monitor-play";
    case AJ_QUALIDADE_IMG: return "aj_images";
    case AJ_NAV_RAPIDA: return "aj_zap";
    case AJ_MEDIDOR: return "aj_activity";
    case AJ_DIAGNOSTICO: return "aj_stethoscope";
    case AJ_VELOCIDADE: return "aj_gauge";
    case AJ_ESPACO: return "aj_database";
    case AJ_TEX_MB: return "aj_memory-stick";
    case AJ_GUIA: return "aj_book-open";
    case AJ_NOVIDADES20: return "aj_sparkles";
    case AJ_APOIAR: return "aj_heart";
    case AJ_VERSAO_I: return "aj_info";
    case AJ_ATUALIZAR: return "aj_download";
    case AJ_VER_REGISTRO: return "aj_scroll-text";
    case AJ_ENVIAR_LOG: return "aj_send";
    case AJ_ENVIO_AUTO: return "aj_scroll-text";
#if defined(NV_TPK) || defined(NV_ANDROID)
    case AJ_GPU_EFEITOS: return "aj_zap";
#endif
    default: return "aj_settings-2";
  }
}

// A IMAGEM DE CADA CATEGORIA (indice e coluna da arte da camada 2). A arte real
// (catalogo da conta, ou a amostra embarcada antes dele) ocupa o quadro 16:9
// inteiro, e por cima vem a mini-UI que a categoria controla, no material das
// ilhas, num tamanho que se le. Coordenadas num quadro de 640x360.
// A ordem dos `case` e a ordem dos SEC em ajustes_ux_tela.inc (AJS_*): a versao
// anterior tinha Cartazes/Trailers/Titulo/Reproducao/Idiomas/Aparencia
// trocados, e Reproducao mostrava os chips de idioma.
#include "perfis.h"
// 2.0.3: a ordem da tarefa (Assistir, Telas, Conta e sistema). AJS_TRAILERS nao
// e mais categoria (Cartazes e trailers e uma so): fica depois de AJS_N como a
// CENA das opcoes de trailer dentro de Cartazes e trailers.
enum { AJS_REPRODUCAO = 0, AJS_IDIOMAS, AJS_FONTES, AJS_TVAOVIVO,
       AJS_HOME, AJS_APARENCIA, AJS_TITULO, AJS_CARTAZES,
       AJS_CONTAS, AJS_DESEMPENHO, AJS_SOBRE, AJS_N, AJS_TRAILERS = AJS_N };
static float ajPreviaHome(float x, float y, float w);
static float ajPreviaHomeArte(float x, float y, float w, int arte);
static const char *ajTituloN(int n);
static const char *ajMetaTitulo(int n);
static void ajTextoMini(const char *s, float px, int forte, float x, float y, int r, int g, int b, float a);
static TxtEstilo ajEstiloPx(float px, int forte);
static TxtEstilo ajEstiloPxS(float px, int forte) { return ajEstiloPx(px < 6.0f ? 6.0f : px, forte); }
// A amostra de arte de cada categoria e de cada SUBMENU dela (o indice de
// ajArte). Linha = categoria (AJS_*), coluna = o bloco (ROT) na ordem de
// ajustes_ux_tela.inc; a coluna 0 e a imagem da categoria, e vale tambem para
// o primeiro bloco (entrar na categoria nao troca a imagem). Nenhum indice se
// repete: sao 36 das 40 amostras de deploy/app/art (sobram 28, 29, 34 e 36).
// -1 = a categoria nao tem esse bloco.
#define AJ_ARTE_BLOCOS 9
// Os blocos de "Mais opcoes" (depois do MAIS() da categoria) repetem a arte do
// bloco basico de mesmo nome: e a mesma parte do app, so que tecnica.
static const signed char AJ_ARTE_SEC[AJS_N + 1][AJ_ARTE_BLOCOS] = {
  { 18,  7, 22, 32,  7, 18, 19, -1, -1 },   // Reproducao: Imagem e som, Escolha da fonte, Durante e depois, Miniatura; Mais: fonte, som e video, P2P
  { 21, 24, 21, -1, -1, -1, -1, -1, -1 },   // Idiomas e legendas: Idiomas, Segunda legenda; Mais: idiomas
  {  5, 37, 10, -1, -1, -1, -1, -1, -1 },   // Fontes e addons: Addons, Debrid, Servidores
  {  2, 14, 14, -1, -1, -1, -1, -1, -1 },   // TV ao vivo: Provedor, Se o canal nao abre; Mais
  { 12,  0, 20,  1, 31, 12,  0, 20,  1 },   // Tela inicial: Estilo, Fileiras, Continuar, Barra lateral, Escolha de perfil; Mais
  {  9, 39, 38,  6, 11,  9, -1, -1, -1 },   // Aparencia: Cores e vidro, Logo e abertura, Texto e tamanho, Movimento e relogio, Tela de descanso; Mais
  {  3,  4, 17,  4, 17, -1, -1, -1, -1 },   // Pagina do titulo: O que aparece, Notas, Dados do titulo; Mais: notas, dados
  { 13, 35, 27, 15, 16, 13, 35, 27, 15 },   // Cartazes e trailers: Cartaz em foco, Forma, Arte, Trailers, Som e imagem; Mais
  { 23, 33,  8, -1, -1, -1, -1, -1, -1 },   // Conta e servicos: Perfil, Trakt e Simkl, Notificacoes
  { 26, -1, -1, -1, -1, -1, -1, -1, -1 },   // Esta TV (Mais: Memoria)
  { 30, 25, -1, -1, -1, -1, -1, -1, -1 },   // Sobre e ajuda: (guia e) App, Registro
  { 15, 16, -1, -1, -1, -1, -1, -1, -1 },   // (cena) Trailers: Onde toca, Som e imagem
};
static int ajCenaArteN(int sec) {
  return sec >= 0 && sec <= AJS_N ? AJ_ARTE_SEC[sec][0] : 12;
}
// A arte do submenu (bloco) da opcao. As opcoes antes do primeiro bloco e as
// do primeiro bloco ficam com a imagem da categoria.
static int ajCenaArteOp(int op) {
  static int cOp = -1, cN = 12;
  int i, j, sec, g = -1, n;
  if (op < 0) return 12;
  if (op == cOp) return cN;
  for (i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) break;
  if (i >= AJ_N_TELA) return 12;
  sec = secDoItem[i];
  for (j = secIni[sec] + 1; j < i; j++) if (TELA[j].tipo == IT_ROT) g++;
  if (g < 0) g = 0;
  if (g >= AJ_ARTE_BLOCOS) g = AJ_ARTE_BLOCOS - 1;
  n = sec >= 0 && sec < AJS_N ? AJ_ARTE_SEC[sec][g] : 12;
  if (n < 0) n = ajCenaArteN(sec);
  cOp = op; cN = n;
  return n;
}
// Arte cobrindo o quadro (ou o cinza das ilhas se ela ainda nao chegou) e o
// escurecimento por cima.
static void ajCenaArte(const char *arte, GfxRect q, float rr, float escuro) {
  if (!ajImg(arte, q, rr, 1)) gfx_cor(q, rr / q.h, 0.10f, 0.105f, 0.12f, 1);
  if (escuro > 0.003f) gfx_cor(q, rr / q.h, 0.031f, 0.031f, 0.039f, escuro);
}
// Peca de ilha sobre a foto: o miolo escuro do vidro (ou o solido #15161A).
static void ajCenaVidro(GfxRect r, float raioPx, float a) {
  if (ajVid()) gfx_cor(r, raioPx / r.h, 0.055f, 0.059f, 0.071f, 0.74f * a);
  else gfx_cor(r, raioPx / r.h, 0.082f, 0.086f, 0.102f, 0.97f * a);
}
// O logo da marca de uma fonte de nota (deploy/app/art/marcas) e a nota de
// amostra. 0 = a fonte nao tem logo (fica fora da previa).
static const char *ajNotaMarca(int op, const char **nota) {
  switch (op) {
    case AJ_NT_IMDB: case AJ_MDB_IMDB: *nota = "8.1"; return "imdb";
    case AJ_NT_TOMATES: case AJ_MDB_TOMATES: *nota = "94%"; return "tomatoes_fresh";
    case AJ_NT_AUDIENCIA: case AJ_MDB_AUDIENCIA: *nota = "89%"; return "audience";
    case AJ_NT_META: case AJ_MDB_META: *nota = "76"; return "metacritic";
    case AJ_NT_METAUSER: *nota = "8.0"; return "metacritic";
    case AJ_NT_TRAKT: case AJ_MDB_TRAKT: *nota = "82%"; return "trakt";
    case AJ_NT_TMDB: case AJ_MDB_TMDB: *nota = "7.9"; return "tmdb";
    case AJ_NT_LETTER: case AJ_MDB_LETTER: *nota = "4.1"; return "letterboxd";
    default: return NULL;
  }
}
// A linha de notas da pagina do titulo: as fontes do grupo do `op` (as notas do
// titulo ou as do MDBList), ligadas acesas e desligadas a 30%; a do `op` em
// foco ganha o ponto do acento.
static void ajCenaNotas(int op, float x, float y, float s, float ar, float ag, float ab) {
  static const int NT[] = { AJ_NT_IMDB, AJ_NT_TOMATES, AJ_NT_AUDIENCIA, AJ_NT_META, AJ_NT_TRAKT, AJ_NT_TMDB, AJ_NT_LETTER };
  static const int MDB[] = { AJ_MDB_IMDB, AJ_MDB_TOMATES, AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_TRAKT, AJ_MDB_TMDB, AJ_MDB_LETTER };
  const int *g = (op >= AJ_MDB_TRAKT && op <= AJ_MDB_MAL) || op == AJ_MDB_LIGADO ? MDB : NT;
  int i, n = 7;
  float cx = x;
  for (i = 0; i < n; i++) {
    const char *nota = "", *m = ajNotaMarca(g[i], &nota);
    char cam[1200];
    int on = lig(g[i]) && (g != MDB || lig(AJ_MDB_LIGADO));
    float al = op < 0 ? 1.0f : on ? 1.0f : 0.3f, lw;
    TxtEstilo e = ajEstiloPxS(13 * s, 1);
    TxtLinha l = { 0 };
    if (!m) continue;
    if (op < 0 && !on && i > 2) continue;   // a categoria: so as ligadas (e as tres primeiras)
    snprintf(cam, sizeof cam, "%s/marcas/%s.png", uxDirArte, m);
    if ((int)e >= 0) l = txt_linha(e, nota, 243, 242, 239, 255);
    lw = 22 * s + 6 * s + l.w + 20 * s;
    ajCenaVidro((GfxRect){ cx, y, lw, 30 * s }, 15 * s, al);
    ajLogo(cam, cx + 10 * s, y + 6 * s, 22 * s, 18 * s, al);
    if (l.w) txt_desenhar_alpha(l, cx + 10 * s + 22 * s + 6 * s, y + (30 * s - l.h) * 0.5f, al);
    if (g[i] == op) gfx_cor((GfxRect){ cx + lw * 0.5f - 3 * s, y + 34 * s, 6 * s, 6 * s }, 0.5f, ar, ag, ab, 1);
    cx += lw + 8 * s;
  }
}
// O tempo de agora (pilula do relogio).
static void ajCenaHora(char *hora, size_t n) {
  time_t t = time(NULL);
  struct tm lt;
  if (localtime_r(&t, &lt)) hora_tela(hora, n, &lt); else snprintf(hora, n, "--:--");
}

// A IMAGEM DE CADA SUBMENU (ajustes_ux_secoes.inc): a cena do que o bloco
// controla, com a peca da opcao em foco marcada. -1 = a opcao fica com a
// imagem da categoria.
static int ajCenaSecao(int op);
static float ajSecaoDesenhar(int cena, int op, float x, float y, float w);
static int ajcSecA = -1;   // a arte da categoria para uma cena de bloco sem opcao
static float ajFontesResumo(int A, float x, float y, float w);   // ajustes_ux_secoes.inc
static float ajCenaGerada(int sec, int op, float x, float y, float w);   // ajustes_ux_cenas.inc
static float ajVisualCena(int sec, int op, float x, float y, float w) {
  // 2.0.2 (dono: "essas fotos reais dos settings atrapalham mais do que
  // ajudam"): toda cena dos Ajustes e o desenho gerado. O desenho com as
  // artes de amostra abaixo ficou so como referencia do que cada cena mostra.
  if (sec >= 0 || op >= 0) return ajCenaGerada(sec, op, x, y, w);
  float s = w / 640.0f, h = w * 9.0f / 16.0f, rr = 22.0f, ar, ag, ab;
  GfxRect q = { x, y, w, h };
  int i, j;
  // A arte do quadro: a do submenu da opcao em foco, ou a da categoria.
  int A = op >= 0 ? ajCenaArteOp(op) : ajCenaArteN(sec);
  if (op >= 0) { int c = ajCenaSecao(op); if (c >= 0) return ajSecaoDesenhar(c, op, x, y, w); }
  // Fontes e addons: o resumo da categoria e a cena dos addons, na arte dela.
  if (sec == AJS_FONTES) return ajFontesResumo(A, x, y, w);
#define PX(a) (x + (a) * s)
#define PY(b) (y + (b) * s)
#define RR(a,b,c,d) (GfxRect){ PX(a), PY(b), (c) * s, (d) * s }
#define VID(a,b,c,d,r) ajCenaVidro(RR(a,b,c,d), (r) * s, 1)
#define BAR(a,b,c,al) gfx_cor(RR(a,b,c,7), 0.5f, 0.953f, 0.949f, 0.937f, al)
#define ACC(a,b,c,d,al) gfx_cor(RR(a,b,c,d), 0.5f, ar, ag, ab, al)
#define ICON(n,a,b,d,al) ajIcone(n, PX(a), PY(b), (d) * s, al)
#define TXT(t,px,f,a,b,al) ajTextoMini(t, (px) * s, f, PX(a), PY(b), 243, 242, 239, al)
  ajustes_acento(&ar, &ag, &ab);
  if (sec == AJS_HOME) {   // a Home dela, em miniatura (o destaque muda com o submenu)
    if (op < 0 || A == ajCenaArteN(AJS_HOME)) return ajPreviaHome(x, y, w);
    return ajPreviaHomeArte(x, y, w, A);
  }
  gfx_recorte(x, y, w, h);
  switch (sec) {
    case AJS_TITULO: {   // pagina do titulo: arte, logo, meta, notas e botoes
      float veu = 0.6f + 0.4f * valor[AJ_DET_VEU] / 100.0f;
      int ti = ajTinta();
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0);
      gfx_veu_css((GfxRect){ x, y, w * 0.72f, h }, 2, 0, 1.0f, 0.9f * veu);
      gfx_veu_css((GfxRect){ x, y + h * 0.45f, w, h * 0.55f }, 0, 0, 1.0f, 0.6f * veu);
      if (ajLogo(ajArte(AJA_LOGO, A), PX(36), PY(52), 260 * s, 84 * s, 1) <= 0)
        TXT(ajTituloN(A), 30, 1, 36, 84, 1);
      TXT(ajMetaTitulo(A), 13, 0, 36, 150, 0.78f);
      ajCenaNotas(op >= AJ_NT_IMDB && op <= AJ_NT_SCORE ? op : (op >= AJ_MDB_LIGADO && op <= AJ_MDB_MAL) ? op : -1,
                  PX(36), PY(186), s, ar, ag, ab);
      { TxtEstilo e = ajEstiloPxS(15 * s, 1);
        TxtLinha l = { 0 };
        float bw;
        if ((int)e >= 0) l = txt_linha(e, "Reproduzir", ti, ti, ti, 255);
        bw = 44 * s + l.w + 22 * s;
        gfx_cor(RR(36, 240, bw / s, 44), 0.5f, ar, ag, ab, 1);
        gfx_icone((GfxRect){ PX(54), PY(254), 16 * s, 16 * s }, "aj_play", ti / 255.0f, ti / 255.0f, ti / 255.0f, 1);
        if (l.w) txt_desenhar(l, PX(36) + 44 * s, PY(240) + (44 * s - l.h) * 0.5f);
        { float bx = 36 + bw / s + 10;
          if (lig(AJ_DET_TRAILER)) {
            TxtLinha t = (int)e >= 0 ? txt_linha(e, "Trailer", 243, 242, 239, 255) : (TxtLinha){ 0 };
            float tw = 22 + t.w / s + 22;
            VID(bx, 240, tw, 44, 22);
            if (t.w) txt_desenhar(t, PX(bx + 22), PY(240) + (44 * s - t.h) * 0.5f);
            bx += tw + 10;
          }
          VID(bx, 240, 44, 44, 22); ICON("aj_bookmark", bx + 12, 252, 20, 0.9f); } }
      break; }
    case AJS_CARTAZES: {   // uma fileira de cartazes com o cartaz em foco
      int P[5] = { 12, 13, 15, 2, 21 };
      int exp = ajustes_expandir_poster(), borda = ajustes_borda_foco(), prof = ajustes_profundidade();
      float pw = 100, ph = 150, gap = 18, by = 150;
      P[1] = A;   // o cartaz em foco e o do submenu
      ajCenaArte(ajArte(AJA_FUNDO, P[1]), q, rr, 0.25f);
      gfx_veu_css((GfxRect){ x, y, w * 0.7f, h }, 2, 0, 1.0f, 0.8f);
      gfx_veu_css((GfxRect){ x, y + h * 0.35f, w, h * 0.65f }, 0, 0, 1.0f, 0.85f);
      if (ajLogo(ajArte(AJA_LOGO, P[1]), PX(36), PY(28), 180 * s, 54 * s, 1) <= 0) TXT(ajTituloN(P[1]), 22, 1, 36, 40, 1);
      TXT(ajMetaTitulo(P[1]), 12, 0, 36, 96, 0.7f);
      for (i = 0; i < 5; i++) {
        float bx = 36 + i * (pw + gap);
        GfxRect r = RR(bx, by, pw, ph);
        float raio = ajustes_raio_poster_px() * r.w / 256.0f;
        if (i == 1) continue;
        ajImg(ajArte(AJA_POSTER, P[i]), r, raio, 0.72f);
      }
      { float k = exp ? 1.08f : 1.0f, fw = pw * k, fh = ph * k, fx = 36 + pw + gap - (fw - pw) * 0.5f, fy = by - (fh - ph) * 0.5f;
        GfxRect r = RR(fx, fy, fw, fh);
        float raio = ajustes_raio_poster_px() * r.w / 256.0f;
        if (exp) gfx_rect((GfxRect){ r.x - 16 * s, r.y + 6 * s, r.w + 32 * s, r.h + 34 * s }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.6f);
        ajImg(ajArte(AJA_POSTER, P[1]), r, raio, 1);
        if (prof) gfx_brilho_topo(r, raio / r.h, 0.3f, 1, 1, 1, 0.16f);
        if (borda) gfx_anel(r, raio / r.h, 3 * s, ar, ag, ab, 1); }
      break; }
    case AJS_TRAILERS: {   // o trailer tocando no destaque
      int semLogo = lig(AJ_LOGO_TRAILER);
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0);
      gfx_veu_css((GfxRect){ x, y + h * 0.4f, w, h * 0.6f }, 0, 0, 1.0f, 0.8f);
      { TxtEstilo e = ajEstiloPxS(13 * s, 1);
        TxtLinha t = (int)e >= 0 ? txt_linha(e, "Trailer", 243, 242, 239, 255) : (TxtLinha){ 0 };
        float tw = 36 + t.w / s + 16;
        VID(28, 26, tw, 32, 16);
        gfx_icone((GfxRect){ PX(40), PY(35), 14 * s, 14 * s }, "aj_play", 0.953f, 0.949f, 0.937f, 0.9f);
        if (t.w) txt_desenhar(t, PX(60), PY(26) + (32 * s - t.h) * 0.5f); }
      VID(572, 26, 40, 40, 20); ICON("aj_audio-lines", 581, 35, 22, 0.9f);
      if (!semLogo && ajLogo(ajArte(AJA_LOGO, A), PX(36), PY(214), 220 * s, 66 * s, 1) <= 0)
        TXT(ajTituloN(A), 24, 1, 36, 240, 1);
      gfx_cor(RR(36, 312, 568, 5), 0.5f, 1, 1, 1, 0.25f);
      ACC(36, 312, 238, 5, 1);
      break; }
    case AJS_APARENCIA: {   // a cor, o vidro, a fonte e o relogio sobre a arte
      char hora[12], fam[96];
      int tema = valor[AJ_TEMA];
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0.18f);
      gfx_rect(RR(26, 50, 320, 290), 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.4f);
      VID(36, 44, 300, 272, 26);
      TXT("Aa", 64, 1, 60, 62, 1);
      uxValorTexto(AJ_FONTE_UI, valor[AJ_FONTE_UI], fam, sizeof fam);
      TXT(fam, 14, 0, 62, 150, 0.6f);
      for (i = 0; i < 6; i++) {
        unsigned c = TEMA_ACENTO[1 + i].fill;
        GfxRect r = RR(60 + i * 42, 196, 30, 30);
        gfx_cor(r, 0.5f, ((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, 1);
        if (tema == 1 + i) gfx_anel_fora(r, 0.5f, 3 * s, 2.5f * s, 0.953f, 0.949f, 0.937f, 0.95f);
      }
      gfx_cor(RR(60, 258, 52, 28), 0.5f, ar, ag, ab, 1);
      gfx_cor(RR(88, 262, 20, 20), 0.5f, 1, 1, 1, 1);
      BAR(128, 268, 120, 0.55f);
      if (valor[AJ_RELOGIO] == 0) {
        ajCenaHora(hora, sizeof hora);
        VID(486, 26, 126, 46, 23);
        TXT(hora, 20, 1, 506, 35, 1);
      }
      ajImg(ajArte(AJA_POSTER, A), RR(400, 104, 156, 234), 14 * s, 1);
      VID(400, 262, 156, 76, 14);
      BAR(416, 282, 100, 0.8f); BAR(416, 300, 70, 0.45f);
      break; }
    case AJS_REPRODUCAO: {   // o player: titulo, marcas de imagem e som, barra de tempo
      static const FormatoMarca M[3] = { FMT_4K, FMT_DV, FMT_ATMOS };
      int on[3];
      float bx = 36;
      on[0] = valor[AJ_QUALIDADE] <= 1; on[1] = lig(AJ_DV); on[2] = lig(AJ_ATMOS);
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0);
      gfx_veu_css((GfxRect){ x, y + h * 0.35f, w, h * 0.65f }, 0, 0, 1.0f, 0.9f);
      TXT(ajTituloN(A), 22, 1, 36, 206, 1);
      TXT(ajMetaTitulo(A), 12, 0, 36, 236, 0.65f);
      for (i = 0; i < 3; i++) {
        float mh = 24, mw = marca_formato_largura(M[i], mh * s) / s;
        int foco = (op == AJ_QUALIDADE && i == 0) || (op == AJ_DV && i == 1) || (op == AJ_ATMOS && i == 2);
        marca_formato(M[i], PX(bx), PY(262), mh * s, 1, 1, 1, on[i] ? 0.9f : 0.25f);
        if (foco) gfx_cor(RR(bx + mw * 0.5f - 3, 290, 6, 6), 0.5f, ar, ag, ab, 1);
        bx += mw + 12;
      }
      gfx_cor(RR(36, 306, 568, 5), 0.5f, 1, 1, 1, 0.22f);
      ACC(36, 306, 216, 5, 1);
      gfx_cor(RR(245, 301.5f, 14, 14), 0.5f, 1, 1, 1, 1);
      TXT("21:40", 11, 0, 36, 320, 0.6f);
      { TxtEstilo e = ajEstiloPxS(11 * s, 0);
        if ((int)e >= 0) { TxtLinha l = txt_linha(e, "58:12", 243, 242, 239, 255); txt_desenhar_alpha(l, PX(604) - l.w, PY(320) + (11 * s * 1.2f - l.h) * 0.5f, 0.6f); } }
      break; }
    case AJS_IDIOMAS: {   // idioma da interface, audio e legenda, cada um com o seu
      static const int OP3[3] = { AJ_IDIOMA, AJ_AUD_LINGUA, AJ_LEG_LINGUA };
      static const char *const ico[3] = { "aj_languages", "aj_audio-lines", "aj_captions" };
      static const char *const cod[3] = { "PT", "EN", "ES" };
      int foco = op == AJ_IDIOMA || op == AJ_TMDB_IDIOMA ? 0 : op == AJ_AUD_LINGUA ? 1 : op == AJ_LEG_LINGUA || op == AJ_LEG_LINGUA2 ? 2 : -1;
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0.45f);
      for (i = 0; i < 3; i++) {
        float by = 36 + i * 100;
        const char *v = textoValor(OP3[i]);
        float al = foco < 0 || foco == i ? 1.0f : 0.6f;
        ajCenaVidro(RR(36, by, 568, 88), 24 * s, 1);
        if (foco == i) gfx_cor(RR(36, by, 568, 88), 24.0f / 88.0f, 1, 1, 1, 0.08f);
        gfx_cor(RR(56, by + 20, 48, 48), 0.5f, 1, 1, 1, 0.08f);
        ICON(ico[i], 66, by + 30, 28, 0.9f * al);
        { TxtLinha l = ajTxtC(ajEstiloPxS(16 * s, 1), rotuloOpcao(OP3[i]), 250 * s);
          txt_desenhar_alpha(l, PX(122), PY(by + 22), al); }
        if (v && v[0]) { TxtLinha l = ajTxtC(ajEstiloPxS(13 * s, 0), v, 250 * s); txt_desenhar_alpha(l, PX(122), PY(by + 50), 0.6f * al); }
        for (j = 0; j < 3; j++) {
          float cx = 432 + j * 52;
          TxtEstilo e = ajEstiloPxS(12 * s, 1);
          gfx_cor(RR(cx, by + 22, 44, 44), 0.5f, 1, 1, 1, j == 0 ? 0.0f : 0.07f);
          if (j == 0) gfx_cor(RR(cx, by + 22, 44, 44), 0.5f, ar, ag, ab, 0.9f * al);
          if ((int)e >= 0) { int t = j == 0 ? ajTinta() : 243;
            TxtLinha l = txt_linha(e, cod[(i + j) % 3], t, t, t, 255);
            txt_desenhar_alpha(l, PX(cx + 22) - l.w * 0.5f, PY(by + 44) - l.h * 0.5f, j == 0 ? al : 0.55f * al); }
        }
      }
      break; }
    case AJS_TVAOVIVO: {   // o canal tocando em cima e o guia embaixo
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0);
      gfx_veu_css((GfxRect){ x, y + h * 0.18f, w, h * 0.82f }, 0, 0, 0.62f, 0.96f);
      VID(28, 26, 92, 32, 16); ACC(42, 37, 10, 10, 1); BAR(60, 38, 44, 0.85f);
      for (i = 0; i < 4; i++) {
        float by = 150 + i * 50;
        char logo[1200];
        const CatItem *canal = cat_n() > 0 ? cat_item((i * 3) % cat_n()) : NULL;
        int temLogoCanal = canal && !strcmp(canal->tipo, "channel") && canal->logo[0];
        VID(28, by, 74, 42, 12);
        if (temLogoCanal) ajLogo(canal->logo, PX(40), PY(by + 7), 50 * s, 28 * s, 0.95f);
        else { snprintf(logo, sizeof logo, "%s/logo/%02d.png", uxDirArte, (i * 3) % 8); ajLogo(logo, PX(40), PY(by + 9), 50 * s, 24 * s, 0.95f); }
        { float bx = 110;
          for (j = 0; j < 3; j++) {
            float bw = j == 0 ? (i % 2 ? 170 : 130) : j == 1 ? (i % 2 ? 150 : 200) : 400;
            if (bx >= 612) break;
            if (bx + bw > 612) bw = 612 - bx;
            ajCenaVidro(RR(bx, by, bw, 42), 12 * s, 1);
            if (j == 0 && i == 1) gfx_cor(RR(bx, by, bw, 42), 12.0f / 42.0f, 1, 1, 1, 0.1f);
            if (bw > 50) BAR(bx + 14, by + 12, (bw - 28) * 0.6f, j == 0 ? 0.85f : 0.5f);
            if (bw > 70) BAR(bx + 14, by + 25, (bw - 28) * 0.35f, 0.3f);
            bx += bw + 6;
          } }
      }
      gfx_cor(RR(258, 140, 3, 216), 0.05f, ar, ag, ab, 0.95f); ACC(253, 134, 13, 13, 1);
      break; }
    case AJS_CONTAS: {   // perfis e servicos conectados
      const ContaPerfil *ativo = perfis_item_ativo();
      int n = perfis_n(), qtd = n > 0 ? (n < 3 ? n : 3) : 1;
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0.55f);
      for (i = 0; i < qtd; i++) {
        const ContaPerfil *p = n > 0 ? perfis_item(i) : ativo;
        int f = (p && p->indice == perfis_ativo()) || (n == 0 && i == 0);
        float d = f ? 96 : 80, cx = 320 + (i - (qtd - 1) * 0.5f) * 140, bx = cx - d * 0.5f, by = f ? 30 : 38;
        char avatar[1200];
        snprintf(avatar, sizeof avatar, "%s/icones/mt_perfil.png", uxDirArte);
        gfx_cor(RR(bx, by, d, d), 0.5f, 0.24f, 0.25f, 0.29f, 1);
        if (p && p->avatarUrl[0]) ajImg(p->avatarUrl, RR(bx, by, d, d), 0.5f * d * s, f ? 1.0f : 0.6f);
        else ajLogo(avatar, PX(bx + d * 0.22f), PY(by + d * 0.22f), d * 0.56f * s, d * 0.56f * s, f ? 0.95f : 0.6f);
        { TxtLinha nm = ajTxtC(ajEstiloPxS(14 * s, f), p && p->nome[0] ? p->nome : "Perfil", 130 * s);
          txt_desenhar_alpha(nm, PX(cx) - nm.w * 0.5f, PY(142), f ? 1.0f : 0.6f); }
      }
      for (i = 0; i < 3; i++) {
        static const char *const sv[3] = { NULL, "aj_cloud", "aj_bell" };   // Trakt, sincronizacao, avisos
        float bx = 36 + i * 196;
        VID(bx, 210, 176, 108, 22);
        if (!sv[i]) { char marca[1200]; snprintf(marca, sizeof marca, "%s/marcas/trakt_wordmark.png", uxDirArte);
                      if (ajLogo(marca, PX(bx + 20), PY(232), 96 * s, 28 * s, 0.95f) <= 0) ICON("aj_plug", bx + 20, 232, 28, 0.85f); }
        else ICON(sv[i], bx + 20, 232, 28, 0.85f);
        BAR(bx + 20, 280, i == 0 ? 90 : 70, 0.5f);
        if (i < 2) ACC(bx + 146, 238, 12, 12, 1); else gfx_cor(RR(bx + 146, 238, 12, 12), 0.5f, 1, 1, 1, 0.2f);
      }
      break; }
    case AJS_DESEMPENHO: {   // fps e memoria de imagens
      static const char *const ic9[4] = { "aj_memory-stick", "aj_images", "aj_zap", "aj_database" };
      static const float pc[4] = { 0.46f, 0.72f, 0.34f, 0.58f };
      static const float fps[16] = { 58, 60, 60, 59, 60, 54, 60, 60, 60, 57, 60, 60, 59, 60, 60, 60 };
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0.6f);
      VID(36, 36, 260, 288, 24);
      TXT("60", 64, 1, 60, 56, 1);
      TXT("fps", 18, 0, 160, 92, 0.6f);
      for (i = 0; i < 16; i++) {
        float bh = 8 + (fps[i] - 50) * 7.0f;
        gfx_cor(RR(60 + i * 13.5f, 296 - bh, 9, bh), 0.3f, ar, ag, ab, i == 15 ? 1.0f : 0.45f);
      }
      VID(312, 36, 292, 288, 24);
      for (i = 0; i < 4; i++) {
        float by = 66 + i * 64;
        ICON(ic9[i], 336, by, 26, 0.8f);
        gfx_cor(RR(376, by + 9, 204, 9), 0.5f, 1, 1, 1, 0.12f);
        ACC(376, by + 9, 204 * pc[i], 9, i == 0 ? 1.0f : 0.6f);
      }
      break; }
    default: {   // Sobre e ajuda: marca, versao, atualizacao e o QR do guia
      static const unsigned QR[21] = { 0x1FC07F, 0x104A41, 0x175B5D, 0x174A5D, 0x1754DD, 0x104C41, 0x1FD57F, 0x000600,
                                       0x1B6D3A, 0x0A9145, 0x15B6CB, 0x08592A, 0x1D2EB5, 0x0005A2, 0x1FCB4C, 0x1046D3,
                                       0x175A28, 0x174D97, 0x1752AC, 0x104B55, 0x1FC93A };
      ajCenaArte(ajArte(AJA_FUNDO, A), q, rr, 0.6f);
      VID(36, 36, 344, 288, 24);
      { const char *marca = logoapp_caminho(logoapp_atual(), LOGO_F_HORIZONTAL);
        ajLogo(marca, PX(62), PY(64), 170 * s, 48 * s, 0.96f); }
      TXT(NV_VERSAO, 18, 1, 62, 132, 0.85f);
      BAR(62, 172, 220, 0.4f); BAR(62, 190, 170, 0.4f);
      VID(62, 248, 200, 48, 24); gfx_cor(RR(62, 248, 200, 48), 0.5f, 1, 1, 1, 0.06f);
      ICON("aj_download", 80, 260, 24, 0.9f); BAR(116, 268, 100, 0.75f); ACC(240, 266, 10, 10, 1);
      VID(396, 36, 208, 288, 24);
      gfx_cor(RR(424, 64, 152, 152), 12.0f / 152.0f, 0.953f, 0.949f, 0.937f, 1);
      for (i = 0; i < 21; i++) for (j = 0; j < 21; j++)
        if (QR[i] >> (20 - j) & 1) gfx_cor(RR(430 + j * 6.66f, 70 + i * 6.66f, 6.8f, 6.8f), 0, 0.06f, 0.06f, 0.07f, 1);
      ICON("aj_book-open", 424, 244, 26, 0.85f); BAR(462, 254, 110, 0.6f); BAR(424, 290, 150, 0.35f);
      break; }
  }
  gfx_sem_recorte();
#undef PX
#undef PY
#undef RR
#undef VID
#undef BAR
#undef ACC
#undef ICON
#undef TXT
  return h;
}
