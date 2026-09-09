#include "trakt.h"
#include "vistoep.h"
#include "idioma.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define CINEMETA "https://v3-cinemeta.strem.io"

static char token[128], cliente[80];
static int  ligado;

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

// API pequena e interna ao port: a declaracao fica no consumidor porque o
// contrato publico historico de trakt_watchlist/trakt_assistido continua void.
int trakt_operacao_estado(int tipo) {
  if (tipo == TK_OP_LISTA) return estadoLer(&listaEstado);
  if (tipo == TK_OP_HISTORICO) return estadoLer(&historicoEstado);
  return TK_OP_NENHUMA;
}

int trakt_ativo(void) { return ligado; }

void trakt_esquecer(void) {
  token[0] = 0;
  cliente[0] = 0;
  ligado = 0;
  printf("[trakt] credencial esquecida (saiu da conta)\n");
}

int trakt_definir(const char *tk, const char *cli) {
  if (!tk || !*tk) return 0;
  snprintf(token, sizeof token, "%s", tk);
  if (cli && *cli) snprintf(cliente, sizeof cliente, "%s", cli);
  ligado = token[0] && cliente[0];
  printf("[trakt] credencial da conta: %s\n",
         ligado ? "ativa" : "sem client id do aplicativo (ver tools/env.sh)");
  return ligado;
}


int trakt_cabecalhos(const char **cab, char *aut, size_t nAut,
                     char *chave, size_t nChave) {
  if (!ligado) return 0;
  snprintf(aut, nAut, "Authorization: Bearer %s", token);
  snprintf(chave, nChave, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;
  return 1;
}

int trakt_carregar(const char *dirArte) {
  char caminho[600], linha[300], *tab;
  FILE *f;
  // Vinculo feito NESTA TV (traktauth_carregar, que roda antes) ganha do
  // arquivo do pacote. MEDIDO: art/trakt.txt de desenvolvimento, com token de
  // 31/08 ja vencido, sobrescrevia o token recem-autorizado a cada arranque e
  // tudo do Trakt voltava a 401 — "autorizei e continua sem".
  if (token[0]) { printf("[trakt] vinculo desta TV mantido; arquivo do pacote ignorado\n"); return 1; }
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
  printf("[trakt] %s\n", ligado ? "credencial carregada" : "credencial incompleta");
  return ligado;
}

// Arte e sinopse por id do IMDb. O Trakt devolve so identificadores e
// progresso; quem tem imagem e o Cinemeta, que e o mesmo indice que os addons
// usam — entao o que aparece na tela e o que da para pedir fonte.
static int enfeitar(CatItem *d, const char *tipo) {
  char url[300], *corpo;
  char serie[24];
  const char *dp;
  int ok = 0;
  snprintf(serie, sizeof serie, "%s", d->imdb);
  dp = strchr(serie, ':');
  if (dp) *(char *)dp = 0;
  snprintf(url, sizeof url, "%s/meta/%s/%s.json", CINEMETA, tipo, serie);
  // 8 s e nao 20: sao ate OITO destes em serie (um por item do historico) antes
  // de a primeira fileira da home existir. Medido no Mac: 2,1 s no caso bom;
  // com um item lento eram 20 s de tela sem conteudo nenhum.
  corpo = rede_baixar(url, 8);
  if (!corpo) return 0;
  ok = js_texto(corpo, NULL, "poster", d->poster, sizeof d->poster);
  js_texto(corpo, NULL, "background", d->backdrop, sizeof d->backdrop);
  js_texto(corpo, NULL, "logo", d->logo, sizeof d->logo);
  if (!d->titulo[0]) js_texto(corpo, NULL, "name", d->titulo, sizeof d->titulo);
  js_texto(corpo, NULL, "description", d->sinopse, sizeof d->sinopse);
  if (!d->backdrop[0]) snprintf(d->backdrop, sizeof d->backdrop, "%s", d->poster);
  { char r[24] = "", ano[24] = "";
    js_texto(corpo, NULL, "runtime", r, sizeof r);
    js_texto(corpo, NULL, "releaseInfo", ano, sizeof ano);
    { char *tr = strstr(ano, "\xe2\x80\x93"); if (tr) *tr = 0; }
    snprintf(d->meta, sizeof d->meta, "%.20s%s%.20s", ano,
             (ano[0] && r[0]) ? "  \xc2\xb7  " : "", r);
    // Minutos que faltam, para a legenda do card. O Trakt da a porcentagem e o
    // Cinemeta a duracao; o cruzamento das duas e o unico jeito de ter isto
    // sem baixar o arquivo.
    if (d->progresso > 0 && d->progresso < 100) {
      int total = atoi(r);
      if (total > 0) d->restanteMin = total - (total * d->progresso) / 100;
    } else if (d->progresso == 0) {
      d->restanteMin = atoi(r);
    } }
  snprintf(d->genero, sizeof d->genero, "%s",
           strcmp(tipo, "series") ? "Filme" : "Programa de TV");
  snprintf(d->classificacao, sizeof d->classificacao, "14");
  free(corpo);
  return ok;
}

// ENFEITAR EM PARALELO.
//
// Sao ate 8 GET ao Cinemeta, um por item do historico, e eram feitos EM SERIE
// dentro do laco de leitura. Medido no Mac: 2,1 s antes de a home ter qualquer
// conteudo de rede — e essa e a PRIMEIRA fileira, a que o dono ve primeiro.
//
// Cada `enfeitar` so escreve no seu proprio CatItem e nao toca estado
// compartilhado, entao a paralelizacao e direta. A ordem do historico e
// preservada porque cada fio escreve na posicao que ja era dele.
#define TK_FIOS 3

typedef struct { CatItem *d; char tipo[8]; int ok; } TarefaEnf;
static TarefaEnf *enfTarefas;
static int enfN, enfProx;
static pthread_mutex_t enfTrava = PTHREAD_MUTEX_INITIALIZER;

static void *fioEnfeitar(void *u) {
  (void)u;
  for (;;) {
    int meu;
    pthread_mutex_lock(&enfTrava);
    if (enfProx >= enfN) { pthread_mutex_unlock(&enfTrava); return NULL; }
    meu = enfProx++;
    pthread_mutex_unlock(&enfTrava);
    enfTarefas[meu].ok = enfeitar(enfTarefas[meu].d, enfTarefas[meu].tipo);
  }
}

// ENFEITAR os n itens em TK_FIOS fios, e so entao compactar: `enfeitar` falha
// para item que o Cinemeta nao conhece, e antes o `if (enfeitar(...)) n++`
// simplesmente nao contava — agora o item ja esta na posicao, entao os que
// falharam saem por compactacao, preservando a ordem do historico.
//
// Publico porque a fileira "Continuar assistindo" montada do progresso LOCAL
// (descoberta.c, sem Trakt) precisa exatamente do mesmo enfeite: tem imdb,
// tipo e porcentagem, e falta arte, sinopse e minutos restantes.
int trakt_enfeitar_lote(CatItem *saida, int n) {
  if (n <= 0) return 0;
  enfTarefas = calloc((size_t)n, sizeof(TarefaEnf));
  if (enfTarefas) {
    pthread_t fios[TK_FIOS];
    int criados = 0, q, r, w;
    for (q = 0; q < n; q++) {
      enfTarefas[q].d = &saida[q];
      snprintf(enfTarefas[q].tipo, sizeof enfTarefas[q].tipo, "%s", saida[q].tipo);
    }
    enfN = n; enfProx = 0;
    for (q = 0; q < TK_FIOS; q++)
      if (pthread_create(&fios[criados], NULL, fioEnfeitar, NULL) == 0) criados++;
    if (!criados) fioEnfeitar(NULL);      // sem fios: em serie, mesmo resultado
    for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);

    for (r = 0, w = 0; r < n; r++)
      if (enfTarefas[r].ok) { if (w != r) saida[w] = saida[r]; w++; }
    n = w;
    free(enfTarefas); enfTarefas = NULL; enfN = 0;
  } else {
    // Sem memoria para a fila: em serie, no proprio fio.
    int r, w;
    for (r = 0, w = 0; r < n; r++)
      if (enfeitar(&saida[r], saida[r].tipo)) { if (w != r) saida[w] = saida[r]; w++; }
    n = w;
  }
  return n;
}

// OS IDS DOS REGISTROS DE PLAYBACK da ultima leitura, para poder APAGAR.
// Preenchida em trakt_continuar; consumida por trakt_playback_remover. Cabe a
// mesma quantidade que a fileira mostra com folga.
#define TK_PLAY_MAX 64
static struct { char chave[28]; long long id; } play[TK_PLAY_MAX];
static int nPlay;

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
  long long id = 0;
  if (!ligado || !imdb || !imdb[0]) return 0;
  for (i = 0; i < nPlay; i++)
    if (!strcmp(play[i].chave, imdb)) { id = play[i].id; break; }
  if (!id) {
    printf("[trakt] playback: sem id para %s (nao veio do Trakt)\n", imdb);
    fflush(stdout);
    return 0;
  }
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chaveCab, sizeof chaveCab, "trakt-api-key: %s", cliente);
  cab[0] = aut;
  cab[1] = "trakt-api-version: 2";
  cab[2] = chaveCab;
  cab[3] = NULL;
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
  if (ok)
    for (i = 0; i < nPlay; i++)
      if (!strcmp(play[i].chave, imdb)) { play[i].chave[0] = 0; break; }
  return ok;
}

// QUAIS EPISODIOS DESTA SERIE ESTAO VISTOS.
//
// POR SERIE, E SOB DEMANDA — e as duas coisas foram MEDIDAS, nao escolhidas.
//
// A primeira tentativa foi /sync/watched/shows, que a documentacao descreve
// como o mapa completo por temporada. Na TV do dono, com 75 series na conta,
// ela devolveu 25540 bytes SEM UMA UNICA ocorrencia de "seasons"; com
// ?extended=full subiu para 90675 bytes e continuou com seasons=0, episodes=0,
// number=0 — so a lista de series com contagem de plays. Ou seja: por este
// caminho o detalhamento por episodio nao vem, e insistir nele custa 90 KB por
// ciclo para nada. Nao retentar sem uma medicao nova.
//
// /shows/<id>/progress/watched devolve o mapa completo de UMA serie, com
// `completed` por episodio. Uma requisicao, no momento em que a lista de
// episodios daquele titulo abre — que e exatamente quando o dado importa. Para
// 75 series, e uma chamada em vez de um mapa gigante que quase nunca e usado
// inteiro.
//
// `?hidden=false&specials=true&count_specials=false`: especiais aparecem na
// lista de episodios do app, entao tem de aparecer no mapa; contar especiais no
// total mudaria a conta de "quantos faltam" sem mudar o que se ve.
int trakt_progresso_serie(const char *imdb) {
  const char *cab[4];
  char aut[200], chaveCab[140], url[180], id[24];
  char *corpo;
  int i, n = 0;
  if (!ligado || !imdb || imdb[0] != 't') return 0;
  for (i = 0; imdb[i] && imdb[i] != ':' && i < (int)sizeof id - 1; i++) id[i] = imdb[i];
  id[i] = 0;
  if (!id[0]) return 0;
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chaveCab, sizeof chaveCab, "trakt-api-key: %s", cliente);
  cab[0] = aut;
  cab[1] = "trakt-api-version: 2";
  cab[2] = chaveCab;
  cab[3] = NULL;
  snprintf(url, sizeof url,
           "https://api.trakt.tv/shows/%s/progress/watched"
           "?hidden=false&specials=true&count_specials=false", id);
  corpo = rede_baixar_com(url, 20, cab);
  if (!corpo) { printf("[trakt] progresso de %s: sem resposta\n", id); fflush(stdout); return 0; }
  n = vistoep_ler_progresso(id, corpo);
  free(corpo);
  return n;
}

// A barra de retomada vem de /sync/playback e nao informa se o titulo foi
// marcado como assistido. Consultamos o historico real uma vez no mesmo ciclo
// de descoberta para que a modal nao trate progresso alto como prova de visto.
// Para series, registros com `episode` sao deliberadamente ignorados: ter
// visto um episodio nao significa ter marcado a serie inteira como assistida.
static void carregarHistoricoReal(const char *const *cab) {
  char *corpo = rede_baixar_com("https://api.trakt.tv/sync/history?limit=100&extended=full", 25, cab);
  const char *p;
  if (!corpo) return;
  p = strchr(corpo, '[');
  p = p ? p + 1 : NULL;
  while (p && *p) {
    const char *f, *obj;
    char id[24] = "";
    const char *tipo = NULL;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    f = js_fim(p);
    if (strstr(p, "\"episode\"") && strstr(p, "\"episode\"") < f) {
      p = js_prox(f);
      continue;
    }
    obj = strstr(p, "\"movie\"");
    if (obj && obj < f) tipo = "movie";
    else {
      obj = strstr(p, "\"show\"");
      if (obj && obj < f) tipo = "series";
    }
    if (obj && tipo) {
      const char *fo = js_fim(strchr(obj, '{'));
      js_texto(obj, fo, "imdb", id, sizeof id);
      if (id[0]) cat_historico_definir_id(id, tipo, 1);
    }
    p = js_prox(f);
  }
  free(corpo);
}

int trakt_continuar(CatItem *saida, int max) {
  const char *cab[4];
  char aut[200], chave[140];
  char *corpo;
  const char *p;
  int n = 0;
  if (!ligado) return 0;
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chave, sizeof chave, "trakt-api-key: %s", cliente);
  cab[0] = aut;
  cab[1] = "trakt-api-version: 2";
  cab[2] = chave;
  cab[3] = NULL;
  nPlay = 0;
  corpo = rede_baixar_com("https://api.trakt.tv/sync/playback?extended=full", 25, cab);
  if (!corpo) { printf("[trakt] sem resposta\n"); return 0; }
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
      if (nPlay < TK_PLAY_MAX) {
        double pid = js_num(p, f, "id", 0.0);
        if (pid > 0.0) {
          snprintf(play[nPlay].chave, sizeof play[nPlay].chave, "%s", d->imdb);
          play[nPlay].id = (long long)pid;
          nPlay++;
        }
      }
      // Enfeitar fica para DEPOIS do laco, em paralelo. Aqui o item ja esta
      // montado: so falta a arte e a sinopse, que vem da rede.
      n++;
    }
    p = js_prox(f);
  }
  free(corpo);
  carregarHistoricoReal(cab);
  // O MAPA DE EPISODIOS VISTOS NAO SAI DAQUI. Ver trakt_progresso_serie: ele e
  // buscado por SERIE, quando a lista de episodios daquele titulo abre.

  n = trakt_enfeitar_lote(saida, n);

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

int trakt_social(CatItem *saida, int max) {
  const char *cab[4];
  char aut[200], chave[140], *corpo;
  const char *p;
  int n = 0;
  if (!ligado || max < 1) return 0;
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) return 0;
  corpo = rede_baixar_com(
    "https://api.trakt.tv/users/me/friends/activities?extended=full&page=1&limit=12",
    20, cab);
  // Contas sem o escopo social novo podem receber 401 no grafo `friends`,
  // embora o token continue valido para historico. `following` e o fallback
  // honesto: ainda sao pessoas escolhidas pelo dono, nunca atividade global.
  if (!corpo) corpo = rede_baixar_com(
    "https://api.trakt.tv/users/me/following/activities?extended=full&page=1&limit=12",
    20, cab);
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
    TarefaEnf *tarefasSoc = calloc((size_t)n, sizeof *tarefasSoc);
    if (tarefasSoc) {
      pthread_t fios[TK_FIOS]; int criados = 0, q;
      enfTarefas = tarefasSoc; enfN = n; enfProx = 0;
      for (q = 0; q < n; q++) {
        tarefasSoc[q].d = &saida[q];
        snprintf(tarefasSoc[q].tipo, sizeof tarefasSoc[q].tipo, "%s", saida[q].tipo);
      }
      for (q = 0; q < TK_FIOS; q++)
        if (pthread_create(&fios[criados], NULL, fioEnfeitar, NULL) == 0) criados++;
      if (!criados) fioEnfeitar(NULL);
      for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);
      // Arte indisponivel nao pode apagar uma pessoa real do feed.
      free(tarefasSoc); enfTarefas = NULL; enfN = 0;
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
  snprintf(d->periodo,sizeof d->periodo,"%s %d",meses[tmv.tm_mon],tmv.tm_year+1900);
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

int trakt_lista(const char *qual, CatItem *saida, int max) {
  const char *cab[4];
  char aut[200], chave[140], url[160], *corpo;
  const char *p;
  int n = 0, passo;
  if (!ligado) return 0;
  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chave, sizeof chave, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;

  // Filmes e series vem em endpoints separados; misturar as duas listas na
  // mesma fileira e o que o dono ve como "Minha Lista".
  for (passo = 0; passo < 2 && n < max; passo++) {
    const char *tipo = passo ? "shows" : "movies";
    snprintf(url, sizeof url, "https://api.trakt.tv/sync/%s/%s", qual, tipo);
    corpo = rede_baixar_com(url, 25, cab);
    if (!corpo) continue;
    p = strchr(corpo, '[');
    p = p ? p + 1 : NULL;
    while (p && *p && n < max) {
      const char *f;
      while (*p && (unsigned char)*p <= ' ') p++;
      if (*p != '{') break;
      f = js_fim(p);
      {
        CatItem *d = &saida[n];
        const char *bloco = strstr(p, passo ? "\"show\"" : "\"movie\"");
        char imdb[24] = "";
        memset(d, 0, sizeof *d);
        if (bloco && bloco < f) {
          const char *fb = js_fim(strchr(bloco, '{'));
          js_texto(bloco, fb, "title", d->titulo, sizeof d->titulo);
          js_texto(bloco, fb, "imdb", imdb, sizeof imdb);
        }
        if (imdb[0]) {
          snprintf(d->imdb, sizeof d->imdb, "%s", imdb);
          snprintf(d->tipo, sizeof d->tipo, "%s", passo ? "series" : "movie");
          if (!strcmp(qual, "watchlist")) d->naLista = 1;
          else                            d->naColecao = 1;
          // Arte SEM consultar: as URLs do metahub sao deterministicas pelo id
          // do IMDb (verificado, 200 em todos os testados). Uma consulta por
          // item custava ~0,3 s e limitava a lista a dez; assim ela pode ter o
          // tamanho que o dono tem, e a imagem so e baixada quando aparece na
          // tela — o tex_cache ja faz isso.
          snprintf(d->poster, sizeof d->poster,
                   // "medium" e nao "small", e a diferenca NAO e tamanho: o
                   // metahub serve poster/small como image/WEBP e poster/medium
                   // como image/jpeg. O libSDL2_image DESTA TV carrega libjpeg,
                   // libpng16 e libtiff por dlopen e NAO carrega libwebp — a
                   // unica string de erro de formato dentro dele e "WEBP images
                   // are not supported". (A libwebp.so.7 existe no sistema; o
                   // SDL2_image e que nao foi compilado com ela.)
                   //
                   // Efeito do small: TODO card vindo do Trakt (watchlist,
                   // colecao, a Biblioteca inteira) nunca decodificava — e pior,
                   // o cache nao guarda falha, entao cada quadro tentava de novo
                   // e queimava uma vaga de decode. Era a maior causa de "nao
                   // aparecem todos os posteres".
                   //
                   // Custo: 105 KB contra 31 KB. Barato pela arte existir.
                   "https://images.metahub.space/poster/medium/%s/img", imdb);
          snprintf(d->backdrop, sizeof d->backdrop,
                   "https://images.metahub.space/background/medium/%s/img", imdb);
          snprintf(d->logo, sizeof d->logo,
                   "https://images.metahub.space/logo/medium/%s/img", imdb);
          snprintf(d->genero, sizeof d->genero, "%s",
                   passo ? "Programa de TV" : "Filme");
          snprintf(d->classificacao, sizeof d->classificacao, "14");
          n++;
        }
      }
      p = js_prox(f);
    }
    free(corpo);
  }
  printf("[trakt] %s: %d\n", qual, n);
  fflush(stdout);
  return n;
}

// --- gravar progresso -------------------------------------------------------

static char marcaId[64];
static double marcaPos, marcaDur;
static pthread_t fioMarca;
static int fioMarcaVivo;

static void *enviarMarca(void *u) {
  const char *cab[4];
  char aut[200], chave[140], corpo[400], *r;
  char id[24];
  int t = 0, e = 0;
  const char *dp;
  double pct;
  (void)u;
  snprintf(id, sizeof id, "%s", marcaId);
  dp = strchr(id, ':');
  if (dp) { sscanf(dp + 1, "%d:%d", &t, &e); *(char *)dp = 0; }
  pct = 100.0 * marcaPos / marcaDur;
  if (pct < 0.0) pct = 0.0;
  if (pct > 100.0) pct = 100.0;

  snprintf(aut, sizeof aut, "Authorization: Bearer %s", token);
  snprintf(chave, sizeof chave, "trakt-api-key: %s", cliente);
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = chave; cab[3] = NULL;

  // Pause preserva o ponto; stop registra a conclusao. Mantemos o limiar
  // conservador de 90% deste cliente. Pause sozinho nunca conclui o episodio.
  if (t > 0 && e > 0)
    snprintf(corpo, sizeof corpo,
             "{\"show\":{\"ids\":{\"imdb\":\"%s\"}},"
             "\"episode\":{\"season\":%d,\"number\":%d},\"progress\":%.2f}",
             id, t, e, pct);
  else
    snprintf(corpo, sizeof corpo,
             "{\"movie\":{\"ids\":{\"imdb\":\"%s\"}},\"progress\":%.2f}",
             id, pct);

  r = rede_postar(pct >= 90 ? "https://api.trakt.tv/scrobble/stop" :
                             "https://api.trakt.tv/scrobble/pause", 20, cab, corpo);
  printf("[trakt] %s %s %.1f%% -> %s\n", pct>=90?"stop":"pause",marcaId,pct,r?"ok":"falhou");
  fflush(stdout);
  free(r);
  fioMarcaVivo = 0;
  return NULL;
}

void trakt_marcar(const char *imdb, double posSeg, double durSeg) {
  if (!ligado || !imdb || !*imdb || durSeg <= 1.0 || fioMarcaVivo) return;
  snprintf(marcaId, sizeof marcaId, "%s", imdb);
  marcaPos = posSeg; marcaDur = durSeg;
  fioMarcaVivo = 1;
  if (pthread_create(&fioMarca, NULL, enviarMarca, NULL) != 0) fioMarcaVivo = 0;
  else pthread_detach(fioMarca);
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

static void *enviarHistorico(void *u) {
  const char *cab[4];
  char aut[200], chave[140], url[120], corpo[200], id[24], tipoItemBuf[8];
  char *resp;
  int status = 0, confirmado;
  int marcar;
  (void)u;
  pthread_mutex_lock(&travaHistorico);
  snprintf(id, sizeof id, "%s", alvoHist);
  snprintf(tipoItemBuf, sizeof tipoItemBuf, "%s", alvoHistTipo);
  marcar = histAdicionar;
  pthread_mutex_unlock(&travaHistorico);
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) {
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
  confirmado = status >= 200 && status < 300;
  estadoEscrever(&historicoEstado, confirmado ? TK_OP_CONFIRMADA : TK_OP_FALHA);
  if (confirmado) cat_historico_definir_id(id, tipoItemBuf, marcar);
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
