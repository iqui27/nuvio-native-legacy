#include "torboxbib_ui.h"
#include "torboxbib.h"
#include "debrid.h"
#include "partes.h"
#include "js.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TBUI_MAX 200

static struct {
  int itemId;
  int arquivoId;         // o video principal do item (maior "files".id)
  int usenet;
  char nome[160];
  char hash[48];
} itens[TBUI_MAX];
static int nItens, pagina;
static char status[96];
// A lista chega num fio proprio: debrid_tb_mylist pode custar 20 s e a aba
// atualiza a cada quadro (biblioteca.c chama atualizar() sem esperar). O fio
// publica o corpo; parseData roda no fio da UI, uma vez por resposta.
static pthread_t fio;
static int fioVivo;
static _Atomic int respostaPronta;   // 0 nada, 1 ha corpo para recolher
static char *respostaJson;
static int respostaSt;
static void *fioCarregar(void *u) {
  int st = 0, pag = *(int *)u;
  char *json;
  free(u);
  json = torboxbib_carregar_json(pag, &st);
  respostaJson = json; respostaSt = st;
  atomic_store_explicit(&respostaPronta, 1, memory_order_release);
  return NULL;
}
static void disparar(void) {
  int *pag;
  if (fioVivo) return;
  pag = malloc(sizeof *pag);
  if (!pag) return;
  *pag = pagina;
  atomic_store_explicit(&respostaPronta, 0, memory_order_release);
  respostaJson = NULL;
  if (pthread_create(&fio, NULL, fioCarregar, pag) != 0) { free(pag); return; }
  fioVivo = 1;
}

static void limpar(void) {
  nItens = 0;
  status[0] = 0;
}

static int tbVideo(const char *nome) {
  static const char *ext[] = { ".mkv", ".mp4", ".m4v", ".avi", ".mov", ".ts", ".m2ts", ".webm", ".wmv", NULL };
  size_t L = strlen(nome);
  for (int i = 0; ext[i]; i++) {
    size_t e = strlen(ext[i]);
    if (L > e && !strcasecmp(nome + L - e, ext[i])) return 1;
  }
  return 0;
}

// O arquivo que o item vai tocar: o MAIOR video de "files" (mesmo criterio do
// item de um video so — o partes.c exige o id real; 0 nao existe como id).
static int arquivoPrincipal(const char *item, const char *fim) {
  const char *fa, *el;
  int id = 0;
  double maior = -1;
  fa = strstr(item, "\"files\"");
  if (!fa || fa >= fim) return 0;
  fa = strchr(fa, '[');
  if (!fa || fa >= fim) return 0;
  el = fa + 1;
  while (el < fim && (*el == ' ' || *el == '\t' || *el == '\n' || *el == '\r')) el++;
  for (; el && el < fim && *el == '{'; el = js_prox(js_fim(el))) {
    const char *ef = js_fim(el);
    char nome[512];
    double tam;
    if (!ef || ef > fim) break;
    nome[0] = 0;
    if (!js_texto_raiz_em(el, ef, "short_name", nome, sizeof nome) || !nome[0])
      js_texto_raiz_em(el, ef, "name", nome, sizeof nome);
    if (!tbVideo(nome)) continue;
    tam = js_num(el, ef, "size", 0);
    if (tam > maior) { maior = tam; id = (int)js_num(el, ef, "id", 0); }
  }
  return id;
}

static void parseData(const char *json) {
  const char *p, *f;
  limpar();
  if (!json) return;
  p = strstr(json, "\"data\"");
  if (!p) return;
  p = strchr(p, '[');
  if (!p) return;
  p++;
  while (*p && nItens < TBUI_MAX) {
    while (*p && *p != '{') p++;
    if (*p != '{') break;
    f = js_fim(p);
    if (!f) break;
    {
      char idb[24], nome[160], hash[48];
      int id = 0, usenet = 0;
      nome[0] = hash[0] = idb[0] = 0;
      js_texto_raiz_em(p, f, "name", nome, sizeof nome);
      js_texto_raiz_em(p, f, "hash", hash, sizeof hash);
      // id e usenet sao NUMEROS/booleanos no JSON do TorBox, nao strings:
      // js_texto_raiz so aceita string e um "id":41 saia como 0 — o OK pedia
      // sempre o arquivo do item 0.
      id = (int)js_num(p, f, "id", 0);
      // usenet pode vir numero (0/1) ou booleano (true/false); js_num so le
      // numero, entao o booleano se confere no texto do campo.
      { const char *ub = strstr(p, "\"usenet\"");
        usenet = js_num(p, f, "usenet", -1) == 1 || (ub && ub < f && strstr(ub, "true") && strstr(ub, "true") < f); }
      if (!nome[0]) snprintf(nome, sizeof nome, "Item %d", id);
      itens[nItens].itemId = id;
      itens[nItens].arquivoId = arquivoPrincipal(p, f);
      itens[nItens].usenet = usenet;
      snprintf(itens[nItens].nome, sizeof itens[nItens].nome, "%s", nome);
      snprintf(itens[nItens].hash, sizeof itens[nItens].hash, "%s", hash);
      nItens++;
    }
    p = f + 1;
  }
}

void torboxbib_ui_atualizar(void) {
  if (!torboxbib_disponivel()) {
    limpar();
    snprintf(status, sizeof status, "TorBox não configurado");
    return;
  }
  if (fioVivo && atomic_load_explicit(&respostaPronta, memory_order_acquire)) {
    char *json = respostaJson;
    int st = respostaSt;
    pthread_join(fio, NULL);
    fioVivo = 0;
    respostaJson = NULL;
    atomic_store_explicit(&respostaPronta, 0, memory_order_release);
    if (!json) {
      limpar();
      snprintf(status, sizeof status, st ? "Erro HTTP %d" : "Falha na lista", st);
    } else {
      parseData(json);
      free(json);
      snprintf(status, sizeof status, nItens == 0 ? "Lista vazia" : "%d itens", nItens);
    }
    return;
  }
  if (!fioVivo && nItens == 0 && !status[0]) disparar();
}

int torboxbib_ui_visivel(void) { return torboxbib_disponivel(); }
int torboxbib_ui_n(void) { return nItens; }
int torboxbib_ui_pagina(void) { return pagina; }

const char *torboxbib_ui_nome(int i) {
  return i >= 0 && i < nItens ? itens[i].nome : "";
}

const char *torboxbib_ui_status(void) { return status; }

void torboxbib_ui_pagina_set(int p) {
  if (p < 0) p = 0;
  pagina = p;
  limpar();
  disparar();
}

int torboxbib_ui_item_id(int i) {
  return i >= 0 && i < nItens ? itens[i].itemId : 0;
}

int torboxbib_ui_usenet(int i) {
  return i >= 0 && i < nItens ? itens[i].usenet : 0;
}

int torboxbib_ui_escolher(int i, int *usenet, int *item, int *arquivo) {
  if (i < 0 || i >= nItens || itens[i].itemId <= 0) return 0;
  partes_arquivo_definir("torbox:bib", itens[i].usenet, itens[i].itemId, itens[i].arquivoId);
  if (usenet) *usenet = itens[i].usenet;
  if (item) *item = itens[i].itemId;
  if (arquivo) *arquivo = itens[i].arquivoId;
  return 1;
}

void torboxbib_ui_entrar(void) {
  pagina = 0;
  limpar();
  disparar();
}
