// QUAIS ADD-ONS DE TV APARECEM NO GUIA (#283). So o guia: o add-on continua
// ligado no resto do app (home, busca, fontes). O padrao e TODOS aparecerem;
// o que fica guardado, por perfil, e a lista dos que a pessoa ESCONDEU, pela
// base (URL) do add-on — a mesma chave que o canal carrega em GCanal.base.
//
// Modulo sem rede nem tela: texto <-> lista, para testar sem a TV.
#ifndef NV_GUIAADDONS_H
#define NV_GUIAADDONS_H
#include <stddef.h>

#define GUIAADDONS_MAX 16   /* mesmo teto do painel de add-ons do guia */

typedef struct {
  int n;
  char base[GUIAADDONS_MAX][2048];
} GuiaAddonsOcultos;

// Uma base por linha. Linha vazia, repetida ou alem do teto e ignorada.
void guiaaddons_ler(GuiaAddonsOcultos *o, const char *texto);
// Devolve o tamanho escrito (0 se `dst` e pequeno demais para tudo).
size_t guiaaddons_texto(const GuiaAddonsOcultos *o, char *dst, size_t n);
int  guiaaddons_oculto(const GuiaAddonsOcultos *o, const char *base);
// Inverte o estado; devolve 1 se agora esta ESCONDIDO. Lista cheia: nao
// esconde mais um (devolve 0).
int  guiaaddons_alternar(GuiaAddonsOcultos *o, const char *base);
// "guia-addons-p<perfil>.txt" — por perfil, como o cache do guia.
const char *guiaaddons_arquivo(int perfil, char *dst, size_t n);

#endif
