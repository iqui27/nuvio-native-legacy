// FONTES SOB DEMANDA (16/11/2026, como o AIOStreams faz no navegador): das
// fontes anexadas a um MKV, o libass so recebe as que o script pede — pelo
// Fontname dos estilos e pelos \fn dos eventos. Um lancamento de fansub traz
// 30-40 fontes (dezenas de MB) e usa poucas; cada uma entregue e copiada pelo
// libass e aberta pelo FreeType no fontselect.
//
// O casamento e o mesmo do fontselect do libass: o nome pedido (sem o '@' da
// escrita vertical, sem diferenca de maiusculas ASCII) contra a familia (IDs 1
// e 16), o nome completo (4) e o nome PostScript (6) da tabela 'name' de cada
// face (colecoes .ttc inteiras). Fonte cuja tabela nao se le fica (na duvida,
// entrega).
#ifndef NV_FONTEFAMILIAS_H
#define NV_FONTEFAMILIAS_H

#include <stddef.h>

typedef struct {
  char **nomes;   // minusculas ASCII, sem '@' e sem espacos nas pontas
  int n, cap;
} FonteFamilias;

// Junta ao conjunto as familias que o documento ASS pede: a coluna Fontname
// das linhas Style: (pelo Format: da secao de estilos) e todo \fn<nome>.
void fontefam_varrer(FonteFamilias *s, const char *doc, size_t n);
int  fontefam_tem(const FonteFamilias *s, const char *nome);
void fontefam_por(FonteFamilias *s, const char *nome, size_t n);
void fontefam_liberar(FonteFamilias *s);

// Chama `cb` com cada nome pelo qual o libass acharia a fonte (todas as faces
// de uma colecao). Devolve quantos nomes leu; 0 = nao deu para ler.
int fontefam_nomes(const unsigned char *d, size_t n, void (*cb)(const char *nome, void *u), void *u);

// 1 = a fonte casa com alguma familia de `s`, ou os nomes dela nao se leem.
int fontefam_quer(const FonteFamilias *s, const unsigned char *d, size_t n);

#endif
