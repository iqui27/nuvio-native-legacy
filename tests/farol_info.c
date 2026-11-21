// Fatos da peca P6 (issue 47): tests/farol_info.sh compila so
// src/farolinfo.c. Tabela de casos por linha: cada assert confere o k,
// o modelo FT_* e os argumentos — a prova e o codigo produzido, nao grep.
//
//   bash tests/farol_info.sh                  (ASan+UBSan, o padrao)
//   SANITIZE=0 bash tests/farol_info.sh       (so os asserts)
#include "../src/farolinfo.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int casos;

static void linha(const FarolInfoLinha *l, int k, const char *m, int na) {
  assert(l->k == k);
  assert(strcmp(l->m, m) == 0);
  assert(l->na == na);
}

static void arg(const FarolInfoLinha *l, int i, const char *v) {
  if (strcmp(l->a[i], v) != 0)
    fprintf(stderr, "arg %d: '%s' != '%s' (m=%s)\n", i, l->a[i], v, l->m);
  assert(strcmp(l->a[i], v) == 0);
}

#define F(alt, dv, hdr, toca, au, leg) \
  { alt, dv, hdr, toca, au, leg }

static int qual(FarolInfoLinha *l, const FarolInfoFonte *f, int n,
                int hdr, int dv) {
  FarolInfoCaps c = { hdr, dv };
  return farolinfo_qualidade(l, 4, f, n, &c);
}

static void t_qualidade(void) {
  FarolInfoLinha l[4];
  FarolInfoFonte uma[] = {
    F(2160, 1, 0, 1, "pt", ""),
    F(2160, 0, 1, 1, "en", ""),
    F(1080, 0, 0, 1, "", ""),
    F(0, 0, 0, 1, "pt", ""),
    F(0, 0, 0, 1, "", ""),
    F(1080, 0, 0, 0, "pt", ""),
  };
  FarolInfoFonte lista[] = {
    F(720, 0, 0, 1, "en", ""),
    F(2160, 0, 0, 1, "pt", "pt"),
    F(1080, 0, 1, 1, "pt", ""),
  };
  FarolInfoFonte leg[] = { F(1080, 0, 0, 1, "pt", "pt") };
  FarolInfoFonte so8k[] = { F(4320, 0, 0, -1, "", "") };

  /* 1-3: sem fonte, sem linha */
  assert(qual(l, NULL, 0, 1, 1) == 0);            casos++;
  assert(qual(l, uma, 0, 1, 1) == 0);            casos++;
  assert(farolinfo_qualidade(l, 4, uma, 1, NULL) > 0); casos++; /* caps NULL: so nao cita formato */
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2);

  /* 4: DV citado SO quando a TV toca (dv == 1) */
  assert(qual(l, uma, 1, 1, 1) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI", 3);
  arg(&l[0], 0, "4K"); arg(&l[0], 1, "Dolby Vision"); arg(&l[0], 2, "pt");
  casos++;
  /* 5: tv.dv == 0 -> DV some da frase */
  assert(qual(l, uma, 1, 1, 0) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2);
  arg(&l[0], 0, "4K"); arg(&l[0], 1, "pt");        casos++;
  /* 6: tv.dv == -1 (desconhecido) tambem nao afirma */
  assert(qual(l, uma, 1, -1, -1) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2); casos++;
  /* 7: HDR citado so com tv.hdr == 1 — e o indicador GENERICO sai "HDR",
     nunca "HDR10" (o formato exato nao e dado que a fonte carrega) */
  assert(qual(l, uma + 1, 1, 1, 0) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI", 3);
  arg(&l[0], 0, "4K"); arg(&l[0], 1, "HDR"); arg(&l[0], 2, "en"); casos++;
  assert(qual(l, uma + 1, 1, 0, 0) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2); casos++;
  /* 8-9: sem audio declarado cai no modelo curto */
  assert(qual(l, uma + 2, 1, 1, 1) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_SO", 1);
  arg(&l[0], 0, "1080p");                        casos++;
  {
    FarolInfoFonte dvSemAudio[] = { F(2160, 1, 0, 1, "", "") };
    assert(qual(l, dvSemAudio, 1, 1, 1) == 1);
    linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_SO", 1);
    arg(&l[0], 0, "4K Dolby Vision");            casos++;
  }
  /* 10-11: so idioma / nada medido */
  assert(qual(l, uma + 3, 1, 1, 1) == 1);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_IDIOMA", 1);
  arg(&l[0], 0, "pt");                           casos++;
  assert(qual(l, uma + 4, 1, 1, 1) == 0);        casos++;
  /* 12: fonte que a TV nao toca nao entra — e a desconhecida (-1) tambem
     nao: a linha so afirma o que esta TV toca de verdade */
  assert(qual(l, uma + 5, 1, 1, 1) == 0);        casos++;
  {
    FarolInfoFonte par[] = { F(2160, 0, 0, 0, "en", ""),
                             F(1080, 0, 0, 1, "pt", "") };
    assert(qual(l, par, 2, 1, 1) == 1);
    arg(&l[0], 0, "1080p");                      casos++;
  }
  {
    /* 4K desconhecido + 1080p confirmado (audio pt nos dois): a linha e
       da 1080p — a desconhecida nao vence nem entra */
    FarolInfoFonte mista[] = { F(2160, 0, 0, -1, "pt", ""),
                               F(1080, 0, 0, 1, "pt", "") };
    assert(qual(l, mista, 2, 1, 1) == 1);
    linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2);
    arg(&l[0], 0, "1080p"); arg(&l[0], 1, "pt"); casos++;
  }
  /* 13-14: a melhor da lista e a de maior altura */
  assert(qual(l, lista, 3, 1, 1) == 2);
  linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2);
  arg(&l[0], 0, "4K"); arg(&l[0], 1, "pt");
  linha(&l[1], FI_K_QUALIDADE, "FT_INFO_LEGENDA", 1);
  arg(&l[1], 0, "pt");                           casos += 2;
  /* 15: legenda declarada vira a segunda linha — mas so ate `max` */
  assert(qual(l, leg, 1, 0, 0) == 2);
  linha(&l[1], FI_K_QUALIDADE, "FT_INFO_LEGENDA", 1); casos++;
  {
    FarolInfoCaps c = { 0, 0 };
    assert(farolinfo_qualidade(l, 1, leg, 1, &c) == 1);
    linha(&l[0], FI_K_QUALIDADE, "FT_INFO_QUALI_AUDIO", 2); casos++;
  }
  /* 16: resolucao de fonte desconhecida nao entra — nem o 8K */
  assert(qual(l, so8k, 1, 1, 1) == 0);           casos++;
}

static void t_continuar(void) {
  FarolInfoLinha l;
  FarolInfoTemp sobe[] = { {1, 80, 8}, {2, 86, 7} };
  FarolInfoTemp cai[] = { {1, 82, 6}, {2, 74, 9} };
  FarolInfoTemp nivel[] = { {1, 80, 6}, {2, 82, 6} };
  FarolInfoTemp pouca[] = { {1, 80, 8}, {2, 90, 4} };
  FarolInfoTemp exato[] = { {1, 80, 5}, {2, 84, 5} };
  FarolInfoTemp espec[] = { {0, 30, 10}, {1, 80, 6}, {2, 84, 6} };
  FarolInfoTemp foraOrd[] = { {2, 86, 7}, {1, 80, 8} };
  FarolInfoTemp tres[] = { {1, 80, 8}, {2, 86, 7}, {3, 70, 9} };
  FarolInfoTemp buraco[] = { {1, 80, 8}, {3, 50, 8} };

  /* 17-19: sem temporada comparavel, sem linha */
  assert(farolinfo_continuar(&l, NULL, 0, 6, 45) == 0);   casos++;
  assert(farolinfo_continuar(&l, sobe, 0, 6, 45) == 0);   casos++;
  assert(farolinfo_continuar(&l, buraco, 2, 0, 0) == 0);  casos++;

  /* 20: sobe >= 0,4 com faltam -> uma linha so com os tres dados */
  assert(farolinfo_continuar(&l, sobe, 2, 6, 45) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_SOBE", 3);
  arg(&l, 0, "2"); arg(&l, 1, "6"); arg(&l, 2, "45");    casos++;
  /* 21: queda -> fato NOVO que so afirma a nota (nunca FT_INFO_CAI: ela
     diz "muita gente para aqui", abandono que o modulo nao mede) */
  assert(farolinfo_continuar(&l, cai, 2, 6, 45) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_NOTA_CAI", 1);
  arg(&l, 0, "2");                                       casos++;
  /* 22: mesmo nivel + faltam -> faltam primeiro */
  assert(farolinfo_continuar(&l, nivel, 2, 6, 45) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_FALTAM", 2);
  arg(&l, 0, "6"); arg(&l, 1, "45");                     casos++;
  /* 23: mesmo nivel sem faltam -> "seguem no mesmo nivel" */
  assert(farolinfo_continuar(&l, nivel, 2, 0, 0) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_MESMO_NIVEL", 0);   casos++;
  /* 24: menos de 5 com nota nao sustenta comparacao — em NENHUM dos lados */
  assert(farolinfo_continuar(&l, pouca, 2, 0, 0) == 0);  casos++;
  {
    FarolInfoTemp pouca1[] = { {1, 80, 4}, {2, 90, 5} };
    assert(farolinfo_continuar(&l, pouca1, 2, 0, 0) == 0); casos++;
  }
  /* 25: diferenca exata de 0,4 conta */
  assert(farolinfo_continuar(&l, exato, 2, 3, 40) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_SOBE", 3);          casos++;
  /* 26: especiais (T0) nao entram na comparacao */
  assert(farolinfo_continuar(&l, espec, 3, 0, 0) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_SOBE_SO", 1);
  arg(&l, 0, "2");                                       casos++;
  /* 27: entrada fora de ordem continua certa */
  assert(farolinfo_continuar(&l, foraOrd, 2, 6, 45) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_SOBE", 3);          casos++;
  /* 28: vale o par mais recente */
  assert(farolinfo_continuar(&l, tres, 3, 6, 45) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_NOTA_CAI", 1);
  arg(&l, 0, "3");                                       casos++;
  /* 29: sobe sem faltam -> forma curta */
  assert(farolinfo_continuar(&l, sobe, 2, 0, 0) == 1);
  linha(&l, FI_K_CONTINUAR, "FT_INFO_SOBE_SO", 1);       casos++;
  /* o limiar |0,4| vale nos dois sentidos: +-0,3 nao e subida nem queda */
  {
    FarolInfoTemp sobe03[] = { {1, 80, 6}, {2, 83, 6} };
    FarolInfoTemp cai04[]  = { {1, 80, 5}, {2, 76, 5} };
    FarolInfoTemp cai03[]  = { {1, 80, 6}, {2, 77, 6} };
    FarolInfoTemp queda76[] = { {1, 80, 5}, {2, 76, 5} };
    assert(farolinfo_continuar(&l, sobe03, 2, 0, 0) == 1);
    linha(&l, FI_K_CONTINUAR, "FT_INFO_MESMO_NIVEL", 0);   casos++;
    /* 8,0 -> 7,6 exato: cai — e sai NOTA_CAI, nao a chave de abandono */
    assert(farolinfo_continuar(&l, cai04, 2, 0, 0) == 1);
    linha(&l, FI_K_CONTINUAR, "FT_INFO_NOTA_CAI", 1);
    arg(&l, 0, "2");                                       casos++;
    assert(strcmp(l.m, "FT_INFO_CAI") != 0);               casos++;
    assert(farolinfo_continuar(&l, cai03, 2, 0, 0) == 1);
    linha(&l, FI_K_CONTINUAR, "FT_INFO_MESMO_NIVEL", 0);   casos++;
    assert(farolinfo_continuar(&l, queda76, 2, 6, 45) == 1);
    linha(&l, FI_K_CONTINUAR, "FT_INFO_NOTA_CAI", 1);      casos++;
  }
  /* numero == INT_MAX nao estoura o teste de consecutivo */
  {
    FarolInfoTemp ultimas[] = { {INT_MAX, 80, 5}, {INT_MAX, 84, 5} };
    assert(farolinfo_continuar(&l, ultimas, 2, 0, 0) == 0); casos++;
  }
}

static void t_tempo(void) {
  FarolInfoLinha l;
  struct tm a = { 0 };

  /* 30-31: sem duracao ou sem relogio, sem linha — duracao negativa
     tambem nao vira linha */
  a.tm_hour = 21;
  assert(farolinfo_tempo(&l, &a, 0, 0, 0) == 0);          casos++;
  assert(farolinfo_tempo(&l, NULL, 0, 90, 0) == 0);       casos++;
  assert(farolinfo_tempo(&l, &a, 0, -30, 0) == 0);        casos++;
  assert(farolinfo_tempo(&l, &a, 0, INT_MIN, 0) == 0);    casos++;
  /* 32: termina hoje, 24 h */
  assert(farolinfo_tempo(&l, &a, 0, 90, 0) == 1);
  linha(&l, FI_K_TEMPO, "FT_INFO_TERMINA", 1);
  arg(&l, 0, "22:30");                                   casos++;
  /* 33: a virada do dia */
  a.tm_hour = 23; a.tm_min = 40;
  assert(farolinfo_tempo(&l, &a, 0, 45, 0) == 1);
  arg(&l, 0, "00:25");                                   casos++;
  /* 34: 12 h com sufixo */
  a.tm_hour = 21; a.tm_min = 0;
  assert(farolinfo_tempo(&l, &a, 1, 90, 0) == 1);
  arg(&l, 0, "10:30 PM");                                casos++;
  a.tm_hour = 23; a.tm_min = 40;
  assert(farolinfo_tempo(&l, &a, 1, 45, 0) == 1);
  arg(&l, 0, "12:25 AM");                                casos++;
  /* 35: meio-dia e meia-noite no formato 12 h */
  a.tm_hour = 11; a.tm_min = 59;
  assert(farolinfo_tempo(&l, &a, 1, 1, 0) == 1);
  arg(&l, 0, "12:00 PM");                                casos++;
  a.tm_hour = 12; a.tm_min = 59;
  assert(farolinfo_tempo(&l, &a, 1, 61, 0) == 1);
  arg(&l, 0, "2:00 PM");                                 casos++;
  /* 36: segundos contam (fim 00:00:50 -> "00:00") */
  a.tm_hour = 23; a.tm_min = 59; a.tm_sec = 50;
  assert(farolinfo_tempo(&l, &a, 0, 1, 0) == 1);
  arg(&l, 0, "00:00");                                   casos++;
  a.tm_sec = 0;
  /* 37: nao cabe na noite -> "da em N noites" */
  assert(farolinfo_tempo(&l, &a, 0, 300, 140) == 1);
  linha(&l, FI_K_TEMPO, "FT_INFO_DUAS_NOITES", 2);
  arg(&l, 0, "3"); arg(&l, 1, "140");                    casos++;
  assert(farolinfo_tempo(&l, &a, 0, 200, 100) == 1);
  arg(&l, 0, "2");                                       casos++;
  /* 38: exatamente uma noite ainda e "termina as" */
  assert(farolinfo_tempo(&l, &a, 0, 140, 140) == 1);
  linha(&l, FI_K_TEMPO, "FT_INFO_TERMINA", 1);           casos++;
  /* sem estouro de int nem de long: INT_MAX minutos satura a conta de
     noites e o relogio de parede mod 24 h (no WASM `long` tem 32 bits) */
  a.tm_hour = 23; a.tm_min = 59; a.tm_sec = 0;
  assert(farolinfo_tempo(&l, &a, 0, INT_MAX, 50) == 1);
  linha(&l, FI_K_TEMPO, "FT_INFO_DUAS_NOITES", 2);
  arg(&l, 0, "42949673"); arg(&l, 1, "50");              casos++;
  assert(farolinfo_tempo(&l, &a, 0, INT_MAX, 0) == 1);
  linha(&l, FI_K_TEMPO, "FT_INFO_TERMINA", 1);
  arg(&l, 0, "02:06");                                   casos++;
}

static void t_onde(void) {
  FarolInfoLinha l;
  /* 39: servico, vazio e NULL */
  assert(farolinfo_onde(&l, "Netflix") == 1);
  linha(&l, FI_K_ONDE, "FT_INFO_ONDE", 1);
  arg(&l, 0, "Netflix");                                 casos++;
  assert(farolinfo_onde(&l, "") == 0);                    casos++;
  assert(farolinfo_onde(&l, NULL) == 0);                  casos++;
  /* o argumento nao corta caractere UTF-8 no meio: servico de 64 B
     terminado em "é" sai sem o caractere incompleto */
  {
    char servico[65], esperado[64];
    memset(servico, 'a', 62);
    memcpy(servico + 62, "\xc3\xa9", 2);
    servico[64] = '\0';
    memset(esperado, 'a', 62);
    esperado[62] = '\0';
    assert(farolinfo_onde(&l, servico) == 1);
    arg(&l, 0, esperado);                                casos++;
  }
}

static void t_amigos(void) {
  FarolInfoLinha l;
  FarolInfoAmigo tresT[] = {
    { "Ana", 1, 0 }, { "Rafa", 1, 0 }, { "Bia", 1, 0 } };
  FarolInfoAmigo umT[] = { { "Ana", 1, 0 } };
  FarolInfoAmigo doisG[] = { { "Ana", 0, 1 }, { "Rafa", 0, 1 } };
  FarolInfoAmigo umG[] = { { "Ana", 0, 1 } };
  FarolInfoAmigo misto[] = { { "Ana", 0, 1 }, { "Rafa", 1, 0 } };
  FarolInfoAmigo vazio[] = { { "", 0, 0 } };

  /* 40-44 */
  assert(farolinfo_amigos(&l, tresT, 3) == 1);
  linha(&l, FI_K_AMIGOS, "FT_INFO_AMIGOS_TERM", 2);
  arg(&l, 0, "Ana"); arg(&l, 1, "Rafa");                 casos++;
  assert(farolinfo_amigos(&l, umT, 1) == 1);
  linha(&l, FI_K_AMIGOS, "FT_INFO_AMIGO_TERM", 1);       casos++;
  assert(farolinfo_amigos(&l, doisG, 2) == 1);
  linha(&l, FI_K_AMIGOS, "FT_INFO_AMIGOS_GOST", 2);      casos++;
  assert(farolinfo_amigos(&l, umG, 1) == 1);
  linha(&l, FI_K_AMIGOS, "FT_INFO_AMIGO_GOST", 1);       casos++;
  assert(farolinfo_amigos(&l, misto, 2) == 1);
  linha(&l, FI_K_AMIGOS, "FT_INFO_AMIGO_TERM", 1);       casos++;
  assert(farolinfo_amigos(&l, vazio, 1) == 0);            casos++;
  assert(farolinfo_amigos(&l, NULL, 0) == 0);             casos++;
}

static void t_saga(void) {
  FarolInfoLinha l;
  /* 45-47 */
  assert(farolinfo_saga(&l, 2, 3, 1) == 1);
  linha(&l, FI_K_SAGA, "FT_INFO_SAGA_VOCE_VIU", 3);
  arg(&l, 0, "2"); arg(&l, 1, "3"); arg(&l, 2, "1");      casos++;
  assert(farolinfo_saga(&l, 2, 3, 0) == 1);
  linha(&l, FI_K_SAGA, "FT_INFO_SAGA", 2);                casos++;
  assert(farolinfo_saga(&l, 1, 1, 0) == 0);               casos++;
  assert(farolinfo_saga(&l, 0, 3, 0) == 0);               casos++;
  assert(farolinfo_saga(&l, 9, 3, 0) == 1);   /* fora do total: grampo */
  arg(&l, 0, "3");                                       casos++;
}

static void t_rever(void) {
  FarolInfoLinha l;
  /* 48-51 */
  assert(farolinfo_rever(&l, 2024, 3, 8) == 1);
  linha(&l, FI_K_REVER, "FT_INFO_REVER", 2);
  arg(&l, 0, "2024-03"); arg(&l, 1, "4");                 casos++;
  assert(farolinfo_rever(&l, 2024, 3, 9) == 1);  /* meia estrela -> simples */
  linha(&l, FI_K_REVER, "FT_INFO_REVER_SIMPLES", 1);      casos++;
  assert(farolinfo_rever(&l, 2024, 3, 0) == 1);
  linha(&l, FI_K_REVER, "FT_INFO_REVER_SIMPLES", 1);      casos++;
  assert(farolinfo_rever(&l, 2024, 0, 8) == 0);           casos++;
  assert(farolinfo_rever(&l, 0, 3, 8) == 0);              casos++;
}

static void t_retomada(void) {
  FarolInfoLinha l;
  long long agora = 1800000000LL;

  /* 52-57 */
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 49LL * 86400) == 1);
  linha(&l, FI_K_RETOMADA, "FT_INFO_RETOMADA", 3);
  arg(&l, 0, "2"); arg(&l, 1, "4"); arg(&l, 2, "7w");     casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, 0) == 1);
  linha(&l, FI_K_RETOMADA, "FT_INFO_RETOMADA_SIMPLES", 2); casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora) == 1);
  linha(&l, FI_K_RETOMADA, "FT_INFO_RETOMADA_SIMPLES", 2); casos++;
  assert(farolinfo_retomada(&l, 0, 4, agora, agora - 100) == 0); casos++;
  assert(farolinfo_retomada(&l, 2, 0, agora, agora - 100) == 0); casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 2700) == 1);
  arg(&l, 2, "45min");                                   casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 30) == 1);
  arg(&l, 2, "1min");                                    casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 5LL * 3600) == 1);
  arg(&l, 2, "5h");                                      casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 3LL * 86400) == 1);
  arg(&l, 2, "3d");                                      casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 150LL * 86400) == 1);
  arg(&l, 2, "5M");                                      casos++;
  assert(farolinfo_retomada(&l, 2, 4, agora, agora - 800LL * 86400) == 1);
  arg(&l, 2, "2y");                                      casos++;
}

int main(void) {
  FarolInfoLinha l;

  /* os nomes de k estaveis, na ordem da tabela 4.2 */
  assert(strcmp(farolinfo_k_nome(FI_K_QUALIDADE), "qualidade") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_CONTINUAR), "continuar") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_TEMPO), "tempo") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_ONDE), "onde") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_AMIGOS), "amigos") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_SAGA), "saga") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_REVER), "rever") == 0);
  assert(strcmp(farolinfo_k_nome(FI_K_RETOMADA), "retomada") == 0);
  assert(farolinfo_k_nome(-1)[0] == 0 && farolinfo_k_nome(FI_K_N)[0] == 0);
  casos++;
  /* linha zerada nao engana: m vazio ate ser preenchida */
  memset(&l, 0, sizeof l);
  assert(l.na == 0 && l.m[0] == 0);
  casos++;

  t_qualidade();
  t_continuar();
  t_tempo();
  t_onde();
  t_amigos();
  t_saga();
  t_rever();
  t_retomada();

  printf("farol_info: %d casos ok\n", casos);
  return 0;
}
