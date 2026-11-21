// FAROL — gramatica UNICA dos codigos nativos que farolerro_ler emite.
//
// Fonte unica da lista fechada do PLANO 2.3: os 28 codigos fixos sao os do
// classificador do Farol 3 — este cabecalho inclui farolerro.h e o redator
// (farolredator.c, farol-4) itera farolerro_codigos/farolerro_codigos_n,
// sem copia. O que nao pode vir de array — as familias geradas na hora —
// fica espelhado aqui como predicado, na mesma regra de farolerro.c:
//   numericos: ums_<100-300|40000-49999>, m3_<2000-2008 (exceto 2003,2007)|
//            3000-4999>           (lerWebos/lerAndroid -> numero())
//   dinamico:  av_<nome do enum AVPlayError em minusculas> — LISTA FECHADA:
//            so os 11 nomes que a Samsung documenta (avplay API, enum
//            AVPlayError). lerWgt recorta 20 bytes depois de "player_error"
//            em qualquer posicao do texto — inclusive dentro de uma URL na
//            mensagem — entao o redator aceita apenas os nomes da tabela;
//            qualquer outro trecho e texto livre e nao sai (PLANO 2.5).
//   genericos (fontesonda_causa): <p>_<rede|conexao|formato|http> — ja sao
//            entradas de farolerro_codigos.
//   desconhecido: <p>_outro, p in {ums,av,tpk,m3,gen} — idem.
#ifndef FAROLNATIVO_H
#define FAROLNATIVO_H
#include "farolerro.h"
#include <stdlib.h>
#include <string.h>

// Os nomes do enum AVPlayError documentados pela Samsung (sem o prefixo
// PLAYER_ERROR_, em minusculas como lerWgt emite). PLAYER_ERROR_GENEREIC e o
// nome real — o erro de grafia e da propria API. Um nome novo que aparecer no
// D1 cai em av_outro ate entrar aqui e no classificador.
static const char *const farolnativo_av[] = {
  "none", "invalid_parameter", "no_such_file", "invalid_operation",
  "seek_failed", "invalid_state", "not_supported_file", "not_supported_format",
  "invalid_uri", "connection_failed", "genereic",
};
#define FAROLNATIVO_AV_N (sizeof farolnativo_av / sizeof *farolnativo_av)

static inline int farolnativo_numerico(const char *s, size_t n,
                                       const char *prefixo, size_t np) {
  // "<prefixo><digitos>" dentro das faixas que o classificador emite.
  unsigned long v;
  char *fim;
  if (strncmp(s, prefixo, np) || n == np || n - np > 5 ||
      strspn(s + np, "0123456789") != n - np)
    return 0;
  v = strtoul(s + np, &fim, 10);
  if (prefixo[0] == 'u')
    return (v >= 100 && v <= 300) || (v >= 40000 && v <= 49999);
  return (v >= 2000 && v <= 2008 && v != 2003 && v != 2007) ||
         (v >= 3000 && v <= 4999);
}

static inline int farolnativo_cod_ok(const char *s) {
  size_t n = s ? strlen(s) : 0;
  if (n < 4 || n > 23) return 0;
  for (int i = 0; i < farolerro_codigos_n; i++)
    if (!strcmp(s, farolerro_codigos[i])) return 1;
  if (farolnativo_numerico(s, n, "ums_", 4)) return 1;
  if (farolnativo_numerico(s, n, "m3_", 3)) return 1;
  if (!strncmp(s, "av_", 3)) {
    for (size_t i = 0; i < FAROLNATIVO_AV_N; i++)
      if (!strcmp(s + 3, farolnativo_av[i])) return 1;
  }
  return 0;
}
#endif
