// #743: "Evitar legendas SDH" — a escolha automatica nunca pega a faixa para
// surdos ([efeitos sonoros], nome do falante) quando ha alternativa; e com o
// ajuste ligado nunca a pega nem quando e a UNICA embutida.
//
// Cobre o mecanismo novo: ling_legenda_auto_tipo2 (a mesma decisao do
// ling_legenda_auto_tipo, com `evitaSdh`) e ling_nome_sdh (o nome das faixas
// e dos arquivos de addon, que nao trazem a flag).
//
// Sem o ajuste (evitaSdh = 0) o comportamento e o de sempre: SDH so se nao
// houver comum/completa no idioma.
#include "../src/linguas.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %s %s\n", ok ? "ok    " : "FALHOU", o_que);
  if (!ok) falhas++;
}

int main(void) {
  static const char *const emb[] = { "eng", "eng", "por" };
  static const int tipos[] = { LING_LEG_COMUM, LING_LEG_SDH, LING_LEG_SDH };
  static const char *const embSoSdh[] = { "eng" };
  static const int tiposSoSdh[] = { LING_LEG_SDH };
  static const char *const add[] = { "eng" };

  setvbuf(stdout, NULL, _IOLBF, 0);

  printf("-- deteccao pelo nome\n");
  confere("rotulo 'English SDH' e SDH", ling_nome_sdh("English SDH"));
  confere("arquivo '.Hearing.Impaired' e SDH", ling_nome_sdh("Movie.2024.Hearing.Impaired.srt"));
  confere("'CC' como palavra e SDH", ling_nome_sdh("Movie.2024.ENG.CC.srt"));
  confere("'accountant' NAO e SDH (cc dentro da palavra)", !ling_nome_sdh("accountant.srt"));
  confere("'access' NAO e SDH", !ling_nome_sdh("Movie.access.eng.srt"));
  confere("rotulo comum nao e SDH", !ling_nome_sdh("Portuguese (Brazil)"));

  printf("-- sem o ajuste: comportamento de sempre (SDH so sem outra)\n");
  confere("comum vence SDH no mesmo idioma",
          ling_legenda_auto_tipo2("eng", "", 0, 0, emb, tipos, 3, 1, NULL, 0, 1) == 0);
  confere("SDH embutida liga quando e a unica (padrao antigo)",
          ling_legenda_auto_tipo2("eng", "", 0, 0, embSoSdh, tiposSoSdh, 1, 1, NULL, 0, 1) == 0);

  printf("-- com o ajuste: SDH nunca liga\n");
  confere("comum continua vencendo",
          ling_legenda_auto_tipo2("eng", "", 0, 1, emb, tipos, 3, 1, NULL, 0, 1) == 0);
  confere("so SDH embutida + addon do idioma: cai no addon",
          ling_legenda_auto_tipo2("eng", "", 0, 1, embSoSdh, tiposSoSdh, 1, 1, add, 1, 1) == 1);
  confere("so SDH embutida, lista fechada, sem addon: NADA",
          ling_legenda_auto_tipo2("eng", "", 0, 1, embSoSdh, tiposSoSdh, 1, 1, NULL, 0, 1) == LING_AUTO_NADA);
  confere("por com SDH embutida e sem outra: NADA",
          ling_legenda_auto_tipo2("por", "", 0, 1, emb, tipos, 3, 1, NULL, 0, 1) == LING_AUTO_NADA);

  printf("-- o wrapper antigo nao muda\n");
  confere("ling_legenda_auto_tipo = tipo2 com evitaSdh 0",
          ling_legenda_auto_tipo("eng", "", 0, embSoSdh, tiposSoSdh, 1, 1, NULL, 0, 1) == 0);

  return falhas != 0;
}
