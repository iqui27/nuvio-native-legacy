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
#include "js.h"
#include <stdio.h>
#include <string.h>

// Versao do app: mesma string do appinfo.json empacotado. Fica aqui porque a
// tela nao tem como ler o manifesto em tempo de execucao no aparelho.
#define AJ_VERSAO       "1.0.17"

#define AJ_LINHA_H       88.0f
#define AJ_LINHA_GAP      8.0f
#define AJ_SEC_GAP       46.0f    // fim de uma secao ao cabecalho da proxima
#define AJ_SEC_CABEC     44.0f    // altura reservada ao cabecalho da secao
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
#define AJ_IDX_X        ajustes_conteudo_x()
#define AJ_IDX_W        196.0f
#define AJ_IDX_GAP       24.0f
#define AJ_IDX_H         58.0f
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
  AJ_CW_LIGADO, AJ_CW_ESTILO, AJ_CW_THUMB, AJ_CW_BLUR_PROX,
  AJ_CW_FURTHEST, AJ_CW_NAO_EXIBIDOS, AJ_CW_ORDEM,
  // Pagina de detalhe
  AJ_DET_BLUR_NAO_VISTOS, AJ_DET_TRAILER, AJ_DET_META_EXT, AJ_DET_DATA_CHEIA,
  // Foco no poster
  AJ_EXPANDIR, AJ_EXPANDIR_ATRASO, AJ_NAV_RAPIDA,
  // Profundidade
  AJ_PROF, AJ_PROF_BORDA, AJ_PROF_BRILHO, AJ_PROF_COBERTURA,
  AJ_PROF_POSTERS, AJ_PROF_CW, AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS,
  // Tamanho do item
  AJ_LARGURA_DP, AJ_RAIO_DP,
  // Interface
  AJ_IDIOMA, AJ_ANIM,
  // Conta
  AJ_PERFIL_ATIVO, AJ_SYNC, AJ_ADDONS, AJ_TRAKT, AJ_SIMKL, AJ_SAIR,
  // Sobre
  AJ_VERSAO_I, AJ_ESPACO,
  AJ_N
} OpcaoId;

static const char *V_QUALIDADE[] = { "Automática", "4K", "1080p", "720p" };
static const char *V_LIGA[]      = { "Ligado", "Desligado" };
static const char *V_IDIOMA[]    = { "Português", "English" };
static const char *V_ANIM[]      = { "Completas", "Reduzidas" };
// `collapseSidebar`: recolhida = a rail some e o conteudo comeca em 104.
static const char *V_RAIL[]      = { "Recolhida", "Fixa" };
// `continueWatchingCardStyle`, validado em layoutPreferences.js contra
// exatamente estes tres valores.
static const char *V_CW[]        = { "Card", "Largo", "P\xc3\xb4ster" };
// `continueWatchingSortMode`, normalizado em normalizeContinueWatchingSortMode.
static const char *V_CW_ORDEM[]  = { "Padrão", "Estilo streaming", "Separar futuros" };
// `discoverLocation`, validado contra estes tres.
static const char *V_DESCOBRIR[] = { "Mostrar na Busca", "Na barra lateral", "Desligado" };
// `homeImdbRatingsVisibility` — normalizeHomeImdbRatingsVisibility so aceita
// SHOW_ALL e HIDE_ALL.
static const char *V_NOTAS[]     = { "Mostrar", "Ocultar" };
// Preenchido em rotulosDeIdioma(), no arranque: os nomes saem de linguas.c em
// vez de serem uma segunda lista escrita a mao aqui. LING_MAX_OPC e folga: se
// linguas.c crescer, o excedente simplesmente nao aparece — melhor que ler
// fora do vetor.
// 32: a lista de linguas.c tem 30 entradas (2 acoes + 28 idiomas). O valor
// anterior era 24 e TRUNCAVA em silencio — os idiomas do fim da lista existiam
// em linguas.c e nao apareciam na tela.
#define LING_MAX_OPC 32
static const char *V_LINGUA[LING_MAX_OPC];
static int         nLingua;
static void rotulosDeIdioma(void) {
  int i, n = ling_opcao_n();
  if (n > LING_MAX_OPC) n = LING_MAX_OPC;
  for (i = 0; i < n; i++) {
    const char *c = ling_opcao_codigo(i);
    // "" = seguir a conta; "*" = mostrar tudo. Os dois primeiros sao acoes, nao
    // idiomas, e por isso tem rotulo proprio.
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

  LER("Perfil"),
  LER("Sincronização"),
  ACAO("Addons"),
  ACAO("Trakt"),
  ACAO("Simkl"),
  ACAO("Sair da conta"),
  LER("Versão"),
  LER("Memória usada por imagens"),
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
  "continueWatchingEnabled", "continueWatchingCardStyle",
  "useEpisodeThumbnailsInCw", "blurContinueWatchingNextUp",
  "nextUpFromFurthestEpisode", "showUnairedNextUp", "continueWatchingSortMode",
  "blurUnwatchedEpisodes", "detailPageTrailerButtonEnabled",
  "preferExternalMetaAddonDetail", "showFullReleaseDate",
  "focusedPosterBackdropExpandEnabled", "focusedPosterBackdropExpandDelaySeconds",
  "fastHorizontalNavigationEnabled",
  "cardDepthEnabled", "cardDepthEdgeStrength", "cardDepthSheenStrength",
  "cardDepthEdgeCoverage", "cardDepthPostersEnabled",
  "cardDepthContinueWatchingEnabled", "cardDepthEpisodeCardsEnabled",
  "cardDepthCastEnabled", "cardDepthTrailersEnabled",
  "posterCardWidthDp", "posterCardCornerRadiusDp",
  "idioma", "animacoes",
  // Conta: sao linhas locais, nao vem nem vao para o perfil na nuvem.
  "-perfil", "-sync", "-addons", "-trakt", "-simkl", "-sair",
  "-versao", "-espaco",
};

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
// parar neles, como no aparelho. Os titulos sao os do app web.
static const struct { const char *titulo; int ini, n; } SECOES[] = {
  { "Reprodução",                     AJ_QUALIDADE,           6 },
  { "Layout da Home",                 AJ_LANDSCAPE,           2 },
  { "Fileiras da Home",               AJ_FIL_LIMITE,          2 },
  { "Conteúdo da Home",               AJ_RAIL,               12 },
  { "Continuar assistindo",           AJ_CW_LIGADO,           7 },
  { "Página de Detalhes",             AJ_DET_BLUR_NAO_VISTOS, 4 },
  { "Foco no Pôster",                 AJ_EXPANDIR,            3 },
  { "Efeito de Profundidade",         AJ_PROF,                9 },
  { "Tamanho dos itens",              AJ_LARGURA_DP,          2 },
  { "Interface",                      AJ_IDIOMA,              2 },
  { "Conta",                          AJ_PERFIL_ATIVO,        6 },
  { "Sobre",                          AJ_VERSAO_I,            2 },
};
#define AJ_N_SECOES (int)(sizeof SECOES / sizeof *SECOES)

// Valor de cada opcao. Para OP_ESCOLHA e o indice; para OP_NUMERO e o proprio
// numero. Os padroes sao os DEFAULTS de layoutPreferences.js, com UMA excecao
// anotada linha a linha: as quatro que o perfil do dono diverge de fabrica
// nascem como ele as deixou, porque e o que ele ve hoje. Todas sao trocaveis
// aqui, que era o ponto.
// Definidas mais abaixo, junto do desenho das linhas; declaradas aqui porque a
// leitura do arquivo e o tratamento de tecla vem antes no arquivo.
static int  nValores(int op);
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
  1, 0,             /* idioma, animacoes */
  0, 0,             /* versao, espaco */
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
static int filPegou;             // 1 = item "na mao", cima/baixo movem ele
static int filPegouDe;           // de onde ele saiu, para Voltar cancelar
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
int ajustes_dolby_vision(void)        { return lig(AJ_DV); }
int ajustes_dolby_atmos(void)         { return lig(AJ_ATMOS); }
int ajustes_pausa_overlay(void)       { return lig(AJ_PAUSA_OVERLAY); }
int ajustes_idioma_ingles(void)       { return valor[AJ_IDIOMA] == 1; }

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
int ajustes_data_completa(void)       { return lig(AJ_DET_DATA_CHEIA); }
int ajustes_notas_home(void)          { return valor[AJ_NOTAS_HOME] == 0; }
int ajustes_local_descobrir(void)     { return valor[AJ_DESCOBRIR]; }
int ajustes_descobrir_na_busca(void)  { return valor[AJ_DESCOBRIR] == 0; }

int ajustes_cw_ligado(void)           { return lig(AJ_CW_LIGADO); }
int ajustes_cw_estilo(void)           { return valor[AJ_CW_ESTILO]; }
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
    case AJ_CW_ESTILO:  return W_CW;
    case AJ_CW_ORDEM:   return W_CW_ORDEM;
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

int ajustes_iniciar(void) {
  focoOp = 0; scrollY = 0.0f; sair = 0;
  focoIndice = 0;
  filAberta = 0; filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
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
  if (op == AJ_VERSAO_I) return AJ_VERSAO;
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
  if (op == AJ_SYNC) {
    switch (sync_estado()) {
      case SYNC_RODANDO: return "sincronizando…";
      case SYNC_FALHOU:  return "falhou";
      case SYNC_PRONTO:  return sync_resumo();
      default:           return sessao_logada() ? "aguardando" : "sem conta";
    }
  }
  if (op == AJ_TRAKT) {
    switch (traktauth_estado()) {
      case TRA_LIGADO:     return "conectado";
      case TRA_PEDINDO:    return "preparando…";
      case TRA_AGUARDANDO: return "aguardando";
      case TRA_ERRO:       return "falhou";
      default:             return "conectar";
    }
  }
  if (op == AJ_SIMKL) {
    switch (simklauth_estado()) {
      case SMK_LIGADO:     return "conectado";
      case SMK_PEDINDO:    return "preparando…";
      case SMK_AGUARDANDO: return "aguardando";
      case SMK_ERRO:       return "falhou";
      default:             return "conectar";
    }
  }
  if (op == AJ_ADDONS) {
    int i, lig = 0, n = addons_n();
    for (i = 0; i < n; i++) if (addons_ativo(i)) lig++;
    snprintf(buf, sizeof buf, i18n("%d de %d"), lig, n);
    return buf;
  }
  if (op == AJ_SAIR) return "OK";
  if (op == AJ_HERO_CATALOGOS) {
    // "Todos" com a lista vazia e o que o web escreve (common_all), e e o estado
    // do perfil do dono. Um "0" ali leria como "nenhum", o oposto do que e.
    if (heroCatalogos <= 0) return "Todos";
    snprintf(buf, sizeof buf, "%d", heroCatalogos);
    return buf;
  }
  int itens = 0, pend = 0; long bytes = 0;
  tex_estatisticas(&itens, &pend, &bytes);
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
    case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_FURTHEST:
    case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM:
      return !ajustes_cw_ligado();
    case AJ_CW_BLUR_PROX: return !ajustes_cw_ligado() || !ajustes_cw_thumb_episodio();
    case AJ_EXPANDIR_ATRASO: return !ajustes_expandir_poster();
    case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA:
    case AJ_PROF_POSTERS: case AJ_PROF_CW: case AJ_PROF_EPS:
    case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS:
      return !ajustes_profundidade();
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

static const char *ajudaOpcao(int op) {
  if (inativa(op)) {
    if (op == AJ_RAIL) return "Desative a barra lateral moderna para escolher entre recolhida e fixa.";
    if (op == AJ_RAIL_BLUR) return "Ative a barra lateral moderna para usar o desfoque.";
    if (op == AJ_HERO_CATALOGOS) return "Ative Mostrar destaque para exibir os catálogos no topo da Home.";
    if (op >= AJ_CW_ESTILO && op <= AJ_CW_ORDEM)
      return op == AJ_CW_BLUR_PROX && ajustes_cw_ligado()
        ? "Ative Miniatura do episódio para desfocar a imagem do próximo episódio."
        : "Ative Continuar assistindo para ajustar os cards de retomada.";
    if (op == AJ_EXPANDIR_ATRASO) return "Ative Expandir pôster ao focar para ajustar o tempo de espera.";
    return "Ative Efeito de profundidade para personalizar este detalhe.";
  }
  switch (op) {
    case AJ_QUALIDADE: return "Define a preferência de resolução. A disponibilidade depende das fontes do addon.";
    case AJ_DV: case AJ_ATMOS: return "Preferência para fontes compatíveis. O formato disponível também depende do arquivo e da TV.";
    case AJ_HERO_CATALOGOS: return "Quantidade de catálogos incluídos no destaque. Esta linha é apenas informativa.";
    case AJ_CW_FURTHEST: return "Escolhe o próximo episódio a partir do mais avançado marcado como assistido.";
    case AJ_CW_BLUR_PROX: case AJ_DET_BLUR_NAO_VISTOS: return "Oculta detalhes da miniatura para evitar spoilers de episódios ainda não assistidos.";
    case AJ_ANIM: return "Use Reduzidas para movimentos mais discretos ao navegar pela interface.";
    case AJ_ESPACO: return "Uso atual de memória pelo cache de imagens, não espaço ocupado no armazenamento da TV.";
    case AJ_VERSAO_I: return "Versão do aplicativo. Esta informação não pode ser alterada.";
    case AJ_FIL_LIMITE: return "Quantas fileiras a Home monta. Menos fileiras também significam menos catálogos pedidos pela rede, e não fileiras invisíveis.";
    case AJ_FIL_ORDEM: return "Abre a lista de fileiras para reordenar, ligar, desligar e escolher o card de cada uma. A escolha vale só nesta TV.";
    case AJ_LARGURA_DP: return "Ajusta a largura dos pôsteres nas fileiras que usam o tamanho personalizável.";
    case AJ_RAIO_DP: return "Controla o arredondamento dos cantos dos pôsteres.";
    default: return "Use as setas laterais para escolher. A preferência é aplicada ao alterar o valor.";
  }
}

// Deslocamento vertical do topo da lista ate a linha `op`, contando os
// cabecalhos das secoes que vieram antes.
static float yDaOpcao(int op) {
  float y = 0.0f;
  for (int s = 0; s < AJ_N_SECOES; s++) {
    y += (s ? AJ_SEC_GAP : 0.0f) + AJ_SEC_CABEC;
    for (int k = 0; k < SECOES[s].n; k++) {
      int o = SECOES[s].ini + k;
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
static void eventoFileiras(SDL_Keycode k) {
  int n = fil_n();
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) {
    if (filPegou) {
      // Voltar com o item na mao DESFAZ o movimento. Soltar e cancelar tem de
      // ser teclas diferentes: sem cancelamento, um movimento errado num
      // controle de TV so se conserta contando os passos de volta.
      //
      // O `passos` nao e paranoia: se a lista encolher enquanto o item esta na
      // mao (logout, ou um addon que sumiu), fil_mover devolve o MESMO indice
      // e um `while (filFoco != filPegouDe)` fica preso — trava o app com o
      // controle na mao da pessoa. Com o teto, o pior caso e o item ficar onde
      // esta.
      int passos = FIL_MAX + 1;
      while (filFoco != filPegouDe && passos-- > 0) {
        int antes = filFoco;
        filFoco = fil_mover(filFoco, filPegouDe > filFoco ? 1 : -1);
        if (filFoco == antes) break;
      }
      filPegou = 0;
    } else {
      filAberta = 0;
    }
    return;
  }
  if (n < 1) return;   // estado vazio: nao ha o que mover nem ligar
  if (filFoco >= n) filFoco = n - 1;
  if (k == SDLK_DOWN || k == SDLK_UP) {
    int dir = (k == SDLK_DOWN) ? 1 : -1;
    if (filPegou) filFoco = fil_mover(filFoco, dir);
    else if (filFoco + dir >= 0 && filFoco + dir < n) filFoco += dir;
    return;
  }
  if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    // Com o item na mao, esquerda/direita nao fazem nada de proposito: trocar de
    // coluna no meio de um movimento e o caminho mais curto para soltar a
    // fileira num lugar que a pessoa nao escolheu.
    if (filPegou) return;
    filCampo += (k == SDLK_RIGHT) ? 1 : -1;
    if (filCampo < 0) filCampo = 0;
    if (filCampo > AJ_FIL_CAMPOS - 1) filCampo = AJ_FIL_CAMPOS - 1;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    switch (filCampo) {
      case 0: if (!filPegou) { filPegou = 1; filPegouDe = filFoco; }
              else filPegou = 0;
              break;
      case 1: fil_alternar(filFoco); break;
      case 2: if (fil_aceita_tipo(filFoco)) fil_ciclar_tipo(filFoco); break;
      default: fil_ciclar_tam(filFoco); break;
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
    if (voltar) { focoIndice = 1; return; } }

  if (k == SDLK_DOWN)      { if (focoOp < AJ_N - 1) focoOp++; }
  else if (k == SDLK_UP)   { if (focoOp > 0)        focoOp--; }
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (OPCOES[focoOp].tipo != OP_ACAO) return;
    if (focoOp == AJ_FIL_ORDEM) {
      filAberta = 1; filFoco = 0; filCampo = 0; filPegou = 0; filTopo = 0;
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
    if (s >= 0 && s < AJ_N_SECOES) focoOp = SECOES[s].ini;
  }
  else if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    // Item so de leitura ou desligado pela dependencia nao muda com nada.
    if (!mutavel(focoOp)) return;
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
  // opcao sem dizer a que grupo ela pertence.
  float topo = yDaOpcao(focoOp);
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
  if (op == focoOp)
    gfx_rect(linha, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_LINHA_H, 0,
             AJ_RAIO, 0.96f, 0.96f, 0.97f, a);

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

  // As setas so aparecem na linha em foco que MUDA. Elas sao a instrucao: sem
  // elas, nada na tela diz que esquerda/direita e o gesto certo.
  if (podeMudar && f > 0.02f) {
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

  l = txt_linha(TXT_BODY, "No celular, abra:", 176, 178, 186, 255);
  txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
  y += 52.0f;
  l = txt_linha(TXT_TITULO3, endereco && endereco[0] ? endereco : "-", 255, 255, 255, 255);
  txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
  y += 92.0f;
  l = txt_linha(TXT_BODY, "e informe o código:", 176, 178, 186, 255);
  txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
  y += 66.0f;

  // Espacamento entre letras: um codigo curto sem tracking le como palavra, e
  // a pessoa transcreve errado.
  { float larg = txt_tracking(TXT_TITULO1, codigo, 255, 255, 255, -1.0f, 0.0f, 1.0f, 16.0f);
    txt_tracking(TXT_TITULO1, codigo, 255, 255, 255,
                 (NV_TELA_W - larg) * 0.5f, y, 1.0f, 16.0f); }
  y += 130.0f;

  if (esperando) {
    l = txt_linha(TXT_CAPTION, "Aguardando a autorização…", 150, 152, 160, 255);
    txt_desenhar(l, (NV_TELA_W - l.w) * 0.5f, y);
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
  float raio = 12.0f / AJ_IDX_H;
  int s;
  for (s = 0; s < AJ_N_SECOES; s++) {
    GfxRect r = { AJ_IDX_X, y, AJ_IDX_W, AJ_IDX_H };
    int atual = (s == sec);
    int c = atual ? 240 : 172;
    TxtLinha t;
    gfx_cor(r, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
            atual ? (focoIndice ? 1.0f : 0.60f) : 0.28f);
    if (atual && focoIndice)
      gfx_rect(r, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_IDX_H, 0, raio,
               0.96f, 0.96f, 0.97f, 1.0f);
    t = txt_linha_corta(TXT_CAPTION, SECOES[s].titulo, c, c, c, 255,
                        AJ_IDX_W - 28.0f);
    txt_desenhar(t, AJ_IDX_X + 14.0f, y + (AJ_IDX_H - t.h) * 0.5f);
    y += AJ_IDX_H + 4.0f;
  }
}

// --- FOLHA "ORDENAR E ATIVAR FILEIRAS" --------------------------------------
#define AJ_FIL_W      1480.0f
#define AJ_FIL_H       920.0f
#define AJ_FIL_Y        80.0f
#define AJ_FIL_LINHA    64.0f
#define AJ_FIL_LGAP      6.0f
#define AJ_FIL_VIS       8      // linhas desenhadas por vez

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
  int n = fil_n(), lim = fil_limite();
  int ligadas[FIL_MAX];
  int i, vistas = 0;
  float cx = cartao.x, y;
  TxtLinha l;
  char buf[120];

  // Veu quase opaco mais cartao solido: a lista de Ajustes atras atravessava o
  // texto do vinculo, e aqui ha texto pequeno em quatro colunas.
  gfx_cor(tela, 0.0f, 0.0f, 0.0f, 0.0f, 0.92f);
  gfx_cor(cartao, 0.03f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);

  l = txt_linha(TXT_TITULO2, "Fileiras da Home", 255, 255, 255, 255);
  txt_desenhar(l, cx + 40.0f, cartao.y + 34.0f);
  // FRASE MONTADA: i18n nas PARTES, porque a string final nunca casa com uma
  // chave da tabela (ver idioma.h).
  snprintf(buf, sizeof buf, i18n("As %d primeiras ligadas aparecem na Home"), lim);
  l = txt_linha(TXT_CAPTION, buf, 176, 179, 188, 255);
  txt_desenhar(l, cx + 40.0f, cartao.y + 34.0f + 46.0f);
  // A escolha e local, e a tela DIZ isso. Sem esta linha a pessoa espera que a
  // ordem apareça no celular dela, e ela nunca vai.
  l = txt_linha(TXT_MINI, "Vale só nesta TV · não altera a Home dos outros aparelhos",
                150, 153, 162, 255);
  txt_desenhar(l, cx + 40.0f, cartao.y + 34.0f + 78.0f);

  if (n < 1) {
    // ESTADO VAZIO com texto, e nao um cartao em branco: "titulo e nada abaixo"
    // se le como travamento.
    l = txt_linha(TXT_HEADLINE, "Nenhuma fileira conhecida ainda",
                  222, 224, 232, 255);
    txt_desenhar(l, cx + 40.0f, cartao.y + 220.0f);
    txt_bloco(TXT_CAPTION,
              "As fileiras aparecem aqui depois que o app lê os catálogos dos seus addons. "
              "Abra a Home, espere o catálogo carregar e volte a esta tela.",
              183, 186, 194, cx + 40.0f, cartao.y + 268.0f, AJ_FIL_W - 80.0f, 34, 1, 3);
    l = txt_linha(TXT_CAPTION, "Voltar  Fechar", 156, 159, 168, 255);
    txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 70.0f);
    return;
  }

  if (filFoco >= n) filFoco = n - 1;
  if (filFoco < 0) filFoco = 0;
  if (filFoco < filTopo) filTopo = filFoco;
  if (filFoco >= filTopo + AJ_FIL_VIS) filTopo = filFoco - AJ_FIL_VIS + 1;
  if (filTopo > n - AJ_FIL_VIS) filTopo = n - AJ_FIL_VIS;
  if (filTopo < 0) filTopo = 0;

  // Quantas LIGADAS existem ate cada linha. Precisa varrer desde o inicio
  // mesmo com a lista rolada: a posicao dentro do limite depende do que esta
  // acima, inclusive do que nao esta na tela.
  for (i = 0; i < n && i < FIL_MAX; i++) {
    if (!fil_linha_oculta(i)) vistas++;
    ligadas[i] = vistas;
  }

  // Cabecalho das colunas.
  for (i = 0; i < AJ_FIL_CAMPOS; i++) {
    l = txt_linha(TXT_MINI, AJ_FIL_COL[i].cabec, 148, 151, 160, 255);
    txt_desenhar(l, cx + AJ_FIL_COL[i].x, cartao.y + 148.0f);
  }

  y = cartao.y + 186.0f;
  for (i = filTopo; i < n && i < filTopo + AJ_FIL_VIS; i++) {
    GfxRect linha = { cx + 24.0f, y, AJ_FIL_W - 48.0f, AJ_FIL_LINHA };
    float raio = 12.0f / AJ_FIL_LINHA;
    int foco = (i == filFoco);
    int oculta = fil_linha_oculta(i);
    int fora = !oculta && ligadas[i] > lim;
    // Apagada por dois motivos DIFERENTES e por isso com dois pesos: desligada
    // e escolha da pessoa, fora do limite e consequencia do limite.
    float aTexto = oculta ? 0.55f : (fora ? 0.72f : 1.0f);
    int c = oculta ? 150 : 234;
    gfx_cor(linha, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
            filPegou && foco ? 1.0f : (foco ? 0.72f : 0.30f));
    if (foco) {
      // O ANEL MARCA A COLUNA, nao a linha: e a coluna que diz o que OK vai
      // fazer. Com o anel na linha inteira, as quatro acoes de OK ficariam
      // indistinguiveis.
      GfxRect cel = { cx + AJ_FIL_COL[filCampo].x - 12.0f, y,
                      AJ_FIL_COL[filCampo].w + 24.0f, AJ_FIL_LINHA };
      gfx_rect(cel, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_FIL_LINHA, 0, raio,
               0.96f, 0.96f, 0.97f, 1.0f);
    }

    { float tx = cx + AJ_FIL_COL[0].x;
      float tw = AJ_FIL_COL[0].w;
      if (filPegou && foco) {
        // Marca de "na mao". Sem ela, a linha pega e a linha em foco tem a
        // mesma cara e cima/baixo parecem ter deixado de navegar.
        TxtLinha m = txt_linha(TXT_CALLOUT, "\xe2\x87\x95", 250, 250, 252, 255);
        txt_desenhar(m, tx, y + (AJ_FIL_LINHA - m.h) * 0.5f);
        tx += m.w + 12.0f; tw -= m.w + 12.0f;
      }
      l = txt_linha_corta(TXT_CALLOUT, fil_titulo(i), c, c, c, 255, tw);
      txt_desenhar_alpha(l, tx, y + (AJ_FIL_LINHA - l.h) * 0.5f, aTexto); }

    { const char *est = oculta ? "Desligada" : (fora ? "Fora do limite" : "Ligada");
      int er = oculta ? 176 : (fora ? 226 : 150);
      int eg = oculta ? 122 : (fora ? 186 : 214);
      int eb = oculta ? 122 : (fora ? 108 : 158);
      l = txt_linha_corta(TXT_CALLOUT, est, er, eg, eb, 255, AJ_FIL_COL[1].w);
      txt_desenhar_alpha(l, cx + AJ_FIL_COL[1].x,
                         y + (AJ_FIL_LINHA - l.h) * 0.5f, 1.0f); }

    { int aceita = fil_aceita_tipo(i);
      // Sem i18n aqui: txt_linha_corta ja traduz toda linha que desenha
      // (text.c), e chamar duas vezes so daria uma busca binaria a mais.
      const char *rot = aceita ? fil_tipo_rotulo(fil_linha_tipo(i)) : "Fixo";
      int cc = aceita ? 220 : 150;
      l = txt_linha_corta(TXT_CALLOUT, rot, cc, cc, cc, 255, AJ_FIL_COL[2].w);
      txt_desenhar_alpha(l, cx + AJ_FIL_COL[2].x,
                         y + (AJ_FIL_LINHA - l.h) * 0.5f, aTexto); }

    { l = txt_linha_corta(TXT_CALLOUT, fil_tam_rotulo(fil_linha_tam(i)),
                          220, 220, 220, 255, AJ_FIL_COL[3].w);
      txt_desenhar_alpha(l, cx + AJ_FIL_COL[3].x,
                         y + (AJ_FIL_LINHA - l.h) * 0.5f, aTexto); }
    y += AJ_FIL_LINHA + AJ_FIL_LGAP;
  }

  // Posicao na lista, em vez de uma barra de rolagem sozinha: com 64 fileiras a
  // barra fica com 6 px e nao diz onde a pessoa esta.
  snprintf(buf, sizeof buf, i18n("%d de %d"), filFoco + 1, n);
  l = txt_linha(TXT_CAPTION, buf, 156, 159, 168, 255);
  txt_desenhar(l, cx + AJ_FIL_W - 40.0f - l.w, cartao.y + 148.0f);

  // A INSTRUCAO, escrita na tela. O gesto de pegar e mover nao se descobre
  // sozinho num D-pad, e ele muda quando o item esta na mao.
  y = cartao.y + AJ_FIL_H - 168.0f;
  if (filPegou) {
    txt_bloco(TXT_CAPTION,
              "↑ ↓  Mover a fileira\nOK  Soltar aqui\nVoltar  Cancelar o movimento",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 3);
  } else {
    txt_bloco(TXT_CAPTION,
              "↑ ↓  Escolher fileira\n← →  Trocar de coluna\n"
              "OK  Pegar e mover (coluna Fileira) · Ligar, trocar card e tamanho nas outras\n"
              "Voltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 4);
    if (filCampo == 2 && !fil_aceita_tipo(filFoco)) {
      l = txt_linha_corta(TXT_MINI, motivoFormaFixa(fil_chave(filFoco)),
                          176, 179, 188, 255, AJ_FIL_W - 80.0f);
      txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 42.0f);
    }
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
  char pos[80];
  snprintf(pos, sizeof pos, i18n("%s  ·  %d de %d"), i18n(SECOES[sec].titulo),
           focoOp - SECOES[sec].ini + 1, SECOES[sec].n);
  TxtLinha contexto = txt_linha(TXT_CAPTION, pos, 178, 180, 186, 255);
  txt_desenhar(contexto, AJ_IDX_X, NV_MARGEM_Y + tit.h + 10.0f);

  desenhaIndice();

  float hx = AJ_LISTA_X + AJ_LISTA_W + 52.0f;
  float hw = NV_TELA_W - NV_MARGEM_X - hx;
  if (hw > 240.0f) {
    TxtLinha tipo = txt_linha(TXT_CAPTION, focoIndice ? "Seções"
                        : inativa(focoOp) ? "Opção indisponível"
                        : soLeitura(focoOp) ? "Informação" : "Personalizar", 168, 171, 180, 255);
    txt_desenhar(tipo, hx, AJ_TOPO + AJ_SEC_CABEC);
    float hy = AJ_TOPO + AJ_SEC_CABEC + tipo.h + 22.0f;
    hy += txt_bloco(TXT_HEADLINE,
                   focoIndice ? SECOES[sec].titulo : OPCOES[focoOp].rotulo,
                   237, 238, 242, hx, hy, hw, 40, 1, 3);
    hy += 22.0f;
    hy += txt_bloco(TXT_CAPTION,
                   focoIndice ? "Escolha a categoria e entre nela. Cima e baixo trocam de categoria em vez de linha em linha."
                              : ajudaOpcao(focoOp),
                   183, 186, 194, hx, hy, hw, 32, 1, 7);
    hy += 42.0f;
    // O RODAPE DE AJUDA DIZ O QUE FUNCIONA NO CONTROLE, e nao o que funciona no
    // teclado do Mac. O texto anterior anunciava PgUp/PgDn, teclas que nenhum
    // controle de TV tem.
    txt_bloco(TXT_CAPTION,
              focoIndice ? "↑ ↓  Escolher categoria\nOK ou →  Entrar na categoria\nVoltar  Sair dos ajustes"
              : OPCOES[focoOp].tipo == OP_ACAO
                         ? "↑ ↓  Navegar\nOK  Abrir\nVoltar  Ir para as categorias"
                         : "↑ ↓  Navegar\n← →  Alterar valor\nVoltar  Ir para as categorias",
              155, 159, 169, hx, hy, hw, 34, 1, 4);
  }

  gfx_recorte(AJ_LISTA_X - NV_ANEL_FOCO, AJ_TOPO,
               AJ_LISTA_W + NV_ANEL_FOCO * 2, AJ_BASE - AJ_TOPO);
  float y = AJ_TOPO - scrollY;
  for (int s = 0; s < AJ_N_SECOES; s++) {
    if (s) y += AJ_SEC_GAP;
    // Cabecalho da secao em corpo pequeno e cinza: ele rotula o grupo, nao
    // compete com os rotulos das opcoes.
    float aC = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f);
    TxtLinha ts = txt_linha(TXT_CAPTION, SECOES[s].titulo, 150, 152, 160, 255);
    if (aC > 0.005f && y < AJ_BASE)
      txt_desenhar_alpha(ts, AJ_LISTA_X + AJ_PAD, y + AJ_SEC_CABEC - ts.h - 10.0f, aC);
    y += AJ_SEC_CABEC;
    for (int k = 0; k < SECOES[s].n; k++) {
      int op = SECOES[s].ini + k;
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
