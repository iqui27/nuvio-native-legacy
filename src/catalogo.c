#include "jfid.h"
#include "catalogo.h"
#include "idbase.h"
#include "tendencia.h"
#include "artereserva.h"
// FRACO: os testes leves compilam catalogo.c sozinho (tests/catcache.sh e
// cinco irmaos) e nao querem historico de ordem nenhum. No app inteiro
// tendencia.c define a de verdade e vence esta.
__attribute__((weak)) void tend_registrar(const CatFileira *f, const CatItem *itens) { (void)f; (void)itens; }
__attribute__((weak)) int arte_reserva_registrar(const char *url, const char *imdb, int poster) { (void)url; (void)imdb; (void)poster; return 1; }
#include "idioma.h"
#include "descoberta.h"
#include "progresso.h"
// O cache em disco depende destes tres: dados.h diz ONDE se pode gravar,
// sessao.h e perfis.h dizem DE QUEM e o que esta gravado. Ver a nota longa
// sobre caminhoCache mais abaixo.
#include "dados.h"
#include "sessao.h"
#include "perfis.h"
#include <pthread.h>
#include <stdio.h>

static int mesmoTitulo(const char *a, const char *b);
static ProgRegistro *lerProgresso(int *k);
static int aplicarProgressoEm(CatItem *v, int m, const ProgRegistro *regs, int k);
static int removidoVence(const CatItem *c, const void *u);

// --- QUEM PODE TROCAR O VETOR ------------------------------------------------
// Dois publicadores podem se encontrar: o fio da descoberta (cat_definir_tudo)
// e o fio que refaz so a fileira "Continuar assistindo" (cat_trocar_continuar,
// ver desc_refazer_continuar). A trava e so ENTRE publicadores — o desenho
// nunca a pega e segue lendo pelo protocolo de ordem de escrita: `n` zera
// antes de o ponteiro trocar, e o bloco velho nao e liberado na hora.
static pthread_mutex_t pubTrava = PTHREAD_MUTEX_INITIALIZER;

// ESPERA PELA TRAVA DE PUBLICACAO, MEDIDA. trylock primeiro: o caso normal
// (livre) nao paga relogio nenhum. So a espera real e cronometrada, e so a
// acima de 8 ms vira linha de log (no maximo uma por segundo) — e o que separa
// "o fio principal ficou parado esperando o catalogo" de "foi outra coisa".
// `catFioPrincipal` e preenchido por cat_quadro (que roda no fio de desenho).
#include <time.h>
static pthread_t catFioPrincipal;
static int catFioPrincipalOk;
double cat_relogio_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}
static void catTravar(void) {
  double t0, ms;
  static double ultimoLog;
  if (pthread_mutex_trylock(&pubTrava) == 0) return;
  t0 = cat_relogio_ms();
  pthread_mutex_lock(&pubTrava);
  ms = cat_relogio_ms() - t0;
  if (ms > 8.0 && t0 - ultimoLog > 1000.0) {
    ultimoLog = t0;
    printf("[perf] cat: espera de %.0f ms pela trava de publicacao (%s)\n", ms,
           catFioPrincipalOk && pthread_equal(pthread_self(), catFioPrincipal)
             ? "fio principal" : "outro fio");
    fflush(stdout);
  }
}
#include <string.h>
#include <stdlib.h>
#ifdef NV_CAT_TEST_ANTES_TRAVA
// Teste (tests/catcorrida.c): outra troca de bloco entre a entrada e a trava.
extern void NV_CAT_TEST_ANTES_TRAVA(void);
#define CAT_TESTE_ANTES_TRAVA() NV_CAT_TEST_ANTES_TRAVA()
#else
#define CAT_TESTE_ANTES_TRAVA() ((void)0)
#endif
#ifdef NV_CAT_TEST_CACHE_MEIO
// Teste (tests/catcachecorrida.c): outro fio mexe no catalogo no meio da
// codificacao do cache.
extern void NV_CAT_TEST_CACHE_MEIO(void);
#define CAT_TESTE_CACHE_MEIO() NV_CAT_TEST_CACHE_MEIO()
#else
#define CAT_TESTE_CACHE_MEIO() ((void)0)
#endif

// --- QUANDO O BLOCO VELHO PODE MORRER ----------------------------------------
// O bloco trocado fora morria na troca SEGUINTE. Isso protegia o leitor de UMA
// troca, e o desenho precisa de mais: ele pega `cat_item(i)->backdrop` no
// comeco do quadro e so o entrega a tex_obter_* mais adiante, e um quadro da C9
// passa de 100 ms. Duas trocas nesse meio — a publicacao por fileira do
// arranque, a montagem publicando junto do fio de "Continuar assistindo", ou
// um cat_acrescentar* de outro fio — e o bloco era liberado debaixo dele.
//
// Pior: eram TRES lixos (lixoTroca, lixoLote, lixoAcr), cada um liberado pela
// proxima troca DO SEU TIPO, e a guarda de "uma troca" nem sempre valia.
//
// O sintoma no campo (1.3.3, 1.4.1, 1.4.3, sempre logo depois de a home
// remontar com os catalogos dos addons):
//   [tex] decode falhou (Couldn't open ���̑C) tam=-1 magica=00000000: ���̑C
// `backdrop` e o PRIMEIRO campo do CatItem, entao o backdrop do item 0 — o
// destaque, o primeiro card de "Continuar assistindo" — e o primeiro byte do
// bloco, onde o alocador escreve os ponteiros dele depois do free. O cache de
// textura copiou esses bytes como caminho. tests/catvida.sh reproduz com ASan.
//
// Agora o bloco vai para uma lista unica e so e liberado por cat_quadro(), no
// fio de desenho, quando o quadro em que ele foi trocado ja TERMINOU. O mais
// recente e mantido sempre, para quem le fora do desenho continuar com a
// folga de uma troca que ja tinha. O teto existe para a memoria nao crescer se
// o desenho parar de virar quadro; bate-lo e sinal de algo muito errado.
#define CAT_APOSENTADOS_MAX 16
static CatItem *aposentados[CAT_APOSENTADOS_MAX];
static int nAposentados;

// Sob pubTrava.
static void aposentar(CatItem *bloco) {
  if (!bloco) return;
  if (nAposentados == CAT_APOSENTADOS_MAX) {
    static int avisou;
    if (!avisou) {
      avisou = 1;
      printf("[cat] %d blocos trocados sem virada de quadro; liberando o mais velho\n",
             CAT_APOSENTADOS_MAX);
      fflush(stdout);
    }
    free(aposentados[0]);
    memmove(aposentados, aposentados + 1,
            sizeof aposentados[0] * (size_t)(CAT_APOSENTADOS_MAX - 1));
    nAposentados--;
  }
  aposentados[nAposentados++] = bloco;
}

// Roda no comeco do quadro, entao tudo que esta na lista foi trocado durante um
// quadro que ja acabou. Sobra so o mais recente (a folga de uma troca).
void cat_quadro(void) {
  int k;
  if (!catFioPrincipalOk) { catFioPrincipal = pthread_self(); catFioPrincipalOk = 1; }
  catTravar();
  if (nAposentados > 1) {
    for (k = 0; k < nAposentados - 1; k++) free(aposentados[k]);
    aposentados[0] = aposentados[nAposentados - 1];
    nAposentados = 1;
  }
  pthread_mutex_unlock(&pubTrava);
}

int cat_blocos_aposentados(void) {
  int q;
  catTravar();
  q = nAposentados;
  pthread_mutex_unlock(&pubTrava);
  return q;
}

// Alocado conforme chega, nao dimensionado por um numero chutado.
static CatItem *itens;
static int n;
static CatFileira fils[CAT_FIL_MAX];
static int nFils;
static int nAlocado;
// 1 enquanto o catalogo na tela veio do cache em disco, e nao da rede desta
// sessao. Ver a nota em catalogo.h.
static int veioDoCache;

// Garante espaco para `quero` itens. Devolve 0 se nao deu (e o chamador segue
// com o que ja tinha, que e melhor que perder tudo).
static void garantirFaixas(int quantos);
static void zerarFaixas(int quantos);

static int garantirEspaco(int quero) {
  CatItem *novo;
  int alvo;
  if (quero <= nAlocado) return 1;
  if (quero > CAT_MAX) quero = CAT_MAX;
  alvo = nAlocado ? nAlocado * 2 : 64;
  while (alvo < quero) alvo *= 2;
  novo = realloc(itens, sizeof(CatItem) * (size_t)alvo);
  if (!novo) return 0;
  memset(novo + nAlocado, 0, sizeof(CatItem) * (size_t)(alvo - nAlocado));
  itens = novo;
  nAlocado = alvo;
  return 1;
}
static char dirGravacao[512];

// Episodios de todos os titulos num vetor unico, com faixa por titulo. Uma
// matriz [titulo][episodio] gastaria memoria pelo pior caso em 40 titulos dos
// quais a maioria e filme e nao tem episodio nenhum.
// 1200 e nao 600: uma serie longa (novela, anime) estourava o vetor e a
// resposta ao estouro era ZERAR a faixa de episodios de TODOS os titulos —
// abrir uma serie grande apagava os episodios das outras telas visitadas.
// Sao ~200 B por CatEp: dobrar custa ~120 KB, metade do problema resolvido.
#define CAT_EP_MAX 1200
static CatEp eps[CAT_EP_MAX];
// Faixas de episodio por titulo, do mesmo tamanho do vetor de itens — que
// agora cresce, entao estes tambem.
static int  *epIni, *epQtd, nEps;

// --- A TRAVA DOS EPISODIOS (#203) --------------------------------------------
// epIni/epQtd/nFaixas/nEps e o CONTEUDO de eps[] so se tocam com epTrava.
//
// A corrida, provada com TSAN no app inteiro: garantirFaixas (realloc de
// epIni/epQtd) rodava no fio de "Continuar assistindo" (cat_trocar_continuar,
// DEPOIS de soltar pubTrava) enquanto o desenho, sem trava nenhuma, fazia
// cat_n_episodios -> epQtd[i]. realloc que muda o bloco = leitura de memoria
// liberada no fio principal; e cat_definir_episodios (fio buscarEps, um por
// pagina de serie) escrevia epIni[i]/epQtd[i] no ponteiro que acabara de ser
// liberado = ESCRITA em heap liberado, que so explode depois, num free()
// qualquer (aquecer_pedir, os SIGSEGV de #271).
//
// Mutex e nao copia-na-escrita porque o que se guarda e pequeno e quente: o
// leitor pega a trava, le DOIS inteiros e solta (dezenas de ns sem disputa); o
// escritor mais longo e o memcpy de ate CAT_EP_MAX episodios (~240 KB, fracao
// de ms). Nada de rede, disco, printf nem desenho dentro dela.
//
// ORDEM DAS TRAVAS (de fora para dentro; nunca o contrario):
//   contTrava (descoberta.c)  ->  pubTrava  ->  epTrava
// epTrava e FOLHA: com ela presa nao se chama nada que trave. histTrava
// (abaixo) nunca e tomada junto com pubTrava/epTrava. cat_n_episodios e
// cat_episodio podem ser chamados com pubTrava presa (respeitam a ordem).
// As travas de fora deste arquivo que andam perto (verTrava em streams.c, a
// `trava` de vistoep.c) podem estar presas quando se pega epTrava; o contrario
// nunca acontece, porque com epTrava presa nao se sai de catalogo.c.
//
// O que o leitor recebe de cat_episodio e um ponteiro para eps[], que e
// ESTATICO: nunca e liberado, entao guardar o ponteiro pelo quadro nao e uso
// de memoria liberada. O conteudo de uma vaga so e reescrito quando o vetor
// da a volta (nEps + qtd > CAT_EP_MAX), e por isso a troca de catalogo NAO
// zera mais nEps — so as faixas. Ver cat_definir_episodios.
// O que RESTA (aceito, nao provado em TV): quem segura o ponteiro enquanto a
// vaga e reescrita na volta le texto misturado por um quadro. Nao e heap
// liberado e nao cresce; copiar para fora mudaria as 35 chamadas.
static pthread_mutex_t epTrava = PTHREAD_MUTEX_INITIALIZER;
// SOBE A CADA TROCA DO CATALOGO INTEIRO. Quem guarda um indice (a pagina de
// detalhe guarda) precisa saber que ele deixou de valer — e, no caso dos
// episodios, que a faixa dele foi ZERADA junto e ninguem vai repedir sozinho.
static unsigned catRevisao;
// Atomica (#203): sobe em fios diferentes (descoberta, "Continuar
// assistindo", o principal ao tirar um card) e e lida pelo desenho a cada
// quadro. O TSAN pegou cat_trocar_continuar x cat_revisao; o incremento
// nao-atomico de dois fios ainda podia perder uma subida — e uma subida
// perdida e a home que nao se reconstroi.
static void revisaoSobe(void) { __atomic_add_fetch(&catRevisao, 1u, __ATOMIC_RELEASE); }
// GERACAO DAS FAIXAS DE EPISODIO (2.0.3): sobe TODA vez que uma faixa e
// apagada — troca de bloco (zerarFaixas) e, principalmente, a VOLTA do vetor
// comum em cat_definir_episodios, que zera as faixas de TODOS os titulos sem
// mexer em catRevisao. Quem mostra episodios (detalhe, player) so repedia
// quando a revisao andava: a volta apagava a lista da serie aberta e ninguem
// a pedia de novo (TCL do dono, 08/10: pagina da serie aberta pela ilha sem
// os cartoes de episodio, e "[posplay] sem lista de episodios" no player).
// Desde 1a5d1670 a troca de catalogo nao zera mais nEps, entao a volta deixou
// de ser rara: acontece a cada CAT_EP_MAX episodios publicados na sessao.
static unsigned epGeracao;
static void epGeracaoSobe(void) { __atomic_add_fetch(&epGeracao, 1u, __ATOMIC_RELEASE); }
unsigned cat_geracao_episodios(void) { return __atomic_load_n(&epGeracao, __ATOMIC_ACQUIRE); }
// SOBE A CADA MUDANCA EM ITEM, e nao so na troca do bloco: marca de lista
// (naLista), progresso, item acrescentado ou substituido. Existe para quem
// mostra uma LISTA DERIVADA do catalogo (o painel de Salvos) poder perguntar
// "mudou alguma coisa?" em O(1) por quadro, em vez de comparar a contagem e o
// primeiro item — o retrato que o painel usava nao via mudanca de marca e,
// quando disparava, a reconstrucao percorria o catalogo inteiro (2000 itens
// de 15 KB cada, um desencontro de cache por item no ARM da TV). Atomico: a
// descoberta escreve de outro fio. Ver cat_revisao_itens em catalogo.h.
static unsigned catMudancas;
static void mudou(void) { __atomic_add_fetch(&catMudancas, 1u, __ATOMIC_RELEASE); }

// O progresso de reproducao e uma posicao, nao uma prova de que o titulo foi
// marcado como assistido. O historico do Trakt fica separado, por identidade
// estavel, para que uma troca do catalogo nao transforme indice em identidade
// e para que um progresso alto nao masque um historico real conhecido.
typedef struct {
  char imdb[32];
  char tipo[8];
  int conhecido;
  int visto;
} CatHistorico;

// TETO PROPRIO, e nao CAT_MAX (#212). A tabela agora recebe o mapa INTEIRO de
// filmes vistos do Trakt (/sync/watched/movies, trakt.c), e quem assiste muito
// passa de 2000 filmes; o catalogo nao tem nada com isso.
#define HIST_MAX 8192
// INDICE POR HASH (#212): o selo de visto e lido por cartaz, por quadro, e a
// busca linear de antes (ate HIST_MAX strcmp por cartaz) custaria fps numa
// fileira cheia. Endereçamento aberto, 2x a capacidade; cada balde guarda
// posicao+1 (0 = vazio). Leitura, insercao e reset compartilham histTrava,
// entao nenhum consumidor observa uma chave pela metade.
#define HIST_BALDES (HIST_MAX * 2)
static CatHistorico historico[HIST_MAX];
static int nHistorico;
static int histBalde[HIST_BALDES];
// O hash continua O(1), mas a tabela e mutavel: escritores concorrentes e
// troca de identidade nao podem publicar a chave enquanto outro fio a limpa.
static pthread_mutex_t histTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long histGeracao = 1;
static char histUsuario[128];
static int histPerfil;

void cat_historico_contexto(const char *usuario, int perfil) {
  const char *u = usuario ? usuario : "";
  pthread_mutex_lock(&histTrava);
  if (histPerfil != perfil || strcmp(histUsuario, u)) {
    snprintf(histUsuario, sizeof histUsuario, "%s", u);
    histPerfil = perfil;
    if (!++histGeracao) ++histGeracao;
    nHistorico = 0;
    memset(histBalde, 0, sizeof histBalde);
    mudou();
  }
  pthread_mutex_unlock(&histTrava);
}

unsigned long long cat_historico_geracao(void) {
  unsigned long long g;
  pthread_mutex_lock(&histTrava);
  g = histGeracao;
  pthread_mutex_unlock(&histTrava);
  return g;
}

static void id_base(const char *origem, char *destino, size_t tam) {
  size_t n = 0;
  if (!destino || tam == 0) return;
  if (origem) {
    while (origem[n] && origem[n] != ':' && n + 1 < tam) n++;
    memcpy(destino, origem, n);
  }
  destino[n] = 0;
}

static const char *tipo_base(const char *tipo) {
  if (tipo && (!strcmp(tipo, "series") || !strcmp(tipo, "show"))) return "series";
  return "movie";
}

// "movie"/"series"/"show" are certain types; anything else ("anime" from AIOMetadata
// catalogs, empty) is a catalog guess that buscarEps resolves via /meta (10da34b0).
static int tipo_certo(const char *tipo) {
  return tipo && (!strcmp(tipo, "movie") || !strcmp(tipo, "series") || !strcmp(tipo, "show"));
}

static unsigned hist_hash(const char *id, const char *tipo) {
  unsigned h = 2166136261u;
  while (*id) { h ^= (unsigned char)*id++; h *= 16777619u; }
  h ^= (unsigned char)tipo[0];
  h *= 16777619u;
  return h;
}

static int historico_pos(const char *imdb, const char *tipo, int criar) {
  char id[32];
  const char *tb = tipo_base(tipo);
  unsigned b;
  int k;
  id_base(imdb, id, sizeof id);
  if (!id[0]) return -1;
  b = hist_hash(id, tb) % HIST_BALDES;
  for (k = 0; k < HIST_BALDES; k++, b = (b + 1) % HIST_BALDES) {
    int v = histBalde[b];
    if (!v) break;
    if (!strcmp(historico[v - 1].imdb, id) && !strcmp(historico[v - 1].tipo, tb))
      return v - 1;
  }
  if (!criar || nHistorico >= HIST_MAX || k >= HIST_BALDES) return -1;
  snprintf(historico[nHistorico].imdb, sizeof historico[nHistorico].imdb, "%s", id);
  snprintf(historico[nHistorico].tipo, sizeof historico[nHistorico].tipo, "%s", tb);
  historico[nHistorico].conhecido = 0;
  historico[nHistorico].visto = 0;
  histBalde[b] = nHistorico + 1;
  return nHistorico++;
}

// Leitura interna da modal: -1 = historico ainda nao consultado, 0 = nao
// visto confirmado, 1 = visto confirmado.
int cat_historico_estado_item(int indice) {
  char id[64], tipo[16];
  catTravar();
  if (!itens || indice < 0 || indice >= n || !itens[indice].imdb[0]) {
    pthread_mutex_unlock(&pubTrava);
    return -1;
  }
  snprintf(id, sizeof id, "%s", itens[indice].imdb);
  snprintf(tipo, sizeof tipo, "%s", itens[indice].tipo);
  pthread_mutex_unlock(&pubTrava);
  return cat_historico_estado_id(id, tipo);
}

// O hash e consultado sob uma trava curta: nenhum ponteiro da tabela escapa,
// e reset de perfil e insercao de outro fio nunca deixam chave pela metade.
int cat_historico_estado_id(const char *imdb, const char *tipo) {
  int p, estado;
  if (!imdb || !imdb[0]) return -1;
  pthread_mutex_lock(&histTrava);
  p = historico_pos(imdb, tipo ? tipo : "movie", 0);
  estado = p >= 0 && historico[p].conhecido ? historico[p].visto : -1;
  pthread_mutex_unlock(&histTrava);
  return estado;
}

// Sob histTrava. Ausencia num snapshot Trakt nao apaga uma marca que veio
// da conta ou de acao local: cada fonte continua acrescentando suas provas.
static void historico_definir(const char *imdb, const char *tipo, int visto) {
  int p = historico_pos(imdb, tipo, 1);
  if (p < 0) return;
  if (historico[p].conhecido && historico[p].visto == (visto ? 1 : 0)) return;
  historico[p].visto = visto ? 1 : 0;
  historico[p].conhecido = 1;
  mudou();
}

void cat_historico_definir_id(const char *imdb, const char *tipo, int visto) {
  pthread_mutex_lock(&histTrava);
  historico_definir(imdb, tipo, visto);
  pthread_mutex_unlock(&histTrava);
}

int cat_historico_definir_se_geracao(const char *imdb, const char *tipo,
                                     int visto, unsigned long long geracao) {
  int atual;
  pthread_mutex_lock(&histTrava);
  atual = geracao == histGeracao;
  if (atual) historico_definir(imdb, tipo, visto);
  pthread_mutex_unlock(&histTrava);
  return atual;
}

// O ESTADO "VISTO" DE UM TITULO INTEIRO, para o selo do cartaz e o olho do
// detalhe (#212). Antes os dois liam so `progresso >= 90` — a posicao de
// retomada, que o Trakt so manda para o que esta PAUSADO. Filme terminado em
// outro aparelho, ou marcado pelo menu, nunca tinha progresso aqui: o selo
// nao aparecia e o olho ficava riscado em todo titulo (medido no log da #212:
// "historico add ... HTTP 201" e nada na tela).
//
// Ordem: historico conhecido (Trakt /sync/watched/movies, /sync/history, conta
// Nuvio, a acao da pessoa) manda — inclusive o "nao visto" de quem desmarcou;
// sem ele, o progresso de sempre (so filme). Leitura O(1): o hash acima.
int cat_visto(const CatItem *c) {
  int h;
  if (!c) return 0;
  h = c->imdb[0] ? cat_historico_estado_id(c->imdb, c->tipo) : -1;
  if (h >= 0) return h;
  // SERIE sem historico: nao visto. O progresso de um item de serie e de UM
  // episodio, nao da serie; quem decide a serie inteira e o historico, que
  // extras.c escreve com os contadores de /shows/<id>/progress/watched.
  if (!strcmp(tipo_base(c->tipo), "series")) return 0;
  return c->progresso >= 90;
}

// Compatibilidade para chamadores antigos que so conhecem o IMDb. A serie e
// inferida do proprio catalogo quando possivel; o sufixo de episodio e o
// fallback para itens que ainda nao entraram no vetor.
const char *cat_tipo_por_imdb(const char *imdb) {
  int i = cat_indice_por_imdb(imdb);
  if (i >= 0 && cat_item(i)) return cat_item(i)->tipo;
  return (imdb && strchr(imdb, ':')) ? "series" : "movie";
}

// Quantas faixas ja existem. Sem este numero nao da para zerar SO a cauda nova,
// e era por nao existir que a funcao abaixo zerava tudo.
static int nFaixas;

// CRESCER O VETOR NAO PODE APAGAR OS EPISODIOS DE QUEM JA ESTAVA NELE.
//
// Esta funcao fazia memset no vetor INTEIRO, e e chamada por cat_acrescentar e
// pelo append em lote — dois caminhos que so ADICIONAM ao fim e NAO mexem no
// indice de ninguem. O efeito, medido na C9: a pagina de detalhe de "Os
// Aspones" publicava os 7 episodios aos 13,8 s (`[desc] ... 7 episodios
// publicados`, `[t] episodios na tela`), o proximo titulo que a descoberta
// acrescentava zerava epQtd de todo mundo, e a secao de episodios sumia da
// pagina — com o D-pad pulando de "Temporadas" direto para as abas, porque
// secao com zero colunas e intransponivel (focus.c). O dono via a pagina de uma
// serie sem lugar nenhum onde ver os episodios.
//
// Quem PRECISA invalidar tudo e cat_definir_tudo, onde os indices realmente
// mudam — e la a chamada e explicita, junto da troca do bloco.
//
// SOB epTrava (as duas): o realloc troca o bloco, e leitor nenhum pode estar
// dentro dele. Ver "A TRAVA DOS EPISODIOS" no topo.
static void garantirFaixas(int quantos) {
  int *a, *b;
  if (quantos < 1) return;
  a = realloc(epIni, sizeof(int) * (size_t)quantos);
  if (a) epIni = a;
  b = realloc(epQtd, sizeof(int) * (size_t)quantos);
  if (b) epQtd = b;
  // Sem memoria: as faixas valem so ate o MENOR dos dois blocos. Antes
  // nFaixas virava `quantos` de qualquer jeito, e o leitor indexava alem.
  if (!a || !b) { if (nFaixas > quantos) nFaixas = quantos; return; }
  // realloc NAO inicializa o que cresceu: a cauda nova sai com lixo, e um
  // epQtd de lixo faz cat_episodio ler fora do vetor de episodios.
  if (quantos > nFaixas) {
    size_t novos = (size_t)(quantos - nFaixas);
    if (epIni) memset(epIni + nFaixas, 0, sizeof(int) * novos);
    if (epQtd) memset(epQtd + nFaixas, 0, sizeof(int) * novos);
  }
  nFaixas = quantos;
}

// Troca de catalogo: os indices mudaram e nenhuma faixa antiga vale.
static void zerarFaixas(int quantos) {
  nFaixas = 0;
  epGeracaoSobe();
  garantirFaixas(quantos);
}

// Copia o campo ate o proximo '|' (ou fim de linha), sem estourar o destino.
static const char *campo(const char *p, char *destino, size_t tam) {
  size_t k = 0;
  while (*p && *p != '|' && *p != '\n') {
    if (k + 1 < tam) destino[k++] = *p;
    p++;
  }
  destino[k] = 0;
  return (*p == '|') ? p + 1 : p;
}

int cat_carregar(const char *dirArte) {
  char caminho[600];
  snprintf(caminho, sizeof caminho, "%s/catalogo.txt", dirArte);
  FILE *f = fopen(caminho, "r");
  if (!f) { printf("catalogo: %s ausente, seguindo sem ele\n", caminho); return 0; }

  char linha[2048];
  n = 0;
  while (n < CAT_MAX && garantirEspaco(n + 1) && fgets(linha, sizeof linha, f)) {
    if (linha[0] == '\n' || linha[0] == '#') continue;
    CatItem *it = &itens[n];
    char rel[512];
    const char *p = linha;
    p = campo(p, rel, sizeof rel);
    // os caminhos no arquivo sao relativos a pasta de arte
    if (rel[0]) snprintf(it->backdrop, sizeof it->backdrop, "%s/%s", dirArte, rel);
    else it->backdrop[0] = 0;
    snprintf(it->backdropCatalogo, sizeof it->backdropCatalogo, "%s", it->backdrop);
    p = campo(p, rel, sizeof rel);
    if (rel[0]) snprintf(it->poster, sizeof it->poster, "%s/%s", dirArte, rel);
    else it->poster[0] = 0;
    p = campo(p, rel, sizeof rel);
    if (rel[0]) snprintf(it->logo, sizeof it->logo, "%s/%s", dirArte, rel);
    else it->logo[0] = 0;
    p = campo(p, it->titulo, sizeof it->titulo);
    p = campo(p, it->genero, sizeof it->genero);
    // O catalogo do pacote guarda o genero JA COMPOSTO e em ingles
    // ("Filme  ·  Science Fiction  ·  Action"). Traduz cada pedaco entre os
    // separadores; o primeiro ("Filme"/"Programa de TV") ja vem em portugues e
    // atravessa a tabela sem mudanca. Feito aqui, na leitura, porque `genero` e
    // lido por varias telas e traduzir no desenho deixaria cada uma resolver
    // por conta propria.
    { char saida[sizeof it->genero]; size_t o = 0;
      const char *q = it->genero;
      const char *SEP = "  \xc2\xb7  ";
      while (*q && o + 1 < sizeof saida) {
        const char *sp = strstr(q, SEP);
        char parte[64]; size_t n = sp ? (size_t)(sp - q) : strlen(q);
        const char *pt;
        if (n >= sizeof parte) n = sizeof parte - 1;
        memcpy(parte, q, n); parte[n] = 0;
        // O PRIMEIRO PEDACO NAO E GENERO, e por isso nao pode ir por
        // desc_genero_pt. Ele e o rotulo do tipo, e o pacote ja o guarda em
        // PORTUGUES ("Filme  ·  Science Fiction"): desc_genero_pt so sabe
        // ingles->portugues e, com o ingles ligado, devolve tudo intacto. O
        // resultado era "Programa de TV  ·  Action  ·  Adventure" numa
        // interface inteira em ingles — generos certos, so o tipo em
        // portugues. Relatado numa OLED48A2PUA e reproduzido aqui.
        // i18n() e quem tem as chaves Filme->Movie e Programa de TV->TV Show.
        pt = o ? desc_genero_pt(parte) : i18n(parte);
        o += (size_t)snprintf(saida + o, sizeof saida - o, "%s%s",
                              o ? SEP : "", pt);
        if (!sp) break;
        q = sp + strlen(SEP);
      }
      if (o) snprintf(it->genero, sizeof it->genero, "%s", saida); }
    p = campo(p, it->meta, sizeof it->meta);
    p = campo(p, it->classificacao, sizeof it->classificacao);
    campo(p, it->sinopse, sizeof it->sinopse);
    it->imdb[0] = 0;
    snprintf(it->tipo, sizeof it->tipo, "movie");
    n++;
  }
  fclose(f);

  // ids.txt e um arquivo A PARTE, uma linha "tt1234567<TAB>movie|series" por
  // titulo, na mesma ordem. Ficou fora de catalogo.txt para nao mexer na ordem
  // das colunas de um arquivo que ja tem parser e dados. Sem ele o app roda
  // igual, so nao consegue perguntar fontes aos addons.
  snprintf(caminho, sizeof caminho, "%s/ids.txt", dirArte);
  f = fopen(caminho, "r");
  if (f) {
    int i = 0;
    while (i < n && fgets(linha, sizeof linha, f)) {
      char *tab = strchr(linha, '\t');
      char *fim;
      if (tab) {
        *tab = 0;
        snprintf(itens[i].tipo, sizeof itens[i].tipo, "%s", tab + 1);
        fim = itens[i].tipo + strlen(itens[i].tipo);
        while (fim > itens[i].tipo && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
      }
      snprintf(itens[i].imdb, sizeof itens[i].imdb, "%s", linha);
      { char *e = itens[i].imdb + strlen(itens[i].imdb);
        while (e > itens[i].imdb && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0; }
      i++;
    }
    fclose(f);
    printf("catalogo: %d ids\n", i);
  }

  // Elenco vem num arquivo separado, uma linha por titulo, na mesma ordem:
  // "nome~papel~foto;nome~papel~foto|direcao". Separado porque tem tamanho bem
  // diferente do resto e mudaria a linha do catalogo a cada ator a mais.
  snprintf(caminho, sizeof caminho, "%s/elenco.txt", dirArte);
  FILE *fe = fopen(caminho, "r");
  if (fe) {
    for (int i = 0; i < n && fgets(linha, sizeof linha, fe); i++) {
      char *barra = strchr(linha, '|');
      if (barra) {
        *barra = 0;
        char *d = barra + 1, *fim = d + strlen(d);
        while (fim > d && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
        snprintf(itens[i].direcao, sizeof itens[i].direcao, "%s", d);
      }
      char *p2 = linha;
      while (*p2 && itens[i].nElenco < CAT_ELENCO_MAX) {
        char *pv = strchr(p2, ';');
        if (pv) *pv = 0;
        char *t1 = strchr(p2, '~');
        if (t1) {
          *t1 = 0;
          char *t2 = strchr(t1 + 1, '~');
          if (t2) *t2 = 0;
          int k = itens[i].nElenco;
          snprintf(itens[i].elenco[k].nome, 64, "%s", p2);
          snprintf(itens[i].elenco[k].papel, 64, "%s", t1 + 1);
          if (t2 && t2[1] && t2[1] != '\n')
            snprintf(itens[i].elenco[k].foto, 512, "%s/%s", dirArte, t2 + 1);
          itens[i].nElenco++;
        }
        if (!pv) break;
        p2 = pv + 1;
      }
    }
    fclose(fe);
  }

  // extra.txt: "nota|logoProv|nomeProv|progresso|temporada|episodio|restanteMin", na
  // mesma ordem. O progresso entrou como QUARTA coluna para nao invalidar
  // arquivos antigos: faltando, o campo fica 0 e a barra some, que e o
  // comportamento certo para quem nunca comecou o titulo.
  snprintf(caminho, sizeof caminho, "%s/extra.txt", dirArte);
  FILE *fx = fopen(caminho, "r");
  if (fx) {
    for (int i = 0; i < n && fgets(linha, sizeof linha, fx); i++) {
      char c1[32] = "", c2[512] = "", c3[64] = "", c4[16] = "";
      char c5[16] = "", c6[16] = "", c7[16] = "";
      const char *q = linha;
      q = campo(q, c1, sizeof c1);
      q = campo(q, c2, sizeof c2);
      q = campo(q, c3, sizeof c3);
      q = campo(q, c4, sizeof c4);
      q = campo(q, c5, sizeof c5);
      q = campo(q, c6, sizeof c6);
      campo(q, c7, sizeof c7);
      itens[i].nota = atoi(c1);
      // PROGRESSO E MINUTOS RESTANTES NAO ENTRAM, e as colunas ficam para nao
      // invalidar o arquivo de quem ainda o gera.
      //
      // extra.txt e um retrato do acervo de QUEM EMPACOTOU, e a quarta coluna e
      // o quanto ELE assistiu de cada titulo. Num pacote distribuido isso vira
      // barra de progresso em filme que a pessoa nunca abriu — e, junto com a
      // fileira de reserva da home, "Continuar assistindo" cheio de titulo de
      // estranho no primeiro arranque. E o issue #19, e e a mesma classe do
      // art/collections.json que saiu do .ipk: dado do empacotador exibido como
      // se fosse do usuario.
      //
      // O progresso de verdade chega logo abaixo, de progresso.c, e depois da
      // conta e do Trakt pelo sync. Temporada e episodio ficam: sao metadados do
      // titulo (qual episodio o pacote descreve), nao consumo de ninguem.
      (void)c4; (void)c7;
      itens[i].temporada = atoi(c5);
      itens[i].episodio  = atoi(c6);
      if (c2[0]) snprintf(itens[i].provLogo, sizeof itens[i].provLogo, "%s/%s", dirArte, c2);
      snprintf(itens[i].provNome, sizeof itens[i].provNome, "%s", c3);
    }
    fclose(fx);
  }

  // Progresso gravado NESTE app (progresso.c). Vem depois de extra.txt de
  // proposito — o que se assistiu aqui e mais recente que o retrato trazido do
  // app web.
  { int k, aplicados;
    ProgRegistro *regs = lerProgresso(&k);
    aplicados = aplicarProgressoEm(itens, n, regs, k);
    free(regs);
    if (aplicados) { mudou(); printf("catalogo: %d progressos deste app\n", aplicados); } }

  // episodios.txt: "indice|temporada|episodio|nome|duracao|data|sinopse".
  // Indice na frente porque so parte dos titulos tem episodio — uma linha por
  // titulo, como nos outros arquivos, desperdicaria a maioria das linhas.
  snprintf(caminho, sizeof caminho, "%s/episodios.txt", dirArte);
  { FILE *fe2 = fopen(caminho, "r");
    pthread_mutex_lock(&epTrava);
    nEps = 0;
    zerarFaixas(nAlocado);   // carga do zero: nenhuma faixa antiga vale
    // Leitura de arquivo com epTrava presa: e o arranque, antes de qualquer
    // fio da descoberta existir, entao ninguem espera por ela.
    if (fe2) {
      while (nEps < CAT_EP_MAX && fgets(linha, sizeof linha, fe2)) {
        char c1[8], c2[8], c3[8];
        const char *q = linha;
        int alvo;
        CatEp *ep = &eps[nEps];
        memset(ep, 0, sizeof *ep);
        q = campo(q, c1, sizeof c1);
        alvo = atoi(c1);
        if (alvo < 0 || alvo >= n) continue;
        q = campo(q, c2, sizeof c2);
        q = campo(q, c3, sizeof c3);
        ep->temporada = atoi(c2);
        ep->episodio  = atoi(c3);
        q = campo(q, ep->nome, sizeof ep->nome);
        q = campo(q, ep->duracao, sizeof ep->duracao);
        q = campo(q, ep->data, sizeof ep->data);
        q = campo(q, ep->sinopse, sizeof ep->sinopse);
        { char rel[512] = "";
          campo(q, rel, sizeof rel);
          if (rel[0]) snprintf(ep->thumb, sizeof ep->thumb, "%s/%s", dirArte, rel); }
        if (!epQtd[alvo]) epIni[alvo] = nEps;
        epQtd[alvo]++;
        nEps++;
      }
      fclose(fe2);
    }
    { int lidos = nEps;
      pthread_mutex_unlock(&epTrava);
      if (fe2) printf("catalogo: %d episodios\n", lidos); }
  }

  int comElenco = 0;
  for (int i = 0; i < n; i++) if (itens[i].nElenco) comElenco++;
  printf("catalogo: %d titulos, %d com elenco (item0: %d atores, dir='%s')\n",
         n, comElenco, n ? itens[0].nElenco : 0, n ? itens[0].direcao : "");
  return n;
}

// --- CACHE EM DISCO ----------------------------------------------------------
//
// Ver a nota em catalogo.h. O cabecalho carrega a versao E o sizeof(CatItem):
// e o sizeof que protege de verdade, porque acrescentar um campo na struct
// muda o layout sem que ninguem se lembre de subir a versao a mao.
// SO O PROTOTIPO, e nao #include "ajustes.h": aquele cabecalho puxa
// <SDL2/SDL.h>, e catalogo.c e compilado sem SDL por tests/catcache.sh — que e
// justamente o teste deste cache. Incluir o cabecalho troca um teste leve por
// um que precisa da biblioteca grafica inteira para conferir um fwrite.
int ajustes_idioma(void);

#define CACHE_MAGIA  0x4E56434Bu   /* "NVCK" */
// VERSAO 2: o cabecalho passou a carregar a identidade do dono. Subir a versao
// nao e formalidade — um arquivo da versao 1 lido com esta struct daria um
// usuario de lixo e um perfil de lixo, e a comparacao abaixo o recusaria por
// acaso em vez de por regra.
// VERSAO 3: o cabecalho passou a carregar o IDIOMA. `CatItem.genero` guarda o
// rotulo do tipo JA TRADUZIDO ("Programa de TV · Drama"), montado na hora de
// analisar — e o cache grava o CatItem inteiro. Sem este campo, uma home
// gravada em portugues continuava dizendo "Programa de TV" e "Filme" depois de
// a pessoa mudar para ingles, para sempre, enquanto o resto da tela (que passa
// por i18n a cada desenho) ja estava traduzido. Relatado numa OLED48A2PUA.
// VERSAO 4: CatItem.elenco cresceu de 6 para CAT_ELENCO_MAX (12) no issue #94.
// O cabecalho ja grava sizeof(CatItem) e recusaria o arquivo por tamanho — a
// versao sobe mesmo assim para o motivo da recusa ser o campo novo, e nao um
// "tamanho diferente" que ninguem lembra de onde veio.
// VERSAO 6: o vinculo do Trakt passou a ser POR PERFIL (traktauth.c). Ate a 5
// o perfil 2 sem Trakt proprio usava o do perfil 1, e o cache gravado com o
// perfil 2 guarda a watchlist, o "continuar" e as listas do Trakt do 1 com o
// cabecalho dizendo "perfil 2" — a identidade confere e a home do 2 abriria
// com o Trakt do 1 ate a rede substituir. O formato NAO mudou: um arquivo da 5
// com perfil 1 continua certo (era o dono daquele Trakt) e e aceito, para o
// perfil 1 nao pagar um arranque sem cache por nada; o de qualquer outro
// perfil e recusado e apagado.
#define CACHE_VERSAO 6
// Mesmo cabecalho, itens CODIFICADOS (zeros em corrida). Um CatItem tem ~16 KB
// e quase tudo e zero (buffers de URL dimensionados para o pior caso): 1600
// titulos eram 25,6 MB gravados na MEMFS dentro da trava do sistema de arquivos
// e levados ao IndexedDB pela syncfs, SINCRONA, no fio principal — e relidos
// por inteiro a cada abertura. A magia diferente (e nao a versao) mantem o
// teste de versao 5 e o formato do cabecalho como estavam; uma build antiga
// que encontre este arquivo o descarta como "outra build".
#define CACHE_MAGIA_RLE 0x4E56434Cu
#define RLE_ZMIN 16u   /* corrida de zeros que vale separar */

/* Formato: pares {u32 zeros, u32 literal, bytes[literal]} ate cobrir `n`.
   dst == NULL so mede. */
static size_t rleCodificar(const unsigned char *src, size_t n, unsigned char *dst) {
  size_t i = 0, saida = 0;
  while (i < n) {
    size_t z = 0, l = 0, j;
    unsigned zz, ll;
    while (i + z < n && src[i + z] == 0) z++;
    j = i + z;
    while (j + l < n) {
      size_t k = 0;
      if (src[j + l] == 0) {
        while (j + l + k < n && src[j + l + k] == 0 && k < RLE_ZMIN) k++;
        if (k >= RLE_ZMIN || j + l + k >= n) break;
        l += k;       /* zeros curtos ficam no literal */
        continue;
      }
      l++;
    }
    zz = (unsigned)z; ll = (unsigned)l;
    if (dst) { memcpy(dst + saida, &zz, 4); memcpy(dst + saida + 4, &ll, 4); memcpy(dst + saida + 8, src + j, l); }
    saida += 8 + l;
    i = j + l;
  }
  return saida;
}
static int rleDecodificar(const unsigned char *enc, size_t encN, unsigned char *dst, size_t n) {
  size_t i = 0, o = 0;
  memset(dst, 0, n);
  while (i < encN) {
    unsigned zz, ll;
    if (i + 8 > encN) return 0;
    memcpy(&zz, enc + i, 4); memcpy(&ll, enc + i + 4, 4);
    i += 8;
    if (zz > n - o || ll > n - o - zz || ll > encN - i) return 0;
    o += zz;
    memcpy(dst + o, enc + i, ll);
    o += ll; i += ll;
  }
  return o <= n;
}
#define CACHE_VERSAO_SO_P1 5

typedef struct {
  unsigned magia, versao, tamItem, tamFileira;
  int nItens, nFileiras;
  // DE QUEM E ESTE CACHE. Ver a nota de cat_apagar_cache em catalogo.h: sem
  // estes dois campos, a primeira abertura depois de trocar de conta ou de
  // perfil mostrava a home da ANTERIOR — watchlist, continuar assistindo e o
  // feed de amigos com nome e avatar — ate a rede substituir. E nao e so
  // estetico: cada CatFileira leva `base` (addonurl.h), campo desse tamanho porque o
  // Xperience embute um JWT no CAMINHO (ver catalogo.h). O arquivo carrega
  // credencial de addon do usuario anterior.
  char usuario[64];   // `sub` do JWT; "" quando deslogado
  int  perfil;        // perfis_ativo()
  int  ingles;        // ajustes_idioma() (IDIOMA_*; 0 pt, 1 en como antes) quando o arquivo foi escrito
} CacheCab;

// Quem esta logado AGORA. Chamada nas duas pontas — gravar e ler — e por isso o
// arquivo so e aceito por quem o escreveu.
//
// `sub` e nao o token: o access_token ROTACIONA na renovacao, e chavear por ele
// faria a mesma pessoa perder o cache toda vez que a sessao se renovasse.
static void identidadeAtual(char *usr, size_t tam, int *perfil) {
  const char *u = sessao_usuario();
  snprintf(usr, tam, "%s", u ? u : "");
  *perfil = perfis_ativo();
}

// Ultima pasta em que o cache foi procurado ou gravado. Existe so para
// cat_apagar_cache, que e chamada do logout e nao tem `dirArte` nenhum na mao.
//
// Sem trava, e de proposito: dados_dir() nao muda depois de dados_iniciar,
// entao o fio da descoberta reescreve aqui sempre os MESMOS bytes que o fio
// principal ja escreveu no arranque. Uma trava protegeria uma escrita que nao
// muda nada — e o unico caso em que o valor difere, dados_dir() vazia, e o
// aparelho onde nada e gravavel e portanto nao ha cache nenhum em disputa.
static char dirCache[512];

// A PASTA GRAVAVEL GANHA DE `dirArte`, E A ESCOLHA MORA AQUI E NAO NOS
// CHAMADORES.
//
// `dirArte` e o PACOTE. No alvo Tizen ele e /app/art, que vem de
// --preload-file (tools/tizen.sh) e portanto e MEMFS: RAM, apagada a cada
// recarga. Gravar la NAO FALHA — o fopen devolve um FILE*, o fwrite escreve, o
// rename funciona, e nada disso sobrevive a fechar o app. O efeito medido e que
// no Tizen este cache nunca existiu na pratica: toda abertura refazia os ~30
// pedidos e esperava os 14,5 s que a nota de catalogo.h registra.
//
// O app ja resolveu isto uma vez para o cache de ARTE — src/main.c aponta
// tex_cache_dir para dados_dir()/cache pelo mesmo motivo e com a mesma nota.
//
// Aqui a regra fica DENTRO do modulo, e nao nos chamadores, porque o leitor
// (home.c) e o escritor (descoberta.c) sao arquivos diferentes: corrigindo num
// so, os dois passariam a discordar sobre onde o arquivo esta, que e pior que o
// defeito. Assim as duas pontas mudam juntas por construcao.
//
// ORDEM DE ARRANQUE, que e o que faz isto funcionar: dados_iniciar roda em
// main.c ANTES de app_iniciar, e e app_iniciar quem chama home_iniciar e
// portanto cat_ler_cache. dados_dir() ja e valido na leitura.
//
// `dados_dir()` pode ser "" quando nenhum candidato aceitou escrita (ver
// src/dados.h) — nesse caso volta-se ao comportamento de sempre.
static void caminhoCache(const char *dirArte, char *dst, size_t tam) {
  const char *d = dados_dir();
  if (!d || !*d) d = (dirArte && *dirArte) ? dirArte
                                           : (dirCache[0] ? dirCache : ".");
  if (d != dirCache) snprintf(dirCache, sizeof dirCache, "%s", d);
  snprintf(dst, tam, "%s/catalogo-rede.bin", d);
}

// GRAVAR BINARIO POR FORA DE dados_gravar, DE PROPOSITO E COM AS CONTAS FEITAS.
//
// dados_gravar mede o conteudo com strlen (ver src/dados.h) e portanto para no
// primeiro zero — inutil para um despejo de struct. Mas ela nao e so um fwrite:
// ela tambem toma a trava do sistema de arquivos e marca a descarga. Quem grava
// por fora fica devendo as duas, e as duas importam justamente no alvo para
// onde este arquivo esta se mudando:
//
//   TRAVA — cat_gravar_cache roda no FIO DA DESCOBERTA. No WASM o sistema de
//   arquivos e uma estrutura JavaScript compartilhada entre os workers e NAO e
//   segura entre fios; o sintoma medido em dados.c de ignorar isso foi o app
//   inteiro CONGELAR, sem erro nenhum, com o fio de sync escrevendo enquanto o
//   laco principal descarregava. Despejar 1,7 MB de catalogo e esse cenario.
//
//   DESCARGA — no Emscripten o fclose so mexe no IDBFS em RAM. Quem leva o
//   arquivo ao IndexedDB e dados_sincronizar(), no laco principal, e ela so faz
//   algo quando alguem marcou sujo. Sem dados_marcar_sujo aqui, trocar de pasta
//   nao resolveria nada: o cache continuaria morrendo ao fechar, so que numa
//   pasta diferente.
//
// Estas tres funcoes publicas de dados.h nao tinham NENHUM chamador ate agora
// (tex_cache.c grava sem elas); as travas internas equivalentes ja rodam dentro
// de dados_gravar e de dados_sincronizar, entao o mecanismo esta vivo — o que
// faltava era alguem de fora usa-lo.
//
// SUJO PESADO (0) e nao leve (1), ao contrario do cache de imagens: este
// arquivo e escrito UMA vez por sessao, quando o catalogo completo chega
// (~14,5 s depois de abrir), e nao a cada quadro. O atraso do leve e de 15 s —
// o bastante para o dono fechar o app logo depois de a home assentar e perder
// exatamente o que este cache existe para guardar. Uma descarga a mais por
// sessao e o preco, e ela ainda pega carona na proxima escrita do sync.
//
// Fora do Emscripten as tres sao no-op dentro de dados.c; mante-las fora do
// build nativo evita arrastar dados.c para testes que so querem o catalogo.
#ifdef __EMSCRIPTEN__
#define CACHE_FS_TRAVAR()   dados_fs_travar()
#define CACHE_FS_LIBERAR()  dados_fs_liberar()
#define CACHE_MARCAR_SUJO() dados_marcar_sujo(0)
#else
#define CACHE_FS_TRAVAR()   ((void)0)
#define CACHE_FS_LIBERAR()  ((void)0)
#define CACHE_MARCAR_SUJO() ((void)0)
#endif

int cat_apagar_cache(void) {
  char caminho[600];
  int foi;
  caminhoCache(NULL, caminho, sizeof caminho);
  CACHE_FS_TRAVAR();
  foi = (remove(caminho) == 0);
  CACHE_FS_LIBERAR();
  // A REMOCAO TAMBEM PRECISA SER DESCARREGADA. No Tizen apagar so do IDBFS em
  // RAM deixa o arquivo intacto no IndexedDB, e ele volta inteiro na proxima
  // abertura — um logout que nao apagou nada, com a aparencia de ter apagado.
  if (foi) {
    CACHE_MARCAR_SUJO();
    printf("[cat] cache do catalogo apagado\n");
    fflush(stdout);
  }
  return foi;
}

// Chamado pela descoberta quando o catalogo COMPLETO da rede substitui o do
// cache. A partir daqui a tela ja e a desta sessao.
void cat_cache_substituido(void) { veioDoCache = 0; }

int cat_gravar_cache_se_identidade(const char *dirArte, const char *donoEsperado,
                                   int perfilEsperado) {
  char caminho[600], tmp[620];
  CacheCab c;
  FILE *f;
  CatItem *copia;
  CatFileira *copiaFils;
  int qtd, qf;
  if (n < 1 && nFils < 1) return 0;
  caminhoCache(dirArte, caminho, sizeof caminho);
  // Grava num temporario e renomeia: quem le na proxima abertura nunca pega
  // arquivo pela metade se o app for fechado no meio da escrita.
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  // Zerar o cabecalho INTEIRO antes de preencher: `usuario` tem 64 bytes e o
  // `sub` usa 36. Sem isto o resto seria lixo de pilha, e o arquivo deixaria de
  // ser identico para o mesmo estado — o que torna qualquer conferencia byte a
  // byte impossivel e vaza pedaco de pilha para o disco.
  memset(&c, 0, sizeof c);
  c.magia = CACHE_MAGIA; c.versao = CACHE_VERSAO;
  c.ingles = ajustes_idioma();
  c.tamItem = (unsigned)sizeof(CatItem);
  c.tamFileira = (unsigned)sizeof(CatFileira);
  identidadeAtual(c.usuario, sizeof c.usuario, &c.perfil);
  if (!donoEsperado || strcmp(c.usuario, donoEsperado) || c.perfil != perfilEsperado)
    return 0;
  // RETRATO DO BLOCO SOB pubTrava, e so dele se codifica (queda "double free
  // or corruption (!prev)" / "malloc(): invalid next size" da 2.0.0, LG webOS).
  // As duas passadas do RLE liam `itens` e `n` da tela sem trava, no fio da
  // descoberta, logo depois de "fileiras remontadas sem rede" — o instante em
  // que o fio principal aplica a biblioteca da conta (cat_definir_na_lista no
  // bloco, cat_acrescentar_lote trocando o bloco). Um naLista que vira 1 entre
  // a passada que MEDE e a que ESCREVE quebra uma corrida de zeros, a segunda
  // sai maior que o malloc da primeira e escreve alem do fim. Ler o bloco fora
  // da trava tambem nao garante que ele viva: cat_quadro libera os trocados.
  // A copia custa um memcpy do catalogo (o mesmo de cada troca de bloco) e
  // tira a codificacao de dentro da trava. tests/catcachecorrida.sh.
  catTravar();
  qtd = n; qf = nFils;
  copia = malloc(sizeof(CatItem) * (size_t)(qtd > 0 ? qtd : 1));
  copiaFils = malloc(sizeof(CatFileira) * (size_t)(qf > 0 ? qf : 1));
  if (!copia || !copiaFils) {
    pthread_mutex_unlock(&pubTrava); free(copia); free(copiaFils); return 0;
  }
  if (qtd > 0) memcpy(copia, itens, sizeof(CatItem) * (size_t)qtd);
  if (qf > 0) memcpy(copiaFils, fils, sizeof(CatFileira) * (size_t)qf);
  pthread_mutex_unlock(&pubTrava);
  c.nItens = qtd; c.nFileiras = qf;
  { /* CODIFICA FORA DA TRAVA DO SISTEMA DE ARQUIVOS: so o fwrite fica dentro. */
    double t0 = cat_relogio_ms(), t1, t2;
    size_t rawN = sizeof(CatItem) * (size_t)qtd, encN;
    unsigned char *enc;
    unsigned long long encN64;
    encN = rleCodificar((const unsigned char *)copia, rawN, NULL);
    enc = malloc(encN ? encN : 1);
    if (!enc) { free(copia); free(copiaFils); return 0; }
    CAT_TESTE_CACHE_MEIO();
    rleCodificar((const unsigned char *)copia, rawN, enc);
    free(copia);
    c.magia = CACHE_MAGIA_RLE;
    encN64 = encN;
    t1 = cat_relogio_ms();
    CACHE_FS_TRAVAR();
    f = fopen(tmp, "wb");
    if (!f) { CACHE_FS_LIBERAR(); free(enc); free(copiaFils); return 0; }
    if (fwrite(&c, sizeof c, 1, f) != 1 ||
        fwrite(&encN64, sizeof encN64, 1, f) != 1 ||
        fwrite(enc, 1, encN, f) != encN ||
        (qf > 0 &&
         fwrite(copiaFils, sizeof(CatFileira), (size_t)qf, f) != (size_t)qf)) {
      fclose(f); remove(tmp); CACHE_FS_LIBERAR(); free(enc); free(copiaFils); return 0;
    }
    free(enc);
    free(copiaFils);
    t2 = cat_relogio_ms();
    printf("[perf] cat cache: %zu KB -> %zu KB, codifica %.1f ms (fora da trava), escreve %.1f ms (dentro da trava do FS)\n",
           rawN / 1024, encN / 1024, t1 - t0, t2 - t1);
    fflush(stdout);
  }
  fclose(f);
  // A profile/account switch during serialization must not publish the old
  // catalogue under the new private identity.
  { char donoAgora[sizeof c.usuario]; int perfilAgora;
    identidadeAtual(donoAgora, sizeof donoAgora, &perfilAgora);
    if (strcmp(donoAgora, donoEsperado) || perfilAgora != perfilEsperado) {
      remove(tmp); CACHE_FS_LIBERAR(); return 0;
    }
  }
  if (rename(tmp, caminho) != 0) { remove(tmp); CACHE_FS_LIBERAR(); return 0; }
  CACHE_FS_LIBERAR();
  CACHE_MARCAR_SUJO();
  printf("[cat] cache gravado em %s: %d titulos, %d fileiras\n",
         caminho, qtd, qf);
  fflush(stdout);
  return 1;
}

int cat_gravar_cache(const char *dirArte) {
  char dono[64]; int perfil;
  identidadeAtual(dono, sizeof dono, &perfil);
  return cat_gravar_cache_se_identidade(dirArte, dono, perfil);
}

int cat_ler_cache(const char *dirArte) {
  char caminho[600];
  char usuario[64];
  int perfil = 0;
  CacheCab c;
  FILE *f;
  CatItem *novo;
  // static: 40 x 1 KB nao pertence a pilha. Roda uma vez, no arranque.
  static CatFileira lidas[CAT_FIL_MAX];
  int nLidas = 0;
  caminhoCache(dirArte, caminho, sizeof caminho);
  f = fopen(caminho, "rb");
  if (!f) return 0;
  if (fread(&c, sizeof c, 1, f) != 1) { fclose(f); return 0; }
  // RECUSA em vez de ler torto. Struct diferente = arquivo de outra build.
  if ((c.magia != CACHE_MAGIA && c.magia != CACHE_MAGIA_RLE) ||
      !(c.versao == CACHE_VERSAO ||
        (c.versao == CACHE_VERSAO_SO_P1 && c.perfil == 1)) ||
      c.tamItem != sizeof(CatItem) || c.tamFileira != sizeof(CatFileira) ||
      c.nItens < 0 || c.nItens > CAT_MAX ||
      c.nFileiras < 0 || c.nFileiras > CAT_FIL_MAX) {
    fclose(f);
    printf("[cat] cache descartado (formato de outra build)\n");
    remove(caminho);
    return 0;
  }
  // CACHE DE OUTRA PESSOA E RECUSADO E APAGADO, nao apenas ignorado.
  //
  // Ignorar deixaria o arquivo em disco, e ele leva os catalogos da conta
  // anterior com a `base` de cada fileira — que no Xperience carrega um JWT
  // dentro do proprio caminho. Um cache que sobra e credencial que sobra.
  //
  // Apagar aqui e a rede de seguranca, nao a porta da frente: o logout deve
  // chamar cat_apagar_cache (ver catalogo.h). Esta verificacao tambem cobre o
  // caso que o logout nao ve — trocar de PERFIL dentro da mesma conta, que nao
  // passa por sync_esquecer_usuario.
  c.usuario[sizeof c.usuario - 1] = 0;
  identidadeAtual(usuario, sizeof usuario, &perfil);
  // O IDIOMA ENTRA NA MESMA COMPARACAO, e pelo mesmo motivo dos outros dois:
  // o arquivo carrega texto ja montado para um idioma. Subir a versao invalida
  // os arquivos antigos UMA VEZ; sem esta linha, trocar de idioma depois disso
  // nao invalidaria nada e a home voltaria a dizer "Programa de TV" em ingles.
  if (strcmp(c.usuario, usuario) != 0 || c.perfil != perfil ||
      c.ingles != ajustes_idioma()) {
    fclose(f);
    printf("[cat] cache descartado (era de outro usuario/perfil/idioma)\n");
    fflush(stdout);
    CACHE_FS_TRAVAR();
    remove(caminho);
    CACHE_FS_LIBERAR();
    CACHE_MARCAR_SUJO();
    return 0;
  }
  novo = malloc(sizeof(CatItem) * (size_t)(c.nItens > 0 ? c.nItens : 1));
  if (!novo) { fclose(f); return 0; }
  if (c.magia == CACHE_MAGIA_RLE) {
    unsigned long long encN64 = 0;
    size_t rawN = sizeof(CatItem) * (size_t)c.nItens;
    unsigned char *enc;
    if (fread(&encN64, sizeof encN64, 1, f) != 1 || encN64 > (unsigned long long)rawN * 2 + 64) {
      free(novo); fclose(f); remove(caminho); return 0;
    }
    enc = malloc(encN64 ? (size_t)encN64 : 1);
    if (!enc || fread(enc, 1, (size_t)encN64, f) != (size_t)encN64 ||
        !rleDecodificar(enc, (size_t)encN64, (unsigned char *)novo, rawN)) {
      free(enc); free(novo); fclose(f); remove(caminho); return 0;
    }
    free(enc);
  } else if (fread(novo, sizeof(CatItem), (size_t)c.nItens, f) != (size_t)c.nItens) {
    free(novo); fclose(f); remove(caminho); return 0;
  }
  if (c.nFileiras > 0) {
    if (fread(lidas, sizeof(CatFileira), (size_t)c.nFileiras, f)
        != (size_t)c.nFileiras) {
      free(novo); fclose(f); remove(caminho); return 0;
    }
    nLidas = c.nFileiras;
  }
  fclose(f);
  // Reaproveita o caminho de troca de bloco, que ja e o seguro para o fio de
  // desenho — e o que corta as janelas de fileira pelo tamanho real.
  cat_definir_tudo(novo, c.nItens, lidas, nLidas);
  veioDoCache = 1;
  free(novo);
  printf("[cat] cache lido: %d titulos, %d fileiras\n", c.nItens, nLidas);
  fflush(stdout);
  return 1;
}

int cat_do_cache(void) { return veioDoCache; }

// ASSINATURA DE UM CATALOGO: o que a home DESENHA dele — fileiras (chave,
// titulo, janela) e a identidade de cada item (imdb, tipo). FNV-1a sobre isso.
// Arte, sinopse e contagens nao entram: mudam sem que a home mude de forma.
// Serve para a descoberta saber se o que ela acabou de montar e o MESMO que ja
// esta na tela, e nesse caso nao publicar — publicar igual e remontar a home
// por nada, que e o "ela fica recarregando" do dono.
unsigned long cat_assinatura_de(const CatItem *lista, int qtd,
                                const CatFileira *fl, int nf) {
  unsigned long h = 2166136261UL;
  int i;
  const char *p;
#define MIX(str) for (p = (str); p && *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
  for (i = 0; i < nf; i++) {
    MIX(fl[i].chave); MIX(fl[i].titulo); MIX(fl[i].tipo);
    // A URL efetiva faz parte do contrato visual: trocar a origem mantendo o
    // mesmo id deve forcar a nova linha a chegar a Home.
    MIX(fl[i].base); MIX(fl[i].catId);
    h ^= (unsigned long)fl[i].ini * 31UL + (unsigned long)fl[i].n; h *= 16777619UL;
    h ^= (unsigned long)fl[i].estado; h *= 16777619UL;
    h ^= (unsigned long)fl[i].socialGeracao; h *= 16777619UL;
  }
  for (i = 0; i < qtd; i++) {
    MIX(lista[i].imdb); MIX(lista[i].tipo); MIX(lista[i].titulo);
    MIX(lista[i].poster); MIX(lista[i].backdrop); MIX(lista[i].logo);
    MIX(lista[i].backdropCatalogo); MIX(lista[i].backdropTmdb);
    MIX(lista[i].backdropTrakt); MIX(lista[i].genero); MIX(lista[i].meta);
    MIX(lista[i].classificacao); MIX(lista[i].sinopse);
  }
#undef MIX
  return h;
}

unsigned long cat_assinatura(void) {
  unsigned long h;
  catTravar();
  h = cat_assinatura_de(itens, n, fils, nFils);
  pthread_mutex_unlock(&pubTrava);
  return h;
}

int cat_n(void) { return __atomic_load_n(&n, __ATOMIC_ACQUIRE); }

// PONTEIRO E CONTAGEM DA MESMA TROCA. A troca publica `n = 0`, o ponteiro e
// `n` novo, nessa ordem — mas o ARM da TV nao garante que outro fio veja as
// escritas na ordem feita sem barreira, e ler o `n` novo (maior) com o ponteiro
// velho indexa alem do fim do bloco velho. Com release/acquire, reler o
// ponteiro depois da contagem e achar o mesmo prova que os dois sao do mesmo
// bloco; se mudou no meio, le de novo.
const CatItem *cat_item(int i) {
  for (;;) {
    CatItem *p = __atomic_load_n(&itens, __ATOMIC_ACQUIRE);
    int k = __atomic_load_n(&n, __ATOMIC_ACQUIRE);
    if (!p || k <= 0) return NULL;
    if (__atomic_load_n(&itens, __ATOMIC_ACQUIRE) == p)
      return &p[((i % k) + k) % k];
  }
}

// ":<digitos>:<digitos>" e so isso — o sufixo de temporada/episodio.
static int ehSufixoEp(const char *r) {
  int n = 0;
  if (*r != ':') return 0;
  r++;
  while (*r >= '0' && *r <= '9') { r++; n++; }
  if (!n || *r != ':') return 0;
  r++; n = 0;
  while (*r >= '0' && *r <= '9') { r++; n++; }
  return n && !*r;
}

// ERA "compara so ate o primeiro ':'", porque serie com progresso e guardada
// como "tt123:2:1" e quem procura tem so "tt123". O corte cego cobrava caro em
// id que NAO e do IMDb: canal e "cs:channel:<hash>", e o primeiro ':' cai logo
// depois do prefixo de duas letras — todo canal virava "cs", qualquer um
// "achava" o primeiro da lista, e clicar num abria outro (#37, relato apos a
// 1.0.42: "jumps to a different channel"). O mesmo valia para "kitsu:12345".
//
// A regra agora nao adivinha pelo prefixo: ou os dois ids sao iguais, ou o que
// sobra de um lado e EXATAMENTE ":<temporada>:<episodio>". E o unico sufixo que
// este codigo mesmo cria.
static int mesmoTitulo(const char *a, const char *b) {
  size_t na, nb;
  if (!strcmp(a, b)) return 1;
  // O que pode sobrar de um lado e SO ":<temporada>:<episodio>", que e como o
  // progresso de serie e chaveado. Quem procura tem o id do titulo; quem esta
  // guardado pode ter o episodio grudado. Qualquer outra diferenca e outro
  // titulo.
  na = strlen(a); nb = strlen(b);
  { const char *lon = na > nb ? a : b, *cur = na > nb ? b : a;
    size_t nc = na > nb ? nb : na;
    const char *r = lon + nc;
    if (strncmp(lon, cur, nc)) return 0;
    return ehSufixoEp(r); }
}

void cat_dir_gravacao(const char *dir) {
  if (dir && *dir) snprintf(dirGravacao, sizeof dirGravacao, "%s", dir);
}

int cat_indice_por_imdb(const char *imdb) {
  int i;
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < cat_n(); i++) {
    const CatItem *c = cat_item(i);
    if (c && c->imdb[0] && mesmoTitulo(c->imdb, imdb)) return i;
  }
  return -1;
}

// O MESMO TITULO EM DUAS FILEIRAS (#151). A serie que a pessoa esta vendo
// costuma estar em "Continuar assistindo" E na fileira de onde o detalhe foi
// aberto — e os episodios so sao publicados na copia do detalhe. O player
// re-resolvia pelo IMDb com cat_indice_por_imdb, que devolve a PRIMEIRA copia
// (a do CW, no topo): ao trocar de fonte dentro do player, ele reabria nessa
// copia, sem lista, e o "Proximo episodio" sumia. No log do #151 (id 4439):
// detalhe com 132 episodios e, na troca de fonte, "lista com 0 episodios".
//
// Ordem: a copia que o chamador ja tinha, se ainda e o mesmo titulo e tem
// episodios; senao qualquer copia com episodios; senao a do chamador (filme
// nao tem lista); senao a primeira.
int cat_indice_titulo(const char *imdb, int preferido) {
  int i, m = cat_n(), primeiro = -1, comEps = -1, prefOk = 0;
  const CatItem *c;
  if (!imdb || !imdb[0] || m < 1) return -1;
  if (preferido >= 0 && preferido < m && (c = cat_item(preferido)) &&
      c->imdb[0] && mesmoTitulo(c->imdb, imdb)) {
    if (cat_n_episodios(preferido) > 0) return preferido;
    prefOk = 1;
  }
  for (i = 0; i < m && comEps < 0; i++) {
    c = cat_item(i);
    if (!c || !c->imdb[0] || !mesmoTitulo(c->imdb, imdb)) continue;
    if (primeiro < 0) primeiro = i;
    if (cat_n_episodios(i) > 0) comEps = i;
  }
  if (comEps >= 0) return comEps;
  return prefOk ? preferido : primeiro;
}

int cat_indice_vivo(int indice, const char *imdb) {
  const CatItem *c;
  if (!imdb || !imdb[0]) return indice;
  if (indice >= 0 && indice < cat_n() && (c = cat_item(indice)) && !strcmp(c->imdb, imdb))
    return indice;
  return cat_indice_titulo(imdb, indice);
}

static int normalizarIndice(int indice) {
  int i = cat_n();
  if (i < 1) return -1;
  return ((indice % i) + i) % i;
}

void cat_apontar_episodio(int indice, int temporada, int episodio) {
  int e;
  indice = normalizarIndice(indice);
  if (indice < 0 || !(temporada > 0 && episodio > 0)) return;
  if (itens[indice].temporada != temporada || itens[indice].episodio != episodio)
    itens[indice].nomeEpisodio[0] = 0;
  itens[indice].temporada = temporada;
  itens[indice].episodio  = episodio;
  mudou();
  for (e = 0; e < cat_n_episodios(indice); e++) {
    const CatEp *ep = cat_episodio(indice, e);
    if (ep && ep->temporada == temporada && ep->episodio == episodio) {
      snprintf(itens[indice].nomeEpisodio, sizeof itens[indice].nomeEpisodio, "%s", ep->nome);
      break;
    }
  }
}

static void aplicarUm(int indice, double posSeg, double durSeg, int temporada, int episodio) {
  itens[indice].progresso = (int)(100.0 * posSeg / durSeg);
  itens[indice].restanteMin = (int)((durSeg - posSeg) / 60.0 + 0.5);
  cat_apontar_episodio(indice, temporada, episodio);
}

// AS COPIAS DA MESMA OBRA (issue #208). O mesmo filme costuma estar em
// "Continuar assistindo" E numa fileira de catalogo, e cada copia e um CatItem
// com o seu `progresso` — e e o da copia ABERTA que o botao do detalhe
// ("Retomar"/"Reproduzir") e o player (retomarPct) leem. Quem gravava so
// numa copia (o player na que tocou, o sync em cat_indice_por_imdb = a
// primeira, o disco na primeira que casava) deixava o tile da outra fileira
// em "Reproduzir", comecando do zero. Medido em tests/retomar_copias.sh: CW a
// 40%, tile da fileira de catalogo a 0.
static int mesmaCopiaEm(const CatItem *v, int a, int b) {
  if (a == b || !v[a].imdb[0] || !v[b].imdb[0]) return 0;
  if (v[a].tipo[0] && v[b].tipo[0] && strcmp(v[a].tipo, v[b].tipo)) return 0;
  return mesmoTitulo(v[a].imdb, v[b].imdb);
}
static int mesmaCopia(int a, int b) { return mesmaCopiaEm(itens, a, b); }

void cat_aplicar_progresso(int indice, double posSeg, double durSeg, int temporada, int episodio) {
  int j;
  indice = normalizarIndice(indice);
  if (indice < 0 || durSeg <= 1.0) return;
  aplicarUm(indice, posSeg, durSeg, temporada, episodio);
  for (j = 0; j < n; j++)
    if (mesmaCopia(indice, j)) {
      aplicarUm(j, posSeg, durSeg, temporada, episodio);
      itens[j].retomadoMs = itens[indice].retomadoMs;
    }
  mudou();
}

// O PROGRESSO DO DISCO E APLICADO NO BLOCO NOVO, ANTES DE ELE SER PUBLICADO
// (#203). Era aplicado depois da troca, sem trava, direto em itens[] — e o TSAN
// pegou: aplicarUm escrevendo no bloco que o desenho estava lendo
// (continuar_desenhar). Pior que a corrida de leitura era o que podia vir junto:
// `m = cat_n()` lido no comeco e itens[j] indexado ate m depois de outro fio
// trocar o bloco por um MENOR (cat_trocar_continuar encolhendo a fileira,
// cat_tirar_continuar), e escrita num bloco ja aposentado que cat_quadro libera
// no quadro seguinte. E o `static ProgRegistro regs[]` era o mesmo para dois
// publicadores (o fio de montar() e o de "Continuar assistindo", o segundo as
// vezes sem contTrava: desc_continuar_otimista so tenta).
//
// Agora: o disco e lido FORA de qualquer trava, num vetor de quem chama
// (lerProgresso), e aplicado no vetor `v` que so quem chama enxerga.
//
// Sem procurar o nome do episodio (cat_apontar_episodio faria): o bloco novo
// tem as faixas de episodio zeradas na publicacao, entao a busca nunca achava
// nada aqui — so limpava o nome quando o episodio muda, e isso continua.
static void apontarEm(CatItem *v, int i, int temporada, int episodio) {
  if (!(temporada > 0 && episodio > 0)) return;
  if (v[i].temporada != temporada || v[i].episodio != episodio) v[i].nomeEpisodio[0] = 0;
  v[i].temporada = temporada;
  v[i].episodio  = episodio;
}
static void aplicarUmEm(CatItem *v, int i, double posSeg, double durSeg, int temporada, int episodio) {
  v[i].progresso = (int)(100.0 * posSeg / durSeg);
  v[i].restanteMin = (int)((durSeg - posSeg) / 60.0 + 0.5);
  apontarEm(v, i, temporada, episodio);
}

// Depois do disco: a copia que ficou sem progresso herda o da copia que tem
// (a do CW, montada do Trakt/conta sem registro local, ou a que o disco nao
// tocou por ser mais nova). Entre varias, a de instante mais novo.
// As fontes sao poucas (o que tem barra), entao o laco interno e sobre elas e
// nao sobre o catalogo inteiro — milhares de itens ao quadrado na TV, nao.
//
// A COPIA QUE A PESSOA TIROU NAO E FONTE. Isto rodava DEPOIS de
// podarContinuar(removidoVence) e o card tirado ja nao estava no vetor; agora
// roda antes da publicacao (e da poda), entao a mesma regra entra aqui.
static void espalharEm(CatItem *v, int m) {
  int i, j, k, nf = 0, *fontes;
  for (i = 0; i < m; i++) if (v[i].progresso > 0 && v[i].imdb[0]) nf++;
  if (!nf || !(fontes = malloc(sizeof(int) * (size_t)nf))) return;
  for (i = 0, k = 0; i < m && k < nf; i++)
    if (v[i].progresso > 0 && v[i].imdb[0] && !removidoVence(&v[i], NULL)) fontes[k++] = i;
  nf = k;
  for (j = 0; j < m && nf; j++) {
    int melhor = -1;
    if (v[j].progresso > 0 || !v[j].imdb[0]) continue;
    for (k = 0; k < nf; k++) {
      i = fontes[k];
      if (!mesmaCopiaEm(v, i, j)) continue;
      if (melhor < 0 || v[i].retomadoMs > v[melhor].retomadoMs) melhor = i;
    }
    if (melhor < 0) continue;
    v[j].progresso   = v[melhor].progresso;
    v[j].restanteMin = v[melhor].restanteMin;
    v[j].retomadoMs  = v[melhor].retomadoMs;
    apontarEm(v, j, v[melhor].temporada, v[melhor].episodio);
  }
  free(fontes);
}

// Fora de trava: prog_ler tem a dele. NULL com *k = 0 se nao ha nada (ou sem
// memoria) — aplicarProgressoEm aceita e so espalha.
static ProgRegistro *lerProgresso(int *k) {
  ProgRegistro *regs = malloc(sizeof(ProgRegistro) * PROG_MAX);
  *k = regs ? prog_ler(regs, PROG_MAX) : 0;
  if (*k < 1) { free(regs); regs = NULL; *k = 0; }
  return regs;
}

// Reaplica o que esta em progresso.c sobre `v` (m itens). Os registros vem do
// mais novo para o mais antigo, e cada titulo recebe so o primeiro que casar:
// numa serie com varios episodios gravados, e o episodio mais recente que a
// fileira e o "Retomar" querem mostrar.
static int aplicarProgressoEm(CatItem *v, int m, const ProgRegistro *regs, int k) {
  char *tocado;
  int i, aplicados = 0;
  if (!v || m < 1) return 0;
  if (k < 1 || !regs) { espalharEm(v, m); return 0; }
  tocado = calloc((size_t)m, 1);
  if (!tocado) return 0;
  for (i = 0; i < k; i++) {
    int j, maisNova = 0;
    // O instante decide pela OBRA, nao por copia: se alguma copia (o card do
    // Trakt) e mais nova que este registro, nenhuma copia recebe o registro —
    // espalharEm, no fim, da a todas o estado da mais nova.
    for (j = 0; j < m; j++)
      if (v[j].imdb[0] && mesmoTitulo(v[j].imdb, regs[i].contentId) &&
          v[j].retomadoMs > 0 && v[j].retomadoMs > regs[i].lastWatchedMs) maisNova = 1;
    for (j = 0; j < m; j++) {
      if (tocado[j] || !v[j].imdb[0] || !mesmoTitulo(v[j].imdb, regs[i].contentId)) continue;
      if (maisNova) { tocado[j] = 1; continue; }
      // O ITEM QUE JA E MAIS NOVO QUE O DISCO NAO VOLTA NO TEMPO. O item do
      // Trakt (pausado ou "a seguir", issue #66) traz o instante em
      // retomadoMs; um registro local mais velho — o S1E1 a 3% de 8/9 quando
      // o Trakt diz "viu o S1E1 inteiro em 19/9, a seguir o S1E2" — punha o
      // episodio ja visto de volta no card, com o selo do outro. Mesma regra
      // de montarContinuar: o instante decide.
      // TODAS as copias, e nao so a primeira (#208): o `break` daqui dava o
      // registro so ao card do CW, que vem antes da fileira de catalogo.
      if (regs[i].durSeg > 1.0) {
        aplicarUmEm(v, j, regs[i].posSeg, regs[i].durSeg, regs[i].temporada, regs[i].episodio);
        aplicados++;
      }
      tocado[j] = 1;
    }
  }
  free(tocado);
  espalharEm(v, m);
  return aplicados;
}

// TIRA O ITEM DA JANELA DA FILEIRA QUE O CONTEM. Issue #22.
//
// cat_zerar_progresso, logo abaixo, apaga o que a legenda desenha — mas o card
// CONTINUA na fileira, agora sem barra, ate a proxima remontagem do catalogo.
// Foi o que o relator descreveu depois do conserto das tres fontes: "e removido
// mesmo, mas so some quando eu fecho o app e abro de novo".
//
// COMO, sem quebrar as outras fileiras: as janelas sao disjuntas e contiguas
// (ver CatFileira em catalogo.h), entao um item pertence a UMA fileira so.
// Encolher `n` e deslocar o resto DENTRO da janela nao move nenhum indice de
// outra fileira — o que sobra e um slot orfao no fim, que fileira nenhuma
// referencia. Compactar o vetor de itens, que seria o reflexo obvio, faria o
// contrario: mudaria o `ini` de todas as fileiras seguintes.
//
// ORDEM DAS DUAS ESCRITAS, e ela importa porque o fio de desenho le sem trava:
// `n` desce PRIMEIRO. Um leitor no meio disso ve a fileira uma unidade menor
// com o conteudo ainda antigo — um quadro com o card repetido no pior caso —,
// nunca um indice fora da janela. Na ordem inversa ele leria o slot orfao.
//
// A REVISAO SOBE AQUI TAMBEM (medido em 22/09, tests/cwremover.sh). A home
// (sincronizarFileiras) ganhou na 1.4 um guarda curto por contadores: com
// cat_revisao, fil_revisao, col_revisao e o numero de fileiras iguais ela sai
// sem olhar as janelas. Esta funcao encolhia `n` sem bumpar nada, entao a home
// seguia com a contagem VELHA: o card tirado era coberto pelo vizinho e o
// ultimo aparecia duas vezes (o slot orfao), e so a proxima republicacao
// acertava a fileira. So quando a fileira esvaziava — nFils mudava — a home
// via na hora. E o mesmo contrato de cat_trocar_continuar, que faz a mesma
// operacao por troca de bloco e sempre bumpou.
//
// AS FAIXAS DE EPISODIO ANDAM JUNTO com os itens: epIni/epQtd sao por indice,
// e deslocar so os itens deixava cada titulo apos o tirado com os episodios do
// vizinho na pagina de detalhe.
//
// Chamar com pubTrava tomada. `r` e a fileira que contem `indice`.
static void tirarDaJanela(int r, int indice) {
  CatFileira *f = &fils[r];
  int quantos, orfao;
  f->n--;
  quantos = f->ini + f->n - indice;
  orfao = f->ini + f->n;
  if (quantos > 0) {
    memmove(&itens[indice], &itens[indice + 1], sizeof(CatItem) * (size_t)quantos);
  }
  pthread_mutex_lock(&epTrava);
  if (quantos > 0 && epIni && epQtd && orfao < nFaixas) {
    memmove(&epIni[indice], &epIni[indice + 1], sizeof(int) * (size_t)quantos);
    memmove(&epQtd[indice], &epQtd[indice + 1], sizeof(int) * (size_t)quantos);
  }
  if (epQtd && orfao < nFaixas) epQtd[orfao] = 0;
  pthread_mutex_unlock(&epTrava);
  // Fileira que esvaziou sai da lista, senao a home desenha um titulo com
  // nada embaixo. Mesmo protocolo: zera a contagem antes de mexer no vetor.
  if (f->n < 1) {
    int k, total = nFils;
    nFils = 0;
    for (k = r; k + 1 < total; k++) fils[k] = fils[k + 1];
    nFils = total - 1;
  }
  revisaoSobe(); mudou();
}

// Devolve 1 se achou e tirou.
int cat_tirar_item_da_fileira(int indice) {
  int r;
  if (indice < 0 || indice >= n) return 0;
  catTravar();
  for (r = 0; r < nFils; r++) {
    CatFileira *f = &fils[r];
    char nome[sizeof f->chave];
    if (indice < f->ini || indice >= f->ini + f->n) continue;
    // COPIA O NOME ANTES, porque a compactacao em tirarDaJanela sobrescreve
    // fils[r] com a fileira seguinte — ler f->chave depois dela imprime o nome
    // ERRADO. O teste pegou: tirando os tres itens da "retomada", a terceira
    // linha dizia "populares".
    snprintf(nome, sizeof nome, "%s", f->chave);
    tirarDaJanela(r, indice);
    printf("[cat] item %d tirado da fileira \"%s\"\n", indice, nome);
    fflush(stdout);
    pthread_mutex_unlock(&pubTrava);
    return 1;
  }
  pthread_mutex_unlock(&pubTrava);
  return 0;
}

// FRACO pelo mesmo motivo de tend_registrar no topo: os testes leves compilam
// catalogo.c sem progresso.c. No app progresso.c define a de verdade.
__attribute__((weak)) int prog_removido_vence(const char *imdb, long long instanteMs) {
  (void)imdb; (void)instanteMs; return 0;
}

static int fileiraContinuar(void) {
  int r;
  for (r = 0; r < nFils; r++) if (!strcmp(fils[r].chave, "continue_watching")) return r;
  return -1;
}

// Tira da janela de "Continuar assistindo" os itens para os quais `quer`
// responde 1. Do fim para o comeco, para o indice seguinte nao andar debaixo
// do laco. pubTrava tomada. Devolve quantos saíram.
static int podarContinuar(int (*quer)(const CatItem *, const void *), const void *u) {
  int r = fileiraContinuar(), i, tirados = 0;
  if (r < 0) return 0;
  for (i = fils[r].ini + fils[r].n - 1; r >= 0 && i >= fils[r].ini; i--) {
    if (!quer(&itens[i], u)) continue;
    { int era = nFils;
      tirarDaJanela(r, i);
      tirados++;
      if (nFils != era) break; }   // esvaziou: a fileira nao existe mais
  }
  return tirados;
}

static int mesmaObraQue(const CatItem *c, const void *u) {
  char a[32], b[32];
  id_base(c->imdb, a, sizeof a);
  id_base((const char *)u, b, sizeof b);
  return a[0] && !strcmp(a, b);
}

// A remocao vence o item: ver prog_removido_vence. retomadoMs e o paused_at do
// Trakt/Simkl ou o lastWatched da conta; 0 perde, e a propria funcao consulta o
// registro local para o caso "assistiu de novo aqui".
static int removidoVence(const CatItem *c, const void *u) {
  (void)u;
  return c->imdb[0] && prog_removido_vence(c->imdb, c->retomadoMs);
}

// "TIRAR DE CONTINUAR ASSISTINDO", NA HORA E POR IDENTIDADE.
//
// Por imdb, e nao pelo indice que a modal guardou: o fio de
// desc_refazer_continuar troca o bloco (cat_trocar_continuar) a qualquer
// instante, e um indice de antes da troca tiraria OUTRO titulo. Sob pubTrava a
// busca e a remocao enxergam o mesmo bloco. Tira TODOS os cards da mesma obra
// na janela — episodios diferentes da mesma serie sao um card so para quem
// esta olhando.
int cat_tirar_continuar(const char *imdb) {
  int k;
  if (!imdb || !imdb[0]) return 0;
  catTravar();
  k = podarContinuar(mesmaObraQue, imdb);
  pthread_mutex_unlock(&pubTrava);
  printf("[cat] %s tirado de Continuar assistindo: %d card(s)\n", imdb, k);
  fflush(stdout);
  return k;
}

static void zerarUm(int indice);
void cat_zerar_progresso(int indice) {
  int j;
  if (indice < 0 || indice >= n) return;
  // Todas as copias da obra (#208): "Assistir do comeco"/"Tirar" numa copia
  // deixava a do outro card retomando um registro que ja foi apagado.
  for (j = 0; j < n; j++) if (mesmaCopia(indice, j)) zerarUm(j);
  zerarUm(indice);
  mudou();
}
static void zerarUm(int indice) {
  // Os quatro campos que a home le para decidir se o card entra em "Continuar
  // assistindo" e o que escrever na legenda dele. Zerar so `progresso` deixaria
  // a linha "T1, E8 · 16 min" desenhada sobre um card sem barra.
  itens[indice].progresso   = 0;
  itens[indice].restanteMin = 0;
  itens[indice].temporada   = 0;
  itens[indice].episodio    = 0;
  mudou();
}

void cat_salvar_progresso(int indice, double posSeg, double durSeg) {
  cat_salvar_progresso_ep(indice,posSeg,durSeg,0,0);
}

void cat_salvar_progresso_ep(int indice, double posSeg, double durSeg, int temporada, int episodio) {
  indice = normalizarIndice(indice);
  if (indice < 0 || !itens[indice].imdb[0]) return;
  // O arquivo e de progresso.c: chave igual a do web, pendente, com hora. O
  // imdb do item pode vir composto ("tt123:4:9", itens do Trakt) — a funcao
  // corta e usa o episodio explicito quando ha.
  // PERSONAL SERVER ITEMS never enter the Nuvio account progress: the
  // server's own check-ins (jellyfin.c) are their authority, and an opaque
  // server id would sync to every device of the account. The card still
  // shows the new position in this session.
  if (jfid_e(itens[indice].imdb)) { cat_aplicar_progresso(indice, posSeg, durSeg, temporada, episodio); return; }
  if (!prog_gravar_local(itens[indice].imdb, temporada, episodio, posSeg, durSeg)) return;
  cat_aplicar_progresso(indice, posSeg, durSeg, temporada, episodio);
}

unsigned cat_revisao(void) { return __atomic_load_n(&catRevisao, __ATOMIC_ACQUIRE); }
unsigned cat_revisao_itens(void) { return __atomic_load_n(&catMudancas, __ATOMIC_ACQUIRE); }

// Leitores: epTrava so pelo tempo de ler dois inteiros. `indiceItem` alem de
// nFaixas e "sem episodios", e nao leitura fora do vetor: cat_acrescentar*
// publicavam `n` maior ANTES de garantirFaixas crescer as faixas.
int cat_n_episodios(int indiceItem) {
  int m = cat_n(), q = 0;
  // epQtd so nasce em garantirFaixas, que em cat_carregar vem DEPOIS de
  // aplicar o progresso do disco — e aplicar progresso de serie pergunta
  // pelos episodios. Sem esta guarda o arranque caia com progresso gravado.
  if (m < 1) return 0;
  indiceItem = ((indiceItem % m) + m) % m;
  pthread_mutex_lock(&epTrava);
  if (epQtd && indiceItem < nFaixas) q = epQtd[indiceItem];
  pthread_mutex_unlock(&epTrava);
  return q;
}

const CatEp *cat_episodio(int indiceItem, int i) {
  int m = cat_n();
  const CatEp *e = NULL;
  if (m < 1 || i < 0) return NULL;
  indiceItem = ((indiceItem % m) + m) % m;
  pthread_mutex_lock(&epTrava);
  if (epQtd && epIni && indiceItem < nFaixas && i < epQtd[indiceItem] &&
      epIni[indiceItem] + i < CAT_EP_MAX)
    e = &eps[epIni[indiceItem] + i];
  pthread_mutex_unlock(&epTrava);
  return e;
}

int cat_id_stream(int indiceItem, int t, int e, char *dst, unsigned tam) {
  const CatItem *c = cat_item(indiceItem);
  char base[96];
  int i, n;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  if (!c || !c->imdb[0]) return 0;
  idbase_copiar(c->imdb, base, sizeof base);
  if (t <= 0 || e <= 0) { snprintf(dst, tam, "%s", c->imdb); return 1; }
  if (!idbase_e_imdb(c->imdb)) {
    n = cat_n_episodios(indiceItem);
    for (i = 0; i < n; i++) {
      const CatEp *ep = cat_episodio(indiceItem, i);
      if (ep && ep->temporada == t && ep->episodio == e && ep->vid[0]) {
        snprintf(dst, tam, "%s", ep->vid);
        return 1;
      }
    }
    // Sem o video: a convencao dos addons de anime e "<id>:<episodio>".
    snprintf(dst, tam, "%s:%d", base, e);
    return 1;
  }
  snprintf(dst, tam, "%s:%d:%d", base, t, e);
  return 1;
}

int cat_copiar_por_id(const char *id, const char *tipo, CatItem *saida) {
  size_t tam;
  int i, melhor = -1;
  if (!id || !*id || !saida) return 0;
  tam = idbase_len(id);
  catTravar();
  for (i = 0; itens && i < n; i++) {
    if (idbase_len(itens[i].imdb) != tam || strncmp(itens[i].imdb, id, tam) ||
        (tipo && *tipo && strcmp(tipo_base(itens[i].tipo), tipo_base(tipo)))) continue;
    if (melhor < 0 || (!itens[melhor].poster[0] && itens[i].poster[0])) melhor = i;
  }
  if (melhor >= 0) *saida = itens[melhor];
  pthread_mutex_unlock(&pubTrava);
  return melhor >= 0;
}

int cat_n_fileiras(void) { return nFils; }
const CatFileira *cat_fileira(int r) {
  return (r >= 0 && r < nFils) ? &fils[r] : NULL;
}

int cat_home_apenas_fixas(void) {
  int r, i, inicial = 1;
  catTravar();
  for (r = 0; r < nFils; r++)
    if (strcmp(fils[r].chave, "continue_watching") &&
        strcmp(fils[r].chave, "social_activity")) { inicial = 0; break; }
  /* Lists may have been merged before any catalogue row. Replacing those
   * items with an early CW/social batch would erase ready watchlist data.
   * Treat unassigned metadata and collection/list flags conservatively too. */
  for (i = 0; inicial && i < n; i++) {
    int dentro = 0;
    if (!itens || itens[i].naLista || itens[i].naColecao) { inicial = 0; break; }
    for (r = 0; r < nFils; r++)
      if (i >= fils[r].ini && i - fils[r].ini < fils[r].n) { dentro = 1; break; }
    if (!dentro) inicial = 0;
  }
  pthread_mutex_unlock(&pubTrava);
  return inicial;
}

int cat_copiar_fileira(const char *chave, CatItem *saida, int max,
                       CatFileira *meta) {
  int r, qtd;
  if (!chave || !*chave || !saida || max < 1) return 0;
  catTravar();
  for (r = 0; r < nFils; r++) {
    CatFileira *f = &fils[r];
    if (strcmp(f->chave, chave) || f->n < 1) continue;
    qtd = f->n < max ? f->n : max;
    if (f->ini < 0 || f->ini + qtd > n || !itens) break;
    memcpy(saida, itens + f->ini, sizeof(CatItem) * (size_t)qtd);
    if (meta) *meta = *f;
    pthread_mutex_unlock(&pubTrava);
    return qtd;
  }
  pthread_mutex_unlock(&pubTrava);
  return 0;
}

// ACRESCENTA UM titulo ao fim do catalogo e devolve o indice dele.
//
// Existe para o titulo que veio de FORA: um credito na filmografia de um ator
// ou um item de "Mais como este" que o catalogo do dono nao tem. Sem isto o
// item ficava apagado e nao abria, o que deixava a filmografia decorativa.
//
// Usa a MESMA troca de bloco de cat_definir_tudo, pelo mesmo motivo (leitor no
// fio de desenho dentro do bloco antigo), com duas diferencas:
//   - acrescenta no FIM, entao as janelas (ini,n) das fileiras continuam
//     valendo e nao precisam ser derrubadas;
//   - `n` NAO e zerado: subir a contagem depois que o bloco novo ja esta
//     publicado e seguro, e zerar faria a home piscar a cada titulo aberto.
// Sob pubTrava; as APIs publicas nao chamam umas as outras com a trava presa.
static void definir_na_lista(int i, int naLista) {
  if (!itens || n <= 0 || i < 0 || i >= n) return;
  if (itens[i].naLista == (naLista ? 1 : 0)) return;
  itens[i].naLista = naLista ? 1 : 0;
  mudou();
}

void cat_definir_na_lista(int i, int naLista) {
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  definir_na_lista(i, naLista);
  pthread_mutex_unlock(&pubTrava);
}

// Um mesmo titulo vive em varias fileiras. A contagem, o ponteiro e cada
// marca pertencem ao mesmo bloco publicado durante toda a varredura.
int cat_definir_na_lista_imdb(const char *imdb, int naLista) {
  int i, k = 0;
  char id[64];
  if (!imdb || !imdb[0]) return 0;
  snprintf(id, sizeof id, "%s", imdb);
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  for (i = 0; itens && i < n; i++)
    if (!strcmp(itens[i].imdb, id)) { definir_na_lista(i, naLista); k++; }
  pthread_mutex_unlock(&pubTrava);
  return k;
}
int cat_imdb_na_lista(const char *imdb) {
  int i, achou = 0;
  char id[64];
  if (!imdb || !imdb[0]) return 0;
  snprintf(id, sizeof id, "%s", imdb);
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  for (i = 0; itens && i < n; i++)
    if (itens[i].naLista && !strcmp(itens[i].imdb, id)) { achou = 1; break; }
  pthread_mutex_unlock(&pubTrava);
  return achou;
}

// O indice so e valido se ainda aponta ao titulo da resposta. Copiar a
// resposta antes da trava tambem aceita um item pertencente ao bloco atual.
void cat_atualizar_item(int i, const CatItem *item) {
  CatItem copia;
  if (!item) return;
  copia = *item;
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  if (itens && i >= 0 && i < n &&
      !strcmp(itens[i].imdb, copia.imdb) &&
      (!tipo_certo(itens[i].tipo) || !strcmp(tipo_base(itens[i].tipo), tipo_base(copia.tipo)))) {
    itens[i] = copia;   // an uncertain stored type may be resolved to movie/series
    mudou();
  }
  pthread_mutex_unlock(&pubTrava);
}

void cat_atualizar_item_sem_abas(int i, const CatItem *item) {
  CatItem copia;
  if (!item) return;
  copia = *item;
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  if (itens && i >= 0 && i < n &&
      !strcmp(itens[i].imdb, copia.imdb) &&
      (!tipo_certo(itens[i].tipo) || !strcmp(tipo_base(itens[i].tipo), tipo_base(copia.tipo)))) {
    copia.nTemporadas = itens[i].nTemporadas;
    memcpy(copia.temporadas, itens[i].temporadas, sizeof copia.temporadas);
    itens[i] = copia;
    mudou();
  }
  pthread_mutex_unlock(&pubTrava);
}

// Copia do item `i` sob pubTrava: o bloco nao pode ser trocado nem liberado
// no meio. Para fios fora do desenho, que nao tem a garantia de cat_item()
// (o ponteiro so vale ate o fim do quadro).
int cat_copiar_item(int i, CatItem *saida) {
  int ok = 0;
  if (!saida) return 0;
  catTravar();
  if (itens && i >= 0 && i < n) { *saida = itens[i]; ok = 1; }
  pthread_mutex_unlock(&pubTrava);
  return ok;
}

// Escrita PARCIAL: so a sinopse (e o titulo, se o item nao tem). Revalida a
// identidade e o "ainda sem sinopse" sob a trava; qualquer outro campo que
// outro fio tenha mudado desde a leitura fica como esta (revisao 2.0.3).
int cat_completar_sinopse(int i, const char *imdb, const char *sinopse,
                          const char *titulo) {
  char id[sizeof itens->imdb];
  int ok = 0;
  if (!imdb || !imdb[0] || !sinopse || !sinopse[0]) return 0;
  snprintf(id, sizeof id, "%s", imdb);
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  if (itens && i >= 0 && i < n && !strcmp(itens[i].imdb, id) && !itens[i].sinopse[0]) {
    snprintf(itens[i].sinopse, sizeof itens[i].sinopse, "%s", sinopse);
    if (!itens[i].titulo[0] && titulo && titulo[0])
      snprintf(itens[i].titulo, sizeof itens[i].titulo, "%s", titulo);
    mudou();
    ok = 1;
  }
  pthread_mutex_unlock(&pubTrava);
  return ok;
}

// Escrita PARCIAL do texto localizado (fioLocalizar): titulo, sinopse, logo e
// fundo (backdrop e backdropCatalogo), cada um so se veio e e diferente — campo
// vazio nao apaga, como aplicarLocItem. Revalida a identidade sob a trava;
// nenhum outro campo do item e tocado (revisao 2.0.3, como cat_completar_sinopse).
int cat_aplicar_localizado(int i, const char *imdb, const char *titulo,
                           const char *sinopse, const char *logo, const char *fundo) {
  char id[sizeof itens->imdb];
  int ok = 0;
  if (!imdb || !imdb[0]) return 0;
  snprintf(id, sizeof id, "%s", imdb);
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  if (itens && i >= 0 && i < n && !strcmp(itens[i].imdb, id)) {
    CatItem *c = &itens[i];
    if (titulo && titulo[0] && strcmp(titulo, c->titulo)) {
      snprintf(c->titulo, sizeof c->titulo, "%s", titulo); ok = 1; }
    if (sinopse && sinopse[0] && strcmp(sinopse, c->sinopse)) {
      snprintf(c->sinopse, sizeof c->sinopse, "%s", sinopse); ok = 1; }
    if (logo && logo[0] && strcmp(logo, c->logo)) {
      snprintf(c->logo, sizeof c->logo, "%s", logo); ok = 1; }
    if (fundo && fundo[0] && strcmp(fundo, c->backdrop)) {
      snprintf(c->backdrop, sizeof c->backdrop, "%s", fundo);
      snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", fundo);
      ok = 1;
    }
    if (ok) mudou();
  }
  pthread_mutex_unlock(&pubTrava);
  return ok;
}

// Acrescenta N de UMA VEZ. cat_acrescentar copia o catalogo inteiro a cada
// chamada, e a busca a chamava POR RESULTADO: com 300 titulos no acervo sao
// ~2,3 MB por copia, vezes 40 resultados, no fio de DESENHO, a cada tecla. Era
// o travamento que aparecia como "a busca engasga quando digito".
//
// Uma troca de bloco so, seguindo a mesma ordem de cat_definir: zera `n` antes
// de trocar o ponteiro (o desenho ve catalogo vazio por um quadro em vez de ler
// memoria liberada) e nao libera o bloco velho aqui — um leitor pode estar
// dentro dele; ele morre em cat_quadro, depois do quadro em curso.
int cat_acrescentar_lote(const CatItem *v, int qtd, int *saidaIdx) {
  CatItem *novo;
  int novoN, k;
  if (!v || qtd < 1) return 0;
  // `n` SO SE LE COM A TRAVA (queda "free(): invalid pointer", 1.7.0 .tpk).
  // O tamanho era calculado antes dela: se a descoberta ou o fio de
  // "Continuar assistindo" publicasse um catalogo maior nesse meio, o memcpy
  // abaixo copiava o `n` novo para dentro do bloco do `n` velho e passava do
  // fim — heap corrompido, abort no proximo free. tests/catcorrida.sh.
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  if (n < 1) { pthread_mutex_unlock(&pubTrava); return 0; }
  if (n + qtd > CAT_MAX) qtd = CAT_MAX - n;
  if (qtd < 1) { pthread_mutex_unlock(&pubTrava); return 0; }
  novoN = n + qtd;
  novo = malloc(sizeof(CatItem) * (size_t)novoN);
  if (!novo) { pthread_mutex_unlock(&pubTrava); return 0; }
  memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  memcpy(&novo[n], v, sizeof(CatItem) * (size_t)qtd);
  { int k; for (k = 0; k < qtd; k++) {
      if (v[k].poster[0])   arte_reserva_registrar(v[k].poster,   v[k].imdb, 1);
      if (v[k].backdrop[0]) arte_reserva_registrar(v[k].backdrop, v[k].imdb, 0); } }
  if (saidaIdx) for (k = 0; k < qtd; k++) saidaIdx[k] = n + k;
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = novoN;
  // Faixas ANTES de `n` subir: um leitor que ve o `n` novo acha a faixa dele.
  pthread_mutex_lock(&epTrava);
  garantirFaixas(nAlocado);
  pthread_mutex_unlock(&epTrava);
  __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
  mudou();
  pthread_mutex_unlock(&pubTrava);
  return qtd;
}

// Ver catalogo.h. Mesma troca de bloco de cat_acrescentar_lote (acrescenta no
// FIM, janelas continuam valendo, `n` nao zera), feita inteira sob pubTrava:
// quem chama e o fio da descoberta, e o de "Continuar assistindo" pode estar
// trocando o bloco ao mesmo tempo.
int cat_mesclar_listas(const CatItem *v, int qtd) {
  CatItem *novo;
  int m, k, i, marcados = 0, novos = 0;
  if (!v || qtd < 1) return 0;
  catTravar();
  // NADA MUDA, NADA SE COPIA. E o caso comum da volta silenciosa (a mesma
  // lista de cinco minutos atras): cada troca de bloco copia o catalogo
  // inteiro, e o CatItem passa de 15 KB.
  { int falta = 0;
    for (k = 0; k < qtd && !falta; k++) {
      int achou = 0;
      if (!v[k].imdb[0]) continue;
      for (i = 0; i < n && !achou; i++)
        if (!strcmp(itens[i].imdb, v[k].imdb) &&
            (!v[k].naLista || itens[i].naLista) &&
            (!v[k].naColecao || itens[i].naColecao)) achou = 1;
      if (!achou) falta = 1;
    }
    if (!falta) { pthread_mutex_unlock(&pubTrava); return 0; } }
  novo = malloc(sizeof(CatItem) * (size_t)(n + qtd > 0 ? n + qtd : 1));
  if (!novo) { pthread_mutex_unlock(&pubTrava); return 0; }
  if (n > 0) memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  m = n;
  for (k = 0; k < qtd; k++) {
    int achou = 0;
    if (!v[k].imdb[0]) continue;
    for (i = 0; i < m; i++) {
      if (strcmp(novo[i].imdb, v[k].imdb)) continue;
      if (v[k].naLista)   novo[i].naLista = 1;
      if (v[k].naColecao) novo[i].naColecao = 1;
      achou = 1;
    }
    if (achou) { marcados++; continue; }
    if (m >= CAT_MAX) continue;
    novo[m] = v[k];
    if (v[k].poster[0])   arte_reserva_registrar(v[k].poster,   v[k].imdb, 1);
    if (v[k].backdrop[0]) arte_reserva_registrar(v[k].backdrop, v[k].imdb, 0);
    m++; novos++;
  }
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = m;
  pthread_mutex_lock(&epTrava);
  garantirFaixas(nAlocado);
  pthread_mutex_unlock(&epTrava);
  __atomic_store_n(&n, m, __ATOMIC_RELEASE);
  mudou();
  pthread_mutex_unlock(&pubTrava);
  printf("[cat] listas do Trakt na tela: %d marcado(s), %d novo(s)\n", marcados, novos);
  fflush(stdout);
  return novos;
}

int cat_acrescentar(const CatItem *item) {
  CatItem *novo;
  int novoN;
  if (!item) return -1;
  // Mesma regra de cat_acrescentar_lote: `n` so com a trava.
  CAT_TESTE_ANTES_TRAVA();
  catTravar();
  if (n < 1 || n >= CAT_MAX) { pthread_mutex_unlock(&pubTrava); return -1; }
  novoN = n + 1;
  novo = malloc(sizeof(CatItem) * (size_t)novoN);
  if (!novo) { pthread_mutex_unlock(&pubTrava); return -1; }
  memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  memcpy(&novo[n], item, sizeof(CatItem));
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = novoN;
  pthread_mutex_lock(&epTrava);
  garantirFaixas(nAlocado);
  pthread_mutex_unlock(&epTrava);
  __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
  mudou();
  if (novo[novoN - 1].poster[0]) arte_reserva_registrar(novo[novoN - 1].poster, novo[novoN - 1].imdb, 1);
  if (novo[novoN - 1].backdrop[0]) arte_reserva_registrar(novo[novoN - 1].backdrop, novo[novoN - 1].imdb, 0);
  pthread_mutex_unlock(&pubTrava);
  return novoN - 1;
}

void cat_definir(const CatItem *lista, int qtd) {
  cat_definir_tudo(lista, qtd, NULL, 0);
}

void cat_republicar_fileiras(const CatFileira *novasFils, int nNovas) {
  int k, q, v = 0;
  // nNovas == 0 e valido (#319): tirar a ultima fileira tem de esvaziar a tela;
  // com o guarda antigo (< 1) a fileira de addon removido ficava publicada.
  if (!novasFils || nNovas < 0 || n < 1) return;
  q = nNovas > CAT_FIL_MAX ? CAT_FIL_MAX : nNovas;
  catTravar();
  // A JANELA QUE ESTA PUBLICADA VENCE A DA MONTAGEM, para a mesma chave. Quem
  // chama (desc_remontar_fileiras) republica o retrato da ultima montagem
  // completa, e ele nao sabe do que mudou depois SEM REDE: um card tirado de
  // "Continuar assistindo" (cat_tirar_continuar) ou uma refacao da fileira
  // (cat_trocar_continuar, que desliza o `ini` das seguintes). Sem isto o
  // proximo sync que remonta as fileiras devolvia n=3 a uma janela que ja era
  // n=2 — o card "voltava" como o primeiro item da fileira de baixo.
  { static CatFileira ajust[CAT_FIL_MAX];
    int r;
    for (k = 0; k < q; k++) {
      ajust[k] = novasFils[k];
      for (r = 0; r < nFils; r++)
        if (!strcmp(fils[r].chave, ajust[k].chave)) {
          ajust[k].ini = fils[r].ini; ajust[k].n = fils[r].n; break;
        }
      // Chave que saiu da lista publicada porque ESVAZIOU (o ultimo card de
      // "Continuar assistindo" tirado) nao volta com a janela velha.
      if (r == nFils && !strcmp(ajust[k].chave, "continue_watching")) ajust[k].n = 0;
    }
    // E a que NASCEU depois da montagem (cat_trocar_continuar cria a fileira
    // quando o primeiro titulo entra em progresso) nao some: entra na frente,
    // onde montar() e cat_trocar_continuar a poem.
    { int cw = fileiraContinuar(), tem = 0;
      for (k = 0; k < q; k++) if (!strcmp(ajust[k].chave, "continue_watching")) tem = 1;
      if (cw >= 0 && !tem && q < CAT_FIL_MAX) {
        memmove(ajust + 1, ajust, sizeof *ajust * (size_t)q);
        ajust[0] = fils[cw];
        q++;
      } }
    novasFils = ajust; }
  // IGUAL AO QUE ESTA: nao mexe. A remontagem sem rede roda a cada sync e a
  // cada ajuste de fileira; quando o resultado e o mesmo conjunto na mesma
  // ordem, zerar e reescrever as fileiras faz a home se reconstruir por nada.
  if (q == nFils) {
    int igual = 1;
    for (k = 0; k < q && igual; k++)
      if (strcmp(novasFils[k].chave, fils[k].chave) ||
          strcmp(novasFils[k].base, fils[k].base) ||
          strcmp(novasFils[k].catId, fils[k].catId) ||
          novasFils[k].ini != fils[k].ini || novasFils[k].n != fils[k].n ||
          novasFils[k].estado != fils[k].estado ||
          novasFils[k].socialGeracao != fils[k].socialGeracao) igual = 0;
    if (igual) { pthread_mutex_unlock(&pubTrava); return; }
  }
  nFils = 0;                 // ver a nota em catalogo.h: zera antes de mexer
  for (k = 0; k < q; k++) {
    CatFileira f = novasFils[k];
    if (f.ini < 0 || f.ini > n || (f.n < 1 && !f.estado)) continue;
    if (f.ini + f.n > n) f.n = n - f.ini;
    fils[v++] = f;
  }
  nFils = v;
  // BUMPA A REVISAO sempre que as fileiras mudaram. cat_definir_tudo e
  // cat_trocar_continuar ja fazem isto; sem isto aqui, cat_republicar_fileiras
  // (usada por desc_remontar_fileiras na mudanca de ordem/colecao/limite sem
  // rede) trocava fils[] sem avisar ninguem — e a home so percebia na proxima
  // publicacao da descoberta. Agora cat_revisao() e um guarda correto do
  // estado das fileiras, usado por sincronizarFileiras em home.c.
  revisaoSobe(); mudou();
  pthread_mutex_unlock(&pubTrava);
}

void cat_definir_tudo(const CatItem *lista, int qtd,
                      const CatFileira *novasFils, int nNovas) {
  double tIni = cat_relogio_ms(), tFora = 0, tTrava = 0, tProg = 0;
  if (qtd < 0 || qtd > CAT_MAX || (qtd > 0 && !lista)) return;
  // TROCA DE BLOCO, sem realloc no lugar.
  //
  // cat_definir roda no fio da descoberta enquanto o desenho le itens[] no fio
  // principal. Com realloc, o bloco antigo e LIBERADO e o desenho passa a ler
  // memoria morta — foi assim que o app comecou a morrer em home_desenhar
  // assim que o catalogo cresceu de 40 para 303. Enquanto era vetor estatico o
  // endereco nunca mudava e o problema nao existia.
  //
  // A ordem das tres linhas abaixo e o que torna isto seguro sem trava:
  // zerar `n` primeiro faz o desenho tratar o catalogo como vazio por um
  // quadro (nao desenha nada), e so depois o ponteiro e a contagem sobem. O
  // bloco antigo NAO e liberado aqui: um leitor pode estar dentro dele neste
  // instante. Ele morre em cat_quadro, quando o quadro em curso termina (ver
  // aposentar, no topo).
  {
    int novoN = qtd > CAT_MAX ? CAT_MAX : qtd;
    CatItem *novo = malloc(sizeof(CatItem) * (size_t)(novoN > 0 ? novoN : 1));
    if (!novo) return;
    if (novoN > 0) memcpy(novo, lista, sizeof(CatItem) * (size_t)novoN);
    // Historico de ORDEM por fileira (tendencia.h), ANTES da troca e sobre os
    // parametros — le e grava arquivo, e depois da troca `novo` pode ser
    // liberado por uma publicacao seguinte.
    // Arte de host de addon -> imdb, para a reserva de arte (#67).
    { int k; for (k = 0; k < novoN; k++) {
        if (novo[k].poster[0])   arte_reserva_registrar(novo[k].poster,   novo[k].imdb, 1);
        if (novo[k].backdrop[0]) arte_reserva_registrar(novo[k].backdrop, novo[k].imdb, 0); } }
    if (novasFils) {
      int k;
      for (k = 0; k < nNovas && k < CAT_FIL_MAX; k++) {
        CatFileira f = novasFils[k];
        if (f.ini < 0 || f.ini > novoN || (f.n < 1 && !f.estado)) continue;
        if (f.ini + f.n > novoN) f.n = novoN - f.ini;
        if (f.n > 0) tend_registrar(&f, novo);
      }
    }
    // O progresso e por imdb e vive em progresso.c, entao sobrevive a troca —
    // mas precisa ser reaplicado, porque os itens novos nasceram zerados. E aqui
    // que uma linha da conta que antes nao casava com nada passa a casar, quando
    // o titulo dela entra no catalogo. No bloco NOVO, antes de publicar e fora
    // da trava: ver aplicarProgressoEm.
    tProg = cat_relogio_ms();
    { int k; ProgRegistro *regs = lerProgresso(&k);
      aplicarProgressoEm(novo, novoN, regs, k);
      free(regs); }
    tProg = cat_relogio_ms() - tProg;
    // As fileiras caem JUNTO com `n`. Elas sao janelas (ini,n) no vetor de
    // itens; deixar as antigas de pe por um quadro enquanto o vetor troca faz o
    // desenho ler fora da faixa.
    tFora = cat_relogio_ms() - tIni;
    catTravar();
    tTrava = cat_relogio_ms();
    __atomic_store_n(&n, 0, __ATOMIC_RELEASE);
    nFils = 0;
    aposentar(itens);
    __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
    nAlocado = novoN;
    // Episodios do catalogo anterior nao valem para o novo: os indices mudaram.
    // Zerados com `n` em 0, ANTES de o `n` novo subir — nenhum leitor ve item
    // novo com faixa velha. Era feito depois de soltar pubTrava (#203).
    pthread_mutex_lock(&epTrava);
    zerarFaixas(nAlocado);
    pthread_mutex_unlock(&epTrava);
    __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
    if (novasFils && nNovas > 0) {
      int k, q = nNovas > CAT_FIL_MAX ? CAT_FIL_MAX : nNovas;
      int v = 0;
      for (k = 0; k < q; k++) {
        CatFileira f = novasFils[k];
        // Corta a janela pelo que sobrou de verdade. Um catalogo que respondeu
        // menos itens do que o esperado deixaria a fileira apontando para o
        // vizinho.
        if (f.ini < 0 || f.ini > n || (f.n < 1 && !f.estado)) continue;
        if (f.ini + f.n > n) f.n = n - f.ini;
        fils[v++] = f;
      }
      nFils = v;
    }
    // O CICLO COMPLETO TAMBEM NAO TRAZ DE VOLTA. montar() chama
    // montarContinuar no comeco e publica aqui dezenas de segundos depois (os
    // manifestos): uma remocao feita nesse meio passaria. Ver a mesma poda em
    // cat_trocar_continuar. As faixas de episodio que ela desloca ja foram
    // zeradas acima.
    podarContinuar(removidoVence, NULL);
    revisaoSobe(); mudou();
    pthread_mutex_unlock(&pubTrava);
    tTrava = cat_relogio_ms() - tTrava;
  }
  printf("[perf] cat_definir_tudo: %d itens, fora da trava %.1f ms, DENTRO da trava %.1f ms, progresso do disco %.1f ms (fora da trava), total %.1f ms\n",
         qtd, tFora, tTrava, tProg, cat_relogio_ms() - tIni);
  fflush(stdout);
}

// TROCA SO A JANELA DE "CONTINUAR ASSISTINDO" (issue #38).
//
// A fileira so era refeita dentro de montar(), no ciclo completo da descoberta.
// Fora dele, nada a recompunha: sair do player atualizava o progresso do item
// no lugar (cat_salvar_progresso_ep), mas um titulo que ENTROU em progresso
// nao aparecia na fileira e um que TERMINOU nao saia dela ate o ciclo
// seguinte — o "nao atualiza ou demora" do relato. O progresso da conta, que
// chega pelo sync fora de qualquer ciclo, caia no mesmo vazio.
//
// A cirurgia e uma troca de bloco igual a de cat_definir_tudo: a janela da
// fileira fica sempre em ini=0 (os dois pontos de publicacao a poem la), entao
// o vetor novo e [itens novos][resto do acervo a partir do fim da janela
// velha]. As fileiras seguintes andam `delta` posicoes no `ini` — janelas sao
// disjuntas, entao nenhuma outra muda de conteudo. Fileira esvaziada sai da
// lista; fileira que nao existia entra na posicao 0, onde montar() a poria.
//
// Roda sob pubTrava: a descoberta pode estar trocando o catalogo neste mesmo
// instante, e duas trocas simultaneas liberariam o mesmo bloco duas vezes.
void cat_trocar_continuar(const CatItem *lista, int qtd) {
  // static (40 KB): so e usado depois de pubTrava, que serializa as chamadas.
  static CatFileira novas[CAT_FIL_MAX];
  CatItem *novo;
  int r, cw = -1, cwIni = 0, cwN = 0, delta, novoN, nv = 0, nRegs;
  double tTrava, tProg;
  // O disco e lido ANTES da trava (#203); a aplicacao e no bloco novo, antes
  // de ele ser publicado. Ver aplicarProgressoEm.
  ProgRegistro *regs;
  if (qtd < 0) qtd = 0;
  regs = lerProgresso(&nRegs);
  catTravar();
  tTrava = cat_relogio_ms();
  for (r = 0; r < nFils; r++)
    if (!strcmp(fils[r].chave, "continue_watching")) {
      cw = r; cwIni = fils[r].ini; cwN = fils[r].n; break;
    }
  delta = qtd - cwN;
  novoN = n + delta;
  if (novoN > CAT_MAX) { qtd -= novoN - CAT_MAX; delta = qtd - cwN; novoN = CAT_MAX; }
  if (novoN < 0) { pthread_mutex_unlock(&pubTrava); free(regs); return; }
  novo = malloc(sizeof(CatItem) * (size_t)(novoN > 0 ? novoN : 1));
  if (!novo) { pthread_mutex_unlock(&pubTrava); free(regs); return; }
  if (cwIni) memcpy(novo, itens, sizeof(CatItem) * (size_t)cwIni);
  if (qtd) memcpy(novo + cwIni, lista, sizeof(CatItem) * (size_t)qtd);
  if (n - cwIni - cwN > 0)
    memcpy(novo + cwIni + qtd, itens + cwIni + cwN,
           sizeof(CatItem) * (size_t)(n - cwIni - cwN));
  for (r = 0; r < nFils; r++) {
    CatFileira f = fils[r];
    if (r == cw) { f.n = qtd; }
    else if (f.ini >= cwIni + cwN) f.ini += delta;
    if (f.n < 1 && !f.estado) continue;
    novas[nv++] = f;
  }
  if (cw < 0 && qtd > 0 && nv < CAT_FIL_MAX) {
    memmove(novas + 1, novas, sizeof *novas * (size_t)nv);
    memset(&novas[0], 0, sizeof novas[0]);
    snprintf(novas[0].chave,  sizeof novas[0].chave,  "continue_watching");
    snprintf(novas[0].titulo, sizeof novas[0].titulo, "Continuar assistindo");
    snprintf(novas[0].tipo,   sizeof novas[0].tipo,   "movie");
    novas[0].ini = 0; novas[0].n = qtd;
    nv++;
  }
  // Ainda privado: ninguem mais enxerga `novo`. O custo fica dentro da trava
  // (o bloco so pode ser montado com ela), e e medido abaixo.
  tProg = cat_relogio_ms();
  aplicarProgressoEm(novo, novoN, regs, nRegs);
  tProg = cat_relogio_ms() - tProg;
  __atomic_store_n(&n, 0, __ATOMIC_RELEASE);
  nFils = 0;
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = novoN;
  // Os indices andaram: nenhuma faixa de episodio vale para o item novo.
  // Com `n` em 0 e sob pubTrava (#203) — antes era depois de solta-la, e o
  // realloc de garantirFaixas corria com o desenho lendo epQtd.
  pthread_mutex_lock(&epTrava);
  zerarFaixas(nAlocado);
  pthread_mutex_unlock(&epTrava);
  __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
  memcpy(fils, novas, sizeof *novas * (size_t)nv);
  nFils = nv;
  // A REFACAO NAO TRAZ DE VOLTA O QUE FOI TIRADO. montarContinuar ja filtra,
  // mas um fio que montou ANTES da remocao e publica DEPOIS dela passaria o
  // item velho; sob a mesma trava de cat_tirar_continuar, ou a remocao ja
  // marcou (e a poda pega aqui) ou ela vem depois (e tira por imdb).
  { int podados = podarContinuar(removidoVence, NULL);
    qtd -= podados; }
  revisaoSobe(); mudou();
  pthread_mutex_unlock(&pubTrava);
  tTrava = cat_relogio_ms() - tTrava;
  free(regs);
  printf("[cat] continuar assistindo refeita: %d item(ns), trava %.1f ms (progresso %.1f ms)\n",
         qtd, tTrava, tProg);
  fflush(stdout);
}

// Roda no fio buscarEps (um por pagina de serie) e no de montar(), com o
// desenho lendo. Tudo sob epTrava (#203): antes, epIni/epQtd eram escritos no
// ponteiro que garantirFaixas, noutro fio, podia ter acabado de liberar.
void cat_definir_episodios(int indiceItem, const CatEp *lista, int qtd) {
  int m = cat_n();
  if (!lista || qtd < 1 || m < 1) return;
  indiceItem = ((indiceItem % m) + m) % m;
  if (qtd > CAT_EP_MAX) qtd = CAT_EP_MAX;
  pthread_mutex_lock(&epTrava);
  // Faixa ainda nao criada para este indice (o `n` subiu antes): nao ha onde
  // escrever. Quem pediu repede quando a revisao andar.
  if (!epIni || !epQtd || indiceItem >= nFaixas) {
    pthread_mutex_unlock(&epTrava);
    return;
  }
  // Anexa no fim do vetor comum. Trocar de temporada varias vezes acumula, mas
  // o teto de CAT_EP_MAX segura e o custo de compactar nao se paga.
  if (nEps + qtd > CAT_EP_MAX) {
    nEps = 0;
    // Invalidar os indices antes de reutilizar o armazenamento: senao outra
    // serie passa a exibir os episodios da obra que acabou de ser carregada.
    memset(epQtd,0,(size_t)nFaixas*sizeof *epQtd);
    memset(epIni,0,(size_t)nFaixas*sizeof *epIni);
    epGeracaoSobe();   // as outras series perderam a lista: quem mostra repede
  }
  memcpy(&eps[nEps], lista, sizeof(CatEp) * (size_t)qtd);
  epIni[indiceItem] = nEps;
  epQtd[indiceItem] = qtd;
  nEps += qtd;
  pthread_mutex_unlock(&epTrava);
}

// Generos de um item, como uma lista de trechos separados por " · ". O primeiro
// campo e sempre "Filme"/"Programa de TV" e nao conta como genero.
static int compartilhaGenero(const CatItem *a, const CatItem *b) {
  const char *p = a->genero;
  int primeiro = 1;
  while (p && *p) {
    const char *sep = strstr(p, "\xc2\xb7");
    char termo[64];
    size_t n;
    if (!sep) break;
    p = sep + 2;
    while (*p == ' ') p++;
    sep = strstr(p, "\xc2\xb7");
    n = sep ? (size_t)(sep - p) : strlen(p);
    while (n && (p[n - 1] == ' ')) n--;
    if (n && n < sizeof termo) {
      memcpy(termo, p, n);
      termo[n] = 0;
      if (strstr(b->genero, termo)) return 1;
    }
    primeiro = 0;
    if (!sep) break;
  }
  (void)primeiro;
  return 0;
}

int cat_similares(int indice, int *saida, int max) {
  int m = cat_n(), i, k = 0;
  const CatItem *base;
  if (m < 1 || !saida || max < 1) return 0;
  indice = ((indice % m) + m) % m;
  base = &itens[indice];
  for (i = 0; i < m && k < max; i++) {
    if (i == indice) continue;
    if (base->tipo[0] && itens[i].tipo[0] && strcmp(base->tipo, itens[i].tipo)) continue;
    if (!compartilhaGenero(base, &itens[i])) continue;
    saida[k++] = i;
  }
  // Sem nenhum genero em comum a fileira ficaria vazia; ai vale mais mostrar os
  // vizinhos do mesmo tipo que sumir com a secao.
  for (i = 0; i < m && k < max; i++) {
    int j, ja = 0;
    if (i == indice) continue;
    for (j = 0; j < k; j++) if (saida[j] == i) { ja = 1; break; }
    if (ja) continue;
    if (base->tipo[0] && itens[i].tipo[0] && strcmp(base->tipo, itens[i].tipo)) continue;
    saida[k++] = i;
  }
  // Nota alta primeiro.
  { int a, b, t;
    for (a = 0; a < k; a++)
      for (b = a + 1; b < k; b++)
        if (itens[saida[b]].nota > itens[saida[a]].nota) {
          t = saida[a]; saida[a] = saida[b]; saida[b] = t;
        } }
  return k;
}
