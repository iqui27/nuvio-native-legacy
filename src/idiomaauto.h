// IDIOMA AUTOMATICO DA INTERFACE — a parte pura: codigo de idioma (da conta ou
// da TV) -> um dos IDIOMA_* de idiomacod.h, e a ordem de precedencia.
// Header-only e sem dependencia de proposito (como idiomacod.h): o teste
// tests/idiomaauto.c o inclui sozinho, e ajustes.c so cuida de guardar e gravar.
//
// DE ONDE VEM A REGRA. No app web oficial (NuvioWeb 0.3.38) a conta NAO
// sincroniza o idioma da interface — ele mora em ThemeStore.language, local — e
// sem escolha manual o web usa o locale do sistema. A conta sincroniza, sim,
// tmdb_language e subtitle_preferred_language (profileSettingsSyncService.js),
// que dizem em que lingua a pessoa le. Aqui isso decide a interface enquanto a
// pessoa nunca escolheu uma na TV:
//   1. tmdb_language           ("pt-BR", "ro-RO", "uk", "es-419"...)
//   2. subtitle_preferred_language ("ro", "rum", "ron", "ukr", "fre", "ger"...)
//   3. o locale da TV          ("pt-BR", "en_US.UTF-8"...)
//   4. ingles
#ifndef NV_IDIOMAAUTO_H
#define NV_IDIOMAAUTO_H

#include <stddef.h>
#include "idiomacod.h"

// Origem da decisao, para o log e para o aviso.
enum { IDA_TMDB = 0, IDA_LEGENDA = 1, IDA_SISTEMA = 2, IDA_PADRAO = 3 };

static inline const char *idiomaauto_fonte_nome(int fonte) {
  switch (fonte) {
    case IDA_TMDB:    return "tmdb_language";
    case IDA_LEGENDA: return "legenda";
    case IDA_SISTEMA: return "sistema";
    default:          return "padrao";
  }
}

// Codigo curto do idioma, para o log e para o id do aviso ("idioma:pt-PT"). E o
// BCP-47 sem regiao, exceto onde a regiao/escrita e o que distingue as tabelas:
// portugues do Brasil x de Portugal e chines simplificado x tradicional.
static inline const char *idiomaauto_codigo(int idioma) {
  static const char *C[IDIOMA_N] = {
    "pt", "en", "ro", "uk", "ru", "fr", "de", "es", "it", "nl", "pl", "tr", "pt-PT",
    "sv", "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr", "bg", "el", "id",
    "vi", "ja", "zh-CN", "zh-TW", "ar"
  };
  return idioma >= 0 && idioma < IDIOMA_N ? C[idioma] : "en";
}

static inline int ida_igual(const char *a, const char *b) {
  for (; *a && *a == *b; a++, b++) {}
  return *a == *b;
}

// Um codigo de idioma qualquer -> IDIOMA_*, ou -1 se nao e um dos trinta (ou nao
// e idioma: "", "none", "off", "DEVICE"). Aceita o que aparece de fato:
//   BCP-47 / locale   pt-BR  pt_PT  es-419  ro-RO  en_US.UTF-8  zh-Hans  sr-Latn
//   ISO 639-1         pt en ro uk ru fr de es it nl pl tr sv da nb no cs sk ...
//   ISO 639-2         por eng ron/rum ukr rus fra/fre deu/ger spa ita nld/dut ...
//   OpenSubtitles     pob (portugues do Brasil), pb
// Sem distinguir caixa. O idioma PRIMARIO decide quase tudo (a regiao nao muda a
// lingua da interface: es-419 e "es", en-GB e "en"). As EXCECOES sao as que tem
// tabela propria:
//   pt-PT (e ao, mz, cv)          portugues europeu; pt, pt-BR, pb e pob: o do Brasil
//   nb, nn, no                    noruegues (uma tabela so, em bokmal)
//   sr, sr-Latn, sr-Cyrl, sr-RS   servio em latim (quem le em cirilico le em latim,
//                                 e cair no ingles seria pior)
//   zh-Hant, zh-TW, zh-HK, zh-MO  chines tradicional; zh, zh-CN, zh-SG, zh-Hans: simplificado
static inline int idiomaauto_mapear(const char *cod) {
  char p[8], sub[2][8];
  size_t n = 0;
  int ns = 0, hant = 0, pt_eu = 0, hans = 0;
  if (!cod) return -1;
  while (*cod == ' ' || *cod == '\t') cod++;
  for (; *cod && *cod != '-' && *cod != '_' && *cod != '.' && *cod != '@' &&
         *cod != ' ' && *cod != ',' && *cod != ';'; cod++) {
    if (n + 1 >= sizeof p) return -1;             // mais longo que qualquer codigo
    p[n++] = (*cod >= 'A' && *cod <= 'Z') ? (char)(*cod + 32) : *cod;
  }
  p[n] = 0;
  // Subtags (escrita "Hant", regiao "PT"): so as duas primeiras interessam.
  while ((*cod == '-' || *cod == '_') && ns < 2) {
    size_t k = 0;
    cod++;
    for (; *cod && *cod != '-' && *cod != '_' && *cod != '.' && *cod != '@' &&
           *cod != ' ' && *cod != ',' && *cod != ';'; cod++)
      if (k + 1 < sizeof sub[0]) sub[ns][k++] = (*cod >= 'A' && *cod <= 'Z') ? (char)(*cod + 32) : *cod;
    sub[ns][k] = 0;
    ns++;
  }
  { int i;
    for (i = 0; i < ns; i++) {
      if (ida_igual(sub[i], "hant") || ida_igual(sub[i], "tw") || ida_igual(sub[i], "hk") ||
          ida_igual(sub[i], "mo")) hant = 1;
      if (ida_igual(sub[i], "hans") || ida_igual(sub[i], "cn") || ida_igual(sub[i], "sg")) hans = 1;
      if (ida_igual(sub[i], "pt") || ida_igual(sub[i], "ao") || ida_igual(sub[i], "mz") ||
          ida_igual(sub[i], "cv")) pt_eu = 1;
    } }
  if (n == 2) {
    if (ida_igual(p, "pt")) return pt_eu ? IDIOMA_PTPT : IDIOMA_PT;
    if (ida_igual(p, "pb")) return IDIOMA_PT;
    if (ida_igual(p, "en")) return IDIOMA_EN;
    if (ida_igual(p, "ro") || ida_igual(p, "mo")) return IDIOMA_RO;
    if (ida_igual(p, "uk")) return IDIOMA_UK;
    if (ida_igual(p, "ru")) return IDIOMA_RU;
    if (ida_igual(p, "fr")) return IDIOMA_FR;
    if (ida_igual(p, "de")) return IDIOMA_DE;
    if (ida_igual(p, "es")) return IDIOMA_ES;
    if (ida_igual(p, "it")) return IDIOMA_IT;
    if (ida_igual(p, "nl")) return IDIOMA_NL;
    if (ida_igual(p, "pl")) return IDIOMA_PL;
    if (ida_igual(p, "tr")) return IDIOMA_TR;
    if (ida_igual(p, "sv")) return IDIOMA_SV;
    if (ida_igual(p, "da")) return IDIOMA_DA;
    if (ida_igual(p, "no") || ida_igual(p, "nb") || ida_igual(p, "nn")) return IDIOMA_NO;
    if (ida_igual(p, "cs")) return IDIOMA_CS;
    if (ida_igual(p, "sk")) return IDIOMA_SK;
    if (ida_igual(p, "sl")) return IDIOMA_SL;
    if (ida_igual(p, "hu")) return IDIOMA_HU;
    if (ida_igual(p, "lt")) return IDIOMA_LT;
    if (ida_igual(p, "bs")) return IDIOMA_BS;
    if (ida_igual(p, "sr")) return IDIOMA_SR;
    if (ida_igual(p, "bg")) return IDIOMA_BG;
    if (ida_igual(p, "el")) return IDIOMA_EL;
    if (ida_igual(p, "id") || ida_igual(p, "in")) return IDIOMA_ID;
    if (ida_igual(p, "vi")) return IDIOMA_VI;
    if (ida_igual(p, "ja")) return IDIOMA_JA;
    if (ida_igual(p, "zh")) return hant && !hans ? IDIOMA_ZHTW : IDIOMA_ZHCN;
    if (ida_igual(p, "ar")) return IDIOMA_AR;
    return -1;
  }
  if (n == 3) {
    if (ida_igual(p, "por") || ida_igual(p, "pob")) return IDIOMA_PT;
    if (ida_igual(p, "eng")) return IDIOMA_EN;
    if (ida_igual(p, "ron") || ida_igual(p, "rum")) return IDIOMA_RO;
    if (ida_igual(p, "ukr")) return IDIOMA_UK;
    if (ida_igual(p, "rus")) return IDIOMA_RU;
    if (ida_igual(p, "fra") || ida_igual(p, "fre")) return IDIOMA_FR;
    if (ida_igual(p, "deu") || ida_igual(p, "ger")) return IDIOMA_DE;
    if (ida_igual(p, "spa")) return IDIOMA_ES;
    if (ida_igual(p, "ita")) return IDIOMA_IT;
    if (ida_igual(p, "nld") || ida_igual(p, "dut")) return IDIOMA_NL;
    if (ida_igual(p, "pol")) return IDIOMA_PL;
    if (ida_igual(p, "tur")) return IDIOMA_TR;
    if (ida_igual(p, "swe")) return IDIOMA_SV;
    if (ida_igual(p, "dan")) return IDIOMA_DA;
    if (ida_igual(p, "nor") || ida_igual(p, "nob") || ida_igual(p, "nno")) return IDIOMA_NO;
    if (ida_igual(p, "ces") || ida_igual(p, "cze")) return IDIOMA_CS;
    if (ida_igual(p, "slk") || ida_igual(p, "slo")) return IDIOMA_SK;
    if (ida_igual(p, "slv")) return IDIOMA_SL;
    if (ida_igual(p, "hun")) return IDIOMA_HU;
    if (ida_igual(p, "lit")) return IDIOMA_LT;
    if (ida_igual(p, "bos")) return IDIOMA_BS;
    if (ida_igual(p, "srp")) return IDIOMA_SR;
    if (ida_igual(p, "bul")) return IDIOMA_BG;
    if (ida_igual(p, "ell") || ida_igual(p, "gre")) return IDIOMA_EL;
    if (ida_igual(p, "ind")) return IDIOMA_ID;
    if (ida_igual(p, "vie")) return IDIOMA_VI;
    if (ida_igual(p, "jpn")) return IDIOMA_JA;
    if (ida_igual(p, "zho") || ida_igual(p, "chi")) return hant && !hans ? IDIOMA_ZHTW : IDIOMA_ZHCN;
    if (ida_igual(p, "yue")) return IDIOMA_ZHTW;    // cantones: escrita tradicional
    if (ida_igual(p, "ara")) return IDIOMA_AR;
  }
  return -1;
}

// A REGRA. Cada fonte pode ser NULL ou vazia; uma que nao mapeia para um dos
// trinta (a conta em coreano, digamos) e pulada e a proxima decide. `fonte`
// (opcional) recebe IDA_*.
static inline int idiomaauto_resolver(const char *tmdb, const char *legenda,
                                      const char *sistema, int *fonte) {
  int r, f;
  if ((r = idiomaauto_mapear(tmdb)) >= 0)         f = IDA_TMDB;
  else if ((r = idiomaauto_mapear(legenda)) >= 0) f = IDA_LEGENDA;
  else if ((r = idiomaauto_mapear(sistema)) >= 0) f = IDA_SISTEMA;
  else { r = IDIOMA_EN; f = IDA_PADRAO; }
  if (fonte) *fonte = f;
  return r;
}

#endif
