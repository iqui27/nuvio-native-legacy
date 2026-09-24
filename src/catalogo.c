#include "catalogo.h"
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
static int aplicarProgressoDoDisco(void);

// --- QUEM PODE TROCAR O VETOR ------------------------------------------------
// Dois publicadores podem se encontrar: o fio da descoberta (cat_definir_tudo)
// e o fio que refaz so a fileira "Continuar assistindo" (cat_trocar_continuar,
// ver desc_refazer_continuar). A trava e so ENTRE publicadores — o desenho
// nunca a pega e segue lendo pelo protocolo de ordem de escrita: `n` zera
// antes de o ponteiro trocar, e o bloco velho nao e liberado na hora.
static pthread_mutex_t pubTrava = PTHREAD_MUTEX_INITIALIZER;
#include <string.h>
#include <stdlib.h>

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
  pthread_mutex_lock(&pubTrava);
  if (nAposentados > 1) {
    for (k = 0; k < nAposentados - 1; k++) free(aposentados[k]);
    aposentados[0] = aposentados[nAposentados - 1];
    nAposentados = 1;
  }
  pthread_mutex_unlock(&pubTrava);
}

int cat_blocos_aposentados(void) {
  int q;
  pthread_mutex_lock(&pubTrava);
  q = nAposentados;
  pthread_mutex_unlock(&pubTrava);
  return q;
}

// Alocado conforme chega, nao dimensionado por um numero chutado.
static CatItem *itens;
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
// SOBE A CADA TROCA DO CATALOGO INTEIRO. Quem guarda um indice (a pagina de
// detalhe guarda) precisa saber que ele deixou de valer — e, no caso dos
// episodios, que a faixa dele foi ZERADA junto e ninguem vai repedir sozinho.
static unsigned catRevisao;
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

static CatHistorico historico[CAT_MAX];
static int nHistorico;

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

static int historico_pos(const char *imdb, const char *tipo, int criar) {
  char id[32];
  int i;
  id_base(imdb, id, sizeof id);
  if (!id[0]) return -1;
  for (i = 0; i < nHistorico; i++)
    if (!strcmp(historico[i].imdb, id) &&
        !strcmp(historico[i].tipo, tipo_base(tipo))) return i;
  if (!criar || nHistorico >= CAT_MAX) return -1;
  snprintf(historico[nHistorico].imdb, sizeof historico[nHistorico].imdb, "%s", id);
  snprintf(historico[nHistorico].tipo, sizeof historico[nHistorico].tipo, "%s", tipo_base(tipo));
  return nHistorico++;
}

// Leitura interna da modal: -1 = historico ainda nao consultado, 0 = nao
// visto confirmado, 1 = visto confirmado.
int cat_historico_estado_item(int indice) {
  const CatItem *it = cat_item(indice);
  int p;
  if (!it || !it->imdb[0]) return -1;
  p = historico_pos(it->imdb, it->tipo, 0);
  return p >= 0 && historico[p].conhecido ? historico[p].visto : -1;
}

// Atualiza o retrato de historico somente depois de uma resposta 2xx do
// Trakt. A chave e o IMDb sem sufixo de episodio, nunca o indice do vetor.
void cat_historico_definir_id(const char *imdb, const char *tipo, int visto) {
  int p = historico_pos(imdb, tipo, 1);
  if (p < 0) return;
  historico[p].conhecido = 1;
  historico[p].visto = visto ? 1 : 0;
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
// mudam — e la a chamada e explicita, logo abaixo de `nEps = 0`.
static void garantirFaixas(int quantos) {
  int *a, *b;
  if (quantos < 1) return;
  a = realloc(epIni, sizeof(int) * (size_t)quantos);
  b = realloc(epQtd, sizeof(int) * (size_t)quantos);
  if (a) epIni = a;
  if (b) epQtd = b;
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
  garantirFaixas(quantos);
}
static int n = 0;

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
  { int aplicados = aplicarProgressoDoDisco();
    if (aplicados) printf("catalogo: %d progressos deste app\n", aplicados); }

  // episodios.txt: "indice|temporada|episodio|nome|duracao|data|sinopse".
  // Indice na frente porque so parte dos titulos tem episodio — uma linha por
  // titulo, como nos outros arquivos, desperdicaria a maioria das linhas.
  snprintf(caminho, sizeof caminho, "%s/episodios.txt", dirArte);
  { FILE *fe2 = fopen(caminho, "r");
    nEps = 0;
    zerarFaixas(nAlocado);   // carga do zero: nenhuma faixa antiga vale
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
      printf("catalogo: %d episodios\n", nEps);
    }
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
int ajustes_idioma_ingles(void);

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
#define CACHE_VERSAO_SO_P1 5

typedef struct {
  unsigned magia, versao, tamItem, tamFileira;
  int nItens, nFileiras;
  // DE QUEM E ESTE CACHE. Ver a nota de cat_apagar_cache em catalogo.h: sem
  // estes dois campos, a primeira abertura depois de trocar de conta ou de
  // perfil mostrava a home da ANTERIOR — watchlist, continuar assistindo e o
  // feed de amigos com nome e avatar — ate a rede substituir. E nao e so
  // estetico: cada CatFileira leva `base[600]`, campo desse tamanho porque o
  // Xperience embute um JWT no CAMINHO (ver catalogo.h). O arquivo carrega
  // credencial de addon do usuario anterior.
  char usuario[64];   // `sub` do JWT; "" quando deslogado
  int  perfil;        // perfis_ativo()
  int  ingles;        // ajustes_idioma_ingles() quando o arquivo foi escrito
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
  c.ingles = ajustes_idioma_ingles();
  c.tamItem = (unsigned)sizeof(CatItem);
  c.tamFileira = (unsigned)sizeof(CatFileira);
  c.nItens = n; c.nFileiras = nFils;
  identidadeAtual(c.usuario, sizeof c.usuario, &c.perfil);
  if (!donoEsperado || strcmp(c.usuario, donoEsperado) || c.perfil != perfilEsperado)
    return 0;
  CACHE_FS_TRAVAR();
  f = fopen(tmp, "wb");
  if (!f) { CACHE_FS_LIBERAR(); return 0; }
  if (fwrite(&c, sizeof c, 1, f) != 1 ||
      fwrite(itens, sizeof(CatItem), (size_t)n, f) != (size_t)n ||
      (nFils > 0 &&
       fwrite(fils, sizeof(CatFileira), (size_t)nFils, f) != (size_t)nFils)) {
    fclose(f); remove(tmp); CACHE_FS_LIBERAR(); return 0;
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
         caminho, n, nFils);
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
  CatFileira lidas[CAT_FIL_MAX];
  int nLidas = 0;
  caminhoCache(dirArte, caminho, sizeof caminho);
  f = fopen(caminho, "rb");
  if (!f) return 0;
  if (fread(&c, sizeof c, 1, f) != 1) { fclose(f); return 0; }
  // RECUSA em vez de ler torto. Struct diferente = arquivo de outra build.
  if (c.magia != CACHE_MAGIA ||
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
      c.ingles != ajustes_idioma_ingles()) {
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
  if (fread(novo, sizeof(CatItem), (size_t)c.nItens, f) != (size_t)c.nItens) {
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
  pthread_mutex_lock(&pubTrava);
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

void cat_aplicar_progresso(int indice, double posSeg, double durSeg, int temporada, int episodio) {
  indice = normalizarIndice(indice);
  if (indice < 0 || durSeg <= 1.0) return;
  itens[indice].progresso = (int)(100.0 * posSeg / durSeg);
  itens[indice].restanteMin = (int)((durSeg - posSeg) / 60.0 + 0.5);
  cat_apontar_episodio(indice, temporada, episodio);
  mudou();
}

// Reaplica o que esta em progresso.c sobre itens[]. Os registros vem do mais
// novo para o mais antigo, e cada titulo recebe so o primeiro que casar: numa
// serie com varios episodios gravados, e o episodio mais recente que a fileira
// e o "Retomar" querem mostrar.
static int aplicarProgressoDoDisco(void) {
  static ProgRegistro regs[PROG_MAX];
  char *tocado;
  int k, i, m = cat_n(), aplicados = 0;
  if (m < 1) return 0;
  k = prog_ler(regs, PROG_MAX);
  if (k < 1) return 0;
  tocado = calloc((size_t)m, 1);
  if (!tocado) return 0;
  for (i = 0; i < k; i++) {
    int j;
    for (j = 0; j < m; j++) {
      if (tocado[j] || !itens[j].imdb[0] || !mesmoTitulo(itens[j].imdb, regs[i].contentId)) continue;
      // O ITEM QUE JA E MAIS NOVO QUE O DISCO NAO VOLTA NO TEMPO. O item do
      // Trakt (pausado ou "a seguir", issue #66) traz o instante em
      // retomadoMs; um registro local mais velho — o S1E1 a 3% de 8/9 quando
      // o Trakt diz "viu o S1E1 inteiro em 19/9, a seguir o S1E2" — punha o
      // episodio ja visto de volta no card, com o selo do outro. Mesma regra
      // de montarContinuar: o instante decide.
      if (itens[j].retomadoMs > 0 && itens[j].retomadoMs > regs[i].lastWatchedMs) { tocado[j] = 1; continue; }
      cat_aplicar_progresso(j, regs[i].posSeg, regs[i].durSeg, regs[i].temporada, regs[i].episodio);
      tocado[j] = 1;
      aplicados++;
      break;
    }
  }
  free(tocado);
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
    if (epIni && epQtd && orfao < nFaixas) {
      memmove(&epIni[indice], &epIni[indice + 1], sizeof(int) * (size_t)quantos);
      memmove(&epQtd[indice], &epQtd[indice + 1], sizeof(int) * (size_t)quantos);
    }
  }
  if (epQtd && orfao < nFaixas) epQtd[orfao] = 0;
  // Fileira que esvaziou sai da lista, senao a home desenha um titulo com
  // nada embaixo. Mesmo protocolo: zera a contagem antes de mexer no vetor.
  if (f->n < 1) {
    int k, total = nFils;
    nFils = 0;
    for (k = r; k + 1 < total; k++) fils[k] = fils[k + 1];
    nFils = total - 1;
  }
  catRevisao++; mudou();
}

// Devolve 1 se achou e tirou.
int cat_tirar_item_da_fileira(int indice) {
  int r;
  if (indice < 0 || indice >= n) return 0;
  pthread_mutex_lock(&pubTrava);
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
  pthread_mutex_lock(&pubTrava);
  k = podarContinuar(mesmaObraQue, imdb);
  pthread_mutex_unlock(&pubTrava);
  printf("[cat] %s tirado de Continuar assistindo: %d card(s)\n", imdb, k);
  fflush(stdout);
  return k;
}

void cat_zerar_progresso(int indice) {
  if (indice < 0 || indice >= n) return;
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
  if (!prog_gravar_local(itens[indice].imdb, temporada, episodio, posSeg, durSeg)) return;
  cat_aplicar_progresso(indice, posSeg, durSeg, temporada, episodio);
}

unsigned cat_revisao(void) { return catRevisao; }
unsigned cat_revisao_itens(void) { return __atomic_load_n(&catMudancas, __ATOMIC_ACQUIRE); }

int cat_n_episodios(int indiceItem) {
  int m = cat_n();
  // epQtd so nasce em garantirFaixas, que em cat_carregar vem DEPOIS de
  // aplicar o progresso do disco — e aplicar progresso de serie pergunta
  // pelos episodios. Sem esta guarda o arranque caia com progresso gravado.
  if (m < 1 || !epQtd) return 0;
  indiceItem = ((indiceItem % m) + m) % m;
  return epQtd[indiceItem];
}

const CatEp *cat_episodio(int indiceItem, int i) {
  int m = cat_n();
  if (m < 1 || !epQtd || !epIni) return NULL;
  indiceItem = ((indiceItem % m) + m) % m;
  if (i < 0 || i >= epQtd[indiceItem]) return NULL;
  return &eps[epIni[indiceItem] + i];
}

int cat_n_fileiras(void) { return nFils; }
const CatFileira *cat_fileira(int r) {
  return (r >= 0 && r < nFils) ? &fils[r] : NULL;
}

int cat_copiar_fileira(const char *chave, CatItem *saida, int max,
                       CatFileira *meta) {
  int r, qtd;
  if (!chave || !*chave || !saida || max < 1) return 0;
  pthread_mutex_lock(&pubTrava);
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
void cat_definir_na_lista(int i, int naLista) {
  if (!itens || n <= 0 || i < 0 || i >= n) return;
  // SO SOBE A REVISAO SE MUDOU DE FATO: os reconciliadores (salvos.c,
  // contalib.c) remarcam o que ja estava marcado, e uma revisao que sobe sem
  // mudanca faria o painel de Salvos reconstruir a toa.
  if (itens[i].naLista == (naLista ? 1 : 0)) return;
  itens[i].naLista = naLista ? 1 : 0;
  mudou();
}

// O MESMO TITULO VIVE EM VARIAS FILEIRAS, cada uma com a sua copia do CatItem
// (a watchlist do Trakt, "Trending", uma colecao). Marcar so a copia do cartao
// segurado deixava as outras dizendo o contrario: salvar pelo Trending nao
// acendia o da watchlist, e remover pelo Trending deixava a copia da watchlist
// marcada — o menu seguinte voltava a oferecer "Remover" para algo ja removido.
int cat_definir_na_lista_imdb(const char *imdb, int naLista) {
  int i, k = 0;
  if (!itens || n <= 0 || !imdb || !imdb[0]) return 0;
  for (i = 0; i < n; i++)
    if (!strcmp(itens[i].imdb, imdb)) {
      if (itens[i].naLista != (naLista ? 1 : 0)) { itens[i].naLista = naLista ? 1 : 0; mudou(); }
      k++;
    }
  return k;
}
int cat_imdb_na_lista(const char *imdb) {
  int i;
  if (!itens || n <= 0 || !imdb || !imdb[0]) return 0;
  for (i = 0; i < n; i++) if (itens[i].naLista && !strcmp(itens[i].imdb, imdb)) return 1;
  return 0;
}

// Atualiza um espelho de item somente quando o indice ainda pertence ao bloco
// atualmente publicado. A modal pode receber a resposta do worker depois que
// a descoberta trocou o catalogo; nesse caso ignorar e seguro, escrever por um
// indice antigo poderia alterar outro titulo.
void cat_atualizar_item(int i, const CatItem *item) {
  if (!item || !itens || n <= 0 || i < 0 || i >= n) return;
  itens[i] = *item;
  mudou();
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
  if (!v || qtd < 1 || n < 1) return 0;
  if (n + qtd > CAT_MAX) qtd = CAT_MAX - n;
  if (qtd < 1) return 0;
  novoN = n + qtd;
  novo = malloc(sizeof(CatItem) * (size_t)novoN);
  if (!novo) return 0;
  pthread_mutex_lock(&pubTrava);
  memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  memcpy(&novo[n], v, sizeof(CatItem) * (size_t)qtd);
  { int k; for (k = 0; k < qtd; k++) {
      if (v[k].poster[0])   arte_reserva_registrar(v[k].poster,   v[k].imdb, 1);
      if (v[k].backdrop[0]) arte_reserva_registrar(v[k].backdrop, v[k].imdb, 0); } }
  if (saidaIdx) for (k = 0; k < qtd; k++) saidaIdx[k] = n + k;
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = novoN;
  __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
  garantirFaixas(nAlocado);
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
  pthread_mutex_lock(&pubTrava);
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
  __atomic_store_n(&n, m, __ATOMIC_RELEASE);
  garantirFaixas(nAlocado);
  mudou();
  pthread_mutex_unlock(&pubTrava);
  printf("[cat] listas do Trakt na tela: %d marcado(s), %d novo(s)\n", marcados, novos);
  fflush(stdout);
  return novos;
}

int cat_acrescentar(const CatItem *item) {
  CatItem *novo;
  int novoN;
  if (!item || n < 1) return -1;
  if (n >= CAT_MAX) return -1;
  novoN = n + 1;
  novo = malloc(sizeof(CatItem) * (size_t)novoN);
  if (!novo) return -1;
  pthread_mutex_lock(&pubTrava);
  memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  memcpy(&novo[n], item, sizeof(CatItem));
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = novoN;
  __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
  garantirFaixas(nAlocado);
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
  if (!novasFils || nNovas < 1 || n < 1) return;
  q = nNovas > CAT_FIL_MAX ? CAT_FIL_MAX : nNovas;
  pthread_mutex_lock(&pubTrava);
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
          novasFils[k].estado != fils[k].estado) igual = 0;
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
  catRevisao++; mudou();
  pthread_mutex_unlock(&pubTrava);
}

void cat_definir_tudo(const CatItem *lista, int qtd,
                      const CatFileira *novasFils, int nNovas) {
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
    // As fileiras caem JUNTO com `n`. Elas sao janelas (ini,n) no vetor de
    // itens; deixar as antigas de pe por um quadro enquanto o vetor troca faz o
    // desenho ler fora da faixa.
    pthread_mutex_lock(&pubTrava);
    __atomic_store_n(&n, 0, __ATOMIC_RELEASE);
    nFils = 0;
    aposentar(itens);
    __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
    nAlocado = novoN;
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
    // cat_trocar_continuar. As faixas de episodio que ela desloca sao zeradas
    // logo abaixo de qualquer jeito.
    podarContinuar(removidoVence, NULL);
    pthread_mutex_unlock(&pubTrava);
  }
  // Episodios do catalogo anterior nao valem para o novo: os indices mudaram.
  nEps = 0;
  zerarFaixas(nAlocado);
  catRevisao++; mudou();
  (void)0;
  // O progresso e por imdb e vive em progresso.c, entao sobrevive a troca —
  // mas precisa ser reaplicado, porque os itens novos nasceram zerados. E aqui
  // que uma linha da conta que antes nao casava com nada passa a casar, quando
  // o titulo dela entra no catalogo.
  aplicarProgressoDoDisco();
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
  CatFileira novas[CAT_FIL_MAX];
  CatItem *novo;
  int r, cw = -1, cwIni = 0, cwN = 0, delta, novoN, nv = 0;
  if (qtd < 0) qtd = 0;
  pthread_mutex_lock(&pubTrava);
  for (r = 0; r < nFils; r++)
    if (!strcmp(fils[r].chave, "continue_watching")) {
      cw = r; cwIni = fils[r].ini; cwN = fils[r].n; break;
    }
  delta = qtd - cwN;
  novoN = n + delta;
  if (novoN > CAT_MAX) { qtd -= novoN - CAT_MAX; delta = qtd - cwN; novoN = CAT_MAX; }
  if (novoN < 0) { pthread_mutex_unlock(&pubTrava); return; }
  novo = malloc(sizeof(CatItem) * (size_t)(novoN > 0 ? novoN : 1));
  if (!novo) { pthread_mutex_unlock(&pubTrava); return; }
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
  __atomic_store_n(&n, 0, __ATOMIC_RELEASE);
  nFils = 0;
  aposentar(itens);
  __atomic_store_n(&itens, novo, __ATOMIC_RELEASE);
  nAlocado = novoN;
  __atomic_store_n(&n, novoN, __ATOMIC_RELEASE);
  memcpy(fils, novas, sizeof *novas * (size_t)nv);
  nFils = nv;
  // A REFACAO NAO TRAZ DE VOLTA O QUE FOI TIRADO. montarContinuar ja filtra,
  // mas um fio que montou ANTES da remocao e publica DEPOIS dela passaria o
  // item velho; sob a mesma trava de cat_tirar_continuar, ou a remocao ja
  // marcou (e a poda pega aqui) ou ela vem depois (e tira por imdb).
  { int podados = podarContinuar(removidoVence, NULL);
    qtd -= podados; }
  pthread_mutex_unlock(&pubTrava);
  // Os indices andaram: nenhuma faixa de episodio vale para o item novo.
  nEps = 0;
  zerarFaixas(nAlocado);
  catRevisao++; mudou();
  aplicarProgressoDoDisco();
  printf("[cat] continuar assistindo refeita: %d item(ns)\n", qtd);
  fflush(stdout);
}

void cat_definir_episodios(int indiceItem, const CatEp *lista, int qtd) {
  int m = cat_n();
  if (!lista || qtd < 1 || m < 1) return;
  indiceItem = ((indiceItem % m) + m) % m;
  if (qtd > CAT_EP_MAX) qtd = CAT_EP_MAX;
  // Anexa no fim do vetor comum. Trocar de temporada varias vezes acumula, mas
  // o teto de CAT_EP_MAX segura e o custo de compactar nao se paga.
  if (nEps + qtd > CAT_EP_MAX) {
    nEps = 0;
    // Invalidar os indices antes de reutilizar o armazenamento: senao outra
    // serie passa a exibir os episodios da obra que acabou de ser carregada.
    memset(epQtd,0,(size_t)nAlocado*sizeof *epQtd);
    memset(epIni,0,(size_t)nAlocado*sizeof *epIni);
  }
  memcpy(&eps[nEps], lista, sizeof(CatEp) * (size_t)qtd);
  epIni[indiceItem] = nEps;
  epQtd[indiceItem] = qtd;
  nEps += qtd;
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
