#ifndef NV_TORBOXBIB_UI_H
#define NV_TORBOXBIB_UI_H
#include <stddef.h>

void torboxbib_ui_entrar(void);
void torboxbib_ui_atualizar(void);
int torboxbib_ui_visivel(void);
int torboxbib_ui_n(void);
int torboxbib_ui_pagina(void);
void torboxbib_ui_pagina_set(int p);
const char *torboxbib_ui_nome(int i);
const char *torboxbib_ui_status(void);
int torboxbib_ui_item_id(int i);
int torboxbib_ui_usenet(int i);
/* Pedido do item i: grava o alvo "torbox:bib" para o Airlock e devolve o
   usenet/item escolhidos e o arquivo principal (o maior video de "files"),
   para o chamador disparar o link DEPOIS de limpar o estado anterior
   (parteZerar cancela qualquer pedido em voo). */
int torboxbib_ui_escolher(int i, int *usenet, int *item, int *arquivo);

#endif
