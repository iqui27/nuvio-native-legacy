#include "trakt.h"
#include "vistoep.h"
#include "ajustes.h"
#include "artemetahub.h"
#include "descoberta.h"
#include "jsw.h"
#include "idioma.h"
#include "rede.h"
#include "metaprov.h"
#include "fichameta.h"
#include "js.h"
#include "nuvem.h"
#include "cwordem.h"
#include "traktscrobble.h"
#include "traktult.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>


static char token[128], cliente[80];
static int  ligado;
// Sessao confirmada morta (401 e o refresh tambem recusado): nenhum pedido
// autenticado sai ate um token novo (trakt_definir) — cada um era um 401 no log.
static int  sessaoMorta;
// Cabecalho e geracao pertencem a mesma credencial. Trocar/desvincular o
// Trakt no mesmo perfil tambem invalida respostas sem apagar provas da conta.
static pthread_mutex_t travaCred = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long credGeracao = 1;
static char filmesAtiv[40];
static unsigned long long filmesAtivMapa;
typedef struct { unsigned long long mapa, credencial; } HistoricoPedido;
static HistoricoPedido historicoPedido(void) {
  HistoricoPedido p;
  p.mapa = cat_historico_geracao();
  pthread_mutex_lock(&travaCred);
  p.credencial = credGeracao;
  pthread_mutex_unlock(&travaCred);
  return p;
}
static int historicoPedidoAtual(HistoricoPedido p) {
  int atual;
  pthread_mutex_lock(&travaCred);
  atual = p.credencial == credGeracao && p.mapa == cat_historico_geracao();
  pthread_mutex_unlock(&travaCred);
  return atual;
}
unsigned long long trakt_credencial_geracao(void) {
  unsigned long long g;
  pthread_mutex_lock(&travaCred); g = credGeracao; pthread_mutex_unlock(&travaCred);
  return g;
}
int trakt_historico_aplicar(const char *id, const char *tipo, int visto,
                            unsigned long long mapa, unsigned long long credencial) {
  HistoricoPedido p = { mapa, credencial };
  int atual;
  pthread_mutex_lock(&travaCred);
  atual = p.credencial == credGeracao &&
    cat_historico_definir_se_geracao(id, tipo, visto, p.mapa);
  pthread_mutex_unlock(&travaCred);
  return atual;
}
static int historicoDefinirPedido(const char *id, const char *tipo, int visto,
                                   HistoricoPedido p) {
  return trakt_historico_aplicar(id, tipo, visto, p.mapa, p.credencial);
}

// Estado da ultima escrita iniciada pelo menu. O corpo de um POST nao e prova
// de sucesso: o Trakt tambem devolve corpo em 4xx. O consumidor usa este
// estado para so espelhar a intencao local depois de um HTTP 2xx.
enum { TK_OP_NENHUMA, TK_OP_PENDENTE, TK_OP_CONFIRMADA, TK_OP_FALHA };
enum { TK_OP_LISTA = 1, TK_OP_HISTORICO = 2 };
static volatile int listaEstado, historicoEstado;

// Mantem o contrato antigo (so IMDb) sem perder o tipo quando o item ja esta
// no catalogo. O sufixo de episodio continua sendo um fallback para chamadas
// antigas feitas antes de o catalogo estar montado.
extern const char *cat_tipo_por_imdb(const char *imdb);
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

static const char *tipo_item(const char *tipo, const char *imdb) {
  if (tipo && (!strcmp(tipo, "series") || !strcmp(tipo, "show"))) return "series";
  if (tipo && !strcmp(tipo, "movie")) return "movie";
  return cat_tipo_por_imdb(imdb);
}
static pthread_mutex_t travaLista = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t travaHistorico = PTHREAD_MUTEX_INITIALIZER;

static void estadoEscrever(volatile int *estado, int valor) {
  __atomic_store_n(estado, valor, __ATOMIC_RELEASE);
}

static int estadoLer(const volatile int *estado) {
  return __atomic_load_n(estado, __ATOMIC_ACQUIRE);
}

static int cabecalhosPedido(HistoricoPedido p, const char **cab,
                            char *aut, size_t nAut, char *chave, size_t nChave) {
  int atual;
  pthread_mutex_lock(&travaCred);
  atual = ligado && !sessaoMorta && p.credencial == credGeracao && p.mapa == cat_historico_geracao();
  if (atual) {
    snprintf(aut, nAut, "Authorization: Bearer %s", token);
    snprintf(chave, nChave, "trakt-api-key: %s", cliente);
    cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;
  }
  pthread_mutex_unlock(&travaCred);
  return atual;
}

// API pequena e interna ao port: a declaracao fica no consumidor porque o
// contrato publico historico de trakt_watchlist/trakt_assistido continua void.
int trakt_operacao_estado(int tipo) {
  if (tipo == TK_OP_LISTA) return estadoLer(&listaEstado);
  if (tipo == TK_OP_HISTORICO) return estadoLer(&historicoEstado);
  return TK_OP_NENHUMA;
}

int trakt_ativo(void) { return ligado; }

void trakt_sessao_morta(void) {
  pthread_mutex_lock(&travaCred);
  if (ligado && !sessaoMorta) {
    sessaoMorta = 1;
    printf("[trakt] sessao expirada: sem pedidos ate reconectar\n");
  }
  pthread_mutex_unlock(&travaCred);
}
int trakt_sessao_e_morta(void) { return sessaoMorta; }

// Definida junto de trakt_social; declarada aqui porque trakt_definir a chama.
void trakt_social_reavaliar(void);

// A CREDENCIAL FOI RECUSADA (HTTP 401). O 401 so chegava ao log — a tela
// seguia dizendo "conectado" e o dono so percebia quando o "Continuar
// assistindo" nao vinha. Quem avisa e a camada de rede (rede_avisar_401),
// porque o 401 pode vir de qualquer chamada — continuar, historico, extras.
// traktauth_passo e quem age: tenta renovar pelo refresh guardado e, se o
// refresh tambem morreu, derruba o estado para invalido.
static volatile int credRecusada;
static void avisoHttp401(const char *url) {
  // /oauth/* responde 401 por credencial de APLICATIVO ruim — e problema do
  // pacote, nao da sessao do usuario.
  //
  // .../activities TAMBEM NAO E PROVA DE NADA. MEDIDO na LG com o token bom
  // desta TV: friends/activities e following/activities respondem 401 SEMPRE
  // (o feed agregado exige um escopo que esta conta nao tem), enquanto
  // sync/playback, sync/history e as listas respondem 200 com o MESMO token.
  // Como este aviso disparava neles, TODO arranque marcava "credencial
  // recusada", gastava o refresh token (que o Trakt gira a cada uso) e ainda
  // pedia remontagem da home. O fallback honesto ja existe em socialPorSeguidos.
  if (!strstr(url, "api.trakt.tv") || strstr(url, "/oauth/") ||
      strstr(url, "/activities")) return;
  if (ligado && !estadoLer(&credRecusada)) {
    estadoEscrever(&credRecusada, 1);
    printf("[trakt] HTTP 401: credencial recusada — renovacao pendente\n");
    fflush(stdout);
  }
}
int trakt_recusada(void) { return estadoLer(&credRecusada); }

// As tabelas da ultima leitura (definidas mais abaixo, junto de quem as
// preenche). Os contadores ficam aqui porque trakt_esquecer os zera.
static int nUlt, nProxIds, nPlay;
// play[]/nPlay: trakt_continuar (fio da sincronizacao) zera e reescreve;
// trakt_playback_remover (fio do menu, tirarremoto) le e apaga. Trava FOLHA:
// ninguem toma outra trava com ela na mao, e nunca atravessa uma chamada de rede.
static pthread_mutex_t playTrava = PTHREAD_MUTEX_INITIALIZER;
static void proxMemLimpar(void);   // memoria do "a seguir" (mais abaixo)

void trakt_esquecer(void) {
  pthread_mutex_lock(&travaCred);
  if (!++credGeracao) ++credGeracao;
  filmesAtiv[0] = 0;
  sessaoMorta = 0;
  token[0] = 0;
  cliente[0] = 0;
  ligado = 0;
  pthread_mutex_unlock(&travaCred);
  // E O QUE A ULTIMA LEITURA DEIXOU. Esquecer vale tambem na TROCA DE PERFIL
  // (traktauth_trocar_perfil), e ai o "a seguir" e os ids de playback do perfil
  // anterior continuariam respondendo trakt_e_a_seguir/trakt_playback_remover
  // para os cards do perfil novo. Zerar contadores so encurta uma varredura que
  // o fio da descoberta esteja fazendo — nunca a faz passar do fim.
  nUlt = nProxIds = 0;
  pthread_mutex_lock(&playTrava); nPlay = 0; pthread_mutex_unlock(&playTrava);
  proxMemLimpar();
  estadoEscrever(&credRecusada, 0);
  trakt_social_reavaliar();
  printf("[trakt] credencial esquecida\n");
}

int trakt_credencial_igual(const char *tk, const char *cli) {
  int igual;
  pthread_mutex_lock(&travaCred);
  const char *c = (cli && *cli) ? cli : cliente;
  igual = ligado && tk && *tk && strlen(tk) < sizeof token &&
    !strcmp(token, tk) && !strcmp(cliente, c);
  pthread_mutex_unlock(&travaCred);
  return igual;
}

// Marca do ultimo /sync/watched/movies aplicado (carregarFilmesVistos).
int trakt_definir(const char *tk, const char *cli) {
  if (!tk || !*tk || strlen(tk) >= sizeof token ||
      (cli && strlen(cli) >= sizeof cliente)) return 0;
  pthread_mutex_lock(&travaCred);
  if (!++credGeracao) ++credGeracao;
  sessaoMorta = 0;
  filmesAtiv[0] = 0;   // conta nova: o mapa de filmes vistos vem de novo
  snprintf(token, sizeof token, "%s", tk);
  if (cli && *cli) snprintf(cliente, sizeof cliente, "%s", cli);
  ligado = token[0] && cliente[0];
  int ativo = ligado;
  pthread_mutex_unlock(&travaCred);
  proxMemLimpar();
  // Token NOVO limpa a marca de recusa — e o mesmo caminho por onde a
  // renovacao (traktauth) e o pareamento novo chegam.
  estadoEscrever(&credRecusada, 0);
  trakt_social_reavaliar();
  rede_avisar_401(avisoHttp401);
  printf("[trakt] credencial da conta: %s\n",
         ativo ? "ativa" : "sem client id do aplicativo (ver tools/env.sh)");
  return ativo;
}


int trakt_cabecalhos(const char **cab, char *aut, size_t nAut,
                     char *chave, size_t nChave) {
  pthread_mutex_lock(&travaCred);
  if (!ligado || sessaoMorta) { pthread_mutex_unlock(&travaCred); return 0; }
  snprintf(aut, nAut, "Authorization: Bearer %s", token);
  snprintf(chave, nChave, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;
  pthread_mutex_unlock(&travaCred);
  return 1;
}

// A chave do APLICATIVO, venha ela de onde vier. Duas fontes e nesta ordem:
// o `cliente` deste modulo (vinculo desta TV, ou art/trakt.txt) e, faltando
// ele, a chave compilada no pacote que nuvem.c guarda. A segunda e a que
// interessa aqui: e a unica que existe em quem nunca vinculou conta nenhuma.
static const char *chaveApp(void) {
  if (cliente[0]) return cliente;
  return nuvem_trakt_cliente();
}

int trakt_cabecalhos_publicos(const char **cab, char *chave, size_t nChave) {
  const char *k = chaveApp();
  if (!k || !k[0]) return 0;
  snprintf(chave, nChave, "trakt-api-key: %s", k);
  cab[0] = "trakt-api-version: 2"; cab[1] = chave; cab[2] = NULL;
  return 1;
}

int trakt_carregar(const char *dirArte) {
  char caminho[600], linha[300], *tab;
  FILE *f;
  // Vinculo feito NESTA TV (traktauth_carregar, que roda antes) ganha do
  // arquivo do pacote. MEDIDO: art/trakt.txt de desenvolvimento, com token de
  // 31/08 ja vencido, sobrescrevia o token recem-autorizado a cada arranque e
  // tudo do Trakt voltava a 401 — "autorizei e continua sem".
  if (token[0]) { printf("[trakt] vinculo deste perfil mantido; arquivo do pacote ignorado\n"); return 1; }
  snprintf(caminho, sizeof caminho, "%s/trakt.txt", dirArte ? dirArte : ".");
  f = fopen(caminho, "r");
  if (!f) { printf("[trakt] sem %s\n", caminho); return 0; }
  if (fgets(linha, sizeof linha, f)) {
    char *fim;
    tab = strchr(linha, '\t');
    if (tab) {
      *tab = 0;
      snprintf(cliente, sizeof cliente, "%s", tab + 1);
      fim = cliente + strlen(cliente);
      while (fim > cliente && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
    }
    snprintf(token, sizeof token, "%s", linha);
  }
  fclose(f);
  ligado = token[0] && cliente[0];
  // O ouvinte de 401 tambem vale para a credencial do PACOTE: este caminho nao
  // passa por trakt_definir, e sem o registro a sessao morta do arquivo seguia
  // invisivel — exatamente o caso que o dono relatou.
  rede_avisar_401(avisoHttp401);
  printf("[trakt] %s\n", ligado ? "credencial carregada" : "credencial incompleta");
  return ligado;
}


// A PRIMEIRA URL DE UMA LISTA DE IMAGENS do bloco "images" do Trakt.
//
// `?extended=full` ja devolve, dentro de cada movie/show, um objeto assim:
//   "images":{"logo":["media.trakt.tv/.../x.png.webp"],"fanart":[...],
//             "poster":[...],"thumb":[],"banner":[],"clearart":[]}
// MEDIDO na LG: os 39 itens de /sync/playback traziam poster e fanart, 37
// traziam logo. Cada uma dessas artes custava um GET ao Cinemeta que agora nao
// precisa acontecer.
//
// js_texto nao serve: o valor e um ARRAY de strings, nao uma string. As URLs
// vem SEM esquema ("media.trakt.tv/..."), entao o https:// e colado aqui.
static int imagemTrakt(const char *bloco, const char *fim, const char *chave,
                       char *dst, size_t n) {
  char alvo[24];
  const char *img, *p, *ini;
  size_t L;
  if (!bloco || !dst || !n) return 0;
  img = strstr(bloco, "\"images\"");
  if (!img || (fim && img >= fim)) return 0;
  snprintf(alvo, sizeof alvo, "\"%s\"", chave);
  p = strstr(img, alvo);
  if (!p || (fim && p >= fim)) return 0;
  p = strchr(p + strlen(alvo), '[');
  if (!p) return 0;
  // Lista VAZIA ("thumb":[]) nao e arte: nao inventar uma.
  while (*++p && (unsigned char)*p <= ' ') { }
  if (*p != '"') return 0;
  ini = ++p;
  while (*p && *p != '"') p++;
  L = (size_t)(p - ini);
  if (!L || L + 9 >= n) return 0;
  if (!strncmp(ini, "http", 4)) snprintf(dst, n, "%.*s", (int)L, ini);
  else                          snprintf(dst, n, "https://%.*s", (int)L, ini);
  return 1;
}

// O QUE O PROPRIO TRAKT JA MANDOU, aproveitado em vez de rebaixado.
// Preenche arte, sinopse e a legenda (ano/duracao/minutos que faltam) a partir
// do bloco movie/show de `?extended=full`. Quem sai daqui com poster, backdrop
// e sinopse nao volta ao Cinemeta — ver a guarda em trakt_enfeitar_lote.
static void doBlocoTrakt(CatItem *d, const char *bloco, const char *fim,
                         const char *tipo) {
  int minutos, ano;
  if (!bloco) return;
  imagemTrakt(bloco, fim, "poster", d->poster, sizeof d->poster);
  imagemTrakt(bloco, fim, "fanart", d->backdrop, sizeof d->backdrop);
  snprintf(d->backdropTrakt, sizeof d->backdropTrakt, "%s", d->backdrop);
  imagemTrakt(bloco, fim, "logo",   d->logo,     sizeof d->logo);
  if (!d->backdrop[0] && strlen(d->poster) < sizeof d->backdrop)
    snprintf(d->backdrop, sizeof d->backdrop, "%s", d->poster);
  if (!d->sinopse[0]) js_texto(bloco, fim, "overview", d->sinopse, sizeof d->sinopse);
  minutos = (int)js_num(bloco, fim, "runtime", 0.0);
  ano     = (int)js_num(bloco, fim, "year", 0.0);
  if (ano > 0 || minutos > 0) {
    char a[16] = "", r[16] = "";
    if (ano > 0)     snprintf(a, sizeof a, "%d", ano);
    if (minutos > 0) snprintf(r, sizeof r, "%d min", minutos);
    snprintf(d->meta, sizeof d->meta, "%s%s%s", a,
             (a[0] && r[0]) ? "  \xc2\xb7  " : "", r);
  }
  // Mesma conta de enfeitar: a porcentagem e do Trakt, a duracao e daqui.
  if (minutos > 0) {
    if (d->progresso > 0 && d->progresso < 100)
      d->restanteMin = minutos - (minutos * d->progresso) / 100;
    else if (d->progresso == 0)
      d->restanteMin = minutos;
  }
  // ROTULO DE TIPO PASSA PELA TABELA. Este campo vai direto para a tela (a
  // linha "Programa de TV · 2025 · 51 min" do destaque), e composto ele nunca
  // casa com chave — o mesmo motivo que ja esta escrito em contalib.c, que
  // corrigiu a metade dele. Com o app em ingles a linha saia em portugues.
  snprintf(d->genero, sizeof d->genero, "%s",
           i18n((tipo && !strcmp(tipo, "series")) ? "Programa de TV" : "Filme"));
  // CLASSIFICACAO SO COM VALOR REAL (#243). Aqui havia um "14" cravado: era o
  // unico selo que sobrava numa linha de Continuar assistindo e nao era o do
  // titulo. O Trakt manda `certification` no bloco (extended=full); sem ele, vazio.
  if (!d->classificacao[0]) {
    char cert[sizeof d->classificacao] = "";
    if (js_texto(bloco, fim, "certification", cert, sizeof cert) && cert[0])
      snprintf(d->classificacao, sizeof d->classificacao, "%s", cert);
  }
}

// Arte e sinopse por id do IMDb. O Trakt devolve so identificadores e
// progresso; quem tem imagem e o Cinemeta, que e o mesmo indice que os addons
// usam — entao o que aparece na tela e o que da para pedir fonte.
// O ULTIMO EPISODIO VISTO DE CADA SERIE, pelo historico (issue #66).
//
// /sync/playback so traz o que esta PAUSADO no meio. Quem assiste um episodio
// ate o fim e para ali nao tem registro de playback — e a serie sumia de
// "Continuar assistindo" ate o proximo episodio ser comecado em algum lugar.
// E o "missing shows" do relato. O historico (/sync/history, que este ciclo
// ja baixa para marcar vistos) diz qual foi o ultimo episodio visto de cada
// serie; o proximo dele e o "a seguir". Tabela lateral e nao campo em
// CatItem, pelo motivo dito em `play[]`: sizeof(CatItem) e o cabecalho do
// cache em disco.
#define TK_ULT_MAX 64
// TkUltimo e a regra de empate: traktult.h (issue #213).
static TkUltimo ult[TK_ULT_MAX];   // nUlt: ver trakt_esquecer
// Os ids ("tt:S:E") dos itens "a seguir" desta rodada, para enfeitar() saber
// que precisa CONFERIR que o episodio existe antes de publicar.
static char proxIds[TK_ULT_MAX][32];   // nProxIds: ver trakt_esquecer
static int ehProximo(const char *id) {
  int i;
  for (i = 0; i < nProxIds; i++) if (!strcmp(proxIds[i], id)) return 1;
  return 0;
}
int trakt_e_a_seguir(const char *id) { return id && ehProximo(id); }
int trakt_cw_concluiu(const char *id, long long progressoMs) {
  char base[24]; int t = 0, e = 0, i;
  const char *sep = id ? strchr(id, ':') : NULL;
  if (!sep || sscanf(sep + 1, "%d:%d", &t, &e) != 2) return 0;
  snprintf(base, sizeof base, "%.*s", (int)(sep - id), id);
  for (i = 0; i < nUlt; i++)
    if (!strcmp(ult[i].imdb, base) && ult[i].temporada == t &&
        ult[i].episodio == e && ult[i].quandoMs > progressoMs) return 1;
  return 0;
}
// O episodio existe no meta do Cinemeta? Os ids em videos[] sao "tt:S:E".
static int episodioExiste(const char *corpo, const char *serie, int t, int e) {
  char chave[48];
  snprintf(chave, sizeof chave, "\"id\":\"%s:%d:%d\"", serie, t, e);
  return strstr(corpo, chave) != NULL;
}

// OBRAS JA TENTADAS NO CINEMETA PARA A NOTA, nesta sessao (#243). Sem isto, quem
// o Cinemeta nao da nota (ou que estoura o tempo) voltava a rede a cada refacao
// da fileira. Tentou uma vez (achou, sem nota ou falhou), conta como satisfeito.
#define TK_TENTADAS_MAX 64
static char tentadas[TK_TENTADAS_MAX][24];
static int nTentadas, proxTentada;
static pthread_mutex_t tentadasTrava = PTHREAD_MUTEX_INITIALIZER;
static void obraBase(const char *imdb, char *out, size_t n) {
  size_t i = 0;
  while (imdb[i] && imdb[i] != ':' && i + 1 < n) { out[i] = imdb[i]; i++; }
  out[i] = 0;
}
static int jaTentada(const char *imdb) {
  char b[24]; int i, r = 0;
  obraBase(imdb, b, sizeof b);
  pthread_mutex_lock(&tentadasTrava);
  for (i = 0; i < nTentadas && !r; i++) r = !strcmp(tentadas[i], b);
  pthread_mutex_unlock(&tentadasTrava);
  return r;
}
static void marcarTentada(const char *imdb) {
  char b[24];
  obraBase(imdb, b, sizeof b);
  if (jaTentada(imdb)) return;
  pthread_mutex_lock(&tentadasTrava);
  if (nTentadas < TK_TENTADAS_MAX) snprintf(tentadas[nTentadas++], sizeof tentadas[0], "%s", b);
  else { snprintf(tentadas[proxTentada], sizeof tentadas[0], "%s", b);
         proxTentada = (proxTentada + 1) % TK_TENTADAS_MAX; }
  pthread_mutex_unlock(&tentadasTrava);
}

// MEMORIA DAS FICHAS DO CATALOGO (B2, arranque). O log da C9 mostra os MESMOS
// ids sendo baixados de novo a cada volta da descoberta (tt4955642 tres vezes
// nos primeiros 40 s, a lista inteira do "Continuar" refeita 3x): cada volta
// pagava de novo uma ficha por titulo, ~12-20 GETs, para o texto e a duracao que
// nao mudam em minutos. Guarda o corpo por (tipo, id, idioma) por 15 min, no
// maximo 24 fichas e so as de ate 256 KB (o videos[] de serie longa e grande).
// Falha nunca e guardada: o proximo ciclo tenta a rede de novo.
#define TK_FICHA_MAX 24
#define TK_FICHA_TTL_S 900
#define TK_FICHA_BYTES (256 * 1024)
static struct { char chave[64]; char *corpo; time_t quando; } fichas[TK_FICHA_MAX];
static int fichaProx;
static pthread_mutex_t fichaTrava = PTHREAD_MUTEX_INITIALIZER;

static char *fichaChave(char *k, size_t n, const char *tipo, const char *id) {
  snprintf(k, n, "%s/%s/%s", tipo, id, metaprov_idioma());
  return k;
}
static char *fichaBuscar(const char *tipo, const char *id) {
  char k[64], *r = NULL;
  int i;
  fichaChave(k, sizeof k, tipo, id);
  pthread_mutex_lock(&fichaTrava);
  for (i = 0; i < TK_FICHA_MAX && !r; i++)
    if (fichas[i].corpo && !strcmp(fichas[i].chave, k) &&
        time(NULL) - fichas[i].quando < TK_FICHA_TTL_S)
      r = strdup(fichas[i].corpo);
  pthread_mutex_unlock(&fichaTrava);
  return r;
}
static void fichaGuardar(const char *tipo, const char *id, const char *corpo) {
  char k[64], *c;
  int i;
  if (!corpo || strlen(corpo) > TK_FICHA_BYTES) return;
  c = strdup(corpo);
  if (!c) return;
  fichaChave(k, sizeof k, tipo, id);
  pthread_mutex_lock(&fichaTrava);
  for (i = 0; i < TK_FICHA_MAX; i++)
    if (fichas[i].corpo && !strcmp(fichas[i].chave, k)) break;
  if (i == TK_FICHA_MAX) { i = fichaProx; fichaProx = (fichaProx + 1) % TK_FICHA_MAX; }
  free(fichas[i].corpo);
  snprintf(fichas[i].chave, sizeof fichas[i].chave, "%s", k);
  fichas[i].corpo = c; fichas[i].quando = time(NULL);
  pthread_mutex_unlock(&fichaTrava);
}

// Ficha do Cinemeta (so ele) em cache proprio, para conferir episodios que a
// ficha do Nuvio nao lista. Devolve copia (free) ou NULL.
static char *cinemetaDaSerie(const char *tipo, const char *serie) {
  char *c = fichaBuscar("cinemeta-series", serie);
  if (c) return c;
  c = metaprov_meta_cinemeta_com(tipo, serie, 8, NULL, NULL);
  if (c) fichaGuardar("cinemeta-series", serie, c);
  return c;
}

static int enfeitar(CatItem *d, const char *tipo) {
  char url[300], *corpo;
  char serie[24];
  const char *dp;
  int precisaCinemeta, proximo, daConta, virou = 0, provFicha = -1;
  const char *epc;   // de onde saem o nome e a data do episodio "a seguir"
  char *corpoCine = NULL;
  // Arte PRIMEIRO, sem rede: mesma URL que trakt_lista ja monta. Antes cada
  // item do historico/local fazia GET ao Cinemeta so para ler poster/logo —
  // medido 2,1 s no Mac com paralelismo, e pior: se o Cinemeta falhava o
  // item SUMIA da fileira (compactacao). Metahub e deterministico pelo tt.
  arte_metahub_preencher(d);
  // Id de addon de anime ("kitsu:41370"): o Cinemeta nao o conhece, e o corte
  // no primeiro ':' pedia /meta/series/kitsu.json. Fica com o que o registro
  // trouxe (sem arte, o item nao entra na fileira, como sempre).
  if (strncmp(d->imdb, "tt", 2)) return d->poster[0] != 0;
  snprintf(serie, sizeof serie, "%s", d->imdb);
  dp = strchr(serie, ':');
  if (dp) *(char *)dp = 0;
  // Cinemeta so para o que o metahub nao tem: validar "a seguir", sinopse,
  // runtime/meta e nota. Arte ja esta; falha la NAO apaga o item.
  // "A seguir" e o do Trakt (historico) ou o da CONTA (vistos, issue #199):
  // os dois sao sugestao de episodio que ninguem confirmou que existe.
  daConta = cwo_conta_a_seguir(d->imdb);
  proximo = ehProximo(d->imdb) || daConta;
  precisaCinemeta = proximo || !d->sinopse[0] ||
                    !d->meta[0] || d->nota <= 0;
  if (!precisaCinemeta)
    return d->poster[0] != 0;

  // 8 s e nao 20: ate oito destes em paralelo antes da primeira fileira.
  // Medido no Mac: 2,1 s no caso bom; com um item lento eram 20 s vazios.
  // Catalogo do Nuvio primeiro (5 s, e some por 1 min se cair), Cinemeta depois.
  { int deCache = 0;
    char dk[64];
    corpo = fichaBuscar(tipo, serie);
    if (!corpo) {
      // Disco: a ficha de uma execucao anterior (6 h). Mostra titulo, sinopse e
      // duracao na primeira volta sem rede; a memoria e a rede a renovam.
      fichaChave(dk, sizeof dk, tipo, serie);
      corpo = fichameta_ler(dk, 6 * 3600);
      if (corpo) fichaGuardar(tipo, serie, corpo);
    }
    deCache = corpo != NULL;
    // "A seguir" que o cache nao confirma pode ser episodio novo: pergunta a
    // rede antes de descartar o item.
    if (corpo && proximo &&
        !episodioExiste(corpo, serie, d->temporada, d->episodio) &&
        !episodioExiste(corpo, serie, d->temporada + 1, 1)) {
      free(corpo); corpo = NULL; deCache = 0;
    }
    if (!corpo) {
      corpo = metaprov_meta(tipo, serie, 8, &provFicha);
      if (corpo) {
        fichaGuardar(tipo, serie, corpo);
        fichaChave(dk, sizeof dk, tipo, serie);
        fichameta_gravar(dk, corpo);
      }
    }
    (void)deCache; }
  marcarTentada(d->imdb);
  if (!corpo) {
    // "A seguir" sem meta: nao da para confirmar que o episodio existe.
    if (proximo) return 0;
    return d->poster[0] != 0;
  }
  // "A SEGUIR" SO ENTRA SE O EPISODIO EXISTE. Depois do ultimo da temporada o
  // proximo e o primeiro da seguinte; depois do ultimo da serie nao ha
  // proximo, e a serie nao entra — nao e "continuar", e "acabou".
  epc = corpo;
  if (proximo && !episodioExiste(corpo, serie, d->temporada, d->episodio) &&
      !episodioExiste(corpo, serie, d->temporada + 1, 1) &&
      provFicha != METAPROV_CINEMETA && !strcmp(tipo, "series")) {
    // A ficha do Nuvio pode estar atras do Cinemeta (The Rookie: 1 futuro
    // contra 18). Antes de largar o item, confere la; a ficha do Nuvio segue
    // valendo para todo o resto.
    corpoCine = cinemetaDaSerie(tipo, serie);
    if (corpoCine) {
      int t = -1, e = -1;
      if (episodioExiste(corpoCine, serie, d->temporada, d->episodio)) {
        t = d->temporada; e = d->episodio;
      } else if (episodioExiste(corpoCine, serie, d->temporada + 1, 1)) {
        t = d->temporada + 1; e = 1;
      }
      if (t >= 0) {
        printf("[trakt] episodio ausente na ficha Nuvio; Cinemeta tem: %s S%dE%d\n",
               serie, t, e);
        epc = corpoCine;
      }
    }
    if (epc == corpo)
      printf("[trakt] episodio ausente na ficha Nuvio; Cinemeta tambem nao tem: %s S%dE%d\n",
             serie, d->temporada, d->episodio);
  }
  if (proximo) {
    if (!episodioExiste(epc, serie, d->temporada, d->episodio)) {
      if (episodioExiste(epc, serie, d->temporada + 1, 1)) {
        char velho[sizeof d->imdb];
        snprintf(velho, sizeof velho, "%s", d->imdb);
        d->temporada++; d->episodio = 1;
        snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", serie, d->temporada, d->episodio);
        // So o da conta acompanha o id novo. O do Trakt continua como era
        // (ehProximo do id novo da 0); mexer nele nao e deste conserto.
        if (daConta) cwo_conta_trocar(velho, d->imdb);
        virou = 1;
      } else { free(corpo); free(corpoCine); return 0; }
    }
    // O nome do episodio, para a legenda do card.
    { char chave[48]; const char *v;
      snprintf(chave, sizeof chave, "\"id\":\"%s:%d:%d\"", serie, d->temporada, d->episodio);
      v = strstr(epc, chave);
      if (v) { const char *ini = v; while (ini > epc && *ini != '{') ini--;
               js_texto(ini, js_fim(ini), "name", d->nomeEpisodio, sizeof d->nomeEpisodio); } }
  }
  // A DATA DE ESTREIA DO EPISODIO "A SEGUIR" (issue #127), do mesmo videos[]
  // que acabou de confirmar que ele existe. E ela que separa o que ja foi ao ar
  // do que ainda vai — a Ordenacao de Continuar assistindo (cwordem.h) poe os
  // futuros no fim ou numa fileira propria. Vale para o Simkl tambem: o lote
  // dele passa por aqui (simkl_continuar -> trakt_enfeitar_lote). Episodio sem
  // `released` fica sem data, e sem data conta como exibido, como no web.
  if (d->progresso == 0 && d->temporada > 0 && d->episodio > 0 && !strcmp(tipo, "series")) {
    char chave[48], quando[40] = "";
    const char *v;
    snprintf(chave, sizeof chave, "\"id\":\"%s:%d:%d\"", serie, d->temporada, d->episodio);
    v = strstr(epc, chave);
    if (v) {
      const char *ini = v;
      long long ms;
      while (ini > epc && *ini != '{') ini--;
      if (!js_texto(ini, js_fim(ini), "released", quando, sizeof quando))
        js_texto(ini, js_fim(ini), "firstAired", quando, sizeof quando);
      ms = js_ms_iso(quando);
      cwo_marcar_estreia(d->imdb, ms > 0 ? ms : CWO_SEM_DATA);
      if (ms <= 0)
        printf("[trakt] estreia: %s sem released/firstAired no Cinemeta (\"%s\")\n",
               d->imdb, quando);
      // TEMPORADA NOVA DA CONTA: so com data, e ate 7 dias a frente
      // (cwo_virada_aceita). Serie terminada com a temporada seguinte so
      // anunciada nao vira "a seguir".
      if (daConta && virou &&
          !cwo_virada_aceita(ms > 0 ? ms : CWO_SEM_DATA, (long long)time(NULL) * 1000LL)) {
        printf("[trakt] a seguir da conta %s: temporada nova sem data ou a mais de 7 dias; fora\n",
               d->imdb);
        free(corpo); free(corpoCine);
        return 0;
      }
    } else {
      printf("[trakt] estreia: %s fora do videos[] do Cinemeta; sem data\n", d->imdb);
      if (daConta && virou) { free(corpo); free(corpoCine); return 0; }
    }
  }
  // So completa buracos: nao trocar metahub por vazio se o Cinemeta omitir.
  if (!d->poster[0])   js_texto(corpo, NULL, "poster", d->poster, sizeof d->poster);
  if (!d->backdrop[0]) js_texto(corpo, NULL, "background", d->backdrop, sizeof d->backdrop);
  if (!d->logo[0])     js_texto(corpo, NULL, "logo", d->logo, sizeof d->logo);
  // Continuar assistindo SEM arte: ids sem "tt" (ou metahub sem o titulo).
  // Com `tmdb_enrich_continue_watching`, o TMDB completa. Com tt o metahub
  // ja preencheu; este bloco quase nao roda mais no caminho quente.
  if (ajustes_tmdb_cw() && (!d->backdrop[0] || !d->poster[0])) {
    const char *chave = desc_chave_tmdb();
    if (chave[0]) {
      char *c2;
      snprintf(url, sizeof url,
               "https://api.themoviedb.org/3/find/%s?api_key=%s"
               "&external_source=imdb_id", serie, chave);
      c2 = rede_baixar(url, 8);
      if (c2) {
        long idT = 0;
        const char *p = js_array(c2, NULL,
                      !strcmp(tipo, "series") ? "tv_results" : "movie_results");
        if (p) idT = (long)js_num(p, js_fim(p), "id", 0.0);
        free(c2);
        if (idT > 0) {
          snprintf(url, sizeof url,
                   "https://api.themoviedb.org/3/%s/%ld?api_key=%s&language=%s",
                   !strcmp(tipo, "series") ? "tv" : "movie", idT, chave,
                   desc_tmdb_idioma());
          c2 = rede_baixar(url, 8);
          if (c2) {
            char pp[160] = "";
            if (!d->backdrop[0] &&
                js_texto(c2, NULL, "backdrop_path", pp, sizeof pp) &&
                pp[0] == '/')
              // w1280 como no resto do app: w780 esticado para o hero de
              // 1421 (ou 1920 em tela cheia) saia borrado — #54.
              snprintf(d->backdrop, sizeof d->backdrop,
                       "https://image.tmdb.org/t/p/w1280%s", pp);
            pp[0] = 0;
            if (!d->poster[0] &&
                js_texto(c2, NULL, "poster_path", pp, sizeof pp) &&
                pp[0] == '/')
              snprintf(d->poster, sizeof d->poster,
                       "https://image.tmdb.org/t/p/w342%s", pp);
            free(c2);
          }
        }
      }
    }
  }
  if (!d->titulo[0]) js_texto(corpo, NULL, "name", d->titulo, sizeof d->titulo);
  if (!d->sinopse[0]) js_texto(corpo, NULL, "description", d->sinopse, sizeof d->sinopse);
  if (!d->backdrop[0] && strlen(d->poster) < sizeof d->backdrop)
    snprintf(d->backdrop, sizeof d->backdrop, "%s", d->poster);
  if (!d->meta[0] || d->restanteMin <= 0) {
    char r[24] = "", ano[24] = "";
    js_texto(corpo, NULL, "runtime", r, sizeof r);
    // O catalogo do Nuvio nao tem duracao de SERIE, so por episodio ("25min"):
    // a do primeiro episodio faz o papel da "54 min" do Cinemeta.
    if (!r[0]) { const char *v = js_array(corpo, NULL, "videos");
                 if (v) js_texto(v, js_fim(v), "runtime", r, sizeof r); }
    metaprov_duracao(r, sizeof r);
    js_texto(corpo, NULL, "releaseInfo", ano, sizeof ano);
    { char *tr = strstr(ano, "\xe2\x80\x93"); if (tr) *tr = 0; }
    if (ano[0] >= '0' && ano[0] <= '9' && ano[1] && ano[2] && ano[3] && ano[4] == '-') ano[4] = 0;
    if (!d->meta[0])
      snprintf(d->meta, sizeof d->meta, "%.20s%s%.20s", ano,
               (ano[0] && r[0]) ? "  \xc2\xb7  " : "", r);
    // Minutos que faltam: Trakt da a %, Cinemeta a duracao.
    if (d->restanteMin <= 0) {
      if (d->progresso > 0 && d->progresso < 100) {
        int total = atoi(r);
        if (total > 0) d->restanteMin = total - (total * d->progresso) / 100;
      } else if (d->progresso == 0) {
        d->restanteMin = atoi(r);
      }
    }
  }
  if (!d->genero[0])
    snprintf(d->genero, sizeof d->genero, "%s",
             i18n(strcmp(tipo, "series") ? "Filme" : "Programa de TV"));
  // A NOTA VEM DA RAIZ: em serie o meta tem videos[] embaixo, mas imdbRating
  // so existe no objeto de fora (mesmo campo do catalogo). x10, issue #87.
  if (d->nota <= 0) {
    double nota = js_num(corpo, NULL, "imdbRating", 0.0);
    if (nota > 0.0) {
      int n10 = (int)(nota * 10.0 + 0.5);
      if (n10 > 99) n10 /= 10;
      d->nota = n10;
    }
  }
  free(corpo);
  free(corpoCine);
  return d->poster[0] != 0;
}

// PRONTO = arte + sinopse + NOTA (#243). Sem a nota o item que veio do Trakt
// (extended=full traz arte e sinopse) nunca chegava ao Cinemeta, a unica fonte
// do `nota`, e o selo IMDb ficava em branco. A guarda continua: so pula a rede
// de quem ja tem tudo.
static int itemPronto(const CatItem *d) {
  return d->poster[0] && d->backdrop[0] && d->sinopse[0] &&
         (d->nota > 0 || jaTentada(d->imdb));
}

// ENFEITAR EM PARALELO.
//
// Arte vem do metahub (sem GET). O Cinemeta so entra quando falta sinopse/
// runtime/nota ou para validar "a seguir". Ate 8 GETs em paralelo no pior
// caso; quem ja veio do Trakt `extended=full` pula tudo.
//
// Cada `enfeitar` so escreve no seu proprio CatItem e nao toca estado
// compartilhado, entao a paralelizacao e direta. A ordem do historico e
// preservada porque cada fio escreve na posicao que ja era dele.
#define TK_FIOS 3

typedef struct { CatItem *d; char tipo[8]; int ok; } TarefaEnf;
// A FILA DE CADA LOTE E DO LOTE (B2, arranque). Eram globais (enfTarefas/enfN/
// enfProx), o que obrigava quem enfeita a rodar um de cada vez: o feed social
// esperava a fileira "Continuar assistindo" inteira (medido na LG: 7 s, e 2,7 s
// no Android) so porque os dois dividiam esta fila. Com a fila por lote, os dois
// lotes enfeitam ao mesmo tempo; cada fio so escreve no CatItem que pegou.
typedef struct {
  TarefaEnf *t;
  int n, prox;
  pthread_mutex_t trava;
} FilaEnf;

static void *fioEnfeitar(void *u) {
  FilaEnf *f = (FilaEnf *)u;
  for (;;) {
    int meu;
    pthread_mutex_lock(&f->trava);
    if (f->prox >= f->n) { pthread_mutex_unlock(&f->trava); return NULL; }
    meu = f->prox++;
    pthread_mutex_unlock(&f->trava);
    { CatItem *d = f->t[meu].d;
      // Pronto = arte + sinopse. Arte so (metahub) ainda pode querer o
      // Cinemeta para texto; `ok` 1 sobrevive a compactacao.
      if (itemPronto(d)) f->t[meu].ok = 1;
      else f->t[meu].ok = enfeitar(d, f->t[meu].tipo); }
  }
}

// Roda a fila em TK_FIOS fios e espera todos.
static void enfeitarFila(FilaEnf *f) {
  pthread_t fios[TK_FIOS];
  int criados = 0, q;
  pthread_mutex_init(&f->trava, NULL);
  f->prox = 0;
  for (q = 0; q < TK_FIOS; q++)
    if (pthread_create(&fios[criados], NULL, fioEnfeitar, f) == 0) criados++;
  if (!criados) fioEnfeitar(f);      // sem fios: em serie, mesmo resultado
  for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);
  pthread_mutex_destroy(&f->trava);
}

// ENFEITAR os n itens em TK_FIOS fios, e so entao compactar. Compacta quem
// ficou sem poster (id sem tt / sem metahub) ou "a seguir" cujo episodio o
// Cinemeta nao confirma — NAO compacta mais por falha generica do Cinemeta:
// a arte ja veio do metahub.
//
// Publico porque a fileira "Continuar assistindo" montada do progresso LOCAL
// (descoberta.c, sem Trakt) precisa exatamente do mesmo enfeite: tem imdb,
// tipo e porcentagem, e falta arte, sinopse e minutos restantes.
int trakt_enfeitar_lote(CatItem *saida, int n) {
  int jaFeitos = 0, q0;
  if (n <= 0) return 0;
  // QUEM JA TEM TUDO NAO VOLTA A REDE. O item vindo do Trakt com
  // `?extended=full` chega com arte e sinopse (ver doBlocoTrakt); o item vindo
  // do progresso LOCAL chega zerado — metahub cobre a arte sem Cinemeta, e o
  // Cinemeta so e tentado para texto. Guarda por CONTEUDO, nao por origem.
  for (q0 = 0; q0 < n; q0++)
    if (itemPronto(&saida[q0])) jaFeitos++;
  if (jaFeitos) { printf("[trakt] enfeite: %d de %d ja vieram prontos\n", jaFeitos, n);
                  fflush(stdout); }
  if (jaFeitos == n) return n;
  { FilaEnf fila;
    fila.t = calloc((size_t)n, sizeof(TarefaEnf));
    fila.n = n;
    if (fila.t) {
      int q, r, w;
      for (q = 0; q < n; q++) {
        fila.t[q].d = &saida[q];
        snprintf(fila.t[q].tipo, sizeof fila.t[q].tipo, "%s", saida[q].tipo);
      }
      enfeitarFila(&fila);
      for (r = 0, w = 0; r < n; r++)
        if (fila.t[r].ok) { if (w != r) saida[w] = saida[r]; w++; }
      n = w;
      free(fila.t);
    } else {
      // Sem memoria para a fila: em serie, no proprio fio.
      int r, w;
      for (r = 0, w = 0; r < n; r++)
        if (itemPronto(&saida[r]) ||
            enfeitar(&saida[r], saida[r].tipo)) { if (w != r) saida[w] = saida[r]; w++; }
      n = w;
    } }
  return n;
}

// OS IDS DOS REGISTROS DE PLAYBACK da ultima leitura, para poder APAGAR.
// Preenchida em trakt_continuar; consumida por trakt_playback_remover. Cabe a
// mesma quantidade que a fileira mostra com folga.
#define TK_PLAY_MAX 64
static struct { char chave[28]; long long id; } play[TK_PLAY_MAX];   // nPlay: ver trakt_esquecer

// Remove o item da barra de retomada do Trakt. `imdb` e a chave COMPOSTA, do
// mesmo jeito que trakt_continuar a montou ("tt123:2:8" em serie, "tt123" em
// filme) — que e exatamente o que CatItem.imdb carrega num item da fileira.
//
// Devolve 0 quando nao ha o que apagar: Trakt desligado, ou id desconhecido
// porque a fileira veio do progresso da CONTA e nao do Trakt. Nao e erro; o
// chamador tem a sua propria remocao a fazer de qualquer jeito.
int trakt_playback_remover(const char *imdb) {
  const char *cab[4];
  char aut[200], chaveCab[140], url[96];
  char *r;
  int i, st = 0, ok;
  int achou = 0, todos = 1;
  long long id, ids[TK_PLAY_MAX];
  if (!ligado || !imdb || !imdb[0]) return 0;
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chaveCab, sizeof chaveCab, "trakt-api-key: %s", cliente);
  cab[0] = aut;
  cab[1] = "trakt-api-version: 2";
  cab[2] = chaveCab;
  cab[3] = NULL;
  // TODOS OS REGISTROS DA CHAVE (#244). /sync/playback guarda um registro por
  // pausa; apagar so o primeiro deixava os outros devolvendo o item.
  // Os ids saem da tabela SOB playTrava e o DELETE vai sem ela: trakt_continuar
  // pode reescrever a tabela enquanto a rede responde.
  pthread_mutex_lock(&playTrava);
  for (i = 0; i < nPlay; i++)
    if (play[i].id && !strcmp(play[i].chave, imdb)) ids[achou++] = play[i].id;
  pthread_mutex_unlock(&playTrava);
  for (i = 0; i < achou; i++) {
    id = ids[i];
    snprintf(url, sizeof url, "https://api.trakt.tv/sync/playback/%lld", id);
    r = rede_apagar(url, 20, cab, &st);
    ok = st >= 200 && st < 300;
    free(r);
    printf("[trakt] playback remover %s (id %lld) -> %s (HTTP %d)\n",
           imdb, id, ok ? "ok" : "falhou", st);
    fflush(stdout);
    // SO ESQUECE O ID SE O SERVIDOR ACEITOU. Apagar a linha da tabela num 5xx
    // faria a segunda tentativa dizer "sem id" e a pessoa nunca mais conseguiria
    // remover aquele item sem reabrir o app.
    if (ok) {
      // A linha pode ter mudado de lugar (ou sumido) numa releitura: apaga
      // pelo par chave+id, nao pelo indice.
      int k;
      pthread_mutex_lock(&playTrava);
      for (k = 0; k < nPlay; k++)
        if (play[k].id == id && !strcmp(play[k].chave, imdb)) play[k].chave[0] = 0;
      pthread_mutex_unlock(&playTrava);
    } else todos = 0;
  }
  if (!achou) {
    printf("[trakt] playback: sem id para %s (nao veio do Trakt)\n", imdb);
    fflush(stdout);
    return 0;
  }
  return todos;
}

// OCULTA (ou desfaz) A SERIE NO "PROGRESS WATCHED" DO TRAKT (#203). E o que faz o
// "a seguir" nao voltar do Trakt em outro aparelho: /sync/playback so tem o
// pausado; o proximo episodio vem do progresso da serie, que o Trakt deixa
// esconder em /users/hidden/progress_watched. Um POST, sincrono: chamar de fio
// de trabalho. Limite do Trakt para POST: 1/s, e aqui sai um por gesto.
// So obra com IMDb (tt...); id de outro catalogo nao existe no Trakt.
int trakt_progresso_ocultar(const char *imdb, int ocultar) {
  const char *cab[5];
  char aut[200], chaveCab[140], id[24], url[96];
  Jsw w;
  char *r;
  int i, st = 0, ok;
  if (!ligado || !imdb || imdb[0] != 't' || imdb[1] != 't') return 0;
  for (i = 0; imdb[i] && imdb[i] != ':' && i < (int)sizeof id - 1; i++) id[i] = imdb[i];
  id[i] = 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "shows");
  jsw_arr_ini(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "ids");
  jsw_obj_ini(&w);
  jsw_cs(&w, "imdb", id);
  jsw_obj_fim(&w);
  jsw_obj_fim(&w);
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chaveCab, sizeof chaveCab, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chaveCab; cab[3] = NULL;
  snprintf(url, sizeof url, "https://api.trakt.tv/users/hidden/progress_watched%s",
           ocultar ? "" : "/remove");
  r = rede_postar_st(url, 20, cab, jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = st >= 200 && st < 300;
  free(r);
  printf("[trakt] %s %s do progresso -> %s (HTTP %d)\n",
         ocultar ? "ocultar" : "reexibir", id, ok ? "ok" : "falhou", st);
  fflush(stdout);
  return ok;
}

// MARCA OU DESMARCA UM LOTE DE EPISODIOS NO TRAKT.
//
// Um POST, nao um por episodio: /sync/history e /sync/history/remove aceitam
// shows[{ids:{imdb}, seasons:[{number, episodes:[{number}]}]}], e e essa forma
// que torna "ate aqui" e "a temporada inteira" uma requisicao so. Marcar 20
// episodios um a um seriam 20 viagens e 20 chances de terminar pela metade.
//
// SINCRONO, no fio de quem chamou. A tela ja aplicou o efeito local antes de
// chegar aqui (vistoep_marcar_lote), entao o que se espera aqui e a
// confirmacao, nao o desenho. Quem chamar do fio de desenho tem de mandar para
// um fio proprio — hoje o unico chamador vem de episodios.c por app.c.
//
// O lote chega ORDENADO por temporada, mas nao se assume isso: o laco agrupa
// procurando cada temporada uma vez, que para os poucos episodios de um gesto
// custa menos que ordenar.
int trakt_episodios_marcar(const char *imdb, const VistoPar *pares, int qtd,
                           int visto) {
  const char *cab[4];
  char aut[200], chaveCab[140], id[24], url[64];
  Jsw w;
  char *r;
  int i, j, st = 0, ok;
  // 256 e nao 64: o lote da temporada agora vem do catalogo tambem (visto.c),
  // e temporada de anime passa de 64 — com 64 o resto era cortado calado.
  char feita[256];
  if (!ligado || !imdb || imdb[0] != 't' || !pares || qtd < 1) return 0;
  if (qtd > (int)sizeof feita) qtd = (int)sizeof feita;
  for (i = 0; imdb[i] && imdb[i] != ':' && i < (int)sizeof id - 1; i++) id[i] = imdb[i];
  id[i] = 0;
  if (!id[0]) return 0;
  memset(feita, 0, sizeof feita);

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "shows");
  jsw_arr_ini(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "ids");
  jsw_obj_ini(&w);
  jsw_cs(&w, "imdb", id);
  jsw_obj_fim(&w);
  jsw_chave(&w, "seasons");
  jsw_arr_ini(&w);
  for (i = 0; i < qtd; i++) {
    if (feita[i]) continue;
    jsw_obj_ini(&w);
    jsw_ci(&w, "number", pares[i].temporada);
    jsw_chave(&w, "episodes");
    jsw_arr_ini(&w);
    for (j = i; j < qtd; j++) {
      if (feita[j] || pares[j].temporada != pares[i].temporada) continue;
      feita[j] = 1;
      jsw_obj_ini(&w);
      jsw_ci(&w, "number", pares[j].episodio);
      jsw_obj_fim(&w);
    }
    jsw_arr_fim(&w);
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);

  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chaveCab, sizeof chaveCab, "trakt-api-key: %s", cliente);
  cab[0] = aut;
  cab[1] = "trakt-api-version: 2";
  cab[2] = chaveCab;
  cab[3] = NULL;
  snprintf(url, sizeof url, "https://api.trakt.tv/sync/history%s",
           visto ? "" : "/remove");
  r = rede_postar_st(url, 20, cab, jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = st >= 200 && st < 300;
  // O corpo diz quantos o Trakt de fato mexeu (added/deleted/not_found): HTTP
  // 200 com "not_found" era indistinguivel de sucesso (Silo, 08/10).
  printf("[trakt] %s %d episodios de %s -> %s (HTTP %d) %.160s\n",
         visto ? "marcar" : "desmarcar", qtd, id, ok ? "ok" : "falhou", st,
         r ? r : "");
  free(r);
  fflush(stdout);
  return ok;
}

// A barra de retomada vem de /sync/playback e nao informa se o titulo foi
// marcado como assistido. Consultamos o historico real uma vez no mesmo ciclo
// de descoberta para que a modal nao trate progresso alto como prova de visto.
// Para series, registros com `episode` sao deliberadamente ignorados: ter
// visto um episodio nao significa ter marcado a serie inteira como assistida.
static void carregarHistoricoReal(const char *const *cab, HistoricoPedido pedido) {
  char *corpo = rede_baixar_com("https://api.trakt.tv/sync/history?limit=100&extended=full", 25, cab);
  const char *p;
  if (!corpo) return;
  if (!historicoPedidoAtual(pedido)) { free(corpo); return; }
  nUlt = 0;
  p = strchr(corpo, '[');
  p = p ? p + 1 : NULL;
  while (p && *p) {
    const char *f, *obj;
    char id[24] = "";
    const char *tipo = NULL;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    f = js_fim(p);
    { const char *ep = strstr(p, "\"episode\"");
      if (ep && ep < f) {
        // EPISODIO: nao marca a serie como vista (ver acima), mas anota o
        // ultimo visto de cada serie. O historico vem do mais recente para o
        // mais antigo; no EMPATE de instante (serie marcada inteira de uma
        // vez) fica o maior episodio — tk_ult_anotar, issue #213.
        const char *sh = strstr(p, "\"show\"");
        char id[24] = "";
        if (sh && sh < f) js_texto(sh, js_fim(strchr(sh, '{')), "imdb", id, sizeof id);
        if (id[0]) {
          const char *fe = js_fim(strchr(ep, '{'));
          char quando[40] = "";
          js_texto(p, f, "watched_at", quando, sizeof quando);
          tk_ult_anotar(ult, &nUlt, TK_ULT_MAX, id,
                        (int)js_num(ep, fe, "season", 0), (int)js_num(ep, fe, "number", 0),
                        quando[0] ? js_ms_iso(quando) : 0);
        }
        p = js_prox(f);
        continue;
      } }
    obj = strstr(p, "\"movie\"");
    if (obj && obj < f) tipo = "movie";
    else {
      obj = strstr(p, "\"show\"");
      if (obj && obj < f) tipo = "series";
    }
    if (obj && tipo) {
      const char *fo = js_fim(strchr(obj, '{'));
      js_texto(obj, fo, "imdb", id, sizeof id);
      if (id[0]) historicoDefinirPedido(id, tipo, 1, pedido);
    }
    p = js_prox(f);
  }
  free(corpo);
}

// O "A SEGUIR" DO PROPRIO TRAKT (issue #213). O historico so diz o ultimo
// episodio visto; quem sabe se ainda ha o que ver e /shows/<id>/progress/
// watched (next_episode). Um GET por serie candidata, TK_FIOS em paralelo,
// so para quem passou do filtro de playback. Falha de rede: fica o palpite do
// historico, como antes (o enfeitar ainda confere no Cinemeta).
typedef struct { int u, estado, t, e; } TarefaProx;
static TarefaProx *proxTarefas;
static int proxN, proxProx;
static const char *const *proxCab;
static pthread_mutex_t proxTrava = PTHREAD_MUTEX_INITIALIZER;

// MEMORIA DO "A SEGUIR" CONFIRMADO (B2, arranque). Cada volta da descoberta
// (arranque, escolha de perfil, sync que trouxe addons, ciclo de 5 min) refazia
// um GET por serie candidata: 21 deles = ~4 s medidos na LG e no Android, quase
// sempre com a mesma resposta. O "proximo episodio" que o Trakt devolve e funcao
// do que foi visto, e o que foi visto aparece no ultimo episodio do historico;
// entao a chave (serie, temporada e episodio vistos) basta: assistiu mais um,
// a chave muda e o GET sai de novo. Guarda so a resposta POSITIVA (estado 1):
// "sem proximo" muda sozinho quando o episodio novo vai ao ar, e nao e guardado.
#define TK_PROX_MEM 64
static struct { char imdb[24]; int tv, ev, t, e; } proxMem[TK_PROX_MEM];
static int nProxMem, proxMemProx;
static pthread_mutex_t proxMemTrava = PTHREAD_MUTEX_INITIALIZER;

static int proxMemBuscar(const char *imdb, int tv, int ev, int *t, int *e) {
  int i, r = 0;
  pthread_mutex_lock(&proxMemTrava);
  for (i = 0; i < nProxMem && !r; i++)
    if (proxMem[i].tv == tv && proxMem[i].ev == ev && !strcmp(proxMem[i].imdb, imdb)) {
      *t = proxMem[i].t; *e = proxMem[i].e; r = 1;
    }
  pthread_mutex_unlock(&proxMemTrava);
  return r;
}
static void proxMemGuardar(const char *imdb, int tv, int ev, int t, int e) {
  int i;
  pthread_mutex_lock(&proxMemTrava);
  for (i = 0; i < nProxMem; i++)
    if (!strcmp(proxMem[i].imdb, imdb)) break;
  if (i == nProxMem) {
    if (nProxMem < TK_PROX_MEM) i = nProxMem++;
    else { i = proxMemProx; proxMemProx = (proxMemProx + 1) % TK_PROX_MEM; }
  }
  snprintf(proxMem[i].imdb, sizeof proxMem[i].imdb, "%s", imdb);
  proxMem[i].tv = tv; proxMem[i].ev = ev; proxMem[i].t = t; proxMem[i].e = e;
  pthread_mutex_unlock(&proxMemTrava);
}
// Esquecida quando a credencial muda (outra conta Trakt, outro historico).
static void proxMemLimpar(void) {
  pthread_mutex_lock(&proxMemTrava);
  nProxMem = 0; proxMemProx = 0;
  pthread_mutex_unlock(&proxMemTrava);
}

// Os GETs de "a seguir" sao so espera de rede: mais fios que o enfeite.
#define TK_FIOS_PROX 6

static void *fioProximo(void *x) {
  (void)x;
  for (;;) {
    int meu;
    char url[160], *corpo;
    const char *imdb;
    int tv, ev;
    pthread_mutex_lock(&proxTrava);
    if (proxProx >= proxN) { pthread_mutex_unlock(&proxTrava); return NULL; }
    meu = proxProx++;
    pthread_mutex_unlock(&proxTrava);
    imdb = ult[proxTarefas[meu].u].imdb;
    tv = ult[proxTarefas[meu].u].temporada;
    ev = ult[proxTarefas[meu].u].episodio;
    if (proxMemBuscar(imdb, tv, ev, &proxTarefas[meu].t, &proxTarefas[meu].e)) {
      proxTarefas[meu].estado = 1;
      continue;
    }
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/progress/watched?hidden=false&specials=false",
             imdb);
    corpo = rede_baixar_com(url, 8, proxCab);
    proxTarefas[meu].estado = tk_prog_proximo(corpo, &proxTarefas[meu].t, &proxTarefas[meu].e);
    if (proxTarefas[meu].estado == 1)
      proxMemGuardar(imdb, tv, ev, proxTarefas[meu].t, proxTarefas[meu].e);
    free(corpo);
  }
}

static void consultarProximos(TarefaProx *v, int n, const char *const *cab) {
  pthread_t fios[TK_FIOS_PROX];
  int q, criados = 0;
  if (n <= 0) return;
  proxTarefas = v; proxN = n; proxProx = 0; proxCab = cab;
  for (q = 0; q < TK_FIOS_PROX && q < n; q++)
    if (pthread_create(&fios[criados], NULL, fioProximo, NULL) == 0) criados++;
  if (!criados) fioProximo(NULL);
  for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);
  proxTarefas = NULL; proxN = 0;
}

// TODOS OS FILMES VISTOS (#212). carregarHistoricoReal le as ultimas 100
// reproducoes, e quem ve serie enche esse limite de episodios: no log da #212
// ("historico: 12 serie(s) com ultimo episodio visto") nenhum filme entrou, e
// o selo de visto do cartaz e o olho do detalhe nao tinham de onde sair.
// O mapa completo vem de TODAS as paginas de /sync/watched/movies. Desde
// junho/2026, sem page/limit o Trakt so devolve os primeiros 100 filmes.
//
// So baixa de novo quando o Trakt diz que mudou: /sync/last_activities e um
// corpo de ~1 KB, e o ciclo da descoberta roda a cada 5 min. O que a pessoa
// marca nesta TV ja entrou no historico na hora (ctxmenu/app), sem esperar.
//
typedef struct { char (*ids)[24]; size_t n, cap; } FilmesVistos;

static const char *filmesPula(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}

// O leitor compartilhado e tolerante com objeto truncado. Aqui um corpo
// incompleto nao pode confirmar a atividade nem publicar metade do mapa.
static const char *filmesObjetoFim(const char *p) {
  char pilha[64];
  size_t n = 0;
  int texto = 0;
  for (; *p; p++) {
    if (texto) {
      if (*p == '\\') { if (!p[1]) return NULL; p++; }
      else if (*p == '"') texto = 0;
    } else if (*p == '"') texto = 1;
    else if (*p == '{' || *p == '[') {
      if (n == sizeof pilha) return NULL;
      pilha[n++] = *p;
    } else if (*p == '}' || *p == ']') {
      if (!n || pilha[n - 1] != (*p == '}' ? '{' : '[')) return NULL;
      if (--n == 0) return p + 1;
    }
  }
  return NULL;
}

static int filmesAdicionar(FilmesVistos *v, const char *id) {
  if (v->n >= INT_MAX) return 0;
  if (v->n == v->cap) {
    size_t cap = v->cap ? v->cap * 2 : 256;
    void *novo;
    if (cap < v->cap || cap > SIZE_MAX / sizeof *v->ids) return 0;
    novo = realloc(v->ids, cap * sizeof *v->ids);
    if (!novo) return 0;
    v->ids = novo; v->cap = cap;
  }
  snprintf(v->ids[v->n++], sizeof *v->ids, "%s", id);
  return 1;
}

// Devolve o numero de OBJETOS, inclusive filmes sem IMDb: esses nao entram
// no catalogo, mas nao podem ser confundidos com a pagina vazia que encerra.
static int filmesLerPagina(const char *corpo, FilmesVistos *v) {
  const char *p;
  int objetos = 0;
  if (!corpo) return -1;
  p = filmesPula(corpo);
  if (*p != '[') return -1;
  p = filmesPula(p + 1);
  while (*p != ']') {
    const char *f, *obj;
    char id[24] = "";
    if (*p != '{' || objetos == INT_MAX) return -1;
    f = filmesObjetoFim(p);
    if (!f) return -1;
    obj = strstr(p, "\"movie\"");
    if (obj && obj < f) {
      const char *o = filmesPula(obj + 7);
      if (*o == ':') {
        o = filmesPula(o + 1);
        if (*o == '{') js_texto(o, filmesObjetoFim(o), "imdb", id, sizeof id);
      }
    }
    if (id[0] && !filmesAdicionar(v, id)) return -1;
    objetos++;
    p = filmesPula(f);
    if (*p == ']') break;
    if (*p != ',') return -1;
    p = filmesPula(p + 1);
    if (*p != '{') return -1;
  }
  return *filmesPula(p + 1) ? -1 : objetos;
}

static int filmesAplicar(const FilmesVistos *v, HistoricoPedido pedido) {
  int atual;
  pthread_mutex_lock(&travaCred);
  atual = pedido.credencial == credGeracao && pedido.mapa == cat_historico_geracao();
  for (size_t i = 0; atual && i < v->n; i++)
    atual = cat_historico_definir_se_geracao(v->ids[i], "movie", 1, pedido.mapa);
  pthread_mutex_unlock(&travaCred);
  if (atual && v->n) {
    printf("[visto] marcado por trakt-importacao pos=0 dur=0 assistido=0s n=%zu filmes (sem reproducao local)\n", v->n);
    fflush(stdout);
  }
  return atual;
}

int trakt_ler_filmes_vistos(const char *corpo) {
  FilmesVistos v = {0};
  HistoricoPedido pedido = historicoPedido();
  int n = filmesLerPagina(corpo, &v);
  if (n >= 0) { if (filmesAplicar(&v, pedido)) n = (int)v.n; else n = -1; }
  free(v.ids);
  return n;
}

static void carregarFilmesVistos(const char *const *cab, HistoricoPedido pedido) {
  FilmesVistos v = {0};
  char ativ[40] = "";
  char url[128], *anterior = NULL;
  int pagina = 1, ok = 0;
  char *corpo = rede_baixar_com("https://api.trakt.tv/sync/last_activities", 15, cab);
  if (corpo) {
    const char *m = strstr(corpo, "\"movies\"");
    const char *o = m ? strchr(m, '{') : NULL;
    if (o) js_texto(o, js_fim(o), "watched_at", ativ, sizeof ativ);
    free(corpo);
  }
  pthread_mutex_lock(&travaCred);
  int repetida = pedido.credencial == credGeracao && pedido.mapa == filmesAtivMapa &&
    ativ[0] && !strcmp(ativ, filmesAtiv);
  pthread_mutex_unlock(&travaCred);
  if (!historicoPedidoAtual(pedido) || repetida) return;
  for (;;) {
    int st = 0, n;
    snprintf(url, sizeof url, "https://api.trakt.tv/sync/watched/movies?page=%d&limit=250", pagina);
    corpo = rede_baixar_st(url, 25, cab, &st);
    if (!corpo || st != 200 || (anterior && !strcmp(anterior, corpo))) { free(corpo); break; }
    n = filmesLerPagina(corpo, &v);
    free(anterior); anterior = corpo;
    if (n < 0) break;
    // O Trakt pode capar a pagina abaixo do limit solicitado. So [] encerra;
    // uma pagina de 37 objetos ainda pode ter outra depois dela.
    if (!n) { ok = 1; break; }
    if (pagina == INT_MAX || !historicoPedidoAtual(pedido)) break;
    pagina++;
  }
  free(anterior);
  if (ok && filmesAplicar(&v, pedido)) {
    pthread_mutex_lock(&travaCred);
    if (pedido.credencial == credGeracao && pedido.mapa == cat_historico_geracao() && ativ[0]) {
      snprintf(filmesAtiv, sizeof filmesAtiv, "%s", ativ);
      filmesAtivMapa = pedido.mapa;
    }
    pthread_mutex_unlock(&travaCred);
    printf("[trakt] filmes vistos: %d (%d pagina(s))\n", (int)v.n, pagina);
  } else printf("[trakt] filmes vistos: falhou na pagina %d; mapa anterior mantido\n", pagina);
  fflush(stdout);
  free(v.ids);
}

static volatile int continuarFalhou;
int trakt_continuar_falhou(void) { return continuarFalhou; }

// UMA OBRA, UM CARD (#244). /sync/playback traz um registro por pausa: o mesmo
// episodio pausado em cinco aparelhos eram cinco CatItem iguais. Fica o de
// `retomadoMs` mais novo por obra (a mesma regra de continuarLocal), na posicao
// do primeiro. Os ids dos registros descartados passam para a chave de quem
// ficou, para trakt_playback_remover apagar TODOS de uma vez.
static size_t obraLen(const char *imdb) {
  const char *c = strchr(imdb, ':');
  return c ? (size_t)(c - imdb) : strlen(imdb);
}
static int dedupObras(CatItem *v, int n) {
  int i, j, w = 0;
  for (i = 0; i < n; i++) {
    size_t L = obraLen(v[i].imdb);
    int achou = -1;
    for (j = 0; j < w && achou < 0; j++)
      if (obraLen(v[j].imdb) == L && !strncmp(v[j].imdb, v[i].imdb, L)) achou = j;
    if (achou < 0) { if (w != i) v[w] = v[i]; w++; continue; }
    { char velho[sizeof v[0].imdb];
      int k;
      if (v[i].retomadoMs > v[achou].retomadoMs) {
        snprintf(velho, sizeof velho, "%s", v[achou].imdb);
        v[achou] = v[i];
      } else snprintf(velho, sizeof velho, "%s", v[i].imdb);
      pthread_mutex_lock(&playTrava);
      for (k = 0; k < nPlay; k++)
        if (!strcmp(play[k].chave, velho))
          snprintf(play[k].chave, sizeof play[k].chave, "%s", v[achou].imdb);
      pthread_mutex_unlock(&playTrava);
      printf("[trakt] playback repetido de %.*s; fica o mais recente (%s)\n",
             (int)L, v[achou].imdb, v[achou].imdb); }
  }
  return w;
}

// O HISTORICO E OS FILMES VISTOS NAO DEPENDEM DO PLAYBACK (B2, arranque). Os tres
// GETs saiam um depois do outro (playback, historico, last_activities + paginas
// de filmes) e, na LG e no Android, eram ~4 s dos 9-14 s ate a fileira "Continuar
// assistindo". Agora saem juntos; cada um escreve em estado proprio (play[]/nPlay
// so o playback, ult[]/nUlt so o historico, o mapa de vistos sob travaCred).
typedef struct { const char *const *cab; HistoricoPedido pedido; } PedidoHist;

static void *fioHistorico(void *u) {
  PedidoHist *h = (PedidoHist *)u;
  carregarHistoricoReal(h->cab, h->pedido);
  return NULL;
}
static void *fioFilmesVistos(void *u) {
  PedidoHist *h = (PedidoHist *)u;
  carregarFilmesVistos(h->cab, h->pedido);
  return NULL;
}

int trakt_continuar(CatItem *saida, int max) {
  const char *cab[4];
  char aut[200], chave[140];
  char *corpo;
  const char *p;
  int n = 0;
  continuarFalhou = 0;
  HistoricoPedido pedido = historicoPedido();
  if (!cabecalhosPedido(pedido, cab, aut, sizeof aut, chave, sizeof chave)) return 0;
  pthread_mutex_lock(&playTrava); nPlay = 0; pthread_mutex_unlock(&playTrava);
  // Historico e filmes vistos em fios proprios enquanto o playback baixa. Sem
  // fio (pthread_create falhou, ou WebAssembly sem threads): ficam em serie,
  // depois do playback, como antes.
  PedidoHist ph = { cab, pedido };
  pthread_t fioHist, fioFilmes;
  int hist = pthread_create(&fioHist, NULL, fioHistorico, &ph) == 0;
  int filmes = pthread_create(&fioFilmes, NULL, fioFilmesVistos, &ph) == 0;
  corpo = rede_baixar_com("https://api.trakt.tv/sync/playback?extended=full", 25, cab);
  if (!corpo) {
    if (hist) pthread_join(fioHist, NULL);
    if (filmes) pthread_join(fioFilmes, NULL);
    continuarFalhou = 1; printf("[trakt] sem resposta\n"); return 0;
  }
  // A prova de conclusao precisa estar pronta antes de consumir vagas de playback.
  if (hist) pthread_join(fioHist, NULL); else carregarHistoricoReal(cab, pedido);
  // O corpo e um array na raiz; js_array procura por chave, entao anda-se a mao.
  p = strchr(corpo, '[');
  p = p ? p + 1 : NULL;
  while (p && *p && n < max) {
    const char *f;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    f = js_fim(p);
    {
      CatItem *d = &saida[n];
      const char *ep = strstr(p, "\"episode\"");
      int serie = ep && ep < f;
      char imdb[24] = "";
      memset(d, 0, sizeof *d);
      d->progresso = (int)js_num(p, f, "progress", 0.0);
      if (d->progresso < ajustes_cw_minimo() || d->progresso >= ajustes_cw_concluido()) {
        p = js_prox(f); continue;
      }
      // QUANDO foi pausado. E o unico dado desta resposta que permite comparar
      // um item do Trakt com um do progresso da conta Nuvio: sem ele a fileira
      // "Continuar assistindo" tinha de chutar a ordem entre as duas fontes.
      // Ver retomadoMs em catalogo.h e montarContinuar em descoberta.c.
      { char quando[40];
        if (js_texto(p, f, "paused_at", quando, sizeof quando))
          d->retomadoMs = js_ms_iso(quando); }
      // O bloco "movie"/"show" tem o titulo e os ids; o "episode" traz
      // temporada e numero. Procurar "imdb" na faixa inteira pegaria o do
      // episodio, que os addons tambem aceitam mas nao identifica a obra.
      { const char *bloco = strstr(p, serie ? "\"show\"" : "\"movie\"");
        if (bloco && bloco < f) {
          const char *fb = js_fim(strchr(bloco, '{'));
          js_texto(bloco, fb, "title", d->titulo, sizeof d->titulo);
          js_texto(bloco, fb, "imdb", imdb, sizeof imdb);
          // ARTE, SINOPSE E DURACAO JA VEM AQUI (extended=full). Sem isto cada
          // item custava um GET ao Cinemeta — ate 15 deles antes de a primeira
          // fileira da home existir.
          doBlocoTrakt(d, bloco, fb, serie ? "series" : "movie");
        } }
      if (!imdb[0]) { p = js_prox(f); continue; }
      if (serie) {
        const char *fe = js_fim(strchr(ep, '{'));
        d->temporada = (int)js_num(ep, fe, "season", 0);
        d->episodio  = (int)js_num(ep, fe, "number", 0);
        js_texto(ep, fe, "title", d->nomeEpisodio, sizeof d->nomeEpisodio);
        snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", imdb,
                 d->temporada ? d->temporada : 1, d->episodio ? d->episodio : 1);
        snprintf(d->tipo, sizeof d->tipo, "series");
      } else {
        snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
        snprintf(d->tipo, sizeof d->tipo, "movie");
      }
      // So o historico posterior do MESMO episodio confirma avancar um pausado.
      if (trakt_cw_concluiu(d->imdb, d->retomadoMs)) { p = js_prox(f); continue; }
      // O ID DO REGISTRO DE PLAYBACK, guardado aqui e em lugar nenhum mais.
      //
      // O Trakt remove um item da barra de retomada por DELETE
      // /sync/playback/<id>, e esse <id> e do REGISTRO — nao do titulo, nao do
      // IMDb. Sem guarda-lo na leitura nao ha como apaga-lo depois, e era essa
      // a metade que faltava do issue #22: "seleciono remover, o prompt some e
      // nada e removido". O local era apagado; o do Trakt voltava no ciclo
      // seguinte.
      //
      // TABELA LATERAL, e nao um campo em CatItem: sizeof(CatItem) faz parte do
      // cabecalho do cache em disco (catalogo.c), e crescer a struct invalida
      // todo cache de Home ja gravado.
      //
      // O "id" do topo do objeto e o do registro; os blocos aninhados trazem
      // "ids" (plural), que nao casa com a busca por "id".
      { double pid = js_num(p, f, "id", 0.0);
        pthread_mutex_lock(&playTrava);
        if (nPlay < TK_PLAY_MAX && pid > 0.0) {
          snprintf(play[nPlay].chave, sizeof play[nPlay].chave, "%s", d->imdb);
          play[nPlay].id = (long long)pid;
          nPlay++;
        }
        pthread_mutex_unlock(&playTrava);
      }
      // Enfeitar fica para DEPOIS do laco, em paralelo. Aqui o item ja esta
      // montado: so falta a arte e a sinopse, que vem da rede.
      n++;
      n = dedupObras(saida, n);
    }
    p = js_prox(f);
  }
  free(corpo);
  // Os filmes vistos seguem rodando durante os GETs de "a seguir" e do enfeite
  // abaixo; so sao esperados no fim, antes de a fileira sair.
  if (!filmes) carregarFilmesVistos(cab, pedido);
  printf("[trakt] historico: %d serie(s) com ultimo episodio visto\n", nUlt);
  // "A SEGUIR": serie cujo ultimo episodio visto terminou e que nao esta
  // pausada em nada. Entra com progresso 0 no episodio seguinte; enfeitar()
  // confere no Cinemeta que ele existe (ou salta para a temporada seguinte) e
  // descarta o que acabou. Ver ult[].
  nProxIds = 0;
  { int u, nTar = 0, acabou = 0;
    static TarefaProx tar[TK_ULT_MAX];
    static int tarDe[TK_ULT_MAX];   // ult[u] -> indice em tar[], ou -1
    for (u = 0; ajustes_cw_proximo() && u < nUlt; u++) {
      int k, ja = 0;
      size_t L = strlen(ult[u].imdb);
      tarDe[u] = -1;
      if (ult[u].temporada <= 0 || ult[u].episodio <= 0) continue;
      for (k = 0; k < n; k++)
        if (!strncmp(saida[k].imdb, ult[u].imdb, L) &&
            (saida[k].imdb[L] == 0 || saida[k].imdb[L] == ':')) { ja = 1; break; }
      if (ja || strncmp(ult[u].imdb, "tt", 2)) continue;
      tar[nTar].u = u; tar[nTar].estado = -1; tar[nTar].t = tar[nTar].e = 0;
      tarDe[u] = nTar++;
    }
    consultarProximos(tar, nTar, cab);
    for (u = 0; ajustes_cw_proximo() && u < nUlt; u++) {
      int k, ja = 0, alvo, proxT = ult[u].temporada, proxE = ult[u].episodio + 1;
      size_t L = strlen(ult[u].imdb);
      if (ult[u].temporada <= 0 || ult[u].episodio <= 0) continue;
      for (k = 0; k < n; k++)
        if (!strncmp(saida[k].imdb, ult[u].imdb, L) &&
            (saida[k].imdb[L] == 0 || saida[k].imdb[L] == ':')) { ja = 1; break; }
      if (ja) continue;
      if (tarDe[u] >= 0) {
        TarefaProx *r = &tar[tarDe[u]];
        if (r->estado == 0) {
          // O Trakt diz que nao ha proximo: a serie acabou (ou tudo o que foi
          // ao ar ja foi visto). Nao e "continuar".
          printf("[trakt] a seguir: %s sem proximo no Trakt (ultimo visto T%dE%d); fora\n",
                 ult[u].imdb, ult[u].temporada, ult[u].episodio);
          acabou++;
          continue;
        }
        if (r->estado == 1) { proxT = r->t; proxE = r->e; }
      }
      // Pausados validos nunca cedem lugar a sugestoes de outras obras.
      if (n < max) alvo = n++;
      else {
        int vel = -1;
        for (k = 0; k < n; k++)
          if (saida[k].progresso == 0 &&
              (vel < 0 || saida[k].retomadoMs < saida[vel].retomadoMs)) vel = k;
        if (vel < 0 || saida[vel].retomadoMs >= ult[u].quandoMs) continue;
        alvo = vel;
      }
      { CatItem *d = &saida[alvo];
        memset(d, 0, sizeof *d);
        d->temporada = proxT;
        d->episodio = proxE;
        snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", ult[u].imdb, d->temporada, d->episodio);
        snprintf(d->tipo, sizeof d->tipo, "series");
        d->progresso = 0;
        d->retomadoMs = ult[u].quandoMs;
        if (nProxIds < TK_ULT_MAX) snprintf(proxIds[nProxIds++], sizeof proxIds[0], "%s", d->imdb);
        printf("[trakt] a seguir: %s (ultimo visto T%dE%d, %lld)\n", d->imdb, ult[u].temporada, ult[u].episodio, ult[u].quandoMs);
      }
    }
    if (nTar)
      printf("[trakt] a seguir conferido no Trakt: %d serie(s), %d sem proximo\n", nTar, acabou); }
  // Pausados primeiro; recencia dentro de cada grupo. Insercao: n <= CONT_MAX.
  { int a, b;
    for (a = 1; a < n; a++) {
      CatItem t = saida[a];
      for (b = a - 1; b >= 0 &&
           ((saida[b].progresso == 0 && t.progresso > 0) ||
            ((saida[b].progresso == 0) == (t.progresso == 0) &&
             saida[b].retomadoMs < t.retomadoMs)); b--) saida[b + 1] = saida[b];
      saida[b + 1] = t;
    } }
  // O MAPA DE EPISODIOS VISTOS NAO SAI DAQUI. Ele e buscado por SERIE, em
  // extras.c, quando a lista de episodios daquele titulo abre — e de la
  // alimenta vistoep.h. Uma tentativa anterior usou /sync/watched/shows, que a
  // documentacao descreve como o mapa completo por temporada; MEDIDO na TV com
  // 75 series, ela devolveu 90675 bytes com ZERO ocorrencias de "seasons" e
  // "number", mesmo com ?extended=full. Nao retentar sem medicao nova.

  n = trakt_enfeitar_lote(saida, n);
  if (filmes) pthread_join(fioFilmes, NULL);

  printf("[trakt] %d em andamento\n", n);
  fflush(stdout);
  return n;
}

// Alguns clientes recebem 401 apenas no feed agregado. O grafo e o
// historico publico dos perfis continuam acessiveis com a mesma credencial.
static char *socialPorSeguidos(const char *const *cab, int max) {
  char *lista=rede_baixar_com("https://api.trakt.tv/users/me/following?extended=full",10,cab);
  if(!lista)return NULL;
  char *out=calloc(1,262144);size_t used=1;int n=0,consultados=0;
  if(!out){free(lista);return NULL;}out[0]='[';
  const char *p=strchr(lista,'[');p=p?p+1:NULL;
  while(p&&*p&&n<max&&consultados<8) {
    while(*p&&(unsigned char)*p<=' ')p++;
    if(*p!='{')break;
    const char *f=js_fim(p),*u=strstr(p,"\"user\"");
    if(!u||u>=f){p=js_prox(f);continue;}
    u=strchr(u,'{');const char *uf=js_fim(u);char id[128]="",url[400];
    js_texto(u,uf,"slug",id,sizeof id);
    if(!id[0] || strspn(id,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=strlen(id)){p=js_prox(f);continue;}
    consultados++;
    snprintf(url,sizeof url,"https://api.trakt.tv/users/%s/watching?extended=full",id);
    char *body=rede_baixar_com(url,8,cab);int agora=body&&strchr(body,'{');
    if(!agora){free(body);snprintf(url,sizeof url,"https://api.trakt.tv/users/%s/history?limit=1&extended=full",id);body=rede_baixar_com(url,8,cab);}
    const char *b=body?strchr(body,'{'):NULL,*bf=b?js_fim(b):NULL;
    if(b&&bf&&bf>b+1) {
      size_t un=(size_t)(uf-u),bn=(size_t)(bf-b-2);
      if(used+un+bn+80<262144){
        int k=snprintf(out+used,262144-used,"%s{\"user\":%.*s,%s%.*s}",n?",":"",(int)un,u,
            agora?"\"action\":\"watching\",":"",(int)bn,b+1);
        used+=(size_t)k;n++;
      }
    }
    free(body);p=js_prox(f);
  }
  out[used++]=']';out[used]=0;free(lista);
  printf("[trakt] social: %d seguidos consultados, %d atividades\n",consultados,n);
  return out;
}

// Zerada por trakt_definir: credencial nova pode ter o escopo que faltava.
static volatile int socialSemFeed;
void trakt_social_reavaliar(void) { socialSemFeed = 0; }

int trakt_social(CatItem *saida, int max) {
  const char *cab[4];
  char aut[200], chave[140], *corpo;
  const char *p;
  int n = 0;
  // O FEED AGREGADO NAO EXISTE PARA ESTA CONTA, e isso nao muda no meio da
  // sessao. MEDIDO na LG: os dois /activities respondem 401 sempre, e o app
  // pagava os dois em TODO ciclo da home antes de cair no fallback. Um token
  // novo (trakt_definir) zera a marca, porque ai a resposta pode mudar.
  if (!ligado || max < 1) return 0;
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) return 0;
  corpo = NULL;
  if (!socialSemFeed) {
    corpo = rede_baixar_com(
      "https://api.trakt.tv/users/me/friends/activities?extended=full&page=1&limit=12",
      20, cab);
    // Contas sem o escopo social novo podem receber 401 no grafo `friends`,
    // embora o token continue valido para historico. `following` e o fallback
    // honesto: ainda sao pessoas escolhidas pelo dono, nunca atividade global.
    if (!corpo) corpo = rede_baixar_com(
      "https://api.trakt.tv/users/me/following/activities?extended=full&page=1&limit=12",
      20, cab);
    if (!corpo) {
      socialSemFeed = 1;
      printf("[trakt] feed agregado indisponivel nesta conta; usando os seguidos\n");
      fflush(stdout);
    }
  }
  if (!corpo) corpo=socialPorSeguidos(cab,max);
  if (!corpo) { printf("[trakt] feed social indisponivel\n"); return 0; }
  p = strchr(corpo, '['); p = p ? p + 1 : NULL;
  while (p && *p && n < max) {
    const char *f, *bu, *bm, *bs, *be, *fb;
    CatItem *d;
    char imdb[24] = "", pessoa[64] = "", acao[32] = "";
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    f = js_fim(p);
    bu = strstr(p, "\"user\"");
    bm = strstr(p, "\"movie\"");
    bs = strstr(p, "\"show\"");
    be = strstr(p, "\"episode\"");
    if (!bu || bu >= f || ((!bm || bm >= f) && (!bs || bs >= f))) {
      p = js_prox(f); continue;
    }
    d = &saida[n]; memset(d, 0, sizeof *d);
    fb = js_fim(strchr(bu, '{'));
    js_texto(bu, fb, "name", pessoa, sizeof pessoa);
    if (!pessoa[0]) js_texto(bu, fb, "username", pessoa, sizeof pessoa);
    js_texto(bu, fb, "slug", d->socialSlug, sizeof d->socialSlug);
    const char *avatar = strstr(bu, "\"avatar\"");
    if (avatar && avatar < fb) js_texto(avatar, fb, "full", d->socialAvatar, sizeof d->socialAvatar);
    snprintf(d->socialNome, sizeof d->socialNome, "%s", pessoa[0] ? pessoa : "Amigo");
    js_texto(p, f, "action", acao, sizeof acao);
    snprintf(d->pais, sizeof d->pais, "%s", pessoa[0] ? pessoa : "Amigo");
    snprintf(d->provNome, sizeof d->provNome, "%s",
             !strcmp(acao,"watching") ? "assistindo agora" :
             !strcmp(acao, "watch") || !strcmp(acao, "scrobble") ? "assistiu" :
             !strcmp(acao, "checkin") ? "registrou um check-in" :
             !strcmp(acao, "rating") ? "avaliou" : "atividade recente");
    snprintf(d->socialAcao, sizeof d->socialAcao, "%s", d->provNome);
    if (bs && bs < f) {
      fb = js_fim(strchr(bs, '{'));
      js_texto(bs, fb, "title", d->titulo, sizeof d->titulo);
      js_texto(bs, fb, "imdb", imdb, sizeof imdb);
      snprintf(d->tipo, sizeof d->tipo, "series");
      doBlocoTrakt(d, bs, fb, "series");
      if (be && be < f) {
        const char *fe = js_fim(strchr(be, '{'));
        d->temporada = (int)js_num(be, fe, "season", 0);
        d->episodio = (int)js_num(be, fe, "number", 0);
        js_texto(be, fe, "title", d->nomeEpisodio, sizeof d->nomeEpisodio);
        // i18n aqui, num fio de trabalho: e seguro. A tabela e const e
        // idioma_registrar sai na primeira linha quando NUVIO_TEXTO_DUMP nao
        // esta ligado. O rotulo fica GUARDADO no item, entao trocar o idioma
        // exige remontar — e ajustes.c ja chama desc_repetir() nessa troca.
        snprintf(d->direcao, sizeof d->direcao, i18n("T%dE%d%s%s"), d->temporada,
                 d->episodio, d->nomeEpisodio[0] ? "  \xc2\xb7  " : "",
                 d->nomeEpisodio);
      }
    } else {
      fb = js_fim(strchr(bm, '{'));
      js_texto(bm, fb, "title", d->titulo, sizeof d->titulo);
      js_texto(bm, fb, "imdb", imdb, sizeof imdb);
      snprintf(d->tipo, sizeof d->tipo, "movie");
      doBlocoTrakt(d, bm, fb, "movie");
      snprintf(d->direcao, sizeof d->direcao, "Filme");
    }
    if (!imdb[0]) { p = js_prox(f); continue; }
    snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
    n++;
    p = js_prox(f);
  }
  free(corpo);
  // O feed ja vem ordenado do mais recente. A arte e resolvida em paralelo,
  // com o mesmo limite de tres conexoes usado pelo Continue Assistindo.
  if (n > 0) {
    FilaEnf fila;
    fila.t = calloc((size_t)n, sizeof *fila.t);
    fila.n = n;
    if (fila.t) {
      int q;
      for (q = 0; q < n; q++) {
        fila.t[q].d = &saida[q];
        snprintf(fila.t[q].tipo, sizeof fila.t[q].tipo, "%s", saida[q].tipo);
      }
      enfeitarFila(&fila);
      // Arte indisponivel nao pode apagar uma pessoa real do feed.
      free(fila.t);
    }
  }
  printf("[trakt] %d atividades de amigos\n", n); fflush(stdout);
  return n;
}

static void perfilGenero(PerfilDados *d, const char *nome) {
  int i;
  if (!nome || !*nome) return;
  static const char *en[]={"drama","science-fiction","comedy","crime","thriller","action","mystery","history","fantasy","horror","adventure","romance","documentary","animation"};
  static const char *pt[]={"Drama","Ficção científica","Comédia","Crime","Suspense","Ação","Mistério","História","Fantasia","Terror","Aventura","Romance","Documentário","Animação"};
  for (unsigned k=0;k<sizeof en/sizeof *en;k++) if(!strcmp(nome,en[k])) {nome=pt[k];break;}
  for (i=0;i<d->nGeneros;i++) if (!strcmp(d->generos[i].nome,nome)) {
    d->generos[i].quantidade++; return;
  }
  if (d->nGeneros < PERFIL_MAX_GENEROS) {
    PerfilGenero *g=&d->generos[d->nGeneros++];
    snprintf(g->nome,sizeof g->nome,"%s",nome); g->quantidade=1;
  }
}

static void perfilGenerosJson(PerfilDados *d, const char *b, const char *f) {
  const char *g = strstr(b,"\"genres\"");
  if (!g || g >= f || !(g=strchr(g,'[')) || g>=f) return;
  g++;
  while (g < f) {
    char nome[40]; size_t n=0;
    while (g<f && *g!='\"' && *g!=']') g++;
    if (g>=f || *g==']') break;
    g++;
    while (g<f && *g!='\"' && n+1<sizeof nome) nome[n++]=*g++;
    nome[n]=0; perfilGenero(d,nome);
    if (g<f) g++;
  }
}

int trakt_perfil(PerfilDados *d) {
  const char *cab[4]; char aut[200],chave[140],url[360],*corpo;
  time_t agora=time(NULL); struct tm tmv=*localtime(&agora);
  char inicio[48]; int diasNoMes;
  PerfilDestaque *ranking;
  int nRanking = 0;
  if (!d || !ligado) return 0;
  memset(d,0,sizeof *d);
  if (!trakt_cabecalhos(cab,aut,sizeof aut,chave,sizeof chave)) return 0;
  static const char *meses[]={"Janeiro","Fevereiro","Março","Abril","Maio","Junho","Julho","Agosto","Setembro","Outubro","Novembro","Dezembro"};
  // O MES PASSA PELA TABELA AQUI, na composicao: "Setembro 2026" e uma string
  // montada, e a traducao de text.c casa a string inteira — ela nunca acharia
  // chave para isso. A foto 06-profile.png mostrava "Setembro 2026" no meio de
  // uma tela em ingles.
  snprintf(d->periodo,sizeof d->periodo,"%s %d",i18n(meses[tmv.tm_mon]),tmv.tm_year+1900);
  struct tm primeiro=tmv;
  primeiro.tm_mday=1;primeiro.tm_hour=primeiro.tm_min=primeiro.tm_sec=0;primeiro.tm_isdst=-1;
  time_t limite=mktime(&primeiro);struct tm utc;
  gmtime_r(&limite,&utc);
  strftime(inicio,sizeof inicio,"%Y-%m-%dT%H%%3A%M%%3A%SZ",&utc);
  // Perfil e avatar. O avatar pode ser WebP no Trakt novo; o renderer so o
  // pede se o firmware aceitar, e a tela continua completa sem ele.
  corpo=rede_baixar_com("https://api.trakt.tv/users/settings?extended=full",15,cab);
  if(corpo){ const char *u=strstr(corpo,"\"user\""); const char *fu=u?js_fim(strchr(u,'{')):NULL;
    if(u&&fu){js_texto(u,fu,"name",d->nome,sizeof d->nome);js_texto(u,fu,"username",d->usuario,sizeof d->usuario);
      js_texto(u,fu,"full",d->avatar,sizeof d->avatar);} free(corpo); }
  snprintf(url,sizeof url,
    "https://api.trakt.tv/users/me/history?start_at=%s&extended=full&page=1&limit=100",inicio);
  corpo=rede_baixar_com(url,25,cab);
  if (!corpo || !strchr(corpo, '[')) { free(corpo); return 0; }
  ranking = calloc(100, sizeof *ranking);
  if (!ranking) { free(corpo); return 0; }
  d->parcial = 1;
  snprintf(d->aviso, sizeof d->aviso,
           "Recorte das 100 reproducoes mais recentes do mes. Duracoes informadas pelo Trakt.");
  { const char *p=strchr(corpo,'['); p=p?p+1:NULL;
    while(p&&*p){
      const char *f,*bm,*bs,*be,*obj,*fo; char watched[32]="",imdb[24]="",titulo[128]="";
      int runtime=0,t=0,e=0,hi=-1;
      while(*p&&(unsigned char)*p<=' ')p++; if(*p!='{')break; f=js_fim(p);
      js_texto(p,f,"watched_at",watched,sizeof watched);
      bm=strstr(p,"\"movie\""); bs=strstr(p,"\"show\""); be=strstr(p,"\"episode\"");
      obj=(bs&&bs<f)?bs:((bm&&bm<f)?bm:NULL); if(!obj){p=js_prox(f);continue;}
      fo=js_fim(strchr(obj,'{')); js_texto(obj,fo,"title",titulo,sizeof titulo); js_texto(obj,fo,"imdb",imdb,sizeof imdb);
      runtime=(int)js_num(obj,fo,"runtime",0); perfilGenerosJson(d,obj,fo);
      if(be&&be<f){const char *fe=js_fim(strchr(be,'{'));t=(int)js_num(be,fe,"season",0);e=(int)js_num(be,fe,"number",0);
        {int re=(int)js_num(be,fe,"runtime",0);if(re>0)runtime=re;} d->episodios++;}
      else d->filmes++;
      d->plays++; if (runtime > 0) d->minutos += runtime;
      { int y,m,day,h,mi,s;
        if(sscanf(watched,"%d-%d-%dT%d:%d:%d",&y,&m,&day,&h,&mi,&s)==6){
          struct tm wt={0},local;wt.tm_year=y-1900;wt.tm_mon=m-1;wt.tm_mday=day;
          wt.tm_hour=h;wt.tm_min=mi;wt.tm_sec=s;time_t stamp=timegm(&wt);
          localtime_r(&stamp,&local);
          if(local.tm_year==tmv.tm_year&&local.tm_mon==tmv.tm_mon&&local.tm_mday>=1&&local.tm_mday<=31)
            d->atividade[local.tm_mday-1]++;
        }
      }
      for(int i=0;i<nRanking;i++)if(imdb[0]&&!strcmp(ranking[i].id,imdb)){hi=i;break;}
      if(hi<0&&imdb[0]&&nRanking<100){hi=nRanking++;PerfilDestaque *h=&ranking[hi];
        snprintf(h->id,sizeof h->id,"%s",imdb);snprintf(h->titulo,sizeof h->titulo,"%s",titulo);
        if(t>0&&e>0)snprintf(h->detalhe,sizeof h->detalhe,i18n("T%dE%d"),t,e);else snprintf(h->detalhe,sizeof h->detalhe,"Filme");
        if(imdb[0]){snprintf(h->poster,sizeof h->poster,"https://images.metahub.space/poster/medium/%s/img",imdb);
          snprintf(h->backdrop,sizeof h->backdrop,"https://images.metahub.space/background/medium/%s/img",imdb);}}
      if(hi>=0){ranking[hi].plays++;if(runtime>0)ranking[hi].minutos+=runtime;}
      p=js_prox(f);
    }
  }
  free(corpo);
  for(int i=0;i<nRanking;i++)for(int j=i+1;j<nRanking;j++)
    if(ranking[j].plays>ranking[i].plays){PerfilDestaque x=ranking[i];ranking[i]=ranking[j];ranking[j]=x;}
  d->nDestaques=nRanking<PERFIL_MAX_DESTAQUES?nRanking:PERFIL_MAX_DESTAQUES;
  memcpy(d->destaques,ranking,d->nDestaques*sizeof *ranking);
  free(ranking);
  diasNoMes=31; if(tmv.tm_mon==1) diasNoMes=((tmv.tm_year+1900)%4==0)?29:28;
  else if(tmv.tm_mon==3||tmv.tm_mon==5||tmv.tm_mon==8||tmv.tm_mon==10)diasNoMes=30;
  d->nDias=diasNoMes;
  {struct tm primeiro=tmv;primeiro.tm_mday=1;mktime(&primeiro);d->primeiroDiaSemana=primeiro.tm_wday;}
  for(int i=0;i<d->nDias;i++)if(d->atividade[i])d->diasAtivosMes++;
  // Um recorte mensal nao comprova a atividade anual.
  d->diasAtivosAno=0;
  for(int i=tmv.tm_mday-1;i>=0&&i<d->nDias;i--){if(!d->atividade[i])break;d->streakAtual++;}
  // Ordena destaques e generos por volume para a leitura visual ser honesta.
  for(int i=0;i<d->nDestaques;i++)for(int j=i+1;j<d->nDestaques;j++)if(d->destaques[j].plays>d->destaques[i].plays){PerfilDestaque x=d->destaques[i];d->destaques[i]=d->destaques[j];d->destaques[j]=x;}
  for(int i=0;i<d->nGeneros;i++)for(int j=i+1;j<d->nGeneros;j++)if(d->generos[j].quantidade>d->generos[i].quantidade){PerfilGenero x=d->generos[i];d->generos[i]=d->generos[j];d->generos[j]=x;}
  printf("[trakt] perfil: %d plays, %d min, %d destaques\n",d->plays,d->minutos,d->nDestaques);fflush(stdout);
  return 1;
}

// PAGINACAO (#6 Shield: biblioteca mostrava Trakt 213 de 1600+). A watchlist
// aceita ?page=&limit= e, sem eles, devolve tudo; a colecao NAO pagina (a doc
// do Trakt so lista page/limit na watchlist). O cabecalho X-Pagination-Page-
// Count nao chega aqui (rede_baixar_com devolve so o corpo), entao a parada e a
// do mapa de filmes vistos: pagina VAZIA, corpo igual ao anterior ou o teto da
// plataforma. O Trakt pode capar a pagina abaixo do limit pedido, por isso
// pagina curta NAO encerra. A colecao e pedida sem parametros, uma vez.
#define TRAKT_LISTA_PAGINA 500
#define TRAKT_LISTA_PAGINAS_MAX 64

// `*pv`/`cap` fixos (cresce=0) ou crescendo por realloc (cresce=1). `max` e o
// teto de itens; `pag` recebe quantas paginas foram lidas e `total` quantos
// itens a conta tinha ate onde foi lido (inclui os que passaram do teto).
static int listaNucleo(const char *qual, CatItem **pv, int *cap, int cresce,
                       int max, int *pag, int *total) {
  const char *cab[4];
  char aut[200], chave[140], url[200], *corpo, *anterior;
  const char *p;
  int n = 0, passo, paginas = 0, visto = 0;
  int paginavel = !strcmp(qual, "watchlist");
  if (!ligado) return 0;
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chave, sizeof chave, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;

  // Filmes e series vem em endpoints separados; misturar as duas listas na
  // mesma fileira e o que o dono ve como "Minha Lista".
  for (passo = 0; passo < 2; passo++) {
    const char *tipo = passo ? "shows" : "movies";
    int pagina = 1;
    anterior = NULL;
    for (;;) {
      int lidos = 0;
      if (paginavel)
        snprintf(url, sizeof url, "https://api.trakt.tv/sync/%s/%s?page=%d&limit=%d",
                 qual, tipo, pagina, TRAKT_LISTA_PAGINA);
      else
        snprintf(url, sizeof url, "https://api.trakt.tv/sync/%s/%s", qual, tipo);
      corpo = rede_baixar_com(url, 25, cab);
      if (!corpo) break;
      if (anterior && !strcmp(anterior, corpo)) { free(corpo); break; }
      paginas++;
      p = strchr(corpo, '[');
      p = p ? p + 1 : NULL;
      while (p && *p) {
        const char *f;
        const char *bloco;
        char imdb[24] = "", titulo[160] = "";
        while (*p && (unsigned char)*p <= ' ') p++;
        if (*p != '{') break;
        f = js_fim(p);
        bloco = strstr(p, passo ? "\"show\"" : "\"movie\"");
        if (bloco && bloco < f) {
          const char *fb = js_fim(strchr(bloco, '{'));
          js_texto(bloco, fb, "title", titulo, sizeof titulo);
          js_texto(bloco, fb, "imdb", imdb, sizeof imdb);
        }
        if (imdb[0]) {
          lidos++; visto++;
          if (n < max) {
            CatItem *d;
            if (cresce && n >= *cap) {
              int nc = *cap ? *cap * 2 : 256;
              CatItem *m;
              if (nc > max) nc = max;
              m = realloc(*pv, sizeof(CatItem) * (size_t)nc);
              if (!m) { max = n; p = js_prox(f); continue; }
              *pv = m; *cap = nc;
            }
            d = &(*pv)[n];
            memset(d, 0, sizeof *d);
            snprintf(d->titulo, sizeof d->titulo, "%s", titulo);
            snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
            snprintf(d->tipo, sizeof d->tipo, "%s", passo ? "series" : "movie");
            if (paginavel) { d->naLista = 1; d->fontes = CAT_FONTE_TRAKT; }
            else           d->naColecao = 1;
            // Arte SEM consultar: metahub deterministico pelo IMDb (ver
            // artemetahub.h). Uma consulta por item limitava a lista a dez.
            arte_metahub_preencher(d);
            snprintf(d->genero, sizeof d->genero, "%s",
                     i18n(passo ? "Programa de TV" : "Filme"));
            n++;
          }
        }
        p = js_prox(f);
      }
      free(anterior); anterior = corpo;
      if (!paginavel || !lidos || pagina >= TRAKT_LISTA_PAGINAS_MAX) break;
      if (n >= max) break;   // teto da plataforma: nao pede o resto
      pagina++;
    }
    free(anterior);
  }
  if (pag) *pag = paginas;
  if (total) *total = visto;
  return n;
}

static void listaLog(const char *qual, int n, int total, int paginas, int max) {
  printf("[trakt] %s: %d de %d (paginas %d)\n", qual, n, total, paginas);
  if (total > n)
    printf("[trakt] %s: teto %d da plataforma; %d ficaram de fora\n", qual, max,
           total - n);
  fflush(stdout);
}

int trakt_lista(const char *qual, CatItem *saida, int max) {
  int pag = 0, total = 0, n, cap = max;
  CatItem *v = saida;
  if (max <= 0) return 0;
  n = listaNucleo(qual, &v, &cap, 0, max, &pag, &total);
  listaLog(qual, n, total, pag, max);
  return n;
}

int trakt_lista_cresc(const char *qual, CatItem **saida, int max) {
  int pag = 0, total = 0, n, cap = 0;
  CatItem *v = NULL;
  *saida = NULL;
  if (max <= 0) return 0;
  n = listaNucleo(qual, &v, &cap, 1, max, &pag, &total);
  listaLog(qual, n, total, pag, max);
  *saida = v;
  return n;
}

// --- gravar progresso -------------------------------------------------------

// SCROBBLE (#179). Um trabalhador unico e uma vaga PENDENTE: o ultimo pedido
// vence, entao apertar pausa repetidamente nao empilha chamadas (o Trakt limita
// a taxa). A decisao de qual chamada mandar mora em traktscrobble.c.
static pthread_mutex_t travaScr = PTHREAD_MUTEX_INITIALIZER;
static ScrobbleEstado scrEstado;
static struct { int acao; char id[64]; double pct; int tem; } scrPend;
static int scrFioVivo;

static void enviarScrobble(int acao, const char *id, double pct) {
  const char *cab[4];
  char aut[200], chave[140], corpo[400], url[64], *r;
  int status = 0;
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chave, sizeof chave, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;
  scrobble_corpo(corpo, sizeof corpo, id, pct);
  snprintf(url, sizeof url, "https://api.trakt.tv/scrobble/%s", scrobble_nome(acao));
  r = rede_postar_st(url, 20, cab, corpo, &status);
  // O log NUNCA leva o token. rede_postar devolvia corpo nao nulo tambem em
  // 401/422, e o log antigo dizia "ok" para isso: o erro ficava invisivel.
  if (status >= 200 && status < 300)
    printf("[trakt] %s %s %.1f%% -> ok (%d)\n", scrobble_nome(acao), id, pct, status);
  else if (status == 409)
    printf("[trakt] %s %s %.1f%% -> 409 ja registrado, ignorado\n", scrobble_nome(acao), id, pct);
  else
    printf("[trakt] %s %s %.1f%% -> FALHOU http=%d%s%.80s\n", scrobble_nome(acao), id, pct,
           status, r && *r ? " corpo=" : "", r ? r : "");
  fflush(stdout);
  free(r);
}

static void *fioScrobble(void *u) {
  (void)u;
  for (;;) {
    int acao; char id[64]; double pct;
    pthread_mutex_lock(&travaScr);
    if (!scrPend.tem) { scrFioVivo = 0; pthread_mutex_unlock(&travaScr); return NULL; }
    acao = scrPend.acao; pct = scrPend.pct;
    snprintf(id, sizeof id, "%s", scrPend.id);
    scrPend.tem = 0;
    pthread_mutex_unlock(&travaScr);
    enviarScrobble(acao, id, pct);
  }
}

// Recusa. Na SAIDA a sessao acabou mesmo sem chamada: com o start ainda "em
// pe" a proxima abertura do mesmo titulo nao mandaria /scrobble/start.
static int recusar(int evento) {
  if (evento == SCR_EV_SAIU) {
    pthread_mutex_lock(&travaScr); scrEstado.ativo = 0; pthread_mutex_unlock(&travaScr);
  }
  return SCR_NADA;
}

// Enfileira. Devolve a acao decidida (SCR_NADA quando nao ha o que mandar).
int trakt_scrobble(int evento, const char *imdb, double posSeg, double durSeg, double assistidoSeg,
                   int assistiu) {
  int acao;
  double pct;
  pthread_t t;
  if (!ligado || !imdb || !*imdb || !isfinite(posSeg) || !isfinite(durSeg) ||
      !isfinite(assistidoSeg) || durSeg < 120.0 || assistidoSeg < 5.0) return recusar(evento);
  pct = 100.0 * posSeg / durSeg;
  // Valida ANTES da vaga pendente: nem um stop sem reproducao entra na fila.
  if (evento == SCR_EV_SAIU && pct >= 90.0 && !assistiu) return recusar(evento);
  pthread_mutex_lock(&travaScr);
  acao = scrobble_decidir(&scrEstado, evento, imdb, pct);
  if (acao != SCR_NADA) {
    if (acao == SCR_STOP) {
      printf("[visto] marcado por trakt-scrobble-player pos=%.3f dur=%.3f assistido=%.3fs id=%s\n",
             posSeg, durSeg, assistidoSeg, imdb);
      fflush(stdout);
    }
    scrPend.acao = acao; scrPend.pct = pct; scrPend.tem = 1;
    snprintf(scrPend.id, sizeof scrPend.id, "%s", imdb);
    if (!scrFioVivo) {
      scrFioVivo = 1;
      if (pthread_create(&t, NULL, fioScrobble, NULL) != 0) scrFioVivo = 0;
      else pthread_detach(t);
    }
  }
  pthread_mutex_unlock(&travaScr);
  return acao;
}

// Saida do player: stop (>= 90%) ou pause, como sempre foi.
void trakt_marcar(const char *imdb, double posSeg, double durSeg, double assistidoSeg, int assistiu) {
  trakt_scrobble(SCR_EV_SAIU, imdb, posSeg, durSeg, assistidoSeg, assistiu);
}

// --- WATCHLIST: escrever e ler ------------------------------------------------
//
// O botao "+" da tela de titulo so mexia num vetor local (biblioteca.c), entao
// a lista do dono nos outros aparelhos nunca soube. Agora ele fala com o Trakt,
// que ja e a fonte de verdade do resto do app.
//
// O ESTADO tambem importa: sem ler de volta, o botao mostrava "+" mesmo para um
// titulo que ja estava na lista, e um segundo toque adicionaria de novo.
// ci->naLista ja e preenchido por trakt_lista na descoberta; o que faltava era
// manter esse campo em dia depois de uma escrita nossa.
static char alvoLista[24];
static char alvoListaTipo[8];
static int  alvoAdicionar, fioListaVivo;
static pthread_t fioLista;

static void *enviarLista(void *u) {
  const char *cab[4];
  char aut[200], chave[140], url[120], corpo[200], id[24], tipoItemBuf[8];
  char *resp;
  int status = 0, confirmado;
  int adicionar;
  (void)u;
  pthread_mutex_lock(&travaLista);
  snprintf(id, sizeof id, "%s", alvoLista);
  snprintf(tipoItemBuf, sizeof tipoItemBuf, "%s", alvoListaTipo);
  adicionar = alvoAdicionar;
  pthread_mutex_unlock(&travaLista);
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) {
    estadoEscrever(&listaEstado, TK_OP_FALHA);
    pthread_mutex_lock(&travaLista); fioListaVivo = 0; pthread_mutex_unlock(&travaLista);
    return NULL;
  }
  // O tipo faz parte da intencao: mandar filme e serie juntos deixa a API
  // resolver o IMDb no escopo errado e torna a confirmacao ambigua.
  if (!strcmp(tipoItemBuf, "series"))
    snprintf(corpo, sizeof corpo, "{\"shows\":[{\"ids\":{\"imdb\":\"%s\"}}]}", id);
  else
    snprintf(corpo, sizeof corpo, "{\"movies\":[{\"ids\":{\"imdb\":\"%s\"}}]}", id);
  snprintf(url, sizeof url, "https://api.trakt.tv/sync/watchlist%s",
           adicionar ? "" : "/remove");
  resp = rede_postar_st(url, 20, cab, corpo, &status);
  confirmado = status >= 200 && status < 300;
  estadoEscrever(&listaEstado, confirmado ? TK_OP_CONFIRMADA : TK_OP_FALHA);
  printf("[trakt] watchlist %s %s (%s) -> %s (HTTP %d)\n",
         adicionar ? "add" : "del", id, tipoItemBuf,
         confirmado ? "confirmado" : "falhou", status);
  fflush(stdout);
  free(resp);
  pthread_mutex_lock(&travaLista); fioListaVivo = 0; pthread_mutex_unlock(&travaLista);
  return NULL;
}

// --- marcar/desmarcar como ASSISTIDO -----------------------------------------
//
// Endpoint DIFERENTE do trakt_marcar: aquele e /scrobble/pause ("parei aqui"),
// que o player usa ao sair. Este e /sync/history ("assisti"), que e o que o
// botao do olho quer dizer.
//
// Nao dava para reaproveitar trakt_marcar: ele guarda `durSeg <= 1.0 -> return`
// para nao mandar scrobble com duracao invalida, e o chamador do olho passava
// exatamente dur=1.0 — a funcao voltava na primeira linha e NADA era enviado. O
// botao parecia funcionar (o espelho local mudava) e o Trakt nunca sabia.
static pthread_t fioHist;
static int       fioHistVivo, histAdicionar;
static char      alvoHist[24];
static char      alvoHistTipo[8];
static HistoricoPedido alvoHistPedido;

static void *enviarHistorico(void *u) {
  const char *cab[4];
  char aut[200], chave[140], url[120], corpo[200], id[24], tipoItemBuf[8];
  char *resp;
  int status = 0, confirmado;
  int marcar;
  HistoricoPedido pedido;
  (void)u;
  pthread_mutex_lock(&travaHistorico);
  snprintf(id, sizeof id, "%s", alvoHist);
  snprintf(tipoItemBuf, sizeof tipoItemBuf, "%s", alvoHistTipo);
  marcar = histAdicionar;
  pedido = alvoHistPedido;
  pthread_mutex_unlock(&travaHistorico);
  if (!cabecalhosPedido(pedido, cab, aut, sizeof aut, chave, sizeof chave)) {
    estadoEscrever(&historicoEstado, TK_OP_FALHA);
    pthread_mutex_lock(&travaHistorico); fioHistVivo = 0; pthread_mutex_unlock(&travaHistorico);
    return NULL;
  }
  // O escopo do comando e explicito. Para serie, o alvo e o show, nao um
  // episodio derivado de progresso e nem um segundo vetor de tipo oposto.
  if (!strcmp(tipoItemBuf, "series"))
    snprintf(corpo, sizeof corpo, "{\"shows\":[{\"ids\":{\"imdb\":\"%s\"}}]}", id);
  else
    snprintf(corpo, sizeof corpo, "{\"movies\":[{\"ids\":{\"imdb\":\"%s\"}}]}", id);
  snprintf(url, sizeof url, "https://api.trakt.tv/sync/history%s",
           marcar ? "" : "/remove");
  resp = rede_postar_st(url, 20, cab, corpo, &status);
  confirmado = status >= 200 && status < 300 &&
    historicoDefinirPedido(id, tipoItemBuf, marcar, pedido);
  estadoEscrever(&historicoEstado, confirmado ? TK_OP_CONFIRMADA : TK_OP_FALHA);
  printf("[trakt] historico %s %s (%s) -> %s (HTTP %d)\n",
         marcar ? "add" : "del", id, tipoItemBuf,
         confirmado ? "confirmado" : "falhou", status);
  fflush(stdout);
  free(resp);
  pthread_mutex_lock(&travaHistorico); fioHistVivo = 0; pthread_mutex_unlock(&travaHistorico);
  return NULL;
}

int trakt_assistido_tipo(const char *imdb, const char *tipo, int marcar) {
  const char *dp;
  // TODA RECUSA FALA. As duas saidas daqui eram MUDAS, e o relato do dono foi
  // exatamente "clico e nao aparece nada no log" — sem uma linha nao ha como
  // separar "o Trakt esta desligado" de "ja ha um pedido no ar" de "o pedido
  // saiu e o servidor recusou". Silencio nao se diagnostica.
  if (!ligado || !imdb || imdb[0] != 't') {
    printf("[trakt] historico recusado: %s (id=%s)\n",
           !ligado ? "Trakt desligado" : "id invalido", imdb ? imdb : "(nulo)");
    fflush(stdout);
    estadoEscrever(&historicoEstado, TK_OP_FALHA);
    return 0;
  }
  pthread_mutex_lock(&travaHistorico);
  if (fioHistVivo) {
    pthread_mutex_unlock(&travaHistorico);
    // ESTA E A SAIDA QUE MAIS ENGANA: o pedido anterior ainda esta no ar (ate
    // 20 s de timeout), a funcao devolve 0 e o modal mostra falha sem que nada
    // tenha sido tentado. Agora ela DIZ, e escreve o estado — sem isso o modal
    // ficava com a mesma cara de "nao fez nada" que uma recusa do servidor.
    printf("[trakt] historico ocupado: ja ha um pedido no ar, %s ignorado\n", imdb);
    fflush(stdout);
    estadoEscrever(&historicoEstado, TK_OP_FALHA);
    return 0;
  }
  // "tt123:2:5" (episodio) vira "tt123": o historico e do TITULO.
  dp = strchr(imdb, ':');
  { size_t k = dp ? (size_t)(dp - imdb) : strlen(imdb);
    if (k >= sizeof alvoHist) k = sizeof alvoHist - 1;
    memcpy(alvoHist, imdb, k); alvoHist[k] = 0; }
  snprintf(alvoHistTipo, sizeof alvoHistTipo, "%s", tipo_item(tipo, imdb));
  histAdicionar = marcar;
  alvoHistPedido = historicoPedido();
  estadoEscrever(&historicoEstado, TK_OP_PENDENTE);
  fioHistVivo = 1;
  pthread_mutex_unlock(&travaHistorico);
  if (pthread_create(&fioHist, NULL, enviarHistorico, NULL) != 0) {
    pthread_mutex_lock(&travaHistorico); fioHistVivo = 0; pthread_mutex_unlock(&travaHistorico);
    estadoEscrever(&historicoEstado, TK_OP_FALHA);
    return 0;
  }
  else pthread_detach(fioHist);
  return 1;
}

int trakt_watchlist_tipo(const char *imdb, const char *tipo, int adicionar) {
  const char *dp;
  if (!ligado || !imdb || imdb[0] != 't') {
    estadoEscrever(&listaEstado, TK_OP_FALHA);
    return 0;
  }
  pthread_mutex_lock(&travaLista);
  if (fioListaVivo) {
    pthread_mutex_unlock(&travaLista);
    return 0;
  }
  dp = strchr(imdb, ':');
  { size_t k = dp ? (size_t)(dp - imdb) : strlen(imdb);
    if (k >= sizeof alvoLista) k = sizeof alvoLista - 1;
    memcpy(alvoLista, imdb, k); alvoLista[k] = 0; }
  snprintf(alvoListaTipo, sizeof alvoListaTipo, "%s", tipo_item(tipo, imdb));
  alvoAdicionar = adicionar;
  estadoEscrever(&listaEstado, TK_OP_PENDENTE);
  fioListaVivo = 1;
  pthread_mutex_unlock(&travaLista);
  if (pthread_create(&fioLista, NULL, enviarLista, NULL) != 0) {
    pthread_mutex_lock(&travaLista); fioListaVivo = 0; pthread_mutex_unlock(&travaLista);
    estadoEscrever(&listaEstado, TK_OP_FALHA);
    return 0;
  }
  else pthread_detach(fioLista);
  return 1;
}

void trakt_assistido(const char *imdb, int marcar) {
  (void)trakt_assistido_tipo(imdb, cat_tipo_por_imdb(imdb), marcar);
}

void trakt_watchlist(const char *imdb, int adicionar) {
  (void)trakt_watchlist_tipo(imdb, cat_tipo_por_imdb(imdb), adicionar);
}

// --- NOTA (/sync/ratings) ------------------------------------------------------
//
// A REACAO DOS CREDITOS (reacao.c) vira nota no Trakt quando ele esta ligado —
// decisao do dono. Um pedido por resposta, num fio proprio e destacado; o
// alvo viaja num bloco alocado, entao duas respostas seguidas nao disputam um
// buffer global (a reacao e rara: no maximo uma por titulo).
typedef struct { char id[24]; char tipo[8]; int nota; } AlvoNota;

static void *enviarNota(void *u) {
  AlvoNota *a = (AlvoNota *)u;
  const char *cab[4];
  char aut[200], chave[140], corpo[200];
  char *resp;
  int status = 0;
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) { free(a); return NULL; }
  snprintf(corpo, sizeof corpo, "{\"%s\":[{\"rating\":%d,\"ids\":{\"imdb\":\"%s\"}}]}",
           !strcmp(a->tipo, "series") ? "shows" : "movies", a->nota, a->id);
  resp = rede_postar_st("https://api.trakt.tv/sync/ratings", 20, cab, corpo, &status);
  printf("[trakt] nota %d %s (%s) -> HTTP %d\n", a->nota, a->id, a->tipo, status);
  fflush(stdout);
  free(resp);
  free(a);
  return NULL;
}

int trakt_avaliar(const char *imdb, const char *tipo, int nota) {
  AlvoNota *a;
  pthread_t f;
  const char *dp;
  if (!ligado || !imdb || imdb[0] != 't' || nota < 1 || nota > 10) return 0;
  a = (AlvoNota *)calloc(1, sizeof *a);
  if (!a) return 0;
  dp = strchr(imdb, ':');
  { size_t k = dp ? (size_t)(dp - imdb) : strlen(imdb);
    if (k >= sizeof a->id) k = sizeof a->id - 1;
    memcpy(a->id, imdb, k); a->id[k] = 0; }
  snprintf(a->tipo, sizeof a->tipo, "%s", tipo_item(tipo, imdb));
  a->nota = nota;
  if (pthread_create(&f, NULL, enviarNota, a) != 0) { free(a); return 0; }
  pthread_detach(f);
  return 1;
}
