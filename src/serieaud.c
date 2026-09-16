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

// A AREA ENTRE A CURVA E OS 100% EXISTIU AQUI, e foi trocada por HASTES.
//
// A ideia era boa e continua valendo: o que interessa neste painel nao e a
// altura da linha, e a DISTANCIA dela ate os 100%, porque essa distancia E a
// gente que foi embora. Pintar essa distancia responde a pergunta do painel
// sem que ninguem precise medir nada com os olhos.
//
// O que nao funcionou foi a area cheia. Ela so pode existir onde ha dado, e
// uma temporada com dois buracos (DS9 T1) virava tres blocos de aresta viva
// dentro da caixa — a mesma "coluna quebrada" que este painel ja tinha levado
// bronca por ter. Medido na captura v3: o degrau vertical na retomada de um
// trecho tinha 55 px de altura e lia como borda de objeto, nao como sombra de
// curva.
//
// HASTE resolve os tres problemas de uma vez. Ela mede a mesma distancia, uma
// por episodio; nao tem aresta horizontal para formar bloco; num buraco ela
// simplesmente nao existe, sem deixar borda; e custa UM desenho por episodio
// em vez de uma centena por trecho — o painel inteiro caiu de ~100 desenhos de
// area para 12 ou 24 de haste.
static void haste(float x, float yRef, float yPonto, float r, float g, float b,
                  float a) {
  float topo = yRef < yPonto ? yRef : yPonto;
  float alt  = fabsf(yPonto - yRef);
  GfxRect s;
  if (alt < 1.0f) return;
  s.x = x - 1.25f; s.y = topo; s.w = 2.5f; s.h = alt;
  gfx_cor(s, 0.0f, r, g, b, a);
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
// como mais uma serie de dados.
static void tracejada(float x, float y, float w, float r, float g, float b,
                      float a) {
  float passo = 22.0f, k;
  for (k = 0; k < w; k += passo) {
    float larg = passo * 0.55f;
    GfxRect s;
    if (k + larg > w) larg = w - k;
    s.x = x + k; s.y = y; s.w = larg; s.h = 2.0f;
    gfx_cor(s, 0.0f, r, g, b, a);
  }
}

// A CAIXA DO GRAFICO, e por que ela existe.
//
// Os tres paineis eram desenhados direto sobre o fundo da pagina (#0D0D0D) com
// traco cinza e rotulo cinza: "cinza sobre cinza, sem estilo e sem
// hierarquia", nas palavras do dono. O conserto nao e pintar a curva de uma
// cor mais forte — e dar a ela uma SUPERFICIE, que e como o resto desta base
// separa camadas (ver a escada de superficies em DESIGN.md; a profundidade
// aqui e tonal, nao por sombra).
//
// #1B1C1F e a mesma superficie de painel que novidades11.c ja usa nos graficos
// miniatura, copiada e nao escolhida, para que os dois tamanhos do mesmo
// grafico sejam o mesmo objeto.
//
// CUSTO: 1 desenho por painel, e ~0,15 tela de preenchimento. Cabe: gfx.h
// registra a tela de detalhe em 2,2-3,5 telas e o limite medido desta Mali e
// nas DUAS telas cheias empilhadas, que isto nao chega perto.
static void caixa(GfxRect r) {
  gfx_cor(r, raioPx(r.w, r.h, 18.0f), 0.105f, 0.108f, 0.122f, 1.0f);
}

// Marca da coluna sem dado, dentro da caixa: diz QUAL episodio faltou, ja que
// o pontilhado da curva so diz que faltou algum.
//
// E UM FIO, e nao um veu largo. A primeira versao era uma faixa de meio passo
// de largura com 4,5% de branco, e na captura ela virava a coisa mais clara do
// painel — mais visivel que a propria curva, e com a mesma cara de "laje
// vertical com vao ao lado" que o dono reclamou do desenho antigo. Ausencia de
// dado nao pode ter mais peso visual que o dado. Dois pixels bastam para dizer
// onde, e o pontilhado da curva ja diz o que.
static void colunaVazia(float cx, float y, float h) {
  GfxRect s = { cx - 1.0f, y, 2.0f, h };
  gfx_cor(s, 0.0f, 1.0f, 1.0f, 1.0f, 0.13f);
}

// Numero grande em forma curta. "388 mil", "24.1 mi".
static void curto(char *dst, unsigned tam, long v) {
  if (v >= 1000000L) snprintf(dst, tam, i18n("%.1f mi"), v / 1000000.0);
  else if (v >= 10000L) snprintf(dst, tam, i18n("%ld mil"), (v + 500) / 1000);
  else snprintf(dst, tam, "%ld", v);
}

// Cabecalho comum aos tres: titulo grande e, LOGO ABAIXO, a procedencia do
// numero. A segunda linha nao e enfeite — e a unica coisa que impede o grafico
// de virar uma afirmacao sem dono.
// A folga de 30 no fim e o respiro ate o topo da CAIXA. Os rotulos do eixo
// ficam agora na calha da esquerda, alinhados a direita, e nao mais centrados
// na primeira linha do grafico: era dali que vinha o "7.8E1 · 7.6" da captura,
// o rotulo do teto do eixo e o rotulo do extremo do E1 no mesmo lugar.
static float cabecalho(GfxRect r, const char *titulo, const char *fonte) {
  TxtLinha t = txt_linha(TXT_HEADLINE, i18n(titulo), 255, 255, 255, 255);
  TxtLinha f = txt_linha_corta(TXT_DET_META2, i18n(fonte), 150, 153, 162, 255, r.w);
  txt_desenhar(t, r.x, r.y);
  txt_desenhar(f, r.x, r.y + t.h + 2.0f);
  return t.h + f.h + 30.0f;
}

// Rotulo de eixo na calha ESQUERDA, alinhado a direita e centrado na altura
// dada. Alinhar a direita e o que faz "105%" e "90%" formarem uma coluna em
// vez de duas larguras soltas.
static void rotuloEixo(float dir, float ycentro, const char *s) {
  TxtLinha l = txt_linha(TXT_CAPTION, s, 150, 153, 162, 255);
  txt_desenhar(l, dir - l.w, ycentro - l.h * 0.5f);
}

// Texto de "ainda nao chegou" / "nao ha". Dois estados diferentes e dizer um
// pelo outro faz a secao parecer quebrada.
static float vazio(GfxRect r, float y) {
  const char *s = serieaud_carregando() ? "Carregando…"
                                        : "Sem dados desta temporada";
  TxtLinha l = txt_linha(TXT_BODY, i18n(s), 150, 153, 162, 255);
  txt_desenhar(l, r.x, y + 20.0f);
  return y - r.y + l.h + 20.0f;
}

// --- PAINEL 1: ARCO DE QUALIDADE ---------------------------------------------
//
// GEOMETRIA: uma caixa com a curva dentro, calha de rotulos a esquerda, valor
// da media a direita, eixo de episodios embaixo. A caixa e o que tirou o
// painel do "cinza sobre cinza"; a calha e o que separou o rotulo do teto do
// eixo do rotulo do melhor episodio.
//
// COR: um acento so, e ele e SEMANTICO. A curva e quase-branca porque ela e o
// dado; os dois unicos pontos coloridos sao o melhor (verde) e o pior
// (vermelho) da temporada, que sao tambem os dois unicos com nome escrito.
// Nada mais nesta caixa tem cor, de proposito: se tres coisas tem cor, nenhuma
// tem destaque a tres metros.

float serieaud_arco(GfxRect r) {
  float y = r.y + cabecalho(r, "Arco de qualidade",
                            "Nota do Trakt por episódio — não é o IMDb");
  float bx = r.x + 78.0f, bw = r.w - 78.0f - 196.0f;
  float gx = bx + 26.0f, gw = bw - 52.0f, gh = 206.0f;
  float py = y + 24.0f;               // topo da area de plotagem
  int i, n = 0, semNota = 0;
  int melhor = serieaud_melhor(), pior = serieaud_pior();
  int med = serieaud_nota_media();
  int lo = 1000, hi = 0;
  float passo;
  char txt[64];

  for (i = 0; i < nEps; i++) if (eps[i].nota > 0) {
    if (eps[i].nota < lo) lo = eps[i].nota;
    if (eps[i].nota > hi) hi = eps[i].nota;
    n++;
  }
  semNota = nEps - n;
  if (n < 2 || gw < 120.0f) return vazio(r, y);

  // A ESCALA NAO COMECA EM ZERO, DE PROPOSITO. Nota de episodio de serie vive
  // entre 7 e 9,5: com o eixo em 0..10 a curva vira uma reta e o painel nao
  // mostra nada. Em compensacao a faixa REAL vai escrita nos dois rotulos do
  // eixo, senao a amplitude visual mente sobre o tamanho da diferenca.
  { int folga = (hi - lo) / 4; if (folga < 2) folga = 2;
    lo -= folga; hi += folga;
    if (lo < 0) lo = 0; if (hi > 100) hi = 100; }

  passo = nEps > 1 ? gw / (float)(nEps - 1) : gw;

#define SA_ARCO_Y(nota) (py + gh - gh * (float)((nota) - lo) / (float)(hi - lo))

  { GfxRect box = { bx, y, bw, gh + 48.0f };
    caixa(box); }

  // Colunas sem nota, antes de tudo: elas sao fundo, nao dado.
  for (i = 0; i < nEps; i++)
    if (eps[i].nota <= 0)
      colunaVazia(gx + passo * (float)i, py - 8.0f, gh + 16.0f);

  // Piso do eixo e os dois extremos, na calha.
  { GfxRect base = { gx - 14.0f, py + gh, gw + 28.0f, 1.0f };
    gfx_cor(base, 0.0f, 1.0f, 1.0f, 1.0f, 0.16f); }
  snprintf(txt, sizeof txt, "%.1f", hi / 10.0);
  rotuloEixo(bx - 18.0f, py, txt);
  snprintf(txt, sizeof txt, "%.1f", lo / 10.0);
  rotuloEixo(bx - 18.0f, py + gh, txt);

  // A MEDIA, tracejada, com o valor na ponta direita, FORA da caixa.
  if (med > 0 && hi > lo) {
    float ym = SA_ARCO_Y(med);
    tracejada(gx - 14.0f, ym, gw + 28.0f, 0.94f, 0.94f, 0.96f, 0.22f);
    snprintf(txt, sizeof txt, i18n("média %.1f"), med / 10.0);
    { TxtLinha l = txt_linha(TXT_CAPTION, txt, 190, 192, 200, 255);
      txt_desenhar(l, bx + bw + 18.0f, ym - l.h * 0.5f); }
  }

  // A CURVA, por TRECHOS CONTIGUOS, e o PONTILHADO no vao entre eles.
  // Episodio sem nota corta o trecho: ligar por cima dele inventaria um valor
  // que ninguem publicou. O que mudou nesta passagem e que o vao deixou de ser
  // vazio — ver `pontilhada`.
  { float ys[SA_EP_MAX];
    int m = 0, inicio = 0, fimAnt = -1;
    for (i = 0; i <= nEps; i++) {
      int temNota = i < nEps && eps[i].nota > 0;
      if (temNota) {
        if (!m) inicio = i;
        ys[m++] = SA_ARCO_Y(eps[i].nota);
        continue;
      }
      if (m >= 1) {
        if (fimAnt >= 0)
          pontilhada(gx + passo * (float)fimAnt, SA_ARCO_Y(eps[fimAnt].nota),
                     gx + passo * (float)inicio, ys[0],
                     5.0f, 0.94f, 0.94f, 0.96f, 0.34f);
        if (m >= 2) {
          float dx;
          int na = amostrar(passo, ys, m, &dx);
          traco(gx + passo * (float)inicio, dx, na, 5.0f,
                0.96f, 0.96f, 0.98f, 1.0f);
        }
        fimAnt = inicio + m - 1;
      }
      m = 0;
    } }

  // OS PONTOS, e os dois que tem nome: melhor em verde, pior em vermelho.
  for (i = 0; i < nEps; i++) {
    float x, yy;
    if (eps[i].nota <= 0) continue;
    x  = gx + passo * (float)i;
    yy = SA_ARCO_Y(eps[i].nota);
    if (i == melhor)     ponto(x, yy, 22.0f, 0.24f, 0.86f, 0.52f, 1.0f);
    else if (i == pior)  ponto(x, yy, 22.0f, 0.93f, 0.30f, 0.30f, 1.0f);
    else                 ponto(x, yy, 11.0f, 0.96f, 0.96f, 0.98f, 1.0f);
  }
  // Os rotulos dos extremos vao DEPOIS de todos os pontos para nao ficarem por
  // baixo do ponto do episodio vizinho.
  for (i = 0; i < nEps; i++) {
    float x, yy;
    TxtLinha l;
    if (i != melhor && i != pior) continue;
    if (eps[i].nota <= 0) continue;
    x  = gx + passo * (float)i;
    yy = SA_ARCO_Y(eps[i].nota);
    snprintf(txt, sizeof txt, "E%d · %.1f", eps[i].ep, eps[i].nota / 10.0);
    l = txt_linha(TXT_CAPTION, txt, 255, 255, 255, 255);
    // O rotulo vira para dentro quando nao cabe fora, e e GRAMPEADO NA CAIXA —
    // nao mais em `gx - 40`, que era um numero inventado e justamente a
    // largura que fazia "E1 · 7.6" pousar sobre o rotulo do eixo. A caixa e a
    // borda de verdade: dentro dela o rotulo nunca encosta na calha.
    { float lx = x - l.w * 0.5f;
      float ly = (i == melhor) ? yy - 20.0f - l.h : yy + 20.0f;
      GfxRect chapa;
      if (ly < py) ly = yy + 20.0f;
      if (ly + l.h > py + gh + 22.0f) ly = yy - 20.0f - l.h;
      if (lx < bx + 12.0f) lx = bx + 12.0f;
      if (lx + l.w > bx + bw - 12.0f) lx = bx + bw - 12.0f - l.w;
      // CHAPA ATRAS DO ROTULO. Sem ela a curva passa POR DENTRO do texto e
      // some com um digito: na captura de 24 episodios o rotulo do melhor
      // episodio lia "E?4 · 8.8", porque o traco branco cruzava o "2" na
      // mesma cor. Afastar mais o rotulo do ponto nao resolve — perto de um
      // extremo a curva sobe justamente por ali —, e diminuir a fonte e
      // proibido (text.c:159). A chapa resolve sempre, custa um desenho por
      // rotulo, e le como pastilha de legenda e nao como falha da curva: ela
      // e um degrau ACIMA da superficie da caixa, nao igual a ela.
      chapa.x = lx - 10.0f; chapa.y = ly - 3.0f;
      chapa.w = l.w + 20.0f; chapa.h = l.h + 6.0f;
      gfx_cor(chapa, raioPx(chapa.w, chapa.h, 8.0f),
              0.137f, 0.141f, 0.157f, 1.0f);
      txt_desenhar(l, lx, ly); }
  }

  // Eixo X: primeiro e ultimo episodio. Um rotulo por episodio a 22px nao cabe
  // em 24 episodios, e abaixar a fonte para caber e o erro que a nota de
  // text.c:159 proibe.
  y += gh + 48.0f + 12.0f;
  snprintf(txt, sizeof txt, "E%d", eps[0].ep);
  { TxtLinha l = txt_linha(TXT_CAPTION, txt, 150, 153, 162, 255);
    txt_desenhar(l, gx - l.w * 0.5f, y); }
  snprintf(txt, sizeof txt, "E%d", eps[nEps - 1].ep);
  { TxtLinha l = txt_linha(TXT_CAPTION, txt, 150, 153, 162, 255);
    txt_desenhar(l, gx + gw - l.w * 0.5f, y);
    y += l.h; }
  // Quantos episodios nao tem nota, DITO EM NUMERO. O pontilhado mostra onde;
  // esta linha diz quantos, que e o que falta para o vao nao parecer defeito.
  //
  // VAI NUMA LINHA PROPRIA, a esquerda, como os rodapes dos outros dois
  // paineis. Estava alinhada a direita na MESMA linha do rotulo do ultimo
  // episodio e as duas se sobrepunham — "1 episódio sem nota publicE12" na
  // captura de DS9. Rotulo de eixo e rodape sao duas camadas de leitura
  // diferentes e nao dividem linha.
  if (semNota > 0) {
    TxtLinha q;
    if (semNota == 1)
      snprintf(txt, sizeof txt, "%s", i18n("1 episódio sem nota publicada"));
    else
      snprintf(txt, sizeof txt, i18n("%d episódios sem nota publicada"),
               semNota);
    q = txt_linha(TXT_CAPTION, txt, 150, 153, 162, 255);
    txt_desenhar(q, r.x, y + 8.0f);
    y += q.h + 8.0f;
  }
#undef SA_ARCO_Y
  return y - r.y;
}

// --- PAINEL 2: RADAR DE DESISTENCIA ------------------------------------------
//
// FORMA: a mesma curva do painel 1 seria a mesma imagem duas vezes, e a
// pergunta e outra. Aqui o que interessa nao e a altura da linha — e a
// DISTANCIA dela ate os 100%, porque essa distancia E a gente que foi embora.
// Entao o painel desenha essa distancia explicitamente, com uma HASTE por
// episodio pendurada na linha dos 100%, e a curva por cima ligando os pontos.
// Ver a nota de `haste` para por que nao e area cheia.
//
// A area ate a BASE da caixa ja tinha sido rejeitada antes, e com razao: com o
// eixo cortado ela vira um bloco solido da altura do painel, que le como
// barra.

float serieaud_radar(GfxRect r) {
  float y = r.y + cabecalho(r, "Radar de desistência",
                            "Quem marcou o episódio no Trakt, sobre quem marcou o E1 — não é a audiência geral");
  float bx = r.x + 88.0f, bw = r.w - 88.0f - 196.0f;
  float gx = bx + 26.0f, gw = bw - 52.0f, gh = 200.0f;
  float py = y + 24.0f;
  int fora[SA_EP_MAX];
  int i, n = 0, topo = 1000, piso = 1000, quedaI = -1, queda = 0, nFora = 0;
  float passo, y100 = 0.0f;
  char txt[140];

  for (i = 0; i < nEps; i++) { fora[i] = 0; if (serieaud_retencao(i) >= 0) n++; }
  if (n < 2 || gw < 120.0f) return vazio(r, y);

  // O EIXO NAO COMECA EM ZERO, E ISSO PRECISA DE JUSTIFICATIVA — grafico de
  // retencao com base cortada e o truque mais velho de estatistica ruim.
  //
  // A razao e MEDIDA, nao estetica: quem marca episodio no Trakt continua
  // marcando. Breaking Bad T2 vai de 100% a 97% em treze episodios; DS9 T1, de
  // 100% a 93% em doze. Num eixo 0..100 as duas curvas sao a MESMA reta colada
  // no teto, e o painel nao responde a pergunta que faz ("quem desistiu?").
  //
  // O que torna o corte honesto e o rotulo: os dois extremos do eixo vao
  // ESCRITOS em 22px na calha, e o rodape traz os numeros absolutos.
  //
  // --- O DEFEITO QUE ESTA REGRA NAO COBRIA -----------------------------------
  //
  // O corte era calculado sobre o MENOR valor da temporada, e um unico valor
  // baixo o desfazia: com um episodio recem-lancado, que ainda tem 3% das
  // marcacoes do E1, o piso descia para 0 e o teto ia a 110, e a temporada
  // inteira virava uma faixa plana colada no topo — exatamente o oposto do que
  // o corte existe para evitar. O painel cujo trabalho e mostrar uma queda
  // mostrava uma reta. Reproduzido em tests/serieaud_shot.c ("estreando").
  //
  // A regra nova exclui O PONTO DISCREPANTE, e so ele, por um criterio dito em
  // uma linha: o menor valor sai da escala quando a distancia dele ate o
  // SEGUNDO menor e maior do que a faixa inteira que sobra. Uma temporada que
  // desaba de verdade (100, 80, 60, 40, 30) nao dispara nada, porque ali o
  // segundo menor esta perto do menor; um episodio recem-lancado dispara
  // sempre, porque ele esta sozinho la embaixo. O laco repete enquanto valer,
  // com teto em um quarto dos episodios: acima disso nao ha ponto discrepante,
  // ha outra temporada.
  //
  // Nada e escondido. O excluido ganha um traco ambar abaixo do eixo, na
  // coluna dele, e uma linha no rodape com o numero.
  for (;;) {
    int lo1 = -1, lo2 = -1, alto = -1;
    for (i = 0; i < nEps; i++) {
      int v = serieaud_retencao(i);
      if (v < 0 || fora[i]) continue;
      if (alto < 0 || v > alto) alto = v;
      if (lo1 < 0 || v < lo1) { lo2 = lo1; lo1 = v; }
      else if (lo2 < 0 || v < lo2) lo2 = v;
    }
    if (lo2 < 0 || alto < 0) break;
    if (nFora * 4 >= n) break;
    if (lo2 - lo1 <= alto - lo2) { piso = lo1; topo = alto; break; }
    for (i = 0; i < nEps; i++)
      if (!fora[i] && serieaud_retencao(i) == lo1) { fora[i] = 1; nFora++; break; }
  }
  if (nFora * 4 >= n) { /* sem ponto discrepante confiavel: usa tudo */
    piso = 1000; topo = 1000;
    for (i = 0; i < nEps; i++) {
      int v = serieaud_retencao(i);
      if (v < 0) continue;
      fora[i] = 0;
      if (v > topo) topo = v;
      if (v < piso) piso = v;
    }
    nFora = 0;
  }

  // A FOLGA DO EIXO E PROPORCIONAL A FAIXA, e nao um degrau fixo de 5 pp com
  // um minimo de 10 pp como era. MEDIDO na captura de breaking-bad T2: a faixa
  // real da temporada e 96,9%-100%, o minimo de 10 pp estufava o eixo para
  // 95%-105%, e a curva ocupava 31% da altura da caixa — dois tercos do painel
  // eram vazio, e a queda que o painel existe para mostrar virava um risco
  // quase reto no meio de um retangulo grande. O mesmo defeito que o dono
  // apontou na versao miniatura ("a barra curta flutuando no vazio").
  //
  // As duas folgas sao DIFERENTES de proposito. Em cima quase nao se precisa:
  // o maior valor e quase sempre o proprio E1, cravado em 100%, entao basta 1
  // pp para o ponto nao encostar na borda. Embaixo se precisa mais, porque e
  // de la que a curva desce e e la que o olho procura o fim dela.
  //
  // O piso do intervalo total fica em 5 pp (e nao nos 10 pp antigos): abaixo
  // disso uma variacao de decimos viraria uma montanha. Acima, a faixa real
  // manda. Os dois extremos vao escritos na calha em 22 px, entao quem olha le
  // a amplitude de verdade e nao so a forma.
  { int faixa = topo - piso;
    int folgaT = faixa / 14, folgaP = faixa / 8;
    if (folgaT < 10) folgaT = 10;
    if (folgaP < 10) folgaP = 10;
    topo += folgaT;
    piso -= folgaP;
    // Arredonda para 1 pp cheio nos dois lados: o rotulo imprime `piso / 10` e
    // `topo / 10` em inteiro, entao um extremo que nao caia num decimo exato
    // faria o rotulo mentir por arredondamento.
    topo = ((topo + 9) / 10) * 10;
    piso = (piso / 10) * 10;
    if (piso < 0) piso = 0;
    if (topo - piso < 50) topo = piso + 50; }
  passo = nEps > 1 ? gw / (float)(nEps - 1) : gw;

#define SA_RAD_Y(v) (py + gh - gh * (float)((v) - piso) / (float)(topo - piso))

  { GfxRect box = { bx, y, bw, gh + 48.0f };
    caixa(box); }

  for (i = 0; i < nEps; i++)
    if (serieaud_retencao(i) < 0)
      colunaVazia(gx + passo * (float)i, py - 8.0f, gh + 16.0f);

  if (piso < 1000 && topo >= 1000) y100 = SA_RAD_Y(1000);

  // AS HASTES, uma por episodio, da linha dos 100% ate o ponto. Vao ANTES da
  // curva e da tracejada para ficarem por baixo das duas: a haste e medida, a
  // curva e o dado e a tracejada e a referencia — nessa ordem de leitura.
  //
  // COR: o reflexo da categoria aqui e "retencao -> azul", e era azul. Alem de
  // ser o primeiro palpite de qualquer um, azul sobre a superficie fria desta
  // base e a combinacao que menos separa. A regra que ficou: a haste NORMAL e
  // quase-branca e discreta, sem alarme nenhum, porque perder 3% de quem
  // marcou o E1 nao e emergencia; a unica coisa colorida e a EXCECAO — o
  // episodio ACIMA dos 100%, em verde, que e o caso raro (E2 com mais gente
  // que o E1) e que antes era um ressalto de tres pixels que ninguem via.
  if (y100 > 0.0f)
    for (i = 0; i < nEps; i++) {
      int v = serieaud_retencao(i);
      if (v < 0 || fora[i]) continue;
      if (v > 1000) haste(gx + passo * (float)i, y100, SA_RAD_Y(v),
                          0.24f, 0.86f, 0.52f, 0.85f);
      else          haste(gx + passo * (float)i, y100, SA_RAD_Y(v),
                          0.94f, 0.94f, 0.96f, 0.20f);
    }

  // A CURVA, por trechos contiguos — mesmo tratamento de buraco do painel 1.
  { float ys[SA_EP_MAX];
    int m = 0, inicio = 0, fimAnt = -1;
    for (i = 0; i <= nEps; i++) {
      int v = i < nEps ? serieaud_retencao(i) : -1;
      int usa = (i < nEps && v >= 0 && !fora[i]);
      if (usa) {
        if (!m) inicio = i;
        ys[m++] = SA_RAD_Y(v);
        continue;
      }
      if (m >= 1) {
        if (fimAnt >= 0)
          pontilhada(gx + passo * (float)fimAnt,
                     SA_RAD_Y(serieaud_retencao(fimAnt)),
                     gx + passo * (float)inicio, ys[0],
                     5.0f, 0.94f, 0.94f, 0.96f, 0.34f);
        if (m >= 2) {
          float dx;
          int na = amostrar(passo, ys, m, &dx);
          traco(gx + passo * (float)inicio, dx, na, 5.0f,
                0.96f, 0.96f, 0.98f, 1.0f);
        }
        fimAnt = inicio + m - 1;
      }
      m = 0;
    } }

  // A LINHA DOS 100%: a referencia de onde a temporada comecou.
  if (y100 > 0.0f) {
    tracejada(gx - 14.0f, y100, gw + 28.0f, 0.94f, 0.94f, 0.96f, 0.30f);
    { TxtLinha l = txt_linha(TXT_CAPTION, "100%", 190, 192, 200, 255);
      txt_desenhar(l, bx + bw + 18.0f, y100 - l.h * 0.5f); }
  }
  { GfxRect base = { gx - 14.0f, py + gh, gw + 28.0f, 1.0f };
    gfx_cor(base, 0.0f, 1.0f, 1.0f, 1.0f, 0.16f); }
  snprintf(txt, sizeof txt, "%d%%", piso / 10);
  rotuloEixo(bx - 18.0f, py + gh, txt);
  snprintf(txt, sizeof txt, "%d%%", topo / 10);
  rotuloEixo(bx - 18.0f, py, txt);

  // Os pontos, e os excluidos da escala como traco ambar abaixo do eixo.
  for (i = 0; i < nEps; i++) {
    int v = serieaud_retencao(i);
    float x = gx + passo * (float)i;
    if (v < 0) continue;
    if (fora[i]) {
      // PENDE ABAIXO DO EIXO de proposito: e a forma de dizer "este ponto
      // continua para baixo, alem do que a caixa mostra" sem desenhar um valor
      // falso dentro dela. O rodape diz o numero.
      GfxRect t2 = { x - 2.5f, py + gh + 5.0f, 5.0f, 24.0f };
      gfx_cor(t2, raioPx(5.0f, 24.0f, 2.5f), 0.96f, 0.78f, 0.30f, 0.95f);
      continue;
    }
    ponto(x, SA_RAD_Y(v), 11.0f, 0.96f, 0.96f, 0.98f, 1.0f);
  }

  // A MAIOR QUEDA entre dois episodios seguidos. E um FATO da serie medida, e
  // e a unica leitura que este painel faz em nome de quem olha.
  //
  // O TEXTO SAIU DE DENTRO DO GRAFICO. Ele era desenhado sobre a area de
  // plotagem e batia na curva — visivel na captura. Dentro da caixa fica so a
  // MARCA (o ponto ambar e o fio ate o eixo); a frase vai para o rodape, junto
  // da outra frase do painel, onde ela e lida a tres metros em vez de
  // decifrada por cima de uma linha.
  { int ant = -1;
    for (i = 0; i < nEps; i++) {
      int v = serieaud_retencao(i);
      if (v < 0 || fora[i]) { ant = -1; continue; }
      if (ant >= 0) {
        int d = serieaud_retencao(ant) - v;
        if (d > queda) { queda = d; quedaI = i; }
      }
      ant = i;
    } }
  if (quedaI > 0 && queda >= 10) {
    float x = gx + passo * (float)quedaI;
    float yy = SA_RAD_Y(serieaud_retencao(quedaI));
    GfxRect fio = { x - 1.0f, yy, 2.0f, py + gh - yy };
    gfx_cor(fio, 0.0f, 0.96f, 0.78f, 0.30f, 0.45f);
    ponto(x, yy, 20.0f, 0.96f, 0.78f, 0.30f, 1.0f);
  }

  y += gh + 48.0f + 18.0f;
  // O RODAPE E A RESPOSTA EM UMA FRASE, e depois os numeros absolutos. Com o
  // eixo cortado a curva quase plana nao "conta" nada a distancia; a frase
  // conta. E sem os absolutos "93%" nao diz se sao trezentas mil pessoas ou
  // trinta — a diferenca entre um dado e um ruido.
  if (eps[0].tem && nEps > 0) {
    char a[32], b[32];
    int ult = -1;
    for (i = nEps - 1; i >= 0; i--) if (eps[i].tem && !fora[i]) { ult = i; break; }
    curto(a, sizeof a, eps[0].watchers);
    if (ult > 0) {
      snprintf(txt, sizeof txt, i18n("%d%% de quem marcou o E%d chegou ao E%d"),
               serieaud_retencao(ult) / 10, eps[0].ep, eps[ult].ep);
      { TxtLinha l = txt_linha(TXT_BODY, txt, 236, 238, 244, 255);
        txt_desenhar(l, r.x, y);
        y += l.h + 6.0f; }
      curto(b, sizeof b, eps[ult].watchers);
      snprintf(txt, sizeof txt, i18n("%s pessoas no E%d  ·  %s no E%d"),
               a, eps[0].ep, b, eps[ult].ep);
    } else {
      snprintf(txt, sizeof txt, i18n("%s pessoas no E%d"), a, eps[0].ep);
    }
    { TxtLinha l = txt_linha(TXT_CAPTION, txt, 150, 153, 162, 255);
      txt_desenhar(l, r.x, y);
      y += l.h + 4.0f; }
  }
  if (quedaI > 0 && queda >= 10) {
    snprintf(txt, sizeof txt, i18n("maior queda: E%d → E%d, -%.1f pp"),
             eps[quedaI - 1].ep, eps[quedaI].ep, queda / 10.0);
    { TxtLinha l = txt_linha(TXT_CAPTION, txt, 246, 200, 120, 255);
      txt_desenhar(l, r.x, y);
      y += l.h + 4.0f; }
  }
  if (nFora > 0) {
    int primeiro = -1;
    for (i = 0; i < nEps; i++) if (fora[i]) { primeiro = i; break; }
    if (nFora == 1 && primeiro >= 0)
      snprintf(txt, sizeof txt, i18n("E%d ficou em %d%% — fora da escala do eixo"),
               eps[primeiro].ep, serieaud_retencao(primeiro) / 10);
    else
      snprintf(txt, sizeof txt, i18n("%d episódios ficaram fora da escala do eixo"),
               nFora);
    { TxtLinha l = txt_linha(TXT_CAPTION, txt, 150, 153, 162, 255);
      txt_desenhar(l, r.x, y);
      y += l.h; }
  }
#undef SA_RAD_Y
  return y - r.y;
}

// --- PAINEL 3: IMPRESSAO DIGITAL DO EPISODIO ---------------------------------
//
// A FORMA MUDOU, e a razao e de leitura, nao de gosto.
//
// Eram quatro faixas EMPILHADAS por episodio, uma coluna por episodio. O que
// se via na TV era um grafico de barras empilhadas quebrado: quatro blocos de
// cor encostados, com legenda embaixo, treze vezes. Empilhar convida a somar,
// e a soma de nota + retencao + reproducoes/pessoa + comentarios nao significa
// coisa nenhuma. Pior: como nada separava as faixas, o olho lia a pilha
// inteira como UMA barra de altura fixa, e a informacao (onde a faixa colorida
// termina dentro da propria fatia) era a parte que ele nao lia.
//
// Agora sao QUATRO FILEIRAS INDEPENDENTES, uma por grandeza, cada uma com o
// nome dela a esquerda e o valor do episodio escolhido a direita. Ler na
// HORIZONTAL da a evolucao daquela grandeza na temporada; ler na VERTICAL, na
// coluna destacada, da a assinatura do episodio — que e o que "impressao
// digital" quis dizer desde o inicio. A legenda embaixo deixou de existir
// porque cada fileira e o proprio rotulo dela.
//
// O que NAO mudou: cada barra continua sendo a posicao do episodio DENTRO DA
// TEMPORADA naquele eixo (0 = o menor, 1 = o maior), e nao um valor absoluto.
// Quatro grandezas com unidades diferentes nao se comparam de outro jeito sem
// mentir sobre alguma delas. A coluna da direita e que traz o numero cru, com
// a unidade dele — e agora ela traz os QUATRO, e nao mais uma frase de 160
// caracteres que ninguem le de longe.

enum { SA_NOTA, SA_RET, SA_REVER, SA_CONV, SA_NEIXOS };

static const struct { float r, g, b; const char *nome; } EIXO[SA_NEIXOS] = {
  { 0.94f, 0.94f, 0.96f, "Nota" },
  { 0.24f, 0.86f, 0.52f, "Retenção" },
  { 0.96f, 0.78f, 0.30f, "Rever" },
  { 0.55f, 0.65f, 1.00f, "Conversa" }
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

// O valor cru do eixo COM A UNIDADE DELE, para a coluna da direita. E aqui que
// "rever" volta a ser "1,19 reproducoes por pessoa" em vez de uma barra sem
// unidade.
static void eixoTexto(char *dst, unsigned tam, int i, int eixo) {
  int v = eixoBruto(i, eixo);
  if (v < 0) { snprintf(dst, tam, "—"); return; }
  switch (eixo) {
    case SA_NOTA:  snprintf(dst, tam, "%.1f", v / 10.0); return;
    case SA_RET:   snprintf(dst, tam, "%.1f%%", v / 10.0); return;
    case SA_REVER: snprintf(dst, tam, "%.2f×", v / 100.0); return;
    default:       curto(dst, tam, v); return;
  }
}

float serieaud_digital(GfxRect r) {
  float y = r.y + cabecalho(r, "Impressão digital do episódio",
                            "Nota, retenção, reproduções por pessoa e comentários+votos — tudo do Trakt, relativo à temporada");
  // Calha ESQUERDA para o nome da grandeza e calha DIREITA para o valor cru do
  // episodio escolhido. As duas larguras sao fixas porque as fileiras tem de
  // comecar e acabar alinhadas — e o alinhamento que faz quatro fileiras
  // lerem como uma tabela e nao como quatro graficos soltos.
  float cLab = 176.0f, cVal = 168.0f;
  float bx = r.x, bw = r.w;
  float gx = bx + cLab, gw = bw - cLab - cVal;
  float fileira = 46.0f, folga = 16.0f;
  float gh = SA_NEIXOS * fileira + (SA_NEIXOS - 1) * folga;
  float py = y + 22.0f;
  int mini[SA_NEIXOS], maxi[SA_NEIXOS];
  int i, e, n = 0;
  float larg, passo;
  char txt[160];

  for (e = 0; e < SA_NEIXOS; e++) { mini[e] = 0x7fffffff; maxi[e] = -1; }
  for (i = 0; i < nEps; i++) {
    int algum = 0;
    for (e = 0; e < SA_NEIXOS; e++) {
      int v = eixoBruto(i, e);
      if (v < 0) continue;
      algum = 1;
      if (v < mini[e]) mini[e] = v;
      if (v > maxi[e]) maxi[e] = v;
    }
    if (algum) n++;
  }
  if (n < 1 || gw < 260.0f) return vazio(r, y);

  // LARGURA DA BARRA: dois tercos do passo, com teto. O teto existe para a
  // temporada CURTA, onde o passo e enorme — com tres episodios ele passa de
  // 400 px e uma barra proporcional viraria uma placa.
  //
  // O teto subiu de 44 para 72 depois de olhar a captura de tres episodios: a
  // 44 px as tres barras ficavam perdidas numa fileira de 1350 px, e o painel
  // lia como tres marcas soltas em vez de uma serie. A 72 a temporada de 13
  // tambem ganha (a barra vai a 68 e encosta no teto so a partir de 8
  // episodios); a de 24 nao muda, porque la quem manda e o passo.
  passo = gw / (float)nEps;
  larg  = passo * 0.66f;
  if (larg > 72.0f) larg = 72.0f;

  { GfxRect box = { bx, y, bw, gh + 44.0f + 38.0f };
    caixa(box); }

  // A COLUNA DO EPISODIO ESCOLHIDO, atras de tudo. E ela que liga as quatro
  // fileiras numa leitura vertical — sem ela cada fileira e um grafico
  // separado e a palavra "digital" do titulo nao tem a que se referir.
  //
  // O vocabulario de foco desta base e SUPERFICIE CLARA PREENCHIDA com texto
  // ESCURO, sem contorno (layout.h, NV_COR_FOCO). A pilula do numeral, embaixo,
  // e quem cumpre esse contrato; este veu e so o rastro dela subindo pelas
  // fileiras, e por isso e claro e fraco, nunca um anel.
  if (dentro(selecionado)) {
    float cx = gx + passo * ((float)selecionado + 0.5f);
    // Vai ate DEPOIS da linha dos numerais, envolvendo a pilula do episodio.
    // Com o veu parando em cima das barras, pilula e coluna liam como dois
    // destaques separados na mesma tela — e o foco desta base e UM objeto.
    GfxRect col = { cx - larg * 0.5f - 9.0f, py - 12.0f,
                    larg + 18.0f, gh + 12.0f + 14.0f + 44.0f };
    gfx_cor(col, raioPx(col.w, col.h, 16.0f), 1.0f, 1.0f, 1.0f, 0.05f);
  }

  for (e = 0; e < SA_NEIXOS; e++) {
    float yb = py + (fileira + folga) * (float)e + fileira;  // base da fileira
    TxtLinha l;
    // Linha de base da fileira: fina, e ela que diz "as barras crescem daqui".
    { GfxRect base = { gx, yb, gw, 1.0f };
      gfx_cor(base, 0.0f, 1.0f, 1.0f, 1.0f, 0.13f); }
    for (i = 0; i < nEps; i++) {
      float cx = gx + passo * ((float)i + 0.5f);
      int v = eixoBruto(i, e);
      int sel = (i == selecionado);
      float t, h;
      GfxRect barra;
      if (v < 0) {
        // AUSENCIA TEM FORMA PROPRIA: um traco cinza deitado sobre a base, que
        // nao e uma barra baixa. Antes era uma calha escura vazia do tamanho
        // da fatia, e na captura ela lia como "o app nao desenhou", nao como
        // "o Trakt nao publicou".
        GfxRect nd = { cx - larg * 0.5f, yb - 3.0f, larg, 3.0f };
        gfx_cor(nd, raioPx(larg, 3.0f, 1.5f), 1.0f, 1.0f, 1.0f, 0.20f);
        continue;
      }
      t = (maxi[e] > mini[e]) ? (float)(v - mini[e]) / (float)(maxi[e] - mini[e])
                              : 1.0f;
      // Piso visivel: um episodio que e o MENOR da temporada nao e "zero", e
      // uma barra de altura nula leria como dado ausente — que agora tem marca
      // propria e nao pode ser confundida com esta.
      h = 11.0f + (fileira - 11.0f) * t;
      barra.x = cx - larg * 0.5f; barra.w = larg;
      barra.h = h; barra.y = yb - h;
      // O raio vem de raioPx: 6 px de canto em TODA barra, alta ou baixa. Com
      // o `0.18` fixo que estava aqui a barra alta saia com 8 px de canto e a
      // baixa com 1 — mesma fileira, duas formas.
      gfx_cor(barra, raioPx(larg, h, 6.0f), EIXO[e].r, EIXO[e].g, EIXO[e].b,
              sel ? 1.0f : 0.72f);
    }
    // Nome da grandeza, na calha esquerda, alinhado a direita contra as barras.
    l = txt_linha(TXT_CAPTION, i18n(EIXO[e].nome), 190, 192, 200, 255);
    txt_desenhar(l, gx - 22.0f - l.w, yb - l.h);
    // Valor cru do episodio escolhido, na calha direita. A cor e a da fileira:
    // e o que amarra o numero a barra sem precisar de legenda.
    if (dentro(selecionado)) {
      eixoTexto(txt, sizeof txt, selecionado, e);
      { TxtLinha v = txt_linha(TXT_BODY, txt,
                               (int)(EIXO[e].r * 255.0f),
                               (int)(EIXO[e].g * 255.0f),
                               (int)(EIXO[e].b * 255.0f), 255);
        txt_desenhar(v, gx + gw + 22.0f, yb - v.h); }
    }
  }

  // O EIXO DE EPISODIOS, e a pilula do escolhido.
  { float yn = py + gh + 14.0f;
    for (i = 0; i < nEps; i++) {
      float cx = gx + passo * ((float)i + 0.5f);
      int sel = (i == selecionado);
      TxtLinha l;
      snprintf(txt, sizeof txt, "%d", eps[i].ep);
      l = txt_linha(TXT_CAPTION, txt, sel ? 13 : 150, sel ? 13 : 153,
                    sel ? 13 : 162, 255);
      if (sel) {
        GfxRect pil = { cx - larg * 0.5f, yn, larg, l.h + 10.0f };
        gfx_cor(pil, raioPx(pil.w, pil.h, pil.h * 0.5f),
                0.94f, 0.94f, 0.96f, 1.0f);
      }
      txt_desenhar(l, cx - l.w * 0.5f, yn + 5.0f);
    }
    y = yn + 30.0f + 38.0f; }

  // QUEM E O EPISODIO ESCOLHIDO, em uma linha curta. Os quatro numeros dele ja
  // estao na calha direita; o que falta aqui e o tamanho da audiencia, que nao
  // e uma das quatro grandezas e sem o qual as outras nao tem escala humana.
  if (dentro(selecionado)) {
    int s = selecionado;
    TxtLinha l;
    if (eps[s].tem) {
      char a[32];
      curto(a, sizeof a, eps[s].watchers);
      snprintf(txt, sizeof txt, i18n("E%d · %s pessoas marcaram no Trakt"),
               eps[s].ep, a);
    } else {
      snprintf(txt, sizeof txt, i18n("E%d · sem dados de audiência"), eps[s].ep);
    }
    l = txt_linha_corta(TXT_BODY, txt, 236, 238, 244, 255, bw);
    txt_desenhar(l, bx, y);
    y += l.h + 6.0f;
  }

  // O INDICE DA SERIE, dito pelo que ele e. O mockup queria isto aberto por ano
  // ("1o ano 68%, 2o 54%"); nao ha fonte para nada por ano, e nao ha conta com
  // o que temos que chegue la — entao fica o numero que existe, com a unidade
  // que ele tem.
  if (serieaud_rever_serie() > 0) {
    char a[32], b[32];
    TxtLinha l;
    curto(a, sizeof a, playsSerie);
    curto(b, sizeof b, watchersSerie);
    snprintf(txt, sizeof txt,
             i18n("Série inteira: %s reproduções para %s pessoas — %.1f por espectador, somando todos os episódios"),
             a, b, serieaud_rever_serie() / 100.0);
    l = txt_linha_corta(TXT_CAPTION, txt, 150, 153, 162, 255, bw);
    txt_desenhar(l, bx, y);
    y += l.h;
  }
  if (truncada) {
    TxtLinha l = txt_linha(TXT_CAPTION,
                           i18n("Temporada maior que o limite: os primeiros 24 episódios"),
                           150, 153, 162, 255);
    txt_desenhar(l, bx, y + 6.0f);
    y += l.h + 6.0f;
  }
  return y - r.y;
}
