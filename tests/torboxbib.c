#include "torboxbib.h"
#include "torboxbib_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int chamadas;
static char ultimoJson[4096];

char *debrid_tb_mylist(int usenet, int item, int pagina, int fresco, int *stOut) {
  (void)usenet; (void)item; (void)fresco;
  chamadas++;
  if (stOut) *stOut = 200;
  return strdup(ultimoJson);
}

int debrid_tb_link(int usenet, int id, int arquivo, char *url, unsigned n) {
  (void)usenet; (void)id; (void)arquivo; (void)url; (void)n;
  return 0;
}

int debrid_tb_geracao(void) { return 1; }

void partes_arquivo_definir(const char *alvo, int usenet, int item, int arquivo) {
  (void)alvo; (void)usenet; (void)item; (void)arquivo;
}

int main(void) {
  int u = -1, it = -1;
  // id NUMERICO e usenet booleano, como o TorBox devolve (probe do revisor).
  snprintf(ultimoJson, sizeof ultimoJson,
    "{\"success\":true,\"data\":["
    "{\"id\":41,\"name\":\"Filme 41\",\"hash\":\"aa\",\"usenet\":false},"
    "{\"id\":7,\"name\":\"Usenet 7\",\"hash\":\"bb\",\"usenet\":true}]}");

  torboxbib_ui_entrar();
  // A busca e assincrona: nenhuma chamada pode ter saida no fio da UI ainda,
  // e a coleta espera a resposta do fio.
  for (int i = 0; i < 400 && !torboxbib_ui_n(); i++) {
    torboxbib_ui_atualizar();
    { struct timespec ts = {0, 2000000}; nanosleep(&ts, NULL); }
  }
  assert(torboxbib_ui_n() == 2);
  assert(chamadas == 1);                       // uma busca por resposta, nao por quadro
  assert(!strcmp(torboxbib_ui_nome(0), "Filme 41"));
  assert(torboxbib_ui_item_id(0) == 41);       // id numerico lido, nao 0
  assert(torboxbib_ui_usenet(0) == 0);
  assert(torboxbib_ui_item_id(1) == 7);
  assert(torboxbib_ui_usenet(1) == 1);

  // A escolha devolve o item certo; quem chama dispara o link DEPOIS de
  // limpar o estado (parteZerar cancela pedidos em voo).
  assert(torboxbib_ui_escolher(0, &u, &it) == 1 && it == 41 && u == 0);
  assert(torboxbib_ui_escolher(1, &u, &it) == 1 && it == 7 && u == 1);
  assert(torboxbib_ui_escolher(9, NULL, NULL) == 0);

  // Repetidas atualizacoes NAO disparam busca nova enquanto a lista esta cheia.
  torboxbib_ui_atualizar();
  torboxbib_ui_atualizar();
  assert(chamadas == 1);

  // Lista vazia e erro HTTP seguem pelo mesmo fio.
  snprintf(ultimoJson, sizeof ultimoJson, "{\"success\":true,\"data\":[]}");
  torboxbib_ui_pagina_set(2);
  for (int i = 0; i < 400 && chamadas < 2; i++)
    { struct timespec ts = {0, 2000000}; nanosleep(&ts, NULL); }
  for (int i = 0; i < 400; i++) { torboxbib_ui_atualizar();
    if (strstr(torboxbib_ui_status(), "vazia")) break;
    { struct timespec ts = {0, 2000000}; nanosleep(&ts, NULL); } }
  assert(torboxbib_ui_n() == 0);
  assert(strstr(torboxbib_ui_status(), "vazia") != NULL);

  puts("torboxbib: ok");
  return 0;
}
