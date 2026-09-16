// Ajustes: lista vertical em secoes, rotulo a esquerda e valor a direita.
//
// A regra que organiza a tela inteira: existem TRES naturezas de linha e elas
// TEM que parecer diferentes. Linha de escolha ganha foco e setas ao redor do
// valor; linha NUMERICA acrescenta uma barra de
// preenchimento sob o valor, porque "28%" sem barra nao diz onde fica no
// intervalo; linha so de leitura ganha um realce apagado, sem setas. Com o mesmo
// desenho nas tres, o usuario aperta esquerda e direita em cima da versao do app
// esperando que algo aconteca — foi por isso que a distincao virou requisito.
//
// As chaves e os agrupamentos seguem a tela de Layout do app web
// (js/ui/screens/settings/settingsScreen.js), inclusive os rotulos em portugues
// lidos da tela rodando.
#include "ajustes.h"
#include "descoberta.h"
#include "extras.h"
#include "fileiras.h"
#include "idioma.h"
#include "linguas.h"
#include "addons.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "sessao.h"
#include "sync.h"
#include "perfis.h"
#include "traktauth.h"
#include "simklauth.h"
#include "qr.h"
#include "atualizacao.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Versao do app: vem do build (-DNV_VERSAO, que tools/env.sh le do
// appinfo.json). Era um literal aqui e ficou parado em 1.0.44 por nove
// releases — a tela de Ajustes mentia a versao. "dev" so aparece numa
// compilacao a mao, sem o env.sh.
#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#define AJ_VERSAO       NV_VERSAO

#define AJ_LINHA_H       88.0f
#define AJ_LINHA_GAP      8.0f
#define AJ_SEC_GAP       46.0f    // fim de uma secao ao cabecalho da proxima
#define AJ_SEC_CABEC     44.0f    // altura reservada ao cabecalho da secao
// Cabecalho de SUBSECAO: um rotulo pequeno no meio da secao. Ver SUBSECOES.
#define AJ_SUB_CABEC     42.0f
// Nao e constante: acompanha a rail, como todo o resto do conteudo. Com a
// barra recolhida a lista tambem comeca em 104 — deixar 248 cravado aqui fazia
// a tela de Ajustes ser a unica desalinhada das outras.
// COLUNA DE SECOES a esquerda da lista. Ela existe por um motivo de controle,
// nao de estetica: o unico atalho entre secoes era PgUp/PgDn, e NAO EXISTE
// PgUp nem PgDn num controle de TV — o atalho era inalcancavel justamente no
// aparelho. Aqui as secoes recebem foco de verdade, alcancadas pela tecla que
// todo controle tem: Voltar. Ver a nota grande em ajustes_evento.
//
// 196 + 24 saem da LISTA, e nao da margem: com a barra lateral FIXA o conteudo
// ja comeca em 248, e empurrar a lista para a direita sem encolher deixaria o
// painel de ajuda com menos de 240 px — ele desaparece abaixo disso e a tela
// perde a unica explicacao que tem.
// 240 e nao 196, e 84 de altura e nao 58: com DOZE categorias a coluna estava
// cheia e os rotulos cortavam ("Continuar assistindo" saia "Continue…"), o que
// e o pior lugar possivel para um corte — e o nome da categoria que diz onde a
// pessoa esta. Com SEIS (ver SECOES) sobra altura de sobra, entao a linha ganha
// o ICONE da categoria a esquerda e o nome inteiro cabe.
#define AJ_IDX_X        ajustes_conteudo_x()
#define AJ_IDX_W        240.0f
#define AJ_IDX_GAP       24.0f
#define AJ_IDX_H         84.0f
#define AJ_IDX_ICONE     34.0f    // lado do icone dentro da linha da categoria
#define AJ_LISTA_X      (AJ_IDX_X + AJ_IDX_W + AJ_IDX_GAP)
#define AJ_LISTA_W      900.0f
#define AJ_PAD           34.0f    // borda da linha ao texto
#define AJ_TOPO        (NV_MARGEM_Y + 118.0f)   // abaixo do titulo da tela
#define AJ_BASE        (NV_TELA_H - NV_MARGEM_Y - 48.0f)
// Raio da linha em fracao do menor lado (o SDF do shader e normalizado):
// 12px sobre 88 de altura.
#define AJ_RAIO           0.14f

// Ordem do enum = ordem no arquivo de chaves e nas tabelas. Acrescentar no MEIO
// e seguro: o arquivo e por chave, nao posicional (ver ajustes_dir).
typedef enum {
  // Reproducao
  AJ_QUALIDADE, AJ_DV, AJ_ATMOS, AJ_LEG_LINGUA, AJ_AUD_LINGUA,
  AJ_PAUSA_OVERLAY,
  // Layout da Home
  AJ_LANDSCAPE, AJ_HERO_CHEIO,
  // Fileiras da Home
  AJ_FIL_LIMITE, AJ_FIL_ORDEM,
  // Conteudo da Home
  AJ_RAIL, AJ_RAIL_MODERNA, AJ_RAIL_BLUR, AJ_HERO, AJ_HERO_CATALOGOS,
  AJ_DESCOBRIR, AJ_ROTULOS, AJ_NOME_ADDON, AJ_SUFIXO_TIPO,
  AJ_OCULTAR_NLANC, AJ_NOTAS_HOME, AJ_GRAD_CLASSICO,
  // Continuar assistindo
  AJ_CW_LIGADO, AJ_CW_FONTE, AJ_CW_ESTILO, AJ_CW_THUMB, AJ_CW_BLUR_PROX,
  AJ_CW_FURTHEST, AJ_CW_NAO_EXIBIDOS, AJ_CW_ORDEM,
  // Pagina de detalhe
  AJ_DET_BLUR_NAO_VISTOS, AJ_DET_TRAILER, AJ_DET_META_EXT, AJ_DET_DATA_CHEIA,
  // Foco no poster
  AJ_EXPANDIR, AJ_EXPANDIR_ATRASO, AJ_NAV_RAPIDA, AJ_BORDA_FOCO,
  // Profundidade
  AJ_PROF, AJ_PROF_BORDA, AJ_PROF_BRILHO, AJ_PROF_COBERTURA,
  AJ_PROF_POSTERS, AJ_PROF_CW, AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS,
  // Tamanho do item
  AJ_LARGURA_DP, AJ_RAIO_DP,
  // Interface
  AJ_IDIOMA, AJ_ANIM, AJ_RESOLUCAO, AJ_TEMA,
  // Conta
  AJ_PERFIL_ATIVO, AJ_SYNC, AJ_ADDONS, AJ_SALVOS_DEST, AJ_TRAKT, AJ_SIMKL, AJ_SAIR,
  // Sobre
  AJ_VERSAO_I, AJ_ESPACO,
  // Integracoes — TMDB (tmdb_settings do blob da conta, ver
  // profileSettingsSyncService.js do web)
  AJ_TMDB_LIGADO, AJ_TMDB_IDIOMA, AJ_TMDB_ARTE, AJ_TMDB_BASICO, AJ_TMDB_FICHA,
  AJ_TMDB_DATAS,
  AJ_TMDB_ELENCO, AJ_TMDB_PROD, AJ_TMDB_REDES, AJ_TMDB_EPS, AJ_TMDB_TRAILERS,
  AJ_TMDB_MAIS, AJ_TMDB_COL, AJ_TMDB_CW,
  // Integracoes — MDBList (mdblist_settings do blob)
  AJ_MDB_LIGADO, AJ_MDB_CHAVE, AJ_MDB_TRAKT, AJ_MDB_IMDB, AJ_MDB_TMDB,
  AJ_MDB_LETTER, AJ_MDB_TOMATES, AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_MAL,
  AJ_N
} OpcaoId;

static const char *V_QUALIDADE[] = { "Automática", "4K", "1080p", "720p" };
static const char *V_LIGA[]      = { "Ligado", "Desligado" };
static const char *V_IDIOMA[]    = { "Português", "English" };
static const char *V_ANIM[]      = { "Completas", "Reduzidas" };
// A ORDEM IMPORTA: o indice 0 e o padrao (ver a lista de padroes, que e
// posicional), e o padrao tem de ser 1080p. Numa TV que NAO concede a
// superficie 4K a escolha nao faz nada, e numa que concede ela quadruplica o
// preenchimento — nao e coisa para ligar sozinha em aparelho nenhum.
static const char *V_RESOLUCAO[] = { "1080p", "4K (experimental)" };
// `collapseSidebar`: recolhida = a rail some e o conteudo comeca em 104.
static const char *V_RAIL[]      = { "Recolhida", "Fixa" };
// `continueWatchingCardStyle`, validado em layoutPreferences.js contra
// exatamente estes tres valores.
static const char *V_CW[]        = { "Card", "Largo", "P\xc3\xb4ster" };
// FONTE do "Continuar assistindo". As duas ja existem e ja sao fundidas em
// montarContinuar (descoberta.c); isto so escolhe quais entram.
//   Ambas  = conta primeiro, Trakt preenchendo o que falta (o de sempre)
//   Conta  = so o progresso da conta Nuvio (syncprog.c)
//   Trakt  = so o /sync/playback do Trakt
static const char *V_CW_FONTE[]  = { "Ambas", "Conta Nuvio", "Trakt" };
// `continueWatchingSortMode`, normalizado em normalizeContinueWatchingSortMode.
static const char *V_CW_ORDEM[]  = { "Padrão", "Estilo streaming", "Separar futuros" };
// `discoverLocation`, validado contra estes tres.
static const char *V_DESCOBRIR[] = { "Mostrar na Busca", "Na barra lateral", "Desligado" };
// `homeImdbRatingsVisibility` — normalizeHomeImdbRatingsVisibility so aceita
// SHOW_ALL e HIDE_ALL.
static const char *V_NOTAS[]     = { "Mostrar", "Ocultar" };
// ONDE O "+" ESCREVE ALEM DA LISTA LOCAL.
//
// A lista local (salvos.c) e escrita SEMPRE, nos dois valores, e isso nao e
// esquecimento: antes dela o "+" nao guardava nada em disco, entao quem nao
// tinha Trakt vinculado perdia tudo no primeiro ciclo de descoberta. Ver a nota
// de abertura de salvos.h. O que esta escolha decide e se o "+" TAMBEM publica
// na watchlist do Trakt.
//
// Padrao "Watchlist do Trakt" = o comportamento que o app ja tinha. Trocar o
// padrao para a lista local faria o "+" de quem usa Trakt parar de publicar la
// depois de uma atualizacao, sem ninguem ter pedido.
static const char *V_SALVOS[]    = { "Lista do Nuvio", "Watchlist do Trakt" };
// `tmdb_language` no blob da conta guarda so o idioma BASE ("pt", "en") —
// normalizeTmdbLanguageForAndroid corta a regiao. A lista aqui e curta de
// proposito: a do web e gerada de AVAILABLE_LANGUAGES inteiro, e atravessar
// 40 idiomas numa seta de controle e pior que cobrir os que fazem sentido
// nesta TV. "Da interface" preserva o comportamento anterior: pt-BR quando a
// interface esta em portugues, en-US em ingles.
static const char *V_TMDB_LING[] = {
  "Da interface", "Português (Brasil)", "English", "Español", "Français",
  "Deutsch", "Italiano", "Português (Portugal)", "日本語", "한국어", "中文"
};
// Preenchido em rotulosDeIdioma(), no arranque: os nomes saem de linguas.c em
// vez de serem uma segunda lista escrita a mao aqui. LING_MAX_OPC e folga: se
// linguas.c crescer, o excedente simplesmente nao aparece — melhor que ler
// fora do vetor.
// 32: a lista de linguas.c tem 30 entradas (2 acoes + 28 idiomas). O valor
// anterior era 24 e TRUNCAVA em silencio — os idiomas do fim da lista existiam
// em linguas.c e nao apareciam na tela.
#define LING_MAX_OPC 32
// TEMA: a cor de DESTAQUE, e so ela.
//
// O app web tem doze temas e cada um troca onze variaveis de CSS, o fundo
// incluido. Aqui entra UMA: `--focus-color`, o anel que marca onde o foco
// esta. E a escolha honesta para este app, e a razao esta medida: os tons de
// cinza desta interface foram calibrados um a um contra o fundo #0D0D0D (ver
// NV_COR_FUNDO em layout.h, e o defeito de contraste 1,0:1 que ele descreve).
// Trocar o fundo por tema invalidaria essa calibragem inteira, sem ninguem
// para refaze-la. O anel, ao contrario, e sempre branco hoje: tingi-lo nao
// depende de recalibrar nada e e o elemento que o olho segue no sofa.
//
// Os valores sao os `--focus-color` de themeColors.js do app web, copiados,
// nao escolhidos — e por isso a TV mostra a mesma cor que a pessoa viu la.
static const struct { float r, g, b; } TEMA_ACENTO[] = {
  { 1.000f, 1.000f, 1.000f },   // WHITE        #ffffff
  { 1.000f, 0.322f, 0.322f },   // CRIMSON      #ff5252
  { 0.259f, 0.647f, 0.961f },   // OCEAN        #42a5f5
  { 0.671f, 0.278f, 0.737f },   // VIOLET       #ab47bc
  { 0.400f, 0.733f, 0.416f },   // EMERALD      #66bb6a
  { 1.000f, 0.655f, 0.149f },   // AMBER        #ffa726
  { 0.925f, 0.251f, 0.478f },   // ROSE         #ec407a
  { 1.000f, 0.831f, 0.361f },   // GOLD         #ffd45c
  { 0.482f, 0.941f, 0.553f },   // JADE         #7bf08d
  { 1.000f, 0.702f, 0.478f },   // ROSE_GOLD    #ffb37a
  { 0.302f, 0.890f, 1.000f },   // ARCTIC_BLUE  #4de3ff
  { 0.953f, 0.961f, 0.969f },   // GRAPHITE     #f3f5f7
};
#define AJ_N_TEMAS (int)(sizeof TEMA_ACENTO / sizeof *TEMA_ACENTO)
static const char *V_TEMA[] = {
  "Branco", "Carmesim", "Oceano", "Violeta", "Esmeralda", "Âmbar",
  "Rosa", "Dourado", "Jade", "Ouro rosé", "Azul ártico", "Grafite"
};
// MESMA ORDEM de TEMA_ACENTO e de V_TEMA: e o indice que liga os tres.
static const char *W_TEMA[] = {
  "WHITE", "CRIMSON", "OCEAN", "VIOLET", "EMERALD", "AMBER",
  "ROSE", "GOLD", "JADE", "ROSE_GOLD", "ARCTIC_BLUE", "GRAPHITE", NULL
};

static const char *V_LINGUA[LING_MAX_OPC];
static int         nLingua;
static void rotulosDeIdioma(void) {
  int i, n = ling_opcao_n();
  if (n > LING_MAX_OPC) n = LING_MAX_OPC;
  for (i = 0; i < n; i++) {
    const char *c = ling_opcao_codigo(i);
    // "" = seguir a conta; "*" = mostrar tudo. Os dois primeiros sao acoes, nao
    // idiomas, e por isso tem rotulo proprio.
    // SEM i18n() AQUI DE PROPOSITO: rotulosDeIdioma() roda uma vez por abertura
    // da tela (ajustes_iniciar/ajustes_dir), nao a cada quadro. Se traduzisse
    // aqui, trocar "Idioma da interface" para Ingles DENTRO da mesma sessao de
    // Ajustes deixaria a lista de idiomas presa no idioma antigo ate a tela
    // reabrir. Guardando o nome cru, quem traduz e desenhaLinha->txt_linha_corta
    // a cada quadro (text.c aplica i18n() no que for desenhado, sempre com o
    // idioma CORRENTE) — o mesmo motivo por que os demais rotulos desta tabela
    // (V_QUALIDADE etc.) tambem ficam em portugues aqui.
    V_LINGUA[i] = !c[0] ? "Da conta" : (!strcmp(c, "*") ? "Todas" : ling_nome(c));
  }
  nLingua = n;
}

// Natureza da linha.
// OP_ACAO responde ao OK, nao a esquerda/direita. Ela NAO e leitura: uma linha
// que faz alguma coisa tem de ter o mesmo destaque de quem muda valor, senao o
// usuario aperta OK esperando que nada aconteca.
typedef enum { OP_ESCOLHA, OP_NUMERO, OP_LEITURA, OP_ACAO } OpcaoTipo;

typedef struct {
  const char  *rotulo;
  OpcaoTipo    tipo;
  const char **valores;   // OP_ESCOLHA
  int          n;         // OP_ESCOLHA: quantos valores
  int          min, max, passo;   // OP_NUMERO
  const char  *sufixo;            // OP_NUMERO: "%", "s", "dp"
} Opcao;

#define ESC(rot, vals, qtd) { rot, OP_ESCOLHA, vals, qtd, 0, 0, 0, NULL }
#define NUM(rot, lo, hi, st, suf) { rot, OP_NUMERO, NULL, 0, lo, hi, st, suf }
#define LER(rot)            { rot, OP_LEITURA, NULL, 0, 0, 0, 0, NULL }
#define ACAO(rot)           { rot, OP_ACAO,    NULL, 0, 0, 0, 0, NULL }

static const Opcao OPCOES[AJ_N] = {
  ESC("Qualidade máxima",           V_QUALIDADE, 4),
  ESC("Dolby Vision",               V_LIGA, 2),
  ESC("Dolby Atmos",                V_LIGA, 2),
  // O `n` real e escrito por rotulosDeIdioma(); 2 aqui so mantem a tabela
  // valida antes do arranque.
  ESC("Idioma da legenda",          V_LINGUA, 2),
  ESC("Idioma do áudio",            V_LINGUA, 2),
  // `playback_pause_overlay` (settingsScreen.js:6320). Liga o painel que
  // sobe cinco segundos depois de pausar; ver pausao.h.
  ESC("Painel ao pausar",           V_LIGA, 2),   // pauseOverlayEnabled

  ESC("Pôsteres horizontais",       V_LIGA, 2),   // modernLandscapePostersEnabled
  ESC("Fundo em tela cheia",        V_LIGA, 2),   // modernHeroFullScreenBackdropEnabled

  // O limite NAO tem valor proprio em valor[]: ele mora em fileiras.c, que e
  // quem grava fileirasui.txt e quem a descoberta e a home consultam. A linha
  // aqui e um ESPELHO, sincronizado em ajustes_dir/ajustes_iniciar — duas
  // copias do mesmo numero divergem no primeiro caminho que esquecer uma.
  NUM("Limite de fileiras",         FIL_LIMITE_MIN, FIL_LIMITE_MAX, 1, NULL),
  ACAO("Ordenar e ativar fileiras"),

  ESC("Barra lateral",              V_RAIL, 2),   // collapseSidebar
  ESC("Barra lateral moderna",      V_LIGA, 2),   // modernSidebar
  ESC("Desfoque da barra moderna",  V_LIGA, 2),   // modernSidebarBlur
  ESC("Mostrar destaque",           V_LIGA, 2),   // heroSectionEnabled
  LER("Catálogos do destaque"),                   // heroCatalogKeys (contagem)
  ESC("Local do Descobrir",         V_DESCOBRIR, 3), // discoverLocation
  ESC("Rótulos nos pôsteres",       V_LIGA, 2),   // posterLabelsEnabled
  ESC("Nome do addon no catálogo",  V_LIGA, 2),   // catalogAddonNameEnabled
  ESC("Tipo de conteúdo",           V_LIGA, 2),   // catalogTypeSuffixEnabled
  ESC("Ocultar não lançados",       V_LIGA, 2),   // hideUnreleasedContent
  ESC("Avaliações gerais",          V_NOTAS, 2),  // homeImdbRatingsVisibility
  ESC("Gradiente de foco clássico", V_LIGA, 2),   // classicFocusGradientEnabled

  ESC("Mostrar \"Continuar assistindo\"", V_LIGA, 2), // continueWatchingEnabled
  ESC("Fonte do \"Continuar assistindo\"", V_CW_FONTE, 3),   // local, ver V_CW_FONTE
  ESC("Estilo do \"Continuar assistindo\"", V_CW, 3), // continueWatchingCardStyle
  ESC("Miniatura do episódio",      V_LIGA, 2),   // useEpisodeThumbnailsInCw
  ESC("Desfocar próximo episódio",  V_LIGA, 2),   // blurContinueWatchingNextUp
  ESC("Próximo do episódio mais alto", V_LIGA, 2),// nextUpFromFurthestEpisode
  ESC("Mostrar episódios não exibidos", V_LIGA, 2),// showUnairedNextUp
  ESC("Ordenação",                  V_CW_ORDEM, 3), // continueWatchingSortMode

  ESC("Desfocar não assistidos",    V_LIGA, 2),   // blurUnwatchedEpisodes
  ESC("Botão de trailer",           V_LIGA, 2),   // detailPageTrailerButtonEnabled
  ESC("Priorizar metadados externos", V_LIGA, 2), // preferExternalMetaAddonDetail
  ESC("Data de lançamento completa", V_LIGA, 2),  // showFullReleaseDate

  ESC("Expandir pôster ao focar",   V_LIGA, 2),   // focusedPosterBackdropExpandEnabled
  NUM("Atraso da expansão",         0, 10, 1, " s"), // ...ExpandDelaySeconds
  ESC("Navegação horizontal rápida", V_LIGA, 2),  // fastHorizontalNavigationEnabled
  ESC("Borda no cartaz em foco",    V_LIGA, 2),   // local: ver bordaFocoCartaz

  ESC("Efeito de profundidade",     V_LIGA, 2),   // cardDepthEnabled
  NUM("Brilho da borda",            0, 100, 2, "%"), // cardDepthEdgeStrength
  NUM("Reflexo",                    0, 100, 2, "%"), // cardDepthSheenStrength
  NUM("Cobertura da borda",         0, 100, 2, "%"), // cardDepthEdgeCoverage
  ESC("Profundidade nos pôsteres",  V_LIGA, 2),
  ESC("Profundidade no \"Continuar\"", V_LIGA, 2),
  ESC("Profundidade nos episódios", V_LIGA, 2),
  ESC("Profundidade no elenco",     V_LIGA, 2),
  ESC("Profundidade nos trailers",  V_LIGA, 2),

  NUM("Largura do item",            72, 200, 2, " dp"), // posterCardWidthDp
  NUM("Arredondamento",             0, 40, 1, " dp"),   // posterCardCornerRadiusDp

  ESC("Idioma",                     V_IDIOMA, 2),
  ESC("Animações",                  V_ANIM, 2),
  ESC("Resolução da interface",     V_RESOLUCAO, 2),
  ESC("Cor de destaque",            V_TEMA, AJ_N_TEMAS),  // selected_theme

  LER("Perfil"),
  LER("Sincronização"),
  ACAO("Addons"),
  ESC("Onde o + salva",             V_SALVOS, 2),
  ACAO("Trakt"),
  ACAO("Simkl"),
  ACAO("Sair da conta"),
  LER("Versão"),
  LER("Memória usada por imagens"),

  // Integracoes — TMDB. Os rotulos seguem a pagina integration:tmdb do web
  // (settingsScreen.js): um master + um toggle por recurso que o enriquecimento
  // toca. LIGADO por padrao em tudo: diferente do web, onde o TMDB e opt-in, o
  // nativo sempre enriqueceu por ele — nascer desligado apagaria elenco com
  // foto, ficha e trailers de quem ja usa o app sem nunca ter visto o ajuste.
  ESC("TMDB",                       V_LIGA, 2),   // tmdb_enabled
  ESC("Idioma dos metadados",       V_TMDB_LING, 11), // tmdb_language
  ESC("Arte localizada",            V_LIGA, 2),   // tmdb_use_artwork
  ESC("Título e sinopse",           V_LIGA, 2),   // tmdb_use_basic_info
  ESC("Ficha técnica",              V_LIGA, 2),   // tmdb_use_details
  ESC("Datas de lançamento",        V_LIGA, 2),   // tmdb_use_release_dates
  ESC("Elenco e equipe",            V_LIGA, 2),   // tmdb_use_credits
  ESC("Produtoras",                 V_LIGA, 2),   // tmdb_use_productions
  ESC("Redes e estúdios",           V_LIGA, 2),   // tmdb_use_networks
  ESC("Episódios",                  V_LIGA, 2),   // tmdb_use_episodes
  ESC("Trailers",                   V_LIGA, 2),   // tmdb_use_trailers
  ESC("\"Mais como este\"",         V_LIGA, 2),   // tmdb_use_more_like_this
  ESC("Coleções e sagas",           V_LIGA, 2),   // tmdb_use_collections
  ESC("Enriquecer \"Continuar assistindo\"", V_LIGA, 2), // tmdb_enrich_continue_watching

  // Integracoes — MDBList. O master liga/desliga a consulta; os demais
  // escolhem quais fontes de nota viram cartao na pagina de titulo.
  ESC("MDBList",                    V_LIGA, 2),   // mdblist_enabled
  LER("Chave da API"),                            // mdblist_api_key (status)
  ESC("Notas do Trakt",             V_LIGA, 2),   // mdblist_show_trakt
  ESC("Notas do IMDb",              V_LIGA, 2),   // mdblist_show_imdb
  ESC("Notas do TMDB",              V_LIGA, 2),   // mdblist_show_tmdb
  ESC("Notas do Letterboxd",        V_LIGA, 2),   // mdblist_show_letterboxd
  ESC("Notas do Rotten Tomatoes",   V_LIGA, 2),   // mdblist_show_tomatoes
  ESC("Nota da audiência",          V_LIGA, 2),   // mdblist_show_audience
  ESC("Notas do Metacritic",        V_LIGA, 2),   // mdblist_show_metacritic
  ESC("Notas do MyAnimeList",       V_LIGA, 2),   // mdblist_show_mal
};

// Nome de cada opcao no arquivo. O formato era POSICIONAL — uma linha por
// opcao, na ordem do enum — e por isso acrescentar uma opcao no meio fazia o
// arquivo de quem ja tinha o app aplicar os valores errados, em silencio. Com
// chave por linha, opcao nova nasce no padrao e as antigas continuam onde
// estavam. Os nomes seguem os do app web onde existe correspondente.
static const char *CHAVE[] = {
  "qualidade", "dolbyVision", "dolbyAtmos",
  "legendaIdioma", "audioIdioma", "pauseOverlayEnabled",
  "modernLandscapePostersEnabled", "modernHeroFullScreenBackdropEnabled",
  // "-": local, nao vem da conta e nao vai para ajustes.txt. Os dois vivem em
  // fileirasui.txt (fileiras.c) e a conta nao tem chave equivalente — o teto do
  // web para este runtime e uma CONSTANTE (HOME_MAX_ROWS_LEGACY_TV), nao uma
  // preferencia, e a ordem da conta nunca pode ser ESCRITA pela TV.
  "-limiteFileiras", "-ordenarFileiras",
  "collapseSidebar", "modernSidebar", "modernSidebarBlur",
  "heroSectionEnabled", "-heroCatalogKeys",
  "discoverLocation", "posterLabelsEnabled", "catalogAddonNameEnabled",
  "catalogTypeSuffixEnabled", "hideUnreleasedContent",
  "homeImdbRatingsVisibility", "classicFocusGradientEnabled",
  "continueWatchingEnabled",
  // LOCAL, e nao do web: o app oficial nao tem esta escolha, entao nao ha
  // campo dela no blob da conta. Chave propria para nao colidir com um nome
  // que o servidor possa criar depois.
  "cwFonteLocal",
  "continueWatchingCardStyle",
  "useEpisodeThumbnailsInCw", "blurContinueWatchingNextUp",
  "nextUpFromFurthestEpisode", "showUnairedNextUp", "continueWatchingSortMode",
  "blurUnwatchedEpisodes", "detailPageTrailerButtonEnabled",
  "preferExternalMetaAddonDetail", "showFullReleaseDate",
  "focusedPosterBackdropExpandEnabled", "focusedPosterBackdropExpandDelaySeconds",
  "fastHorizontalNavigationEnabled",
  "bordaFocoCartaz",
  "cardDepthEnabled", "cardDepthEdgeStrength", "cardDepthSheenStrength",
  "cardDepthEdgeCoverage", "cardDepthPostersEnabled",
  "cardDepthContinueWatchingEnabled", "cardDepthEpisodeCardsEnabled",
  "cardDepthCastEnabled", "cardDepthTrailersEnabled",
  "posterCardWidthDp", "posterCardCornerRadiusDp",
  "idioma", "animacoes", "resolucao_ui",
  // A conta JA MANDAVA esta chave e o app a jogava fora: ela vem dentro de
  // theme_settings no blob de ajustes (profileSettingsSyncService.js), e o
  // laco de ajustes_aplicar_blob so procura as chaves que estao nesta lista.
  // Quem escolher JADE no app web ganha o anel jade na TV sem configurar de
  // novo — e o contrario tambem vale.
  "selected_theme",
  // Conta: sao linhas locais, nao vem nem vao para o perfil na nuvem.
  // "salvosDestino" e LOCAL como cwFonteLocal, e por isso SEM o "-": o app
  // oficial nao tem esta escolha, entao nao ha campo dela no blob da conta —
  // mas ela precisa sobreviver ao fechamento, e gravar() pula toda chave
  // iniciada por "-".
  "-perfil", "-sync", "-addons", "salvosDestino", "-trakt", "-simkl", "-sair",
  "-versao", "-espaco",
  // Integracoes: os nomes sao exatamente os que profileSettingsSyncService.js
  // exporta dentro de tmdb_settings / mdblist_settings — a conta aplica e a
  // TV respeita a escolha feita no app web, e vice-versa.
  "tmdb_enabled", "tmdb_language", "tmdb_use_artwork", "tmdb_use_basic_info",
  "tmdb_use_details",
  "tmdb_use_release_dates", "tmdb_use_credits", "tmdb_use_productions",
  "tmdb_use_networks", "tmdb_use_episodes", "tmdb_use_trailers",
  "tmdb_use_more_like_this", "tmdb_use_collections",
  "tmdb_enrich_continue_watching",
  // A chave do mdblist chega pelas CREDENCIAIS da conta (sync.c), nao pelo
  // blob de ajustes; a linha aqui so mostra o estado, por isso o "-".
  "mdblist_enabled", "-mdblistChave",
  "mdblist_show_trakt", "mdblist_show_imdb", "mdblist_show_tmdb",
  "mdblist_show_letterboxd", "mdblist_show_tomatoes", "mdblist_show_audience",
  "mdblist_show_metacritic", "mdblist_show_mal",
};
// QUATRO VETORES PARALELOS indexados pelo mesmo enum AJ_*: OPCOES, CHAVE,
// valor e as secoes. OPCOES ja e declarado [AJ_N], e `valor` aceita inicializacao
// parcial em silencio — CHAVE nao tem nenhuma protecao. Inserir uma opcao no
// meio do enum e esquecer UMA das listas desloca todas as seguintes: a chave de
// um ajuste passa a gravar o valor de outro, e o arquivo de quem ja usava o app
// volta trocado. Barato de conferir, caro de descobrir.
_Static_assert(sizeof CHAVE / sizeof *CHAVE == AJ_N,
               "CHAVE fora de sincronia com o enum AJ_*");


// O compilador CONFERE que ha uma chave por opcao. Sem isto, acrescentar uma
// opcao no enum e esquecer a chave deixa as ultimas entradas em NULL e
// DESALINHA todas as chaves depois do ponto de insercao — e o defeito nao
// aparece na hora: so quando o ajustes.txt passa a existir, o strcmp(NULL,...)
// derruba o app no arranque seguinte. Foi exatamente o que aconteceu, e o
// unico sintoma na TV foi o app abrir e fechar.
typedef char conferi_uma_chave_por_opcao[
  (sizeof CHAVE / sizeof *CHAVE == AJ_N) ? 1 : -1];

// Onde cada secao comeca e quantas opcoes ela tem. Secao e um agrupamento
// visual, nao um nivel de navegacao: cima/baixo atravessa os cabecalhos sem
// parar neles, como no aparelho.
//
// ERAM DOZE, e o relato foi "tem muito menu e submenu, nao ta agrupado". Doze
// categorias para 58 opcoes nao e agrupamento: e uma segunda lista para
// atravessar antes de chegar na primeira. Cinco delas falavam da MESMA coisa
// com nomes diferentes — "Layout da Home", "Fileiras da Home" e "Conteudo da
// Home" sao a home; "Foco no Poster", "Efeito de Profundidade" e "Tamanho dos
// itens" sao o cartaz. Aqui elas viram SEIS categorias, e o que se perdeu de
// granularidade voltou como SUBSECOES (abaixo), que rotulam o bloco sem exigir
// mais um nivel de navegacao.
//
// AS FAIXAS SAO CONTIGUAS NO ENUM, e tinham de ser: o desenho e a rolagem
// percorrem a lista em ordem de enum. Nenhuma opcao mudou de lugar e NENHUMA
// CHAVE mudou — trocar a chave faria o ajuste de quem ja usa o app voltar ao
// padrao (ver CHAVE). So o agrupamento mudou.
//
// `icone` e o basename em art/icones. Sao os SVG do app web ja rasterizados,
// nunca forma desenhada a mao (ver gfx_icone).
// `curto` e o nome na COLUNA de categorias, que tem 240 px: "Continuar
// assistindo" nao cabe la em corpo legivel do sofa, e cortar justamente o nome
// da categoria e o pior corte possivel. `titulo` continua sendo o nome inteiro,
// usado no cabecalho da lista, na linha de contexto e na area de ajuda.
static const struct {
  const char *titulo, *curto, *icone;
  int ini, n;
} SECOES[] = {
  { "Reprodução",           "Reprodução", "play",         AJ_QUALIDADE,            6 },
  { "Home",                 "Home",       "menu_home",    AJ_LANDSCAPE,           16 },
  { "Continuar assistindo", "Retomar",    "avancar",      AJ_CW_LIGADO,            8 },
  { "Página de detalhes",   "Detalhes",   "episodios",    AJ_DET_BLUR_NAO_VISTOS,  4 },
  { "Pôsteres e cards",     "Cartazes",   "aspecto",      AJ_EXPANDIR,            14 },
  { "Interface e conta",    "Conta",      "menu_profile", AJ_IDIOMA,              13 },
  { "Integrações",          "Integrações","addon",        AJ_TMDB_LIGADO,         24 },
};
#define AJ_N_SECOES (int)(sizeof SECOES / sizeof *SECOES)

// O QUE A CATEGORIA CONTEM, em uma frase. Aparece na area de ajuda quando o
// foco esta na coluna de categorias: sem ela, escolher categoria e adivinhar
// pelo titulo, e "Pôsteres e cards" nao diz que o efeito de profundidade mora
// ali dentro.
static const char *SECAO_AJUDA[AJ_N_SECOES] = {
  "Qualidade da imagem, formatos de áudio e os idiomas preferidos de legenda e áudio.",
  "Tudo o que a tela inicial mostra: quais fileiras aparecem, em que ordem, a barra lateral e o destaque do topo.",
  "A fileira de retomada: de onde ela vem, que card usa e como ordena o que você deixou pela metade.",
  "A tela de um filme ou série: spoilers dos episódios, botão de trailer e de onde vêm os dados.",
  "A aparência dos cartazes em toda a interface: foco, profundidade, largura e arredondamento.",
  "Idioma da interface, animações, sua conta, addons, serviços conectados e informações do app.",
  "Serviços de metadados: o que o TMDB enriquece na interface e quais fontes de nota o MDBList mostra.",
};

// SUBSECAO: rotula um bloco DENTRO da categoria. Existe porque juntar doze
// categorias em seis deixaria a Home com dezesseis linhas seguidas sem nenhuma
// divisao — trocar "muito menu" por "muita lista" nao e conserto.
//
// Nao e um nivel de navegacao: cima/baixo atravessa o rotulo como atravessa o
// cabecalho da secao. `op` e a PRIMEIRA opcao do bloco. A primeira opcao de uma
// SECAO nunca entra aqui: ela ja e rotulada pelo cabecalho da secao, e os dois
// juntos seriam a mesma informacao duas vezes.
static const struct { int op; const char *titulo; } SUBSECOES[] = {
  { AJ_FIL_LIMITE,   "Fileiras da Home" },
  { AJ_RAIL,         "Barra lateral e destaque" },
  { AJ_ROTULOS,      "O que aparece em cada cartaz" },
  { AJ_PROF,         "Efeito de profundidade" },
  { AJ_LARGURA_DP,   "Tamanho do cartaz" },
  { AJ_PERFIL_ATIVO, "Sua conta" },
  { AJ_VERSAO_I,     "Sobre este app" },
  // A regra "primeira opcao da secao nao leva subsecao" vale quando o rotulo
  // repetiria o da secao. Aqui ele NAO repete — a secao e "Integracoes" e o
  // bloco e "TMDB" — e sem ele as treze linhas do TMDB liam-se como avulsas.
  { AJ_TMDB_LIGADO,  "TMDB" },
  { AJ_MDB_LIGADO,   "MDBList" },
};
#define AJ_N_SUBSECOES (int)(sizeof SUBSECOES / sizeof *SUBSECOES)

static const char *subsecaoDe(int op) {
  int i;
  for (i = 0; i < AJ_N_SUBSECOES; i++)
    if (SUBSECOES[i].op == op) return SUBSECOES[i].titulo;
  return NULL;
}
// Altura extra que o rotulo de subsecao reserva antes da linha `op`.
static float alturaSub(int op) { return subsecaoDe(op) ? AJ_SUB_CABEC : 0.0f; }

// Valor de cada opcao. Para OP_ESCOLHA e o indice; para OP_NUMERO e o proprio
// numero. Os padroes sao os DEFAULTS de layoutPreferences.js, com UMA excecao
// anotada linha a linha: as quatro que o perfil do dono diverge de fabrica
// nascem como ele as deixou, porque e o que ele ve hoje. Todas sao trocaveis
// aqui, que era o ponto.
// Definidas mais abaixo, junto do desenho das linhas; declaradas aqui porque a
// leitura do arquivo e o tratamento de tecla vem antes no arquivo.
static int  nValores(int op);
// Definida junto da leitura do arquivo, bem abaixo; declarada aqui porque o
// setter de "onde o + salva" grava na hora e vem antes dela.
static void gravar(void);
static void aplicarIdioma(int op);
// Segundos restantes antes de refazer a busca de legendas. Declarada aqui, e
// nao junto de aplicarIdioma, porque ajustes_atualizar a le e vem ANTES dela no
// arquivo. Ver o comentario em aplicarIdioma.
static float legendaEspera;

static int valor[AJ_N] = {
  0, 0, 0,          /* qualidade, DV, Atmos */
  0, 0,             /* idioma de legenda e de audio: 0 = seguir a conta */
  0,                /* painel ao pausar: ligado (o default do web) */

  0,                /* posteres deitados: LIGADO (perfil do dono; fabrica: desligado) */
  0,                /* fundo em tela cheia: LIGADO (perfil; fabrica: desligado) */

  FIL_LIMITE_PADRAO,/* limite de fileiras: 7, o pedido do dono (espelho de fileiras.c) */
  0,                /* ordenar fileiras: acao */

  0,                /* barra lateral: recolhida (perfil; fabrica: fixa) */
  1,                /* barra lateral moderna: desligada */
  0,                /* desfoque da barra moderna: ligado (perfil) */
  0,                /* mostrar destaque: ligado */
  0,                /* catalogos do destaque: leitura */
  0,                /* local do descobrir: na busca */
  // DESLIGADO por padrao: o cartaz ja traz o titulo impresso na arte, e repetir
  // o nome logo abaixo e a mesma informacao duas vezes ocupando altura de
  // fileira. Continua sendo ajuste — quem quiser o rotulo liga em Ajustes.
  1,                /* rotulos nos posteres: desligado */
  0,                /* nome do addon: ligado */
  0,                /* tipo de conteudo: ligado */
  1,                /* ocultar nao lancados: desligado */
  0,                /* avaliacoes gerais: mostrar (SHOW_ALL) */
  1,                /* gradiente de foco classico: desligado */

  0,                /* continuar assistindo: ligado */
  0,                /* fonte do continuar: ambas (o comportamento de sempre) */
  0,                /* estilo: card */
  0,                /* miniatura do episodio: ligada */
  1,                /* desfocar proximo: desligado */
  0,                /* proximo do episodio mais alto: ligado */
  0,                /* mostrar nao exibidos: ligado */
  0,                /* ordenacao: padrao */

  1,                /* desfocar nao assistidos: desligado */
  0,                /* botao de trailer: ligado */
  0,                /* metadados externos: ligado */
  0,                /* data completa: ligada */

  0,                /* expandir poster ao focar: ligado (DEFAULT do web) */
  3,                /* atraso: 3s */
  1,                /* navegacao horizontal rapida: desligada (fabrica) */
  0,                /* borda no cartaz em foco: ligada (o foco de sempre) */

  1,                /* efeito de profundidade: desligado (fabrica) */
  28,               /* brilho da borda */
  10,               /* reflexo */
  0,                /* cobertura da borda */
  0, 0, 0, 0, 0,    /* profundidade em posters, cw, episodios, elenco, trailers */

  126,              /* largura do item, dp (fabrica; o perfil do dono usa 120) */
  12,               /* arredondamento, dp */

  // Idioma 1 = English. O padrao NAO e o do dono do pacote: quem instala vem
  // do release publico, e ler uma interface em portugues sem ter escolhido e
  // pior do que ler em ingles sem ter escolhido. Quem prefere portugues troca
  // em Ajustes -> Interface, e a escolha fica gravada.
  1, 0, 0,          /* idioma, animacoes, resolucao (0 = 1080p) */
  // O COMENTARIO ANTIGO AQUI ESTAVA ERRADO, e o erro so nao machucou por sorte.
  // Ele dizia `0, 0, /* versao, espaco */` logo depois do idioma, mas esta
  // lista e POSICIONAL: entre AJ_ANIM e AJ_VERSAO_I existem SETE opcoes de
  // conta (perfil, sync, addons, onde o + salva, trakt, simkl, sair). Aqueles
  // dois zeros caiam em AJ_PERFIL_ATIVO e AJ_SYNC, nao em versao e espaco — e
  // versao e espaco ficavam com o zero da inicializacao parcial, que por acaso
  // e o valor certo para uma linha de leitura. A primeira opcao com padrao
  // DIFERENTE de zero nesta faixa (a de agora) teria caido no lugar errado.
  // Explicitados um a um, e nao contados de cabeca.
  0, 0, 0,          /* perfil, sincronizacao, addons: linhas de leitura/acao */
  1,                /* onde o + salva: watchlist do Trakt (ver V_SALVOS) */
  0, 0, 0,          /* trakt, simkl, sair: acoes */
  0, 0,             /* versao, espaco */

  // Integracoes — TMDB. Tudo LIGADO de fabrica neste app: o enriquecimento por
  // TMDB sempre foi incondicional aqui, e nascer desligado removeria da tela
  // dados que o usuario ja ve (elenco com foto, ficha, trailers). A escolha da
  // conta continua mandando quando a chave chega no blob — este e so o ponto
  // de partida de quem nunca abriu o ajuste em lugar nenhum.
  0,                /* tmdb ligado */
  0,                /* idioma: da interface */
  0, 0, 0, 0, 0, 0, 0, /* arte, basico, ficha, datas, elenco, produtoras, redes */
  0, 0, 0, 0,       /* episodios, trailers, mais como este, colecoes */
  0,                /* enriquecer continuar assistindo */
  // MDBList: idem — a conta que traz a chave, e a consulta sempre correu.
  0,                /* mdblist ligado */
  0,                /* chave: leitura */
  0, 0, 0, 0, 0, 0, 0, 0, /* trakt, imdb, tmdb, letterboxd, tomatoes,
                           audiencia, metacritic, mal */
};

// Pedido de abrir a lista de addons, lido e zerado pelo app.c. A tela nao e
// aberta daqui porque quem troca de tela e o app.c — ajustes.c nao conhece as
// outras telas, e ganhar essa dependencia agora era o comeco de um no.
static int pediuAddons;
int ajustes_pediu_addons(void) { int v = pediuAddons; pediuAddons = 0; return v; }

static int focoOp = 0;
// 1 = o foco esta na COLUNA DE SECOES, e nao na lista de opcoes. Nao ha um
// segundo indice: a secao em foco e a de focoOp (secaoAtual()), entao mover no
// indice move o foco da lista junto e sair do indice nao precisa decidir onde
// pousar.
static int focoIndice = 0;

// --- folha "Ordenar e ativar fileiras" --------------------------------------
// Modal dentro desta tela, e nao uma tela nova: quem troca de tela e o app.c e
// ajustes.c nao conhece as outras telas (a mesma razao registrada em
// ajustes_pediu_addons). Uma folha aqui nao pede nada ao app.c.
#define AJ_FIL_CAMPOS 4          // nome/mover, estado, card, tamanho
static int filAberta;
static int filFoco;
static int filCampo;
// 0 = nada na mao, 1 = fileira na mao, 2 = BLOCO do addon na mao.
// O bloco e o conjunto de fileiras contiguas do mesmo addon — mover o bloco
// inteiro e o "mover de uma vez" que a pessoa pede quando ha 60 fileiras de 5
// addons e ela quer subir "o Xperience" sem subir cada catalogo dele.
static int filPegou;
static int filPegouDe;           // de onde ele saiu, para Voltar cancelar
// MODO EDICAO da lista principal. Antes esquerda/direita trocava o valor da
// linha em foco DIRETO — cada toque de navegacao que errasse a linha mudava
// um ajuste sem a pessoa pedir ("fica estranho, nao intuitivo"). Agora OK
// trava a linha para edicao e so entao as setas ajustam; OK ou Voltar soltam.
static int emEdicao;
static int filTopo;              // primeira linha desenhada (rolagem)
// Uma lista de UMA coluna nao precisa do focus.h: a memoria de coluna que ele
// existe para resolver nao tem o que lembrar aqui, e o indice cru deixa o
// "pula o cabecalho da secao" ser uma soma em vez de um mapa de fileiras.
static float animFoco[AJ_N];
static float scrollY = 0.0f;
static int sair = 0;

// Quantos catalogos o destaque usa. 0 = todos, que e o que o web escreve como
// "Todos" quando heroCatalogKeys esta vazio — e o caso do perfil do dono.
static int heroCatalogos = 0;

static int lig(int op)  { return valor[op] == 0; }

int ajustes_animacoes_reduzidas(void) { return valor[AJ_ANIM] == 1; }
// Lido UMA vez, na criacao da janela, antes de qualquer desenho: trocar isto
// com o app aberto nao redimensiona a superficie. Ver main.c.
int ajustes_4k(void)                  { return valor[AJ_RESOLUCAO] == 1; }
int ajustes_dolby_vision(void)        { return lig(AJ_DV); }
int ajustes_dolby_atmos(void)         { return lig(AJ_ATMOS); }
int ajustes_pausa_overlay(void)       { return lig(AJ_PAUSA_OVERLAY); }
int ajustes_idioma_ingles(void)       { return valor[AJ_IDIOMA] == 1; }

// Cor do ANEL DE FOCO. Ver TEMA_ACENTO: um tema aqui e so isto.
void ajustes_acento(float *r, float *g, float *b) {
  int i = valor[AJ_TEMA];
  if (i < 0 || i >= AJ_N_TEMAS) i = 0;   // arquivo de outra versao: branco
  if (r) *r = TEMA_ACENTO[i].r;
  if (g) *g = TEMA_ACENTO[i].g;
  if (b) *b = TEMA_ACENTO[i].b;
}

// `collapseSidebar: modernSidebar ? false : Boolean(collapseSidebar)` — a barra
// moderna DESLIGA o recolhimento, e nao o contrario. Copiado de
// normalizeLayoutPreferences para nao inventar precedencia.
int ajustes_rail_moderna(void)        { return lig(AJ_RAIL_MODERNA); }
int ajustes_rail_recolhida(void)      { return ajustes_rail_moderna() ? 0 : lig(AJ_RAIL); }
int ajustes_rail_moderna_blur(void)   { return lig(AJ_RAIL_BLUR); }
int ajustes_hero_ligado(void)         { return lig(AJ_HERO); }
int ajustes_hero_cheio(void)          { return lig(AJ_HERO_CHEIO); }
int ajustes_posteres_deitados(void)   { return lig(AJ_LANDSCAPE); }
int ajustes_gradiente_foco_classico(void) { return lig(AJ_GRAD_CLASSICO); }

int ajustes_rotulos_poster(void)      { return lig(AJ_ROTULOS); }
int ajustes_nome_addon(void)          { return lig(AJ_NOME_ADDON); }
int ajustes_sufixo_tipo(void)         { return lig(AJ_SUFIXO_TIPO); }
int ajustes_ocultar_nao_lancados(void){ return lig(AJ_OCULTAR_NLANC); }
// O blob da ordem de catalogos (plataforma home_catalog_shared) traz esta mesma
// opcao, separada do blob de ajustes do perfil. Setter proprio porque so vale
// quando a chave EXISTE la: ausente nao e `false`, e "mantem o que esta na TV".
void ajustes_definir_ocultar_nao_lancados(int ligado) {
  valor[AJ_OCULTAR_NLANC] = ligado ? 0 : 1;
}
// 1 = o "+" tambem publica na watchlist do Trakt. A lista LOCAL e escrita nos
// dois casos; ver a nota de V_SALVOS e a de abertura de salvos.h.
int ajustes_salvos_no_trakt(void)     { return valor[AJ_SALVOS_DEST] == 1; }
// Setter para o explicador de primeira vez (salvosintro.c), que faz esta
// pergunta antes de a pessoa chegar em Ajustes. Grava na hora: quem respondeu e
// desligou a TV nao deve ser perguntado de novo.
void ajustes_definir_salvos_no_trakt(int noTrakt) {
  valor[AJ_SALVOS_DEST] = noTrakt ? 1 : 0;
  gravar();
}
int ajustes_data_completa(void)       { return lig(AJ_DET_DATA_CHEIA); }
int ajustes_notas_home(void)          { return valor[AJ_NOTAS_HOME] == 0; }
int ajustes_local_descobrir(void)     { return valor[AJ_DESCOBRIR]; }
int ajustes_descobrir_na_busca(void)  { return valor[AJ_DESCOBRIR] == 0; }

int ajustes_cw_ligado(void)           { return lig(AJ_CW_LIGADO); }
int ajustes_cw_estilo(void)           { return valor[AJ_CW_ESTILO]; }
int ajustes_cw_fonte(void)            { return valor[AJ_CW_FONTE]; }
int ajustes_cw_thumb_episodio(void)   { return lig(AJ_CW_THUMB); }
int ajustes_cw_desfocar_proximo(void) { return lig(AJ_CW_BLUR_PROX); }
int ajustes_cw_do_episodio_mais_alto(void) { return lig(AJ_CW_FURTHEST); }
int ajustes_cw_mostrar_nao_exibidos(void)  { return lig(AJ_CW_NAO_EXIBIDOS); }
int ajustes_cw_ordem(void)            { return valor[AJ_CW_ORDEM]; }

int ajustes_desfocar_nao_assistidos(void) { return lig(AJ_DET_BLUR_NAO_VISTOS); }
int ajustes_botao_trailer(void)       { return lig(AJ_DET_TRAILER); }
int ajustes_meta_externo(void)        { return lig(AJ_DET_META_EXT); }

int   ajustes_expandir_poster(void)   { return lig(AJ_EXPANDIR); }
float ajustes_expandir_poster_atraso(void) { return (float)valor[AJ_EXPANDIR_ATRASO]; }
int   ajustes_navegacao_horizontal_rapida(void) { return lig(AJ_NAV_RAPIDA); }
int   ajustes_borda_foco(void) { return lig(AJ_BORDA_FOCO); }

int   ajustes_profundidade(void)      { return lig(AJ_PROF); }
float ajustes_profundidade_borda(void)     { return valor[AJ_PROF_BORDA] / 100.0f; }
float ajustes_profundidade_brilho(void)    { return valor[AJ_PROF_BRILHO] / 100.0f; }
float ajustes_profundidade_cobertura(void) { return valor[AJ_PROF_COBERTURA] / 100.0f; }
int   ajustes_profundidade_posters(void)   { return lig(AJ_PROF_POSTERS); }
int   ajustes_profundidade_cw(void)        { return lig(AJ_PROF_CW); }
int   ajustes_profundidade_episodios(void) { return lig(AJ_PROF_EPS); }
int   ajustes_profundidade_elenco(void)    { return lig(AJ_PROF_ELENCO); }
int   ajustes_profundidade_trailers(void)  { return lig(AJ_PROF_TRAILERS); }

int   ajustes_largura_poster_dp(void) { return valor[AJ_LARGURA_DP]; }
int   ajustes_raio_poster_dp(void)    { return valor[AJ_RAIO_DP]; }
// dpToPx = 2 em buildModernHomeSizingStyle. 12dp -> 24px, que e o raio medido.
float ajustes_raio_poster_px(void)    { return (float)valor[AJ_RAIO_DP] * 2.0f; }

// A regra do web, e nao dois layouts: o conteudo tem sempre 104 de recuo e a
// rail acrescenta os 144 dela quando esta fixa.
float ajustes_conteudo_x(void) {
  return ajustes_rail_recolhida() ? NV_CONTENT_PAD
                                  : NV_LEGACY_RAIL_W + NV_CONTENT_PAD;
}
const char *ajustes_qualidade(void)   { return V_QUALIDADE[valor[AJ_QUALIDADE]]; }

// --- Integracoes ------------------------------------------------------------
//
// TMDB: cada sub-toggle vale sozinho, mas o CONSUMIDOR so deve ler
// `ajustes_tmdb_*` DEPOIS do portao — `desc_chave_tmdb()` devolve "" quando
// ajustes_tmdb_ligado() e 0, e sem chave nenhum pedido ao TMDB sai. Ainda
// assim os acessores ja retornam 0 com o master desligado, para quem os usar
// nao precisar lembrar da segunda pergunta.
int ajustes_tmdb_ligado(void)         { return lig(AJ_TMDB_LIGADO); }
// Codigo no formato da API do TMDB ("pt-BR", "en-US"). "Da interface" (0)
// segue o idioma do app, que e o comportamento que desc_tmdb_idioma() sempre
// teve.
const char *ajustes_tmdb_idioma(void) {
  static const char *L[] = {
    NULL, "pt-BR", "en-US", "es-ES", "fr-FR", "de-DE", "it-IT", "pt-PT",
    "ja-JP", "ko-KR", "zh-CN"
  };
  int v = valor[AJ_TMDB_IDIOMA];
  if (v < 0 || v >= (int)(sizeof L / sizeof *L)) v = 0;
  // "Da interface" resolve AQUI, na hora de perguntar, e nao na gravacao:
  // trocar o idioma do app tem de refletir sem tocar neste ajuste.
  if (!L[v]) return ajustes_idioma_ingles() ? "en-US" : "pt-BR";
  return L[v];
}
#define TMDB_USA(op) (lig(AJ_TMDB_LIGADO) && lig(op))
int ajustes_tmdb_arte(void)           { return TMDB_USA(AJ_TMDB_ARTE); }
int ajustes_tmdb_basico(void)         { return TMDB_USA(AJ_TMDB_BASICO); }
int ajustes_tmdb_ficha(void)          { return TMDB_USA(AJ_TMDB_FICHA); }
int ajustes_tmdb_datas(void)          { return TMDB_USA(AJ_TMDB_DATAS); }
int ajustes_tmdb_elenco(void)         { return TMDB_USA(AJ_TMDB_ELENCO); }
int ajustes_tmdb_prod(void)           { return TMDB_USA(AJ_TMDB_PROD); }
int ajustes_tmdb_redes(void)          { return TMDB_USA(AJ_TMDB_REDES); }
int ajustes_tmdb_eps(void)            { return TMDB_USA(AJ_TMDB_EPS); }
int ajustes_tmdb_trailers(void)       { return TMDB_USA(AJ_TMDB_TRAILERS); }
int ajustes_tmdb_mais(void)           { return TMDB_USA(AJ_TMDB_MAIS); }
int ajustes_tmdb_col(void)            { return TMDB_USA(AJ_TMDB_COL); }
int ajustes_tmdb_cw(void)             { return TMDB_USA(AJ_TMDB_CW); }

int ajustes_mdblist_ligado(void)      { return lig(AJ_MDB_LIGADO); }
// `fonte` e um ExFonte de extras.h (a ordem dele, nao a das linhas aqui).
// NAO combina com o master de proposito: o master corta a CONSULTA ao mdbList,
// e as notas Trakt/IMDb que o app tem por conta propria (sem chave nenhuma)
// nao sao dados do mdbList — esconde-las junto seria punir o usuario pelo que
// outro servico faz. Cada show_* continua valendo sobre a sua fonte. MAL nao
// tem fonte no extras de hoje — o ajuste fica gravado a espera dela.
int ajustes_mdblist_fonte(int fonte) {
  static const int OP[] = {
    AJ_MDB_TRAKT, AJ_MDB_IMDB, AJ_MDB_TMDB, AJ_MDB_TOMATES,
    AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_LETTER
  };
  if (fonte < 0 || fonte >= (int)(sizeof OP / sizeof *OP)) return 0;
  return lig(OP[fonte]);
}

// Onde os ajustes ficam. Ate a versao anterior nada era gravado: mexer numa
// opcao valia so enquanto o app estivesse aberto, e voltar depois mostrava tudo
// no padrao — o que faz a tela inteira parecer decorativa.
static char dirAjustes[512];


// Valores LITERAIS que o app web grava nas opcoes que nao sao booleanas. A
// ordem casa, uma a uma, com a do vetor de rotulos correspondente — e essa
// correspondencia e o contrato: mexer num vetor sem mexer no outro troca o
// ajuste da pessoa em silencio. Todos conferidos no codigo do app web.
static const char *W_DESCOBRIR[] = { "in_search", "in_sidebar", "off", NULL };
static const char *W_NOTAS[]     = { "SHOW_ALL", "HIDE_ALL", NULL };
static const char *W_CW[]        = { "card", "wide", "poster", NULL };
static const char *W_CW_ORDEM[]  = { "default", "streaming_style", "split_upcoming", NULL };
// `tmdb_language` chega da conta ja cortado na BASE ("pt", "en" — ver
// normalizeTmdbLanguageForAndroid no web). Posicional com V_TMDB_LING: "pt"
// vira Portugues (Brasil), e pt-PT e inalcancavel pelo blob — fica como
// escolha local apenas. O indice 0 e um sentinela: a conta sempre manda um
// idioma de verdade, e um idioma que a lista nao tem (digamos "nl") mantem o
// valor atual em vez de inventar um.
static const char *W_TMDB_LING[] = {
  "interface", "pt", "en", "es", "fr", "de", "it", "pt-pt", "ja", "ko", "zh",
  NULL
};

// `heroSectionEnabled` -> `hero_section_enabled`. Uma sequencia de maiusculas
// conta como uma palavra so (`homeImdbRatingsVisibility` ->
// `home_imdb_ratings_visibility`, e nao `home_i_m_d_b_...`).
static void camelParaSnake(const char *src, char *dst, size_t tam) {
  size_t w = 0;
  int i;
  for (i = 0; src[i] && w + 2 < tam; i++) {
    int alto = src[i] >= 'A' && src[i] <= 'Z';
    if (alto && w > 0) {
      int anteriorBaixo = src[i - 1] >= 'a' && src[i - 1] <= 'z';
      int anteriorDigito = src[i - 1] >= '0' && src[i - 1] <= '9';
      int proximoBaixo = src[i + 1] >= 'a' && src[i + 1] <= 'z';
      if (anteriorBaixo || anteriorDigito || proximoBaixo) dst[w++] = '_';
    }
    dst[w++] = alto ? (char)(src[i] - 'A' + 'a') : src[i];
  }
  dst[w] = 0;
}

static int igualSemCaixa(const char *a, const char *b) {
  for (; *a && *b; a++, b++) {
    char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
    char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
    if (x != y) return 1;
  }
  return *a || *b;   // 0 quando iguais, como strcmp
}

static const char *const *literaisDe(int op) {
  switch (op) {
    case AJ_DESCOBRIR:  return W_DESCOBRIR;
    case AJ_NOTAS_HOME: return W_NOTAS;
    case AJ_CW_FONTE:   return W_CW;
    case AJ_CW_ESTILO:  return W_CW;
    case AJ_CW_ORDEM:   return W_CW_ORDEM;
    case AJ_TMDB_IDIOMA: return W_TMDB_LING;
    case AJ_TEMA:       return W_TEMA;
    default:            return NULL;
  }
}

static int limita(int op, int v) {
  const Opcao *o = &OPCOES[op];
  if (o->tipo == OP_ESCOLHA) return (v >= 0 && v < nValores(op)) ? v : valor[op];
  if (o->tipo == OP_NUMERO)  return v < o->min ? o->min : (v > o->max ? o->max : v);
  return valor[op];
}

void ajustes_dir(const char *dir) {
  FILE *f;
  char caminho[600], linha[96];
  if (!dir || !*dir) return;
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dirAjustes);
  f = fopen(caminho, "r");
  if (!f) return;
  while (fgets(linha, sizeof linha, f)) {
    char chave[64]; int v, i;
    if (sscanf(linha, "%63s %d", chave, &v) != 2) continue;
    for (i = 0; i < AJ_N; i++) {
      if (!CHAVE[i] || strcmp(CHAVE[i], chave)) continue;
      if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
      // Valor fora da faixa (arquivo de outra versao, ou editado a mao) cai no
      // padrao em vez de indexar fora do vetor.
      valor[i] = limita(i, v);
      break;
    }
  }
  fclose(f);
  // O limite mora em fileiras.c; esta linha e so o espelho dele. Ler daqui em
  // vez de gravar evita a divergencia: o arquivo de ajustes nao guarda o
  // numero, entao nao ha como os dois discordarem.
  valor[AJ_FIL_LIMITE] = fil_limite();
  // A escolha lida do disco so existe de verdade quando chega em linguas.c.
  rotulosDeIdioma();
  aplicarIdioma(AJ_LEG_LINGUA);
  aplicarIdioma(AJ_AUD_LINGUA);
}

static void gravar(void) {
  char caminho[600], tmp[600];
  FILE *f;
  int i;
  if (!dirAjustes[0]) return;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dirAjustes);
  snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dirAjustes);
  f = fopen(tmp, "w");
  if (!f) return;
  for (i = 0; i < AJ_N; i++) {
    // "-" marca linha local (versao, espaco, conta): nao tem valor para
    // guardar. Acao tambem nao. E chave ausente NUNCA vai para o arquivo — foi
    // um "(null) 0" gravado assim que derrubou o app na leitura seguinte.
    if (!CHAVE[i] || CHAVE[i][0] == '-') continue;
    if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
    fprintf(f, "%s %d\n", CHAVE[i], valor[i]);
  }
  fclose(f);
  rename(tmp, caminho);
}


// Idiomas de audio e legenda do blob. NAO passam pelo laco das opcoes abaixo
// porque o valor deles nao e um indice de enum, e um codigo ISO ("en", "pt") —
// e porque as sentinelas do web ("DEVICE", "none", "off") precisam virar
// "sem filtro" em vez de virar um idioma inventado. Ver linguas.h.
//
// MEDIDO no app web (profileSettingsSyncService.js:1066): as quatro chaves
// vivem sob `player_settings`, ja em snake_case, como o resto do blob.
static void idiomasDoBlob(const char *json, const char *fim) {
  static const struct { const char *chave; void (*aplica)(const char *); } M[] = {
    { "subtitle_preferred_language",         ling_conta_legenda  },
    { "subtitle_secondary_language",         ling_conta_legenda2 },
    { "preferred_audio_language",            ling_conta_audio    },
  };
  size_t k;
  for (k = 0; k < sizeof M / sizeof *M; k++) {
    char bruto[80], texto[80];
    size_t n;
    if (!js_bruto(json, fim, M[k].chave, bruto, sizeof bruto)) continue;
    if (bruto[0] == '{' &&
        !js_bruto(bruto, bruto + strlen(bruto), "value", texto, sizeof texto))
      continue;
    if (bruto[0] != '{') snprintf(texto, sizeof texto, "%s", bruto);
    n = strlen(texto);
    if (n >= 2 && texto[0] == '"') { memmove(texto, texto + 1, n - 2); texto[n - 2] = 0; }
    else if (!strcmp(texto, "null")) texto[0] = 0;
    M[k].aplica(texto);
  }
  printf("[ajustes] idiomas da conta: legenda=\"%s\" audio=\"%s\"\n",
         ling_legenda(), ling_audio());
  fflush(stdout);
}

int ajustes_aplicar_blob(const char *json) {
  const char *fim;
  int i, mudou = 0, reconhecidas = 0;
  if (!json || !*json) return 0;
  fim = json + strlen(json);
  idiomasDoBlob(json, fim);

  for (i = 0; i < AJ_N; i++) {
    char snake[80], embrulho[400], bruto[160];
    int novo;
    // Linha de leitura/acao nao tem valor; chave com "-" e marcador local
    // (heroCatalogKeys, versao, espaco) e nao vem do blob.
    if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
    if (!CHAVE[i] || CHAVE[i][0] == '-') continue;
    // MEDIDO na TV, com uma conta de verdade: o blob NAO e um mapa plano de
    // camelCase. Ele e
    //   {"version":1,"features":{"layout_settings":{
    //      "hero_section_enabled":{"type":"boolean","value":true}, ...}}}
    // — chave em snake_case, aninhada por "feature", e o valor EMBRULHADO num
    // objeto com tipo. Procurando por `heroSectionEnabled` o app achava zero
    // chaves em 12 KB de ajustes e nao aplicava nada, sem erro nenhum.
    //
    // A busca por nome ignora o aninhamento de proposito: js_bruto varre o
    // texto inteiro, e os nomes destas chaves sao unicos no documento.
    camelParaSnake(CHAVE[i], snake, sizeof snake);
    if (!js_bruto(json, fim, snake, embrulho, sizeof embrulho) &&
        !js_bruto(json, fim, CHAVE[i], embrulho, sizeof embrulho)) continue;
    if (embrulho[0] == '{') {
      // Desembrulha {"type":...,"value":X}. O `type` vem antes do `value` no
      // codificador do web, entao a primeira chave "value" e a certa.
      if (!js_bruto(embrulho, embrulho + strlen(embrulho), "value",
                    bruto, sizeof bruto)) continue;
    } else {
      snprintf(bruto, sizeof bruto, "%s", embrulho);
    }

    if (!strcmp(bruto, "true") || !strcmp(bruto, "false")) {
      // O primeiro rotulo de V_LIGA e "Ligado" e o de V_RAIL e "Recolhida" —
      // nos dois, o indice 0 e o `true` do web. Coincidencia util, mas
      // coincidencia: se um vetor novo comecar pelo estado desligado, ele
      // precisa de literais proprios em literaisDe().
      novo = !strcmp(bruto, "true") ? 0 : 1;
    } else if (bruto[0] == '"') {
      const char *const *lit = literaisDe(i);
      char texto[128];
      size_t n = strlen(bruto);
      if (n < 2) continue;
      if (n - 2 >= sizeof texto) continue;
      memcpy(texto, bruto + 1, n - 2);
      texto[n - 2] = 0;
      novo = -1;
      // Comparacao SEM CAIXA. MEDIDO na TV: o servidor guarda estes enums em
      // MAIUSCULA ("IN_SEARCH", "CARD", "DEFAULT") enquanto o codigo JS do app
      // web os escreve em minuscula. Ler so o codigo do web levava a rejeitar
      // o valor de verdade — e a rejeicao era CORRETA (melhor manter que
      // inventar), mas o efeito era o ajuste nunca chegar.
      if (lit) { int k; for (k = 0; lit[k]; k++) if (!igualSemCaixa(lit[k], texto)) { novo = k; break; } }
      if (novo < 0) {
        // Valor que este app nao conhece (versao nova do web, opcao nova).
        // Manter o que esta e a resposta certa: escolher um padrao aqui
        // inventaria uma preferencia que a pessoa nunca marcou.
        printf("[ajustes] %s=\"%s\" nao reconhecido; mantido\n", CHAVE[i], texto);
        continue;
      }
    } else if ((bruto[0] >= '0' && bruto[0] <= '9') || bruto[0] == '-' || bruto[0] == '.') {
      novo = (int)(atof(bruto) + 0.5);
    } else {
      continue;   // null, objeto, array: nao ha o que aplicar
    }

    reconhecidas++;
    novo = limita(i, novo);
    if (novo != valor[i]) { valor[i] = novo; mudou++; }
  }

  if (mudou) gravar();   // o que veio da conta tem de sobreviver ao arranque
  // Registra SEMPRE, inclusive zero. "Nenhuma linha no log" tem duas leituras
  // opostas — o blob nao foi aplicado, ou foi aplicado e ja estava tudo igual —
  // e sem o numero nao da para saber qual. Foi exatamente a duvida que sobrou
  // na primeira verificacao na TV.
  printf("[ajustes] blob da conta: %d chave(s) reconhecida(s), %d mudou(aram)\n",
         reconhecidas, mudou);
  return mudou;
}

// AS SECOES TEM DE LADRILHAR O ENUM INTEIRO, sem buraco e sem sobreposicao.
// Nao da para exigir isso do compilador (seria somar `n` a mao num
// _Static_assert, ou seja, uma terceira copia dos mesmos numeros para
// dessincronizar), entao a conferencia e no arranque e GRITA no log.
//
// O que o erro custaria sem ela: uma opcao fora de toda secao simplesmente NAO
// E DESENHADA — o laco de desenho percorre as secoes, nao o enum — e ainda
// assim continua respondendo a cima/baixo, porque a navegacao percorre o enum.
// O sintoma na TV seria o foco sumir num vao invisivel da lista.
static void conferirSecoes(void) {
  int s, esperado = 0;
  for (s = 0; s < AJ_N_SECOES; s++) {
    if (SECOES[s].ini != esperado)
      printf("[ajustes] SECOES[%d] \"%s\" comeca em %d, esperado %d\n",
             s, SECOES[s].titulo, SECOES[s].ini, esperado);
    esperado = SECOES[s].ini + SECOES[s].n;
  }
  if (esperado != AJ_N)
    printf("[ajustes] SECOES cobre %d de %d opcoes: %d linha(s) nao seriam "
           "desenhadas\n", esperado, (int)AJ_N, (int)AJ_N - esperado);
  fflush(stdout);
}

int ajustes_iniciar(void) {
  conferirSecoes();
  focoOp = 0; scrollY = 0.0f; sair = 0;
  focoIndice = 0;
  filAberta = 0; filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
  emEdicao = 0;
  valor[AJ_FIL_LIMITE] = fil_limite();
  // Tambem aqui, e nao so em ajustes_dir: sem arquivo de ajustes aquele caminho
  // volta cedo e os rotulos ficariam vazios na primeira abertura da tela.
  rotulosDeIdioma();
  return 1;
}
void ajustes_encerrar(void) { }
int ajustes_quer_sair(void) { return sair; }

// Valor das linhas so de leitura. O espaco em disco NAO e um numero inventado:
// vem do cache de texturas, que e exatamente o que "imagens" consome no
// aparelho — um numero fixo aqui seria mentira e nunca mudaria.
static const char *textoLeitura(int op) {
  static char buf[64];
  if (op == AJ_VERSAO_I) {
    // Com release mais nova no GitHub, a linha diz as duas.
    if (atualizacao_nova()[0]) {
      snprintf(buf, sizeof buf, i18n("%s · nova: %s"), AJ_VERSAO, atualizacao_nova());
      return buf;
    }
    return AJ_VERSAO;
  }
  if (op == AJ_PERFIL_ATIVO) {
    static char bufp[80];
    int i;
    for (i = 0; i < perfis_n(); i++)
      if (perfis_item(i)->indice == perfis_ativo()) return perfis_item(i)->nome;
    // Sem lista de perfis, dizer "Perfil 1" e mais honesto que deixar vazio: e
    // literalmente o que o app esta usando em p_profile_id.
    snprintf(bufp, sizeof bufp, i18n("Perfil %d"), perfis_ativo());
    return bufp;
  }
  // TODO VALOR DAQUI VAI PARA A TELA, ENTAO TODO VALOR PASSA POR i18n().
  //
  // Estes voltavam CRUS e a varredura nao os via: ela olha o literal entregue a
  // uma funcao de DESENHO, e aqui o literal e devolvido por um `return` — quem
  // desenha recebe um `const char *` e nao tem como saber de onde veio. Doze
  // literais atravessaram assim, e o relator do #23 os leu na TV em ingles:
  // "conectado" e "conectar" nas linhas do Trakt e do Simkl.
  //
  // Regra para quem editar esta funcao: se o texto aparece na lista de Ajustes,
  // ele e interface. Nao ha valor "tecnico demais para traduzir" aqui.
  if (op == AJ_SYNC) {
    switch (sync_estado()) {
      case SYNC_RODANDO: return i18n("sincronizando…");
      case SYNC_FALHOU:  return i18n("falhou");
      case SYNC_PRONTO:  return sync_resumo();
      default:           return sessao_logada() ? i18n("aguardando") : i18n("sem conta");
    }
  }
  if (op == AJ_TRAKT) {
    switch (traktauth_estado()) {
      case TRA_LIGADO:     return i18n("conectado");
      case TRA_PEDINDO:    return i18n("preparando…");
      case TRA_AGUARDANDO: return i18n("aguardando");
      case TRA_ERRO:       return i18n("falhou");
      case TRA_INVALIDO:   return i18n("expirou — reconectar");
      default:             return i18n("conectar");
    }
  }
  if (op == AJ_SIMKL) {
    switch (simklauth_estado()) {
      case SMK_LIGADO:     return i18n("conectado");
      case SMK_PEDINDO:    return i18n("preparando…");
      case SMK_AGUARDANDO: return i18n("aguardando");
      case SMK_ERRO:       return i18n("falhou");
      default:             return i18n("conectar");
    }
  }
  if (op == AJ_ADDONS) {
    int i, lig = 0, n = addons_n();
    for (i = 0; i < n; i++) if (addons_ativo(i)) lig++;
    snprintf(buf, sizeof buf, i18n("%d de %d"), lig, n);
    return buf;
  }
  if (op == AJ_SAIR) return "OK";   /* igual nos dois idiomas */
  if (op == AJ_MDB_CHAVE) {
    // A chave chega pela CONTA (sync.c -> extras_definir_chave) ou pelo
    // arquivo art/mdblist.txt. Mostra so o estado, nunca os caracteres — a
    // linha e de leitura justamente porque nao ha teclado nesta tela.
    return extras_mdblist_tem_chave() ? i18n("definida") : i18n("ausente");
  }
  if (op == AJ_HERO_CATALOGOS) {
    // "Todos" com a lista vazia e o que o web escreve (common_all), e e o estado
    // do perfil do dono. Um "0" ali leria como "nenhum", o oposto do que e.
    if (heroCatalogos <= 0) return i18n("Todos");
    snprintf(buf, sizeof buf, "%d", heroCatalogos);
    return buf;
  }
  int itens = 0, pend = 0; long bytes = 0;
  tex_estatisticas(&itens, &pend, &bytes, NULL, NULL);
  snprintf(buf, sizeof buf, i18n("%.1f MB em %d imagens"), bytes / 1048576.0, itens);
  return buf;
}

// Uma opcao pode ficar INATIVA por causa de outra — o web esconde a linha
// (`model.layout.modernSidebar ? "" : renderToggleRow(...)`), mas esconder num
// D-pad muda a contagem de linhas embaixo do dedo do usuario a cada toque. Aqui
// ela continua no lugar, apagada e sem setas: a dependencia fica visivel em vez
// de a linha sumir.
static int inativa(int op) {
  switch (op) {
    case AJ_RAIL:         return ajustes_rail_moderna();
    case AJ_RAIL_BLUR:    return !ajustes_rail_moderna();
    case AJ_HERO_CATALOGOS: return !ajustes_hero_ligado();
    case AJ_CW_FONTE:
    case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_FURTHEST:
    case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM:
      return !ajustes_cw_ligado();
    case AJ_CW_BLUR_PROX: return !ajustes_cw_ligado() || !ajustes_cw_thumb_episodio();
    case AJ_EXPANDIR_ATRASO: return !ajustes_expandir_poster();
    case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA:
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return !ajustes_profundidade();
    // Integracoes: cada recurso depende do master da sua integracao, como o
    // `disabled: !enabled` das linhas do web.
    case AJ_TMDB_IDIOMA: case AJ_TMDB_ARTE: case AJ_TMDB_FICHA:
    case AJ_TMDB_DATAS: case AJ_TMDB_ELENCO: case AJ_TMDB_PROD:
    case AJ_TMDB_REDES: case AJ_TMDB_EPS: case AJ_TMDB_TRAILERS:
    case AJ_TMDB_MAIS: case AJ_TMDB_COL: case AJ_TMDB_CW:
      return !ajustes_tmdb_ligado();
    case AJ_MDB_CHAVE:
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB:
    case AJ_MDB_LETTER: case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA:
    case AJ_MDB_META: case AJ_MDB_MAL:
      return !ajustes_mdblist_ligado();
    default: return 0;
  }
}

// Acao NAO e leitura (tem o destaque de linha ativa), mas tambem NAO e mutavel
// (esquerda/direita nao fazem nada nela). As duas respostas sao diferentes de
// proposito, e e por isso que sao duas funcoes.
static int soLeitura(int op) { return OPCOES[op].tipo == OP_LEITURA; }
static int mutavel(int op)   { return OPCOES[op].tipo != OP_LEITURA &&
                                      OPCOES[op].tipo != OP_ACAO && !inativa(op); }

static int secaoAtual(void) {
  for (int s = 0; s < AJ_N_SECOES; s++)
    if (focoOp < SECOES[s].ini + SECOES[s].n) return s;
  return AJ_N_SECOES - 1;
}

// UMA FRASE POR OPCAO, sem excecao.
//
// O relato foi "tem pouca informacao". Ele nao era impressao: das 58 linhas,
// quinze tinham frase propria e as OUTRAS QUARENTA E TRES caiam num texto
// generico ("Use as setas laterais para escolher") que nao diz o que a opcao
// faz — ou seja, a area de ajuda ocupava um terco da tela para nao informar
// nada em tres de cada quatro linhas. O `default` continua existindo como rede
// de seguranca para opcao nova, mas nenhuma opcao de hoje cai nele.
//
// A frase responde "o que isto E". O que MUDA na pratica vai em efeitoOpcao,
// separado de proposito: as duas perguntas sao diferentes e juntas viram um
// paragrafo que ninguem le do sofa.
static const char *ajudaOpcao(int op) {
  if (inativa(op)) {
    if (op == AJ_RAIL) return "Desative a barra lateral moderna para escolher entre recolhida e fixa.";
    if (op == AJ_RAIL_BLUR) return "Ative a barra lateral moderna para usar o desfoque.";
    if (op == AJ_HERO_CATALOGOS) return "Ative Mostrar destaque para exibir os catálogos no topo da Home.";
    if (op >= AJ_CW_FONTE && op <= AJ_CW_ORDEM)
      return op == AJ_CW_BLUR_PROX && ajustes_cw_ligado()
        ? "Ative Miniatura do episódio para desfocar a imagem do próximo episódio."
        : "Ative Continuar assistindo para ajustar os cards de retomada.";
    if (op == AJ_EXPANDIR_ATRASO) return "Ative Expandir pôster ao focar para ajustar o tempo de espera.";
    if (op > AJ_TMDB_LIGADO && op <= AJ_TMDB_CW)
      return "Ative TMDB para ajustar o que ele enriquece.";
    if (op > AJ_MDB_LIGADO && op <= AJ_MDB_MAL)
      return "Ative MDBList para escolher as fontes de nota.";
    return "Ative Efeito de profundidade para personalizar este detalhe.";
  }
  switch (op) {
    // --- Reproducao
    case AJ_QUALIDADE: return "Define a preferência de resolução. A disponibilidade depende das fontes do addon.";
    case AJ_DV: case AJ_ATMOS: return "Preferência para fontes compatíveis. O formato disponível também depende do arquivo e da TV.";
    case AJ_LEG_LINGUA: return "Idioma procurado primeiro na lista de legendas de cada título. \"Da conta\" segue o que está no seu perfil.";
    case AJ_AUD_LINGUA: return "Faixa de áudio escolhida quando o arquivo tem mais de uma. Se o idioma não existir no arquivo, o player usa a primeira.";
    case AJ_PAUSA_OVERLAY: return "Ao pausar, sobe uma ficha com a sinopse e os dados do que você está vendo.";

    // --- Home
    case AJ_LANDSCAPE: return "Usa a arte deitada (16:9) no lugar do cartaz em pé nas fileiras que têm as duas.";
    case AJ_HERO_CHEIO: return "O destaque do topo ocupa a tela inteira atrás das fileiras, em vez de ficar num bloco.";
    case AJ_FIL_LIMITE: return "Quantas fileiras a Home monta. Menos fileiras também significam menos catálogos pedidos pela rede, e não fileiras invisíveis.";
    case AJ_FIL_ORDEM: return "Abre a lista de fileiras para reordenar, ligar, desligar e escolher o card de cada uma. É lá que dá para ver de onde cada fileira vem.";
    case AJ_RAIL: return "A barra de navegação da esquerda fica sempre aberta, ou recolhida até você ir até ela.";
    case AJ_RAIL_MODERNA: return "Troca a barra lateral pela versão nova, com ícones maiores. Ela ignora a escolha entre recolhida e fixa.";
    case AJ_RAIL_BLUR: return "Desfoca a arte atrás da barra lateral moderna em vez de usar um fundo sólido.";
    case AJ_HERO: return "O bloco grande no topo da Home, com a arte e o nome de um título em destaque.";
    case AJ_HERO_CATALOGOS: return "Quantidade de catálogos incluídos no destaque. Esta linha é apenas informativa.";
    case AJ_DESCOBRIR: return "Onde fica a tela Descobrir: junto da Busca, como item próprio na barra lateral, ou em lugar nenhum.";
    case AJ_ROTULOS: return "Escreve o nome do título abaixo do cartaz. A maior parte da arte já traz o nome impresso.";
    case AJ_NOME_ADDON: return "Acrescenta o nome do addon ao título da fileira, para separar dois catálogos com o mesmo nome.";
    case AJ_SUFIXO_TIPO: return "Acrescenta \"Filme\" ou \"Série\" ao título da fileira, para separar as duas versões do mesmo catálogo.";
    case AJ_OCULTAR_NLANC: return "Esconde das fileiras o que ainda não estreou. Título sem fonte nenhuma ocupa lugar e não abre.";
    case AJ_NOTAS_HOME: return "Mostra a nota do IMDb no canto dos cartazes da Home.";
    case AJ_GRAD_CLASSICO: return "Volta ao degradê antigo sob o cartaz em foco, no lugar do realce atual.";

    // --- Continuar assistindo
    case AJ_CW_LIGADO: return "A fileira de retomada, com o que você deixou pela metade e o próximo episódio das séries que acompanha.";
    case AJ_CW_FONTE: return "De onde vem a fileira de retomada. \"Ambas\" usa a conta Nuvio e completa com o Trakt.";
    case AJ_CW_ESTILO: return "A forma do card da retomada: quadrado com a arte, deitado largo, ou o cartaz em pé.";
    case AJ_CW_THUMB: return "Usa a imagem do próprio episódio no card, em vez da arte da série.";
    case AJ_CW_BLUR_PROX: case AJ_DET_BLUR_NAO_VISTOS: return "Oculta detalhes da miniatura para evitar spoilers de episódios ainda não assistidos.";
    case AJ_CW_FURTHEST: return "Escolhe o próximo episódio a partir do mais avançado marcado como assistido.";
    case AJ_CW_NAO_EXIBIDOS: return "Mostra na retomada o próximo episódio mesmo antes de ele ir ao ar.";
    case AJ_CW_ORDEM: return "Como a retomada se ordena: pelo mais recente, no estilo dos streamings, ou com os episódios futuros num bloco separado.";

    // --- Pagina de detalhe
    case AJ_DET_TRAILER: return "Mostra o botão de trailer na tela do título, quando existe um trailer conhecido.";
    case AJ_DET_META_EXT: return "Prefere a ficha do addon de metadados à do Cinemeta. Útil quando o seu addon tem sinopse e elenco melhores.";
    case AJ_DET_DATA_CHEIA: return "Escreve a data de estreia por extenso em vez de só o ano.";

    // --- Posteres e cards
    case AJ_EXPANDIR: return "O cartaz em foco cresce e abre a arte deitada atrás dele depois de um instante parado.";
    case AJ_EXPANDIR_ATRASO: return "Quanto tempo o foco precisa ficar parado antes de o cartaz expandir.";
    case AJ_NAV_RAPIDA: return "Andar de lado numa fileira não espera a animação terminar. Serve para controle que repete rápido.";
    case AJ_BORDA_FOCO: return "O anel colorido que marca o cartaz em foco na Home. Desligado, o foco fica só pelo tamanho do cartaz.";
    case AJ_PROF: return "Dá relevo aos cartazes: borda iluminada e um reflexo que acompanha o foco.";
    case AJ_PROF_BORDA: return "Quanto a borda do cartaz em foco acende.";
    case AJ_PROF_BRILHO: return "Quanto o reflexo passa por cima da arte do cartaz em foco.";
    case AJ_PROF_COBERTURA: return "Que parte da volta do cartaz a borda iluminada percorre.";
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return "Onde o relevo é aplicado. Desligar em alguns lugares alivia o desenho sem perder o efeito onde ele importa.";
    case AJ_LARGURA_DP: return "Ajusta a largura dos pôsteres nas fileiras que usam o tamanho personalizável.";
    case AJ_RAIO_DP: return "Controla o arredondamento dos cantos dos pôsteres.";

    // --- Interface e conta
    case AJ_IDIOMA: return "Idioma de toda a interface. Não muda o idioma das legendas nem do áudio.";
    case AJ_TEMA: return "Cor do anel que marca onde está o foco. É a mesma escolha de tema do app web, e vale só para o anel: o resto da interface não muda de cor.";
    case AJ_ANIM: return "Use Reduzidas para movimentos mais discretos ao navegar pela interface.";
    case AJ_RESOLUCAO: return "Desenha a interface em 4K nas TVs que permitem. Muitas ignoram o pedido e continuam em 1080p — o log diz qual é o caso. Vale reiniciar o app depois de mudar. O vídeo já é 4K nos dois casos.";
    case AJ_PERFIL_ATIVO: return "Perfil em uso nesta TV. Trocar de perfil é feito na tela de perfis, ao abrir o app.";
    case AJ_SYNC: return "Estado da última troca de dados com a sua conta: addons, progresso, coleções e preferências.";
    case AJ_ADDONS: return "Abre a lista de addons da sua conta, para ligar e desligar cada um nesta TV.";
    case AJ_TRAKT: return "Conecta a sua conta do Trakt para marcar o que assistiu e usar a sua lista.";
    case AJ_SIMKL: return "Conecta a sua conta do Simkl, uma alternativa ao Trakt para acompanhar séries.";
    case AJ_SAIR: return "Sai da conta nesta TV e apaga daqui a sessão, os addons e o progresso guardados.";
    case AJ_ESPACO: return "Uso atual de memória pelo cache de imagens, não espaço ocupado no armazenamento da TV.";
    case AJ_VERSAO_I: return "Versão do aplicativo. Esta informação não pode ser alterada.";

    // --- Integracoes
    case AJ_TMDB_LIGADO: return "O TMDB enriquece títulos com sinopse, elenco com foto, ficha técnica e trailers. Desligar corta tudo isso de uma vez.";
    case AJ_TMDB_IDIOMA: return "Idioma dos textos que o TMDB traz (sinopse, títulos). \"Da interface\" segue o idioma do app.";
    case AJ_TMDB_ARTE: return "Prefere pôster e fundo traduzidos pelo TMDB quando o título tem arte no seu idioma.";
    case AJ_TMDB_BASICO: return "Usa título e sinopse do TMDB no lugar dos que vieram no catálogo do addon.";
    case AJ_TMDB_FICHA: return "Preenche a ficha técnica da página do título (status, duração, países).";
    case AJ_TMDB_DATAS: return "Datas de estreia e classificação etária do seu país, pelo TMDB.";
    case AJ_TMDB_ELENCO: return "Fotos do elenco e nomes dos papéis, vindos do TMDB.";
    case AJ_TMDB_PROD: return "Lista as produtoras na página do título; tocar num logo abre os títulos dela.";
    case AJ_TMDB_REDES: return "Lista as redes (HBO, Netflix…) na página da série; tocar num logo abre os títulos dela.";
    case AJ_TMDB_EPS: return "Busca dados de episódios no TMDB para complementar os que vêm do addon.";
    case AJ_TMDB_TRAILERS: return "A fileira de trailers da página do título. Desligue para esconder os cards.";
    case AJ_TMDB_MAIS: return "A aba \"Mais como este\" passa a usar as recomendações do TMDB.";
    case AJ_TMDB_COL: return "A aba de coleção/saga (as outras partes da franquia) na página do filme.";
    case AJ_TMDB_CW: return "Usa o TMDB para preencher os cartazes da fileira de retomada.";
    case AJ_MDB_LIGADO: return "O MDBList junta notas de várias fontes na página do título. Desligar esconde a fileira inteira.";
    case AJ_MDB_CHAVE: return "A chave vem da sua conta Nuvio ou do arquivo do pacote. Não dá para digitar nesta TV.";
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB:
    case AJ_MDB_LETTER: case AJ_MDB_TOMATES: case AJ_MDB_AUDIENCIA:
    case AJ_MDB_META: case AJ_MDB_MAL:
      return "Mostra ou esconde esta fonte na fileira de notas da página do título.";
    default: return "Use as setas laterais para escolher. A preferência é aplicada ao alterar o valor.";
  }
}

// O QUE MUDA NA PRATICA quando esta opcao muda. NULL quando nao ha nada
// honesto a dizer — inventar uma consequencia para cada linha encheria a tela
// de texto e ensinaria a pessoa a nao ler nenhum.
//
// So entram as consequencias que a pessoa NAO adivinha olhando a linha: custo
// de rede, escopo (esta TV x a conta), e dependencia entre opcoes.
static const char *efeitoOpcao(int op) {
  if (inativa(op)) return NULL;
  switch (op) {
    case AJ_FIL_LIMITE:
      return "Vale só nesta TV. Cada fileira a mais é um pedido a mais pela rede quando a Home monta.";
    case AJ_FIL_ORDEM:
      return "Vale só nesta TV: não altera a Home dos seus outros aparelhos.";
    case AJ_CW_FONTE:
      return "Vale só nesta TV. Ao mudar, a fileira é remontada na hora.";
    case AJ_IDIOMA:
      return "Ao mudar, as fileiras são remontadas para os títulos saírem no idioma novo.";
    case AJ_LEG_LINGUA:
      return "Se um título já estiver aberto, a busca de legendas é refeita um instante depois.";
    case AJ_PROF:
      return "É o ajuste mais caro desta tela para a TV desenhar. Desligue se a rolagem engasgar.";
    case AJ_SAIR:
      return "Não pede confirmação: OK sai na hora. Para voltar é preciso entrar de novo pelo QR.";
    case AJ_SALVOS_DEST:
      return "A lista desta TV recebe o título nos dois casos. Isto decide se ele também vai para o Trakt.";
    case AJ_ADDONS: case AJ_TRAKT: case AJ_SIMKL:
      return "OK abre. As setas laterais não fazem nada nesta linha.";
    default: return NULL;
  }
}

// txt_bloco QUEBRA POR ESPACO E SO POR ESPACO: um "\n" no meio do texto nao
// quebra linha nenhuma — ele chega na fonte como caractere e sai desenhado como
// um retangulo vazio, com as linhas coladas numa so. Os tres blocos de dica
// desta tela e os dois da folha de fileiras estavam assim, e na captura de
// 1080p liam "↑ ↓ Navegar▯← → Alterar valor▯Voltar Ir para as categorias".
//
// Aqui cada dica e uma string propria e uma linha propria. Nao e conserto do
// txt_bloco de proposito: ele e de text.c, que esta com outro dono agora — a
// falha esta anotada no relatorio para quem cuidar daquele arquivo.
static void desenhaDicas(const char *const *linhas, int n, float x, float y,
                         float larg, int r, int g, int b) {
  int i;
  for (i = 0; i < n; i++) {
    TxtLinha l = txt_linha_corta(TXT_CAPTION, linhas[i], r, g, b, 255, larg);
    txt_desenhar(l, x, y);
    y += 34.0f;
  }
}

// Deslocamento vertical do topo da lista ate a linha `op`, contando os
// cabecalhos das secoes E das subsecoes que vieram antes. Tem de casar
// exatamente com o laco de desenho em ajustes_desenhar: duas contas do mesmo
// layout sao duas chances de discordar, e quando discordam a rolagem para na
// linha errada.
static float yDaOpcao(int op) {
  float y = 0.0f;
  for (int s = 0; s < AJ_N_SECOES; s++) {
    y += (s ? AJ_SEC_GAP : 0.0f) + AJ_SEC_CABEC;
    for (int k = 0; k < SECOES[s].n; k++) {
      int o = SECOES[s].ini + k;
      y += alturaSub(o);
      if (o == op) return y;
      y += AJ_LINHA_H + AJ_LINHA_GAP;
    }
  }
  return y;
}

// Tecla dentro da folha de fileiras.
//
// O GESTO. "OK para pegar, cima/baixo para mover, OK para soltar" e o padrao de
// reordenar em TV e e o que a folha usa — mas so na PRIMEIRA coluna, a do nome.
// Se OK pegasse em qualquer coluna, nao sobraria tecla nenhuma para ligar,
// desligar e trocar o card: as coloridas do controle nao chegam aos dois alvos
// (no webOS so a azul tem scancode conhecido, e no Tizen a vermelha ja e o
// painel de diagnostico do shell), e esquerda/direita sao a navegacao entre
// colunas. Entao: esquerda/direita escolhem A COLUNA, OK age NA COLUNA, e a
// coluna do nome e a alca de mover. A folha escreve isso na tela.
// REAGIR A UMA MUDANCA NA FOLHA. Mexer na ordem/liga-desliga so gravava a
// preferencia — a home seguia com o arranjo velho e o rotulo "Fora do limite"
// ficava preso, porque naHome so e re-marcado quando a home remonta. Duas
// reacoes, na ordem do barato para o caro:
//
// 1. desc_remontar_fileiras(): sem rede, reaplica ordem/limite sobre o que ja
//    foi baixado. Resolve na hora tudo que ja tem dado no aparelho.
// 2. desc_repetir(): a fileira promovida para dentro do limite que nunca foi
//    buscada (vista na declaracao, nunca naHome) nao tem item nenhum salvo —
//    nao ha o que a remontagem desenhar. So um ciclo de rede a traz. Varre so
//    as primeiras `limite` posicoes: fora dele ela continuaria cortada de
//    qualquer jeito, e o ciclo seria pago sem efeito.
static void fileirasReagir(void) {
  int i, teto = fil_limite(), n = fil_n();
  desc_remontar_fileiras();
  for (i = 0; i < n && i < teto; i++)
    if (!fil_linha_oculta(i) && fil_linha_vista(i) && !fil_linha_na_home(i)) {
      printf("[ajustes] %d dentro do limite sem dados: ciclo de rede\n", i);
      fflush(stdout);
      desc_repetir();
      return;
    }
}

// DUAS ABAS: "Na Home" e "Fora da Home". Desenho do dono.
//
// A lista unica anterior misturava tudo — 192 linhas com ligadas no meio,
// desligadas, addon que sumiu, catalogos de busca — e a pessoa nao achava as
// 16 que importam (relato do @rawldon: "the active ones should be grouped at
// the top"). Agora:
//
//   NA HOME      as ligadas, na ordem em que a home monta. Ate `limite` delas
//                estao na home; as que passam do limite estao NA FILA, abaixo
//                de um separador, e entram sozinhas quando alguem sai. Aqui se
//                reordena, remove e escolhe card e tamanho.
//
//   FORA DA HOME tudo o que esta desligado, agrupado por addon e em ordem
//                alfabetica, com salto por letra ao segurar cima/baixo. OK
//                adiciona: entra na home se cabe, senao entra na fila — e a
//                tela DIZ "Home cheia".
//
// A fila nao e estrutura nova: e a ordem de sempre lida de outro jeito (ver
// fil_estado em fileiras.h). Tudo continua no mesmo arquivo, agora por perfil.
static int filAba;                    // 0 = Na Home, 1 = Fora da Home
static int filNaBarra;                // foco na barra de abas
static int filLista[FIL_MAX];         // indices fil_* da aba corrente, na ordem da tela
static int filListaN;
static int filSep;                    // posicao na lista onde comeca a fila (-1 = nao ha)
static char   filAviso[120];          // "Home cheia..." por alguns segundos
static Uint32 filAvisoAte;
// Salto por letra: cima/baixo SEGURADO. O firmware repete o KEYDOWN; tres
// repeticoes seguidas dentro de FIL_RAJADA_MS viram salto para a proxima letra.
static Uint32 filUltTecla; static int filRajada; static SDL_Keycode filRajadaTecla;
#define FIL_RAJADA_MS   260
#define FIL_RAJADA_MIN    3

static void filAvisar(const char *txt) {
  snprintf(filAviso, sizeof filAviso, "%s", txt);
  filAvisoAte = SDL_GetTicks() + 2600;
}

// Ordem da aba "Fora": origem (app, colecao, catalogo), depois addon, depois
// titulo — os tres sem caixa. Estavel: empate fica na ordem da lista.
static int filForaAgrupada = 1;   // aba "Fora": por addon (1) ou alfabetica unica (0)
static int filCmpFora(const void *pa, const void *pb) {
  int a = *(const int *)pa, b = *(const int *)pb, c;
  if (filForaAgrupada) {
    // Addon primeiro; grupo de colecao entra no addon dele quando tem um
    // (col_grupo_addon), senao fica no bloco "Colecao" com os sem addon.
    c = strcasecmp(fil_linha_addon(a), fil_linha_addon(b));
    if (c) return c;
    c = fil_linha_origem(a) - fil_linha_origem(b);
    if (c) return c;
  }
  c = strcasecmp(fil_titulo(a), fil_titulo(b));
  if (c) return c;
  return a - b;
}

static void filMontarLista(void) {
  int i, n = fil_n();
  filListaN = 0; filSep = -1;
  if (filAba == 0) {
    for (i = 0; i < n; i++) {
      int e = fil_estado(i);
      if (e == FIL_FORA) continue;
      if (e == FIL_NA_FILA && filSep < 0) filSep = filListaN;
      filLista[filListaN++] = i;
    }
  } else {
    for (i = 0; i < n; i++) if (fil_estado(i) == FIL_FORA) filLista[filListaN++] = i;
    qsort(filLista, (size_t)filListaN, sizeof *filLista, filCmpFora);
  }
  { int max = filListaN + (filAba == 1 ? 1 : 0);
    if (filFoco > max) filFoco = max; }
  if (filFoco < 0) filFoco = 0;
}

// Linha da tela -> indice em fil_*. -1 no botao (aba 0, posicao filListaN).
static int filIdx(int pos) {
  return (pos >= 0 && pos < filListaN) ? filLista[pos] : -1;
}

// Primeira letra "de ordem" do titulo, em caixa alta; digito e simbolo viram '#'.
static char filLetra(int i) {
  const char *t = fil_titulo(i);
  unsigned char c = (unsigned char)(t && t[0] ? t[0] : '#');
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 'A' && c <= 'Z') return (char)c;
  return '#';
}

static void eventoFileiras(SDL_Keycode k) {
  int n;
  filMontarLista();
  n = filListaN;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    if (filPegou) {
      // Voltar com o item na mao DESFAZ o movimento. Soltar e cancelar tem de
      // ser teclas diferentes: sem cancelamento, um movimento errado num
      // controle de TV so se conserta contando os passos de volta.
      //
      // O `passos` nao e paranoia: se a lista encolher enquanto o item esta na
      // mao (logout, ou um addon que sumiu), fil_mover devolve o MESMO indice
      // e um `while` sem teto fica preso — trava o app com o controle na mao
      // da pessoa. Com o teto, o pior caso e o item ficar onde esta.
      int passos = FIL_MAX + 1;
      int idx = filIdx(filFoco);
      while (idx >= 0 && idx != filPegouDe && passos-- > 0) {
        int antes = idx;
        idx = (filPegou == 2) ? fil_mover_grupo(idx, filPegouDe > idx ? 1 : -1)
                              : fil_mover(idx, filPegouDe > idx ? 1 : -1);
        if (idx == antes) break;
      }
      filPegou = 0;
      filMontarLista();
      { int p; for (p = 0; p < filListaN; p++) if (filLista[p] == idx) filFoco = p; }
    } else {
      filAberta = 0;
    }
    return;
  }

  // BARRA DE ABAS: ← → trocam a aba, ↓ volta para a lista.
  if (filNaBarra) {
    if (k == SDLK_LEFT || k == SDLK_RIGHT) {
      filAba = !filAba; filFoco = 0; filTopo = 0; filCampo = 0; filPegou = 0;
      filMontarLista();
    } else if (k == SDLK_DOWN || k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      filNaBarra = 0;
    }
    return;
  }

  if (k == SDLK_DOWN || k == SDLK_UP) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    Uint32 agora = SDL_GetTicks();
    // Rajada: a mesma tecla repetida sem folga.
    if (k == filRajadaTecla && agora - filUltTecla < FIL_RAJADA_MS) filRajada++;
    else filRajada = 0;
    filRajadaTecla = k; filUltTecla = agora;

    if (filPegou == 2) { int idx = fil_mover_grupo(filIdx(filFoco), dir); filMontarLista();
                         { int p; for (p = 0; p < filListaN; p++) if (filLista[p] == idx) filFoco = p; } return; }
    if (filPegou)      { int idx = fil_mover(filIdx(filFoco), dir); filMontarLista();
                         { int p; for (p = 0; p < filListaN; p++) if (filLista[p] == idx) filFoco = p; } return; }
    if (k == SDLK_UP && filFoco == 0) { filNaBarra = 1; return; }
    // SALTO POR LETRA na aba "Fora": segurando, pula para a proxima letra em
    // vez de andar linha a linha — com 200 linhas e o unico jeito de chegar
    // ao fim sem soltar o dedo por um minuto.
    if (filAba == 1 && filRajada >= FIL_RAJADA_MIN && n > 0 && filFoco < n) {
      int p = filFoco, letra = filLetra(filIdx(filFoco));
      while (p + dir >= 0 && p + dir < n && filLetra(filIdx(p + dir)) == letra) p += dir;
      if (p + dir >= 0 && p + dir < n) p += dir;
      // Sem proxima letra o salto nao anda — e ai o passo normal vale, senao a
      // tecla segurada nunca chegava ao botao "Atualizar tudo" no fim da lista.
      if (p != filFoco) { filFoco = p; return; }
    }
    // Botoes no fim: aba 0 tem "Atualizar tudo" em n; aba 1 tem "Agrupar por
    // addon" em n e "Atualizar tudo" em n+1.
    { int max = (filAba == 0) ? n : n + 1;
      if (filFoco + dir >= 0 && filFoco + dir <= max) filFoco += dir; }
    return;
  }
  filRajada = 0;

  if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    if (filPegou) {
      // Com o item na mao, esquerda/direita alterna entre mover a FILEIRA e
      // mover o BLOCO do addon inteiro. O texto da folha diz qual e o modo.
      filPegou = (filPegou == 1) ? 2 : 1;
      return;
    }
    if (filAba == 1 || filFoco >= n) return;   // uma coluna so; os botoes nao tem colunas
    filCampo += (k == SDLK_RIGHT) ? 1 : -1;
    if (filCampo < 0) filCampo = 0;
    if (filCampo > AJ_FIL_CAMPOS - 1) filCampo = AJ_FIL_CAMPOS - 1;
    return;
  }

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    int idx = filIdx(filFoco);
    // ATUALIZAR TUDO: um sync da conta (addons, colecoes, ajustes) e uma volta
    // completa da descoberta. E o botao para "instalei/removi um addon no
    // celular e quero ver aqui agora", sem sair da conta nem esperar o ciclo.
    // A poda de fantasmas roda dentro da volta.
    if ((filAba == 0 && filFoco == n) || (filAba == 1 && filFoco == n + 1)) {
      sync_iniciar();
      desc_repetir();
      filAvisar(i18n("Atualizando: addons, coleções e fileiras — a Home se refaz uma vez, no fim"));
      return;
    }
    if (n < 1) return;
    if (filAba == 1 && filFoco == n) {
      // AGRUPAR POR ADDON e uma alternancia desta aba: agrupado (padrao) ou
      // uma lista alfabetica unica. Na aba "Na Home" nao existe — la a ordem e
      // a da home, e a pessoa e quem a arruma.
      filForaAgrupada = !filForaAgrupada;
      filFoco = 0; filTopo = 0;
      filMontarLista();
      return;
    }
    if (filAba == 1) {
      // ADICIONAR. Cabe: entra na home. Nao cabe: entra na fila, e a tela diz.
      int est = -1;
      if (idx < 0) return;
      fil_adicionar(idx, &est);
      if (est == FIL_NA_FILA) {
        char b[120];
        snprintf(b, sizeof b, i18n("Home cheia (%d de %d) · entrou na fila e sobe quando abrir vaga"),
                 fil_limite(), fil_limite());
        filAvisar(b);
      } else filAvisar(i18n("Adicionada à Home"));
      fileirasReagir();
      filMontarLista();
      if (filFoco >= filListaN) filFoco = filListaN - 1;
      if (filFoco < 0) filFoco = 0;
      return;
    }
    if (idx < 0) return;
    switch (filCampo) {
      case 0: if (!filPegou) { filPegou = 1; filPegouDe = idx; }
              else { filPegou = 0;
                     // Soltou em lugar diferente: a home nao sabe ainda.
                     if (idx != filPegouDe) fileirasReagir(); }
              break;
      case 1: // REMOVER: vai para "Fora da Home"; quem estava na fila sobe.
              fil_remover(idx);
              filAvisar(i18n("Removida da Home"));
              fileirasReagir(); filMontarLista();
              if (filFoco >= filListaN) filFoco = filListaN > 0 ? filListaN - 1 : 0;
              break;
      case 2: if (fil_aceita_tipo(idx)) { fil_ciclar_tipo(idx); fileirasReagir(); } break;
      default: fil_ciclar_tam(idx); break;
    }
  }
}

void ajustes_evento(const SDL_Event *e) {
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;

  // Vinculo em andamento e uma pergunta: enquanto ele esta em pe, nada mais na
  // tela responde ao controle.
  { TraEstado ta = traktauth_estado();
    SmkEstado sa = simklauth_estado();
    int traAtivo = (ta == TRA_PEDINDO || ta == TRA_AGUARDANDO || ta == TRA_ERRO);
    int smkAtivo = (sa == SMK_PEDINDO || sa == SMK_AGUARDANDO || sa == SMK_ERRO);
    if (traAtivo || smkAtivo) {
      if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) {
        if (traAtivo) traktauth_cancelar(); else simklauth_cancelar();
      } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        // OK so refaz o pedido quando deu erro; com o codigo na tela ele nao
        // faz nada de proposito, para nao trocar o codigo que a pessoa acabou
        // de digitar no celular.
        if (traAtivo && ta == TRA_ERRO) traktauth_comecar();
        else if (smkAtivo && sa == SMK_ERRO) simklauth_comecar();
      }
      return;
    } }
  // A folha de fileiras e modal, como a do vinculo acima.
  if (filAberta) { eventoFileiras(k); return; }

  { int voltar = (k == SDLK_ESCAPE || k == SDLK_AC_BACK ||
                  k == SDLK_BACKSPACE || k == SDLK_DELETE);
    // NAVEGAR ENTRE CATEGORIAS COM A TECLA QUE O CONTROLE TEM.
    //
    // O atalho de secoes era PgUp/PgDn. Num controle de TV essas duas teclas
    // NAO EXISTEM: o atalho estava escrito no rodape e era inalcancavel no
    // aparelho, que era exatamente a reclamacao. As coloridas nao servem —
    // no webOS so a azul tem scancode conhecido (NV_SCANCODE_BLUE, e o app.c
    // ja a usa na home) e no Tizen a vermelha e o painel de diagnostico do
    // shell, e nenhuma das quatro tem mapeamento garantido no SDL do
    // Emscripten. Esquerda/direita ja mudam o VALOR da linha e nao podem ser
    // tomadas.
    //
    // Sobra a tecla que TODO controle tem e que ja chega aos tres alvos:
    // Voltar. Ela passa a ser um nivel de hierarquia — da lista de opcoes para
    // o indice de secoes, do indice para fora dos Ajustes. Dois toques ainda
    // saem da tela, entao ninguem fica preso; e no indice cima/baixo andam de
    // secao em secao em vez de linha em linha.
    if (focoIndice) {
      int sec = secaoAtual();
      if (voltar || k == SDLK_LEFT) { sair = 1; return; }
      if (k == SDLK_DOWN)  { if (sec < AJ_N_SECOES - 1) focoOp = SECOES[sec + 1].ini; return; }
      if (k == SDLK_UP)    { if (sec > 0)               focoOp = SECOES[sec - 1].ini; return; }
      if (k == SDLK_RIGHT || k == SDLK_RETURN || k == SDLK_KP_ENTER) { focoIndice = 0; return; }
      return;
    }
    // Voltar solta a edicao antes de subir para o indice — dois niveis de
    // "sair", como o Voltar da folha de fileiras que primeiro solta o item.
    if (voltar) { if (emEdicao) emEdicao = 0; else focoIndice = 1; return; } }

  if (k == SDLK_DOWN || k == SDLK_UP) {
    // Navegar CONFIRMA a edicao (o valor ja foi gravado a cada toque) — como
    // o OK, e nao como um cancelamento que a lista nao tem.
    emEdicao = 0;
    if (k == SDLK_DOWN) { if (focoOp < AJ_N - 1) focoOp++; }
    else                { if (focoOp > 0)        focoOp--; }
  }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    // OK em linha de valor entra no modo edicao; em acao, age; nas demais,
    // nao faz nada — uma linha de leitura nao tem o que confirmar.
    if (OPCOES[focoOp].tipo != OP_ACAO) {
      // OK alterna: entra no modo edicao e, de dentro dele, confirma — o valor
      // ja foi gravado a cada toque de seta, nao ha o que desfazer.
      if (mutavel(focoOp)) emEdicao = !emEdicao;
      return;
    }
    if (focoOp == AJ_FIL_ORDEM) {
      filAberta = 1; filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
      filAba = 0; filNaBarra = 0; filAviso[0] = 0;
      // O que passou do limite sem ter sido pedido vai para "Fora da Home"
      // antes de a lista aparecer — ver fil_normalizar.
      fil_normalizar();
      emEdicao = 0;
      return;
    }
    if (focoOp == AJ_ADDONS) { pediuAddons = 1; return; }
    if (focoOp == AJ_TRAKT) { traktauth_comecar(); return; }
    if (focoOp == AJ_SIMKL) { simklauth_comecar(); return; }
    if (focoOp == AJ_SAIR) {
      // Sair apaga a sessao do disco. Sem confirmacao de proposito: o custo de
      // sair sem querer e um login por QR, e uma caixa de confirmacao nesta
      // lista exigiria um modal que a tela nao tem.
      sessao_sair();
      traktauth_esquecer();
      simklauth_esquecer();
      // A ordem e o liga/desliga das fileiras sao da home de QUEM SAIU, como a
      // ordem que vem da conta (ver catordem_esquecer). Sem isto, a proxima
      // pessoa herda a home montada pela anterior.
      fil_esquecer();
      // A sessao sozinha nao basta: addons, Trakt, perfil e progresso ficariam
      // para a proxima pessoa. Ver o cabecalho de sync_esquecer_usuario.
      sync_esquecer_usuario();
      sair = 1;   // volta para a home, que cai no login no proximo quadro
    }
  }
  else if (k == SDLK_PAGEUP || k == SDLK_PAGEDOWN) {
    int s = secaoAtual() + (k == SDLK_PAGEDOWN ? 1 : -1);
    if (s >= 0 && s < AJ_N_SECOES) { focoOp = SECOES[s].ini; emEdicao = 0; }
  }
  else if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    // Fora do modo edicao as setas nao tocam em valor nenhum — OK e a porta
    // de entrada. Ver a nota de emEdicao.
    if (!emEdicao || !mutavel(focoOp)) return;
    const Opcao *o = &OPCOES[focoOp];
    int dir = (k == SDLK_RIGHT) ? 1 : -1;
    if (o->tipo == OP_NUMERO) {
      // Numero NAO circula: passar de 100% para 0% com um toque a mais e um
      // salto que ninguem pede, e no controle da TV a seta repete sozinha.
      int v = valor[focoOp] + dir * o->passo;
      valor[focoOp] = limita(focoOp, v);
      // O limite de fileiras nao vive em valor[]: quem grava e consulta e
      // fileiras.c. Sem esta linha o numero mudaria na tela e a home nao.
      if (focoOp == AJ_FIL_LIMITE) {
        fil_definir_limite(valor[focoOp]);
        valor[focoOp] = fil_limite();
      }
    } else {
      // Escolha circula: a lista e curta e voltar do fim ao inicio poupa
      // toques no controle. Sem circular, o ultimo valor vira um beco.
      { int n = nValores(focoOp);
        valor[focoOp] = (valor[focoOp] + (dir > 0 ? 1 : n - 1)) % n; }
      // A escolha de idioma so vale quando chega em linguas.c; guardar o indice
      // e desenhar o rotulo deixaria o ajuste bonito e inerte, que foi
      // exatamente o defeito do seletor de idioma da interface.
      if (focoOp == AJ_LEG_LINGUA || focoOp == AJ_AUD_LINGUA) aplicarIdioma(focoOp);
      // O rotulo de tipo e os generos das fileiras sao montados na entrada do
      // catalogo, ja no idioma da interface; trocar o idioma remonta.
      if (focoOp == AJ_IDIOMA) desc_repetir();
      // A FONTE DO CONTINUAR tambem remonta, e por um motivo diferente do
      // idioma: quem monta aquela fileira e montarContinuar (descoberta.c), e
      // o conteudo dela nao e refeito por desc_remontar_fileiras — essa so
      // reordena e filtra FILEIRAS, nao os itens de uma. Sem o ciclo, trocar a
      // fonte so teria efeito no proximo sync, e para quem apertou parece que
      // o ajuste nao faz nada.
      if (focoOp == AJ_CW_FONTE) desc_repetir();
    }
    gravar();   // grava a cada mudanca: nao ha botao de "salvar" nesta tela
  }
}

void ajustes_atualizar(float dt, Uint32 agora) {
  (void)agora;
  // Repouso da escolha de idioma de legenda: ver aplicarIdioma.
  if (legendaEspera > 0.0f) {
    legendaEspera -= dt;
    if (legendaEspera <= 0.0f) { legendaEspera = 0.0f; addons_legendas_reiniciar(); }
  }
  for (int i = 0; i < AJ_N; i++) {
    float alvo = (i == focoOp) ? 1.0f : 0.0f;
    animFoco[i] = ajustes_animacoes_reduzidas() ? alvo : anim_mola(animFoco[i], alvo, dt,
                            alvo > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // Rola o minimo para a linha focada caber, e leva junto o cabecalho da secao
  // quando a linha e a primeira dela — sem isso, entrar numa secao mostra a
  // opcao sem dizer a que grupo ela pertence. Vale igual para a subsecao: um
  // bloco que comeca fora da tela vira uma lista sem titulo.
  float topo = yDaOpcao(focoOp) - alturaSub(focoOp);
  for (int s = 0; s < AJ_N_SECOES; s++)
    if (SECOES[s].ini == focoOp) { topo -= AJ_SEC_CABEC; break; }
  float base = yDaOpcao(focoOp) + AJ_LINHA_H;
  float alvo = scrollY;
  if (base - alvo > AJ_BASE - AJ_TOPO) alvo = base - (AJ_BASE - AJ_TOPO);
  if (topo - alvo < 0.0f)              alvo = topo;
  if (alvo < 0.0f) alvo = 0.0f;
  scrollY = ajustes_animacoes_reduzidas() ? alvo : anim_mola(scrollY, alvo, dt, NV_MOLA_SCROLL);
}

// Leva a escolha da linha para linguas.c. "Da conta" (indice 0) manda string
// vazia, que e como linguas.c representa "sem escolha local, siga a conta".
static void aplicarIdioma(int op) {
  const char *c = ling_opcao_codigo(valor[op]);
  if (op == AJ_LEG_LINGUA) {
    ling_local_legenda(c);
    // A lista de legendas do titulo carregado foi montada com o idioma ANTERIOR
    // e nao se refaz sozinha (ver addons_legendas_reiniciar). Mas NAO refazer
    // aqui, e sim depois de a pessoa PARAR de mexer: esta funcao roda a cada
    // toque de esquerda/direita, a lista tem trinta idiomas, e no controle da
    // TV a seta repete sozinha — atravessar a lista dispararia dezenas de
    // buscas, cada uma consultando todos os addons de legenda.
    legendaEspera = 0.7f;
  } else {
    // O audio nao precisa de nada equivalente: as faixas de audio vem do
    // arquivo que esta tocando e a preferencia so escolhe entre as que ja
    // existem (video.c), sem consultar a rede.
    ling_local_audio(c);
  }
}

// Quantos valores uma linha de escolha tem. As duas linhas de idioma sao as
// unicas dinamicas: a lista vem de linguas.c e nao da tabela OPCOES, que e
// const e foi escrita antes de linguas.c existir.
static int nValores(int op) {
  if (op == AJ_LEG_LINGUA || op == AJ_AUD_LINGUA) return nLingua > 0 ? nLingua : 1;
  return OPCOES[op].n;
}

// Texto do valor de uma linha. Buffer estatico porque so uma linha e desenhada
// por vez dentro de desenhaLinha.
static const char *textoValor(int op) {
  static char buf[48];
  const Opcao *o = &OPCOES[op];
  if (o->tipo == OP_LEITURA || o->tipo == OP_ACAO) return textoLeitura(op);
  if (o->tipo == OP_NUMERO) {
    snprintf(buf, sizeof buf, "%d%s", valor[op], o->sufixo ? o->sufixo : "");
    return buf;
  }
  if (op == AJ_LEG_LINGUA || op == AJ_AUD_LINGUA) {
    int v = valor[op];
    return (v >= 0 && v < nLingua && V_LINGUA[v]) ? V_LINGUA[v] : "Da conta";
  }
  return o->valores[valor[op]];
}

static void desenhaLinha(int op, float y, float f) {
  if (y + AJ_LINHA_H < AJ_TOPO - 40.0f || y > AJ_BASE + 40.0f) return;
  // Some antes de cruzar o titulo da tela, como as secoes da pagina de detalhe:
  // texto passando por baixo de texto se le como borrao.
  float a = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f);
  if (a <= 0.005f) return;

  int desligada = inativa(op);
  int podeMudar = mutavel(op);
  GfxRect linha = { AJ_LISTA_X, y, AJ_LISTA_W, AJ_LINHA_H };
  // Mesmo vocabulário do menu: superfície escura, texto claro e foco explícito.
  gfx_cor(linha, AJ_RAIO, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
          (0.34f + 0.66f * f) * a);
  if (op == focoOp) {
    float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
    gfx_rect(linha, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_LINHA_H, 0,
             AJ_RAIO, ar, ag, ab, a);
  }

  // Uma linha inativa fica visivelmente mais apagada QUE a de leitura: leitura e
  // informacao, inativa e "isto existe mas depende de outra coisa".
  float aTexto = a * (desligada ? 0.65f : 1.0f);
  int cr = podeMudar ? 240 : 192;
  TxtLinha rot = txt_linha_corta(TXT_CALLOUT, OPCOES[op].rotulo,
                                cr, cr, cr, 255, AJ_LISTA_W - 420.0f);
  txt_desenhar_alpha(rot, AJ_LISTA_X + AJ_PAD,
                     y + (AJ_LINHA_H - rot.h) * 0.5f, aTexto);

  const char *v = textoValor(op);
  int cv = podeMudar ? 220 : 176;
  TxtLinha val = txt_linha_corta(TXT_CALLOUT, v, cv, cv, cv, 255, 310.0f);
  float xDir = AJ_LISTA_X + AJ_LISTA_W - AJ_PAD;
  float valorDir = xDir - 36.0f;
  float vy = y + (AJ_LINHA_H - val.h) * 0.5f;

  // Barra de preenchimento da linha numerica. Sem ela, "28%" nao diz nada sobre
  // onde 28 fica no intervalo — e o web mostra um slider justamente por isso.
  if (OPCOES[op].tipo == OP_NUMERO) {
    const Opcao *o = &OPCOES[op];
    float t = (o->max > o->min)
            ? (float)(valor[op] - o->min) / (float)(o->max - o->min) : 0.0f;
    float bw = 220.0f, bh = 4.0f;
    float bx = valorDir - bw;
    float by = y + AJ_LINHA_H - 17.0f;
    vy -= 8.0f;
    GfxRect trilho = { bx, by, bw, bh };
    GfxRect cheio  = { bx, by, bw * anim_clamp(t, 0.0f, 1.0f), bh };
    gfx_cor(trilho, 0.5f, 0.94f, 0.94f, 0.96f, 0.22f * aTexto);
    if (cheio.w > 0.5f)
      gfx_cor(cheio, 0.5f, 0.94f, 0.94f, 0.96f, 0.92f * aTexto);
  }

  // MODO EDICAO: as setas e o realce do valor so existem depois do OK. Sem
  // edicao a linha em foco mostra so o valor — a porta de entrada e escrita
  // no rodape de dicas, nao rabiscada em cada linha.
  if (podeMudar && emEdicao && f > 0.02f) {
    GfxRect pill = { valorDir - val.w - 44.0f, y + (AJ_LINHA_H - 34.0f) * 0.5f,
                     val.w + 80.0f, 34.0f };
    gfx_cor(pill, 0.5f, 0.55f, 0.62f, 0.75f, 0.35f * a);
    TxtLinha dir = txt_linha(TXT_CAPTION2, "\xe2\x96\xb6", cv, cv, cv, 255);
    TxtLinha esq = txt_linha(TXT_CAPTION2, "\xe2\x97\x80", cv, cv, cv, 255);
    txt_desenhar_alpha(dir, xDir - dir.w, y + (AJ_LINHA_H - dir.h) * 0.5f, aTexto * f);
    txt_desenhar_alpha(esq, valorDir - val.w - 16.0f - esq.w,
                       y + (AJ_LINHA_H - esq.h) * 0.5f, aTexto * f);
  }
  txt_desenhar_alpha(val, valorDir - val.w, vy, aTexto);
}

// Sobreposicao do vinculo (Trakt ou Simkl). O codigo destes dois e CURTO — 8
// caracteres no Trakt — e o endereco e fixo, entao da para ler da TV e digitar
// no celular. Nao precisa de QR, ao contrario dos 32 digitos hexadecimais do
// login da conta.
// O QR DO VINCULO. A folha so mostrava o codigo curto e o endereco — a pessoa
// tinha de abrir o navegador do celular, digitar trakt.tv/activate e DEPOIS o
// codigo. Com o simbolo a camera abre a pagina direto. Mesmo cuidado da tela
// de login (login.c): NEAREST, fundo claro com zona de silencio.
static GLuint texQrVin;
static char   qrVinDe[256];

static void qrVinTex(const char *texto) {
  Qr q; int lado, x, y; unsigned char *px;
  if (!texto || !texto[0]) return;
  if (!strcmp(qrVinDe, texto) && texQrVin) return;
  if (!qr_gerar(&q, texto)) return;
  lado = q.lado + 8;                    // 4 modulos de silencio por lado
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return;
  memset(px, 255, (size_t)lado * lado * 3);
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) {
        size_t i = ((size_t)(y + 4) * lado + (x + 4)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }
  if (!texQrVin) glGenTextures(1, &texQrVin);
  glBindTexture(GL_TEXTURE_2D, texQrVin);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB,
               GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(px);
  snprintf(qrVinDe, sizeof qrVinDe, "%s", texto);
}

static void desenhaVinculo(const char *servico, const char *codigo,
                           const char *endereco, const char *falha, int esperando) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  GfxRect cartao = { (NV_TELA_W - 1000.0f) * 0.5f, 250.0f, 1000.0f, 560.0f };
  TxtLinha l;
  char t[80];
  float y = 300.0f;
  // Veu QUASE opaco mais um cartao solido atras do bloco. Com 0.80 de veu e sem
  // cartao, as linhas de Ajustes atravessavam o texto — "aguardando" caia em
  // cima de "Trakt" e "conectar" em cima de "e informe o codigo". Um codigo que
  // a pessoa precisa transcrever nao pode competir com texto de fundo.
  gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, 0.92f);
  gfx_cor(cartao, 0.045f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);

  snprintf(t, sizeof t, i18n("Conectar %s"), servico);
  l = txt_linha(TXT_TITULO2, t, 255, 255, 255, 255);
  txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
  y += 92.0f;

  if (falha && falha[0]) {
    l = txt_linha(TXT_HEADLINE, falha, 236, 108, 108, 255);
    txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
    y += 70.0f;
    l = txt_linha(TXT_CAPTION, "OK para tentar de novo · Voltar para fechar",
                  150, 152, 160, 255);
    txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
    return;
  }
  if (!codigo || !codigo[0]) {
    l = txt_linha(TXT_HEADLINE, "Preparando o código…", 210, 212, 220, 255);
    txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
    return;
  }

  // QR a ESQUERDA, instrucoes a DIREITA. Apontar a camera abre a pagina de
  // ativacao; o codigo continua grande ao lado porque a pagina o pede em
  // seguida — sem ele visivel o QR serviria para nada.
  { float qLado = 300.0f;
    float qx = cartao.x + 64.0f, qy = 350.0f;
    float tx = cartao.x + 440.0f, ty = qy + 6.0f;
    qrVinTex(endereco);
    if (texQrVin) {
      GfxRect moldura = { qx - 16.0f, qy - 16.0f, qLado + 32.0f, qLado + 32.0f };
      GfxRect rq = { qx, qy, qLado, qLado };
      gfx_cor(moldura, 0.06f, 1.0f, 1.0f, 1.0f, 1.0f);
      gfx_tex_aspect_atual = 0.0f;   // 1:1, sem recorte
      gfx_rect(rq, texQrVin, GFX_SNAP, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0, 1.0f);
    }
    l = txt_linha(TXT_BODY, "No celular, abra:", 176, 178, 186, 255);
    txt_desenhar(l, tx, ty);
    ty += 52.0f;
    l = txt_linha(TXT_TITULO3, endereco && endereco[0] ? endereco : "-",
                  255, 255, 255, 255);
    txt_desenhar(l, tx, ty);
    ty += 96.0f;
    l = txt_linha(TXT_BODY, "e informe o código:", 176, 178, 186, 255);
    txt_desenhar(l, tx, ty);
    ty += 62.0f;
    // Espacamento entre letras: um codigo curto sem tracking le como palavra,
    // e a pessoa transcreve errado.
    txt_tracking(TXT_TITULO1, codigo, 255, 255, 255, tx, ty, 1.0f, 16.0f); }

  if (esperando) {
    l = txt_linha(TXT_CAPTION, "Aguardando a autorização…", 150, 152, 160, 255);
    txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, cartao.y + cartao.h - 60.0f);
  }
}


// --- COLUNA DE SECOES -------------------------------------------------------
// Ela nao e enfeite: e o unico caminho entre categorias que o controle da TV
// alcanca (ver a nota em ajustes_evento). Por isso ela esta SEMPRE visivel, e
// nao so quando tem foco — um atalho que a pessoa nao ve nao existe, foi o que
// aconteceu com PgUp/PgDn.
static void desenhaIndice(void) {
  int sec = secaoAtual();
  float y = AJ_TOPO;
  float raio = 14.0f / AJ_IDX_H;
  int s;
  for (s = 0; s < AJ_N_SECOES; s++) {
    GfxRect r = { AJ_IDX_X, y, AJ_IDX_W, AJ_IDX_H };
    int atual = (s == sec);
    int c = atual ? 240 : 168;
    float ci = atual ? 0.94f : 0.62f;
    GfxRect ic = { AJ_IDX_X + 18.0f, y + (AJ_IDX_H - AJ_IDX_ICONE) * 0.5f,
                   AJ_IDX_ICONE, AJ_IDX_ICONE };
    float tx = ic.x + AJ_IDX_ICONE + 14.0f;
    TxtLinha t;
    gfx_cor(r, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
            atual ? (focoIndice ? 1.0f : 0.60f) : 0.26f);
    if (atual && focoIndice) {
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      gfx_rect(r, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_IDX_H, 0, raio,
               ar, ag, ab, 1.0f);
    }
    // O ICONE E A ANCORA da varredura de olho: a 3 m o nome da categoria e
    // texto pequeno, e o simbolo e o que se reconhece antes de ler.
    gfx_icone(ic, SECOES[s].icone, ci, ci, ci, 1.0f);
    t = txt_linha_corta(TXT_CALLOUT, SECOES[s].curto, c, c, c, 255,
                        AJ_IDX_X + AJ_IDX_W - 16.0f - tx);
    txt_desenhar(t, tx, y + (AJ_IDX_H - t.h) * 0.5f);
    y += AJ_IDX_H + 6.0f;
  }
}

// --- FOLHA "ORDENAR E ATIVAR FILEIRAS" --------------------------------------
#define AJ_FIL_W      1480.0f
#define AJ_FIL_H       920.0f
#define AJ_FIL_Y        80.0f
#define AJ_FIL_LINHA    76.0f
#define AJ_FIL_LGAP      6.0f
// SEIS, e nao sete. A linha cresceu para 76 para caber o nome do addon sob o
// titulo — sete linhas de 76 terminariam em y+578 e a ficha comeca em y+684.
#define AJ_FIL_VIS       6      // linhas desenhadas por vez

// As quatro colunas, em x relativo ao cartao. A primeira e a ALCA DE MOVER: e
// nela que OK pega e solta a fileira.
static const struct { float x, w; const char *cabec; } AJ_FIL_COL[AJ_FIL_CAMPOS] = {
  {   40.0f, 600.0f, "Fileira"  },
  {  670.0f, 190.0f, "Estado"   },
  {  880.0f, 250.0f, "Card"     },
  { 1150.0f, 250.0f, "Tamanho"  },
};

// POR QUE ESTA FILEIRA NAO ESCOLHE A FORMA DO CARD. Dizer "Fixo" e deixar a
// pessoa apertando OK sem efeito e o mesmo defeito das linhas inativas da lista
// principal: a dependencia tem de ficar visivel.
static const char *motivoFormaFixa(const char *chave) {
  if (!strcmp(chave, "continue_watching"))
    return "A forma desta fileira vem de Estilo do \"Continuar assistindo\".";
  if (!strcmp(chave, "social_activity"))
    return "O feed dos amigos usa o card com o nome de quem assistiu.";
  if (!strncmp(chave, "collection_", 11))
    return "Um grupo de coleções mostra atalhos para catálogos, não títulos.";
  return "A forma desta fileira não é escolhida aqui.";
}

static void desenhaFileiras(void) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  GfxRect cartao = { (NV_TELA_W - AJ_FIL_W) * 0.5f, AJ_FIL_Y, AJ_FIL_W, AJ_FIL_H };
  int n, lim = fil_limite();
  int i, vis;
  float cx = cartao.x, y, filCabecY;
  TxtLinha l;
  char buf[160];
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);

  filMontarLista();
  n = filListaN;

  // Veu quase opaco mais cartao solido: a lista de Ajustes atras atravessava o
  // texto do vinculo, e aqui ha texto pequeno em quatro colunas.
  gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, 0.92f);
  gfx_cor(cartao, 0.03f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);

  // EMPILHADO PELA ALTURA MEDIDA, e nao por deslocamentos fixos: a altura de
  // uma linha depende do estilo, da escala e do idioma, e so txt_linha sabe.
  { float hy = cartao.y + 30.0f;
    l = txt_linha(TXT_TITULO2, "Fileiras da Home", 255, 255, 255, 255);
    txt_desenhar(l, cx + 40.0f, hy);
    hy += l.h + 8.0f;
    l = txt_linha(TXT_MINI, "Vale só nesta TV e neste perfil · não altera a Home dos outros aparelhos",
                  150, 153, 162, 255);
    txt_desenhar(l, cx + 40.0f, hy);
    hy += l.h + 14.0f;

    // BARRA DE ABAS. Duas pilulas com a contagem: "Na Home 14 de 16" diz de
    // uma vez o que esta em uso e o limite; "Fora da Home 178" diz o tamanho
    // do resto. A ativa e clara; a outra, apagada; o anel so quando a barra
    // tem o foco.
    { float bx = cx + 40.0f, bh = 44.0f;
      int t;
      for (t = 0; t < 2; t++) {
        int ativa = (filAba == t);
        if (t == 0) snprintf(buf, sizeof buf, i18n("Na Home  %d de %d"), fil_n_na_home(), lim);
        else        snprintf(buf, sizeof buf, i18n("Fora da Home  %d"), fil_n() - fil_n_na_home() - fil_n_fila());
        l = txt_linha(TXT_CALLOUT, buf, ativa ? 255 : 170, ativa ? 255 : 173, ativa ? 255 : 182, 255);
        { GfxRect pil = { bx, hy, l.w + 44.0f, bh };
          gfx_cor(pil, NV_RAIO_PILL, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
                  ativa ? 0.95f : 0.30f);
          if (filNaBarra && ativa)
            gfx_rect((GfxRect){ pil.x - NV_ANEL_FOCO, pil.y - NV_ANEL_FOCO,
                                pil.w + NV_ANEL_FOCO * 2, pil.h + NV_ANEL_FOCO * 2 },
                     0, GFX_ANEL, 0, NV_ANEL_FOCO / (pil.h + NV_ANEL_FOCO * 2), 0,
                     NV_RAIO_PILL, ar, ag, ab, 1.0f);
          txt_desenhar(l, pil.x + 22.0f, pil.y + (bh - l.h) * 0.5f);
          bx += pil.w + 14.0f; }
      }
      // Fila, quando ha: um numero ao lado das abas, para nao ser surpresa.
      if (fil_n_fila() > 0) {
        snprintf(buf, sizeof buf, fil_n_fila() == 1 ? i18n("%d na fila") : i18n("%d na fila"), fil_n_fila());
        l = txt_linha(TXT_CAPTION, buf, 226, 186, 108, 255);
        txt_desenhar(l, bx + 8.0f, hy + (bh - l.h) * 0.5f);
      }
      hy += bh + 12.0f; }
    filCabecY = hy; }

  if (n < 1 && filAba == 0) {
    l = txt_linha(TXT_HEADLINE, "Nenhuma fileira ligada", 222, 224, 232, 255);
    txt_desenhar(l, cx + 40.0f, filCabecY + 40.0f);
    txt_bloco(TXT_CAPTION,
              "Vá para a aba \"Fora da Home\" (↑ e depois →) e aperte OK numa fileira para adicioná-la.",
              183, 186, 194, cx + 40.0f, filCabecY + 88.0f, AJ_FIL_W - 80.0f, 34, 1, 3);
  } else if (n < 1) {
    l = txt_linha(TXT_HEADLINE, "Tudo o que o app conhece já está na Home", 222, 224, 232, 255);
    txt_desenhar(l, cx + 40.0f, filCabecY + 40.0f);
    txt_bloco(TXT_CAPTION,
              "As fileiras aparecem aqui depois que o app lê os catálogos dos seus addons. "
              "Abra a Home, espere o catálogo carregar e volte a esta tela.",
              183, 186, 194, cx + 40.0f, filCabecY + 88.0f, AJ_FIL_W - 80.0f, 34, 1, 3);
  }

  // QUANTAS LINHAS CABEM, medido no espaco que sobra entre o cabecalho das
  // colunas e a ficha do rodape — e nao um numero cravado. A barra de abas
  // empurrou a lista ~60 px para baixo, e com o 6 fixo a sexta linha caia em
  // cima da ficha e do botao (visto na captura de revisao).
  { float topoLista = filCabecY + 34.0f;
    float fimLista  = cartao.y + AJ_FIL_H - 232.0f - 8.0f - (52.0f + 8.0f);
    vis = (int)((fimLista - topoLista) / (AJ_FIL_LINHA + AJ_FIL_LGAP));
    if (vis < 3) vis = 3; }

  { int max = n + (filAba == 1 ? 1 : 0);
    if (filFoco > max) filFoco = max; }
  if (filFoco < 0) filFoco = 0;
  if (filFoco < filTopo) filTopo = filFoco;
  if (filFoco >= filTopo + vis) filTopo = filFoco - vis + 1;
  if (filTopo > n - vis) filTopo = n - vis;
  if (filTopo < 0) filTopo = 0;

  // Cabecalho das colunas. Na aba "Fora" so ha a coluna da fileira e a acao.
  if (n > 0) {
    if (filAba == 0)
      for (i = 0; i < AJ_FIL_CAMPOS; i++) {
        l = txt_linha(TXT_MINI, AJ_FIL_COL[i].cabec, 148, 151, 160, 255);
        txt_desenhar(l, cx + AJ_FIL_COL[i].x, filCabecY);
      }
    else {
      l = txt_linha(TXT_MINI, "Fileira", 148, 151, 160, 255);
      txt_desenhar(l, cx + AJ_FIL_COL[0].x, filCabecY);
      l = txt_linha(TXT_MINI, "Addon", 148, 151, 160, 255);
      txt_desenhar(l, cx + AJ_FIL_COL[1].x, filCabecY);
    }
  }

  y = filCabecY + 34.0f;
  for (i = filTopo; i < n && i < filTopo + vis; i++) {
    int idx = filLista[i];
    GfxRect linha = { cx + 24.0f, y, AJ_FIL_W - 48.0f, AJ_FIL_LINHA };
    float raio = 12.0f / AJ_FIL_LINHA;
    int foco = (i == filFoco && !filNaBarra);
    int naFila = (filAba == 0 && filSep >= 0 && i >= filSep);
    float aTexto = naFila ? 0.80f : 1.0f;
    int c = 234;
    gfx_cor(linha, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
            filPegou && foco ? 1.0f : (foco ? 0.72f : 0.30f));

    // SEPARADOR DA FILA: a partir daqui as fileiras esperam vaga.
    if (filAba == 0 && filSep >= 0 && i == filSep) {
      GfxRect corte = { cx + 24.0f, y - AJ_FIL_LGAP * 0.5f - 1.0f, AJ_FIL_W - 48.0f, 2.0f };
      gfx_cor(corte, 0.0f, 0.90f, 0.72f, 0.42f, 0.65f);
    }

    if (foco) {
      // O ANEL MARCA A COLUNA, nao a linha: e a coluna que diz o que OK vai
      // fazer. Na aba "Fora" ha uma acao so, e o anel toma a linha inteira.
      GfxRect cel = (filAba == 0)
        ? (GfxRect){ cx + AJ_FIL_COL[filCampo].x - 12.0f, y, AJ_FIL_COL[filCampo].w + 24.0f, AJ_FIL_LINHA }
        : linha;
      gfx_rect(cel, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_FIL_LINHA, 0, raio, ar, ag, ab, 1.0f);
    }

    { float tx = cx + AJ_FIL_COL[0].x;
      float tw = AJ_FIL_COL[0].w;
      // SELO DE ORIGEM ANTES DO NOME: icone, porque a 3 m dois cinzas sao
      // iguais e a forma sobrevive ao idioma.
      { int orig = fil_linha_origem(idx);
        GfxRect ic = { tx, y + (AJ_FIL_LINHA - 26.0f) * 0.5f, 26.0f, 26.0f };
        gfx_icone(ic, fil_origem_icone(orig), 0.78f, 0.80f, 0.85f, aTexto * 0.9f);
        tx += 38.0f; tw -= 38.0f; }
      if (filPegou && foco) {
        TxtLinha m = txt_linha(TXT_CALLOUT, "\xe2\x87\x95", 250, 250, 252, 255);
        txt_desenhar(m, tx, y + (AJ_FIL_LINHA - m.h) * 0.5f);
        tx += m.w + 12.0f; tw -= m.w + 12.0f;
      }
      { const char *addon = fil_linha_addon(idx);
        l = txt_linha_corta(TXT_CALLOUT, fil_titulo(idx), c, c, c, 255, tw);
        txt_desenhar_alpha(l, tx, y + 8.0f, aTexto);
        // Na aba 0 o addon vai sob o titulo; na aba 1 ele tem coluna propria,
        // porque e o agrupamento — e o cabecalho de grupo e a mudanca de nome.
        if (filAba == 0 && addon[0]) {
          TxtLinha ad = txt_linha_corta(TXT_MINI, addon, 148, 151, 160, 255, tw);
          txt_desenhar_alpha(ad, tx, y + 8.0f + l.h + 4.0f, aTexto * 0.85f);
        }
      } }

    if (filAba == 0) {
      { const char *est = naFila ? "Na fila" : "Na Home";
        int er = naFila ? 226 : 150, eg = naFila ? 186 : 214, eb = naFila ? 108 : 158;
        // A coluna "Estado" e tambem a acao REMOVER: com o foco nela, diz.
        if (foco && filCampo == 1) { est = "Remover"; er = 240; eg = 200; eb = 200; }
        l = txt_linha_corta(TXT_CALLOUT, est, er, eg, eb, 255, AJ_FIL_COL[1].w);
        txt_desenhar_alpha(l, cx + AJ_FIL_COL[1].x, y + (AJ_FIL_LINHA - l.h) * 0.5f, 1.0f); }
      { int aceita = fil_aceita_tipo(idx);
        const char *rot = aceita ? fil_tipo_rotulo(fil_linha_tipo(idx)) : "Fixo";
        int cc = aceita ? 220 : 150;
        l = txt_linha_corta(TXT_CALLOUT, rot, cc, cc, cc, 255, AJ_FIL_COL[2].w);
        txt_desenhar_alpha(l, cx + AJ_FIL_COL[2].x, y + (AJ_FIL_LINHA - l.h) * 0.5f, aTexto); }
      { l = txt_linha_corta(TXT_CALLOUT, fil_tam_rotulo(fil_linha_tam(idx)), 220, 220, 220, 255, AJ_FIL_COL[3].w);
        txt_desenhar_alpha(l, cx + AJ_FIL_COL[3].x, y + (AJ_FIL_LINHA - l.h) * 0.5f, aTexto); }
    } else {
      // GRUPO: o nome do addon aparece na PRIMEIRA linha de cada bloco e some
      // nas seguintes — e o agrupamento que a pessoa pediu, sem gastar linha
      // de cabecalho numa lista que ja e longa.
      const char *addon = fil_linha_addon(idx);
      const char *rot = addon[0] ? addon : i18n(fil_origem_rotulo(fil_linha_origem(idx)));
      int primeiro = !filForaAgrupada || (i == 0) ||
                     strcasecmp(fil_linha_addon(filLista[i - 1]), addon) != 0 ||
                     fil_linha_origem(filLista[i - 1]) != fil_linha_origem(idx);
      if (primeiro) {
        l = txt_linha_corta(TXT_CALLOUT, rot, 200, 203, 212, 255, AJ_FIL_COL[1].w + AJ_FIL_COL[2].w);
        txt_desenhar_alpha(l, cx + AJ_FIL_COL[1].x, y + (AJ_FIL_LINHA - l.h) * 0.5f, 1.0f);
      }
      { const char *acao = foco ? "OK  Adicionar à Home" : "";
        l = txt_linha(TXT_CALLOUT, acao, 150, 214, 158, 255);
        txt_desenhar_alpha(l, cx + AJ_FIL_COL[3].x, y + (AJ_FIL_LINHA - l.h) * 0.5f, 1.0f); }
    }
    y += AJ_FIL_LINHA + AJ_FIL_LGAP;
  }

  // Posicao na lista: com 200 linhas a barra de rolagem teria 6 px.
  if (n > 0) {
    if (filFoco >= n) snprintf(buf, sizeof buf, "%s", i18n("Botão"));
    else snprintf(buf, sizeof buf, i18n("%d de %d"), filFoco + 1, n);
    l = txt_linha(TXT_CAPTION, buf, 156, 159, 168, 255);
    txt_desenhar(l, cx + AJ_FIL_W - 40.0f - l.w, filCabecY);
  }

  // BOTOES no fim da lista, lado a lado. Aba "Fora": "Agrupar por addon" (a
  // alternancia agrupado/alfabetico) e "Atualizar tudo". Aba "Na Home": so
  // "Atualizar tudo" — la a ordem e a da home e a pessoa e quem a arruma.
  { float sy = y + 14.0f, bw = AJ_FIL_W - 48.0f, bx = cx + 24.0f;
    int nb = (filAba == 1) ? 2 : 1, b;
    float cada = (bw - (nb - 1) * 16.0f) / nb;
    for (b = 0; b < nb; b++) {
      int ehAgrupar = (filAba == 1 && b == 0);
      int pos = ehAgrupar ? n : (filAba == 1 ? n + 1 : n);
      int foco = (filFoco == pos && !filNaBarra);
      const char *rot = ehAgrupar ? (filForaAgrupada ? "Agrupado por addon · OK: lista alfabética"
                                                     : "Alfabética · OK: agrupar por addon")
                                  : "Atualizar tudo";
      GfxRect btn = { bx + b * (cada + 16.0f), sy, cada, 52.0f };
      gfx_cor(btn, 26.0f / cada, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, foco ? 0.72f : 0.30f);
      if (foco)
        gfx_rect(btn, 0, GFX_ANEL, 0, NV_ANEL_FOCO / 52.0f, 0, 26.0f / cada, ar, ag, ab, 1.0f);
      l = txt_linha(TXT_CALLOUT, rot, 220, 220, 220, 255);
      txt_desenhar(l, btn.x + (btn.w - l.w) * 0.5f, btn.y + (btn.h - l.h) * 0.5f);
    } }

  // A FICHA DA FILEIRA EM FOCO: de qual addon veio, filme ou serie, e quantos
  // titulos ela tem AGORA (omitido antes de a Home montar — "0 titulos" seria
  // mentira sobre uma fileira talvez cheia).
  if (n > 0 && filFoco >= 0 && filFoco < n && !filNaBarra) {
    int idx = filLista[filFoco];
    int orig = fil_linha_origem(idx);
    const char *addon = fil_linha_addon(idx);
    const char *cont  = fil_linha_conteudo(idx);
    int itens = fil_linha_itens(idx);
    char ficha[220];
    int k = snprintf(ficha, sizeof ficha, "%s", i18n(fil_origem_rotulo(orig)));
    if (addon && addon[0]) k += snprintf(ficha + k, sizeof ficha - (size_t)k, "  ·  %s", addon);
    if (cont && cont[0])   k += snprintf(ficha + k, sizeof ficha - (size_t)k, "  ·  %s", i18n(cont));
    if (itens >= 0)
      snprintf(ficha + k, sizeof ficha - (size_t)k,
               itens == 1 ? i18n("  ·  %d título") : i18n("  ·  %d títulos"), itens);
    l = txt_linha_corta(TXT_CALLOUT, ficha, 232, 234, 241, 255, AJ_FIL_W - 80.0f);
    txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 232.0f);
    l = txt_linha_corta(TXT_MINI, fil_origem_ajuda(orig), 170, 173, 182, 255, AJ_FIL_W - 80.0f);
    txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 200.0f);
  }

  // A INSTRUCAO, escrita na tela: o gesto de pegar e mover nao se descobre
  // sozinho num D-pad, e muda com o item na mao e com a aba.
  y = cartao.y + AJ_FIL_H - 168.0f;
  if (filPegou == 2) {
    txt_bloco(TXT_CAPTION,
              "↑ ↓  Mover o bloco do addon\n← →  Mover so a fileira\nOK  Soltar aqui\nVoltar  Cancelar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 4);
  } else if (filPegou) {
    txt_bloco(TXT_CAPTION,
              "↑ ↓  Mover a fileira\n← →  Mover o bloco do addon\nOK  Soltar aqui\nVoltar  Cancelar o movimento",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 4);
  } else if (filNaBarra) {
    txt_bloco(TXT_CAPTION, "← →  Trocar de aba\n↓  Voltar à lista\nVoltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 3);
  } else if (filAba == 1 && filFoco < n) {
    txt_bloco(TXT_CAPTION,
              "↑ ↓  Escolher fileira · segure para pular por letra\n"
              "OK  Adicionar à Home (entra na fila se a Home estiver cheia)\n"
              "↑ no topo  Abas\nVoltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 4);
  } else if ((filAba == 0 && filFoco == n) || (filAba == 1 && filFoco == n + 1)) {
    txt_bloco(TXT_CAPTION,
              "OK  Sincronizar a conta e refazer a Home agora (addons, coleções e fileiras)\nVoltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 2);
  } else if (filAba == 1 && filFoco == n) {
    txt_bloco(TXT_CAPTION, "OK  Alternar entre agrupado por addon e lista alfabética\nVoltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 2);
  } else {
    txt_bloco(TXT_CAPTION,
              "↑ ↓  Escolher fileira\n← →  Trocar de coluna\n"
              "OK  Pegar e mover (coluna Fileira) · Remover, trocar card e tamanho nas outras\n"
              "Voltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 4);
    if (filFoco < n && filCampo == 2 && !fil_aceita_tipo(filLista[filFoco])) {
      l = txt_linha_corta(TXT_MINI, motivoFormaFixa(fil_chave(filLista[filFoco])),
                          176, 179, 188, 255, AJ_FIL_W - 80.0f);
      txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 42.0f);
    }
  }

  // AVISO ("Home cheia...", "Adicionada..."): pilula no rodape do cartao por
  // alguns segundos. E a resposta visivel ao OK, que na aba "Fora" nao muda
  // nada na linha em que a pessoa esta olhando.
  if (filAviso[0] && SDL_GetTicks() < filAvisoAte) {
    l = txt_linha(TXT_CALLOUT, filAviso, 20, 22, 28, 255);
    { GfxRect pil = { cx + (AJ_FIL_W - l.w - 56.0f) * 0.5f, cartao.y + AJ_FIL_H - 84.0f, l.w + 56.0f, 48.0f };
      gfx_cor(pil, NV_RAIO_PILL, 0.96f, 0.86f, 0.52f, 0.96f);
      txt_desenhar(l, pil.x + 28.0f, pil.y + (pil.h - l.h) * 0.5f); }
  }
}

void ajustes_desenhar(Uint32 agora) {
  (void)agora;
  // Fundo opaco proprio: a tela cobre tudo e nao pode depender de quem desenhou
  // antes dela — sem isto a home aparece entre as linhas da lista.
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  // A tela ja foi limpa com ESTA MESMA COR por glClearColor/glClear em
  // main.c antes de app_desenhar. Pintar por cima era uma camada de tela
  // cheia jogada fora por quadro — e o custo dominante nesta GPU e fill
  // rate (gfx.c registra que DUAS camadas de tela cheia derrubavam a
  // Mali-G71 para ~40fps). Nao repor sem antes mudar a cor do clear.
  (void)tela;

  // O cabecalho fica na borda do CONTEUDO (a do indice, agora a coluna mais a
  // esquerda), e nao na da lista: a nota em AJ_LISTA_X existe porque esta tela
  // ja foi a unica desalinhada das outras.
  TxtLinha tit = txt_linha(TXT_TITULO1, "Ajustes", 255, 255, 255, 255);
  txt_desenhar(tit, AJ_IDX_X, NV_MARGEM_Y);

  int sec = secaoAtual();
  char pos[120];
  snprintf(pos, sizeof pos, i18n("%s  ·  %d de %d"), i18n(SECOES[sec].titulo),
           focoOp - SECOES[sec].ini + 1, SECOES[sec].n);
  // AO LADO DO TITULO, e nao abaixo dele. Abaixo, esta linha caia exatamente
  // sobre o topo da primeira categoria — texto por cima de texto, visivel na
  // captura de 1080p. Alinhada pela base do titulo ela fica no espaco vazio a
  // direita, que era o unico lugar desperdicado do cabecalho.
  { TxtLinha contexto = txt_linha(TXT_CAPTION, pos, 178, 180, 186, 255);
    txt_desenhar(contexto, AJ_IDX_X + tit.w + 28.0f,
                 NV_MARGEM_Y + tit.h - contexto.h - 12.0f); }

  desenhaIndice();

  float hx = AJ_LISTA_X + AJ_LISTA_W + 52.0f;
  float hw = NV_TELA_W - NV_MARGEM_X - hx;
  if (hw > 240.0f) {
    // O ICONE DA CATEGORIA, grande, abre o painel. Ele nao e enfeite: e a mesma
    // marca da coluna da esquerda, e e o que liga "onde estou" a "o que estou
    // lendo" sem obrigar a ler dois titulos.
    GfxRect gi = { hx, AJ_TOPO + 4.0f, 56.0f, 56.0f };
    float hy;
    gfx_icone(gi, SECOES[sec].icone, 0.88f, 0.89f, 0.93f, 1.0f);
    { TxtLinha tipo = txt_linha(TXT_CAPTION, focoIndice ? "Categoria"
                          : inativa(focoOp) ? "Indisponível agora"
                          : soLeitura(focoOp) ? "Informação"
                          : OPCOES[focoOp].tipo == OP_ACAO ? "Abre uma tela"
                          : "Personalizar", 168, 171, 180, 255);
      txt_desenhar(tipo, gi.x + gi.w + 16.0f, gi.y + (gi.h - tipo.h) * 0.5f); }
    hy = gi.y + gi.h + 22.0f;
    hy += txt_bloco(TXT_HEADLINE,
                   focoIndice ? SECOES[sec].titulo : OPCOES[focoOp].rotulo,
                   237, 238, 242, hx, hy, hw, 40, 1, 3);
    hy += 18.0f;
    hy += txt_bloco(TXT_CAPTION,
                   focoIndice ? SECAO_AJUDA[sec] : ajudaOpcao(focoOp),
                   183, 186, 194, hx, hy, hw, 32, 1, 8);
    // O QUE MUDA NA PRATICA. A frase de ajuda diz o que a opcao E; esta diz o
    // que acontece quando ela muda, que e a pergunta de quem esta com o
    // controle na mao. Vazia quando nao ha nada honesto a dizer.
    { const char *ef = efeitoOpcao(focoOp);
      if (!focoIndice && ef) {
        hy += 16.0f;
        hy += txt_bloco(TXT_CAPTION, ef, 150, 176, 150, hx, hy, hw, 32, 1, 4);
      } }
    hy += 34.0f;
    // O RODAPE DE AJUDA DIZ O QUE FUNCIONA NO CONTROLE, e nao o que funciona no
    // teclado do Mac. Uma linha por dica, desenhada a mao: ver desenhaDicas.
    { const char *dIdx[] = { "↑ ↓   Escolher categoria",
                             "OK ou →   Entrar na categoria",
                             "Voltar   Sair dos ajustes" };
      const char *dAcao[] = { "↑ ↓   Navegar",
                              "OK   Abrir",
                              "Voltar   Ir para as categorias" };
      const char *dVal[] = { "↑ ↓   Navegar",
                             "OK   Alterar o valor",
                             "Voltar   Ir para as categorias" };
      const char *dEdi[] = { "← →   Alterar o valor",
                             "OK   Confirmar",
                             "Voltar   Confirmar" };
      const char *const *d = focoIndice ? dIdx
                           : emEdicao ? dEdi
                           : OPCOES[focoOp].tipo == OP_ACAO ? dAcao : dVal;
      desenhaDicas(d, 3, hx, hy, hw, 155, 159, 169); }
  }

  gfx_recorte(AJ_LISTA_X - NV_ANEL_FOCO, AJ_TOPO,
               AJ_LISTA_W + NV_ANEL_FOCO * 2, AJ_BASE - AJ_TOPO);
  float y = AJ_TOPO - scrollY;
  for (int s = 0; s < AJ_N_SECOES; s++) {
    if (s) y += AJ_SEC_GAP;
    // Cabecalho da secao: agora e o titulo GRANDE do grupo, e nao mais um
    // rotulo cinza do tamanho de legenda. Com seis categorias no lugar de doze,
    // cada uma cobre mais linhas e o cabecalho e o unico marco de onde um grupo
    // comeca — em corpo de legenda ele passava despercebido do sofa.
    float aC = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f);
    TxtLinha ts = txt_linha(TXT_HEADLINE, SECOES[s].titulo, 226, 228, 236, 255);
    if (aC > 0.005f && y < AJ_BASE)
      txt_desenhar_alpha(ts, AJ_LISTA_X + AJ_PAD, y + AJ_SEC_CABEC - ts.h - 6.0f, aC);
    y += AJ_SEC_CABEC;
    for (int k = 0; k < SECOES[s].n; k++) {
      int op = SECOES[s].ini + k;
      const char *sub = subsecaoDe(op);
      if (sub) {
        // Rotulo do bloco mais um fio: sem o fio, um rotulo cinza no meio de
        // linhas escuras se le como mais uma linha desligada.
        float aS = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f);
        if (aS > 0.005f && y < AJ_BASE) {
          TxtLinha tsub = txt_linha(TXT_CAPTION, sub, 156, 159, 168, 255);
          GfxRect fio = { AJ_LISTA_X + AJ_PAD + tsub.w + 18.0f,
                          y + AJ_SUB_CABEC - 18.0f,
                          AJ_LISTA_W - AJ_PAD * 2.0f - tsub.w - 18.0f, 1.0f };
          txt_desenhar_alpha(tsub, AJ_LISTA_X + AJ_PAD,
                             y + AJ_SUB_CABEC - tsub.h - 8.0f, aS);
          if (fio.w > 20.0f)
            gfx_cor(fio, 0.5f, 0.60f, 0.62f, 0.68f, 0.20f * aS);
        }
        y += AJ_SUB_CABEC;
      }
      desenhaLinha(op, y, animFoco[op]);
      y += AJ_LINHA_H + AJ_LINHA_GAP;
    }
  }
  gfx_sem_recorte();

  float total = yDaOpcao(AJ_N - 1) + AJ_LINHA_H;
  float janela = AJ_BASE - AJ_TOPO;
  if (total > janela) {
    float altura = janela * janela / total;
    float sy = AJ_TOPO + (janela - altura) * anim_clamp(scrollY / (total - janela), 0, 1);
    gfx_cor((GfxRect){ AJ_LISTA_X + AJ_LISTA_W + 18, AJ_TOPO, 3, janela },
            0.5f, 0.60f, 0.62f, 0.66f, 0.14f);
    gfx_cor((GfxRect){ AJ_LISTA_X + AJ_LISTA_W + 18, sy, 3, altura },
            0.5f, 0.80f, 0.82f, 0.86f, 0.8f);
  }
  { TxtLinha rodape = txt_linha(TXT_CAPTION,
        focoIndice ? "Categorias  ·  OK entra na categoria escolhida"
                   : "Voltar  Categorias  ·  Voltar de novo  Sair dos ajustes",
        156, 159, 168, 255);
    txt_desenhar(rodape, AJ_IDX_X, AJ_BASE + 20); }

  // A folha de fileiras cobre a lista; o vinculo cobre as duas, porque ele e a
  // unica coisa aqui com prazo (o codigo do dispositivo expira).
  if (filAberta) desenhaFileiras();

  // Por cima de tudo: enquanto um vinculo esta em andamento, ele e a pergunta
  // da tela.
  { TraEstado ta = traktauth_estado();
    SmkEstado sa = simklauth_estado();
    if (ta == TRA_PEDINDO || ta == TRA_AGUARDANDO || ta == TRA_ERRO)
      desenhaVinculo("o Trakt", traktauth_codigo(), traktauth_url(),
                     traktauth_erro(), ta == TRA_AGUARDANDO);
    else if (sa == SMK_PEDINDO || sa == SMK_AGUARDANDO || sa == SMK_ERRO)
      desenhaVinculo("o Simkl", simklauth_codigo(), simklauth_url(),
                     simklauth_erro(), sa == SMK_AGUARDANDO); }
}
