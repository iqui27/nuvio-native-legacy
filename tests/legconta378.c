// #378: LEGENDA "DA CONTA" VIRAVA "NENHUMA" NA HORA DE TOCAR.
//
// O relato (S90C, .tpk 2.0.2): com o idioma de legenda em "Da conta" o filme
// abre sem legenda; escolhendo o idioma a mao em Ajustes, funciona.
//
// O DEFEITO MEDIDO: os idiomas da conta (subtitle_preferred_language e
// companhia) so chegavam a linguas.c dentro de ajustes_aplicar_blob. Com os
// ajustes locais protegidos (ajustes-locais.txt = 1, gravado por QUALQUER
// mudanca feita em Ajustes nesta TV — inclusive trocar o idioma da legenda e
// voltar para "Da conta"), sync.c nao aplica o blob, a conta nunca era lida, e
// "Da conta" valia "" (sem preferencia) = nada liga. O valor da conta nao
// sobrescreve escolha local (emVigor), entao alimentar linguas.c com ele mesmo
// protegido nao desfaz nada do que a pessoa escolheu aqui.
//
// Este teste usa as funcoes reais: ajustes.c (o parser do blob), linguas.c
// (ling_legenda, ling_legenda_auto_tipo) e legmemoria.c (legmem_preferencia).
// A parte do sync.c (chamar com o blob protegido) esta em tests/syncordem.c,
// sessao "legconta".
#include "../src/ajustes.c"
#include "../src/legmemoria.h"
#include <assert.h>

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %s %s\n", ok ? "ok    " : "FALHOU", o_que);
  if (!ok) falhas++;
}

static char blobBuf[512];
static const char *blob(const char *leg) {
  snprintf(blobBuf, sizeof blobBuf,
           "{\"features\":{\"player_settings\":{"
           "\"subtitle_preferred_language\":{\"type\":\"string\",\"value\":\"%s\"}}}}", leg);
  return blobBuf;
}

// A decisao do player (faixas.c, legendaAutomatica) com a lista fechada:
// indice da embutida que liga, ou LING_AUTO_NADA.
static int liga(const char *pref, const char *const *emb, int n) {
  return ling_legenda_auto_tipo(pref, "", 0, emb, NULL, n, 1, NULL, 0, 1);
}

int main(void) {
  static const char *const emb[] = { "spa", "eng", "por", "chi" };
  const char *p;
  int origem;

  setvbuf(stdout, NULL, _IOLBF, 0);
  if (getenv("NV_T_DIR")) ajustes_dir(getenv("NV_T_DIR"));   // ajustes.txt fora do repo
  ling_local_legenda("");                         // Ajustes desta TV: "Da conta"

  printf("-- ajustes protegidos: o blob nao e aplicado, so os idiomas da conta\n");
  ling_conta_legenda("");
  ajustes_idiomas_da_conta(blob("en"));
  confere("\"Da conta\" vale o idioma da conta (en)", !strcmp(ling_legenda(), "en"));
  confere("a legenda automatica liga a embutida em ingles", liga(ling_legenda(), emb, 4) == 1);
  confere("a escolha local continua ganhando da conta",
          (ling_local_legenda("es"), !strcmp(ling_legenda(), "es")));
  ling_local_legenda("");

  printf("-- variantes de codigo da conta\n");
  { static const struct { const char *conta; int faixa; } V[] = {
      { "pt-BR", 2 }, { "pt-br", 2 }, { "por", 2 }, { "pt", 2 },
      { "es-419", 0 }, { "zh-CN", 3 }, { "eng", 1 },
    };
    size_t k;
    for (k = 0; k < sizeof V / sizeof *V; k++) {
      char o[64];
      ajustes_aplicar_blob(blob(V[k].conta));
      snprintf(o, sizeof o, "conta \"%s\" liga a faixa %d", V[k].conta, V[k].faixa);
      confere(o, liga(ling_legenda(), emb, 4) == V[k].faixa);
    } }

  printf("-- conta sem idioma: cai na ultima escolha a mao, nunca \"nenhuma\" calada\n");
  ajustes_aplicar_blob(blob(""));
  p = legmem_preferencia(ling_legenda(), NULL, "en", &origem);
  confere("conta vazia -> ultima escolha a mao (en)", !strcmp(p, "en") && origem == LEGMEM_DE_ULTIMA);
  // "forced" e sentinela do app oficial ("Usar legendas forcadas"), nao idioma:
  // como codigo ele nao casa com faixa nenhuma e a ilha dizia "sem legenda em
  // FORCED". Vale como conta sem idioma.
  ajustes_aplicar_blob(blob("forced"));
  confere("conta \"forced\" nao vira idioma", !strcmp(ling_legenda(), ""));
  p = legmem_preferencia(ling_legenda(), NULL, "pt", &origem);
  confere("conta \"forced\" -> ultima escolha a mao (pt)", !strcmp(p, "pt") && origem == LEGMEM_DE_ULTIMA);
  // "none" na conta e a pessoa pedindo nenhuma — continua valendo sem escolha a mao.
  ajustes_aplicar_blob(blob("none"));
  confere("conta \"none\" sem escolha a mao: nenhuma", liga(legmem_preferencia(ling_legenda(), NULL, "", &origem), emb, 4) == LING_AUTO_NADA);

  printf("-- troca de perfil: A com en/fr/es, B sem as chaves (revisao P2)\n");
  ling_local_legenda(""); ling_local_legenda2(""); ling_local_audio("");
  ajustes_idiomas_da_conta("{\"features\":{\"player_settings\":{"
    "\"subtitle_preferred_language\":\"en\",\"subtitle_secondary_language\":\"fr\","
    "\"preferred_audio_language\":\"es\"}}}");
  confere("perfil A: legenda en, secundaria fr, audio es",
          !strcmp(ling_legenda(), "en") && !strcmp(ling_legenda2(), "fr") && !strcmp(ling_audio(), "es"));
  ajustes_idiomas_da_conta("{}");
  confere("blob do B sem as chaves: nada do A sobra",
          !ling_legenda()[0] && !ling_legenda2()[0] && !ling_audio()[0]);
  ajustes_idiomas_da_conta("{\"features\":{\"player_settings\":{"
    "\"subtitle_preferred_language\":\"en\",\"preferred_audio_language\":\"es\"}}}");
  ajustes_aplicar_blob("{\"features\":{}}");
  confere("blob APLICADO sem as chaves tambem limpa", !ling_legenda()[0] && !ling_audio()[0]);
  ajustes_idiomas_da_conta("{\"features\":{\"player_settings\":{"
    "\"subtitle_preferred_language\":\"en\",\"preferred_audio_language\":\"es\"}}}");
  ajustes_idiomas_da_conta(NULL);                  // sync_reaplicar_ajustes (troca de perfil)
  confere("troca de perfil limpa os idiomas da conta antes do blob novo",
          !ling_legenda()[0] && !ling_audio()[0]);
  ling_local_audio("de");
  ajustes_idiomas_da_conta("{}");
  confere("a escolha local de audio sobrevive a limpeza", !strcmp(ling_audio(), "de"));
  ling_local_audio("");

  printf("-- chaves parciais: so o audio, so a secundaria\n");
  ajustes_idiomas_da_conta("{\"features\":{\"player_settings\":{"
    "\"subtitle_preferred_language\":\"en\",\"subtitle_secondary_language\":\"fr\","
    "\"preferred_audio_language\":\"es\"}}}");
  ajustes_idiomas_da_conta("{\"features\":{\"player_settings\":{\"preferred_audio_language\":\"ja\"}}}");
  confere("so audio: audio ja, legendas da conta vazias",
          !strcmp(ling_audio(), "ja") && !ling_legenda()[0] && !ling_legenda2()[0]);
  ajustes_idiomas_da_conta("{\"features\":{\"player_settings\":{\"subtitle_secondary_language\":\"it\"}}}");
  confere("so secundaria: secundaria it, principal e audio vazios",
          !strcmp(ling_legenda2(), "it") && !ling_legenda()[0] && !ling_audio()[0]);

  printf("-- o log separa o que a conta diz do que vale\n");
  ling_local_legenda("es");
  ajustes_idiomas_da_conta(blob("en"));           // a linha sai daqui (grep no .sh)
  ling_local_legenda("");

  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
