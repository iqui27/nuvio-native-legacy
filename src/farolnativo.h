// FAROL — gramatica UNICA dos codigos nativos que farolerro_ler emite.
//
// Fonte unica da lista fechada do PLANO 2.3: o classificador (farolerro.c,
// farol-3) e o redator (farolredator.c, farol-4) incluem este cabecalho — as
// duas pontas aceitam/produzem exatamente o mesmo conjunto, sem segunda copia.
// farolerro_ler produz:
//   fixos:   ums_pipeline ums_faixa_audio ls2_permissao m3_parado m3_audio
//            tpk_conexao tpk_audio tpk_conflito tpk_preso av_conexao
//   genericos (fontesonda_causa): <p>_<rede|conexao|formato|http>
//            rede p/ ums/m3/gen, conexao p/ av/tpk, formato e http p/ todos
//   numericos: ums_<100-300|40000-49999>, m3_<2000-2008 (exceto 2003,2007)|
//            3000-4999>
//   dinamico:  av_<nome do enum AVPlay em minusculas, 1-20 de [a-z0-9_]>
//            (lerWgt: isalnum+'_' apos "player_error", sem exigir '_' dentro)
//   desconhecido: <p>_outro, p in {ums,av,tpk,m3,gen}
#ifndef FAROLNATIVO_H
#define FAROLNATIVO_H
#include <stdlib.h>
#include <string.h>

static const char *const farolnativo_fixos[] = {
  "ums_pipeline", "ums_faixa_audio", "ls2_permissao",
  "m3_parado", "m3_audio",
  "av_conexao", "tpk_conexao", "tpk_audio", "tpk_conflito", "tpk_preso",
  "ums_rede", "m3_rede", "gen_rede",
  "ums_formato", "m3_formato", "gen_formato", "av_formato", "tpk_formato",
  "ums_http", "m3_http", "gen_http", "av_http", "tpk_http",
  "ums_outro", "av_outro", "tpk_outro", "m3_outro", "gen_outro",
};
#define FAROLNATIVO_FIXOS_N (sizeof farolnativo_fixos / sizeof *farolnativo_fixos)

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
  for (size_t i = 0; i < FAROLNATIVO_FIXOS_N; i++)
    if (!strcmp(s, farolnativo_fixos[i])) return 1;
  if (farolnativo_numerico(s, n, "ums_", 4)) return 1;
  if (farolnativo_numerico(s, n, "m3_", 3)) return 1;
  if (!strncmp(s, "av_", 3) && n - 3 <= 20) {
    // nome do enum AVPlay ja vem minusculo: isalnum+'_' como em lerWgt.
    return strspn(s + 3, "abcdefghijklmnopqrstuvwxyz0123456789_") == n - 3;
  }
  return 0;
}
#endif
