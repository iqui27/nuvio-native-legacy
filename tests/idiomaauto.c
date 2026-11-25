// IDIOMA AUTOMATICO: o mapa codigo -> IDIOMA_* e a ordem de precedencia.
// Funcao pura (src/idiomaauto.h), sem SDL, sem disco.
#include "../src/idiomaauto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define M(cod, esperado) do { \
  int r_ = idiomaauto_mapear(cod); \
  if (r_ != (esperado)) { printf("FALHOU mapear(\"%s\") = %d, esperava %d\n", \
    (cod) ? (cod) : "(nulo)", r_, (esperado)); return 1; } } while (0)

#define R(tmdb, leg, sis, idioma, origem) do { \
  int f_ = -1, r_ = idiomaauto_resolver(tmdb, leg, sis, &f_); \
  if (r_ != (idioma) || f_ != (origem)) { \
    printf("FALHOU resolver(%s, %s, %s) = %d/%d, esperava %d/%d\n", \
      (tmdb) ? (tmdb) : "NULL", (leg) ? (leg) : "NULL", (sis) ? (sis) : "NULL", \
      r_, f_, (idioma), (origem)); return 1; } } while (0)

int main(void) {
  // tmdb_language e locales, com regiao
  M("pt-BR", IDIOMA_PT);  M("pt", IDIOMA_PT);
  M("pt_BR", IDIOMA_PT);  M("PT-br", IDIOMA_PT);
  M("en-US", IDIOMA_EN);  M("en-GB", IDIOMA_EN);  M("en", IDIOMA_EN);
  M("ro-RO", IDIOMA_RO);  M("ro", IDIOMA_RO);
  M("uk", IDIOMA_UK);     M("uk-UA", IDIOMA_UK);
  M("ru-RU", IDIOMA_RU);  M("ru", IDIOMA_RU);
  M("fr-FR", IDIOMA_FR);  M("fr-CA", IDIOMA_FR);
  M("de-DE", IDIOMA_DE);  M("de-AT", IDIOMA_DE);
  M("es-ES", IDIOMA_ES);  M("es-419", IDIOMA_ES);  M("es-MX", IDIOMA_ES);
  M("es", IDIOMA_ES);
  // locale de sistema com codificacao e modificador
  M("en_US.UTF-8", IDIOMA_EN);  M("pt_BR.UTF-8", IDIOMA_PT);
  M("ro_RO@euro", IDIOMA_RO);   M("de_DE.utf8", IDIOMA_DE);
  // ISO 639-2 (legenda): bibliografico e terminologico
  M("por", IDIOMA_PT);  M("pob", IDIOMA_PT);  M("eng", IDIOMA_EN);
  M("rum", IDIOMA_RO);  M("ron", IDIOMA_RO);  M("ukr", IDIOMA_UK);
  M("rus", IDIOMA_RU);  M("fre", IDIOMA_FR);  M("fra", IDIOMA_FR);
  M("ger", IDIOMA_DE);  M("deu", IDIOMA_DE);  M("spa", IDIOMA_ES);
  M("RUM", IDIOMA_RO);  M(" ron", IDIOMA_RO);
  // OS 22 DE 2026-09
  M("it", IDIOMA_IT);  M("it-IT", IDIOMA_IT);  M("ita", IDIOMA_IT);  M("it_CH", IDIOMA_IT);
  M("nl-NL", IDIOMA_NL);  M("nl-BE", IDIOMA_NL);  M("nld", IDIOMA_NL);  M("dut", IDIOMA_NL);
  M("pl", IDIOMA_PL);  M("pl-PL", IDIOMA_PL);  M("pol", IDIOMA_PL);
  M("tr-TR", IDIOMA_TR);  M("tur", IDIOMA_TR);
  M("sv-SE", IDIOMA_SV);  M("sv-FI", IDIOMA_SV);  M("swe", IDIOMA_SV);
  M("da-DK", IDIOMA_DA);  M("da", IDIOMA_DA);  M("dan", IDIOMA_DA);
  M("cs-CZ", IDIOMA_CS);  M("cs", IDIOMA_CS);  M("ces", IDIOMA_CS);  M("cze", IDIOMA_CS);
  M("sk-SK", IDIOMA_SK);  M("slk", IDIOMA_SK);  M("slo", IDIOMA_SK);
  M("sl-SI", IDIOMA_SL);  M("slv", IDIOMA_SL);
  M("hu-HU", IDIOMA_HU);  M("hun", IDIOMA_HU);
  M("lt-LT", IDIOMA_LT);  M("lit", IDIOMA_LT);
  M("bs-BA", IDIOMA_BS);  M("bos", IDIOMA_BS);
  M("bg-BG", IDIOMA_BG);  M("bul", IDIOMA_BG);
  M("el-GR", IDIOMA_EL);  M("el", IDIOMA_EL);  M("ell", IDIOMA_EL);  M("gre", IDIOMA_EL);
  M("id-ID", IDIOMA_ID);  M("id", IDIOMA_ID);  M("in", IDIOMA_ID);  M("ind", IDIOMA_ID);
  M("vi-VN", IDIOMA_VI);  M("vie", IDIOMA_VI);
  M("ja-JP", IDIOMA_JA);  M("ja", IDIOMA_JA);  M("jpn", IDIOMA_JA);  M("ja_JP.UTF-8", IDIOMA_JA);
  // portugues: Brasil x Portugal
  M("pt-PT", IDIOMA_PTPT);  M("pt_PT", IDIOMA_PTPT);  M("PT-pt", IDIOMA_PTPT);
  M("pt-AO", IDIOMA_PTPT);  M("pt_PT.UTF-8", IDIOMA_PTPT);  M("pt-BR", IDIOMA_PT);
  // noruegues: no, nb e nn caem na mesma tabela
  M("no", IDIOMA_NO);  M("nb", IDIOMA_NO);  M("nn", IDIOMA_NO);  M("nb-NO", IDIOMA_NO);
  M("nn_NO", IDIOMA_NO);  M("no-NO", IDIOMA_NO);  M("nor", IDIOMA_NO);  M("nob", IDIOMA_NO);
  // servio: qualquer escrita cai no latim (quem le em cirilico le em latim)
  M("sr", IDIOMA_SR);  M("sr-Latn", IDIOMA_SR);  M("sr-Latn-RS", IDIOMA_SR);
  M("sr-Cyrl", IDIOMA_SR);  M("sr-RS", IDIOMA_SR);  M("srp", IDIOMA_SR);
  // chines: simplificado por padrao, tradicional por escrita ou por regiao
  M("zh", IDIOMA_ZHCN);  M("zh-CN", IDIOMA_ZHCN);  M("zh-Hans", IDIOMA_ZHCN);
  M("zh-Hans-CN", IDIOMA_ZHCN);  M("zh-SG", IDIOMA_ZHCN);  M("zh_CN.UTF-8", IDIOMA_ZHCN);
  M("zho", IDIOMA_ZHCN);  M("chi", IDIOMA_ZHCN);
  M("zh-Hant", IDIOMA_ZHTW);  M("zh-TW", IDIOMA_ZHTW);  M("zh-HK", IDIOMA_ZHTW);
  M("zh-MO", IDIOMA_ZHTW);  M("zh-Hant-HK", IDIOMA_ZHTW);  M("zh_TW.UTF-8", IDIOMA_ZHTW);
  M("yue", IDIOMA_ZHTW);
  // o que NAO e um dos trinta, ou nem e idioma
  M(NULL, -1);   M("", -1);   M("none", -1);  M("off", -1);  M("DEVICE", -1);
  M("ar", IDIOMA_AR); M("ar-SA", IDIOMA_AR);
  M("ko", -1);   M("ko-KR", -1);  M("hr", -1);  M("he", -1);  M("th", -1);
  M("C", -1);  M("POSIX", -1);  M("-", -1);  M("e", -1);
  M("portugues", -1);   // nome por extenso nao e codigo
  M("xxxxxxxxxxxxxxxxxxxxxxxx", -1);   // muito longo: nao estoura o buffer

  // precedencia: tmdb_language > legenda > sistema > ingles
  R("pt-BR", "ro", "de-DE", IDIOMA_PT, IDA_TMDB);
  R("ro-RO", NULL, NULL,    IDIOMA_RO, IDA_TMDB);
  R("es-419", "", "fr-FR",  IDIOMA_ES, IDA_TMDB);
  R("uk", "rus", "en-US",   IDIOMA_UK, IDA_TMDB);
  R(NULL, "rum", "en-US",   IDIOMA_RO, IDA_LEGENDA);
  R("", "fre", "de-DE",     IDIOMA_FR, IDA_LEGENDA);
  R("", "ger", NULL,        IDIOMA_DE, IDA_LEGENDA);
  R(NULL, NULL, "pt-BR",    IDIOMA_PT, IDA_SISTEMA);
  R("", "", "ru_RU.UTF-8",  IDIOMA_RU, IDA_SISTEMA);
  R(NULL, NULL, NULL,       IDIOMA_EN, IDA_PADRAO);
  R("", "", "",             IDIOMA_EN, IDA_PADRAO);
  // os novos entram na precedencia como qualquer outro
  R("it-IT", "spa", "de-DE", IDIOMA_IT, IDA_TMDB);
  R("pt-PT", "por", "pt-BR", IDIOMA_PTPT, IDA_TMDB);
  R("", "sr", "de-DE",       IDIOMA_SR, IDA_LEGENDA);
  R(NULL, NULL, "zh_TW.UTF-8", IDIOMA_ZHTW, IDA_SISTEMA);
  R("nb", "swe", "en-US",    IDIOMA_NO, IDA_TMDB);
  // uma fonte que existe mas nao e um dos trinta passa a vez para a proxima
  R("ko", "spa", "de-DE",    IDIOMA_ES, IDA_LEGENDA);
  R("ar", "none", "fr-FR",   IDIOMA_AR, IDA_TMDB);
  R("ko", "th", "hr",        IDIOMA_EN, IDA_PADRAO);
  R("he", "off", "C",        IDIOMA_EN, IDA_PADRAO);
  // fonte NULL no ponteiro de saida e aceita
  assert(idiomaauto_resolver("de", NULL, NULL, NULL) == IDIOMA_DE);

  assert(!strcmp(idiomaauto_codigo(IDIOMA_PTPT), "pt-PT") && !strcmp(idiomaauto_codigo(IDIOMA_ZHTW), "zh-TW"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_TMDB), "tmdb_language"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_LEGENDA), "legenda"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_SISTEMA), "sistema"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_PADRAO), "padrao"));
  { int i;
    for (i = 0; i < IDIOMA_N; i++)
      assert(idiomaauto_mapear(idiomaauto_codigo(i)) == i);   // codigo e mapa fecham o ciclo
  }
  puts("idiomaauto: ok");
  return 0;
}
