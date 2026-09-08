#include "catalogo.h"
#include "descoberta.h"
#include "progresso.h"
#include <stdio.h>

static int mesmoTitulo(const char *a, const char *b);
static int aplicarProgressoDoDisco(void);
#include <string.h>
#include <stdlib.h>

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
#define CAT_EP_MAX 600
static CatEp eps[CAT_EP_MAX];
// Faixas de episodio por titulo, do mesmo tamanho do vetor de itens — que
// agora cresce, entao estes tambem.
static int  *epIni, *epQtd, nEps;

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

static void garantirFaixas(int quantos) {
  int *a, *b;
  if (quantos < 1) return;
  a = realloc(epIni, sizeof(int) * (size_t)quantos);
  b = realloc(epQtd, sizeof(int) * (size_t)quantos);
  if (a) epIni = a;
  if (b) epQtd = b;
  if (epIni) memset(epIni, 0, sizeof(int) * (size_t)quantos);
  if (epQtd) memset(epQtd, 0, sizeof(int) * (size_t)quantos);
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
        pt = desc_genero_pt(parte);
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
      while (*p2 && itens[i].nElenco < 6) {
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
    garantirFaixas(nAlocado);
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
#define CACHE_MAGIA  0x4E56434Bu   /* "NVCK" */
#define CACHE_VERSAO 1

typedef struct {
  unsigned magia, versao, tamItem, tamFileira;
  int nItens, nFileiras;
} CacheCab;

static void caminhoCache(const char *dirArte, char *dst, size_t tam) {
  snprintf(dst, tam, "%s/catalogo-rede.bin", dirArte ? dirArte : ".");
}

// Chamado pela descoberta quando o catalogo COMPLETO da rede substitui o do
// cache. A partir daqui a tela ja e a desta sessao.
void cat_cache_substituido(void) { veioDoCache = 0; }

int cat_gravar_cache(const char *dirArte) {
  char caminho[600], tmp[620];
  CacheCab c;
  FILE *f;
  if (n < 1) return 0;
  caminhoCache(dirArte, caminho, sizeof caminho);
  // Grava num temporario e renomeia: quem le na proxima abertura nunca pega
  // arquivo pela metade se o app for fechado no meio da escrita.
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  f = fopen(tmp, "wb");
  if (!f) return 0;
  c.magia = CACHE_MAGIA; c.versao = CACHE_VERSAO;
  c.tamItem = (unsigned)sizeof(CatItem);
  c.tamFileira = (unsigned)sizeof(CatFileira);
  c.nItens = n; c.nFileiras = nFils;
  if (fwrite(&c, sizeof c, 1, f) != 1 ||
      fwrite(itens, sizeof(CatItem), (size_t)n, f) != (size_t)n ||
      (nFils > 0 &&
       fwrite(fils, sizeof(CatFileira), (size_t)nFils, f) != (size_t)nFils)) {
    fclose(f); remove(tmp); return 0;
  }
  fclose(f);
  if (rename(tmp, caminho) != 0) { remove(tmp); return 0; }
  printf("[cat] cache gravado: %d titulos, %d fileiras\n", n, nFils);
  fflush(stdout);
  return 1;
}

int cat_ler_cache(const char *dirArte) {
  char caminho[600];
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
  if (c.magia != CACHE_MAGIA || c.versao != CACHE_VERSAO ||
      c.tamItem != sizeof(CatItem) || c.tamFileira != sizeof(CatFileira) ||
      c.nItens < 1 || c.nItens > CAT_MAX ||
      c.nFileiras < 0 || c.nFileiras > CAT_FIL_MAX) {
    fclose(f);
    printf("[cat] cache descartado (formato de outra build)\n");
    remove(caminho);
    return 0;
  }
  novo = malloc(sizeof(CatItem) * (size_t)c.nItens);
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

int cat_n(void) { return n; }

const CatItem *cat_item(int i) {
  if (!itens || n <= 0) return NULL;
  return &itens[((i % n) + n) % n];
}

// Compara so ate o primeiro ':' — o catalogo guarda "tt123:2:1" em serie com
// progresso, e quem procura tem so o id do titulo.
static int mesmoTitulo(const char *a, const char *b) {
  while (*a && *b && *a != ':' && *b != ':') { if (*a != *b) return 0; a++; b++; }
  return (!*a || *a == ':') && (!*b || *b == ':');
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
      cat_aplicar_progresso(j, regs[i].posSeg, regs[i].durSeg, regs[i].temporada, regs[i].episodio);
      tocado[j] = 1;
      aplicados++;
      break;
    }
  }
  free(tocado);
  return aplicados;
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
  itens[i].naLista = naLista ? 1 : 0;
}

// Atualiza um espelho de item somente quando o indice ainda pertence ao bloco
// atualmente publicado. A modal pode receber a resposta do worker depois que
// a descoberta trocou o catalogo; nesse caso ignorar e seguro, escrever por um
// indice antigo poderia alterar outro titulo.
void cat_atualizar_item(int i, const CatItem *item) {
  if (!item || !itens || n <= 0 || i < 0 || i >= n) return;
  itens[i] = *item;
}

// Acrescenta N de UMA VEZ. cat_acrescentar copia o catalogo inteiro a cada
// chamada, e a busca a chamava POR RESULTADO: com 300 titulos no acervo sao
// ~2,3 MB por copia, vezes 40 resultados, no fio de DESENHO, a cada tecla. Era
// o travamento que aparecia como "a busca engasga quando digito".
//
// Uma troca de bloco so, seguindo a mesma ordem de cat_definir: zera `n` antes
// de trocar o ponteiro (o desenho ve catalogo vazio por um quadro em vez de ler
// memoria liberada) e nao libera o bloco velho aqui — um leitor pode estar
// dentro dele; ele morre na proxima troca.
int cat_acrescentar_lote(const CatItem *v, int qtd, int *saidaIdx) {
  static CatItem *lixoLote;
  CatItem *novo;
  int novoN, k;
  if (!v || qtd < 1 || n < 1) return 0;
  if (n + qtd > CAT_MAX) qtd = CAT_MAX - n;
  if (qtd < 1) return 0;
  novoN = n + qtd;
  novo = malloc(sizeof(CatItem) * (size_t)novoN);
  if (!novo) return 0;
  memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  memcpy(&novo[n], v, sizeof(CatItem) * (size_t)qtd);
  if (saidaIdx) for (k = 0; k < qtd; k++) saidaIdx[k] = n + k;
  free(lixoLote);
  lixoLote = itens;
  itens = novo;
  nAlocado = novoN;
  n = novoN;
  garantirFaixas(nAlocado);
  return qtd;
}

int cat_acrescentar(const CatItem *item) {
  static CatItem *lixoAcr;
  CatItem *novo;
  int novoN;
  if (!item || n < 1) return -1;
  if (n >= CAT_MAX) return -1;
  novoN = n + 1;
  novo = malloc(sizeof(CatItem) * (size_t)novoN);
  if (!novo) return -1;
  memcpy(novo, itens, sizeof(CatItem) * (size_t)n);
  memcpy(&novo[n], item, sizeof(CatItem));
  free(lixoAcr);
  lixoAcr = itens;
  itens = novo;
  nAlocado = novoN;
  n = novoN;
  garantirFaixas(nAlocado);
  return novoN - 1;
}

void cat_definir(const CatItem *lista, int qtd) {
  cat_definir_tudo(lista, qtd, NULL, 0);
}

void cat_republicar_fileiras(const CatFileira *novasFils, int nNovas) {
  int k, q, v = 0;
  if (!novasFils || nNovas < 1 || n < 1) return;
  q = nNovas > CAT_FIL_MAX ? CAT_FIL_MAX : nNovas;
  nFils = 0;                 // ver a nota em catalogo.h: zera antes de mexer
  for (k = 0; k < q; k++) {
    CatFileira f = novasFils[k];
    if (f.ini < 0 || f.ini >= n) continue;
    if (f.ini + f.n > n) f.n = n - f.ini;
    if (f.n < 1) continue;
    fils[v++] = f;
  }
  nFils = v;
}

void cat_definir_tudo(const CatItem *lista, int qtd,
                      const CatFileira *novasFils, int nNovas) {
  if (!lista || qtd < 1) return;
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
  // instante. Ele morre na proxima troca, quando ninguem mais o alcanca.
  {
    int novoN = qtd > CAT_MAX ? CAT_MAX : qtd;
    CatItem *novo = malloc(sizeof(CatItem) * (size_t)novoN);
    static CatItem *lixo;
    if (!novo) return;
    memcpy(novo, lista, sizeof(CatItem) * (size_t)novoN);
    // As fileiras caem JUNTO com `n`. Elas sao janelas (ini,n) no vetor de
    // itens; deixar as antigas de pe por um quadro enquanto o vetor troca faz o
    // desenho ler fora da faixa.
    n = 0;
    nFils = 0;
    free(lixo);
    lixo = itens;
    itens = novo;
    nAlocado = novoN;
    n = novoN;
    if (novasFils && nNovas > 0) {
      int k, q = nNovas > CAT_FIL_MAX ? CAT_FIL_MAX : nNovas;
      int v = 0;
      for (k = 0; k < q; k++) {
        CatFileira f = novasFils[k];
        // Corta a janela pelo que sobrou de verdade. Um catalogo que respondeu
        // menos itens do que o esperado deixaria a fileira apontando para o
        // vizinho.
        if (f.ini < 0 || f.ini >= n) continue;
        if (f.ini + f.n > n) f.n = n - f.ini;
        if (f.n < 1) continue;
        fils[v++] = f;
      }
      nFils = v;
    }
  }
  // Episodios do catalogo anterior nao valem para o novo: os indices mudaram.
  nEps = 0;
  garantirFaixas(nAlocado);
  (void)0;
  // O progresso e por imdb e vive em progresso.c, entao sobrevive a troca —
  // mas precisa ser reaplicado, porque os itens novos nasceram zerados. E aqui
  // que uma linha da conta que antes nao casava com nada passa a casar, quando
  // o titulo dela entra no catalogo.
  aplicarProgressoDoDisco();
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
