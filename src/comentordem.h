// Ordem dos comentarios do Trakt: os escritos no idioma da interface primeiro.
//
// O texto de um comentario e de uma PESSOA e nao se traduz; o que se pode fazer
// e nao enterrar os que a pessoa consegue ler. O Trakt marca cada comentario com
// `language` (ISO 639-1, "en", "pt"), e a rota /comments/likes nao tem filtro de
// idioma — entao a lista chega toda misturada e a ordenacao mora aqui, sobre os
// oito que ja foram pedidos. NENHUM pedido a mais.
//
// ESTAVEL: dentro de cada grupo continua a ordem de curtidas que o Trakt deu.
// Sem `language` (comentario antigo) conta como "nao sei" e fica no grupo do
// idioma da interface — melhor que rebaixar o que talvez seja legivel.
#ifndef NV_COMENTORDEM_H
#define NV_COMENTORDEM_H

#include <string.h>

typedef struct {
  char u[40]; char *t; int c, nota; char l[4];
  char data[40]; int spoiler, respostas;
} ComentAchado;

// 1 = `lingua` (do Trakt, pode ser "" ou "pt-br") e o idioma `iso` da interface.
static inline int coment_mesmo_idioma(const char *lingua, const char *iso) {
  if (!lingua || !lingua[0]) return 1;
  return iso && lingua[0] == iso[0] && lingua[1] == iso[1];
}

// Reordena `v[0..n)` in-place: idioma da interface primeiro, depois o resto.
// Devolve quantos ficaram no primeiro grupo.
static inline int coment_ordenar(ComentAchado *v, int n, const char *iso) {
  ComentAchado tmp[16];
  int i, k = 0, m = 0;
  if (n < 2 || n > 16) {
    for (i = 0, k = 0; i < n; i++) k += coment_mesmo_idioma(v[i].l, iso);
    return k;
  }
  for (i = 0; i < n; i++)
    if (coment_mesmo_idioma(v[i].l, iso)) tmp[m++] = v[i];
  k = m;
  for (i = 0; i < n; i++)
    if (!coment_mesmo_idioma(v[i].l, iso)) tmp[m++] = v[i];
  memcpy(v, tmp, sizeof(ComentAchado) * (size_t)n);
  return k;
}

#endif
