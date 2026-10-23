// LIVE TV POR FONTE — o modelo de dados do guia, sem SDL e sem desenho.
//
// POR QUE EXISTE (plano docs/plans/guia-tv, M1/M2): o guia era UMA lista de 900
// canais e 48 categorias somando todas as fontes, com categorias fundidas por
// nome e ordenadas A-Z. Nos logs da 2.0.x, 18 de 23 pessoas com Xtream viam ~5%
// da lista, sem aviso; canal de categoria alem da 48a sumia em silencio.
//
// O QUE MUDA:
//   - FONTE e o primeiro nivel: addon (por base), Stalker, Xtream, M3U por URL.
//     Cada uma tem chave estavel (ltv_chave_fonte), grupos proprios na ordem do
//     provedor (A-Z opcional por fonte), numeros proprios e cache proprio.
//   - Strings numa ARENA de blocos (ponteiros estaveis, sem realloc), e nao em
//     char[600] fixos por canal: ~200 B por canal em vez de ~2,4 KB.
//   - TETO POR PLATAFORMA (ltv_teto_canais), e nunca corte silencioso: quando
//     a fonte passa do que cabe, entram GRUPOS INTEIROS na ordem do provedor (ou
//     os que a pessoa escolheu) e a fonte fica marcada "cortada" com as contas,
//     para a tela dizer e oferecer a escolha.
//   - Numeracao da decisao 7 do dono (ltv_numero_achar).
//   - M3U por URL (#EXTINF) em fluxo, sem a lista inteira na memoria.
//
// Tudo aqui e puro e testado em tests/ltv.c. A E/S (dados_ler/gravar) fica nas
// poucas funcoes marcadas "disco".
#ifndef NV_LTV_H
#define NV_LTV_H
#include <stddef.h>

// --- arena de strings ---------------------------------------------------------
// Blocos de LTV_BLOCO bytes encadeados; uma string nunca muda de lugar depois de
// copiada (o canal guarda o ponteiro). "" nao aloca. Soltar libera tudo.
#define LTV_BLOCO (64 * 1024)
typedef struct LtvBloco LtvBloco;
typedef struct { LtvBloco *prim, *atual; size_t usado, total; } LtvArena;
const char *ltv_dup(LtvArena *a, const char *s);
const char *ltv_dupn(LtvArena *a, const char *s, size_t n);
void   ltv_arena_soltar(LtvArena *a);
size_t ltv_arena_bytes(const LtvArena *a);   // reservados (para o log de memoria)

// --- fontes -------------------------------------------------------------------
enum { LTV_ADDON = 0, LTV_STALKER, LTV_XTREAM, LTV_M3U };
// Estado de saude: o ponto do chip na barra de fontes.
enum { LTV_SAUDE_OK = 0, LTV_SAUDE_DEGRADADA, LTV_SAUDE_FORA, LTV_SAUDE_CARREGANDO };
#define LTV_CHAVE 24
// "a<8 hex>" (addon, hash da base), "xt", "st", "m<8 hex>" (M3U, hash da URL).
// A chave nunca carrega a URL nem credencial: e o que vai para arquivo e log.
void ltv_chave_fonte(int tipo, const char *ref, char *dst, size_t n);
unsigned long ltv_hash(const char *s);
const char *ltv_tipo_nome(int tipo);          // "addon", "stalker", "xtream", "m3u"

// Teto de canais RETIDOS somando as fontes. `plat`: 0 webOS, 1 Tizen .wgt,
// 2 Tizen .tpk, 3 Android (o PtvPlataforma de perfiltv.h). `memMB` 0 = nao se
// sabe. TV de pouca RAM (< 1200 MB) desce para 5.000.
int ltv_teto_canais(int plat, long memMB);

// --- grupos e o teto ------------------------------------------------------------
// Decide quais grupos de UMA fonte entram. `sel` (nomes escolhidos pela pessoa,
// nsel 0 = sem escolha) manda quando existe; sem escolha, grupos INTEIROS na
// ordem do provedor ate a `cota` (o primeiro grupo entra sempre, mesmo maior
// que a cota, cortado no fim — senao uma fonte de um grupo so mostraria zero).
// entra[i] = 1 para os admitidos. Devolve quantos canais entram.
int ltv_grupos_admitir(const int *nPorGrupo, const char *const *nomes, int ng,
                       const char *const *sel, int nsel, int cota,
                       unsigned char *entra);

// --- numeracao (decisao 7) ------------------------------------------------------
// Um canal da vista, para a busca por numero. `ordemFonte` = posicao da fonte
// na barra (menor vem antes).
typedef struct { int fonte, numero, fav, ordemFonte; } LtvNum;
// Onde o numero leva. `filtro` >= 0: so a fonte filtrada. `filtro` < 0
// ("Todas"): primeiro os favoritos com aquele numero, depois as fontes na ordem
// da barra. Devolve o indice em `c` ou -1. `*ambiguo` (opcional) = 1 quando o
// mesmo numero existe em mais de uma fonte (a tela mostra "fonte · numero").
int ltv_numero_achar(const LtvNum *c, int n, int numero, int filtro, int *ambiguo);

// Digitar numero no controle: o estado do selo "1 0 2 ▸ ESPN".
#define LTV_DIG_MAX 5
#define LTV_DIG_MS  1500u
typedef struct { char d[LTV_DIG_MAX + 1]; unsigned desde; } LtvDigitos;
// Um digito novo (0..9). 1 se entrou. O "0" so entra com digitacao em curso:
// sozinho ele e o "ultimo canal" (decisao 5) e quem chama decide.
int  ltv_dig_apertar(LtvDigitos *g, int digito, unsigned agora);
int  ltv_dig_ativo(const LtvDigitos *g);
// 1 quando venceu o prazo: *numero recebe o valor e o estado zera.
int  ltv_dig_pronto(LtvDigitos *g, unsigned agora, int *numero);
// OK confirma na hora.
int  ltv_dig_confirmar(LtvDigitos *g, int *numero);
void ltv_dig_limpar(LtvDigitos *g);

// --- favoritos ----------------------------------------------------------------------
// Uma linha do arquivo de favoritos. Formato novo: "chave\tid"; o antigo era so
// o id (guia-fav.txt). `chave` sai "" no antigo: vale para qualquer fonte, e a
// primeira publicacao da lista reescreve com a fonte certa (migracao).
int ltv_fav_linha(const char *linha, char *chave, size_t nc, char *id, size_t ni);

// --- preferencias por fonte (disco) ----------------------------------------------
// A-Z, fuso (minutos), formato/modo do player gravados pelo diagnostico, e os
// grupos escolhidos quando a fonte passa do teto.
typedef struct {
  char chave[LTV_CHAVE];
  int  az;          // 1 = grupos em ordem alfabetica
  int  fusoMin;     // deslocamento da grade, -720..+840
  int  formato;     // 0 automatico, 1 HLS, 2 TS (diagnostico)
  int  modo;        // -1 = o de Ajustes; 0..2 = modo A/B/C do player
  int  proxy;       // -1 = o de Ajustes; 0/1
  char grupos[2048];// grupos escolhidos, separados por '\n'; "" = sem escolha
} LtvPref;
#define LTV_PREF_MAX 16
int    ltv_pref_de_texto(const char *t, LtvPref *out, int max);
size_t ltv_pref_texto(const LtvPref *p, int n, char *dst, size_t tam);
// As prefs do perfil (cache em memoria, gravadas a cada mudanca). Ponteiro
// estavel ate a proxima ltv_pref_carregar.
void     ltv_pref_carregar(int perfil);
LtvPref *ltv_pref(const char *chave, int criar);
void     ltv_pref_gravar(void);
// Grupos escolhidos como vetor (aponta para dentro de p->grupos, ate mudar).
int  ltv_pref_grupos(const LtvPref *p, const char **v, int max);
void ltv_pref_definir_grupos(LtvPref *p, const char *const *v, int n);

// --- M3U ------------------------------------------------------------------------------
typedef struct {
  const char *nome, *url, *tvgId, *tvgNome, *logo, *grupo;
  const char *ua, *referer;              // |user-agent=, |referer= ou #EXTVLCOPT
  const char *catchup, *catchupSrc;      // guardados para a v2 (catch-up)
  int chno;                              // tvg-chno (0 = nao veio)
  int catchupDias;
} LtvM3uCanal;
typedef int (*LtvM3uCb)(const LtvM3uCanal *c, void *u);   // 0 = parar
// Parser em fluxo: alimenta pedacos de qualquer tamanho. Linha maior que
// LTV_M3U_LINHA e descartada inteira (nunca cortada no meio de uma URL).
#define LTV_M3U_LINHA 4096
typedef struct {
  LtvM3uCb cb; void *u;
  char linha[LTV_M3U_LINHA]; size_t nl; int linhaGrande;
  char ext[LTV_M3U_LINHA]; int temExt;
  char opUa[256], opRef[256];
  char urlTvg[1024];
  int  nCanais, cabecalho, parou, nLinhas;
} LtvM3u;
void ltv_m3u_iniciar(LtvM3u *p, LtvM3uCb cb, void *u);
int  ltv_m3u_alimentar(LtvM3u *p, const char *buf, size_t n);   // 0 = o cb pediu parar
void ltv_m3u_fim(LtvM3u *p);
// Valor de atributo `nome="..."` numa linha #EXTINF (sem caixa no nome).
int  ltv_m3u_atributo(const char *linha, const char *nome, char *dst, size_t n);

// --- fontes por URL (M9): M3U e XMLTV dos Ajustes -------------------------------
// Por perfil, em ltv-urls-p<N>.txt ("m3u|xmltv TAB nome TAB url"). A URL pode
// levar usuario e senha: fica so no disco do aparelho, nunca em log nem na conta
// (a tela de Ajustes mostra so o host). Copia sob trava: o fio do guia le.
enum { LTV_URL_M3U = 0, LTV_URL_XMLTV = 1 };
#define LTV_URLS_MAX 8
typedef struct { int tipo; char nome[64]; char url[1024]; } LtvUrl;
void ltv_urls_carregar(int perfil);
int  ltv_urls_copiar(LtvUrl *out, int max);
int  ltv_urls_adicionar(int tipo, const char *nome, const char *url);   // indice ou -1
void ltv_urls_remover(int i);
int  ltv_urls_de_texto(const char *t, LtvUrl *out, int max);
size_t ltv_urls_texto(const LtvUrl *v, int n, char *dst, size_t tam);
// "host" da URL para a tela (sem usuario, senha, caminho nem consulta).
void ltv_url_host(const char *url, char *dst, size_t n);

#endif
