#include "torboxbib_ui.h"
#include "torboxbib.h"
#include "debrid.h"
#include "partes.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TBUI_MAX 200

static struct {
  int itemId;
  int usenet;
  char nome[160];
  char hash[48];
} itens[TBUI_MAX];
static int nItens, pagina, carregando;
static char status[96];

static void limpar(void) {
  nItens = 0;
  status[0] = 0;
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
    f = strchr(p, '}');
    if (!f) break;
    {
      char idb[24], nome[160], hash[48];
      int id = 0, usenet = 0;
      nome[0] = hash[0] = idb[0] = 0;
      if (js_texto_raiz(p, "name", nome, sizeof nome) && nome[0]) { /* */ }
      js_texto_raiz(p, "hash", hash, sizeof hash);
      if (js_texto_raiz(p, "id", idb, sizeof idb)) id = atoi(idb);
      js_texto_raiz(p, "usenet", idb, sizeof idb);
      if (idb[0] == '1' || (idb[0] == 't' && idb[1] == 'r')) usenet = 1;
      if (!nome[0]) snprintf(nome, sizeof nome, "Item %d", id);
      itens[nItens].itemId = id;
      itens[nItens].usenet = usenet;
      snprintf(itens[nItens].nome, sizeof itens[nItens].nome, "%s", nome);
      snprintf(itens[nItens].hash, sizeof itens[nItens].hash, "%s", hash);
      nItens++;
    }
    p = f + 1;
  }
}

void torboxbib_ui_atualizar(void) {
  int st = 0;
  char *json;
  if (!torboxbib_disponivel()) {
    limpar();
    snprintf(status, sizeof status, "TorBox não configurado");
    return;
  }
  if (carregando) return;
  carregando = 1;
  json = torboxbib_carregar_json(pagina, &st);
  carregando = 0;
  if (!json) {
    limpar();
    snprintf(status, sizeof status, st ? "Erro HTTP %d" : "Falha na lista", st);
    return;
  }
  parseData(json);
  free(json);
  if (nItens == 0)
    snprintf(status, sizeof status, "Lista vazia");
  else
    snprintf(status, sizeof status, "%d itens", nItens);
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
  torboxbib_ui_atualizar();
}

int torboxbib_ui_item_id(int i) {
  return i >= 0 && i < nItens ? itens[i].itemId : 0;
}

int torboxbib_ui_usenet(int i) {
  return i >= 0 && i < nItens ? itens[i].usenet : 0;
}

void torboxbib_ui_play_index(int i) {
  if (i < 0 || i >= nItens) return;
  partes_arquivo_definir("torbox:bib", itens[i].usenet, itens[i].itemId, 0);
  partes_link_pedir(itens[i].usenet, itens[i].itemId, 0, 1);
}

void torboxbib_ui_entrar(void) {
  pagina = 0;
  torboxbib_ui_atualizar();
}
