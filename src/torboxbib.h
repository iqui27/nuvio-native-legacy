#ifndef NV_TORBOXBIB_H
#define NV_TORBOXBIB_H
#include <stddef.h>

/* Biblioteca TorBox: lista mylist (debrid_tb_mylist). */
int torboxbib_disponivel(void);
char *torboxbib_carregar_json(int pagina, int *stOut);
int torboxbib_carregar_pagina(int pagina, char *buf, size_t n);
int torboxbib_contar_itens(const char *json);

#endif
