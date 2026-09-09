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

static SalvoItem itens[SALVOS_MAX];
static int nItens;
static int carregado;

// Marca de reconciliacao, igual a de contalib.c: quantos itens o catalogo tinha
// e QUAL era o ultimo item que tocamos. Contagem igual nao prova catalogo
// igual — a descoberta republica o mesmo numero de titulos com outro conteudo.
static int  marcaCatN = -1;
static int  marcaIdx  = -1;
static char marcaId[24];

static int acharLocal(const char *imdb) {
  int i;
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < nItens; i++)
    if (!strcmp(itens[i].id, imdb)) return i;
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
  // 300 itens x ~800 bytes de campos + folga. No heap e nao na pilha: a pilha
  // do fio principal no webOS nao tem 256 KB de sobra para um buffer temporario.
  size_t cap = (size_t)SALVOS_MAX * 900u + 64u;
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
    { SalvoItem *s = &itens[nItens++];
      memset(s, 0, sizeof *s);
      snprintf(s->id, sizeof s->id, "%s", id);
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
    if (nItens >= SALVOS_MAX) {
      printf("[salvos] lista cheia (%d); \"%s\" nao entrou\n",
             SALVOS_MAX, ci->titulo);
      fflush(stdout);
      return 0;
    }
    { SalvoItem *s = &itens[nItens++];
      memset(s, 0, sizeof *s);
      snprintf(s->id, sizeof s->id, "%s", ci->imdb);
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

void salvos_esquecer(void) {
  nItens = 0;
  marcaCatN = marcaIdx = -1;
  marcaId[0] = 0;
  dados_apagar(SALVOS_ARQ);
}
