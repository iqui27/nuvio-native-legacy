#ifndef NV_ARTEFALTA_H
#define NV_ARTEFALTA_H
// ARTE QUE A ATUALIZACAO DO .tpk NAO TROUXE (#290).
//
// No Samsung nativo a auto-atualizacao troca SO a libnuvio.so (atualizacao.c);
// o res/ do pacote continua o da instalacao. Todo icone, logo ou marca que
// entrou no repositorio depois da versao instalada pelo .tpk falta no disco, e
// o codigo novo pede por ele. Medido no D1 (06/10, tizen-tpk 2.0.x): 14 pessoas
// com "[tex] decode falhou (Couldn't open .../res/art/icones/pl_pause-f.png: No
// such file)" — os botoes do player e os da ficha (Explorar, Ver trailer)
// apareciam como circulos vazios.
//
// Saida: o arquivo que falta no res/art vem da MESMA tag no GitHub
// (raw.githubusercontent.com/.../v<versao>/deploy/app/art/<resto>), pelo cache
// de arte de sempre — baixa uma vez, fica no disco. Versao "dev" ou com algo
// alem de digitos e pontos nao tem tag: devolve 0 e o icone falta como antes.
//
// Funcao pura (sem I/O), testada em tests/artefalta.c.
#include <stdio.h>
#include <string.h>

#define ARTE_RAIZ_PACOTE "/res/art/"
#define ARTE_RAIZ_REMOTA "https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/v"

static inline int arte_url_remota(const char *caminho, const char *versao, char *dst, size_t tam) {
  const char *p, *v;
  int n;
  if (!caminho || !versao || !dst || tam == 0) return 0;
  if (!versao[0] || versao[0] < '0' || versao[0] > '9') return 0;
  for (v = versao; *v; v++) if (!((*v >= '0' && *v <= '9') || *v == '.')) return 0;
  if (!strncmp(caminho, "http://", 7) || !strncmp(caminho, "https://", 8)) return 0;
  p = strstr(caminho, ARTE_RAIZ_PACOTE);
  if (!p) return 0;
  p += strlen(ARTE_RAIZ_PACOTE);
  if (!*p || strstr(p, "..") || strchr(p, '?') || strchr(p, '#') || strchr(p, ' ')) return 0;
  n = snprintf(dst, tam, "%s%s/deploy/app/art/%s", ARTE_RAIZ_REMOTA, versao, p);
  if (n < 0 || (size_t)n >= tam) { dst[0] = 0; return 0; }
  return 1;
}

#endif
