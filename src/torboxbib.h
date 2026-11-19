#ifndef NV_TORBOXBIB_H
#define NV_TORBOXBIB_H
/* Aba mínima: lista mylist TorBox (debrid_tb_mylist). */
int torboxbib_disponivel(void);
int torboxbib_carregar_pagina(int pagina, char *buf, size_t n);
#endif
