// Lista local de "Salvos". Ver a nota longa em salvos.h — em especial por que
// ela existe (sem Trakt, o "+" nao guardava nada) e por que ela nao e uma
// quarta superficie de biblioteca.
#include "salvos.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SALVOS_ARQ "salvos.txt"

static SalvoItem *itens;
static int nItens, capItens;

// Garante vaga para `n` itens. Cresce em blocos, dobrando, ate SALVOS_MAX.
static int garantir(int n) {
  SalvoItem *novo;
  int cap;
  if (n <= capItens) return 1;
  if (n > SALVOS_MAX) return 0;
  cap = capItens ? capItens * 2 : 64;
  while (cap < n) cap *= 2;
  if (cap > SALVOS_MAX) cap = SALVOS_MAX;
  novo = (SalvoItem *)realloc(itens, sizeof *itens * (size_t)cap);
  if (!novo) return 0;
  itens = novo;
  capItens = cap;
  return 1;
}
static int carregado;

// Marca de reconciliacao, igual a de contalib.c: quantos itens o catalogo tinha
// e QUAL era o ultimo item que tocamos. Contagem igual nao prova catalogo
// igual — a descoberta republica o mesmo numero de titulos com outro conteudo.
static int  marcaCatN = -1;
static int  marcaIdx  = -1;
static char marcaId[24];

// ":<digitos>:<digitos>" e so isso. Copia de ehSufixoEp em catalogo.c: linkar
// catalogo.c aqui arrastaria descoberta e rede para dentro de tests/salvos.sh.
static int sufixoEp(const char *r) {
  int k = 0;
  if (*r != ':') return 0;
  r++;
  while (*r >= '0' && *r <= '9') { r++; k++; }
  if (!k || *r != ':') return 0;
  r++; k = 0;
  while (*r >= '0' && *r <= '9') { r++; k++; }
  return k && !*r;
}

int salvos_mesmo_titulo(const char *a, const char *b) {
  char ka[64], kb[64];
  if (!a || !b || !a[0] || !b[0]) return 0;
  if (!strcmp(a, b)) return 1;
  salvos_id_titulo(a, ka, sizeof ka);
  salvos_id_titulo(b, kb, sizeof kb);
  return !strcmp(ka, kb);
}

void salvos_id_titulo(const char *id, char *out, size_t tam) {
  const char *c;
  size_t k;
  if (!out || tam < 1) return;
  out[0] = 0;
  if (!id) return;
  k = strlen(id);
  // O sufixo sao os DOIS ultimos ':' — procura o penultimo de tras para frente.
  c = strrchr(id, ':');
  if (c && c > id) {
    const char *d = c - 1;
    while (d > id && *d != ':') d--;
    // O que sobra tem de ser um id de titulo: "tt..." ou "<fonte>:<id>".
    // "kitsu:12:1" e episodio 1 do anime 12, e "kitsu" nao e titulo nenhum.
    if (*d == ':' && d > id && sufixoEp(d) &&
        (!strncmp(id, "tt", 2) || memchr(id, ':', (size_t)(d - id))))
      k = (size_t)(d - id);
  }
  if (k >= tam) k = tam - 1;
  memcpy(out, id, k);
  out[k] = 0;
}

static int acharLocal(const char *imdb) {
  int i;
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < nItens; i++)
    if (salvos_mesmo_titulo(itens[i].id, imdb)) return i;
  return -1;
}

int salvos_n(void) { return nItens; }
const SalvoItem *salvos_item(int i) {
  return (i >= 0 && i < nItens) ? &itens[i] : NULL;
}
int salvos_tem(const char *imdb) { return acharLocal(imdb) >= 0; }

// --- ARQUIVO ----------------------------------------------------------------
//
// Uma linha por titulo, campos separados por TAB. Nao e JSON de proposito: o
// app ja carrega um leitor (js.c) mas ele e para RESPOSTA DE REDE, e escrever
// um gerador so para este arquivo custaria mais que o formato inteiro. TAB e
// nao ";" ou "|" porque titulo de filme tem os dois e nao tem tabulacao.
//
// Campo que faltar na leitura vira vazio, e a linha continua valida: uma versao
// futura que acrescente coluna nao pode invalidar o arquivo de quem ja usa o
// app — foi assim que ajustes.c se protegeu (ver a nota de CHAVE la).

static void gravar(void) {
  // ~800 bytes de campos por item + folga, do tamanho da lista DE AGORA. No
  // heap e nao na pilha: a pilha do fio principal no webOS nao tem centenas de
  // KB de sobra para um buffer temporario.
  size_t cap = (size_t)nItens * 900u + 64u;
  char *buf = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio salvos v1\n");
  for (i = 0; i < nItens && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%s\t%lld\t%d\t%s\t%s\t%s\n",
                          itens[i].id, itens[i].tipo, itens[i].quandoS,
                          itens[i].nota, itens[i].meta, itens[i].poster,
                          itens[i].titulo);
  dados_gravar(SALVOS_ARQ, buf);
  free(buf);
}

// Recorta o proximo campo ate o TAB, avanca `p` e devolve o inicio. Escreve NUL
// no separador: o buffer e nosso e morre no fim de salvos_iniciar.
static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

void salvos_iniciar(void) {
  char *b, *linha, *prox;
  if (carregado) return;
  carregado = 1;
  nItens = 0;
  b = dados_ler(SALVOS_ARQ);
  if (!b) return;
  for (linha = b; linha && *linha && nItens < SALVOS_MAX; linha = prox) {
    char *p, *id, *tipo, *quando, *nota, *meta, *poster, *titulo;
    char *fim = strchr(linha, '\n');
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    id     = campo(&p);
    tipo   = campo(&p);
    quando = campo(&p);
    nota   = campo(&p);
    meta   = campo(&p);
    poster = campo(&p);
    // O TITULO E O ULTIMO CAMPO de proposito: ele e o unico que pode conter
    // qualquer coisa, e sendo o ultimo nao precisa de separador depois dele.
    titulo = p ? p : (char *)"";
    if (strncmp(id, "tt", 2)) continue;   // linha sem IMDb nao serve para nada
    // ARQUIVO GRAVADO POR VERSAO ANTERIOR pode ter o mesmo titulo duas vezes:
    // salvar pelo card de "Continuar assistindo" guardava "tt123:1:2", e o
    // mesmo titulo salvo pelo detalhe entrava de novo como "tt123". A primeira
    // (a mais antiga) fica; a outra nao entra, e o proximo gravar() limpa.
    if (acharLocal(id) >= 0) continue;
    if (!garantir(nItens + 1)) break;
    { SalvoItem *s = &itens[nItens++];
      memset(s, 0, sizeof *s);
      salvos_id_titulo(id, s->id, sizeof s->id);
      snprintf(s->tipo, sizeof s->tipo, "%s", tipo[0] ? tipo : "movie");
      snprintf(s->meta, sizeof s->meta, "%s", meta);
      snprintf(s->poster, sizeof s->poster, "%s", poster);
      snprintf(s->titulo, sizeof s->titulo, "%s", titulo);
      s->quandoS = atoll(quando);
      s->nota = atoi(nota); }
  }
  free(b);
  printf("[salvos] %d titulos na lista local\n", nItens);
  fflush(stdout);
}

int salvos_definir(const CatItem *ci, int salvo) {
  int k;
  if (!ci || !ci->imdb[0]) return 0;
  // Carregar sob demanda: se alguem salvar antes de salvos_iniciar (nao deveria,
  // mas o roteador tem muitos caminhos), gravar por cima de uma lista nao lida
  // APAGARIA o arquivo inteiro. Ler primeiro custa uma vez.
  if (!carregado) salvos_iniciar();
  k = acharLocal(ci->imdb);
  if (salvo) {
    if (k >= 0) return 0;
    // CHEIA: sai o MAIS ANTIGO (o primeiro, a lista e na ordem de insercao) e
    // o novo entra. Recusar o novo era o defeito — o "+" acendia e o titulo
    // nao ia para lugar nenhum.
    if (nItens >= SALVOS_MAX || !garantir(nItens + 1)) {
      if (nItens < 1) return 0;
      printf("[salvos] lista cheia (%d); saiu o mais antigo \"%s\" para "
             "\"%s\" entrar\n", nItens, itens[0].titulo, ci->titulo);
      fflush(stdout);
      memmove(&itens[0], &itens[1], sizeof(SalvoItem) * (size_t)(nItens - 1));
      nItens--;
    }
    { SalvoItem *s = &itens[nItens++];
      memset(s, 0, sizeof *s);
      // O ID DO TITULO, nunca o do episodio: o "+" do card de "Continuar
      // assistindo" chega com "tt123:1:2", e guardado assim ele nao batia com
      // o "tt123" da conta nem da watchlist.
      salvos_id_titulo(ci->imdb, s->id, sizeof s->id);
      snprintf(s->tipo, sizeof s->tipo, "%s", ci->tipo[0] ? ci->tipo : "movie");
      snprintf(s->titulo, sizeof s->titulo, "%s", ci->titulo);
      snprintf(s->poster, sizeof s->poster, "%s", ci->poster);
      snprintf(s->meta, sizeof s->meta, "%s", ci->meta);
      s->nota = ci->nota;
      s->quandoS = (long long)time(NULL); }
  } else {
    if (k < 0) return 0;
    // Ordem preservada na remocao: a lista e desenhada na ordem de insercao e
    // trocar o removido pelo ultimo reordenaria o painel debaixo do dedo de
    // quem acabou de apertar OK.
    memmove(&itens[k], &itens[k + 1],
            sizeof(SalvoItem) * (size_t)(nItens - k - 1));
    nItens--;
  }
  gravar();
  return 1;
}

int salvos_aplicar_catalogo(void) {
  int i, marcados = 0, ultimo = -1;
  if (nItens < 1) return 0;
  // Catalogo vazio nao tem onde marcar. Sair sem tocar na marca faz a
  // reconciliacao tentar de novo assim que o catalogo existir.
  if (cat_n() < 1) return 0;
  for (i = 0; i < nItens; i++) {
    int k = cat_indice_por_imdb(itens[i].id);
    if (k < 0) continue;
    cat_definir_na_lista(k, 1);
    marcados++;
    ultimo = k;
  }
  // NAO ACRESCENTA AO CATALOGO, e a diferenca para contalib_aplicar_catalogo e
  // deliberada. La os itens da conta PRECISAM entrar no catalogo, senao a tela
  // de Biblioteca nao teria o que mostrar. Aqui o painel de Salvos desenha da
  // PROPRIA lista (titulo e poster estao guardados no arquivo), entao inflar o
  // catalogo com 300 titulos so para marca-los custaria memoria nesta TV sem
  // dar nada em troca. Quem estiver no catalogo fica marcado; quem nao estiver
  // aparece no painel do mesmo jeito.
  marcaCatN = cat_n();
  marcaIdx = ultimo;
  marcaId[0] = 0;
  if (ultimo >= 0 && ultimo < cat_n()) {
    const CatItem *c = cat_item(ultimo);
    if (c) snprintf(marcaId, sizeof marcaId, "%s", c->imdb);
  }
  return marcados;
}

void salvos_reconciliar(void) {
  const CatItem *c;
  if (nItens < 1 || cat_n() < 1) return;
  if (cat_n() != marcaCatN) { salvos_aplicar_catalogo(); return; }
  if (marcaIdx < 0 || marcaIdx >= cat_n()) return;
  c = cat_item(marcaIdx);
  if (c && !strcmp(c->imdb, marcaId) && c->naLista) return;
  salvos_aplicar_catalogo();
}

// A copia do catalogo que representa o titulo: a primeira COM progresso, senao
// a primeira. cat_indice_por_imdb devolve so a primeira, e ela pode ser a copia
// da watchlist (sem progresso) enquanto a de "Continuar assistindo" tem o
// episodio — a linha perderia a barra so por causa da ordem das fileiras.
static int melhorCopia(const char *id) {
  int i, n = cat_n(), primeira = -1;
  for (i = 0; i < n; i++) {
    const CatItem *c = cat_item(i);
    if (!c || !c->imdb[0] || !salvos_mesmo_titulo(c->imdb, id)) continue;
    if (c->progresso > 0) return i;
    if (primeira < 0) primeira = i;
  }
  return primeira;
}

// Posicao em `out` da entrada SO DO CATALOGO com o mesmo titulo; -1 sem ela.
// As entradas locais ja foram cobertas por acharLocal.
static int uniaoAchar(const SalvosEntrada *out, int k, const char *id) {
  int i;
  for (i = 0; i < k; i++) {
    const CatItem *c;
    if (out[i].local >= 0) continue;
    c = cat_item(out[i].cat);
    if (c && salvos_mesmo_titulo(c->imdb, id)) return i;
  }
  return -1;
}

int salvos_uniao(SalvosEntrada *out, int cap) {
  int i, k = 0, n;
  if (!out || cap < 1) return 0;
  for (i = 0; i < nItens && k < cap; i++) {
    out[k].local = i;
    out[k].cat = melhorCopia(itens[i].id);
    k++;
  }
  n = cat_n();
  for (i = 0; i < n && k < cap; i++) {
    const CatItem *c = cat_item(i);
    int j;
    if (!c || !c->naLista || !c->imdb[0]) continue;
    if (acharLocal(c->imdb) >= 0) continue;
    if ((j = uniaoAchar(out, k, c->imdb)) >= 0) {
      // Segunda copia do mesmo titulo: nao vira linha. Se so ELA tem o
      // progresso, passa a ser a copia da linha que ja existe.
      const CatItem *v = cat_item(out[j].cat);
      if (c->progresso > 0 && (!v || v->progresso <= 0)) out[j].cat = i;
      continue;
    }
    out[k].local = -1;
    out[k].cat = i;
    k++;
  }
  return k;
}

void salvos_esquecer(void) {
  free(itens);
  itens = NULL;
  capItens = 0;
  nItens = 0;
  marcaCatN = marcaIdx = -1;
  marcaId[0] = 0;
  dados_apagar(SALVOS_ARQ);
  // O arquivo foi apagado: ler de novo da vazio, e deixa salvos_iniciar valer
  // para o proximo usuario em vez de ficar preso no "ja carreguei".
  carregado = 0;
}
