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
#include "dados.h"
#include "stalker.h"
#include "xtream.h"
#include "teclado.h"
#include "descoberta.h"
#include "extras.h"
#include "fileiras.h"
#include "listas.h"
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
#include "simkl.h"
#include "qr.h"
#include "atualizacao.h"
#include "avisos.h"
#include "js.h"
#include "artehero.h"
#include "artereserva.h"
#include "corviva.h"
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

// TEXTO SOBRE A COR DE REALCE: BRANCO, a nao ser que o realce seja branco (ou
// quase). Regra do dono (21/09/2026): "se nao for branco o accent, a cor de
// texto tem que ser branca". Antes era 20 cravado, do tempo em que o realce
// era sempre branco; com o rosa o texto escuro ficava sujo. A medida mora em
// ajustes_acento_tinta, que todos os modulos usam.
#define AJ_TEXTO_ESCURO  (tintaFoco())
#define AJ_TEXTO_ESCURO2 (tintaFoco() > 128 ? 232 : 50)   // valor, um degrau abaixo
static int tintaFoco(void) { return ajustes_acento_tinta(NULL, NULL, NULL) > 0.5f ? 255 : 20; }
static int focoEscuro(void) { return tintaFoco() < 128; }   // superficie do foco e clara?
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
  AJ_PAUSA_OVERLAY, AJ_FONTE_MANUAL, AJ_FONTE_AUTO, AJ_FONTE_REPOR,
  // Layout da Home
  AJ_LANDSCAPE, AJ_HERO_CHEIO, AJ_HERO_FUNDO, AJ_HERO_ARTE_DIF, AJ_HERO_TRAILER,
  // Fileiras da Home
  AJ_FIL_LIMITE, AJ_FIL_ORDEM,
  // Conteudo da Home
  AJ_RAIL, AJ_RAIL_MODERNA, AJ_RAIL_BLUR, AJ_HERO, AJ_HERO_CATALOGOS,
  AJ_PS_FUNDO,
  AJ_DESCOBRIR, AJ_ROTULOS, AJ_NOME_ADDON, AJ_SUFIXO_TIPO,
  AJ_OCULTAR_NLANC, AJ_NOTAS_HOME, AJ_GRAD_CLASSICO,
  // Continuar assistindo
  AJ_CW_LIGADO, AJ_CW_OK, AJ_CW_FONTE, AJ_CW_ESTILO, AJ_CW_THUMB, AJ_CW_BLUR_PROX,
  AJ_CW_FURTHEST, AJ_CW_NAO_EXIBIDOS, AJ_CW_ORDEM,
  // Pagina de detalhe
  AJ_DET_BLUR_NAO_VISTOS, AJ_DET_TRAILER, AJ_DET_META_EXT, AJ_DET_DATA_CHEIA,
  AJ_DET_VEU, AJ_DET_TRAILER_AUTO, AJ_TRAILER_QUAL, AJ_TRAILER_ASPECTO,
  AJ_TRAILER_FONTE,
  // Foco no poster
  AJ_EXPANDIR, AJ_EXPANDIR_ATRASO, AJ_NAV_RAPIDA, AJ_BORDA_FOCO,
  // Profundidade
  AJ_PROF, AJ_PROF_BORDA, AJ_PROF_BRILHO, AJ_PROF_COBERTURA,
  AJ_PROF_POSTERS, AJ_PROF_CW, AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS,
  // Tamanho do item
  AJ_LARGURA_DP, AJ_RAIO_DP, AJ_QUALIDADE_IMG,
  // Interface
  AJ_IDIOMA, AJ_ANIM, AJ_RESOLUCAO, AJ_TEMA,
  // Conta
  AJ_PERFIL_ATIVO, AJ_SYNC, AJ_ADDONS,
  AJ_STALKER_PORTAL, AJ_STALKER_MAC, AJ_STALKER_LIMPAR,
  AJ_XTREAM_SERVIDOR, AJ_XTREAM_USUARIO, AJ_XTREAM_SENHA, AJ_XTREAM_LIMPAR,
  AJ_SALVOS_DEST, AJ_TRAKT, AJ_SIMKL, AJ_SAIR,
  // Sobre
  AJ_VERSAO_I, AJ_ATUALIZAR, AJ_ENVIAR_LOG, AJ_ENVIO_AUTO, AJ_ESPACO, AJ_TEX_MB,
  // Integracoes — TMDB (tmdb_settings do blob da conta, ver
  // profileSettingsSyncService.js do web)
  AJ_TMDB_LIGADO, AJ_TMDB_IDIOMA, AJ_TMDB_ARTE, AJ_TMDB_BASICO, AJ_TMDB_FICHA,
  AJ_TMDB_DATAS,
  AJ_TMDB_ELENCO, AJ_TMDB_PROD, AJ_TMDB_REDES, AJ_TMDB_EPS, AJ_TMDB_TRAILERS,
  AJ_TMDB_MAIS, AJ_TMDB_COL, AJ_TMDB_CW,
  // Integracoes — MDBList (mdblist_settings do blob)
  AJ_MDB_LIGADO, AJ_MDB_CHAVE, AJ_MDB_TRAKT, AJ_MDB_IMDB, AJ_MDB_TMDB,
  AJ_MDB_LETTER, AJ_MDB_TOMATES, AJ_MDB_AUDIENCIA, AJ_MDB_META, AJ_MDB_MAL,
  // Integracoes — fanart.tv (fonte do destaque, so com chave pessoal)
  AJ_FANART_CHAVE,
  AJ_DIAGNOSTICO,
  AJ_N
} OpcaoId;

static const char *V_QUALIDADE[] = { "Automática", "4K", "1080p", "720p" };
static const char *V_LIGA[]      = { "Ligado", "Desligado" };
// "Fonte automatica" (issue #130). O INDICE e o gravado (fonteAutoLocal) e o
// FONTEAUTO_* de fonteauto.h: 0 = a regra de pontuacao, 1 = a primeira da
// lista do addon, e so ela — o "Auto-play first source" do Nuvio.
static const char *V_FONTE_AUTO[] = { "Melhor fonte", "Primeira da lista" };
// Quantas OUTRAS fontes o automatico tenta quando a escolhida nao abre. O
// indice e o numero (fonteReporLocal); ate a 1.4.3 eram 7, fixos, e cada
// tentativa e mais um arquivo na conta de debrid da pessoa.
static const char *V_FONTE_REPOR[] = { "Desligado", "1 fonte", "2 fontes", "3 fontes" };
// #90: o fundo de arte da tela de escolha de perfil (psfundo.c). "Automático"
// e o comportamento atual (perfil primeiro, catalogo como reserva, com
// rotacao); "Desligado" volta a tela ao que era antes da issue — psfundo nao
// desenha nada e so `profile_background_url` do proprio perfil (se houver)
// aparece. NAO tem as fontes de fil_hero_fonte() (topo do catalogo / fileira
// escolhida): aquela preferencia e por PERFIL, e aqui, antes de alguem
// escolher um perfil, nao ha perfil para ler a preferencia dele (ver a nota em
// perfilsel.c).
static const char *V_PS_FUNDO[]  = { "Automático", "Desligado" };
// Teto de memoria para imagens. O indice vira MB em ajustes_tex_mb; 0 e a
// regra automatica pela RAM (tex_cache.c). Escolha por APARELHO: fica no
// ajustes.txt e nunca vai para a conta — a TV da sala e a do quarto nao tem
// a mesma RAM. Relato #71: a linha "Memoria usada por imagens" parecia um
// ajuste e nao era; este e.
// 400 e 512 so passam da trava em TV com 3 GB ou mais (C1/C2/C3); a C9 de 2,2
// GB para em 300, que e o unico valor acima do automatico medido em aparelho.
static const char *V_TEX_MB[]    = { "Automático", "96 MB", "160 MB", "240 MB", "300 MB", "400 MB", "512 MB" };
static const int   TEX_MB_DE[]   = { 0, 96, 160, 240, 300, 400, 512 };
// TRES PADROES DE IMAGEM. O nome diz o que a pessoa ganha, nao o que o cache
// faz: "Alta" e mais pixel de arte e mais memoria; "Baixa" e arte que chega
// antes e cabe em TV com pouca RAM.
static const char *V_QUALIMG[]   = { "Baixa", "Padrão", "Alta" };
static const char *V_QUALTRAIL[] = { "Máxima", "1080p", "720p", "480p" };
static const char *V_ASPTRAIL[]  = { "Zoom cinema", "Zoom leve", "Zoom ultra", "Original" };
// Fonte do trailer (trailerfonte.h). O indice e o gravado e o TRF_* do
// modulo: 0 Automatico, 1 Apple, 2 IMDb, 3 YouTube — nao reordenar.
static const char *V_TRAILFONTE[] = { "Automático", "Apple TV", "IMDb", "YouTube" };
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
//   Ambas  = conta primeiro, Trakt e Simkl preenchendo o que falta
//   Conta  = so o progresso da conta Nuvio (syncprog.c)
//   Trakt  = so o /sync/playback do Trakt
//   Simkl  = so o /sync/playback e o "watching" do Simkl (simkl.c, #110)
// O INDICE E O QUE ESTA GRAVADO em ajustes.txt ("cwFonteLocal N"), entao a
// ordem e contrato: "Simkl" entrou NO FIM para quem ja tinha 1 ou 2 continuar
// lendo Conta e Trakt. Os numeros tem nome em ajustes.h (AJ_CWF_*), e
// tests/simkl_cw.sh confere rotulo por indice. "Ambas" continua com o nome
// antigo mesmo sendo tres: e o rotulo que quem ja usa conhece, o Simkl so
// entra nela com vinculo feito, e a ajuda da linha diz a regra inteira.
static const char *V_CW_FONTE[]  = { "Ambas", "Conta Nuvio", "Trakt", "Simkl" };
// `continueWatchingSortMode`, normalizado em normalizeContinueWatchingSortMode.
static const char *V_CW_ORDEM[]  = { "Padrão", "Estilo streaming", "Separar futuros" };
// O que o toque curto de OK faz num card da retomada (issue #93): abre o
// episodio direto no player, ou abre a pagina do titulo como sempre fez.
// Local, como cwFonteLocal — o app oficial nao tem esta escolha.
static const char *V_CW_OK[]     = { "Retomar", "Abrir página" };
// `discoverLocation`, validado contra estes tres.
static const char *V_DESCOBRIR[] = { "Mostrar na Busca", "Na barra lateral", "Desligado" };
// `homeImdbRatingsVisibility` — normalizeHomeImdbRatingsVisibility so aceita
// SHOW_ALL e HIDE_ALL.
static const char *V_NOTAS[]     = { "Mostrar", "Ocultar" };
// Fonte da arte do hero. O valor 0 mantém o comportamento atual, incluindo
// still de episódio quando a fileira é Continuar assistindo. Os outros valores
// pedem a arte daquela fonte PARA O TITULO — a que veio no item ou, para TMDB
// e Trakt, a buscada pelo id do IMDb (url virtual, artereserva.h); nunca um
// fundo generico. Vale para destaque, detalhe e card deitado (artehero.h).
// O INDICE e o gravado ("heroFundoLocal N"): ordem e contrato (ARTEHERO_*).
// 23/09/2026: Apple TV, fanart.tv e Anime entraram NO FIM (5, 6, 7), pelo
// mesmo contrato — quem ja tinha 1..4 gravado continua lendo a mesma fonte.
static const char *V_HERO_FONTE[] = {
  "Automático", "Catálogo / Cinemeta", "IMDb / Metahub", "TMDB", "Trakt",
  "Apple TV", "fanart.tv", "Anime (Kitsu / AniList)"
};
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
//
// "Plan to Watch do Simkl" (#110) entrou NO FIM pelo mesmo motivo de
// V_CW_FONTE: o indice e o gravado ("salvosDestino N"), e 1 tem de continuar
// sendo o Trakt. Com ele o "+" publica no Plan to Watch, e o Plan to Watch
// passa a entrar nos Salvos (descoberta.c). Nomes em ajustes.h (AJ_SALVOS_*).
static const char *V_SALVOS[]    = { "Lista do Nuvio", "Watchlist do Trakt",
                                     "Plan to Watch do Simkl" };
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
// OS DOIS TEMAS DINAMICOS (cor viva, corviva.h) vem DEPOIS dos doze da conta,
// e a posicao nao e acaso: os indices 0..11 continuam sendo os de W_TEMA, e o
// ajustes.txt de quem ja escolheu um tema le igual. 12 = "Dinâmica" (o destaque
// segue a arte do titulo em cena), 13 = "Dinâmica estilizada" (e o fundo
// tambem, de leve).
#define AJ_TEMA_DINAMICA    AJ_N_TEMAS
#define AJ_TEMA_ESTILIZADA  (AJ_N_TEMAS + 1)
#define AJ_N_TEMAS_OPC      (AJ_N_TEMAS + 2)
static const char *V_TEMA[] = {
  "Branco", "Carmesim", "Oceano", "Violeta", "Esmeralda", "Âmbar",
  "Rosa", "Dourado", "Jade", "Ouro rosé", "Azul ártico", "Grafite",
  "Dinâmica", "Dinâmica estilizada"
};
_Static_assert(sizeof V_TEMA / sizeof *V_TEMA == AJ_N_TEMAS_OPC,
               "V_TEMA: os doze temas da conta e os dois dinamicos");
// MESMA ORDEM de TEMA_ACENTO e de V_TEMA: e o indice que liga os tres.
//
// SEM LITERAL PARA OS DINAMICOS, de proposito: o app web nao tem esse tema, e
// e esta lista que decide o que a conta aceita e o que sobe. Um "DYNAMIC" aqui
// deixaria um blob futuro ligar o dinamico sem a pessoa pedir nesta TV, e na
// subida gravaria na conta um valor que o web nao sabe desenhar. A regra das
// duas direcoes esta em ajustes_aplicar_blob e ajustes_mesclar_blob
// (temaDinamico).
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
    V_LINGUA[i] = !c[0] ? "Da conta"
                : !strcmp(c, "*") ? "Todas"
                : !strcmp(c, "~") ? "Original do título"
                : ling_nome(c);
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

// Qual campo do portal esta sendo digitado: 0 = nenhum, AJ_STALKER_PORTAL ou
// AJ_STALKER_MAC. A modal e uma so; o destino do que sair dela e isto.
static int stCampo;

// Alfabetos da modal. Sao diferentes porque os campos sao diferentes: endereco
// precisa de ponto, dois pontos e hifen; MAC so de hexadecimal e dois pontos, e
// oferecer o resto so daria chance de digitar um MAC invalido.
static const char *ST_ALFA_PORTAL =
  "abcdefghijklmnopqrstuvwxyz0123456789.:-";
static const char *ST_ALFA_MAC = "0123456789abcdef:";
// Usuario e senha de Xtream sao o que o provedor gerou: letras dos dois casos,
// digitos e uns poucos sinais. Sem espaco — nenhum painel Xtream o aceita.
static const char *XT_ALFA_CONTA =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@!#$%&*+=";

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
  ESC("Escolher a fonte ao reproduzir", V_LIGA, 2), // local: ver ajustes_fonte_manual
  ESC("Fonte automática",           V_FONTE_AUTO, 2),  // local: ver fonteauto.h
  ESC("Outra fonte se falhar",      V_FONTE_REPOR, 4), // local: ver ajustes_fonte_repor

  ESC("Pôsteres horizontais",       V_LIGA, 2),   // modernLandscapePostersEnabled
  ESC("Fundo em tela cheia",        V_LIGA, 2),   // modernHeroFullScreenBackdropEnabled
  ESC("Background do hero",         V_HERO_FONTE, 8), // local: fonte real da arte
  // Card e destaque com fotos diferentes (dono, 22/09: "tem que ter um toggle
  // de ter uma versao diferente do que mostra no card do que ta na hero").
  // DESLIGADO por padrao: a mesma foto nos dois e o pedido de 19/09 (ver
  // artehero_url em artehero.c). A regra inteira esta em artehero.h.
  ESC("Destaque com outra arte",    V_LIGA, 2),   // local: heroDifferentFromCard
  // Trailer mudo no destaque do topo, alguns segundos depois de o foco parar
  // nele (dono, 20/09/2026: "coloca para tocar no hero tb", "nos ajustes o de
  // tocar no hero separadamente"). Separado do da pagina de titulo.
  ESC("Trailer no destaque",        V_LIGA, 2),

  // O limite NAO tem valor proprio em valor[]: ele mora em fileiras.c, que e
  // quem grava fileirasui.txt e quem a descoberta e a home consultam. A linha
  // aqui e um ESPELHO, sincronizado em ajustes_dir/ajustes_iniciar — duas
  // copias do mesmo numero divergem no primeiro caminho que esquecer uma.
  NUM("Limite de fileiras",         FIL_LIMITE_MIN, FIL_LIMITE_MAX, 1, NULL),
  ACAO("Ordenar e ativar fileiras"),
  // LOCAL (#95): a home grava coluna/rolagem em home-pos.txt. Ligado = ao
  // reabrir, cada fileira comeca no primeiro tile (o pedido da issue).

  ESC("Barra lateral",              V_RAIL, 2),   // collapseSidebar
  ESC("Barra lateral moderna",      V_LIGA, 2),   // modernSidebar
  ESC("Desfoque da barra moderna",  V_LIGA, 2),   // modernSidebarBlur
  ESC("Mostrar destaque",           V_LIGA, 2),   // heroSectionEnabled
  LER("Catálogos do destaque"),                   // heroCatalogKeys (contagem)
  ESC("Fundo da escolha de perfil", V_PS_FUNDO, 2), // local: ver psfundo.c
  ESC("Local do Descobrir",         V_DESCOBRIR, 3), // discoverLocation
  ESC("Rótulos nos pôsteres",       V_LIGA, 2),   // posterLabelsEnabled
  ESC("Nome do addon no catálogo",  V_LIGA, 2),   // catalogAddonNameEnabled
  ESC("Tipo de conteúdo",           V_LIGA, 2),   // catalogTypeSuffixEnabled
  ESC("Ocultar não lançados",       V_LIGA, 2),   // hideUnreleasedContent
  ESC("Avaliações gerais",          V_NOTAS, 2),  // homeImdbRatingsVisibility
  ESC("Gradiente de foco clássico", V_LIGA, 2),   // classicFocusGradientEnabled

  ESC("Mostrar \"Continuar assistindo\"", V_LIGA, 2), // continueWatchingEnabled
  ESC("OK no card",                     V_CW_OK, 2),  // local, ver V_CW_OK
  ESC("Fonte do \"Continuar assistindo\"", V_CW_FONTE, 4),   // local, ver V_CW_FONTE
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
  // Forca da vinheta escura sobre o fundo do titulo (dono, 20/09/2026: "mexer
  // na opacidade desse layer escuro, ate tirar"). 100 = a vinheta medida no
  // web; 0 = arte limpa. Local, sem chave no blob da conta.
  NUM("Escurecimento do fundo",     0, 100, 10, "%"),
  // Trailer mudo no lugar da arte, depois de a pagina assentar (dono,
  // 20/09/2026). Samsung: embed do YouTube; LG: MP4 do IMDb (trailer.h).
  ESC("Trailer automático",         V_LIGA, 2),
  // Definicao do MP4 do IMDb (trailerimdb.h): "Máxima" pega a maior que o
  // IMDb tem (1080p hoje); as outras sao tetos.
  ESC("Qualidade do trailer",       V_QUALTRAIL, 4),
  // Mesmos zooms do player (player.h): o trailer do IMDb vem 16:9 com a
  // tarja do 2.39:1 embutida, e "Zoom cinema" (1,34) e o que a tira.
  ESC("Proporção do trailer",       V_ASPTRAIL, 4),
  // De onde vem o trailer (dono, 22/09/2026: "deixa o toggle no settings de
  // qual o source do trailer"). Vale para a pagina de titulo e o destaque;
  // a regra inteira, com o que cada TV toca, esta em trailerfonte.h.
  ESC("Fonte do trailer",           V_TRAILFONTE, 4),

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
  ESC("Qualidade da imagem",        V_QUALIMG, 3),

  ESC("Idioma",                     V_IDIOMA, 2),
  ESC("Animações",                  V_ANIM, 2),
  ESC("Resolução da interface",     V_RESOLUCAO, 2),
  ESC("Cor de destaque",            V_TEMA, AJ_N_TEMAS_OPC),  // selected_theme (+2 locais)

  LER("Perfil"),
  LER("Sincronização"),
  ACAO("Addons"),
  ACAO("Portal Stalker (MAC)"),
  ACAO("MAC do portal"),
  ACAO("Remover o portal Stalker"),
  ACAO("Servidor Xtream"),
  ACAO("Usuário Xtream"),
  ACAO("Senha Xtream"),
  ACAO("Remover o Xtream"),
  ESC("Onde o + salva",             V_SALVOS, 3),
  ACAO("Trakt"),
  ACAO("Simkl"),
  ACAO("Sair da conta"),
  LER("Versão"),
  // A porta de saida para quem dispensou o cartao. Ele aparece UMA VEZ por
  // versao (a marca em atualizacao-vista.txt), e sem esta linha "Depois"
  // significava "nunca mais nesta versao".
  ACAO("Atualizar o aplicativo"),
  // Manda o log DESTA sessao ao servico de recomendacoes (dono, 20/09/2026):
  // ate aqui so o cartao do crash mandava, e so o da sessao anterior.
  ACAO("Enviar registro"),
  // ENVIO AUTOMATICO (dono, 20/09/2026: "temos que pegar todos os logs para
  // resolver a Samsung"). Desligado de fabrica; o cartao de primeira vez no
  // Tizen pergunta (telemetria.c) e grava aqui. Ligado, avisos.c manda o
  // registro da sessao anterior no arranque e o desta a cada minuto.
  ESC("Enviar registros sozinho",   V_LIGA, 2),
  LER("Memória usada por imagens"),
  ESC("Memória para imagens",       V_TEX_MB, 7),

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
  // Chave PESSOAL do fanart.tv (fonte "fanart.tv" do destaque). Nunca vem no
  // pacote: a chave de projeto do fanart.tv e por aplicativo e nao pode ser
  // publicada; a pessoal cada um tira em fanart.tv/get-an-api-key.
  ACAO("Chave do fanart.tv"),
  ACAO("Diagnóstico e otimização"),
};

// Nome de cada opcao no arquivo. O formato era POSICIONAL — uma linha por
// opcao, na ordem do enum — e por isso acrescentar uma opcao no meio fazia o
// arquivo de quem ja tinha o app aplicar os valores errados, em silencio. Com
// chave por linha, opcao nova nasce no padrao e as antigas continuam onde
// estavam. Os nomes seguem os do app web onde existe correspondente.
static const char *CHAVE[] = {
  "qualidade", "dolbyVision", "dolbyAtmos",
  "legendaIdioma", "audioIdioma", "pauseOverlayEnabled",
  // Sem "-": grava no ajustes.txt como qualquer outra. Nao tem equivalente na
  // conta (o app web nao expoe esta escolha), entao o blob simplesmente nao
  // traz a chave e o valor local fica de pe.
  "escolherFonteManual",
  // LOCAIS (#130), mesma razao: o app web nao tem estas escolhas.
  "fonteAutoLocal", "fonteReporLocal",
  "modernLandscapePostersEnabled", "modernHeroFullScreenBackdropEnabled", "heroFundoLocal",
  // LOCAL, e em ingles como pedido: o app web nao tem esta escolha (grep em
  // NuvioWeb 0.3.38 por hero/backdrop: so buildHeroBackdropSources, sem
  // preferencia), entao nao ha chave dele para reusar.
  "heroDifferentFromCard", "trailerHero",
  // "-": local, nao vem da conta e nao vai para ajustes.txt. Os dois vivem em
  // fileirasui.txt (fileiras.c) e a conta nao tem chave equivalente — o teto do
  // web para este runtime e uma CONSTANTE (HOME_MAX_ROWS_LEGACY_TV), nao uma
  // preferencia, e a ordem da conta nunca pode ser ESCRITA pela TV.
  "-limiteFileiras", "-ordenarFileiras",
  // LOCAL (#95): nao ha chave no blob da conta — o app web nao expoe isto.
  "collapseSidebar", "modernSidebar", "modernSidebarBlur",
  "heroSectionEnabled", "-heroCatalogKeys",
  // LOCAL, e nao do web: o app oficial nao tem tela de escolha de perfil com
  // fundo de arte, entao nao ha campo equivalente no blob da conta.
  "perfilFundoLocal",
  "discoverLocation", "posterLabelsEnabled", "catalogAddonNameEnabled",
  "catalogTypeSuffixEnabled", "hideUnreleasedContent",
  "homeImdbRatingsVisibility", "classicFocusGradientEnabled",
  "continueWatchingEnabled",
  // LOCAL, e nao do web: o app oficial nao tem esta escolha, entao nao ha
  // campo dela no blob da conta. Chave propria para nao colidir com um nome
  // que o servidor possa criar depois.
  "cwOkLocal",
  // LOCAL, e nao do web: o app oficial nao tem esta escolha, entao nao ha
  // campo dela no blob da conta. Chave propria para nao colidir com um nome
  // que o servidor possa criar depois.
  "cwFonteLocal",
  "continueWatchingCardStyle",
  "useEpisodeThumbnailsInCw", "blurContinueWatchingNextUp",
  "nextUpFromFurthestEpisode", "showUnairedNextUp", "continueWatchingSortMode",
  "blurUnwatchedEpisodes", "detailPageTrailerButtonEnabled",
  "preferExternalMetaAddonDetail", "showFullReleaseDate", "detalheVeu", "trailerAuto", "trailerQualidade", "trailerAspecto",
  // LOCAL: o app web nao tem esta escolha (NuvioWeb 0.3.38: trailerSource e
  // estado interno da tela de detalhe, nenhuma chave em settings/), entao o
  // nome e proprio e somenteDesteAparelho o segura aqui.
  "trailerFonteLocal",
  "focusedPosterBackdropExpandEnabled", "focusedPosterBackdropExpandDelaySeconds",
  "fastHorizontalNavigationEnabled",
  "bordaFocoCartaz",
  "cardDepthEnabled", "cardDepthEdgeStrength", "cardDepthSheenStrength",
  "cardDepthEdgeCoverage", "cardDepthPostersEnabled",
  "cardDepthContinueWatchingEnabled", "cardDepthEpisodeCardsEnabled",
  "cardDepthCastEnabled", "cardDepthTrailersEnabled",
  "posterCardWidthDp", "posterCardCornerRadiusDp", "qualidadeImagem",
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
  "-perfil", "-sync", "-addons",
  // Portal IPTV: os VALORES moram em stalker-p<N>.txt, por perfil, porque um
  // deles e credencial (ver stalker.h). Aqui sao so linhas de tela, e por isso
  // levam "-": nada delas entra no ajustes.txt nem no blob da conta.
  "-stalkerPortal", "-stalkerMac", "-stalkerLimpar",
  "-xtreamServidor", "-xtreamUsuario", "-xtreamSenha", "-xtreamLimpar",
  "salvosDestino", "-trakt", "-simkl", "-sair",
  "-versao", "-atualizar", "-registro", "envioAuto", "-espaco", "texturasMB",
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
  // "-": o VALOR mora em fanart.txt (dados), por aparelho; e credencial, nao
  // entra no ajustes.txt nem no blob da conta.
  "-fanartChave",
  "-diagnostico",
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
// `icone` e o basename em art/icones. Nos Ajustes sao os aj_* do Lucide (ISC),
// rasterizados por tools/icones-lucide.sh — nunca forma desenhada a mao (ver
// gfx_icone). Eram os PNG do app web, e com seis desenhos para oito categorias
// "aspecto" marcava Cartazes E Diagnostico: a ancora do olho apontava para dois
// lugares. Agora cada categoria tem o seu.
// `curto` e o nome na COLUNA de categorias, que tem 240 px: "Continuar
// assistindo" nao cabe la em corpo legivel do sofa, e cortar justamente o nome
// da categoria e o pior corte possivel. `titulo` continua sendo o nome inteiro,
// usado no cabecalho da lista, na linha de contexto e na area de ajuda.
// O TAMANHO DA CATEGORIA NAO SE ESCREVE: ele e a distancia ate a proxima.
//
// Aqui havia uma coluna com a contagem de cada faixa, escrita a mao. Ela
// DESCASOU do enum: "Pôsteres e cards" dizia 14 e a faixa tem 15, "Interface e
// conta" dizia 13 e tem 14. O efeito nao foi um erro visivel — foram DUAS
// opcoes que deixaram de existir na tela, "Arredondamento do cartaz" e
// "Memória usada por imagens": nenhuma categoria as continha, entao nenhum laco
// de desenho chegava nelas. O painel do cache de imagens continuava sendo
// desenhado quando o foco caia na opcao orfa, o que dava a cena que o dono
// fotografou — o painel a direita e nenhuma linha correspondente a esquerda.
//
// Com `n` derivado do `ini` da categoria seguinte, inserir uma opcao no enum
// passa a ser suficiente: ela entra na categoria em que foi escrita e ninguem
// precisa lembrar de somar um numero noutro lugar. A ultima categoria vai ate
// AJ_N, que o compilador mantem certo sozinho.
static const struct {
  const char *titulo, *curto, *icone;
  int ini;
} SECOES[] = {
  { "Reprodução",           "Reprodução", "aj_circle-play",          AJ_QUALIDADE },
  { "Home",                 "Home",       "aj_house",                AJ_LANDSCAPE },
  { "Continuar assistindo", "Retomar",    "aj_rotate-ccw-clock",     AJ_CW_LIGADO },
  { "Página de detalhes",   "Detalhes",   "aj_file-text",            AJ_DET_BLUR_NAO_VISTOS },
  { "Pôsteres e cards",     "Cartazes",   "aj_gallery-vertical-end", AJ_EXPANDIR },
  { "Interface e conta",    "Conta",      "aj_user-round-cog",       AJ_IDIOMA },
  { "Integrações",          "Integrações","aj_plug",                 AJ_TMDB_LIGADO },
  { "Diagnóstico",          "Diagnóstico", "aj_activity",            AJ_DIAGNOSTICO },
};
#define AJ_N_SECOES (int)(sizeof SECOES / sizeof *SECOES)

// Quantas opcoes a categoria tem: ate onde a proxima comeca, e a ultima ate o
// fim do enum.
static int secN(int s) {
  return (s + 1 < AJ_N_SECOES ? SECOES[s + 1].ini : (int)AJ_N) - SECOES[s].ini;
}

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
  "Mede addons, artes e fontes nesta TV e aplica o perfil de Qualidade ou Desempenho.",
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
  { AJ_FANART_CHAVE, "fanart.tv" },
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
  0,                /* idioma de legenda: 0 = seguir a conta */
  // AUDIO NO ORIGINAL POR PADRAO (LING_OPC_ORIGINAL, o ULTIMO item de
  // OPCOES_COD em linguas.c — ele fica no fim porque este arquivo grava o
  // INDICE da opcao, e inserir no meio reinterpreta ajustes.txt ja salvo).
  // Pedido do dono: "o que eu quero que seja padrao e tocar o audio na
  // linguagem original". Antes era 0 = "Da conta", e uma conta com "Portugues"
  // salvo fazia o app pular para a dublagem sem ninguem ter pedido naquele
  // filme.
  //
  // SO VALE PARA INSTALACAO NOVA: quem ja tem ajustes.txt continua com o que
  // esta gravado la, porque o arquivo ganha do padrao — de proposito, senao
  // toda atualizacao desfaria escolha de usuario.
  LING_OPC_ORIGINAL, /* idioma de audio: Original do titulo */
  0,                /* painel ao pausar: ligado (o default do web) */
  1,                /* escolher a fonte ao reproduzir: DESLIGADO (V_LIGA: 1 = Desligado) */
  0,                /* fonte automatica: melhor fonte (a regra de sempre) */
  2,                /* outra fonte se falhar: ate 2 (eram 7 fixas ate a 1.4.3) */

  0,                /* posteres deitados: LIGADO (perfil do dono; fabrica: desligado) */
  0,                /* fundo em tela cheia: LIGADO (perfil; fabrica: desligado) */
  0,                /* background do hero: seleção automática */
  1,                /* destaque com outra arte: DESLIGADO (mesma foto do card, 19/09) */
  0,                /* trailer no destaque: ligado */

  FIL_LIMITE_PADRAO,/* limite de fileiras: 7, o pedido do dono (espelho de fileiras.c) */
  0,                /* ordenar fileiras: acao */
  // AQUI HAVIA `1, /* resetar foco ao iniciar */`, UMA OPCAO QUE NAO EXISTE
  // no enum (entrou em 876742e so neste vetor). Esta lista e POSICIONAL: cada
  // padrao de AJ_RAIL ate AJ_RESOLUCAO caia na opcao SEGUINTE, e so voltava
  // a alinhar porque faltava o de AJ_TEMA la embaixo. Quem instalava do zero
  // nascia com barra fixa, barra moderna ligada, animacoes reduzidas e o
  // cartao com largura 0 e ARREDONDAMENTO 126 dp — a "pilula" que o dono viu
  // na janela do tests/heroarte_shot.sh (22/09), que roda sem ajustes.txt.
  // Quem ja tem ajustes.txt nao muda: o arquivo ganha do padrao.
  // tests/ajustes_padroes.sh confere os padroes desta faixa um a um.

  0,                /* barra lateral: recolhida (perfil; fabrica: fixa) */
  1,                /* barra lateral moderna: desligada */
  0,                /* desfoque da barra moderna: ligado (perfil) */
  0,                /* mostrar destaque: ligado */
  0,                /* catalogos do destaque: leitura */
  0,                /* fundo da escolha de perfil: automatico (padrao de fabrica, #90) */
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
  0,                /* OK no card: retomar (cwOkLocal; 1 = abrir pagina) */
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
  100,              /* escurecimento do fundo do titulo: vinheta inteira */
  0,                /* trailer automatico na pagina de titulo: ligado */
  0,                /* qualidade do trailer: maxima */
  0,                /* proporcao do trailer: zoom cinema */
  0,                /* fonte do trailer: automatico (Apple -> IMDb -> YouTube) */

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
  1,                /* qualidade da imagem: Padrão (0 Baixa, 1 Padrão, 2 Alta) */

  // Idioma 1 = English. O padrao NAO e o do dono do pacote: quem instala vem
  // do release publico, e ler uma interface em portugues sem ter escolhido e
  // pior do que ler em ingles sem ter escolhido. Quem prefere portugues troca
  // em Ajustes -> Interface, e a escolha fica gravada.
  1, 0, 0,          /* idioma, animacoes, resolucao (0 = 1080p) */
  0,                /* tema (cor de destaque): o primeiro, o de sempre */
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
  0, 0, 0, 1,       /* versao, atualizar, registro, envio sozinho: DESLIGADO */
  0,                /* espaco */
  0,                /* memoria para imagens: automatico */

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
  0,                /* chave do fanart.tv: acao (o valor mora em fanart.txt) */
  0,                /* diagnostico */
};

// Pedido de abrir a lista de addons, lido e zerado pelo app.c. A tela nao e
// aberta daqui porque quem troca de tela e o app.c — ajustes.c nao conhece as
// outras telas, e ganhar essa dependencia agora era o comeco de um no.
static int pediuAddons;
int ajustes_pediu_addons(void) { int v = pediuAddons; pediuAddons = 0; return v; }
static int pediuDiagnostico;
int ajustes_pediu_diagnostico(void) { int v = pediuDiagnostico; pediuDiagnostico = 0; return v; }

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
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velY = 0.0f;
static float paginaA = 1.0f;   // entrada da pagina da categoria (0..1)
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
int ajustes_fonte_manual(void)        { return lig(AJ_FONTE_MANUAL); }
int ajustes_fonte_primeira(void)      { return valor[AJ_FONTE_AUTO] == 1; }
int ajustes_fonte_repor(void) {
  int v = valor[AJ_FONTE_REPOR];
  return v < 0 ? 0 : v > 3 ? 3 : v;     // arquivo editado a mao: dentro da tabela
}
int ajustes_idioma_ingles(void)       { return valor[AJ_IDIOMA] == 1; }

// 1 = o tema escolhido e um dos dinamicos (cor viva), que so existem nesta TV.
static int temaDinamico(void) {
  return valor[AJ_TEMA] == AJ_TEMA_DINAMICA || valor[AJ_TEMA] == AJ_TEMA_ESTILIZADA;
}
int ajustes_cor_viva(void) {
  return valor[AJ_TEMA] == AJ_TEMA_DINAMICA   ? CORVIVA_SIMPLES
       : valor[AJ_TEMA] == AJ_TEMA_ESTILIZADA ? CORVIVA_ESTILIZADA
       : CORVIVA_DESLIGADA;
}

// Cor do ANEL DE FOCO. Ver TEMA_ACENTO: um tema aqui e so isto.
//
// Com o tema dinamico, a cor e a que corviva_quadro ja calculou para ESTE
// quadro (o laco principal anima uma vez; aqui so se copia). ~43 arquivos
// chamam esta funcao, varias vezes por quadro: nenhuma conta mora aqui.
void ajustes_acento(float *r, float *g, float *b) {
  int i = valor[AJ_TEMA];
  if (temaDinamico()) { corviva_acento(r, g, b); return; }
  if (i < 0 || i >= AJ_N_TEMAS) i = 0;   // arquivo de outra versao: branco
  if (r) *r = TEMA_ACENTO[i].r;
  if (g) *g = TEMA_ACENTO[i].g;
  if (b) *b = TEMA_ACENTO[i].b;
}

float ajustes_acento_tinta(float *r, float *g, float *b) {
  float cr, cg, cb, lum;
  ajustes_acento(&cr, &cg, &cb);
  if (r) *r = cr;
  if (g) *g = cg;
  if (b) *b = cb;
  lum = 0.2126f * cr + 0.7152f * cg + 0.0722f * cb;
  // So o realce BRANCO (ou quase) leva tinta escura; qualquer cor leva
  // branco. Regra do dono (21/09/2026), no lugar do corte a 0,55 que punha
  // texto escuro sobre o rosa e o amarelo.
  return lum > 0.88f ? 0.067f : 1.0f;
}
int ajustes_tinta_foco(void)  { return ajustes_acento_tinta(NULL, NULL, NULL) > 0.5f ? 255 : 20; }
// Secundario sobre realce colorido: 238, nao 225 — a 3 m, sobre rosa, 225
// ja lia como cinza (dono, 21/09/2026).
int ajustes_tinta_foco2(void) { return ajustes_acento_tinta(NULL, NULL, NULL) > 0.5f ? 238 : 60; }

// `collapseSidebar: modernSidebar ? false : Boolean(collapseSidebar)` — a barra
// moderna DESLIGA o recolhimento, e nao o contrario. Copiado de
// normalizeLayoutPreferences para nao inventar precedencia.
int ajustes_rail_moderna(void)        { return lig(AJ_RAIL_MODERNA); }
int ajustes_rail_recolhida(void)      { return ajustes_rail_moderna() ? 0 : lig(AJ_RAIL); }
int ajustes_rail_moderna_blur(void)   { return lig(AJ_RAIL_BLUR); }
int ajustes_hero_ligado(void)         { return lig(AJ_HERO); }
int ajustes_hero_cheio(void)          { return lig(AJ_HERO_CHEIO); }
int ajustes_hero_arte_diferente(void) { return lig(AJ_HERO_ARTE_DIF); }
int ajustes_hero_fonte(void) {
  int v = valor[AJ_HERO_FUNDO];
  return v >= 0 && v < (int)(sizeof V_HERO_FONTE / sizeof *V_HERO_FONTE) ? v : 0;
}
// #90: "Automático" (indice 0, padrao de fabrica) = psfundo.c continua
// escolhendo arte do catalogo como reserva; "Desligado" (indice 1) = a tela de
// perfil volta a nao desenhar nada alem do que o proprio perfil traz.
int ajustes_ps_fundo_automatico(void) { return lig(AJ_PS_FUNDO); }
int ajustes_tex_mb(void) {
  int i = valor[AJ_TEX_MB];
  return (i >= 0 && i < 7) ? TEX_MB_DE[i] : 0;
}
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
int ajustes_salvos_no_trakt(void)     { return valor[AJ_SALVOS_DEST] == AJ_SALVOS_TRAKT; }
int ajustes_salvos_no_simkl(void)     { return valor[AJ_SALVOS_DEST] == AJ_SALVOS_SIMKL; }
// Setter para o explicador de primeira vez (salvosintro.c), que faz esta
// pergunta antes de a pessoa chegar em Ajustes. Grava na hora: quem respondeu e
// desligou a TV nao deve ser perguntado de novo.
void ajustes_definir_salvos_no_trakt(int noTrakt) {
  valor[AJ_SALVOS_DEST] = noTrakt ? 1 : 0;
  gravar();
}
int ajustes_data_completa(void)       { return lig(AJ_DET_DATA_CHEIA); }
float ajustes_detalhe_veu(void)       { int v = valor[AJ_DET_VEU]; return (v < 0 ? 0 : v > 100 ? 100 : v) / 100.0f; }
int   ajustes_trailer_auto(void)      { return lig(AJ_DET_TRAILER_AUTO); }
int   ajustes_trailer_hero(void)      { return lig(AJ_HERO_TRAILER); }
float ajustes_trailer_zoom(void)      { static const float z[] = { 1.34f, 1.15f, 1.55f, 1.0f }; int v = valor[AJ_TRAILER_ASPECTO]; return (v >= 0 && v < 4) ? z[v] : 1.34f; }
// Teto de definicao do trailer: 0 = a maior que houver.
int   ajustes_trailer_qualidade(void) { static const int t[] = { 0, 1080, 720, 480 }; int v = valor[AJ_TRAILER_QUAL]; return (v >= 0 && v < 4) ? t[v] : 0; }
// Fonte do trailer: o TRF_* de trailerfonte.h. Fora da lista le Automatico.
int   ajustes_trailer_fonte(void)     { int v = valor[AJ_TRAILER_FONTE]; return (v >= 0 && v < 4) ? v : 0; }
int  ajustes_envio_auto(void)         { return lig(AJ_ENVIO_AUTO); }
void ajustes_definir_envio_auto(int ligado) { valor[AJ_ENVIO_AUTO] = ligado ? 0 : 1; gravar(); }
// ARTE DO DESTAQUE ESCOLHIDA PELO DIAGNOSTICO, e so depois de a pessoa ver a
// proposta na tela e apertar OK no botao (diagnostico.c): nunca sozinho. Os
// dois ajustes sao locais (ver somenteDesteAparelho), entao nao ha blob de conta para
// avisar. `fonte` e o indice ARTEHERO_* de V_HERO_FONTE; fora da faixa fica.
void ajustes_definir_destaque(int fonte, int diferente) {
  if (fonte >= 0 && fonte < (int)(sizeof V_HERO_FONTE / sizeof *V_HERO_FONTE))
    valor[AJ_HERO_FUNDO] = fonte;
  valor[AJ_HERO_ARTE_DIF] = diferente ? 0 : 1;
  gravar();
}
int ajustes_notas_home(void)          { return valor[AJ_NOTAS_HOME] == 0; }
int ajustes_local_descobrir(void)     { return valor[AJ_DESCOBRIR]; }
int ajustes_descobrir_na_busca(void)  { return valor[AJ_DESCOBRIR] == 0; }

int ajustes_cw_ligado(void)           { return lig(AJ_CW_LIGADO); }
int ajustes_cw_ok_toca(void)          { return valor[AJ_CW_OK] == 0; }
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
// 0 baixa, 1 padrao, 2 alta. Quem consome sao tex_cache (teto de decodificacao)
// e artehero (qual url pedir para a arte de tela cheia).
int   ajustes_qualidade_imagem(void)  { return valor[AJ_QUALIDADE_IMG]; }
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
    // AJ_CW_FONTE NAO TEM LITERAIS DO WEB, e devolvia W_CW ("card", "wide",
    // "poster") — copia da linha de baixo. Nao mordia porque a chave e local
    // (cwFonteLocal nunca esta no blob) e somenteDesteAparelho a barra na
    // subida; mas com o "Simkl" no indice 3 um blob com "cw_fonte_local":
    // "poster" viraria Simkl. Sem literais, texto desconhecido e "mantido".
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

// CHAVE PESSOAL DO FANART.TV. Mora em fanart.txt na pasta de dados (por
// aparelho, como o portal IPTV), nunca no ajustes.txt nem na conta, e so
// aparece mascarada. Quem usa e artereserva.c (fonte "fanart.tv" do destaque).
static char fanartChave[64];
static void fanartAplicar(void) {
  arte_fonte_chave_fanart(fanartChave);
  artehero_fanart_disponivel(fanartChave[0] != 0);
}
static void fanartCarregar(void) {
  char *t = dados_ler("fanart.txt");
  size_t i, k = 0;
  fanartChave[0] = 0;
  // So hexadecimal: e o formato da chave pessoal, e assim uma linha torta no
  // arquivo nao vira cabecalho nem pedaco de url.
  for (i = 0; t && t[i] && k + 1 < sizeof fanartChave; i++)
    if ((t[i] >= '0' && t[i] <= '9') || (t[i] >= 'a' && t[i] <= 'f')) fanartChave[k++] = t[i];
  fanartChave[k] = 0;
  free(t);
  fanartAplicar();
}
static void fanartDefinir(const char *txt) {
  size_t i, k = 0;
  char nova[64];
  for (i = 0; txt && txt[i] && k + 1 < sizeof nova; i++)
    if ((txt[i] >= '0' && txt[i] <= '9') || (txt[i] >= 'a' && txt[i] <= 'f')) nova[k++] = txt[i];
  nova[k] = 0;
  snprintf(fanartChave, sizeof fanartChave, "%s", nova);
  // Campo vazio = esquecer a chave.
  if (fanartChave[0]) dados_gravar("fanart.txt", fanartChave);
  else dados_apagar("fanart.txt");
  fanartAplicar();
}
static const char *fanartMascarada(void) {
  static char m[24];
  size_t n = strlen(fanartChave);
  if (!n) return i18n("Não configurado");
  snprintf(m, sizeof m, "····%s", n > 4 ? fanartChave + n - 4 : "");
  return m;
}

void ajustes_dir(const char *dir) {
  FILE *f;
  char caminho[600], linha[96];
  if (!dir || !*dir) return;
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  fanartCarregar();
  // ANTES DO LACO, e nao so no fim (#129): limita() confere as duas linhas de
  // idioma contra nValores() -> nLingua, e quem preenche nLingua e esta
  // chamada. No arranque ela ainda nao tinha rodado: a lista tinha "1 valor",
  // "legendaIdioma 3" era recusado como fora da faixa e a legenda voltava a
  // "Da conta". Quem salvava era a SEGUNDA leitura de main.c — e a migracao do
  // Tizen logo abaixo grava no meio da primeira, levando o padrao ao disco.
  rotulosDeIdioma();
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
#if defined(__EMSCRIPTEN__) && !defined(NV_TRAILER_AUTO_TIZEN)
  // MIGRACAO UNICA (1.3.10): o .wgt da 1.3.9 saiu de uma build com
  // NV_TRAILER_AUTO_TIZEN (a das fotos das notas), entao na Samsung o
  // autoplay do trailer nasceu LIGADO — e qualquer gravacao de ajustes
  // naquela versao escreveu "trailerAuto 0" no arquivo, o que o padrao novo
  // acima nao alcanca. Uma vez, marcada em disco, os dois voltam a
  // desligado; quem quiser liga de novo e a escolha fica.
  { char *m = dados_ler("trailer-1310.txt");
    if (m) free(m);
    else {
      // V_LIGA e { Ligado, Desligado }: 1 e DESLIGADO (ver lig()).
      valor[AJ_DET_TRAILER_AUTO] = 1;
      valor[AJ_HERO_TRAILER] = 1;
      dados_gravar("trailer-1310.txt", "1\n");
      gravar();
    } }
#endif
  // O limite mora em fileiras.c; esta linha e so o espelho dele. Ler daqui em
  // vez de gravar evita a divergencia: o arquivo de ajustes nao guarda o
  // numero, entao nao ha como os dois discordarem.
  valor[AJ_FIL_LIMITE] = fil_limite();
  // A escolha lida do disco so existe de verdade quando chega em linguas.c.
  rotulosDeIdioma();
  aplicarIdioma(AJ_LEG_LINGUA);
  aplicarIdioma(AJ_AUD_LINGUA);
  // O teto de imagens escolhido vale desde o arranque, nao so quando a tela
  // de Ajustes e aberta. tex_iniciar ja rodou (main.c); isto so o corrige.
  if (valor[AJ_TEX_MB] > 0) tex_definir_orcamento_mb(ajustes_tex_mb());
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
  // SAMSUNG (#85, 21/09/2026): o arquivo fica no IDBFS, e o IDBFS so vai ao
  // IndexedDB quando alguem marca o sistema de arquivos como sujo. Este
  // gravador escreve por fora de dados_gravar e nunca marcava: qualquer
  // ajuste local (borda do cartaz, tamanho do poster, profundidade, cor de
  // destaque, fonte ao reproduzir) sobrevivia so ate a proxima descarga que
  // OUTRO modulo pedisse — e num app que so navegou, ate o proximo arranque,
  // onde voltava ao padrao. Na LG o disco e real e nada disto acontecia.
  dados_marcar_sujo(0);
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
    // TEMA DINAMICO E DESTA TV, e a conta nao o desfaz. O blob so conhece os
    // doze temas do web (W_TEMA), entao o que vier dele e sempre um desses — e
    // aplica-lo sobre "Dinâmica" trocaria, a cada sincronizacao, a escolha que
    // a pessoa fez aqui pela que ela fez no celular. Com um tema FIXO aqui, a
    // conta continua mandando como sempre mandou.
    if (i == AJ_TEMA && temaDinamico()) {
      printf("[ajustes] selected_theme da conta mantido na conta: tema dinamico e local\n");
      continue;
    }
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

// ---------------------------------------------------------------- subir (#85)
//
// O CAMINHO DE VOLTA do blob: o que a pessoa mudou NESTA TV entra no objeto
// `settings_json` da conta e sync.c o empurra. Antes disto a TV so LIA o blob,
// e a unica defesa contra o proximo arranque desfazer a mudanca local era
// parar de aplicar a conta (ajustes-locais.txt) — isto e, divergir em silencio.
//
// A COSTURA E TEXTUAL, e nao um objeto remontado campo a campo. Duas razoes, as
// duas medidas neste projeto:
//   - o blob real tem MUITO mais chaves do que este app conhece (foi o que
//     estourou o vetor de 4096 bytes da primeira versao do leitor). Remontar o
//     objeto aqui mandaria de volta um blob com so as ~40 chaves daqui, e o
//     servidor guarda o que vier: a TV APAGARIA as preferencias que so o app
//     web tem. E o mesmo defeito da "lista vazia apaga tudo" da secao 1.6.
//   - a chave e aninhada por feature ("layout_settings", "player_settings",
//     "theme_settings"...) e este app nao sabe em qual feature cada chave vive.
//     Reescrevendo no lugar, nao precisa saber.
//
// Por isso a regra e estrita: SO CHAVE QUE JA EXISTE NO BLOB e reescrita, e
// somente o valor dela. Chave que a conta nao tem nao e inventada — inventa-la
// exigiria adivinhar a feature e o `type`, e um blob com forma errada e pior
// que uma chave a menos.
static int somenteDesteAparelho(int op) {
  switch (op) {
    // LAYOUT/APARELHO: nao sobem nem que o blob tenha a chave.
    // A regra que separa: se o valor descreve ESTA TV (RAM, painel, rede,
    // consentimento de registro) ou uma escolha que so este port tem, ele fica.
    // A TV da sala e a do quarto nao tem a mesma memoria nem a mesma tela, e a
    // conta e uma so.
    case AJ_RESOLUCAO:      /* pedir superficie 4K: depende do painel */
    case AJ_TEX_MB:         /* teto de memoria de imagem: depende da RAM */
    case AJ_QUALIDADE_IMG:  /* resolucao da arte decodificada: idem */
    case AJ_ENVIO_AUTO:     /* consentimento de envio de registro deste aparelho */
    case AJ_ANIM:           /* animacoes reduzidas: acessibilidade nesta TV */
    case AJ_IDIOMA:         /* idioma da interface deste aparelho */
    case AJ_FIL_LIMITE:     /* fileiras da home: por aparelho (fileirasui-p<N>.txt) */
    case AJ_FIL_ORDEM:
    case AJ_CW_FONTE:
    case AJ_BORDA_FOCO:
    case AJ_FONTE_MANUAL:
    case AJ_FONTE_AUTO:
    case AJ_FONTE_REPOR:
    case AJ_SALVOS_DEST:
    // Arte do destaque: o web nao tem as chaves (heroFundoLocal,
    // heroDifferentFromCard); ficam neste aparelho mesmo que um blob futuro
    // traga algo com o mesmo nome.
    case AJ_HERO_FUNDO:
    case AJ_HERO_ARTE_DIF:
    // Fonte do trailer: o que toca depende da TV (YouTube so na Samsung; o
    // IMDb da Samsung depende do servico de recomendacoes), entao a escolha e
    // deste aparelho.
    case AJ_TRAILER_FONTE:
      return 1;
    default:
      return 0;
  }
}

// Onde esta o valor de `chave` dentro de [ini,fim): *vi aponta o primeiro
// caractere do valor e *vf o seguinte ao ultimo. 1 quando achou.
//
// Procura a chave ENTRE ASPAS e exige os dois-pontos: sem isso, "value" casaria
// com o pedaco de "values" e com qualquer texto que contivesse a palavra.
static int acharValor(const char *ini, const char *fim, const char *chave,
                      const char **vi, const char **vf) {
  char alvo[96];
  const char *p;
  size_t n;
  n = (size_t)snprintf(alvo, sizeof alvo, "\"%s\"", chave);
  if (n >= sizeof alvo) return 0;
  for (p = ini; p && (p = strstr(p, alvo)) != NULL && p < fim; p += n) {
    const char *v = p + n;
    while (v < fim && (unsigned char)*v <= ' ') v++;
    if (v >= fim || *v != ':') continue;
    v++;
    while (v < fim && (unsigned char)*v <= ' ') v++;
    if (v >= fim) return 0;
    if (*v == '{' || *v == '[') { *vf = js_fim(v); }
    else if (*v == '"') {
      const char *q = v + 1;
      while (q < fim && *q != '"') q += (*q == '\\' && q + 1 < fim) ? 2 : 1;
      *vf = (q < fim) ? q + 1 : fim;
    } else {
      const char *q = v;
      while (q < fim && *q != ',' && *q != '}' && *q != ']' &&
             (unsigned char)*q > ' ') q++;
      *vf = q;
    }
    if (!*vf || *vf > fim) return 0;
    *vi = v;
    return 1;
  }
  return 0;
}

// O valor local de `op` no MESMO formato do que ja esta no blob. 0 quando nao
// da para escrever com fidelidade — e ai a chave nao e tocada, que e sempre a
// resposta certa: mandar um tipo diferente do que o servidor guarda faria o app
// web ler a preferencia errada, ou nenhuma.
static int textoDoValor(int op, const char *vi, const char *vf,
                        char *dst, size_t tam) {
  size_t n = (size_t)(vf - vi);
  if (n >= 4 && !strncmp(vi, "true", 4))  { snprintf(dst, tam, "%s", valor[op] == 0 ? "true" : "false"); return 1; }
  if (n >= 5 && !strncmp(vi, "false", 5)) { snprintf(dst, tam, "%s", valor[op] == 0 ? "true" : "false"); return 1; }
  if (*vi == '"') {
    const char *const *lit = literaisDe(op);
    int k, maiuscula = 0, temBaixa = 0;
    size_t i;
    if (!lit) return 0;
    for (k = 0; k <= valor[op]; k++) if (!lit[k]) return 0;   // fora da lista
    // A CAIXA DO SERVIDOR, e nao a do codigo JS. MEDIDO na TV: a conta guarda
    // estes enums em MAIUSCULA ("IN_SEARCH", "CARD") enquanto o web os escreve
    // em minuscula, e a leitura daqui e sem caixa justamente por isso. Na
    // escrita nao ha essa folga — devolver a caixa que o servidor ja usava e o
    // unico jeito de nao trocar o valor de forma para todo mundo.
    for (i = 1; i + 1 < n; i++) {
      if (vi[i] >= 'a' && vi[i] <= 'z') temBaixa = 1;
      if (vi[i] >= 'A' && vi[i] <= 'Z') maiuscula = 1;
    }
    maiuscula = maiuscula && !temBaixa;
    snprintf(dst, tam, "\"%s\"", lit[valor[op]]);
    if (maiuscula) for (i = 0; dst[i]; i++)
      if (dst[i] >= 'a' && dst[i] <= 'z') dst[i] = (char)(dst[i] - 'a' + 'A');
    return 1;
  }
  if ((*vi >= '0' && *vi <= '9') || *vi == '-' || *vi == '.') {
    snprintf(dst, tam, "%d", valor[op]);
    return 1;
  }
  return 0;   // null, objeto, array: nao ha o que escrever
}

typedef struct { const char *vi, *vf; char texto[160]; } Troca;

int ajustes_mesclar_blob(const char *base, char **saida) {
  Troca troca[AJ_N];
  int n = 0, i, j;
  const char *fim, *p;
  char *out;
  size_t cap, w = 0;

  if (saida) *saida = NULL;
  if (!base || !*base || !saida) return 0;
  fim = base + strlen(base);

  for (i = 0; i < AJ_N; i++) {
    char snake[80];
    const char *vi, *vf, *ei, *ef;
    if (OPCOES[i].tipo == OP_LEITURA || OPCOES[i].tipo == OP_ACAO) continue;
    if (!CHAVE[i] || CHAVE[i][0] == '-') continue;
    if (somenteDesteAparelho(i)) continue;
    // TEMA DINAMICO NAO SOBE. O web nao tem esse tema: gravar "DYNAMIC" na
    // conta deixaria o web sem tema que ele saiba desenhar, e gravar o padrao
    // ("WHITE") no lugar APAGARIA o tema que a pessoa escolheu la — o JADE do
    // celular viraria branco porque ela ligou o dinamico na TV. O valor da
    // conta fica exatamente como esta (a costura nao toca a chave), que e o
    // "padrao" certo: o ultimo tema fixo que a conta conhece.
    if (i == AJ_TEMA && temaDinamico()) continue;
    camelParaSnake(CHAVE[i], snake, sizeof snake);
    if (!acharValor(base, fim, snake, &vi, &vf) &&
        !acharValor(base, fim, CHAVE[i], &vi, &vf)) continue;
    // Valor embrulhado em {"type":...,"value":X}: o que se reescreve e o X.
    if (*vi == '{' && acharValor(vi, vf, "value", &ei, &ef)) { vi = ei; vf = ef; }
    if (!textoDoValor(i, vi, vf, troca[n].texto, sizeof troca[n].texto)) {
      printf("[ajustes] %s nao tem forma para subir; mantido como esta na conta\n",
             CHAVE[i]);
      continue;
    }
    // Ja igual: nao entra na costura. Serve tambem de freio — um ciclo em que
    // nada mudou de verdade nao gera push.
    if ((size_t)(vf - vi) == strlen(troca[n].texto) &&
        !strncmp(vi, troca[n].texto, strlen(troca[n].texto))) continue;
    troca[n].vi = vi; troca[n].vf = vf;
    n++;
  }
  if (!n) return 0;

  // Por posicao, para a costura ser uma passada so. n e pequeno (dezenas) e a
  // insercao simples e mais curta de conferir que a alternativa.
  for (i = 1; i < n; i++) {
    for (j = i; j > 0 && troca[j - 1].vi > troca[j].vi; j--) {
      Troca t = troca[j - 1];
      troca[j - 1] = troca[j]; troca[j] = t;
    }
  }

  cap = strlen(base) + 1;
  for (i = 0; i < n; i++) cap += strlen(troca[i].texto);
  out = (char *)malloc(cap);
  if (!out) return 0;
  p = base;
  for (i = 0; i < n; i++) {
    size_t pre = (size_t)(troca[i].vi - p), t = strlen(troca[i].texto);
    memcpy(out + w, p, pre); w += pre;
    memcpy(out + w, troca[i].texto, t); w += t;
    p = troca[i].vf;
  }
  memcpy(out + w, p, (size_t)(fim - p)); w += (size_t)(fim - p);
  out[w] = 0;
  *saida = out;
  printf("[ajustes] %d ajuste(s) desta TV entram no blob da conta\n", n);
  return n;
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
    esperado = SECOES[s].ini + secN(s);
  }
  if (esperado != AJ_N)
    printf("[ajustes] SECOES cobre %d de %d opcoes: %d linha(s) nao seriam "
           "desenhadas\n", esperado, (int)AJ_N, (int)AJ_N - esperado);
  fflush(stdout);
}

// TODO PADRAO CABE NA LISTA DELE. `valor[]` e posicional e escrito a mao, e
// uma opcao inserida no meio do enum sem o inicializador correspondente desloca
// todos os padroes seguintes — foi assim que o 7 do limite de fileiras foi
// parar numa lista de dois itens e a tela quebrou ao desenhar o valor.
//
// Conferir custa 91 comparacoes uma vez por arranque, e o que ela encontra e
// CORRIGIDO na hora: um padrao fora da lista vira o primeiro item. A linha no
// log diz qual opcao, para o conserto de verdade (o inicializador que falta)
// acontecer no lugar certo.
static int nValores(int op);
// O LIMITE E O DE nValores, e nao o `n` da tabela (#129). As duas linhas de
// idioma tem n=2 na tabela const e a lista real (nLingua) — conferir contra 2
// zerava todo idioma escolhido alem de "Da conta" a cada abertura desta tela,
// e a gravacao seguinte levava o zero ao disco: "o ingles nao fica salvo".
// Visto nos logs de campo da 1.4.1 a 1.4.3: "padrao fora da lista em 4
// (\"Idioma do áudio\"): 30 de 2 valores".
static void conferirPadroes(void) {
  int i;
  for (i = 0; i < AJ_N; i++) {
    const Opcao *o = &OPCOES[i];
    int n = nValores(i);
    if (o->tipo == OP_ESCOLHA && n > 0 && (valor[i] < 0 || valor[i] >= n)) {
      printf("[ajustes] padrao fora da lista em %d (\"%s\"): %d de %d valores"
             " — vetor `valor[]` desalinhado; usando o primeiro\n",
             i, o->rotulo, valor[i], n);
      valor[i] = 0;
    }
  }
  fflush(stdout);
}

int ajustes_iniciar(void) {
  conferirSecoes();
  // ANTES de conferir: e rotulosDeIdioma quem preenche nLingua.
  rotulosDeIdioma();
  conferirPadroes();
  // Fora do vetor posicional de proposito: aquele vetor ja esta com menos
  // entradas do que o enum (as ultimas ficam em 0), e um 1 no lugar errado
  // ligaria outra coisa. O arquivo, lido depois, sobrescreve.
  valor[AJ_ENVIO_AUTO] = 1;
  // BUG (#82/#86, 1.3.9 e 1.3.10): havia aqui um bloco Samsung escrevendo
  // AJ_DET_TRAILER_AUTO = AJ_HERO_TRAILER = 1 (desligado) como "padrao de
  // fabrica". So que ajustes_iniciar() roda TODA VEZ que a tela de Ajustes
  // abre (app.c, TELA_AJUSTES), depois de ajustes_dir() ja ter lido o
  // arquivo — e nada o relê. Quem ligava o trailer, saia e voltava
  // encontrava os dois desligados de novo, e a proxima gravacao levava o
  // desligado ao disco: e o "the settings aren't retained" do rawldon. O
  // padrao de fabrica e o valor[] posicional (1 = desligado nas duas
  // linhas), e quem veio da 1.3.9 com o autoplay nascido ligado recebe o
  // reset unico em ajustes_dir() (marca trailer-1310.txt).
  focoOp = 0; scrollY = 0.0f; velY = 0.0f; sair = 0;
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
  // MASCARADO, sempre. Esta tela e fotografada e colada em issue — foi assim
  // que chegou o relato do painel de Tracking. O portal sai sem esquema e o MAC
  // so com os dois ultimos octetos: o bastante para a pessoa reconhecer QUAL
  // cadastro esta ali, insuficiente para alguem usar o acesso dela.
  if (op == AJ_STALKER_PORTAL)
    return stalker_configurado() ? stalker_portal_curto() : i18n("Não configurado");
  if (op == AJ_STALKER_MAC)
    return stalker_configurado() ? stalker_mac_mascarado() : i18n("Não configurado");
  if (op == AJ_XTREAM_SERVIDOR)
    return strcmp(xtream_servidor_curto(), "-") ? xtream_servidor_curto() : i18n("Não configurado");
  if (op == AJ_XTREAM_USUARIO)
    return strcmp(xtream_usuario(), "-") ? xtream_usuario() : i18n("Não configurado");
  if (op == AJ_XTREAM_SENHA)
    return strcmp(xtream_senha_mascarada(), "-") ? xtream_senha_mascarada() : i18n("Não configurado");
  if (op == AJ_FANART_CHAVE) return fanartMascarada();
  if (op == AJ_ENVIAR_LOG) {
    switch (avisos_envio_estado()) {
      case 1:  return i18n("enviando…");
      case 2:  return i18n("enviado. Obrigado.");
      case 3:  return i18n("não foi possível enviar");
      default: return i18n("OK envia");
    }
  }
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
  if (op == AJ_ESPACO) {
    // CURTO O BASTANTE PARA CABER NA COLUNA: "201.0 MB em 209 imagens" era
    // cortado em "209..." na TV, e o numero que sobrava era o menos util. O
    // detalhe (orcamento, grafico, o que esta na tela) vai no painel da
    // direita, que tem espaco — ver desenhaPainelImagens.
    int itens = 0; long bytes = 0;
    tex_estatisticas(&itens, NULL, &bytes, NULL, NULL);
    snprintf(buf, sizeof buf, i18n("%.1f MB · %d imagens"), bytes / 1048576.0, itens);
    return buf;
  }
  // ACAO SEM VALOR PROPRIO. Este `return` era o da memoria de imagens, e toda
  // acao que nao tinha ramo acima caia nele: "Ordenar e ativar fileiras"
  // mostrava "100.1 MB em 119..." na coluna do valor (foto do dono, 16/09).
  if (OPCOES[op].tipo == OP_ACAO) return i18n("Abrir");
  return "";
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
    case AJ_CW_OK: case AJ_CW_FONTE:
    case AJ_CW_ESTILO: case AJ_CW_THUMB: case AJ_CW_FURTHEST:
    case AJ_CW_NAO_EXIBIDOS: case AJ_CW_ORDEM:
      return !ajustes_cw_ligado();
    case AJ_CW_BLUR_PROX: return !ajustes_cw_ligado() || !ajustes_cw_thumb_episodio();
    case AJ_EXPANDIR_ATRASO: return !ajustes_expandir_poster();
    // Sem versao nova no GitHub nao ha o que atualizar: a linha continua
    // visivel e APAGADA, em vez de sumir — sumir mudaria a contagem de linhas
    // debaixo do dedo, que e a regra ja escrita para a tela de Layout.
    case AJ_ATUALIZAR:    return !atualizacao_nova()[0];
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
    if (focoOp < SECOES[s].ini + secN(s)) return s;
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
    if (op >= AJ_CW_OK && op <= AJ_CW_ORDEM)
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
    case AJ_LEG_LINGUA: return "Idioma procurado primeiro nas legendas de cada título, e ligado sozinho quando o vídeo começa. \"Da conta\" segue o que está no seu perfil.";
    case AJ_AUD_LINGUA: return "Faixa de áudio escolhida quando o arquivo tem mais de uma. Se o idioma não existir no arquivo, o player usa a primeira.";
    case AJ_PAUSA_OVERLAY: return "Ao pausar, sobe uma ficha com a sinopse e os dados do que você está vendo.";
    case AJ_STALKER_PORTAL: return "Os canais do portal entram no Guia de TV, junto com os dos addons. Endereço sem http:// e sem barra no fim: meu-portal.exemplo.tv:8080";
    case AJ_STALKER_MAC: return "O MAC que o provedor cadastrou para você. É credencial: vale como senha, e só aparece nesta tela mascarado.";
    case AJ_STALKER_LIMPAR: return "Apaga o portal e o MAC deste perfil, e os canais dele somem do Guia. Sair da conta também apaga.";
    case AJ_XTREAM_SERVIDOR: return "Endereço e porta que o provedor mandou, sem http:// e sem barra no fim: meu-servidor.tv:8080. Os canais entram no Guia de TV com usuário e senha preenchidos.";
    case AJ_XTREAM_USUARIO: {
      // #88: a linha da lista corta em ~360 px; o valor completo mora aqui no
      // painel de ajuda, onde ha largura de sobra para ler/conferir.
      static char buf[220];
      const char *u = xtream_usuario();
      if (strcmp(u, "-")) {
        snprintf(buf, sizeof buf, i18n("O usuário da sua assinatura Xtream.\n\nAgora: %s"), u);
        return buf;
      }
      return "O usuário da sua assinatura Xtream.";
    }
    case AJ_XTREAM_SENHA: return "A senha da assinatura. É credencial: vai dentro de cada URL de canal e nunca aparece nesta tela em claro.";
    case AJ_XTREAM_LIMPAR: return "Apaga servidor, usuário e senha deste perfil, e os canais somem do Guia. Sair da conta também apaga.";
    case AJ_FONTE_MANUAL: return "Ao mandar reproduzir, abre a lista de fontes em vez de escolher sozinho. Canal ao vivo não pergunta.";
    case AJ_FONTE_AUTO: return "Melhor fonte: prefere 4K, Dolby Vision e MP4 e confere uma fonte por vez. Primeira da lista: toca a primeira que o addon mandou e não confere nenhuma outra — para quem já filtra e ordena no AIOStreams.";
    case AJ_FONTE_REPOR: return "Quantas outras fontes o automático tenta quando a escolhida não abre. Cada tentativa pode adicionar um arquivo na sua conta de debrid.";

    // --- Home
    case AJ_LANDSCAPE: return "Usa a arte deitada (16:9) no lugar do cartaz em pé nas fileiras que têm as duas.";
    case AJ_HERO_CHEIO: return "O destaque do topo ocupa a tela inteira atrás das fileiras, em vez de ficar num bloco.";
    case AJ_HERO_FUNDO: return "De onde vem a arte de fundo do destaque, da página do título e dos cards deitados: catálogo/Cinemeta, IMDb/Metahub, TMDB, Trakt, Apple TV, fanart.tv (com chave) ou Anime (Kitsu/AniList). Automático usa a do catálogo. MDBList fornece notas, não imagens.";
    case AJ_HERO_ARTE_DIF: return "Desligado: card, destaque e página do título mostram a mesma imagem. Ligado: o card fica com a arte do catálogo e o destaque usa outra foto — TMDB vira outro fundo do TMDB; em Automático, ou se a escolhida repetir o card, usa Apple TV, outro fundo do TMDB, fanart.tv, anime ou Trakt.";
    case AJ_HERO_TRAILER: return "Com o foco parado no destaque do topo, o trailer do título toca sem som no lugar da arte. Mover o foco volta para a arte.";
    case AJ_FIL_LIMITE: return "Quantas fileiras a Home monta. Menos fileiras também significam menos catálogos pedidos pela rede, e não fileiras invisíveis.";
    case AJ_FIL_ORDEM: return "Abre a lista de fileiras para reordenar, ligar, desligar e escolher o card de cada uma. É lá que dá para ver de onde cada fileira vem.";
    case AJ_RAIL: return "A barra de navegação da esquerda fica sempre aberta, ou recolhida até você ir até ela.";
    case AJ_RAIL_MODERNA: return "Troca a barra lateral pela versão nova, com ícones maiores. Ela ignora a escolha entre recolhida e fixa.";
    case AJ_RAIL_BLUR: return "Desfoca a arte atrás da barra lateral moderna em vez de usar um fundo sólido.";
    case AJ_HERO: return "O bloco grande no topo da Home, com a arte e o nome de um título em destaque.";
    case AJ_HERO_CATALOGOS: return "Quantidade de catálogos incluídos no destaque. Esta linha é apenas informativa.";
    case AJ_PS_FUNDO: return "A tela \"Quem está assistindo?\" mostra arte do catálogo atrás dos perfis. Desligado volta à tela lisa de antes.";
    case AJ_DESCOBRIR: return "Onde fica a tela Descobrir: junto da Busca, como item próprio na barra lateral, ou em lugar nenhum.";
    case AJ_ROTULOS: return "Escreve o nome do título abaixo do cartaz. A maior parte da arte já traz o nome impresso.";
    case AJ_NOME_ADDON: return "Acrescenta o nome do addon ao título da fileira, para separar dois catálogos com o mesmo nome.";
    case AJ_SUFIXO_TIPO: return "Acrescenta \"Filme\" ou \"Série\" ao título da fileira, para separar as duas versões do mesmo catálogo.";
    case AJ_OCULTAR_NLANC: return "Esconde das fileiras o que ainda não estreou. Título sem fonte nenhuma ocupa lugar e não abre.";
    case AJ_NOTAS_HOME: return "Mostra a nota do IMDb no canto dos cartazes da Home.";
    case AJ_GRAD_CLASSICO: return "Volta ao degradê antigo sob o cartaz em foco, no lugar do realce atual.";

    // --- Continuar assistindo
    case AJ_CW_LIGADO: return "A fileira de retomada, com o que você deixou pela metade e o próximo episódio das séries que acompanha.";
    case AJ_CW_OK: return "O que o OK faz no card da retomada: toca de onde parou, ou abre a página do título. Segurar OK abre o menu nos dois casos.";
    case AJ_CW_FONTE: return "De onde vem a fileira de retomada. \"Ambas\" usa a conta Nuvio e completa com o Trakt e, se estiver vinculado, com o Simkl.";
    case AJ_CW_ESTILO: return "A forma do card da retomada: quadrado com a arte, deitado largo, ou o cartaz em pé.";
    case AJ_CW_THUMB: return "Usa a imagem do próprio episódio no card, em vez da arte da série.";
    case AJ_CW_BLUR_PROX: case AJ_DET_BLUR_NAO_VISTOS: return "Oculta detalhes da miniatura para evitar spoilers de episódios ainda não assistidos.";
    case AJ_CW_FURTHEST: return "Escolhe o próximo episódio a partir do mais avançado marcado como assistido.";
    case AJ_CW_NAO_EXIBIDOS: return "A retomada mostra o próximo episódio antes de ir ao ar.";
    case AJ_CW_ORDEM: return "Como a retomada se ordena: pelo mais recente, no estilo dos streamings, ou com os episódios futuros num bloco separado.";

    // --- Pagina de detalhe
    case AJ_DET_TRAILER:
#ifdef __EMSCRIPTEN__
      // Samsung: a tela cheia e muda (trailerfonte_com_som) — a ajuda diz, em
      // vez de o botao prometer um som que nao vem.
      return "Mostra o botão de trailer na tela do título, quando existe um trailer conhecido. Nesta TV o trailer toca sem som.";
#else
      return "Mostra o botão de trailer na tela do título, quando existe um trailer conhecido.";
#endif
    case AJ_DET_META_EXT: return "Prefere a ficha do addon de metadados à do Cinemeta. Útil quando o seu addon tem sinopse e elenco melhores.";
    case AJ_DET_DATA_CHEIA: return "Escreve a data de estreia por extenso em vez de só o ano.";
    case AJ_DET_VEU: return "Quanto a vinheta escura cobre a arte na tela do título. Cem por cento é o padrão; zero mostra a arte limpa — o texto pode ficar difícil de ler sobre cenas claras.";
    case AJ_DET_TRAILER_AUTO: return "Alguns segundos depois de abrir um título, o trailer toca sem som no lugar da arte de fundo. Rolar a página ou sair dela volta para a arte.";
    case AJ_TRAILER_QUAL: return "Definição do vídeo do trailer. Máxima usa a maior que existir para o título; as outras são um teto, para conexões mais lentas.";
    case AJ_TRAILER_ASPECTO: return "Quanto o trailer é ampliado para encher a tela. Zoom cinema tira a tarja preta de um trailer de cinema; Original mostra o quadro inteiro, com tarja.";
    case AJ_TRAILER_FONTE:
      // O que cada TV toca (trailerfonte.c, existe): a ajuda nomeia a fonte que
      // falta AQUI, senao escolher IMDb na Samsung e ficar sem trailer parece
      // defeito.
#ifdef __EMSCRIPTEN__
      return "De onde vem o trailer da tela do título e do destaque. Automático tenta a Apple TV, depois o IMDb e, sem os dois, o YouTube (só na tela do título); uma fonte escolhida é a única tentada. Nesta TV o trailer toca sempre sem som.";
#else
      return "De onde vem o trailer da tela do título e do destaque. Automático tenta a Apple TV e, sem ela, o IMDb; uma fonte escolhida é a única tentada. O YouTube não toca nesta TV.";
#endif

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
    case AJ_QUALIDADE_IMG: return "Quanto de pixel a arte carrega. Alta pede a versão grande de cada imagem e gasta mais memória; Baixa pede a menor, carrega antes e cabe em TV com pouca RAM.";

    // --- Interface e conta
    case AJ_IDIOMA: return "Idioma de toda a interface. Não muda o idioma das legendas nem do áudio.";
    case AJ_TEMA: return "Cor do anel que marca onde está o foco. Os doze temas são os do app web e seguem a conta. Dinâmica usa a cor da arte do título em cena; Dinâmica estilizada também tinge o fundo, de leve. As duas ficam só nesta TV.";
    case AJ_ANIM: return "Use Reduzidas para movimentos mais discretos ao navegar pela interface.";
    case AJ_RESOLUCAO: return "Desenha a interface em 4K nas TVs que permitem. Muitas ignoram o pedido e continuam em 1080p — o log diz qual é o caso. Vale reiniciar o app depois de mudar. O vídeo já é 4K nos dois casos.";
    case AJ_PERFIL_ATIVO: return "Perfil em uso nesta TV. Trocar de perfil é feito na tela de perfis, ao abrir o app.";
    case AJ_SYNC: return "Estado da última troca de dados com a sua conta: addons, progresso, coleções e preferências.";
    case AJ_ADDONS: return "Abre a lista de addons da sua conta, para ligar e desligar cada um nesta TV.";
    case AJ_TRAKT: return "Conecta a sua conta do Trakt para marcar o que assistiu e usar a sua lista.";
    case AJ_SIMKL: return "Conecta a sua conta do Simkl, uma alternativa ao Trakt para acompanhar séries.";
    case AJ_SAIR: return "Sai da conta nesta TV e apaga daqui a sessão, os addons e o progresso guardados.";
    case AJ_ESPACO: return "Uso atual de memória pelo cache de imagens, não espaço ocupado no armazenamento da TV.";
    case AJ_TEX_MB: return "Quanta memória o cache de imagens pode usar. Automático escolhe pela RAM da TV. Um valor acima do que esta TV suporta é reduzido ao máximo dela — o painel ao lado mostra o teto em vigor.";
    case AJ_VERSAO_I: return "Versão do aplicativo. Esta informação não pode ser alterada.";
    case AJ_ATUALIZAR: return "Abre o cartão da versão nova, com o que mudou e o botão de instalar. Fica apagado quando não há versão nova.";
    case AJ_ENVIAR_LOG: return "Manda os últimos 200 KB do registro desta sessão (sem senhas nem chaves) para quem faz o app. Use quando algo estiver errado agora.";
    case AJ_ENVIO_AUTO: return "Ligado, o app manda o registro sozinho: o da sessão anterior ao abrir e o desta a cada minuto. Sem senhas nem chaves; serve para achar o que trava a Samsung. Desligue quando quiser.";
    case AJ_DIAGNOSTICO: return "Testa manifestos, fontes e artes dos addons, mede os tempos e aplica um perfil seguro de Qualidade ou Desempenho. O teste não marca títulos como assistidos.";

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
    case AJ_FANART_CHAVE: return "Sua chave pessoal do fanart.tv, gratuita em fanart.tv/get-an-api-key. Com ela a fonte fanart.tv entra no Background do hero. Fica só nesta TV e aparece mascarada.";
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
    case AJ_CW_OK:
      return "Vale só nesta TV: não altera a Home dos seus outros aparelhos.";
    case AJ_CW_FONTE:
      // SEM VINCULO, "Simkl" e uma fileira vazia. Dizer isso aqui, na linha
      // onde a escolha e feita, e o que impede a Home de so perder a fileira
      // sem explicacao. Mesma frase de simkl.h, que as outras telas usam.
      if (valor[op] == AJ_CWF_SIMKL && !simklauth_token()[0])
        return "Vincule o Simkl em Ajustes: sem o vínculo, a fileira fica vazia.";
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
      if (valor[op] == AJ_SALVOS_SIMKL && !simklauth_token()[0])
        return "Vincule o Simkl em Ajustes: sem o vínculo, o + guarda só na lista desta TV.";
      return "A lista desta TV recebe o título em todos os casos. Isto decide se ele também vai para o Trakt ou para o Simkl.";
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
// UMA CATEGORIA POR PAGINA (dono, 20/09/2026: "separar por categorias em vez
// de mostrar tudo de uma vez"). A lista desenha so a categoria da opcao em
// foco, entao o y de uma opcao e medido do topo da PROPRIA categoria — e
// trocar de categoria zera a rolagem (ajustes_atualizar).
static int secaoDe(int op) {
  for (int s = 0; s < AJ_N_SECOES; s++)
    if (op < SECOES[s].ini + secN(s)) return s;
  return AJ_N_SECOES - 1;
}
static float yDaOpcao(int op) {
  int s = secaoDe(op);
  float y = AJ_SEC_CABEC;
  for (int k = 0; k < secN(s); k++) {
    int o = SECOES[s].ini + k;
    y += alturaSub(o);
    if (o == op) return y;
    y += AJ_LINHA_H + AJ_LINHA_GAP;
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
// FIL_MAX + 1: a aba "Na Home" escreve o DESTAQUE antes das fileiras, e com a
// tabela cheia (320) a lista teria 321 entradas. Um a mais aqui custa 4 bytes e
// tira do caminho um estouro que so apareceria na TV de quem tem addon demais.
static int filLista[FIL_MAX + 1];     // indices fil_* da aba corrente, na ordem da tela
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

// O DESTAQUE E UMA LINHA DA LISTA, com indice proprio.
//
// Ele nao e uma fileira (nao tem catalogo, nao entra na fila, nao se move), mas
// a pessoa o procura onde procura as fileiras — foi o pedido: "no reorder tem
// que ter o hero para poder substituir e colocar o que quiser lá". Entrar como
// SENTINELA dentro de filLista, em vez de deslocar as posicoes de todo mundo,
// mantem a navegacao, a rolagem, o salto por letra e o arrastar exatamente
// como estavam: tudo isso ja passa por filIdx, e todo caminho que age sobre uma
// fileira ja tinha o `if (idx < 0) return` que o botao do rodape exigia.
#define AJ_FIL_DESTAQUE (-2)

static void filMontarLista(void) {
  int i, n = fil_n();
  filListaN = 0; filSep = -1;
  if (filAba == 0) {
    filLista[filListaN++] = AJ_FIL_DESTAQUE;
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

// Rotulo da fonte do destaque, para a coluna de valor.
static const char *heroFonteRotulo(void) {
  const char *f = fil_hero_fonte();
  int i, n;
  if (!f[0]) return i18n("Automático");
  if (f[0] == '*' && !f[1]) return i18n("Aleatório do catálogo");
  n = fil_n();
  for (i = 0; i < n; i++)
    if (!strcmp(fil_chave(i), f)) return fil_titulo(i);
  // A fileira saiu (addon removido). A escolha NAO e apagada aqui — ver
  // fileiras.h —, entao o rotulo tem de dizer o que esta acontecendo em vez de
  // mostrar "Automático" e fingir que ninguem escolheu nada.
  return i18n("Fileira indisponível");
}

// Percorre as fontes possiveis: automatico, sorteio, e cada fileira que esta na
// home, na ordem em que ela aparece. As fileiras FORA da home nao entram: o
// destaque mostraria titulos de uma fileira que a pessoa desligou.
static void heroFonteCiclar(int dir) {
  char ops[FIL_MAX + 2][FIL_CHAVE];
  int n = 0, i, atual = 0, total = fil_n();
  snprintf(ops[n++], FIL_CHAVE, "%s", "");
  snprintf(ops[n++], FIL_CHAVE, "%s", "*");
  for (i = 0; i < total && n < (int)(sizeof ops / sizeof *ops); i++)
    if (fil_estado(i) == FIL_NA_HOME)
      snprintf(ops[n++], FIL_CHAVE, "%s", fil_chave(i));
  { const char *f = fil_hero_fonte();
    for (i = 0; i < n; i++) if (!strcmp(ops[i], f)) { atual = i; break; } }
  atual += dir;
  if (atual < 0) atual = n - 1;
  if (atual >= n) atual = 0;
  fil_definir_hero_fonte(ops[atual]);
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
    if (filIdx(filFoco) == AJ_FIL_DESTAQUE) {
      // A linha do destaque nao tem colunas: ela tem um valor, e as setas o
      // trocam — a mesma gramatica das linhas de escolha da lista principal.
      heroFonteCiclar(k == SDLK_RIGHT ? 1 : -1);
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
    if (filIdx(filFoco) == AJ_FIL_DESTAQUE) { heroFonteCiclar(1); return; }
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
  // A MODAL DE DIGITACAO VEM ANTES DE TUDO, como em biblioteca.c e recenviar.c:
  // enquanto ela esta em pe, nenhuma tecla pertence a lista de opcoes atras.
  // Sem esta linha, o D-pad moveria o foco da lista por baixo da modal.
  if (teclado_aberto()) { teclado_evento(e); return; }
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
    if (focoOp == AJ_ATUALIZAR) { atualizacao_abrir(); return; }
    if (focoOp == AJ_ENVIAR_LOG) { avisos_enviar_registro_atual(); return; }
    if (focoOp == AJ_ADDONS) { pediuAddons = 1; return; }
    if (focoOp == AJ_DIAGNOSTICO) { pediuDiagnostico = 1; return; }
    if (focoOp == AJ_STALKER_PORTAL || focoOp == AJ_STALKER_MAC) {
      int mac = focoOp == AJ_STALKER_MAC;
      stCampo = focoOp;
      // O valor atual volta para o campo: trocar a porta de um portal nao pode
      // obrigar a redigitar o endereco inteiro no D-pad. O MAC e a excecao —
      // ele nunca e devolvido em claro, nem para o proprio dono, porque a
      // modal fica na tela e a tela vira foto.
      teclado_abrir_com(mac ? "MAC do portal" : "Portal Stalker (MAC)",
                        mac ? "Formato 00:1a:79:xx:xx:xx"
                            : "Endereço e porta, sem http://",
                        mac ? 17 : 48,
                        mac ? ST_ALFA_MAC : ST_ALFA_PORTAL,
                        (!mac && stalker_configurado()) ? stalker_portal_curto() : NULL);
      return;
    }
    if (focoOp == AJ_STALKER_LIMPAR) { stalker_esquecer(); return; }
    if (focoOp == AJ_XTREAM_SERVIDOR || focoOp == AJ_XTREAM_USUARIO || focoOp == AJ_XTREAM_SENHA) {
      int srv = focoOp == AJ_XTREAM_SERVIDOR, sen = focoOp == AJ_XTREAM_SENHA;
      stCampo = focoOp;
      // Servidor e usuario voltam para o campo (corrigir uma letra nao pode
      // obrigar a redigitar tudo no D-pad); a senha nao — a modal fica na
      // tela e a tela vira foto.
      teclado_abrir_com(srv ? "Servidor Xtream" : sen ? "Senha Xtream" : "Usuário Xtream",
                        srv ? "Endereço e porta, sem http://" : sen ? "Como o provedor mandou" : "Como o provedor mandou",
                        srv ? 64 : 48,
                        srv ? ST_ALFA_PORTAL : XT_ALFA_CONTA,
                        srv ? (strcmp(xtream_servidor_curto(), "-") ? xtream_servidor_curto() : NULL)
                            : (!sen && strcmp(xtream_usuario(), "-")) ? xtream_usuario() : NULL);
      return;
    }
    if (focoOp == AJ_XTREAM_LIMPAR) { xtream_esquecer(); return; }
    if (focoOp == AJ_FANART_CHAVE) {
      // A chave NUNCA volta para o campo (a modal fica na tela e a tela vira
      // foto); confirmar vazio esquece a que estava.
      stCampo = focoOp;
      teclado_abrir_com("Chave do fanart.tv", "Chave pessoal: fanart.tv/get-an-api-key. Vazio apaga.",
                        40, "0123456789abcdef", NULL);
      return;
    }
    if (focoOp == AJ_TRAKT) { traktauth_comecar(); return; }
    if (focoOp == AJ_SIMKL) { simklauth_comecar(); return; }
    if (focoOp == AJ_SAIR) {
      // Sair apaga a sessao do disco. Sem confirmacao de proposito: o custo de
      // sair sem querer e um login por QR, e uma caixa de confirmacao nesta
      // lista exigiria um modal que a tela nao tem.
      sessao_sair();
      traktauth_esquecer();
      simklauth_esquecer();
      simkl_esquecer();
      // A ordem e o liga/desliga das fileiras sao da home de QUEM SAIU, como a
      // ordem que vem da conta (ver catordem_esquecer). Sem isto, a proxima
      // pessoa herda a home montada pela anterior.
      fil_esquecer();
      // As listas FIXADAS tambem sao da conta que saiu: uma lista do Trakt
      // presa na Biblioteca continuaria ali, com o nome de quem foi embora, e
      // a fileira dela na home tentaria buscar itens com o token novo.
      lst_esquecer_conta();
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
      // A ORDENACAO E OS NAO EXIBIDOS (issue #127) tambem sao decididos em
      // montarContinuar; aqui basta refazer so a retomada, sem o ciclo
      // inteiro — o conjunto de itens e o mesmo, muda a ordem e quem fica.
      if (focoOp == AJ_CW_ORDEM || focoOp == AJ_CW_NAO_EXIBIDOS) desc_refazer_continuar();
      // O DESTINO DO "+" tambem: com "Plan to Watch do Simkl" o Plan to Watch
      // entra nos Salvos pela descoberta (descoberta.c), e sem o ciclo ele so
      // apareceria no proximo sync.
      if (focoOp == AJ_SALVOS_DEST) desc_repetir();
      // O teto de imagens vale NA HORA: subir e so deixar entrar mais; descer
      // despeja pelo LRU de sempre no proximo quadro.
      if (focoOp == AJ_TEX_MB) tex_definir_orcamento_mb(ajustes_tex_mb());
    }
    gravar();   // grava a cada mudanca: nao ha botao de "salvar" nesta tela
    // #85 (resto): a TV le o blob de layout da conta mas nao o escreve. Sem
    // isto, o proximo arranque reaplicava o blob antigo e desfazia largura,
    // arredondamento, profundidade e cor de destaque mudados so na TV.
    // NAO vai dentro de gravar(): o blob da conta tambem chama gravar() e
    // nao pode marcar a TV como "fonte da verdade" por ter recebido a conta.
    sync_proteger_ajustes_locais();
  }
}

void ajustes_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (teclado_aberto()) teclado_atualizar(dt, agora);
  // O resultado e CONSUMIDO NA LEITURA (ver teclado.h): ler duas vezes daria
  // TECLADO_NADA na segunda, e por isso a gravacao acontece aqui, uma vez.
  { int r = teclado_resultado();
    if (r == TECLADO_PRONTO && stCampo) {
      if (stCampo == AJ_STALKER_MAC) stalker_definir_mac(teclado_texto());
      else if (stCampo == AJ_XTREAM_SERVIDOR) xtream_definir_servidor(teclado_texto());
      else if (stCampo == AJ_XTREAM_USUARIO)  xtream_definir_usuario(teclado_texto());
      else if (stCampo == AJ_XTREAM_SENHA)    xtream_definir_senha(teclado_texto());
      else if (stCampo == AJ_FANART_CHAVE)    fanartDefinir(teclado_texto());
      else                           stalker_definir_portal(teclado_texto());
      stCampo = 0;
    } else if (r == TECLADO_CANCELOU) {
      stCampo = 0;
    } }
  // Repouso da escolha de idioma de legenda: ver aplicarIdioma.
  if (legendaEspera > 0.0f) {
    legendaEspera -= dt;
    if (legendaEspera <= 0.0f) { legendaEspera = 0.0f; addons_legendas_reiniciar(); }
  }
  for (int i = 0; i < AJ_N; i++) {
    // Com o foco na coluna de categorias a linha DESCANSA: o preenchimento
    // de realce e o do foco, e o foco esta na categoria — duas superficies
    // claras ao mesmo tempo diriam "voce esta em dois lugares".
    float alvo = (i == focoOp && !focoIndice) ? 1.0f : 0.0f;
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
  // PAGINA NOVA: a lista passa a ser outra categoria. A rolagem nao anima de
  // uma lista para a outra — recomeca do topo, e a pagina entra por
  // `paginaA` (um deslize curto na lista, nao no indice).
  { static int secVista = -1;
    int secAgora = secaoDe(focoOp);
    if (secAgora != secVista) {
      if (secVista >= 0) paginaA = 0.0f;
      secVista = secAgora; scrollY = 0.0f; velY = 0.0f; alvo = 0.0f;
    } }
  paginaA = ajustes_animacoes_reduzidas() ? 1.0f : anim_rampa(paginaA, 1.0f, dt, 220.0f);
  if (base - alvo > AJ_BASE - AJ_TOPO) alvo = base - (AJ_BASE - AJ_TOPO);
  if (topo - alvo < 0.0f)              alvo = topo;
  if (alvo < 0.0f) alvo = 0.0f;
  scrollY = anim_mola2_reduzida(&velY, scrollY, alvo, dt, NV_MOLA2_SCROLL,
                                ajustes_animacoes_reduzidas());
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
  // FORA DO INTERVALO NAO LE FORA DO VETOR.
  //
  // Esta linha era `return o->valores[valor[op]]`, sem conferir nada, e foi o
  // segfault que o harness de Ajustes pegou: `valor[]` e um vetor POSICIONAL
  // inicializado a mao, e uma opcao nova inserida no meio do enum desloca todos
  // os padroes seguintes — o 7 do limite de fileiras foi parar numa lista de
  // dois itens, e `valores[7]` e lixo que vira ponteiro de string.
  //
  // O deslocamento e um defeito a parte e esta consertado logo abaixo; ESTA
  // guarda fica de qualquer jeito, porque `valor[]` tambem vem do DISCO: um
  // ajustes.txt de outra versao, ou editado a mao, derruba o app na primeira
  // vez que a linha aparece na tela. Valor invalido tem de ler como "o
  // primeiro", nunca como um endereco qualquer da memoria.
  if (!o->valores || o->n <= 0) return "";
  { int v = valor[op];
    if (v < 0 || v >= o->n) v = 0;
    return o->valores[v] ? o->valores[v] : ""; }
}

// O ICONE DE CADA LINHA (dono, 20/09/2026). Nao ha um desenho por opcao — sao
// 99 — e nem precisa: o icone diz de que FAMILIA a opcao e (reproducao,
// legenda, audio, cartaz, conta...), e a familia e o que o olho procura numa
// lista longa. Um icone por subsecao, com excecoes onde a opcao tem cara
// propria.
//
// TODOS SAO LUCIDE (aj_*, tools/icones-lucide.sh), 25/09/2026: "pode pegar SVG
// externos novos pra gente nao reutilizar os mesmos em tudo e fazer mais
// sentido". Antes eram os PNG do app web, e com poucos desenhos o mesmo icone
// cobria coisas sem relacao — o "aspecto" (proporcao de tela) marcava memoria de
// imagens, escurecimento do fundo e o diagnostico; a engrenagem marcava idioma,
// tema, versao e envio de registro. Um icone que aparece em tudo nao diz nada.
//
// A REGRA AGORA: o mesmo desenho so se repete quando e A MESMA COISA em dois
// lugares — idioma e idioma (interface e metadados), nota e nota (Home e
// MDBList), addon e addon, spoiler escondido e spoiler escondido (Continuar e
// detalhes), trailer e trailer, chave de API e chave de API, datas e datas.
static const char *iconeOpcao(int op) {
  switch (op) {
    // Reproducao: imagem, som, legenda, pausa e as tres de fonte.
    case AJ_QUALIDADE: return "aj_hd";
    case AJ_DV: return "aj_sun";                     // HDR = brilho
    case AJ_ATMOS: return "aj_speaker";
    case AJ_AUD_LINGUA: return "aj_audio-lines";
    case AJ_LEG_LINGUA: return "aj_captions";
    case AJ_PAUSA_OVERLAY: return "aj_circle-pause";
    case AJ_FONTE_MANUAL: return "aj_list-video";    // escolher na lista
    case AJ_FONTE_AUTO: return "aj_wand-sparkles";   // o app escolhe
    case AJ_FONTE_REPOR: return "aj_life-buoy";      // socorro quando falha
    // Home: formato, destaque, fileiras, barra lateral, cartaz.
    case AJ_LANDSCAPE: return "aj_rectangle-horizontal";
    case AJ_HERO_CHEIO: return "aj_maximize-2";
    case AJ_HERO_FUNDO: return "aj_wallpaper";
    case AJ_HERO_ARTE_DIF: return "aj_images";       // duas artes
    case AJ_HERO: case AJ_HERO_CATALOGOS: return "aj_panel-top";
    case AJ_FIL_LIMITE: return "aj_rows-3";
    case AJ_FIL_ORDEM: return "aj_list-ordered";
    case AJ_RAIL: case AJ_RAIL_MODERNA: case AJ_RAIL_BLUR: return "aj_panel-left";
    case AJ_PS_FUNDO: return "aj_users";             // a tela de perfis
    case AJ_DESCOBRIR: return "aj_compass";
    case AJ_ROTULOS: return "aj_tag";
    case AJ_SUFIXO_TIPO: return "aj_tags";
    case AJ_NOME_ADDON: case AJ_ADDONS: return "aj_puzzle";
    case AJ_OCULTAR_NLANC: return "aj_calendar-off";
    case AJ_NOTAS_HOME: return "aj_star";
    case AJ_GRAD_CLASSICO: return "aj_blend";
    // Continuar assistindo: a familia e o relogio que volta (secao); aqui so as
    // que tem cara propria.
    case AJ_CW_ORDEM: return "aj_arrow-down-wide-narrow";
    case AJ_CW_BLUR_PROX: case AJ_DET_BLUR_NAO_VISTOS: return "aj_eye-off";
    case AJ_CW_NAO_EXIBIDOS: return "aj_calendar-clock";
    case AJ_CW_THUMB: return "aj_image-play";         // foto do episodio
    case AJ_TMDB_CW: return "aj_rotate-ccw-clock";   // e o Continuar assistindo
    // Detalhes.
    case AJ_DET_TRAILER: case AJ_DET_TRAILER_AUTO: case AJ_TRAILER_QUAL: case AJ_TRAILER_ASPECTO:
    case AJ_TRAILER_FONTE: case AJ_HERO_TRAILER: case AJ_TMDB_TRAILERS: return "aj_clapperboard";
    case AJ_DET_META_EXT: return "aj_database";      // metadado, como o TMDB
    case AJ_DET_DATA_CHEIA: return "aj_calendar";
    case AJ_DET_VEU: return "aj_sun-dim";            // escurecer o fundo
    // Cartazes. A profundidade inteira (inclusive nos trailers) e camada.
    case AJ_EXPANDIR: return "aj_scaling";
    case AJ_EXPANDIR_ATRASO: return "aj_timer";
    case AJ_NAV_RAPIDA: return "aj_chevrons-right";
    case AJ_BORDA_FOCO: return "aj_scan";
    case AJ_PROF: case AJ_PROF_BORDA: case AJ_PROF_BRILHO: case AJ_PROF_COBERTURA: case AJ_PROF_POSTERS:
    case AJ_PROF_CW: case AJ_PROF_EPS: case AJ_PROF_ELENCO: case AJ_PROF_TRAILERS: return "aj_layers";
    case AJ_LARGURA_DP: return "aj_move-horizontal";
    case AJ_RAIO_DP: return "aj_square-round-corner";
    case AJ_QUALIDADE_IMG: return "aj_image-upscale";
    // Interface e conta.
    case AJ_IDIOMA: case AJ_TMDB_IDIOMA: return "aj_languages";
    case AJ_ANIM: return "aj_sparkles";
    case AJ_RESOLUCAO: return "aj_monitor-cog";
    case AJ_TEMA: return "aj_palette";
    case AJ_PERFIL_ATIVO: return "aj_user-round";
    case AJ_SYNC: return "aj_refresh-cw";
    case AJ_STALKER_PORTAL: case AJ_STALKER_MAC: case AJ_STALKER_LIMPAR:
    case AJ_XTREAM_SERVIDOR: case AJ_XTREAM_USUARIO: case AJ_XTREAM_SENHA: case AJ_XTREAM_LIMPAR: return "aj_tv";
    case AJ_SALVOS_DEST: return "aj_bookmark";
    case AJ_TRAKT: case AJ_SIMKL: return "aj_link";  // servico conectado
    case AJ_SAIR: return "aj_log-out";
    case AJ_VERSAO_I: return "aj_info";
    case AJ_ATUALIZAR: return "aj_download";
    case AJ_ENVIAR_LOG: return "aj_send";
    case AJ_ENVIO_AUTO: return "aj_file-clock";    // o registro, sozinho
    case AJ_ESPACO: case AJ_TEX_MB: return "aj_memory-stick";
    // Integracoes. O TMDB e treze interruptores de "o que pegar do TMDB":
    // com o mesmo cilindro de banco em todos, a lista virava uma coluna de
    // desenhos iguais e o icone nao ajudava a achar nada. Cada um leva o
    // desenho DO QUE ELE TRAZ; o proprio "TMDB" fica com o cilindro (padrao da
    // secao, abaixo). O MDBList e todo nota, e ai a repeticao e o certo.
    case AJ_TMDB_ARTE: return "aj_image";
    case AJ_TMDB_BASICO: return "aj_type";           // titulo e sinopse
    case AJ_TMDB_FICHA: return "aj_clipboard-list";
    case AJ_TMDB_DATAS: return "aj_calendar";
    case AJ_TMDB_ELENCO: return "aj_drama";
    case AJ_TMDB_PROD: return "aj_factory";
    case AJ_TMDB_REDES: return "aj_radio-tower";
    case AJ_TMDB_EPS: return "aj_layout-list";
    case AJ_TMDB_MAIS: return "aj_thumbs-up";
    case AJ_TMDB_COL: return "aj_library-big";
    case AJ_MDB_TRAKT: case AJ_MDB_IMDB: case AJ_MDB_TMDB: case AJ_MDB_LETTER: case AJ_MDB_TOMATES:
    case AJ_MDB_AUDIENCIA: case AJ_MDB_META: case AJ_MDB_MAL: case AJ_MDB_LIGADO: return "aj_star";
    case AJ_MDB_CHAVE: case AJ_FANART_CHAVE: return "aj_key-round";
    case AJ_DIAGNOSTICO: return "aj_gauge";          // mede velocidade
    default: break;
  }
  switch (secaoDe(op)) {
    case 0: return "aj_circle-play";
    case 1: return "aj_house";
    case 2: return "aj_rotate-ccw-clock";
    case 3: return "aj_file-text";
    case 4: return "aj_gallery-vertical-end";
    case 5: return "aj_user-round-cog";
    default: return "aj_database";                   // TMDB
  }
}

static void desenhaLinha(int op, float y, float f, float dx, float aPag) {
  if (y + AJ_LINHA_H < AJ_TOPO - 40.0f || y > AJ_BASE + 40.0f) return;
  // Some antes de cruzar o titulo da tela, como as secoes da pagina de detalhe:
  // texto passando por baixo de texto se le como borrao.
  float a = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f) * aPag;
  if (a <= 0.005f) return;

  int desligada = inativa(op);
  int podeMudar = mutavel(op);
  GfxRect linha = { AJ_LISTA_X + dx, y, AJ_LISTA_W, AJ_LINHA_H };
  // Mesmo vocabulário do menu: superfície escura, texto claro e foco explícito.
  gfx_cor(linha, AJ_RAIO, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, 0.34f * a);
  // FOCO = A LINHA PREENCHIDA COM A COR DE REALCE E O TEXTO ESCURO, sem anel.
  //
  // Era anel de 4 px por fora de uma superficie um pouco mais clara. Pedido
  // do dono (16/09): "os botoes quando selecionados ficar brancos com o texto
  // preto e pode tirar o contorno". A cor de realce passa a ser a cor do
  // botao — e por isso a escolha de tema muda algo visivel nesta tela.
  //
  // `f` e a mola do foco (0..1): o preenchimento acompanha, o texto troca de
  // cor no meio do caminho. Texto ja rasterizado nao muda de cor, e pedir uma
  // rasterizacao por passo da mola encheria o cache de linhas.
  float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
  int emFoco = (f > 0.5f);
  // Brilho difuso por tras da linha em foco (0,9x a altura de folga em cima
  // e embaixo, alpha 0,35 x mola): a mesma luz da pilula do menu lateral
  // (21/09/2026), para o foco acender em vez de so trocar de cor. SEM folga
  // lateral: a lista e recortada a 4 px da linha (ajustes_desenhar), e uma
  // luz que vazasse para os lados seria cortada reta no meio do nada.
  if (f > 0.01f) {
    GfxRect luz = { linha.x, linha.y - linha.h * 0.9f, linha.w, linha.h * 2.8f };
    gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * f * a);
  }
  if (f > 0.01f) gfx_cor(linha, AJ_RAIO, ar, ag, ab, f * a);

  // Uma linha inativa fica visivelmente mais apagada QUE a de leitura: leitura e
  // informacao, inativa e "isto existe mas depende de outra coisa".
  float aTexto = a * (desligada ? 0.65f : 1.0f);
  int cr = emFoco ? AJ_TEXTO_ESCURO : (podeMudar ? 240 : 192);
  // O icone da familia, num disco discreto; sobre o foco claro ele escurece
  // junto com o texto.
  //
  // 28 NO DISCO DE 44, e nao mais 24 (25/09/2026). Os PNG do app web eram
  // quase todos CHEIOS e aguentavam 24; o Lucide e so traco (2 na grade 24), e
  // a 24 px o traco dava 2 px e o "HD", o alto-falante e a legenda viravam
  // borrao a 3 m. A 28 o traco chega a 2,3 px, perto do corpo do rotulo, e o
  // disco continua com 8 px de respiro.
  { int esc = emFoco && focoEscuro();
    float ci = esc ? 0.16f : (emFoco ? 1.0f : 0.70f), cd = esc ? 0.0f : 1.0f;
    GfxRect disco = { linha.x + AJ_PAD - 6.0f, y + (AJ_LINHA_H - 44.0f) * 0.5f, 44.0f, 44.0f };
    gfx_cor(disco, 0.5f, cd, cd, cd, (emFoco ? 0.08f : 0.06f) * a);
    gfx_icone((GfxRect){ disco.x + 8.0f, disco.y + 8.0f, 28.0f, 28.0f }, iconeOpcao(op), ci, ci, ci + 0.02f, aTexto); }
  TxtLinha rot = txt_linha_corta(TXT_CALLOUT, OPCOES[op].rotulo,
                                cr, cr, cr, 255, AJ_LISTA_W - 480.0f);
  txt_desenhar_alpha(rot, linha.x + AJ_PAD + 60.0f,
                     y + (AJ_LINHA_H - rot.h) * 0.5f, aTexto);

  const char *v = textoValor(op);
  int cv = emFoco ? AJ_TEXTO_ESCURO2 : (podeMudar ? 220 : 176);
  TxtLinha val = txt_linha_corta(TXT_CALLOUT, v, cv, cv, cv, 255, 360.0f);
  float xDir = linha.x + AJ_LISTA_W - AJ_PAD;
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
    // Sobre o preenchimento claro do foco a barra tem de ser escura.
    float cb = (emFoco && focoEscuro()) ? 0.08f : 0.94f;
    gfx_cor(trilho, 0.5f, cb, cb, cb + 0.02f, 0.22f * aTexto);
    if (cheio.w > 0.5f)
      gfx_cor(cheio, 0.5f, cb, cb, cb + 0.02f, 0.92f * aTexto);
  }

  // MODO EDICAO: as setas e o realce do valor so existem depois do OK. Sem
  // edicao a linha em foco mostra so o valor — a porta de entrada e escrita
  // no rodape de dicas, nao rabiscada em cada linha.
  if (podeMudar && emEdicao && f > 0.02f) {
    GfxRect pill = { valorDir - val.w - 44.0f, y + (AJ_LINHA_H - 34.0f) * 0.5f,
                     val.w + 80.0f, 34.0f };
    gfx_cor(pill, 0.5f, 0.0f, 0.0f, 0.0f, 0.14f * a);
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
    int emFoco = (atual && focoIndice);
    int c = emFoco ? AJ_TEXTO_ESCURO : (atual ? 240 : 168);
    float ci = emFoco ? 0.10f : (atual ? 0.94f : 0.62f);
    GfxRect ic = { AJ_IDX_X + 18.0f, y + (AJ_IDX_H - AJ_IDX_ICONE) * 0.5f,
                   AJ_IDX_ICONE, AJ_IDX_ICONE };
    float tx = ic.x + AJ_IDX_ICONE + 14.0f;
    TxtLinha t;
    // A categoria ATUAL (foco na lista) e a superficie mais clara; a categoria
    // EM FOCO e o preenchimento de realce com texto escuro, como as linhas —
    // mesma regra da lista, sem anel (ver desenhaLinha).
    if (emFoco) {
      float ar, ag, ab; ajustes_acento(&ar, &ag, &ab);
      // Brilho difuso atras da categoria em foco, como na lista. O indice
      // nao tem mola de foco, entao a luz acende com a pilula.
      GfxRect luz = { r.x - r.h * 0.9f, r.y - r.h * 0.9f, r.w + r.h * 1.8f, r.h * 2.8f };
      gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f);
      gfx_cor(r, raio, ar, ag, ab, 1.0f);
    } else {
      gfx_cor(r, raio, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
              atual ? 0.60f : 0.26f);
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

// PREVIA DAS FORMAS DE CARD, desenhada com as MEDIDAS DE VERDADE.
//
// A coluna "Card" oferece sete nomes e a de "Tamanho" tres nomes, e nada dizia
// o que cada um faz com a fileira — "Coleção" e "Serviço" sao os dois arte
// deitada, e a diferenca entre eles e so tamanho. Pedido do dono: "coloque mais
// informacoes, imagens e svg sobre cada tipo de card na fileira que nao tem
// info sobre, e do tamanho tambem".
//
// Os numeros abaixo sao os MESMOS de home.c (larguraDe/alturaDe), reduzidos por
// um fator unico — e por isso a previa mostra a proporcao E a diferenca de
// tamanho entre as formas, que e justamente o que o nome nao diz. Se home.c
// mudar uma medida, esta previa passa a mentir; a tabela cita a fonte para que
// quem mexer la saiba que ha um segundo lugar.
static const struct { float w, h; } AJ_FIL_FORMA[FIL_TIPO_N] = {
  { 212.0f, 322.0f },   // AUTO     — desenhado como fantasma, ver abaixo
  { 212.0f, 322.0f },   // CARTAZ   — NV_CARD_W x NV_CARD_H
  { NV_DESTAQUE_EDITORIAL_W, NV_DESTAQUE_EDITORIAL_H }, // DESTAQUE — 16:9
  { 480.0f, 270.0f },   // COLECAO
  { 360.0f, 203.0f },   // SERVICO
  { 212.0f, 320.0f },   // TOP10
  { NV_DESTAQUE_QUADRADO_W, NV_DESTAQUE_QUADRADO_H }, // DESTAQUE 4:3
};

// Uma frase por forma. Diz o que a forma E e para que serve, nao como se chama.
static const char *aj_fil_forma_ajuda(int t) {
  switch (t) {
    case FIL_TIPO_CARTAZ:   return i18n("Cartaz em pé 2:3, o mesmo das fileiras de catálogo.");
    case FIL_TIPO_DESTAQUE: return i18n("Arte deitada panorâmica 16:9, como a faixa Destaques.");
    case FIL_TIPO_COLECAO:  return i18n("Arte deitada média: cabe mais que a grande e ainda mostra o cenário.");
    case FIL_TIPO_SERVICO:  return i18n("Arte deitada compacta: a que cabe mais títulos por fileira.");
    case FIL_TIPO_TOP10:    return i18n("Cartaz com o número do ranking ao lado, como no Top 10.");
    case FIL_TIPO_DESTAQUE_QUADRADO:
      return i18n("Arte maior em 4:3: recorta a capa para preencher todo o card.");
    default:                return i18n("O app escolhe pela fileira: retomada e coleções já têm forma própria.");
  }
}

// O desenho de UMA forma, na escala dada. `foco` acende; `fantasma` e o
// AUTOMATICO, que nao tem forma propria — duas silhuetas sobrepostas dizem
// "depende" melhor que um retangulo qualquer com um rotulo.
static void desenhaForma(float x, float yBase, int tipo, float esc, int aceso,
                         float ar, float ag, float ab) {
  float w = AJ_FIL_FORMA[tipo].w * esc, h = AJ_FIL_FORMA[tipo].h * esc;
  float raio = 10.0f * esc;
  GfxRect r = { x, yBase - h, w, h };
  if (tipo == FIL_TIPO_AUTO) {
    GfxRect deitado = { x, yBase - AJ_FIL_FORMA[FIL_TIPO_SERVICO].h * esc,
                        AJ_FIL_FORMA[FIL_TIPO_SERVICO].w * esc,
                        AJ_FIL_FORMA[FIL_TIPO_SERVICO].h * esc };
    gfx_cor(deitado, raio / deitado.h, 0.62f, 0.65f, 0.72f, aceso ? 0.45f : 0.22f);
    r.w = AJ_FIL_FORMA[FIL_TIPO_CARTAZ].w * esc * 0.7f;
    gfx_cor(r, raio / r.h, aceso ? ar : 0.72f, aceso ? ag : 0.74f,
            aceso ? ab : 0.80f, aceso ? 0.9f : 0.5f);
    return;
  }
  gfx_cor(r, raio / h, aceso ? ar : 0.55f, aceso ? ag : 0.57f, aceso ? ab : 0.63f,
          aceso ? 1.0f : 0.55f);
  if (tipo == FIL_TIPO_TOP10) {
    // O numeral E a forma: o Top 10 desenha o cartaz deslocado com o numero
    // atras. Sem ele a previa do Top 10 e igual a do cartaz.
    TxtLinha num = txt_linha(TXT_TITULO1, "1", aceso ? 250 : 170,
                             aceso ? 250 : 172, aceso ? 252 : 180, 255);
    txt_desenhar_alpha(num, x - num.w * 0.42f, yBase - h * 0.58f,
                       aceso ? 0.95f : 0.5f);
  }
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
  // CARTAO FLUTUANTE na "cara nova" (menu.c, 21/09/2026): cantos de 28 px
  // pelo menor lado (a altura), fundo translucido — o veu de 0,92 atras ja
  // apaga a lista — e UMA luz difusa na cor de realce pelo canto superior
  // esquerdo, presa aos cantos do cartao (GFX_LUZ).
  gfx_cor(cartao, 28.0f / AJ_FIL_H, 0.055f, 0.058f, 0.068f, 0.94f);
  gfx_luz_canto(cartao, 28.0f / AJ_FIL_H, AJ_FIL_H * 0.1f, -AJ_FIL_H * 0.1f, AJ_FIL_H * 0.65f, ar, ag, ab, 0.22f);

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
        { int emFoco = (filNaBarra && ativa);
          int ct = emFoco ? AJ_TEXTO_ESCURO : (ativa ? 255 : 170);
          l = txt_linha(TXT_CALLOUT, buf, ct, emFoco || ativa ? ct : 173,
                        emFoco || ativa ? ct : 182, 255);
          GfxRect pil = { bx, hy, l.w + 44.0f, bh };
          // Foco = pilula na cor de realce com texto escuro; ativa sem foco =
          // superficie clara; a outra, apagada. Sem anel (ver desenhaLinha).
          if (emFoco) {
            GfxRect luz = { pil.x - bh * 0.9f, pil.y - bh * 0.9f, pil.w + bh * 1.8f, bh * 2.8f };
            gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f);
            gfx_cor(pil, NV_RAIO_PILL, ar, ag, ab, 1.0f);
          }
          else gfx_cor(pil, NV_RAIO_PILL, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B,
                       ativa ? 0.95f : 0.30f);
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
      // fazer. Na aba "Fora" ha uma acao so, e o anel toma a linha inteira — e
      // no DESTAQUE tambem, que nao tem colunas e sim um valor.
      GfxRect cel = (filAba == 0 && idx != AJ_FIL_DESTAQUE)
        ? (GfxRect){ cx + AJ_FIL_COL[filCampo].x - 12.0f, y, AJ_FIL_COL[filCampo].w + 24.0f, AJ_FIL_LINHA }
        : linha;
      gfx_rect(cel, 0, GFX_ANEL, 0, NV_ANEL_FOCO / AJ_FIL_LINHA, 0, raio, ar, ag, ab, 1.0f);
    }

    // A LINHA DO DESTAQUE. Nome a esquerda, fonte a direita, e a dica das setas
    // so quando ela esta em foco — a gramatica das linhas de escolha da lista
    // principal, que e onde a pessoa aprendeu que ← → trocam um valor.
    if (idx == AJ_FIL_DESTAQUE) {
      { GfxRect ic = { cx + AJ_FIL_COL[0].x, y + (AJ_FIL_LINHA - 26.0f) * 0.5f, 26.0f, 26.0f };
        gfx_icone(ic, "aj_panel-top", 0.78f, 0.80f, 0.85f, 0.9f); }   // o de "Mostrar destaque"
      l = txt_linha_corta(TXT_CALLOUT, i18n("Destaque do topo"), 234, 234, 234, 255,
                          AJ_FIL_COL[0].w - 38.0f);
      txt_desenhar(l, cx + AJ_FIL_COL[0].x + 38.0f, y + 8.0f);
      { TxtLinha sub = txt_linha_corta(TXT_MINI,
            i18n("O que aparece no destaque da Home"), 148, 151, 160, 255,
            AJ_FIL_COL[0].w - 38.0f);
        txt_desenhar_alpha(sub, cx + AJ_FIL_COL[0].x + 38.0f, y + 8.0f + l.h + 4.0f, 0.85f); }
      { float vw = AJ_FIL_COL[2].w + AJ_FIL_COL[3].w;
        l = txt_linha_corta(TXT_CALLOUT, heroFonteRotulo(), 220, 220, 220, 255, vw);
        txt_desenhar(l, cx + AJ_FIL_COL[2].x, y + (AJ_FIL_LINHA - l.h) * 0.5f); }
      if (foco) {
        l = txt_linha(TXT_MINI, i18n("← →  trocar"), 150, 214, 158, 255);
        txt_desenhar(l, cx + AJ_FIL_COL[1].x, y + (AJ_FIL_LINHA - l.h) * 0.5f);
      }
      y += AJ_FIL_LINHA + AJ_FIL_LGAP;
      continue;
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
    // O DESTAQUE NAO ENTRA NA CONTAGEM. Ele esta na lista, mas nao e fileira:
    // dizer "1 de 8" com a aba mostrando "7 de 7" seriam dois numeros do mesmo
    // conjunto que nao batem, e quem le acredita no que estiver mais perto.
    int fileirasN = n - (filAba == 0 ? 1 : 0);
    if (filAba == 0 && filFoco == 0) snprintf(buf, sizeof buf, "%s", i18n("Destaque"));
    else if (filFoco >= n) snprintf(buf, sizeof buf, "%s", i18n("Botão"));
    else snprintf(buf, sizeof buf, i18n("%d de %d"),
                  filFoco + (filAba == 0 ? 0 : 1), fileirasN);
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
      // Botao em foco: preenchido com a cor de realce, texto escuro, sem anel.
      if (foco) gfx_cor(btn, 26.0f / cada, ar, ag, ab, 1.0f);
      else gfx_cor(btn, 26.0f / cada, NV_COR_FOCO_R, NV_COR_FOCO_G, NV_COR_FOCO_B, 0.30f);
      { int ct = foco ? AJ_TEXTO_ESCURO : 220;
        l = txt_linha(TXT_CALLOUT, rot, ct, ct, ct, 255); }
      txt_desenhar(l, btn.x + (btn.w - l.w) * 0.5f, btn.y + (btn.h - l.h) * 0.5f);
    } }

  // A FICHA DA FILEIRA EM FOCO: de qual addon veio, filme ou serie, e quantos
  // titulos ela tem AGORA (omitido antes de a Home montar — "0 titulos" seria
  // mentira sobre uma fileira talvez cheia).
  if (n > 0 && filFoco >= 0 && filFoco < n && !filNaBarra &&
      filLista[filFoco] == AJ_FIL_DESTAQUE) {
    l = txt_linha_corta(TXT_CALLOUT, heroFonteRotulo(), 232, 234, 241, 255,
                        AJ_FIL_W - 80.0f);
    txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 232.0f);
    l = txt_linha_corta(TXT_MINI,
        i18n("Automático usa os primeiros títulos do catálogo; o sorteio troca a cada abertura; uma fileira mostra os títulos dela."),
        170, 173, 182, 255, AJ_FIL_W - 80.0f);
    txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 200.0f);
  } else if (n > 0 && filFoco >= 0 && filFoco < n && !filNaBarra) {
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
    // A SEGUNDA LINHA E DA COLUNA EM FOCO quando ela tem o que explicar. A
    // origem da fileira ja foi dita na linha de cima (o rotulo e o addon); com
    // o foco em "Card" ou "Tamanho" a pergunta de quem esta ali e outra.
    { const char *frase = fil_origem_ajuda(orig);
      if (filAba == 0 && !filPegou) {
        if (filCampo == 2)
          frase = fil_aceita_tipo(idx)
                ? aj_fil_forma_ajuda(fil_linha_tipo(idx))
                : "Esta fileira tem forma própria: ver a frase abaixo do nome.";
        else if (filCampo == 3)
          frase = "O fator vale sobre a medida do tipo, então a proporção do card não muda.";
      }
      l = txt_linha_corta(TXT_MINI, frase, 170, 173, 182, 255, AJ_FIL_W - 760.0f); }
    txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 200.0f);
  }

  // A PREVIA DA COLUNA EM FOCO, no espaco livre a direita das instrucoes.
  //
  // So aparece nas colunas "Card" e "Tamanho", que sao as que oferecem uma
  // escolha sem dizer o que ela faz. Nas outras o espaco fica vazio de
  // proposito: desenhar sempre alguma coisa ali ensinaria a ignorar o canto.
  if (filAba == 0 && !filPegou && !filNaBarra &&
      filFoco >= 0 && filFoco < n && filLista[filFoco] >= 0 &&
      (filCampo == 2 || filCampo == 3)) {
    int idx = filLista[filFoco];
    int aceita = fil_aceita_tipo(idx);
    int tipo = aceita ? fil_linha_tipo(idx) : FIL_TIPO_AUTO;
    int tam = fil_linha_tam(idx);
    // A tira comeca depois da coluna de instrucoes (que ocupa ~700px) e assenta
    // as formas sobre uma linha de base comum: e a base que deixa comparar
    // altura entre elas, que e metade da informacao.
    // CANTO INFERIOR DIREITO, que e o unico retangulo livre do cartao: a ficha
    // ocupa a esquerda logo acima, e das quatro linhas de instrucao a mais
    // comprida termina a 788px da borda esquerda do cartao. MEDIDO na captura,
    // nao estimado — foi assim que as duas primeiras tentativas sairam por
    // cima do texto.
    float px = cx + 850.0f, base = cartao.y + AJ_FIL_H - 24.0f;
    float esc = 0.19f;   // 322 (o card mais alto) x 0,19 = 61px
    int t;
    // SO A ESCOLHIDA E NOMEADA. Seis rotulos lado a lado nao cabem sem
    // reticencia, e reticencia em rotulo de 9 caracteres nao ensina nada — as
    // formas se explicam pelo desenho, e o nome da escolhida ja esta na coluna.
    if (filCampo == 2) {
      float x = px;
      for (t = 0; t < FIL_TIPO_N; t++) {
        float w = AJ_FIL_FORMA[t].w * esc;
        desenhaForma(x, base, t, esc, t == tipo, ar, ag, ab);
        if (t == tipo) {
          TxtLinha rot = txt_linha(TXT_MINI, fil_tipo_rotulo(t), 236, 238, 243, 255);
          txt_desenhar(rot, x + (w - rot.w) * 0.5f,
                       base - AJ_FIL_FORMA[t].h * esc - rot.h - 6.0f);
        }
        x += w + 22.0f;
      }
    } else {
      // TAMANHO: a MESMA forma tres vezes, nos tres fatores. O que muda e o
      // tamanho, entao mostrar tres formas diferentes seria mudar duas coisas.
      float x = px;
      int formaBase = (aceita && tipo != FIL_TIPO_AUTO) ? tipo : FIL_TIPO_CARTAZ;
      for (t = 0; t < FIL_TAM_N; t++) {
        float e = esc * fil_tam_escala(t);
        float w = AJ_FIL_FORMA[formaBase].w * e;
        desenhaForma(x, base, formaBase, e, t == tam, ar, ag, ab);
        if (t == tam) {
          char rot[48];
          TxtLinha lr;
          snprintf(rot, sizeof rot, "%s  %.0f%%", i18n(fil_tam_rotulo(t)),
                   (double)(fil_tam_escala(t) * 100.0f));
          lr = txt_linha(TXT_MINI, rot, 236, 238, 243, 255);
          txt_desenhar(lr, x + (w - lr.w) * 0.5f,
                       base - AJ_FIL_FORMA[formaBase].h * e - lr.h - 6.0f);
        }
        x += w + 46.0f;
      }
    }
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
  } else if (filFoco >= 0 && filFoco < n && filLista[filFoco] == AJ_FIL_DESTAQUE) {
    txt_bloco(TXT_CAPTION,
              "← →  Trocar o que aparece no destaque\n"
              "OK  Avançar para a próxima fonte\n"
              "↑ no topo  Abas\nVoltar  Fechar",
              206, 209, 218, cx + 40.0f, y, AJ_FIL_W - 80.0f, 36, 1, 4);
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
      // LARGURA ATE A TIRA DE FORMAS, e nao a do cartao. A nota fica na mesma
      // altura da ultima linha de instrucao ("Voltar Fechar", que e curta) e,
      // com a largura cheia, as duas se sobrepunham — visivel na captura do
      // album. 800 px param antes da tira e depois do texto da instrucao.
      l = txt_linha_corta(TXT_MINI, motivoFormaFixa(fil_chave(filLista[filFoco])),
                          176, 179, 188, 255, 800.0f);
      txt_desenhar(l, cx + 40.0f, cartao.y + AJ_FIL_H - 24.0f);
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

// PAINEL DA LINHA "MEMORIA USADA POR IMAGENS": o numero que a linha corta e
// aqui inteiro, mais o que ele nao diz sozinho — quanto e o teto, quanto do
// cache e o que esta NA TELA agora, se ele anda despejando, e como o teto foi
// escolhido. Pedido do dono (16/09): "mostrar o valor real e nao cortado, e
// colocar um grafico e estatistica de uso ao lado". Devolve a altura usada.
//
// Os numeros vem do proprio cache (tex_estatisticas, tex_orcamento_info,
// tex_historico), nao de contas feitas aqui: se o cache mudar de regra, o
// painel muda junto.
static float linhaStat(float x, float y, float w, const char *rot, const char *val) {
  TxtLinha r = txt_linha(TXT_CAPTION, rot, 156, 159, 168, 255);
  TxtLinha v = txt_linha_corta(TXT_CAPTION, val, 226, 228, 236, 255, w - r.w - 16.0f);
  txt_desenhar(r, x, y);
  txt_desenhar(v, x + w - v.w, y);
  return 34.0f;
}

// COR DE PRESSAO do cache: verde com folga, amarelo perto do teto, vermelho
// encostado. Pedido do dono (16/09): o grafico e a barra passam a dizer com
// cor o que o numero diz com digitos. As faixas vem do comportamento medido
// em tex_cache.c: acima de ~90% o cache despeja a cada arte nova (encostado),
// entre 70 e 90 ele ainda absorve uma tela de fileiras sem despejar.
static void corPressao(float t, float *r, float *g, float *b) {
  if (t < 0.70f)      { *r = 0.24f; *g = 0.86f; *b = 0.52f; }   // #3ddc84
  else if (t < 0.90f) { *r = 0.96f; *g = 0.78f; *b = 0.30f; }   // ambar
  else                { *r = 0.93f; *g = 0.30f; *b = 0.30f; }   // vermelho
}

static float desenhaPainelImagens(float x, float y, float w) {
  float y0 = y;
  int itens = 0, pend = 0, quentes = 0, mb = 0, fixo = 0, slots = 0;
  long bytes = 0, bytesQ = 0, memTotal = 0, teto;
  long hist[120]; int nh, i;
  char a[96], b[96];
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQ);
  tex_orcamento_info(&mb, &memTotal, &fixo, &slots);
  teto = tex_orcamento_bytes();
  if (teto <= 0) teto = 1;

  // 1. A BARRA: usado sobre o teto, na cor de realce. O teto MOSTRADO e o
  // efetivo (tex_orcamento_bytes), nao o `mb` decidido: no Mac retina o
  // orcamento e mb x 4 e a linha dizia "139 de 96 MB". Na TV os dois sao
  // iguais.
  snprintf(a, sizeof a, i18n("%.1f de %d MB · %d%%"), bytes / 1048576.0,
           (int)(teto / 1048576), (int)(bytes * 100 / teto));
  y += linhaStat(x, y, w, i18n("Ocupado"), a);
  { GfxRect trilho = { x, y, w, 8.0f };
    float t = (float)bytes / (float)teto; if (t > 1.0f) t = 1.0f;
    float pr, pg, pb;
    GfxRect cheio = { x, y, w * t, 8.0f };
    GfxRect naTela = { x, y, w * ((float)bytesQ / (float)teto), 8.0f };
    corPressao(t, &pr, &pg, &pb);
    gfx_cor(trilho, 0.5f, 0.94f, 0.94f, 0.96f, 0.16f);
    if (cheio.w > 0.5f) gfx_cor(cheio, 0.5f, pr, pg, pb, 0.55f);
    // O trecho que esta NA TELA agora, mais forte: e a parte que nao pode
    // ser despejada sem piscar (ver `quente` em tex_cache.c).
    if (naTela.w > 0.5f && naTela.w <= cheio.w) gfx_cor(naTela, 0.5f, pr, pg, pb, 1.0f);
    y += 8.0f + 22.0f; }

  // 2. O GRAFICO: ocupacao nos ultimos dois minutos, uma coluna por segundo,
  // escala do teto. Uma linha reta e um cache que assentou; serrilhado e
  // despejo em ciclo — a forma do defeito que este cache tinha.
  nh = tex_historico(hist, 120);
  { float gh = 84.0f, gx = x, gw = w;
    GfxRect fundo = { gx, y, gw, gh };
    gfx_cor(fundo, 0.0f, 1.0f, 1.0f, 1.0f, 0.06f);
    if (nh > 1) {
      float passo = gw / 120.0f;
      for (i = 0; i < nh; i++) {
        float t = (float)hist[i] / (float)teto;
        float h = gh * t, pr, pg, pb;
        if (h > gh) h = gh;
        if (h < 1.0f) continue;
        // Cada coluna com a cor da pressao DAQUELE segundo: um grafico que
        // fica verde, sobe para amarelo e vira vermelho e a historia do
        // cache enchendo — e uma faixa vermelha continua e ele encostado.
        corPressao(t, &pr, &pg, &pb);
        GfxRect col = { gx + gw - (float)(nh - i) * passo, y + gh - h, passo + 0.5f, h };
        gfx_cor(col, 0.0f, pr, pg, pb, 0.80f);
      }
      // Linhas de referencia dos 70% e 90%, para o olho saber onde a cor vira.
      { GfxRect l70 = { gx, y + gh * 0.30f, gw, 1.0f };
        GfxRect l90 = { gx, y + gh * 0.10f, gw, 1.0f };
        gfx_cor(l70, 0.0f, 1.0f, 1.0f, 1.0f, 0.10f);
        gfx_cor(l90, 0.0f, 1.0f, 1.0f, 1.0f, 0.10f); }
    }
    { TxtLinha l = txt_linha(TXT_MINI, i18n("últimos 2 min · escala do teto"), 130, 133, 142, 255);
      txt_desenhar(l, gx, y + gh + 6.0f); }
    y += gh + 34.0f; }

  // 3. AS ESTATISTICAS.
  snprintf(a, sizeof a, i18n("%d imagens · %.1f MB"), quentes, bytesQ / 1048576.0);
  y += linhaStat(x, y, w, i18n("Na tela agora"), a);
  snprintf(a, sizeof a, i18n("%d imagens · %d em carregamento"), itens, pend);
  y += linhaStat(x, y, w, i18n("No cache"), a);
  snprintf(a, sizeof a, i18n("%d de %d"), itens + pend, slots);
  y += linhaStat(x, y, w, i18n("Vagas"), a);
  snprintf(a, sizeof a, i18n("%ld · %ld da tela"), tex_despejos_total, tex_despejos_quentes_total);
  y += linhaStat(x, y, w, i18n("Despejadas na sessão"), a);
  snprintf(a, sizeof a, i18n("%.1f MB"), tex_cache_disco_bytes() / 1048576.0);
  y += linhaStat(x, y, w, i18n("Baixado na sessão"), a);
  if (fixo == 1)      snprintf(b, sizeof b, "%s", i18n("cravado nesta build"));
  else if (fixo == 2) snprintf(b, sizeof b, "%s", "NUVIO_TEX_MB");
  else if (fixo == 3) snprintf(b, sizeof b, "%s", i18n("escolhido em Ajustes"));
  else if (memTotal > 0) snprintf(b, sizeof b, i18n("pela RAM da TV (%.1f GB)"), memTotal / 1024.0);
  else snprintf(b, sizeof b, "%s", i18n("padrão"));
  // O teto mostrado e o efetivo (no Mac retina e mb x 4; na TV, o mesmo).
  snprintf(a, sizeof a, "%d MB · %s", (int)(teto / 1048576), b);
  y += linhaStat(x, y, w, i18n("Teto"), a);
  return y - y0;
}


// PREVIA DESENHADA NO PAINEL DE AJUDA (dono, 20/09/2026: "icones e graficos no
// que faltam"). Nao e imagem: sao as mesmas primitivas da tela, com o VALOR
// ATUAL da opcao — mexer na opcao mexe no desenho na hora. So para o que tem
// forma: tamanho e canto do cartaz, estilo do Continuar assistindo, limite de
// fileiras, layout da home, qualidade maxima. Devolve a altura ocupada.
static void previaCartaz(float x, float y, float w, float h, float raioPx, float ar, float ag, float ab, float a) {
  float r = raioPx / (w < h ? w : h);
  if (r > 0.5f) r = 0.5f;
  gfx_cor((GfxRect){ x, y, w, h }, r, 0.30f, 0.32f, 0.38f, a);
  // Um "poster" abstrato: faixa clara em cima, titulo em baixo.
  gfx_cor((GfxRect){ x + w * 0.18f, y + h * 0.14f, w * 0.64f, h * 0.10f }, 0.5f, 0.86f, 0.87f, 0.90f, 0.35f * a);
  gfx_cor((GfxRect){ x + w * 0.12f, y + h * 0.78f, w * 0.50f, h * 0.06f }, 0.5f, ar, ag, ab, 0.9f * a);
}
static float desenhaPrevia(int op, float x, float y, float w) {
  float ar, ag, ab, y0 = y;
  ajustes_acento(&ar, &ag, &ab);
  switch (op) {
    case AJ_LARGURA_DP: case AJ_RAIO_DP: {
      // Tres cartazes no tamanho ESCOLHIDO (dp x 2 = px da tela), lado a lado
      // como na fileira; o do meio com foco. A escala e 1:1 com a home ate
      // caber na largura do painel.
      float cw = (float)valor[AJ_LARGURA_DP] * 2.0f, ch = cw * 1.5f, gap = 18.0f;
      float esc = (3.0f * cw + 2.0f * gap > w) ? w / (3.0f * cw + 2.0f * gap) : 1.0f;
      float raio = ajustes_raio_poster_px() * esc;
      int i;
      cw *= esc; ch *= esc; gap *= esc;
      if (ch > 300.0f) { esc = 300.0f / ch; cw *= esc; ch *= esc; gap *= esc; raio *= esc; }
      for (i = 0; i < 3; i++) {
        float px = x + (float)i * (cw + gap);
        if (i == 1) gfx_cor((GfxRect){ px - 4.0f, y - 4.0f, cw + 8.0f, ch + 8.0f }, (raio + 4.0f) / (cw + 8.0f), ar, ag, ab, 0.95f);
        previaCartaz(px, y, cw, ch, raio, ar, ag, ab, 1.0f);
      }
      { char t[80];
        snprintf(t, sizeof t, i18n("%d px de largura · canto de %d px, como na home"), (int)(valor[AJ_LARGURA_DP] * 2), (int)ajustes_raio_poster_px());
        TxtLinha l = txt_linha_corta(TXT_MINI, t, 130, 133, 142, 255, w);
        txt_desenhar(l, x, y + ch + 10.0f);
        return ch + 10.0f + l.h + 8.0f; }
    }
    case AJ_FIL_LIMITE: {
      // Uma home em miniatura: heroi em cima e N fileiras, N = o limite.
      int n = valor[AJ_FIL_LIMITE], i;
      float mh = 300.0f, hero = 70.0f, fil = 22.0f, gap = 8.0f;
      gfx_cor((GfxRect){ x, y, w, mh }, 12.0f / mh, 0.09f, 0.095f, 0.11f, 1.0f);
      gfx_cor((GfxRect){ x + 12.0f, y + 12.0f, w - 24.0f, hero }, 8.0f / hero, 0.22f, 0.24f, 0.30f, 1.0f);
      for (i = 0; i < n; i++) {
        float fy = y + 12.0f + hero + 12.0f + (float)i * (fil + gap);
        int k;
        if (fy + fil > y + mh - 8.0f) break;
        for (k = 0; k < 7; k++)
          gfx_cor((GfxRect){ x + 12.0f + (float)k * ((w - 24.0f) / 7.0f), fy, (w - 24.0f) / 7.0f - 6.0f, fil },
                  4.0f / fil, 0.28f, 0.30f, 0.36f, 1.0f);
      }
      { char t[80];
        snprintf(t, sizeof t, i18n("%d fileiras abaixo do herói"), n);
        TxtLinha l = txt_linha_corta(TXT_MINI, t, 130, 133, 142, 255, w);
        txt_desenhar(l, x, y + mh + 10.0f);
        return mh + 10.0f + l.h + 8.0f; }
    }
    case AJ_CW_ESTILO: {
      // Os tres estilos, o escolhido em destaque: card (16:9 com texto
      // embaixo), largo (16:9 com texto dentro), poster (2:3).
      int est = valor[AJ_CW_ESTILO], i;
      float cw = (w - 2.0f * 18.0f) / 3.0f;
      for (i = 0; i < 3; i++) {
        float px = x + (float)i * (cw + 18.0f), ch = i == 2 ? cw * 1.5f : cw * 0.5625f;
        float al = (i == est) ? 1.0f : 0.35f;
        if (i == est) gfx_cor((GfxRect){ px - 4.0f, y - 4.0f, cw + 8.0f, ch + 8.0f }, 12.0f / (cw + 8.0f), ar, ag, ab, 0.95f);
        gfx_cor((GfxRect){ px, y, cw, ch }, 8.0f / (cw < ch ? cw : ch), 0.30f, 0.32f, 0.38f, al);
        gfx_cor((GfxRect){ px + 10.0f, y + ch - 12.0f, cw * 0.5f, 4.0f }, 0.5f, ar, ag, ab, 0.9f * al);
        if (i != 2 && i == 0) {
          TxtLinha l = txt_linha(TXT_MINI, i18n("Título · T1E3"), 200, 203, 210, 255);
          txt_desenhar_alpha(l, px, y + ch + 8.0f, al);
        }
      }
      return cw * 1.5f + 8.0f + 30.0f;
    }
    case AJ_LANDSCAPE: case AJ_HERO_CHEIO: case AJ_HERO: {
      // A home em miniatura com o heroi em tela cheia ou nao, e cartazes
      // deitados ou em pe.
      float mh = 300.0f;
      int cheio = valor[AJ_HERO_CHEIO] == 0, deitado = valor[AJ_LANDSCAPE] == 0, semHero = valor[AJ_HERO] != 0, k;
      float hero = semHero ? 0.0f : (cheio ? 150.0f : 96.0f);
      gfx_cor((GfxRect){ x, y, w, mh }, 12.0f / mh, 0.09f, 0.095f, 0.11f, 1.0f);
      if (!semHero) gfx_cor((GfxRect){ x + (cheio ? 0.0f : 12.0f), y + (cheio ? 0.0f : 12.0f), w - (cheio ? 0.0f : 24.0f), hero },
                            (cheio ? 12.0f : 8.0f) / hero, 0.22f, 0.24f, 0.30f, 1.0f);
      { float cw = deitado ? (w - 24.0f) / 4.0f - 8.0f : (w - 24.0f) / 6.0f - 8.0f;
        float ch = deitado ? cw * 0.5625f : cw * 1.5f, fy = y + hero + 24.0f;
        int n = deitado ? 4 : 6;
        for (k = 0; k < n; k++)
          if (fy + ch < y + mh - 8.0f)
            gfx_cor((GfxRect){ x + 12.0f + (float)k * (cw + 8.0f), fy, cw, ch }, 6.0f / (cw < ch ? cw : ch), 0.28f, 0.30f, 0.36f, 1.0f); }
      { TxtLinha l = txt_linha_corta(TXT_MINI,
            semHero ? i18n("Sem herói: as fileiras sobem") : cheio ? i18n("Herói em tela cheia, fileiras por cima") : i18n("Herói contido, fileiras abaixo"),
            130, 133, 142, 255, w);
        txt_desenhar(l, x, y + mh + 10.0f);
        return mh + 10.0f + l.h + 8.0f; }
    }
    case AJ_QUALIDADE: {
      // Quatro barras, uma por resolucao; as que o teto deixa passar acesas.
      static const char *R[] = { "720p", "1080p", "4K" };
      int teto = valor[AJ_QUALIDADE], i;   // 0 auto, 1 4K, 2 1080p, 3 720p
      float bw = (w - 2.0f * 14.0f) / 3.0f;
      for (i = 0; i < 3; i++) {
        int passa = teto == 0 || (teto == 1) || (teto == 2 && i <= 1) || (teto == 3 && i == 0);
        float bh = 40.0f + (float)i * 40.0f, px = x + (float)i * (bw + 14.0f);
        gfx_cor((GfxRect){ px, y + 120.0f - bh, bw, bh }, 6.0f / bw, passa ? ar : 0.30f, passa ? ag : 0.32f, passa ? ab : 0.38f, passa ? 0.9f : 0.6f);
        { TxtLinha l = txt_linha(TXT_MINI, R[i], 200, 203, 210, 255);
          txt_desenhar(l, px + (bw - l.w) * 0.5f, y + 128.0f); }
      }
      return 128.0f + 30.0f;
    }
    default: return 0.0f;
  }
  (void)y0;
}

void ajustes_desenhar(Uint32 agora) {
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
  // "Reproducao · 1 de 7" era a OPCAO dentro da categoria, e foi lido como
  // "categoria 1 de 7" quando as categorias ja eram 8 (dono, 22/09, com a
  // Diagnostico nova). A contagem estava certa; a frase e que nao dizia de
  // que. Agora diz as duas, e o 8 sai de AJ_N_SECOES, nunca escrito a mao.
  snprintf(pos, sizeof pos, i18n("Categoria %d de %d · %s · opção %d de %d"),
           sec + 1, AJ_N_SECOES, i18n(SECOES[sec].titulo),
           focoOp - SECOES[sec].ini + 1, secN(sec));
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
    if (!focoIndice && focoOp == AJ_ESPACO) {
      hy += 22.0f;
      hy += desenhaPainelImagens(hx, hy, hw);
    } else if (!focoIndice) {
      float ph;
      hy += 22.0f;
      ph = desenhaPrevia(focoOp, hx, hy, hw);
      hy += ph > 0.0f ? ph : -22.0f;
    }
    hy += 34.0f;
    // O RODAPE DE AJUDA DIZ O QUE FUNCIONA NO CONTROLE, e nao o que funciona no
    // teclado do Mac. Uma linha por dica, desenhada a mao: ver desenhaDicas.
    //
    // i18n() AQUI, embora text.c ja traduza tudo o que desenha. Estas linhas
    // vivem num VETOR e chegam ao desenho por ponteiro, entao a varredura nao
    // as ve como argumento de funcao de desenho e cai na heuristica de
    // portugues — que nao reconheceu "Escolher categoria" nem "Entrar na
    // categoria" (sem acento e sem nenhuma das palavras da lista). Resultado:
    // duas linhas em portugues no meio da interface em ingles, na foto do
    // dono. Escrever i18n( e a declaracao de que aquilo e tela, e a varredura
    // cobra a chave sem adivinhar idioma. A traducao dupla e inofensiva: a
    // segunda busca nao acha a frase em ingles e devolve o que recebeu.
    { const char *dIdx[] = { i18n("↑ ↓   Escolher categoria"),
                             i18n("OK ou →   Entrar na categoria"),
                             i18n("Voltar   Sair dos ajustes") };
      const char *dAcao[] = { i18n("↑ ↓   Navegar"),
                              i18n("OK   Abrir"),
                              i18n("Voltar   Ir para as categorias") };
      const char *dVal[] = { i18n("↑ ↓   Navegar"),
                             i18n("OK   Alterar o valor"),
                             i18n("Voltar   Ir para as categorias") };
      const char *dEdi[] = { i18n("← →   Alterar o valor"),
                             i18n("OK   Confirmar"),
                             i18n("Voltar   Confirmar") };
      const char *const *d = focoIndice ? dIdx
                           : emEdicao ? dEdi
                           : OPCOES[focoOp].tipo == OP_ACAO ? dAcao : dVal;
      desenhaDicas(d, 3, hx, hy, hw, 155, 159, 169); }
  }

  gfx_recorte(AJ_LISTA_X - NV_ANEL_FOCO, AJ_TOPO,
               AJ_LISTA_W + NV_ANEL_FOCO * 2, AJ_BASE - AJ_TOPO);
  float y = AJ_TOPO - scrollY;
  float aPag = anim_suave(paginaA), dxPag = (1.0f - aPag) * 28.0f;
  { int s = sec;
    // Cabecalho da categoria: o titulo GRANDE, e o unico marco do grupo.
    float aC = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f) * aPag;
    TxtLinha ts = txt_linha(TXT_HEADLINE, SECOES[s].titulo, 226, 228, 236, 255);
    if (aC > 0.005f && y < AJ_BASE)
      txt_desenhar_alpha(ts, AJ_LISTA_X + AJ_PAD + dxPag, y + AJ_SEC_CABEC - ts.h - 6.0f, aC);
    y += AJ_SEC_CABEC;
    for (int k = 0; k < secN(s); k++) {
      int op = SECOES[s].ini + k;
      const char *sub = subsecaoDe(op);
      if (sub) {
        // Rotulo do bloco mais um fio: sem o fio, um rotulo cinza no meio de
        // linhas escuras se le como mais uma linha desligada.
        float aS = anim_clamp((y - (AJ_TOPO - 70.0f)) / 60.0f, 0.0f, 1.0f) * aPag;
        if (aS > 0.005f && y < AJ_BASE) {
          TxtLinha tsub = txt_linha(TXT_CAPTION, sub, 156, 159, 168, 255);
          GfxRect fio = { AJ_LISTA_X + AJ_PAD + tsub.w + 18.0f + dxPag,
                          y + AJ_SUB_CABEC - 18.0f,
                          AJ_LISTA_W - AJ_PAD * 2.0f - tsub.w - 18.0f, 1.0f };
          txt_desenhar_alpha(tsub, AJ_LISTA_X + AJ_PAD + dxPag,
                             y + AJ_SUB_CABEC - tsub.h - 8.0f, aS);
          if (fio.w > 20.0f)
            gfx_cor(fio, 0.5f, 0.60f, 0.62f, 0.68f, 0.20f * aS);
        }
        y += AJ_SUB_CABEC;
      }
      desenhaLinha(op, y, animFoco[op], dxPag, aPag);
      y += AJ_LINHA_H + AJ_LINHA_GAP;
    }
  }
  gfx_sem_recorte();

  float total = yDaOpcao(SECOES[sec].ini + secN(sec) - 1) + AJ_LINHA_H;
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

  // A modal de digitacao e a ultima: ela e sempre a pergunta mais recente da
  // tela, e tem de ficar por cima ate do cartao de vinculo.
  if (teclado_aberto()) teclado_desenhar(agora);
}
