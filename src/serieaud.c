// Audiencia da serie. Ver serieaud.h para O QUE cada painel diz; aqui esta
// COMO ele e obtido e quanto custa.
//
// --- O ORCAMENTO DE REDE, dito por inteiro -----------------------------------
//
// Isto e uma TV, nao um navegador. A conta de um painel que pede /stats por
// episodio e a parte que pode estragar a pagina inteira, entao ela esta escrita
// aqui e nao escondida no codigo:
//
//   QUANDO — so em serieaud_abrir(), que o detalhe chama quando a pessoa ENTRA
//     na secao. Abrir a pagina de uma serie nao dispara nada. A maioria das
//     visitas nunca rola ate aqui e essas pagam ZERO.
//
//   QUANTO — 1 pedido de /shows/<id>/stats (o indice de revisita da serie) +
//     1 pedido por episodio, ATE SA_EP_MAX (24). Temporada de 10 episodios:
//     11 pedidos. Temporada de anime com 26: 25 pedidos, e o 26o episodio fica
//     sem dado e o grafico DIZ isso, em vez de fingir que a temporada acaba no
//     24 (ver `truncada`).
//
//   EM QUE ORDEM — E1 PRIMEIRO, sempre, e depois em ordem de episodio. Nao e
//     arbitrario: o E1 e o DENOMINADOR da retencao, entao sem ele nenhum ponto
//     dos outros dois paineis pode ser desenhado; e a ordem de episodio e
//     exatamente a ordem da esquerda para a direita no grafico, entao o que
//     chega primeiro e o que o olho procura primeiro. Cada resposta e publicada
//     na hora, sob a trava: a curva CRESCE na tela em vez de aparecer inteira
//     no fim.
//
//   ONDE — num fio proprio (pthread), nunca no laco de desenho. rede_baixar_com
//     bloqueia.
//
//   ATE QUANDO — serieaud_fechar() levanta `abandonar`, conferido ANTES de cada
//     pedido. Sair da pagina no meio de uma temporada de 24 interrompe na
//     fronteira do episodio seguinte, em vez de gastar as 20 viagens restantes
//     para uma tela que ninguem esta vendo.
//
//   E NA SEGUNDA VISITA — zero. O resultado vai para o disco em
//     `serieaud-<imdb>-t<N>.txt` (~40 bytes por episodio, ~1 KB por
//     temporada), e uma temporada em cache e lida sem tocar na rede. O arquivo
//     vale SA_VALIDADE_DIAS; depois disso a temporada e buscada de novo, porque
//     watchers de serie nova ainda sobe rapido.
//
// --- MEMORIA -----------------------------------------------------------------
//
// Vetores ESTATICOS de tamanho fixo (SA_EP_MAX), como o resto desta base. O
// alvo Tizen e um heap WebAssembly de 256 MiB que ja estourou antes
// (TIZEN-MEMORIA.md); um painel de grafico nao e lugar para malloc por
// episodio. O unico bloco dinamico e o corpo da resposta, que rede.c devolve e
// este arquivo libera na mesma funcao.
//
// --- DESENHO -----------------------------------------------------------------
//
// So gfx_* e texto. Nenhuma textura: o orcamento de imagens
// (NV_TEX_ORCAMENTO_MB = 96) ja vive encostado no teto na TV do reporter, e um
// grafico rasterizado para textura seria mais uma arte de tela grande
// disputando o mesmo espaco por uma coisa que e geometria pura.
#include "serieaud.h"
#include "trakt.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "text.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "textogate.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define SA_VALIDADE_DIAS 14
#define SA_ARQ_V "# nuvio serieaud v1"

// --- ESTADO ------------------------------------------------------------------

typedef struct {
  int  ep;          // numero do episodio
  int  nota;        // decimos, 0 = sem nota
  int  tem;         // 1 quando o /stats respondeu
  long watchers;
  long plays;
  int  comentarios;
  int  votos;
} SaEp;

static SaEp eps[SA_EP_MAX];
static int  nEps;
static int  temporadaAtual;
static char imdbAtual[24];
static long playsSerie = -1, watchersSerie = -1;
static int  selecionado;
// 1 quando a temporada tem MAIS episodios do que o teto de pedidos. O rodape
// do painel diz isso; sem a marca, uma temporada de 26 pareceria ter 24.
static int  truncada;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char imdbPedido[24], imdbEmCurso[24];
static int  tempPedida, tempEmCurso;
static int  fioVivo, abandonar;
static pthread_t fio;

// --- UTIL --------------------------------------------------------------------

// "tt1234567:2:4" -> "tt1234567". A lista de episodios usa a chave composta, e
// o Trakt nao resolve ela.
static void soImdb(char *dst, unsigned tam, const char *s) {
  unsigned k = 0;
  if (!s) { if (tam) dst[0] = 0; return; }
  while (s[k] && s[k] != ':' && k + 1 < tam) { dst[k] = s[k]; k++; }
  dst[k] = 0;
}

static void nomeArquivo(char *dst, unsigned tam, const char *imdb, int temp) {
  char limpo[24];
  unsigned k = 0, j = 0;
  // So alfanumerico no nome: o imdb vem de fora e um '/' ali viraria escrita
  // em outra pasta.
  while (imdb[k] && j + 1 < sizeof limpo) {
    char c = imdb[k++];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      limpo[j++] = c;
  }
  limpo[j] = 0;
  snprintf(dst, tam, "serieaud-%s-t%d.txt", limpo, temp);
}

// --- CACHE EM DISCO ----------------------------------------------------------

static void gravarCache(void) {
  char nome[80];
  char buf[SA_EP_MAX * 48 + 160];
  size_t k = 0;
  int i;
  if (!imdbAtual[0]) return;
  nomeArquivo(nome, sizeof nome, imdbAtual, temporadaAtual);
  k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\n", SA_ARQ_V);
  k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\t%d\t%lld\t%ld\t%ld\n",
                        imdbAtual, temporadaAtual, (long long)time(NULL),
                        playsSerie, watchersSerie);
  for (i = 0; i < nEps && k + 1 < sizeof buf; i++) {
    if (!eps[i].tem) continue;
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%d\t%ld\t%ld\t%d\t%d\n",
                          eps[i].ep, eps[i].watchers, eps[i].plays,
                          eps[i].comentarios, eps[i].votos);
  }
  // LEVE, e nao dados_gravar: isto e conteudo RE-OBTIVEL (dados.h). Perder o
  // cache custa uma temporada de pedidos; pagar uma descarga sincrona de
  // IndexedDB por causa dele, na TV, custa um engasgo na tela.
  dados_gravar_leve(nome, buf);
}

// Preenche o que ja estiver em `eps` a partir do CONTEUDO do arquivo (que quem
// chama leu do disco). Devolve quantos episodios foram preenchidos; 0 quando o
// cache nao vale.
//
// A LEITURA DO DISCO FICA DE FORA DE PROPOSITO. Ela e feita antes, SEM a trava
// do modulo: `dados_ler` pega a trava do sistema de arquivos (no alvo Tizen ela
// nao e opcional — ver dados.h), e segurar a trava daqui durante um I/O faria o
// LACO DE DESENHO parar no serieaud_carregando() do proximo quadro, esperando
// um arquivo. Uma vez por secao aberta e pouco, mas e um engasgo de tela por um
// motivo que nao precisa existir.
static int aplicarCache(char *b, const char *imdb, int temp) {
  char *p;
  char imdbArq[24];
  int tempArq = 0, achados = 0;
  long long quando = 0;
  if (!b) return 0;
  if (strncmp(b, SA_ARQ_V, strlen(SA_ARQ_V))) return 0;
  p = strchr(b, '\n');
  if (!p) return 0;
  p++;
  { long pl = -1, wt = -1;
    imdbArq[0] = 0;
    if (sscanf(p, "%23[^\t]\t%d\t%lld\t%ld\t%ld", imdbArq, &tempArq, &quando,
               &pl, &wt) < 3) return 0;
    // Confere a identidade: um arquivo de OUTRA serie no lugar deste seria um
    // grafico com a cara certa e os numeros de outra obra.
    if (strcmp(imdbArq, imdb) || tempArq != temp) return 0;
    if ((long long)time(NULL) - quando > (long long)SA_VALIDADE_DIAS * 86400)
      return 0;
    playsSerie = pl; watchersSerie = wt;
  }
  p = strchr(p, '\n');
  while (p) {
    int ep = 0, com = 0, vot = 0, i;
    long wt = 0, pl = 0;
    p++;
    if (!*p) break;
    if (sscanf(p, "%d\t%ld\t%ld\t%d\t%d", &ep, &wt, &pl, &com, &vot) == 5) {
      for (i = 0; i < nEps; i++) if (eps[i].ep == ep) {
        eps[i].tem = 1; eps[i].watchers = wt; eps[i].plays = pl;
        eps[i].comentarios = com; eps[i].votos = vot;
        achados++;
        break;
      }
    }
    p = strchr(p, '\n');
  }
  return achados;
}

// --- LEITURA DE UM /stats ----------------------------------------------------
//
// O corpo e raso e conhecido:
//   {"watchers":387823,"plays":460619,"collectors":621573,
//    "comments":29,"lists":867,"votes":4935}
// `collectors` e `lists` sao deliberadamente ignorados: colecionador e quem tem
// o arquivo, nao quem assistiu, e usa-lo como audiencia seria justamente o tipo
// de rotulo generoso que este modulo existe para nao ter.
static int lerStats(const char *corpo, SaEp *d) {
  double w;
  if (!corpo) return 0;
  w = js_num(corpo, NULL, "watchers", -1.0);
  if (w < 0.0) return 0;
  d->watchers    = (long)w;
  d->plays       = (long)js_num(corpo, NULL, "plays", 0.0);
  d->comentarios = (int)js_num(corpo, NULL, "comments", 0.0);
  d->votos       = (int)js_num(corpo, NULL, "votes", 0.0);
  d->tem         = 1;
  return 1;
}

// --- O FIO -------------------------------------------------------------------

// Encerra o fio e, se a pessoa trocou de temporada ENQUANTO ele estava no ar,
// comeca o pedido novo na hora.
//
// SEM ISTO A TELA SEGUINTE FICA VAZIA PARA SEMPRE. O laco de busca percebe que
// `imdbPedido` mudou e sai — certo —, mas serieaud_abrir so cria fio quando
// `fioVivo` e 0, e naquele instante ele ainda era 1: o pedido novo nunca nasce.
// E o mesmo defeito que extras.c ja teve e documenta em finalizarBusca().
static void *buscar(void *arg);
static void finalizar(const char *imdb, int temp) {
  int continuar = 0;
  pthread_mutex_lock(&trava);
  if (strcmp(imdbPedido, imdb) || tempPedida != temp) {
    snprintf(imdbEmCurso, sizeof imdbEmCurso, "%s", imdbPedido);
    tempEmCurso = tempPedida;
    abandonar = 0;
    continuar = 1;
  } else {
    fioVivo = 0;
  }
  pthread_mutex_unlock(&trava);
  if (continuar) {
    if (pthread_create(&fio, NULL, buscar, NULL) != 0) {
      pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    } else pthread_detach(fio);
  }
}

static int deveParar(void) {
  int p;
  pthread_mutex_lock(&trava);
  p = abandonar;
  pthread_mutex_unlock(&trava);
  return p;
}

static void *buscar(void *arg) {
  const char *cab[4];
  char aut[200], chave[140], url[220], imdb[24];
  int temp, i;
  (void)arg;

  pthread_mutex_lock(&trava);
  snprintf(imdb, sizeof imdb, "%s", imdbEmCurso);
  temp = tempEmCurso;
  pthread_mutex_unlock(&trava);

  // 1. O CACHE PRIMEIRO. Uma temporada ja vista nao gasta pedido nenhum.
  //
  // COBERTURA PARCIAL NAO ENCERRA A BUSCA. Se a visita anterior foi
  // interrompida (a pessoa saiu no meio) ou alguns /stats falharam, o arquivo
  // tem SO parte da temporada. Sair aqui deixaria buracos permanentes por
  // SA_VALIDADE_DIAS, com cara de "estes episodios nao tem dado" quando o que
  // houve foi uma queda de rede. Entao o que ja veio e usado (a curva aparece
  // na hora) e o laco adiante PULA o que ja esta na mao.
  { int achados;
    char nomeC[80], *bruto;
    nomeArquivo(nomeC, sizeof nomeC, imdb, temp);
    bruto = dados_ler(nomeC);            /* I/O FORA da trava do modulo */
    pthread_mutex_lock(&trava);
    achados = aplicarCache(bruto, imdb, temp);
    pthread_mutex_unlock(&trava);
    free(bruto);
    if (achados >= nEps && achados > 0) {
      printf("[serieaud] %s T%d: %d episodios do cache, 0 pedidos\n",
             imdb, temp, achados);
      fflush(stdout);
      finalizar(imdb, temp);
      return NULL;
    }
    if (achados > 0) {
      printf("[serieaud] %s T%d: %d de %d episodios do cache; buscando o resto\n",
             imdb, temp, achados, nEps);
      fflush(stdout);
    } }

  // 2. CREDENCIAL. MEDIDO em 16/09/2026 contra a api: /stats responde com
  // APENAS `trakt-api-key` + `trakt-api-version`, sem Authorization — os dois
  // /stats daqui e o `seasons?extended=episodes,full` que da as notas devolvem
  // 200 sem token, e 403 sem chave nenhuma.
  //
  // A NOTA ANTERIOR AQUI DIZIA que a secao dependia do Trakt vinculado porque o
  // unico caminho para a chave do aplicativo era trakt_cabecalhos(), que exige
  // token. Isso deixou de ser verdade: trakt_cabecalhos_publicos() abre a chave
  // do pacote sozinha, e extras.c passou a pedir as notas por episodio pelo
  // mesmo caminho — ou seja, nao sobrou nada nestes tres paineis que precise de
  // conta. Com token o cabecalho completo continua ganhando: quem esta logado
  // nao tem razao para pedir anonimamente.
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave) &&
      !trakt_cabecalhos_publicos(cab, chave, sizeof chave)) {
    printf("[serieaud] sem chave do Trakt; a secao fica vazia\n");
    fflush(stdout);
    finalizar(imdb, temp);
    return NULL;
  }

  // 3. A SERIE INTEIRA: um pedido, para o indice de revisita do rodape. Nao
  // sai quando o cache ja trouxe o numero.
  if (!deveParar() && watchersSerie <= 0) {
    char *corpo;
    snprintf(url, sizeof url, "https://api.trakt.tv/shows/%s/stats", imdb);
    corpo = rede_baixar_com(url, 15, cab);
    if (corpo) {
      long pl = (long)js_num(corpo, NULL, "plays", -1.0);
      long wt = (long)js_num(corpo, NULL, "watchers", -1.0);
      free(corpo);
      pthread_mutex_lock(&trava);
      if (!strcmp(imdb, imdbPedido) && temp == tempPedida) {
        playsSerie = pl; watchersSerie = wt;
      }
      pthread_mutex_unlock(&trava);
    }
  }

  // 4. EPISODIO A EPISODIO, na ordem em que o grafico desenha.
  for (i = 0; i < nEps; i++) {
    char *corpo;
    SaEp d;
    int ep;
    if (deveParar()) break;
    pthread_mutex_lock(&trava);
    /* Ja veio do cache: nao ha o que pedir. */
    ep = eps[i].tem ? -1 : eps[i].ep;
    // Outro titulo foi aberto enquanto este estava no ar: o resultado nao
    // pertence mais a tela que esta la.
    if (strcmp(imdb, imdbPedido) || temp != tempPedida) ep = -2;
    pthread_mutex_unlock(&trava);
    if (ep == -2) break;      /* trocou de titulo: o resultado nao serve mais */
    if (ep < 0) continue;     /* este episodio ja estava no cache */
    memset(&d, 0, sizeof d);
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/seasons/%d/episodes/%d/stats",
             imdb, temp, ep);
    corpo = rede_baixar_com(url, 12, cab);
    if (corpo) { lerStats(corpo, &d); free(corpo); }
    pthread_mutex_lock(&trava);
    // Publica NA HORA: a curva cresce na tela em vez de aparecer de uma vez.
    if (!strcmp(imdb, imdbPedido) && temp == tempPedida && i < nEps && d.tem) {
      eps[i].tem = 1;
      eps[i].watchers = d.watchers;
      eps[i].plays = d.plays;
      eps[i].comentarios = d.comentarios;
      eps[i].votos = d.votos;
    }
    pthread_mutex_unlock(&trava);
  }

  // 5. GRAVA O QUE CONSEGUIU. Ate uma temporada interrompida vale cache: na
  // proxima visita os episodios que ja chegaram nao sao pedidos de novo.
  pthread_mutex_lock(&trava);
  if (!strcmp(imdb, imdbPedido) && temp == tempPedida) gravarCache();
  pthread_mutex_unlock(&trava);
  finalizar(imdb, temp);
  return NULL;
}

// --- API ---------------------------------------------------------------------

void serieaud_abrir(const char *imdb, int temporada, const int *epNum,
                    const int *notaDecimos, int n) {
  char id[24];
  int i, precisa = 0;
  soImdb(id, sizeof id, imdb);
  if (!id[0] || n <= 0) return;

  pthread_mutex_lock(&trava);
  if (!strcmp(id, imdbAtual) && temporada == temporadaAtual) {
    // Mesma temporada: nao refaz nada (nem o pedido, nem a selecao).
    pthread_mutex_unlock(&trava);
    return;
  }
  truncada = n > SA_EP_MAX;
  if (n > SA_EP_MAX) n = SA_EP_MAX;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < n; i++) {
    eps[i].ep   = epNum ? epNum[i] : i + 1;
    eps[i].nota = notaDecimos ? notaDecimos[i] : 0;
  }
  nEps = n;
  selecionado = 0;
  playsSerie = watchersSerie = -1;
  snprintf(imdbAtual, sizeof imdbAtual, "%s", id);
  temporadaAtual = temporada;
  snprintf(imdbPedido, sizeof imdbPedido, "%s", id);
  tempPedida = temporada;
  abandonar = 0;
  if (!fioVivo) {
    snprintf(imdbEmCurso, sizeof imdbEmCurso, "%s", id);
    tempEmCurso = temporada;
    fioVivo = 1;
    precisa = 1;
  }
  pthread_mutex_unlock(&trava);

  if (precisa) {
    if (pthread_create(&fio, NULL, buscar, NULL) != 0) {
      pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    } else pthread_detach(fio);
  }
}

void serieaud_fechar(void) {
  pthread_mutex_lock(&trava);
  abandonar = 1;
  pthread_mutex_unlock(&trava);
}

int serieaud_carregando(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = fioVivo;
  pthread_mutex_unlock(&trava);
  return v;
}

int serieaud_pronto(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = nEps > 0 && eps[0].tem && eps[0].watchers > 0;
  pthread_mutex_unlock(&trava);
  return v;
}

int  serieaud_n(void)          { return nEps; }
int  serieaud_temporada(void)  { return temporadaAtual; }
static int dentro(int i) { return i >= 0 && i < nEps; }
int  serieaud_ep(int i)        { return dentro(i) ? eps[i].ep : 0; }
int  serieaud_nota(int i)      { return dentro(i) ? eps[i].nota : 0; }
int  serieaud_tem_stats(int i) { return dentro(i) ? eps[i].tem : 0; }
long serieaud_watchers(int i)  { return dentro(i) ? eps[i].watchers : 0; }
long serieaud_plays(int i)     { return dentro(i) ? eps[i].plays : 0; }
int  serieaud_comentarios(int i){ return dentro(i) ? eps[i].comentarios : 0; }
int  serieaud_votos(int i)     { return dentro(i) ? eps[i].votos : 0; }

int serieaud_retencao(int i) {
  if (!dentro(i) || !eps[i].tem) return -1;
  if (!eps[0].tem || eps[0].watchers <= 0) return -1;
  // SEM GRAMPO EM 1000. Ver a nota em serieaud.h: E2 com mais watchers que E1
  // e um caso real e e justamente o que vale olhar.
  return (int)((eps[i].watchers * 1000 + eps[0].watchers / 2) / eps[0].watchers);
}

int serieaud_rever(int i) {
  if (!dentro(i) || !eps[i].tem || eps[i].watchers <= 0) return -1;
  return (int)((eps[i].plays * 100 + eps[i].watchers / 2) / eps[i].watchers);
}

int serieaud_rever_serie(void) {
  if (playsSerie < 0 || watchersSerie <= 0) return -1;
  return (int)((playsSerie * 100 + watchersSerie / 2) / watchersSerie);
}

int serieaud_nota_media(void) {
  long soma = 0;
  int i, n = 0;
  for (i = 0; i < nEps; i++) if (eps[i].nota > 0) { soma += eps[i].nota; n++; }
  return n ? (int)((soma + n / 2) / n) : 0;
}

int serieaud_melhor(void) {
  int i, m = -1;
  for (i = 0; i < nEps; i++)
    if (eps[i].nota > 0 && (m < 0 || eps[i].nota > eps[m].nota)) m = i;
  return m;
}

int serieaud_pior(void) {
  int i, m = -1;
  for (i = 0; i < nEps; i++)
    if (eps[i].nota > 0 && (m < 0 || eps[i].nota < eps[m].nota)) m = i;
  return m;
}

void serieaud_selecionar(int i) {
  if (i < 0) i = 0;
  if (i >= nEps) i = nEps - 1;
  selecionado = i < 0 ? 0 : i;
}
int serieaud_selecionado(void) { return selecionado; }

// --- PRIMITIVAS DE GRAFICO ---------------------------------------------------
//
// O gfx so desenha QUADS ALINHADOS AOS EIXOS — nao ha rotacao no shader. Uma
// poligonal inclinada, entao, e uma sequencia de retangulos verticais, e todo
// o desenho destes tres paineis sai daqui.
//
// O ORCAMENTO DE DESENHO, porque ele decide a forma. gfx.h registra o quadro
// inteiro da home em 123 desenhos e 1,9 ms de CPU — cerca de 0,015 ms por
// desenho. Amostrar uma curva por coluna de pixel daria ~500 desenhos so para
// ela, 7,7 ms, e isso sozinho estoura um quadro de 16 ms. MEDIDO nesta base
// antes desta passagem: a tela com arco + radar custava 580 desenhos, quase
// todos do arco (passo fixo de 4 px, um retangulo por amostra).
//
// A saida nao foi amostrar menos — amostra rala volta a fazer degrau. Foi
// separar AMOSTRAGEM de DESENHO: a curva e avaliada fino (3 px, custo zero em
// GL) e depois as amostras vizinhas sao JUNTADAS num retangulo so enquanto
// couberem numa faixa de SA_TOL px. Trecho quase horizontal vira um retangulo
// largo; trecho ingreme continua gastando um por degrau. A curva de retencao,
// que e quase reta, cai de centenas de desenhos para dezenas.

// RAIO DE CANTO EM PIXELS — e a armadilha que ele esconde.
//
// gfx_cor recebe o raio como FRACAO DA ALTURA do retangulo, e nao do menor
// lado. Esta na conta do fragmento (FS_SDF em gfx.c): ele normaliza com
// `p = (uv - 0.5) * vec2(w/h, 1.0)`, entao a meia-extensao VERTICAL e sempre
// 0.5 e o raio em pixels e `raio * h`, qualquer que seja a largura.
//
// Por isso o idioma "px / min(w,h)" NAO da pixel constante. Numa barra de
// 34 px de largura por 76 de altura, 6/34 pede 0,18 DA ALTURA — 13 px; na
// barra baixa ao lado a mesma conta da ~6. Mesma fileira de barras, dois
// formatos: capsula numa ponta, quase quadrado na outra. E o defeito que o
// dono fotografou no grafico miniatura de novidades11.c, e o painel 3 daqui
// tinha a versao dele (raio 0.18 fixo sobre barras de altura variavel).
//
// Os dois tetos: 0.5 e a capsula vertical, acima dela o SDF degenera; e
// 0.5*w/h impede que um retangulo mais largo que alto fique com o canto
// horizontal quadrado enquanto o vertical ja arredondou.
static float raioPx(float w, float h, float px) {
  float t;
  if (h <= 0.0f) return 0.0f;
  t = px / h;
  if (t > 0.5f) t = 0.5f;
  if (w > 0.0f && t > 0.5f * w / h) t = 0.5f * w / h;
  return t;
}

// A `linha` reta que existia aqui SAIU. Ela so era usada como caso especial de
// dois pontos da curva; a Hermite monotona ja trata n = 2 (com m0 = m1 = a
// secante, ou seja, exatamente a reta), entao manter as duas era manter dois
// caminhos que tinham de concordar. O vao entre trechos, que era o outro uso
// possivel, agora e pontilhado de proposito — ver `pontilhada`.

// A CURVA SUAVE — e por que ela deixou de ser Catmull-Rom.
//
// Catmull-Rom uniforme passa exatamente pelos pontos, e foi por isso que
// entrou aqui. Mas ela ULTRAPASSA entre eles: com as notas de breaking-bad T2
// (8.4 no E2, 7.9 no E3, 7.8 no E4) a spline desce, entre o E3 e o E4, ABAIXO
// de 7.8 — mais fundo do que qualquer nota publicada da temporada. Isso esta
// na captura /tmp/nuvio-serieaud-bb.bmp desta base como uma onda que o dado
// nao tem, e e exatamente o que serieaud.h proibe: metadado inventado. So que
// inventado por spline, que e mais dificil de ver do que inventado por texto.
//
// A troca e HERMITE CUBICA MONOTONA (Fritsch-Carlson, 1980). Mesmas duas
// garantias que valiam: passa pelos pontos e chega neles com tangente dada
// pelos vizinhos. Mais uma, que e a que importa: entre dois episodios a curva
// NUNCA sai do intervalo [nota_i, nota_i+1]. Onde o dado sobe ela sobe, onde o
// dado desce ela desce, e onde ele vira ela tem tangente zero. O preco e uma
// curva um pouco menos "desenhada" nos vales — que e o preco certo.
//
// O algoritmo: inclinacao inicial = media das secantes vizinhas; depois cada
// par (m_i, m_i+1) e projetado para dentro do circulo de raio 3 da secante.
// Fora desse circulo e onde nasce a ultrapassagem.
//
// O BURACO CONTINUA BURACO: quem chama passa um TRECHO CONTIGUO de episodios
// com dado. Episodio sem dado corta o trecho, e o vao entre dois trechos e
// LIGADO POR PONTILHADO, nunca por curva — ver `pontilhada`.
#define SA_PASSO 3.0f
#define SA_TOL   1.5f
#define SA_AMOSTRAS 768

// Buffer das amostras. Estatico pelo mesmo motivo do resto do arquivo (o alvo
// Tizen ja estourou o heap uma vez) e porque so o laco de desenho o usa, um
// painel de cada vez.
static float saY[SA_AMOSTRAS];

static void inclinacoes(const float *ys, int n, float *m) {
  float d[SA_EP_MAX];
  int i;
  for (i = 0; i < n - 1; i++) d[i] = ys[i + 1] - ys[i];
  m[0] = d[0];
  m[n - 1] = d[n - 2];
  for (i = 1; i < n - 1; i++) m[i] = (d[i - 1] + d[i]) * 0.5f;
  for (i = 0; i < n - 1; i++) {
    float a, b, s;
    // Secante nula: o trecho e plano, e as duas pontas tem de ser planas
    // tambem. Sem isto a curva faz um S entre dois episodios de mesma nota.
    if (d[i] == 0.0f) { m[i] = 0.0f; m[i + 1] = 0.0f; continue; }
    a = m[i] / d[i];
    b = m[i + 1] / d[i];
    if (a < 0.0f) { m[i] = 0.0f; a = 0.0f; }     // extremo local: tangente 0
    if (b < 0.0f) { m[i + 1] = 0.0f; b = 0.0f; }
    s = a * a + b * b;
    if (s > 9.0f) {
      float f = 3.0f / sqrtf(s);
      m[i]     = f * a * d[i];
      m[i + 1] = f * b * d[i];
    }
  }
}

static float hermite(float y0, float y1, float m0, float m1, float t) {
  float t2 = t * t, t3 = t2 * t;
  return (2.0f * t3 - 3.0f * t2 + 1.0f) * y0 + (t3 - 2.0f * t2 + t) * m0 +
         (-2.0f * t3 + 3.0f * t2) * y1 + (t3 - t2) * m1;
}

// Avalia a curva em saY[]. Devolve o numero de amostras e escreve em *dx o
// espacamento delas em pixels. NENHUM desenho acontece aqui: amostrar e de
// graca, desenhar e que custa.
static int amostrar(float passoX, const float *ys, int n, float *dx) {
  float m[SA_EP_MAX];
  float largura = passoX * (float)(n - 1);
  int amostras, k;
  if (n < 2 || largura <= 0.0f) return 0;
  amostras = (int)(largura / SA_PASSO) + 1;
  if (amostras < 2) amostras = 2;
  if (amostras > SA_AMOSTRAS) amostras = SA_AMOSTRAS;
  inclinacoes(ys, n, m);
  for (k = 0; k < amostras; k++) {
    float u = (float)k / (float)(amostras - 1) * (float)(n - 1);
    int i = (int)u;
    float t;
    if (i > n - 2) { i = n - 2; t = 1.0f; } else t = u - (float)i;
    saY[k] = hermite(ys[i], ys[i + 1], m[i], m[i + 1], t);
  }
  *dx = largura / (float)(amostras - 1);
  return amostras;
}

// Desenha saY[0..n-1] como traco, juntando amostras vizinhas enquanto elas
// couberem numa faixa de SA_TOL px. O `j > i` garante que todo retangulo
// avanca pelo menos uma amostra, entao um trecho vertical nao trava o laco nem
// abre buraco: ele vira um retangulo alto, como em `linha`.
static void traco(float x0, float dx, int n, float esp,
                  float r, float g, float b, float a) {
  int i = 0;
  if (n < 2) return;
  while (i < n - 1) {
    float lo = saY[i], hi = saY[i];
    int j = i;
    while (j < n - 1) {
      float l = saY[j + 1] < lo ? saY[j + 1] : lo;
      float h = saY[j + 1] > hi ? saY[j + 1] : hi;
      if (j > i && h - l > SA_TOL) break;
      lo = l; hi = h; j++;
    }
    { GfxRect s = { x0 + dx * (float)i, lo - esp * 0.5f,
                    dx * (float)(j - i) + 1.0f, (hi - lo) + esp };
      gfx_cor(s, 0.0f, r, g, b, a); }
    i = j;
  }
}

static void ponto(float x, float y, float d, float r, float g, float b, float a) {
  GfxRect p = { x - d * 0.5f, y - d * 0.5f, d, d };
  gfx_cor(p, 0.5f, r, g, b, a);
}

// LIGACAO PONTILHADA entre dois trechos da curva.
//
// O vao entre episodios sem dado era deixado VAZIO, e o resultado na captura
// nao lia como "nao publicado": lia como falha de desenho — dois pedacos de
// curva soltos na caixa. Pontilhado e o contrario disso: e uma marca
// deliberada, com vocabulario proprio (fino, apagado, redondo), que qualquer
// pessoa le como "aqui nao se sabe". E ele nao afirma valor nenhum, porque
// nao ha traco continuo em altura nenhuma — so bolinhas espacadas.
//
// A coluna do episodio sem dado ainda recebe um veu proprio no painel, para
// dizer QUAL episodio faltou, e nao so que faltou algum.
static void pontilhada(float x0, float y0, float x1, float y1, float d,
                       float r, float g, float b, float a) {
  float dx = x1 - x0, dy = y1 - y0;
  float comp = sqrtf(dx * dx + dy * dy);
  int n = (int)(comp / 20.0f), k;
  if (n < 2) n = 2;
  if (n > 40) n = 40;
  for (k = 1; k < n; k++) {
    float t = (float)k / (float)n;
    ponto(x0 + dx * t, y0 + dy * t, d, r, g, b, a);
  }
}

// Linha horizontal tracejada — a referencia (media, 100%) nao pode ser lida

// Numero grande em forma curta. "388 mil", "24.1 mi".
static void curto(char *dst, unsigned tam, long v) {
  if (v >= 1000000L) { snprintf(dst, tam, i18n("%.1f mi"), v / 1000000.0); idioma_decimal_texto(dst, ajustes_idioma()); }
  else if (v >= 10000L) snprintf(dst, tam, i18n("%ld mil"), (v + 500) / 1000);
  else snprintf(dst, tam, "%ld", v);
}

enum { SA_NOTA, SA_RET, SA_REVER, SA_CONV, SA_NEIXOS };

static const struct { float r, g, b; const char *nome; } EIXO[SA_NEIXOS] = {
  { 0.88f, 0.89f, 0.93f, "Nota" },       // a celula usa a rampa das notas
  { 0.263f, 0.827f, 0.620f, "Retenção" }, // #43d39e
  { 0.941f, 0.541f, 0.294f, "Rever" },    // #f08a4b
  { 0.486f, 0.769f, 1.0f, "Conversa" }    // #7cc4ff
};

// Valor bruto do eixo para um episodio; -1 quando nao ha dado.
static int eixoBruto(int i, int eixo) {
  switch (eixo) {
    case SA_NOTA:  return eps[i].nota > 0 ? eps[i].nota : -1;
    case SA_RET:   return serieaud_retencao(i);
    case SA_REVER: return serieaud_rever(i);
    case SA_CONV:  return eps[i].tem ? eps[i].comentarios + eps[i].votos : -1;
  }
  return -1;
}


// --- BLOCO "NUMEROS DA TEMPORADA" (Glass UI 1.8) -----------------------------
//
// Os tres graficos numa tela so, logo abaixo das Notas da serie (mockup "Notas
// e graficos" aprovado pelo dono): RETENCAO e IMPRESSAO DIGITAL a esquerda,
// NOTAS POR EPISODIO (notasui_mapa_card) a direita. Cada cartao tem um numero
// ou uma frase que explica, e o episodio em foco (esquerda/direita no bloco)
// aparece destacado nos tres ao mesmo tempo.
//
// O "Arco de qualidade" saiu como painel proprio: a nota do Trakt por episodio
// continua na fileira "Nota" da impressao digital e no mapa de notas.
//
// AS FONTES NAO MUDARAM: retencao = watchers/watchers(E1) do /stats, rever =
// plays/watchers, conversa = comments+votes, nota = rating do Trakt. Cartao sem
// dado diz que nao ha (ou que ainda vem), nunca desenha numero inventado.
#define BL_CAB      104.0f    // titulo + linha de apoio
#define BL_GAP       24.0f
#include "telefoneui.h"
static int blocoEmpilhado(void) { return telefoneui_ativo() && NV_TELA_W < 1500; }
#define BL_ESQ_W    (blocoEmpilhado() ? NV_TELA_W - 192.0f : 884.0f)
#define BL_RET_H    396.0f
#define BL_DIG_H    380.0f
#define BL_PAD       34.0f
#define BL_RAIO      30.0f
#define BL_CEL_H     36.0f
#define BL_CEL_GAP    6.0f

static TextoGate gateBloco;
void serieaud_bloco_reiniciar(void) { textogate_reiniciar(&gateBloco); }
float serieaud_bloco_altura(void) {
  return BL_CAB + BL_RET_H + BL_GAP + BL_DIG_H + (blocoEmpilhado() ? BL_GAP + 480 : 0);
}

// Frase de estado de um cartao sem dado: tres situacoes diferentes, e dizer
// uma pela outra faz a secao parecer quebrada.
static const char *estadoVazio(int aberto) {
  if (!aberto) return "Uma consulta por episódio: carrega quando você desce até aqui";
  return serieaud_carregando() ? "Carregando…" : "Sem dados desta temporada";
}
static void mensagem(float x, float y, float w, const char *s, float a) {
  txt_bloco_corta(TXT_BODY, i18n(s), 150, 153, 162, x, y, w, 34.0f, a, 2);
}

// Area sob a curva ate `base`, ou so `prof` px abaixo dela (prof > 0). Em
// colunas de pixel inteiro, sem sobreposicao (a dupla pintura riscava o veu).
// Junta amostras numa folga de 6 px: a 5,5% de alfa o degrau nao se ve, e o
// custo cai de ~50 para ~12 retangulos por camada.
// Cinco camadas destas (base, 90, 56, 32, 14 px) fazem o degrade do mockup: o gfx
// nao tem degrade colorido que comece opaco no alto.
static void areaSob(float x0, float dx, int n, float base, float prof,
                    float r, float g, float b, float a) {
  int i = 0;
  while (i < n - 1) {
    float lo = saY[i], hi = saY[i], xa, xb, top, bot;
    int j = i;
    while (j < n - 1) {
      float l = saY[j + 1] < lo ? saY[j + 1] : lo;
      float h = saY[j + 1] > hi ? saY[j + 1] : hi;
      if (j > i && h - l > 6.0f) break;
      lo = l; hi = h; j++;
    }
    xa = floorf(x0 + dx * (float)i + 0.5f);
    xb = floorf(x0 + dx * (float)j + 0.5f);
    top = (lo + hi) * 0.5f;     // +-3 px, escondido sob o traco de 4 px
    bot = prof > 0.0f ? top + prof : base;
    if (bot > base) bot = base;
    if (xb > xa && bot > top) gfx_cor((GfxRect){ xa, top, xb - xa, bot - top }, 0.0f, r, g, b, a);
    i = j;
  }
}

#define VERDE_R 0.263f
#define VERDE_G 0.827f
#define VERDE_B 0.620f

static void cardRetencao(GfxRect c, const SaBloco *b, int usar, float a) {
  float px = c.x + BL_PAD, pw = c.w - BL_PAD * 2.0f, py = c.y + 30.0f;
  float gx, gw, top, bot, passo;
  int i, ult = -1, lo = 0x7fffffff, hi = -1, queda = 0, quedaI = -1;
  char txt[160], s1[32], s2[32];
  TxtLinha lt = txt_linha(TXT_G26B, "Retenção", 245, 245, 245, 255);
  notasui_painel(c, BL_RAIO, a);
  txt_desenhar_alpha(lt, px, py, a);
  py += (float)lt.h + 14.0f;
  if (!usar || !serieaud_pronto()) { mensagem(px, py, pw, estadoVazio(b->aberto), a); return; }
  for (i = nEps - 1; i >= 0; i--) if (eps[i].tem) { ult = i; break; }
  curto(s1, sizeof s1, eps[0].watchers);
  if (ult <= 0) {
    snprintf(txt, sizeof txt, i18n("%s pessoas no E%d"), s1, eps[0].ep);
    { TxtLinha l = txt_linha(TXT_BODY, txt, 200, 204, 212, 255);
      txt_desenhar_alpha(l, px, py, a); }
    return;
  }
  // O NUMERO DE DESTAQUE e as duas linhas que dizem de onde ele vem.
  { TxtLinha big, l1, l2;
    float tx;
    snprintf(txt, sizeof txt, "%d%%", (serieaud_retencao(ult) + 5) / 10);
    big = txt_linha(TXT_AJ_NUM58, txt, 245, 245, 245, 255);
    txt_desenhar_alpha(big, px, py, a);
    tx = px + (float)big.w + 26.0f;
    snprintf(txt, sizeof txt, i18n("de quem marcou o E%d chegou ao E%d"), eps[0].ep, eps[ult].ep);
    l1 = txt_linha_corta(TXT_ILHA_SUB, txt, 200, 204, 212, 255, px + pw - tx);
    curto(s2, sizeof s2, eps[ult].watchers);
    snprintf(txt, sizeof txt, i18n("%s pessoas no E%d  ·  %s no E%d"), s1, eps[0].ep, s2, eps[ult].ep);
    l2 = txt_linha_corta(TXT_ILHA_SUB, txt, 150, 153, 162, 255, px + pw - tx);
    { float yc = py + (float)big.h * 0.5f, hh = (float)(l1.h + l2.h) + 4.0f;
      txt_desenhar_alpha(l1, tx, yc - hh * 0.5f, a);
      txt_desenhar_alpha(l2, tx, yc - hh * 0.5f + (float)l1.h + 4.0f, a); }
    py += (float)big.h + 12.0f; }

  // A CURVA. Eixo da menor a maior retencao com folga: comecar em zero achata
  // a temporada inteira numa reta (a queda tipica e de poucos pontos).
  for (i = 0; i < nEps; i++) {
    int v = serieaud_retencao(i);
    if (v < 0) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  { int folga = (hi - lo) / 8 + 6; lo -= folga; hi += folga; }
  gx = px + 12.0f; gw = pw - 24.0f;
  top = py + 30.0f; bot = c.y + c.h - 30.0f - 30.0f;
  passo = nEps > 1 ? gw / (float)(nEps - 1) : gw;
#define BL_RY(v) (top + (bot - top) * (float)(hi - (v)) / (float)(hi - lo))
  gfx_cor((GfxRect){ gx, bot, gw, 1.0f }, 0.0f, 1, 1, 1, 0.12f * a);
  // O episodio em foco: um fio vertical claro, o mesmo episodio dos outros dois cartoes.
  if (dentro(b->sel)) {
    float xs = gx + passo * (float)b->sel;
    gfx_cor((GfxRect){ xs - 1.0f, top - 10.0f, 2.0f, bot - top + 10.0f }, 0.0f, 1, 1, 1, 0.18f * a);
  }
  { float ys[SA_EP_MAX];
    int m = 0, inicio = 0, fimAnt = -1;
    for (i = 0; i <= nEps; i++) {
      int v = i < nEps ? serieaud_retencao(i) : -1;
      if (v >= 0) { if (!m) inicio = i; ys[m++] = BL_RY(v); continue; }
      if (m >= 1) {
        if (fimAnt >= 0)
          pontilhada(gx + passo * (float)fimAnt, BL_RY(serieaud_retencao(fimAnt)),
                     gx + passo * (float)inicio, ys[0], 5.0f, VERDE_R, VERDE_G, VERDE_B, 0.45f * a);
        if (m >= 2) {
          float dx, x0 = gx + passo * (float)inicio;
          int na = amostrar(passo, ys, m, &dx);
          { static const float PROF[5] = { 0.0f, 90.0f, 56.0f, 32.0f, 14.0f };
            int k;
            for (k = 0; k < 5; k++)
              areaSob(x0, dx, na, bot, PROF[k], VERDE_R, VERDE_G, VERDE_B,
                      (k ? 0.055f : 0.06f) * a); }
          traco(x0, dx, na, 4.0f, VERDE_R, VERDE_G, VERDE_B, a);
        }
        fimAnt = inicio + m - 1;
      }
      m = 0;
    } }
  ponto(gx, BL_RY(serieaud_retencao(0)), 12.0f, VERDE_R, VERDE_G, VERDE_B, a);
  ponto(gx + passo * (float)ult, BL_RY(serieaud_retencao(ult)), 12.0f, VERDE_R, VERDE_G, VERDE_B, a);
  if (dentro(b->sel) && serieaud_retencao(b->sel) >= 0)
    ponto(gx + passo * (float)b->sel, BL_RY(serieaud_retencao(b->sel)), 14.0f, 1, 1, 1, a);

  // A MAIOR QUEDA entre episodios vizinhos com dado, anotada. Abaixo de 1 pp
  // nao ha o que anotar.
  { int ant = -1;
    for (i = 0; i < nEps; i++) {
      int v = serieaud_retencao(i);
      if (v < 0) { ant = -1; continue; }
      if (ant >= 0 && serieaud_retencao(ant) - v > queda) { queda = serieaud_retencao(ant) - v; quedaI = i; }
      ant = i;
    } }
  if (quedaI > 0 && queda >= 10) {
    float xq = gx + passo * (float)quedaI, yq = BL_RY(serieaud_retencao(quedaI));
    TxtLinha l;
    float lx, ly;
    gfx_anel((GfxRect){ xq - 10.0f, yq - 10.0f, 20.0f, 20.0f }, 0.5f, 3.0f,
             0.941f, 0.541f, 0.294f, a);
    snprintf(txt, sizeof txt, i18n("maior queda: E%d → E%d, -%.1f pp"),
             eps[quedaI - 1].ep, eps[quedaI].ep, queda / 10.0);
    idioma_decimal_texto(txt, ajustes_idioma());
    l = txt_linha(TXT_LOG_18B, txt, 240, 138, 75, 255);
    lx = xq + 16.0f;
    if (lx + (float)l.w > px + pw) lx = xq - 16.0f - (float)l.w;
    if (lx < px) lx = px;
    ly = yq - 14.0f - (float)l.h;
    if (ly < top - 30.0f) ly = top - 30.0f;
    txt_desenhar_alpha(l, lx, ly, a);
  }
  // Eixo de episodios: todos quando cabem, senao de k em k (o em foco sempre).
  { int k = 1;
    float larg = (float)txt_largura(TXT_ILHA_NUM, "E24") + 14.0f;
    while (passo * (float)k < larg && k < nEps) k++;
    for (i = 0; i < nEps; i++) {
      int sel = i == b->sel;
      TxtLinha l;
      if (!sel && i % k && i != nEps - 1) continue;
      if (!sel && dentro(b->sel) && i != b->sel &&
          fabsf((float)(i - b->sel)) * passo < larg) continue;
      snprintf(txt, sizeof txt, "E%d", eps[i].ep);
      l = sel ? txt_linha(TXT_LOG_18B, txt, 245, 245, 245, 255)
              : txt_linha(TXT_ILHA_NUM, txt, 130, 134, 142, 255);
      txt_desenhar_alpha(l, gx + passo * (float)i - (float)l.w * 0.5f, bot + 10.0f, a);
    } }
#undef BL_RY
}

static void cardDigital(GfxRect c, const SaBloco *b, int usar, float a) {
  float px = c.x + BL_PAD, pw = c.w - BL_PAD * 2.0f, py = c.y + 30.0f;
  float labW = 130.0f, gx = px + labW, gw = pw - labW, cw;
  int mini[SA_NEIXOS], maxi[SA_NEIXOS];
  int i, e, n = 0, passoNum = 1;
  char txt[160];
  TxtLinha lt;
  snprintf(txt, sizeof txt, i18n("Impressão digital · E%d"), b->selEp);
  lt = txt_linha(TXT_G26B, txt, 245, 245, 245, 255);
  notasui_painel(c, BL_RAIO, a);
  txt_desenhar_alpha(lt, px, py, a);
  py += (float)lt.h + 14.0f;
  if (usar)
    for (i = 0; i < nEps; i++)
      for (e = 0; e < SA_NEIXOS; e++) if (eixoBruto(i, e) >= 0) { n++; e = SA_NEIXOS; }
  if (!usar || n < 1) { mensagem(px, py, pw, estadoVazio(b->aberto), a); return; }
  for (e = 0; e < SA_NEIXOS; e++) { mini[e] = 0x7fffffff; maxi[e] = -1; }
  for (i = 0; i < nEps; i++)
    for (e = 0; e < SA_NEIXOS; e++) {
      int v = eixoBruto(i, e);
      if (v < 0) continue;
      if (v < mini[e]) mini[e] = v;
      if (v > maxi[e]) maxi[e] = v;
    }
  cw = (gw - BL_CEL_GAP * (float)(nEps - 1)) / (float)nEps;
  if (cw > 90.0f) cw = 90.0f;
  while (cw * (float)passoNum < 26.0f && passoNum < nEps) passoNum++;
  // Cabecalho: o numero de cada episodio, o em foco em branco forte.
  for (i = 0; i < nEps; i++) {
    float cx = gx + (cw + BL_CEL_GAP) * (float)i + cw * 0.5f;
    int sel = i == b->sel;
    TxtLinha l;
    if (!sel && i % passoNum) continue;
    snprintf(txt, sizeof txt, "%d", eps[i].ep);
    l = sel ? txt_linha(TXT_LOG_18B, txt, 245, 245, 245, 255)
            : txt_linha(TXT_V2_18, txt, 115, 120, 130, 255);
    txt_desenhar_alpha(l, cx - (float)l.w * 0.5f, py, a);
  }
  py += 30.0f;
  // Uma fileira por medida. A cor (ou a opacidade) e a posicao do episodio
  // DENTRO DA TEMPORADA naquela medida: quatro unidades diferentes nao se
  // comparam de outro jeito sem mentir sobre alguma delas.
  for (e = 0; e < SA_NEIXOS; e++) {
    float ry = py + (BL_CEL_H + BL_CEL_GAP) * (float)e;
    TxtLinha l = txt_linha(TXT_V2_18, EIXO[e].nome, 180, 184, 192, 255);
    txt_desenhar_alpha(l, px, ry + (BL_CEL_H - (float)l.h) * 0.5f, a);
    for (i = 0; i < nEps; i++) {
      GfxRect q = { gx + (cw + BL_CEL_GAP) * (float)i, ry, cw, BL_CEL_H };
      float rr = raioPx(q.w, q.h, 9.0f);
      int v = eixoBruto(i, e);
      if (v < 0) gfx_cor(q, rr, 1, 1, 1, 0.06f * a);
      else if (e == SA_NOTA) {
        float cr, cg, cb;
        notasui_cor_rampa(v, &cr, &cg, &cb);
        gfx_cor(q, rr, cr, cg, cb, a);
      } else {
        float t = maxi[e] > mini[e] ? (float)(v - mini[e]) / (float)(maxi[e] - mini[e]) : 1.0f;
        gfx_cor(q, rr, EIXO[e].r, EIXO[e].g, EIXO[e].b, (0.30f + 0.70f * t) * a);
      }
      if (i == b->sel) gfx_anel_fora(q, rr, 2.0f, 2.0f, 1, 1, 1, 0.85f * a);
    }
  }
  py += (BL_CEL_H + BL_CEL_GAP) * (float)SA_NEIXOS - BL_CEL_GAP + 18.0f;
  // O episodio em foco em uma frase: quanta gente e o que ele tem de unico.
  if (dentro(b->sel)) {
    int s = b->sel, mr = -1, mc = -1, com = 0;
    const char *sup = NULL;
    TxtLinha l;
    if (eps[s].tem) {
      char q[32];
      curto(q, sizeof q, eps[s].watchers);
      snprintf(txt, sizeof txt, i18n("E%d · %s pessoas marcaram no Trakt"), eps[s].ep, q);
    } else snprintf(txt, sizeof txt, i18n("E%d · sem dados de audiência"), eps[s].ep);
    l = txt_linha_corta(TXT_ILHA_SUB, txt, 210, 214, 222, 255, pw);
    txt_desenhar_alpha(l, px, py, a);
    py += (float)l.h + 4.0f;
    for (i = 0; i < nEps; i++) {
      if (!eps[i].tem) continue;
      com++;
      if (mr < 0 || serieaud_rever(i) > serieaud_rever(mr)) mr = i;
      if (mc < 0 || eixoBruto(i, SA_CONV) > eixoBruto(mc, SA_CONV)) mc = i;
    }
    // So com tres episodios ou mais: "o mais revisto" de dois nao diz nada.
    if (com >= 3 && eps[s].tem) {
      if (mr == s && mc == s) sup = "O mais revisto e o mais comentado da temporada.";
      else if (mr == s) sup = "O mais revisto da temporada.";
      else if (mc == s) sup = "O mais comentado da temporada.";
    }
    if (!sup && serieaud_melhor() == s) {
      int comNota = 0;
      for (i = 0; i < nEps; i++) if (eps[i].nota > 0) comNota++;
      if (comNota >= 3) sup = "A maior nota da temporada.";
    }
    if (sup) {
      l = txt_linha_corta(TXT_ILHA_SUB, sup, 150, 153, 162, 255, pw);
      txt_desenhar_alpha(l, px, py, a);
    }
  }
}

float serieaud_bloco(float x, float y, const SaBloco *b, float a) {
  float alt = serieaud_bloco_altura(), aa, y0 = y + BL_CAB;
  int pend0 = txt_pendentes, aberto, usar;
  char txt[200];
  if (y >= NV_TELA_H || y + alt <= 0.0f) return alt;
  aberto = textogate_aberto(&gateBloco);
  aa = aberto ? a * textogate_passo(&gateBloco, 0, SDL_GetTicks()) : NV_TXTGATE_AQUECER;
  usar = b->aberto && nEps > 0 && serieaud_temporada() == b->temporada;
  { TxtLinha l;
    snprintf(txt, sizeof txt, i18n("Números da temporada %d"), b->temporada);
    l = txt_linha(TXT_HEADLINE, txt, 245, 248, 255, 255);
    txt_desenhar_alpha(l, x, y, aa);
    // A linha de apoio e o indice da SERIE INTEIRA, dito pelo que ele e: o
    // Trakt nao publica reproducoes por temporada.
    if (usar && serieaud_rever_serie() > 0) {
      char s1[32], s2[32];
      TxtLinha ls;
      curto(s1, sizeof s1, playsSerie);
      curto(s2, sizeof s2, watchersSerie);
      snprintf(txt, sizeof txt,
               i18n("Série inteira: %s reproduções para %s pessoas — %.1f por espectador, somando todos os episódios"),
               s1, s2, serieaud_rever_serie() / 100.0);
      idioma_decimal_texto(txt, ajustes_idioma());
      ls = txt_linha_corta(TXT_DET_META2, txt, 180, 184, 192, 255, NV_TELA_W - 2.0f * x);
      txt_desenhar_alpha(ls, x, y + (float)l.h + 8.0f, aa);
    } }
  cardRetencao((GfxRect){ x, y0, BL_ESQ_W, BL_RET_H }, b, usar, aa);
  cardDigital((GfxRect){ x, y0 + BL_RET_H + BL_GAP, BL_ESQ_W, BL_DIG_H }, b, usar, aa);
  notasui_mapa_card(b->notas,
                    (GfxRect){ blocoEmpilhado() ? x : x + BL_ESQ_W + BL_GAP,
                               y0 + (blocoEmpilhado() ? BL_RET_H + BL_GAP + BL_DIG_H + BL_GAP : 0),
                               blocoEmpilhado() ? BL_ESQ_W : NV_TELA_W - 2.0f * x - BL_ESQ_W - BL_GAP,
                               blocoEmpilhado() ? 480 : BL_RET_H + BL_GAP + BL_DIG_H },
                    b->tempIdx, b->sel, a);
  if (!aberto) textogate_passo(&gateBloco, txt_pendentes - pend0, SDL_GetTicks());
  return alt;
}
