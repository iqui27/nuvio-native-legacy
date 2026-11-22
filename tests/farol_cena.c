// Compositor deterministico do FarolCena (lote sense, issue 44;
// PLANO-GOSTO 4.2, 4.3 e 5.2). Sem SDL nem rede: linka
// src/farolcena.c + src/farolrec.c + src/farolgosto.c + src/farolinfo.c
// + src/farolpeca.c + src/faroltexto.c + src/js.c — toda cena gerada
// passa pelo validador da 39 (inclusive em perfis aleatorios), com
// nRecusadas == 0. Cobre: frio 0/1/2, ate 2 fileiras + 1 dica, motivo
// so quando e verdadeiro, titulo pelo motivo dominante, detalhe
// (porque + info), veredito da comparacao e o determinismo byte a byte
// (mesma entrada + mesma semente = mesma cena). O segundo ciclo (r1)
// soma: mesas para motivo sem dado real (MN1..MN5), bordas de envelope
// e cap (MN6/MN7), e a conferencia linha a linha da 3.5 em toda cena
// do fuzz — nao so esquema e determinismo.
//
//   bash tests/farol_cena.sh                  (ASan+UBSan, o padrao)
//   SANITIZE=0 bash tests/farol_cena.sh       (so os asserts)
//   FC_DUMP=1 ./farol-cena-test               (imprime as cenas da mesa)
#include "../src/farolcena.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIA 400          // "hoje" do teste, em dias desde 1/1/2026
#define FH  2            // faixa de hora "agora" (0..7)

// ---------------------------------------------------------------- helpers

static FarolFicha mkF(int32_t tmdb, int tipo, int ano, int min, int nota,
                      int votos, int32_t g0, int32_t g1, int32_t p0,
                      int32_t t0) {
  FarolFicha f;
  memset(&f, 0, sizeof f);
  f.tmdb = tmdb;
  f.tipo = (uint8_t)tipo;
  f.ano = (uint16_t)ano;
  f.minutos = (uint16_t)min;
  f.nota = (uint8_t)nota;
  f.votos = (uint16_t)votos;
  f.idioma = ('p' << 8) | 't';
  f.genero[0] = g0;
  f.genero[1] = g1;
  f.pessoa[0] = p0;
  f.tema[0] = t0;
  f.nGenero = (uint8_t)(g1 ? 2 : (g0 ? 1 : 0));
  f.nPessoa = (uint8_t)(p0 ? 1 : 0);
  f.nTema = (uint8_t)(t0 ? 1 : 0);
  f.fonte = FF_DE_TMDB;
  return f;
}

static FarolGostoFicha toGF(const FarolFicha *f) {
  FarolGostoFicha g;
  int i;
  memset(&g, 0, sizeof g);
  g.tmdb = f->tmdb;
  g.tipo = (uint8_t)(f->tipo == FF_SERIE ? FG_TIPO_SERIE : FG_TIPO_FILME);
  g.ano = (int16_t)f->ano;
  g.idioma = f->idioma;
  g.minutos = (int16_t)f->minutos;
  for (i = 0; i < 4; i++) g.genero[i] = f->genero[i];
  for (i = 0; i < 6; i++) g.pessoa[i] = f->pessoa[i];
  for (i = 0; i < 8; i++) g.tema[i] = f->tema[i];
  return g;
}

static void ve(FarolGosto *g, const FarolFicha *f, int delta, uint16_t dia,
               int nova) {
  FarolGostoFicha gf = toGF(f);
  farolgosto_aplicar(g, &gf, delta, dia, 0, nova);
}

// Aplica o delta marcando a faixa de hora FH (a do "agora" do teste).
static void veH(FarolGosto *g, const FarolFicha *f, int delta, uint16_t dia) {
  FarolGostoFicha gf = toGF(f);
  farolgosto_aplicar(g, &gf, delta, dia, FH, 1);
}

static FarolRecCand mkC(const FarolFicha *f, int64_t chave, uint8_t fontes) {
  FarolRecCand c;
  memset(&c, 0, sizeof c);
  c.ficha = f;
  c.chave = chave;
  c.fontes = fontes;
  c.semente = FRC_SEM_SEMENTE;
  return c;
}

static FarolCenaCtx mkCC(const FarolGosto *g, const FarolRecCand *cand,
                         int nCand, const FarolRecSemente *sem, int nSem,
                         const char *const *semRef) {
  FarolCenaCtx x;
  memset(&x, 0, sizeof x);
  x.g = g;
  x.cand = cand;
  x.nCand = nCand;
  x.sementes = sem;
  x.nSementes = nSem;
  x.sementeRef = semRef;
  x.dia = DIA;
  x.faixaHora = FH;
  x.perfil = 2;
  x.feita = 1795000000;
  x.valeS = 86400;
  x.semente = 7;
  return x;
}

// A FarolCena do validador e 25,9 KB: estatica aqui, como a propria
// revisao da 39 pede (nunca na pilha do fio da UI).
static FarolCena C;
static int nValid = 0, nMesas = 0, dump = 0;

static void valida(const char *tag, const char *json, int nCand) {
  int rc = farolpeca_ler(&C, json, nCand);
  if (rc < 0 || C.nRecusadas != 0) {
    fprintf(stderr, "farol_cena: %s recusada rc=%d rec=%d\n%s\n",
            tag, rc, C.nRecusadas, json);
    exit(1);
  }
  nValid++;
}

static void espera(const char *tag, const char *got, const char *exp,
                   int nCand) {
  if (dump) {
    printf("=== %s\n%s\n", tag, got);
    valida(tag, got, nCand);
    nMesas++;
    return;
  }
  if (strcmp(got, exp)) {
    fprintf(stderr, "farol_cena: %s diverge\n got: %s\n exp: %s\n",
            tag, got, exp);
    exit(1);
  }
  valida(tag, got, nCand);
  nMesas++;
}

// Determinismo: duas chamadas com a mesma entrada sao byte a byte iguais.
static void duas_vezes(const char *tag, const char *a, const char *b) {
  if (strcmp(a, b)) {
    fprintf(stderr, "farol_cena: %s nao deterministico\n 1: %s\n 2: %s\n",
            tag, a, b);
    exit(1);
  }
}

// Modelo com `n` titulos aplicando `delta`; as fichas ficam em `sem`.
static void modelo2(FarolGosto *g, FarolFicha *sem, int n, int32_t g0,
                    int32_t g1, int32_t p0, int32_t t0, int delta);
static void modelo(FarolGosto *g, FarolFicha *sem, int n, int32_t g0,
                   int32_t p0, int32_t t0, int delta) {
  modelo2(g, sem, n, g0, 0, p0, t0, delta);
}
static void modelo2(FarolGosto *g, FarolFicha *sem, int n, int32_t g0,
                    int32_t g1, int32_t p0, int32_t t0, int delta) {
  int i;
  farolgosto_iniciar(g, 1);
  for (i = 0; i < n; i++) {
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + (i % 20), 100 + (i % 4) * 10,
                 75, 5000, g0, g1, p0, t0);
    ve(g, &sem[i], delta, (uint16_t)(DIA - 30 + (i % 20)), 1);
  }
}

static void dica(FarolDicaFato *d, int tipo, int acao, const char *id,
                 const char *alvo, int n1, const char *a1, const char *a2,
                 const char *a3, int n2, const char *b1) {
  memset(d, 0, sizeof *d);
  d->tipo = tipo;
  d->acao = acao;
  d->id = id;
  d->alvo = alvo;
  d->l1na = n1;
  d->l1a[0] = a1;
  d->l1a[1] = a2;
  d->l1a[2] = a3;
  d->l2na = n2;
  d->l2a[0] = b1;
}

// Buffer de cena da mesa (8 KB): estatico — a mesma regra do validador
// vale para a cena serializada e para as duas copias do determinismo.
static char B1[FAROL_CENA_MAX + 1], B2[FAROL_CENA_MAX + 1];

// ------------------------------------------------------------- M1..M12

// M1: frio 0 — sem fileira; a dica PERGUNTAS e a porta (3.4).
static void m1(void) {
  FarolGosto g;
  FarolFicha sem[2];
  FarolCenaCtx x;
  FarolDicaFato d[1];
  modelo(&g, sem, 2, 18, 0, 0, FG_D_GOSTOU);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  dica(d, FC_DICA_PERGUNTAS, FP_ACAO_ESCOLHA, "d_perg", NULL, 0, 0, 0, 0,
       0, 0);
  farolcena_home(B1, sizeof B1, &x, d, 1);
  farolcena_home(B2, sizeof B2, &x, d, 1);
  duas_vezes("m1", B1, B2);
  espera("m1", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"dica\",\"id\":\"d_perg\",\"l1\":{\"m\":\"FT_DICA_PERGUNTAS\"},\"acao\":{\"tipo\":\"escolha\"},\"icone\":\"aj_farol\"}]}", 0);
}

// M2: frio 0 — PAROU nao sobe no frio 0 (3.4: so a porta); TRAKT sim.
static void m2(void) {
  FarolGosto g;
  FarolFicha sem[1];
  FarolCenaCtx x;
  FarolDicaFato d[2];
  modelo(&g, sem, 1, 18, 0, 0, FG_D_GOSTOU);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  dica(&d[0], FC_DICA_PAROU, FP_ACAO_ABRIR, "d_parou", "tt0903747",
       3, "tt0903747", "2", "4", 1, "5");
  dica(&d[1], FC_DICA_TRAKT, FP_ACAO_NADA, "d_trakt", NULL, 0, 0, 0, 0,
       0, 0);
  farolcena_home(B1, sizeof B1, &x, d, 2);
  farolcena_home(B2, sizeof B2, &x, d, 2);
  duas_vezes("m2", B1, B2);
  espera("m2", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"dica\",\"id\":\"d_trakt\",\"l1\":{\"m\":\"FT_DICA_TRAKT\"},\"acao\":{\"tipo\":\"nada\"},\"icone\":\"aj_farol\"}]}", 0);
}

// M3: frio 1 — uma fileira so, titulo fixo "Para comecar", candidatos
// so das recs das sementes; motivo TERMINOU (a semente e' nomeavel).
static void m3(void) {
  FarolGosto g;
  FarolFicha sem[5], cf[8];
  FarolRecCand c[8];
  FarolRecSemente sm[8];
  const char *sr[8] = { "tt1000", "tt1001", "tt1002", "tt1003",
                        "tt1004", "tt1000", "tt1001", "tt1002" };
  FarolCenaCtx x;
  int i;
  static const int32_t gg[8] = { 18, 35, 28, 50, 80, 90, 99, 12 };
  modelo2(&g, sem, 5, 18, 35, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(2000 + i, FF_FILME, 2015 + i, 90 + i * 5, 80, 9000,
                gg[i], 60 + i, 8000 + i, 0);
    c[i] = mkC(&cf[i], 2000 + i, FRC_F_SEMENTE);
    c[i].semente = (uint16_t)i;
    c[i].flags = FRC_FL_EM_REC;
  }
  memset(sm, 0, sizeof sm);
  for (i = 0; i < 8; i++) {
    sm[i].ficha = &sem[i % 5];
    sm[i].terminou = 1;
  }
  x = mkCC(&g, c, 8, sm, 8, sr);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m3", B1, B2);
  espera("m3", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_COMECAR\"},\"forma\":\"cartaz\",\"itens\":[\"c1\",\"c0\",\"c4\",\"c7\",\"c6\",\"c2\",\"c5\",\"c3\"],\"motivo\":{\"c1\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1001\"]},\"c0\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1000\"]},\"c4\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1004\"]},\"c7\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1002\"]},\"c6\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1001\"]},\"c2\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1002\"]},\"c5\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1000\"]},\"c3\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1003\"]}}}]}", 8);
}

// M4: nivel 2, dominante TERMINOU — titulo "Porque voce terminou <ref>".
static void m4(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[8];
  FarolRecCand c[8];
  FarolRecSemente sm[4];
  const char *sr[4] = { "tt1000", "tt1001", "tt1002", "tt1003" };
  FarolCenaCtx x;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(3000 + i, FF_FILME, 2015 + i, 90 + i * 5, 80, 9000,
                i % 2 ? 50 : 99, 0, 0, 0);
    c[i] = mkC(&cf[i], 3000 + i, FRC_F_SEMENTE);
    c[i].semente = (uint16_t)(i / 2);
    c[i].flags = FRC_FL_EM_REC;
  }
  memset(sm, 0, sizeof sm);
  for (i = 0; i < 4; i++) {
    sm[i].ficha = &sem[i];
    sm[i].terminou = 1;
  }
  x = mkCC(&g, c, 8, sm, 4, sr);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m4", B1, B2);
  espera("m4", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PORQUE_TERMINOU\",\"a\":[\"tt1000\"]},\"forma\":\"cartaz\",\"itens\":[\"c1\",\"c2\",\"c4\",\"c7\",\"c3\",\"c0\",\"c5\",\"c6\"],\"motivo\":{\"c1\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1000\"]},\"c2\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1001\"]},\"c4\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1002\"]},\"c7\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1003\"]},\"c3\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g50\"]},\"c0\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1000\"]},\"c5\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1002\"]},\"c6\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt1003\"]}}}]}", 8);
}

// M5: nivel 2, dominante PESSOA — titulo "Com <pessoa>, de quem voce
// gosta"; a linha sai "p<id>" e a afinidade sustenta (n>=2, peso>0,5).
static void m5(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[8];
  FarolRecCand c[8];
  FarolCenaCtx x;
  int i;
  modelo(&g, sem, 12, 18, 137427, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(4000 + i, FF_FILME, 2015 + i, 90 + i * 5, 80, 9000,
                i % 2 ? 50 : 99, 0, 137427, 0);
    c[i] = mkC(&cf[i], 4000 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 8, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m5", B1, B2);
  espera("m5", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PESSOA\",\"a\":[\"p137427\"]},\"forma\":\"cartaz\",\"itens\":[\"c2\",\"c0\",\"c3\",\"c1\",\"c4\",\"c7\",\"c6\",\"c5\"],\"motivo\":{\"c2\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c0\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c3\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c1\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c4\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g99\"]},\"c7\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c6\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c5\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]}}}]}", 8);
}

// M6: fileira 2 — maior afinidade de tema na segunda pagina (5.2).
static void m6(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[20];
  FarolRecCand c[20];
  FarolCenaCtx x;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    // 4 fichas de cada: pessoa 137427, tema 4565 e genero 18 — as tres
    // afinidades sustentam explicacao.
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, i < 4 ? 137427 : 0, i < 8 ? 4565 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 20; i++) {
    // 12 fortes de pessoa; 8 do grupo de tema, nota menor — o grupo
    // fica para a segunda pagina e vira a fileira 2 inteira.
    int tema = i >= 12 ? 4565 : 0;
    cf[i] = mkF(6000 + i, FF_FILME, 2010 + i, 90 + i * 5,
                i >= 12 ? 55 : 92, 9000,
                i % 2 ? 35 : 28, 0, i < 12 ? 137427 : 0, tema);
    c[i] = mkC(&cf[i], 6000 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 20, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m6", B1, B2);
  espera("m6", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PESSOA\",\"a\":[\"p137427\"]},\"forma\":\"cartaz\",\"itens\":[\"c7\",\"c18\",\"c4\",\"c9\",\"c3\",\"c8\",\"c6\",\"c5\",\"c1\",\"c0\",\"c2\",\"c19\"],\"motivo\":{\"c7\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c18\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]},\"c4\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c9\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c3\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g35\"]},\"c8\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c6\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c5\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c1\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c0\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c2\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c19\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]}}},{\"t\":\"fileira\",\"id\":\"gosto2\",\"titulo\":{\"m\":\"FT_FIL_TEMA\",\"a\":[\"k4565\"]},\"forma\":\"cartaz\",\"itens\":[\"c14\",\"c12\",\"c15\",\"c16\",\"c17\",\"c13\"],\"motivo\":{\"c14\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]},\"c12\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]},\"c15\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]},\"c16\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]},\"c17\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]},\"c13\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k4565\"]}}}]}", 20);
}

// M7: dominante CABE_HOJE — titulo de dia de semana e o de fim de
// semana na mesma entrada, so mudando fimSemana.
static void m7(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[8];
  FarolRecCand c[8];
  FarolCenaCtx x;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(7000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i % 3 == 2 ? 28 : (i % 2 ? 50 : 99), 0, 0, 0);
    c[i] = mkC(&cf[i], 7000 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 8, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m7a", B1, B2);
  espera("m7a", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_HORA\"},\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c2\",\"c1\",\"c4\",\"c3\",\"c7\",\"c6\",\"c5\"],\"motivo\":{\"c0\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c2\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c1\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c4\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c3\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g50\"]},\"c7\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c6\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c5\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]}}}]}", 8);
  x.fimSemana = 1;
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m7b", B1, B2);
  espera("m7b", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_FIMSEMANA\"},\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c2\",\"c1\",\"c4\",\"c3\",\"c7\",\"c6\",\"c5\"],\"motivo\":{\"c0\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c2\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c1\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c4\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c3\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g50\"]},\"c7\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c6\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c5\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]}}}]}", 8);
}

// M8: dominante AMIGO — "O que seus amigos gostaram" e "u<quem>" com a
// concordancia em dezenas.
static void m8(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[8];
  FarolRecCand c[8];
  FarolCenaCtx x;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(8000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i % 2 ? 50 : 99, 0, 0, 0);
    c[i] = mkC(&cf[i], 8000 + i, FRC_F_AMIGO);
    c[i].quem = 42;
    c[i].gostoPct = 80;
  }
  x = mkCC(&g, c, 8, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m8", B1, B2);
  espera("m8", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_AMIGOS\"},\"forma\":\"cartaz\",\"itens\":[\"c1\",\"c4\",\"c2\",\"c0\",\"c3\",\"c5\",\"c6\",\"c7\"],\"motivo\":{\"c1\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]},\"c4\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]},\"c2\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]},\"c0\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]},\"c3\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g50\"]},\"c5\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]},\"c6\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]},\"c7\":{\"m\":\"FT_MOT_AMIGO\",\"a\":[\"u42\",\"8\"]}}}]}", 8);
}

// M9: menos de 6 elegiveis — sem fileira ("nao aparece", 4.2); a dica
// fica de pe.
static void m9(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[8];
  FarolRecCand c[8];
  FarolCenaCtx x;
  FarolDicaFato d[1];
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(9000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i % 2 ? 35 : 18, 0, 0, 0);
    c[i] = mkC(&cf[i], 9000 + i, FRC_F_CATALOGO);
    if (i >= 4) c[i].flags = FRC_FL_VISTO;
  }
  x = mkCC(&g, c, 8, NULL, 0, NULL);
  dica(d, FC_DICA_SALVO, FP_ACAO_ABRIR, "d_salvo", "tt9010", 1, "5M",
       0, 0, 1, "1h32");
  farolcena_home(B1, sizeof B1, &x, d, 1);
  farolcena_home(B2, sizeof B2, &x, d, 1);
  duas_vezes("m9", B1, B2);
  espera("m9", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"dica\",\"id\":\"d_salvo\",\"l1\":{\"m\":\"FT_DICA_SALVO_L1\",\"a\":[\"5M\"]},\"l2\":{\"m\":\"FT_DICA_SALVO_L2\",\"a\":[\"1h32\"]},\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt9010\"},\"icone\":\"aj_farol\"}]}", 8);
}

// M10: dominante SALVO — "Dos seus salvos" e "Esta nos seus salvos ha
// 5 meses." (o dia entrou no contexto).
static void m10(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[8];
  FarolRecCand c[8];
  FarolCenaCtx x;
  uint16_t sd[8];
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(9500 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i % 2 ? 50 : 99, 0, 0, 0);
    c[i] = mkC(&cf[i], 9500 + i, FRC_F_SALVO);
    sd[i] = (uint16_t)(DIA - 150);
  }
  x = mkCC(&g, c, 8, NULL, 0, NULL);
  x.salvoDia = sd;
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m10", B1, B2);
  espera("m10", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_SALVOS\"},\"forma\":\"cartaz\",\"itens\":[\"c4\",\"c3\",\"c2\",\"c0\",\"c1\",\"c5\",\"c6\",\"c7\"],\"motivo\":{\"c4\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]},\"c3\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]},\"c2\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]},\"c0\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]},\"c1\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g50\"]},\"c5\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]},\"c6\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]},\"c7\":{\"m\":\"FT_MOT_SALVO\",\"a\":[\"5M\"]}}}]}", 8);
}

// M11: a vaga fora da curva sai com a exploracao dita na cara.
static void m11(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[9];
  FarolRecCand c[9];
  FarolCenaCtx x;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 9; i++) {
    cf[i] = mkF(9800 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i < 3 ? 18 : (i < 8 ? 35 + i : 50), 0, 0, 0);
    c[i] = mkC(&cf[i], 9800 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 9, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("m11", B1, B2);
  espera("m11", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_HORA\"},\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c2\",\"c1\",\"c3\",\"c4\",\"c5\",\"c8\",\"c6\",\"c7\"],\"motivo\":{\"c0\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c2\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c1\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c3\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c4\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g39\"]},\"c5\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c8\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c6\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]},\"c7\":{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]}}}]}", 9);
}

// M12: TERMINOU_TEMA (a afinidade dividida sustenta o "e gosta de") +
// dica PAROU com l1/l2 — a cena cheia: fileira + dica.
static void m12(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[9];
  FarolRecCand c[8];
  FarolRecSemente sm[4];
  const char *sr[4] = { "tt1000", "tt1001", "tt1002", "tt1003" };
  FarolCenaCtx x;
  FarolDicaFato d[1];
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, 0, i < 8 ? 4565 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 8; i++) {
    cf[i] = mkF(12000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i % 2 ? 35 : 18, 0, 0, 4565);
    c[i] = mkC(&cf[i], 12000 + i, FRC_F_SEMENTE);
    c[i].semente = (uint16_t)(i / 2);
    c[i].flags = FRC_FL_EM_REC;
  }
  memset(sm, 0, sizeof sm);
  for (i = 0; i < 4; i++) {
    sm[i].ficha = &sem[i];
    sm[i].terminou = 1;
  }
  x = mkCC(&g, c, 8, sm, 4, sr);
  dica(d, FC_DICA_PAROU, FP_ACAO_ABRIR, "d_parou", "tt0903747",
       3, "tt0903747", "2", "4", 1, "5");
  farolcena_home(B1, sizeof B1, &x, d, 1);
  farolcena_home(B2, sizeof B2, &x, d, 1);
  duas_vezes("m12", B1, B2);
  espera("m12", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PORQUE_TERMINOU\",\"a\":[\"tt1001\"]},\"forma\":\"cartaz\",\"itens\":[\"c2\",\"c4\",\"c0\",\"c7\",\"c1\",\"c3\",\"c6\",\"c5\"],\"motivo\":{\"c2\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1001\",\"k4565\"]},\"c4\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1002\",\"k4565\"]},\"c0\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1000\",\"k4565\"]},\"c7\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1003\",\"k4565\"]},\"c1\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g35\"]},\"c3\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1001\",\"k4565\"]},\"c6\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1003\",\"k4565\"]},\"c5\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1002\",\"k4565\"]}}},{\"t\":\"dica\",\"id\":\"d_parou\",\"l1\":{\"m\":\"FT_DICA_PAROU_L1\",\"a\":[\"tt0903747\",\"2\",\"4\"]},\"l2\":{\"m\":\"FT_DICA_PAROU_L2\",\"a\":[\"5\"]},\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt0903747\"},\"icone\":\"aj_farol\"}]}", 8);
}

// ------------------------------------------------------ MN1..MN7 (r1)
// Mesas novas do segundo ciclo: cada uma mata a mutacao nomeada e trava
// a borda correspondente (as ressalvas de teste fraco e de borda da r1).

// MN1: motivo sem dado real nao vira linha — SALVO sem data, TERMINOU
// de semente sem ref nomeavel e item sem nenhum motivo renderizavel
// saem SEM entrada em "motivo" (3.5; mutacoes M01, M02 e M13). E o
// titulo do dominante le o motivo RENDERIZADO (midx): com o GENERO
// dominante vindo de itens cujo TERMINOU nao renderizou, o arg tem de
// ser "g18" e nao "g<indice da semente>" (M21).
static void mn1(void) {
  FarolGosto g;
  FarolFicha sem[15], cf[10];
  FarolRecCand c[12];
  FarolRecSemente sm[5];
  const char *sr[5] = { NULL, NULL, NULL, "tt2033", "tt2044" };
  FarolCenaCtx x;
  uint16_t sd[12];
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1200 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, 0, 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 3; i++) {
    sem[12 + i] = mkF(1300 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                      35, 0, 0, 0);
    ve(&g, &sem[12 + i], FG_D_GOSTOU, (uint16_t)(DIA - 15 + i), 1);
  }
  memset(sm, 0, sizeof sm);
  for (i = 0; i < 5; i++) {
    sm[i].ficha = &sem[i];               // todas {g18}
    sm[i].terminou = 1;
  }
  for (i = 0; i < 12; i++) {
    sd[i] = 0;
    if (i >= 10) {
      // So o sinal SALVO e sem a data (sd = 0): nenhum motivo da 3.5 se
      // sustenta — o item nao pode ganhar linha nenhuma na fileira.
      c[i] = mkC(NULL, 31000 + i, FRC_F_SALVO);
    } else {
      // genero principal alterna 18/35 — os dois provados, sem teto de
      // genero na janela e sem vaga fora da curva.
      cf[i] = mkF(31000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                  i % 2 ? 35 : 18, 0, 0, 0);
      c[i] = mkC(&cf[i], 31000 + i, FRC_F_SEMENTE);
      // 0..5 apontam as sementes 0..2, que nao tem ref nomeavel: o
      // TERMINOU nao renderiza e a linha cai no GENERO provado. 6..9
      // apontam as sementes 3/4, nomeaveis: TERMINOU de verdade.
      c[i].semente = (uint16_t)(i < 6 ? i / 2 : 3 + (i - 6) / 2);
      c[i].flags = FRC_FL_EM_REC;
    }
  }
  x = mkCC(&g, c, 12, sm, 5, sr);
  x.salvoDia = sd;
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("mn1", B1, B2);
  espera("mn1", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_GENERO\",\"a\":[\"g35\"]},\"forma\":\"cartaz\",\"itens\":[\"c3\",\"c4\",\"c1\",\"c8\",\"c7\",\"c2\",\"c0\",\"c9\",\"c6\",\"c5\",\"c11\",\"c10\"],\"motivo\":{\"c3\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g35\"]},\"c4\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c1\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g35\"]},\"c8\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt2044\",\"g18\"]},\"c7\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt2033\"]},\"c2\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c0\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c9\":{\"m\":\"FT_MOT_TERMINOU\",\"a\":[\"tt2044\"]},\"c6\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt2033\",\"g18\"]},\"c5\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g35\"]}}}]}", 12);
}

// MN2: dominante empatado — a ordem do aceite decide o titulo, nao o
// ultimo a marcar (M05 trocava ">" por ">=" e roubava para o GENERO o
// que e do TERMINOU).
static void mn2(void) {
  FarolGosto g;
  FarolFicha sem[15], cf[6];
  FarolRecCand c[8];
  FarolRecSemente sm[3];
  const char *sr[3] = { "tt3100", "tt3101", "tt3102" };
  FarolCenaCtx x;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1200 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, 0, 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 3; i++) {
    sem[12 + i] = mkF(1300 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                      35, 0, 0, 0);
    ve(&g, &sem[12 + i], FG_D_GOSTOU, (uint16_t)(DIA - 15 + i), 1);
  }
  memset(sm, 0, sizeof sm);
  for (i = 0; i < 3; i++) {
    sm[i].ficha = &sem[i];
    sm[i].terminou = 1;
  }
  for (i = 0; i < 8; i++) {
    if (i < 6) {
      // 3 TERMINOU (sementes nomeaveis) e 3 GENERO g35: empate 3x3.
      cf[i] = mkF(32000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                  i < 3 ? 18 : 35, 0, 0, 0);
      c[i] = mkC(&cf[i], 32000 + i,
                 i < 3 ? FRC_F_SEMENTE : FRC_F_CATALOGO);
      if (i < 3) {
        c[i].semente = (uint16_t)i;
        c[i].flags = FRC_FL_EM_REC;
      }
    } else {
      c[i] = mkC(NULL, 32000 + i, FRC_F_CATALOGO);   // enchedor sem linha
    }
  }
  x = mkCC(&g, c, 8, sm, 3, sr);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("mn2", B1, B2);
  espera("mn2", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PORQUE_TERMINOU\",\"a\":[\"tt3100\"]},\"forma\":\"cartaz\",\"itens\":[\"c0\",\"c4\",\"c1\",\"c2\",\"c3\",\"c5\",\"c6\",\"c7\"],\"motivo\":{\"c0\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt3100\",\"g18\"]},\"c4\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g35\"]},\"c1\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt3101\",\"g18\"]},\"c2\":{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt3102\",\"g18\"]},\"c3\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g35\"]},\"c5\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g35\"]}}}]}", 8);
}

// MN3: duas dicas validas do mesmo tipo — a primeira da ordem vence
// (M16 escolhia a ultima).
static void mn3(void) {
  FarolGosto g;
  FarolFicha sem[12];
  FarolCenaCtx x;
  FarolDicaFato d[2];
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  dica(&d[0], FC_DICA_SALVO, FP_ACAO_ABRIR, "d_aa", "tt9001",
       1, "5M", 0, 0, 1, "1h32");
  dica(&d[1], FC_DICA_SALVO, FP_ACAO_ABRIR, "d_bb", "tt9002",
       1, "3M", 0, 0, 1, "2h10");
  farolcena_home(B1, sizeof B1, &x, d, 2);
  farolcena_home(B2, sizeof B2, &x, d, 2);
  duas_vezes("mn3", B1, B2);
  espera("mn3", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"dica\",\"id\":\"d_aa\",\"l1\":{\"m\":\"FT_DICA_SALVO_L1\",\"a\":[\"5M\"]},\"l2\":{\"m\":\"FT_DICA_SALVO_L2\",\"a\":[\"1h32\"]},\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt9001\"},\"icone\":\"aj_farol\"}]}", 0);
}

// MN4: "do diretor de <semente>, que voce viu <n> vezes" — o <n> conta
// so os titulos VISTOS da pessoa nas sementes (terminou + ela na
// direcao), nao todo sinal que o modelo acumulou (ressalva 3 da r1:
// aqui n = 12 no modelo, mas so 2 foram vistos). Sem a semente citada
// terminada e com ela na direcao, a frase nao sai — cai no PESSOA comum
// (M20 tirava a prova inteira).
static void mn4(void) {
  FarolGosto g;
  FarolFicha sem[21], fs[3], f0, f9, cf[8];
  FarolRecCand c[8];
  FarolRecSemente sm[5];
  const char *sr[5] = { "tt5000", "tt5001", "tt5002", "tt5003",
                        "tt5004" };
  FarolCenaCtx x;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    // pessoa 777: n = 12 de sinal no modelo, mas so 2 das sementes
    // terminadas a tem na direcao — e esse o "viu" que a frase conta.
    sem[i] = mkF(1400 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, 777, 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 9; i++) {
    // generos 35/36/37 com 3 evidencias fracas: conhecidos demais para
    // a vaga fora da curva, abaixo da pessoa na escolha do motivo.
    sem[12 + i] = mkF(1500 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                      35 + i / 3, 0, 999, 0);
    ve(&g, &sem[12 + i], FG_D_TERMINOU, (uint16_t)(DIA - 20 + i), 1);
  }
  f0 = mkF(5000, FF_FILME, 2010, 100, 80, 9000, 99, 0, 555, 0);
  fs[0] = mkF(5001, FF_FILME, 2011, 100, 80, 9000, 28, 0, 777, 0);
  fs[1] = mkF(5002, FF_FILME, 2012, 100, 80, 9000, 28, 0, 777, 0);
  fs[2] = mkF(5003, FF_FILME, 2013, 100, 80, 9000, 28, 0, 777, 0);
  f9 = mkF(5004, FF_FILME, 2014, 100, 80, 9000, 28, 0, 999, 0);
  f9.pessoa[1] = 777;
  f9.nPessoa = 2;                    // ela cria, mas nao dirige o titulo
  memset(sm, 0, sizeof sm);
  // a semente 0 e enchedor: o b do motor e o indice e 0 nao vale prova.
  sm[0].ficha = &f0;    sm[0].terminou = 1;
  sm[1].ficha = &fs[0]; sm[1].terminou = 1;    // dirigido por ela, visto
  sm[2].ficha = &fs[1]; sm[2].terminou = 1;    // dirigido por ela, visto
  sm[3].ficha = &fs[2]; sm[3].terminou = 0;    // dirigido, NAO visto
  sm[4].ficha = &f9;    sm[4].terminou = 1;    // visto, sem ela na direcao
  for (i = 0; i < 8; i++) {
    // genero principal gira 18/35/36/37: sem teto de genero na janela,
    // e so a pessoa em comum com a semente — nada de motivo TERMINOU.
    cf[i] = mkF(33000 + i, FF_FILME, 2015 + i, 95, 80, 9000,
                i % 4 ? 34 + i % 4 : 18, 0, 777, 0);
    c[i] = mkC(&cf[i], 33000 + i, FRC_F_SEMENTE);
    c[i].semente = (uint16_t)(1 + i / 2);
  }
  x = mkCC(&g, c, 8, sm, 5, sr);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("mn4", B1, B2);
  espera("mn4", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PESSOA\",\"a\":[\"p777\"]},\"forma\":\"cartaz\",\"itens\":[\"c1\",\"c4\",\"c2\",\"c7\",\"c3\",\"c0\",\"c5\",\"c6\"],\"motivo\":{\"c1\":{\"m\":\"FT_MOT_DIRETOR\",\"a\":[\"tt5001\",\"2\"]},\"c4\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p777\"]},\"c2\":{\"m\":\"FT_MOT_DIRETOR\",\"a\":[\"tt5002\",\"2\"]},\"c7\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p777\"]},\"c3\":{\"m\":\"FT_MOT_DIRETOR\",\"a\":[\"tt5002\",\"2\"]},\"c0\":{\"m\":\"FT_MOT_DIRETOR\",\"a\":[\"tt5001\",\"2\"]},\"c5\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p777\"]},\"c6\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p777\"]}}}]}", 8);
}

// MN5: fileira 2 pede tema que sustente explicacao — um grupo inteiro
// com tema sem prova nao vira fileira (M22 tirava a prova).
static void mn5(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[20];
  FarolRecCand c[20];
  FarolCenaCtx x;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, i < 4 ? 137427 : 0, i < 8 ? 4565 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 20; i++) {
    // 12 fortes de pessoa; 8 do tema 7777, que o modelo nunca viu — o
    // grupo existe na pagina 2 mas nao sustenta uma fileira.
    cf[i] = mkF(6000 + i, FF_FILME, 2010 + i, 90 + i * 5,
                i >= 12 ? 55 : 92, 9000,
                i % 2 ? 35 : 28, 0, i < 12 ? 137427 : 0,
                i >= 12 ? 7777 : 0);
    c[i] = mkC(&cf[i], 6000 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 20, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("mn5", B1, B2);
  espera("mn5", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_PESSOA\",\"a\":[\"p137427\"]},\"forma\":\"cartaz\",\"itens\":[\"c7\",\"c4\",\"c9\",\"c3\",\"c8\",\"c6\",\"c5\",\"c1\",\"c0\",\"c2\",\"c10\",\"c11\"],\"motivo\":{\"c7\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c4\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c9\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c3\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c8\":{\"m\":\"FT_MOT_FORA_CURVA\",\"a\":[\"g28\"]},\"c6\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c5\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c1\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c0\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c2\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c10\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]},\"c11\":{\"m\":\"FT_MOT_PESSOA\",\"a\":[\"p137427\"]}}}]}", 20);
}

// MN6: envelope no teto int64 — o validador para um digito antes do
// INT64_MAX real, entao a cena satura no maior numero que ela aceita
// (ressalva 4a: INT64_MAX bruto saia e era recusado).
static void mn6(void) {
  FarolGosto g;
  FarolFicha sem[2];
  FarolCenaCtx x;
  FarolDicaFato d[1];
  modelo(&g, sem, 2, 18, 0, 0, FG_D_GOSTOU);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  x.perfil = INT32_MAX;
  x.feita = INT64_MAX;
  x.valeS = INT64_MAX;
  dica(d, FC_DICA_TRAKT, FP_ACAO_NADA, "d_trakt", NULL, 0, 0, 0, 0,
       0, 0);
  farolcena_home(B1, sizeof B1, &x, d, 1);
  farolcena_home(B2, sizeof B2, &x, d, 1);
  duas_vezes("mn6", B1, B2);
  espera("mn6", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2147483647,\"feita\":9223372036854775799,\"vale_s\":9223372036854775799,\"origem\":\"regras\",\"pecas\":[{\"t\":\"dica\",\"id\":\"d_trakt\",\"l1\":{\"m\":\"FT_DICA_TRAKT\"},\"acao\":{\"tipo\":\"nada\"},\"icone\":\"aj_farol\"}]}", 0);
}

// MN7: cap apertado — gosto2 nunca sai sem gosto1 (ressalva 4b) e nada
// e escrito alem do cap; toda cena emitida passa no validador.
static void mn7(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[20];
  FarolRecCand c[20];
  FarolCenaCtx x;
  int i;
  size_t cp;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, i < 4 ? 137427 : 0, i < 8 ? 4565 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 20; i++) {
    cf[i] = mkF(6000 + i, FF_FILME, 2010 + i, 90 + i * 5,
                i >= 12 ? 55 : 92, 9000,
                i % 2 ? 35 : 28, 0, i < 12 ? 137427 : 0,
                i >= 12 ? 4565 : 0);
    c[i] = mkC(&cf[i], 6000 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 20, NULL, 0, NULL);
  for (cp = 96; cp <= sizeof B1; cp += 29) {
    int rc;
    memset(B2, 'Z', sizeof B2);
    rc = farolcena_home(B2, cp, &x, NULL, 0);
    if (rc < 0) continue;
    if ((size_t)rc >= cp || (cp < sizeof B2 && B2[cp] != 'Z')) {
      fprintf(stderr, "farol_cena: mn7 escreveu alem de cap %d\n", (int)cp);
      exit(1);
    }
    valida("mn7-cap", B2, 20);
    if (strstr(B2, "\"id\":\"gosto2\"") &&
        !strstr(B2, "\"id\":\"gosto1\"")) {
      fprintf(stderr, "farol_cena: mn7 gosto2 sem gosto1 (cap %d)\n%s\n",
              (int)cp, B2);
      exit(1);
    }
  }
  nMesas++;
}

// MN8: frio 0 com candidatos NAO vira fileira nem abre dica de gosto —
// a porta do frio 0 e so PERGUNTAS/TRAKT (3.4). M18 entrava no bloco
// com `ctx->g && nCand > 0` e forcava nivel = 2: a dica SALVO, valida
// nos fatos, escapava na cena de um perfil sem nenhum sinal.
static void mn8(void) {
  FarolGosto g;
  FarolFicha cf[10];
  FarolRecCand c[10];
  FarolDicaFato d[1];
  FarolCenaCtx x;
  int i;
  farolgosto_iniciar(&g, 1);                 // modelo vazio => nivel 0
  for (i = 0; i < 10; i++) {
    cf[i] = mkF(7000 + i, FF_FILME, 2010 + i, 90, 80, 9000,
                18, 0, 0, 0);
    c[i] = mkC(&cf[i], 7000 + i, FRC_F_CATALOGO);
  }
  dica(d, FC_DICA_SALVO, FP_ACAO_NADA, "d_s", NULL,
       1, "5M", 0, 0, 1, "1h32");
  x = mkCC(&g, c, 10, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, d, 1);
  farolcena_home(B2, sizeof B2, &x, d, 1);
  duas_vezes("mn8", B1, B2);
  espera("mn8", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[]}", 0);
}

// MN9: a fileira 2 pede grupo de tema com 6+ itens alem da primeira —
// 3 itens com tema provado no rabo da saida nao formam fileira (M10
// baixava o minimo para 1 e a fileira escapava).
static void mn9(void) {
  FarolGosto g;
  FarolFicha sem[16], cf[18];
  FarolRecCand c[18];
  FarolCenaCtx x;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(300 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 i < 10 ? 18 : 35, 0, i < 8 ? 137427 : 0,
                 i < 3 ? 500 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  for (i = 0; i < 4; i++) {        // genero detestado: empurra os itens
    sem[12 + i] = mkF(400 + i, FF_FILME, 2001 + i, 95, 80,  // do grupo de
                      9000, 28, 0, 0, 0);                   // tema p/ o rabo
    ve(&g, &sem[12 + i], FG_D_REPROVOU, (uint16_t)(DIA - 20 + i), 1);
  }
  for (i = 0; i < 18; i++) {
    cf[i] = mkF(7000 + i, FF_FILME, 2010 + i, 90 + i,
                i >= 12 ? 55 : 92, 9000,
                i >= 14 ? 28 : 18, 0, i < 14 ? 137427 : 0,
                i >= 14 ? 500 : 0);
    c[i] = mkC(&cf[i], 7000 + i, FRC_F_CATALOGO);
    c[i].h = (uint32_t)i * 2654435761u;
  }
  x = mkCC(&g, c, 18, NULL, 0, NULL);
  farolcena_home(B1, sizeof B1, &x, NULL, 0);
  farolcena_home(B2, sizeof B2, &x, NULL, 0);
  duas_vezes("mn9", B1, B2);
  espera("mn9", B1, "{\"v\":1,\"tela\":\"home\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"fileira\",\"id\":\"gosto1\",\"titulo\":{\"m\":\"FT_FIL_GENERO\",\"a\":[\"g18\"]},\"forma\":\"cartaz\",\"itens\":[\"c9\",\"c7\",\"c0\",\"c14\",\"c16\",\"c17\",\"c8\",\"c2\",\"c6\",\"c5\",\"c4\",\"c1\"],\"motivo\":{\"c9\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c7\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c0\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c14\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k500\"]},\"c16\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k500\"]},\"c17\":{\"m\":\"FT_MOT_TEMA\",\"a\":[\"k500\"]},\"c8\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c2\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c6\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c5\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c4\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},\"c1\":{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]}}}]}", 18);
}

// ----------------------------------------------------- detalhe MD1..MD4

static FarolCenaDet mkDet(FarolRecCand *alvo, const char *ref) {
  FarolCenaDet d;
  memset(&d, 0, sizeof d);
  d.alvo = *alvo;
  d.alvoRef = ref;
  return d;
}

// MD1: nivel 2 — porque (terminou + tema, selos) e a coluna info.
static void md1(void) {
  FarolGosto g;
  FarolFicha sem[12], af;
  FarolRecCand alvo;
  FarolRecSemente sm[1];
  const char *sr[1] = { "tt1000" };
  FarolCenaCtx x;
  FarolCenaDet d;
  FarolInfoAmigo am[2];
  struct tm agora;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, i < 4 ? 137427 : 0, i < 8 ? 4565 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  af = mkF(555, FF_FILME, 2024, 95, 80, 9000, 18, 0, 137427, 4565);
  alvo = mkC(&af, 555, FRC_F_SEMENTE);
  alvo.semente = 0;
  alvo.flags = FRC_FL_EM_REC;
  memset(sm, 0, sizeof sm);
  sm[0].ficha = &sem[0];
  sm[0].terminou = 1;
  x = mkCC(&g, NULL, 0, sm, 1, sr);
  d = mkDet(&alvo, "tt555");
  memset(&agora, 0, sizeof agora);
  agora.tm_hour = 22;
  agora.tm_min = 5;
  d.agora = &agora;
  d.minutos = 95;
  d.onde = "Netflix";
  memset(am, 0, sizeof am);
  snprintf(am[0].nome, sizeof am[0].nome, "Ana");
  snprintf(am[1].nome, sizeof am[1].nome, "Rafa");
  am[0].terminou = am[1].terminou = 1;
  d.amigos = am;
  d.nAmigos = 2;
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md1", B1, B2);
  espera("md1", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"porque\",\"alvo\":\"tt555\",\"confianca\":3,\"linhas\":[{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1000\",\"k4565\"]},{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]}],\"selos\":[{\"k\":\"pessoa\",\"id\":137427},{\"k\":\"tema\",\"id\":4565},{\"k\":\"genero\",\"id\":18}]},{\"t\":\"info\",\"alvo\":\"tt555\",\"itens\":[{\"k\":\"tempo\",\"l\":{\"m\":\"FT_INFO_TERMINA\",\"a\":[\"23:40\"]}},{\"k\":\"onde\",\"l\":{\"m\":\"FT_INFO_ONDE\",\"a\":[\"Netflix\"]}},{\"k\":\"amigos\",\"l\":{\"m\":\"FT_INFO_AMIGOS_TERM\",\"a\":[\"Ana\",\"Rafa\"]}}]}]}", 0);
}

// MD2: titulo visto — sem "porque" (vira "rever" na P6, nao na P3).
static void md2(void) {
  FarolGosto g;
  FarolFicha sem[12], af;
  FarolRecCand alvo;
  FarolCenaCtx x;
  FarolCenaDet d;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  af = mkF(556, FF_FILME, 2024, 95, 80, 9000, 18, 0, 0, 0);
  alvo = mkC(&af, 556, FRC_F_CATALOGO);
  alvo.flags = FRC_FL_VISTO;
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  d = mkDet(&alvo, "tt556");
  d.onde = "Netflix";
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md2", B1, B2);
  espera("md2", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"info\",\"alvo\":\"tt556\",\"itens\":[{\"k\":\"onde\",\"l\":{\"m\":\"FT_INFO_ONDE\",\"a\":[\"Netflix\"]}}]}]}", 0);
}

// MD3: frio 0 — sem "porque"; os fatos da P6 seguem valendo.
static void md3(void) {
  FarolGosto g;
  FarolFicha sem[2], af;
  FarolRecCand alvo;
  FarolCenaCtx x;
  FarolCenaDet d;
  modelo(&g, sem, 2, 18, 0, 0, FG_D_GOSTOU);
  af = mkF(557, FF_FILME, 2024, 95, 80, 9000, 18, 0, 0, 0);
  alvo = mkC(&af, 557, FRC_F_CATALOGO);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  d = mkDet(&alvo, "tt557");
  d.onde = "Netflix";
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md3", B1, B2);
  espera("md3", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"info\",\"alvo\":\"tt557\",\"itens\":[{\"k\":\"onde\",\"l\":{\"m\":\"FT_INFO_ONDE\",\"a\":[\"Netflix\"]}}]}]}", 0);
}

// MD4: sinal contrario fraco aqui — e a prova de que a mesma afinidade
// nao se repete em duas linhas do porque (a g18 do TERMINOU_TEMA cala a
// linha GENERO que a repetiria; ressalva 4c da r1).
static void md4(void) {
  FarolGosto g;
  FarolFicha sem[12], af;
  FarolRecCand alvo;
  FarolRecSemente sm[1];
  const char *sr[1] = { "tt1000" };
  FarolCenaCtx x;
  FarolCenaDet d;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1000 + i, FF_FILME, 2000 + i, 100, 80, 9000, 18, 0,
                 0, 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  // Um abandono cedo de filme de 2 h derruba o habito da faixa 100-140.
  {
    FarolFicha longo = mkF(1999, FF_FILME, 2020, 120, 80, 9000,
                           18, 0, 0, 0);
    ve(&g, &longo, FG_D_ABANDONOU, (uint16_t)(DIA - 10), 1);
  }
  af = mkF(558, FF_FILME, 2024, 120, 80, 9000, 18, 0, 0, 0);
  alvo = mkC(&af, 558, FRC_F_SEMENTE);
  alvo.semente = 0;
  alvo.flags = FRC_FL_EM_REC;
  memset(sm, 0, sizeof sm);
  sm[0].ficha = &sem[0];
  sm[0].terminou = 1;
  x = mkCC(&g, NULL, 0, sm, 1, sr);
  d = mkDet(&alvo, "tt558");
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md4", B1, B2);
  espera("md4", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"porque\",\"alvo\":\"tt558\",\"confianca\":2,\"linhas\":[{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1000\",\"g18\"]}],\"selos\":[{\"k\":\"genero\",\"id\":18}]}]}", 0);
}

// MD5: selo so com afinidade comprovada — pessoa, tema e o segundo
// genero da ficha nao sustentam explicacao e ficam de fora (M14).
static void md5(void) {
  FarolGosto g;
  FarolFicha sem[12], af;
  FarolRecCand alvo;
  FarolCenaCtx x;
  FarolCenaDet d;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  af = mkF(559, FF_FILME, 2024, 95, 80, 9000, 18, 99, 8888, 77);
  alvo = mkC(&af, 559, FRC_F_CATALOGO);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  d = mkDet(&alvo, "tt559");
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md5", B1, B2);
  espera("md5", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"porque\",\"alvo\":\"tt559\",\"confianca\":3,\"linhas\":[{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},{\"m\":\"FT_MOT_CABE_HOJE\",\"a\":[\"95\"]}],\"selos\":[{\"k\":\"genero\",\"id\":18}]}]}", 0);
}

// MD6: sinal contrario de verdade — o habito de duracao pesa contra o
// titulo (peso < -0,5) e a P3 honesta avisa na segunda linha (4.2). O
// modelo tambem reprova aquela duracao na faixa de hora atual, entao o
// "cabe hoje" nao nasce e sobra lugar para o aviso.
static void md6(void) {
  FarolGosto g;
  FarolFicha sem[12], af;
  FarolRecCand alvo;
  FarolCenaCtx x;
  FarolCenaDet d;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    // 10 reprovacoes + 2 "gostou" em filmes de ~2 h na faixa FH: habito
    // da faixa 100-140 min e o da hora atual ficam bem negativos; o
    // genero 18 segue provado (n = 2).
    sem[i] = mkF(1600 + i, FF_FILME, 2000 + i, 120, 80, 9000,
                 i < 10 ? 50 : 18, 0, 0, 0);
    veH(&g, &sem[i], i < 10 ? FG_D_REPROVOU : FG_D_GOSTOU,
        (uint16_t)(DIA - 30 + i));
  }
  af = mkF(560, FF_FILME, 2024, 120, 80, 9000, 18, 0, 0, 0);
  alvo = mkC(&af, 560, FRC_F_CATALOGO);
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  d = mkDet(&alvo, "tt560");
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md6", B1, B2);
  espera("md6", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"porque\",\"alvo\":\"tt560\",\"confianca\":3,\"linhas\":[{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]},{\"m\":\"FT_MOT_CONTRA\"}],\"selos\":[{\"k\":\"genero\",\"id\":18}]}]}", 0);
}

// MD7: titulo VISTO nao ganha o "porque" — a linha de recomendacao e
// so para o que a pessoa ainda nao viu (M11 tirava a trava e o porque
// escapava em pagina de titulo ja assistido).
static void md7(void) {
  FarolGosto g;
  FarolFicha sem[12], af;
  FarolRecCand alvo;
  FarolCenaCtx x;
  FarolCenaDet d;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 12; i++) {
    sem[i] = mkF(1100 + i, FF_FILME, 2000 + i, 100, 80, 9000,
                 18, 0, i < 4 ? 137427 : 0, i < 8 ? 4565 : 0);
    ve(&g, &sem[i], FG_D_GOSTOU, (uint16_t)(DIA - 30 + i), 1);
  }
  af = mkF(561, FF_FILME, 2024, 95, 80, 9000, 18, 0, 137427, 4565);
  alvo = mkC(&af, 561, FRC_F_CATALOGO);
  alvo.flags = FRC_FL_VISTO;                 // ja viu: sem porque
  x = mkCC(&g, NULL, 0, NULL, 0, NULL);
  d = mkDet(&alvo, "tt561");
  farolcena_detalhe(B1, sizeof B1, &x, &d);
  farolcena_detalhe(B2, sizeof B2, &x, &d);
  duas_vezes("md7", B1, B2);
  espera("md7", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[]}", 0);
}

// --------------------------------------------------- comparar MC1..MC3

static void cmpctx(FarolCenaCtx *x, FarolGosto *g, FarolRecCand *c, int n) {
  *x = mkCC(g, c, n, NULL, 0, NULL);
}

// MC1: veredito — a coluna de maior pontuacao que cabe no tempo.
static void mc1(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[3];
  FarolRecCand c[3];
  FarolCenaCtx x;
  FarolCenaCmp cp;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 3; i++) {
    cf[i] = mkF(20000 + i, FF_FILME, 2020, 95, 80, 9000, 18, 0, 0, 0);
    c[i] = mkC(&cf[i], 20000 + i, FRC_F_CATALOGO);
  }
  cmpctx(&x, &g, c, 3);
  memset(&cp, 0, sizeof cp);
  cp.itens[0] = "c0"; cp.itens[1] = "c1"; cp.itens[2] = "c2";
  cp.nItens = 3;
  cp.pontos[0] = 5000; cp.pontos[1] = 4000; cp.pontos[2] = 6000;
  cp.minutos[0] = 150; cp.minutos[1] = 95; cp.minutos[2] = 200;
  cp.tempoMin = 120;
  cp.eixos[0] = FP_EIXO_DURACAO; cp.eixos[1] = FP_EIXO_NOTA;
  cp.nEixos = 2;
  farolcena_comparar(B1, sizeof B1, &x, &cp);
  farolcena_comparar(B2, sizeof B2, &x, &cp);
  duas_vezes("mc1", B1, B2);
  espera("mc1", B1, "{\"v\":1,\"tela\":\"comparar\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"comparar\",\"itens\":[\"c0\",\"c1\",\"c2\"],\"eixos\":[\"duracao\",\"nota\"],\"veredito\":{\"m\":\"FT_CMP_VEREDITO_CABE\",\"a\":[\"c1\"]},\"escolha\":\"c1\"}]}", 3);
}

// MC2: nenhuma cabe — escolha e a de maior pontuacao, sem veredito
// (afirmar "cabe" sem dado e mentira, a mesma alma da 3.5).
static void mc2(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[3];
  FarolRecCand c[3];
  FarolCenaCtx x;
  FarolCenaCmp cp;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 3; i++) {
    cf[i] = mkF(21000 + i, FF_FILME, 2020, 95, 80, 9000, 18, 0, 0, 0);
    c[i] = mkC(&cf[i], 21000 + i, FRC_F_CATALOGO);
  }
  cmpctx(&x, &g, c, 3);
  memset(&cp, 0, sizeof cp);
  cp.itens[0] = "c0"; cp.itens[1] = "c1"; cp.itens[2] = "c2";
  cp.nItens = 3;
  cp.pontos[0] = 5000; cp.pontos[1] = 4000; cp.pontos[2] = 6000;
  cp.minutos[0] = 150; cp.minutos[1] = 95; cp.minutos[2] = 200;
  cp.tempoMin = 60;
  cp.eixos[0] = FP_EIXO_DURACAO; cp.eixos[1] = FP_EIXO_NOTA;
  cp.nEixos = 2;
  farolcena_comparar(B1, sizeof B1, &x, &cp);
  espera("mc2", B1, "{\"v\":1,\"tela\":\"comparar\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"comparar\",\"itens\":[\"c0\",\"c1\",\"c2\"],\"eixos\":[\"duracao\",\"nota\"],\"escolha\":\"c2\"}]}", 3);
}

// MC3: sem dado de tempo — escolha pela pontuacao, sem veredito.
static void mc3(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[2];
  FarolRecCand c[2];
  FarolCenaCtx x;
  FarolCenaCmp cp;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 2; i++) {
    cf[i] = mkF(22000 + i, FF_FILME, 2020, 95, 80, 9000, 18, 0, 0, 0);
    c[i] = mkC(&cf[i], 22000 + i, FRC_F_CATALOGO);
  }
  cmpctx(&x, &g, c, 2);
  memset(&cp, 0, sizeof cp);
  cp.itens[0] = "c0"; cp.itens[1] = "c1";
  cp.nItens = 2;
  cp.pontos[0] = 3000; cp.pontos[1] = 7000;
  cp.minutos[0] = 150; cp.minutos[1] = 95;
  cp.eixos[0] = FP_EIXO_DURACAO;
  cp.nEixos = 1;
  farolcena_comparar(B1, sizeof B1, &x, &cp);
  espera("mc3", B1, "{\"v\":1,\"tela\":\"comparar\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"comparar\",\"itens\":[\"c0\",\"c1\"],\"eixos\":[\"duracao\"],\"escolha\":\"c1\"}]}", 2);
}

// --------------------------------------------------------- contrato

// Entradas tortas sao recusadas na entrada, nao viram cena torta.
static void contrato(void) {
  FarolGosto g;
  FarolFicha sem[12], cf[3];
  FarolRecCand c[3];
  FarolCenaCtx x;
  FarolCenaCmp cp;
  int i;
  modelo(&g, sem, 12, 18, 0, 0, FG_D_GOSTOU);
  for (i = 0; i < 3; i++) {
    cf[i] = mkF(23000 + i, FF_FILME, 2020, 95, 80, 9000, 18, 0, 0, 0);
    c[i] = mkC(&cf[i], 23000 + i, FRC_F_CATALOGO);
  }
  x = mkCC(&g, c, 3, NULL, 0, NULL);
  memset(&cp, 0, sizeof cp);
  cp.nItens = 2;
  cp.nEixos = 1;
  cp.eixos[0] = FP_EIXO_NOTA;
  cp.itens[0] = "c0";
  cp.itens[1] = "c0";                       // ref repetida
  assert(farolcena_comparar(B1, sizeof B1, &x, &cp) == -1);
  cp.itens[1] = "c9";                       // ref fora da lista
  assert(farolcena_comparar(B1, sizeof B1, &x, &cp) == -1);
  cp.itens[1] = "c1";
  cp.eixos[1] = FP_EIXO_NOTA;               // eixo repetido
  cp.nEixos = 2;
  assert(farolcena_comparar(B1, sizeof B1, &x, &cp) == -1);
  cp.eixos[1] = 9;                          // eixo fora da lista
  assert(farolcena_comparar(B1, sizeof B1, &x, &cp) == -1);
  assert(farolcena_home(B1, 4, &x, NULL, 0) == -1);
  assert(farolcena_home(NULL, sizeof B1, &x, NULL, 0) == -1);
}

// ------------------------------------------ perfis aleatorios (fuzz)

static uint32_t rng;
static uint32_t xr(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}

// -------------------------------------- conferencia da verdade (3.5)
// Oraculo independente das regras, portado da sonda da r1 e endurecido
// nos pontos que a revisao cobrou: gosto2 sem gosto1 virou falha (era
// contador), a colisao k/g do TERMINOU_TEMA virou falha (era contador),
// o DIRETOR confere a contagem de titulos VISTOS nas sementes (nao o n
// total do modelo) e o porque nao repete afinidade. Roda em TODA cena
// do fuzz — nao so esquema e determinismo.

static const char *vJ;          // cena conferida, so p/ a mensagem
#define VFALHA(...) do { \
    fprintf(stderr, "farol_cena: 3.5 " __VA_ARGS__); \
    fprintf(stderr, "\n%s\n", vJ); exit(1); } while (0)

static const FarolAfin *vg_afin(const FarolGosto *g, int dim, int32_t id) {
  const FarolAfin *v = dim == FG_DIM_GENERO ? g->genero :
                       dim == FG_DIM_PESSOA ? g->pessoa : g->tema;
  int cap = dim == FG_DIM_GENERO ? FG_GENERO_N :
            dim == FG_DIM_PESSOA ? FG_PESSOA_N : FG_TEMA_N, i;
  for (i = 0; i < cap; i++)
    if (v[i].id == id) return &v[i];
  return NULL;
}
// Mesma prova da 3.5 que o compositor exige: n >= 2 e peso > 0,5.
static int vg_prova(const FarolCenaCtx *x, int dim, int32_t id) {
  const FarolAfin *a = vg_afin(x->g, dim, id);
  return a && a->n >= 2 && farolgosto_peso(x->g, dim, id, x->dia) > 128;
}
static int vg_tem(const int32_t *v, int cap, int32_t id) {
  int i;
  for (i = 0; i < cap && v[i]; i++)
    if (v[i] == id) return 1;
  return 0;
}
static int vg_comum(const FarolFicha *a, const FarolFicha *b) {
  int n = 0, i;
  for (i = 0; i < FAROL_FICHA_GEN && a->genero[i]; i++)
    n += vg_tem(b->genero, FAROL_FICHA_GEN, a->genero[i]);
  for (i = 0; i < FAROL_FICHA_PESSOA && a->pessoa[i]; i++)
    n += vg_tem(b->pessoa, FAROL_FICHA_PESSOA, a->pessoa[i]);
  for (i = 0; i < FAROL_FICHA_TEMA && a->tema[i]; i++)
    n += vg_tem(b->tema, FAROL_FICHA_TEMA, a->tema[i]);
  return n;
}
// Le {"m":"X"[,"a":["..",..]]} em p (apos o '{'); devolve o fim.
static const char *vtexto(const char *p, char *m, char a[3][64], int *na) {
  const char *q;
  *na = 0;
  m[0] = 0;
  if (strncmp(p, "{\"m\":\"", 6)) VFALHA("texto sem m em %.40s", p);
  p += 6;
  q = strchr(p, '"');
  snprintf(m, 48, "%.*s", (int)(q - p), p);
  p = q + 1;
  if (!strncmp(p, ",\"a\":[", 6)) {
    p += 6;
    while (*p == '"') {
      p++;
      q = strchr(p, '"');
      if (*na >= 3) VFALHA("args demais");
      snprintf(a[*na], 64, "%.*s", (int)(q - p), p);
      (*na)++;
      p = q + 1;
      if (*p == ',') p++;
    }
    if (*p != ']') VFALHA("a[] torto");
    p++;
  }
  if (*p != '}') VFALHA("texto nao fecha");
  return p + 1;
}
static void vtempo(char *d, long dd) {
  if (dd < 14) sprintf(d, "%ldd", dd > 0 ? dd : 1);
  else if (dd < 63) sprintf(d, "%ldw", dd / 7);
  else if (dd < 730) sprintf(d, "%ldM", dd / 30);
  else sprintf(d, "%ldy", dd / 365);
}
// A linha de motivo e VERDADEIRA para o candidato c? (3.5, recalculada
// aqui — nao basta o esquema bater.)
static void vlinha(const FarolCenaCtx *x, const FarolRecCand *c,
                   uint16_t sd, const char *m, char a[3][64], int na,
                   int nivel) {
  static const char *const MODN[] = {
    "FT_MOT_TERMINOU", "FT_MOT_TERMINOU_TEMA", "FT_MOT_GENERO",
    "FT_MOT_PESSOA", "FT_MOT_DIRETOR", "FT_MOT_TEMA", "FT_MOT_AMIGO",
    "FT_MOT_AMIGO_ANON", "FT_MOT_RECOMENDADO", "FT_MOT_RECOM_ANON",
    "FT_MOT_SALVO", "FT_MOT_CABE_HOJE", "FT_MOT_REVER",
    "FT_MOT_FORA_CURVA", "FT_MOT_CONTRA", 0 };
  const FarolFicha *f = c->ficha;
  char t[64];
  int i;
  for (i = 0; MODN[i] && strcmp(MODN[i], m); i++) ;
  if (!MODN[i]) VFALHA("modelo de motivo desconhecido %s", m);
  if (!strncmp(m, "FT_MOT_TERMINOU", 15)) {
    const FarolRecSemente *s;
    if (c->semente == FRC_SEM_SEMENTE || c->semente >= x->nSementes)
      VFALHA("TERMINOU sem semente");
    s = &x->sementes[c->semente];
    if (!s->terminou) VFALHA("TERMINOU de semente nao terminada");
    if (!((c->flags & FRC_FL_EM_REC) ||
          (f && s->ficha && vg_comum(f, s->ficha) >= 2)))
      VFALHA("TERMINOU sem rec nem 2+ afinidades");
    if (!x->sementeRef || !x->sementeRef[c->semente] ||
        strcmp(x->sementeRef[c->semente], a[0]))
      VFALHA("TERMINOU ref errada %s", a[0]);
    if (!strcmp(m, "FT_MOT_TERMINOU_TEMA")) {
      int32_t id = atoi(a[1] + 1);
      if (na != 2 || nivel < 2 || !f || !s->ficha)
        VFALHA("TERMINOU_TEMA sem base");
      if (a[1][0] == 'k') {
        // r2: "k" so com a prova do TEMA — um id de genero provado nao
        // vale rotulo de tema (a colisao numerica da r1 virou falha).
        if (!vg_tem(f->tema, FAROL_FICHA_TEMA, id) ||
            !vg_tem(s->ficha->tema, FAROL_FICHA_TEMA, id) ||
            !vg_prova(x, FG_DIM_TEMA, id))
          VFALHA("TERMINOU_TEMA k%ld sem prova de tema", (long)id);
      } else if (a[1][0] == 'g') {
        if (!vg_tem(f->genero, FAROL_FICHA_GEN, id) ||
            !vg_tem(s->ficha->genero, FAROL_FICHA_GEN, id) ||
            !vg_prova(x, FG_DIM_GENERO, id))
          VFALHA("TERMINOU_TEMA g%ld sem prova de genero", (long)id);
      } else VFALHA("TERMINOU_TEMA prefixo %s", a[1]);
    } else if (na != 1) VFALHA("TERMINOU aridade");
  } else if (!strcmp(m, "FT_MOT_GENERO") || !strcmp(m, "FT_MOT_PESSOA") ||
             !strcmp(m, "FT_MOT_TEMA")) {
    int dim = m[7] == 'G' ? FG_DIM_GENERO :
              m[7] == 'P' ? FG_DIM_PESSOA : FG_DIM_TEMA;
    char pf = m[7] == 'G' ? 'g' : m[7] == 'P' ? 'p' : 'k';
    int32_t id = atoi(a[0] + 1);
    if (na != 1 || a[0][0] != pf || nivel < 2 || !f)
      VFALHA("%s sem base (nivel %d)", m, nivel);
    if (!(dim == FG_DIM_GENERO ? vg_tem(f->genero, FAROL_FICHA_GEN, id)
          : dim == FG_DIM_PESSOA ? vg_tem(f->pessoa, FAROL_FICHA_PESSOA, id)
          : vg_tem(f->tema, FAROL_FICHA_TEMA, id)))
      VFALHA("%s: id %ld fora da ficha", m, (long)id);
    if (!vg_prova(x, dim, id))
      VFALHA("%s: afinidade %ld sem n>=2/peso>0,5", m, (long)id);
  } else if (!strcmp(m, "FT_MOT_DIRETOR")) {
    const FarolRecSemente *s;
    int nv = 0;
    if (na != 2 || nivel < 2 || !f || c->semente == FRC_SEM_SEMENTE ||
        c->semente >= x->nSementes)
      VFALHA("DIRETOR sem base");
    s = &x->sementes[c->semente];
    // Ressalva 3: so a semente terminada com ela na direcao sustenta a
    // frase, e o <n> conta so titulos vistos dela nas sementes — nao o
    // n total de sinais do modelo.
    if (!s->ficha || !f->pessoa[0] ||
        s->ficha->pessoa[0] != f->pessoa[0] || !s->terminou ||
        !vg_prova(x, FG_DIM_PESSOA, f->pessoa[0]))
      VFALHA("DIRETOR sem prova");
    for (i = 0; i < x->nSementes; i++) {
      const FarolRecSemente *si = &x->sementes[i];
      if (si->ficha && si->terminou &&
          si->ficha->pessoa[0] == f->pessoa[0])
        nv++;
    }
    if (atoi(a[1]) != nv || !x->sementeRef ||
        !x->sementeRef[c->semente] ||
        strcmp(x->sementeRef[c->semente], a[0]))
      VFALHA("DIRETOR contagem/ref errada (diz %s, conta %d)", a[1], nv);
  } else if (!strcmp(m, "FT_MOT_AMIGO")) {
    int d2 = (c->gostoPct + 5) / 10;
    if (d2 > 10) d2 = 10;
    if (na != 2 || !(c->fontes & FRC_F_AMIGO) || !c->quem ||
        c->gostoPct < 70 || a[0][0] != 'u' ||
        atoi(a[0] + 1) != c->quem || atoi(a[1]) != d2)
      VFALHA("AMIGO falso");
  } else if (!strcmp(m, "FT_MOT_RECOMENDADO")) {
    if (na != 1 || !(c->fontes & FRC_F_REC) || !c->quem ||
        a[0][0] != 'u' || atoi(a[0] + 1) != c->quem)
      VFALHA("RECOMENDADO falso");
  } else if (!strcmp(m, "FT_MOT_AMIGO_ANON") ||
             !strcmp(m, "FT_MOT_RECOM_ANON") ||
             !strcmp(m, "FT_MOT_REVER")) {
    VFALHA("%s nao pode sair (anon com quem!=0 / REVER nunca existe)", m);
  } else if (!strcmp(m, "FT_MOT_SALVO")) {
    if (na != 1 || !(c->fontes & FRC_F_SALVO) || !sd || x->dia <= sd)
      VFALHA("SALVO sem data real");
    vtempo(t, (long)x->dia - sd);
    if (strcmp(t, a[0])) VFALHA("SALVO tempo %s != %s", a[0], t);
  } else if (!strcmp(m, "FT_MOT_CABE_HOJE")) {
    if (na != 1 || !f || !f->minutos || atoi(a[0]) != f->minutos)
      VFALHA("CABE_HOJE sem duracao");
  } else if (!strcmp(m, "FT_MOT_FORA_CURVA")) {
    const FarolAfin *af;
    if (na != 1 || nivel < 2 || !f || !f->genero[0] || a[0][0] != 'g' ||
        atoi(a[0] + 1) != f->genero[0])
      VFALHA("FORA_CURVA sem genero real");
    af = vg_afin(x->g, FG_DIM_GENERO, f->genero[0]);
    if (af && af->n >= 3) VFALHA("FORA_CURVA em genero com n>=3");
  } else if (!strcmp(m, "FT_MOT_CONTRA")) {
    int mn = f ? f->minutos : 0;
    int fd = mn <= 0 ? -1 : mn < 30 ? 0 : mn < 60 ? 1 : mn < 100 ? 2 :
             mn < 140 ? 3 : 4;
    if (na || fd < 0 ||
        farolgosto_peso(x->g, FG_DIM_DURACAO,
                        fd + 1 + (f->tipo == FF_SERIE ? 5 : 0),
                        x->dia) >= -128)
      VFALHA("CONTRA sem habito contrario");
  }
}
// Classe do motivo p/ o titulo, na prioridade do aceite (mesma ordem do
// compositor: terminou, pessoa, tema, genero, hora, amigos, salvos,
// recomendados, rever, fora da curva).
static int vtipo(const char *m) {
  if (!strncmp(m, "FT_MOT_TERMINOU", 15)) return 0;
  if (!strcmp(m, "FT_MOT_PESSOA") || !strcmp(m, "FT_MOT_DIRETOR"))
    return 1;
  if (!strcmp(m, "FT_MOT_TEMA")) return 2;
  if (!strcmp(m, "FT_MOT_GENERO")) return 3;
  if (!strcmp(m, "FT_MOT_CABE_HOJE")) return 4;
  if (!strncmp(m, "FT_MOT_AMIGO", 12)) return 5;
  if (!strcmp(m, "FT_MOT_SALVO")) return 6;
  if (!strncmp(m, "FT_MOT_RECOM", 12)) return 7;
  if (!strcmp(m, "FT_MOT_REVER")) return 8;
  return 9;
}
// O argumento de afinidade que a linha carrega ("g<"/"k<"/"p<"), para a
// regra "a mesma afinidade nao aparece duas vezes no porque".
static const char *vargafin(const char *m, char a[3][64], int na) {
  if (!na) return NULL;
  if (!strcmp(m, "FT_MOT_TERMINOU_TEMA")) return a[1];
  if (!strcmp(m, "FT_MOT_GENERO") || !strcmp(m, "FT_MOT_PESSOA") ||
      !strcmp(m, "FT_MOT_TEMA") || !strcmp(m, "FT_MOT_FORA_CURVA"))
    return a[0];
  return NULL;
}
// Recheca a cena home inteira: pecas validas, fileiras com 6+ itens sem
// repeticao, cada linha de motivo verdadeira, titulo pelo dominante,
// gosto2 so atras de gosto1 e com tema provado, frio respeitado.
static void vhome(const FarolCenaCtx *x, const char *J, int rc) {
  const char *p;
  int nivel, nf = 0, ndica = 0;
  unsigned char usado[400] = { 0 };
  static const char *const RUIM[] = { ",]", ",}", "[,", "{,", ",,", ":,", 0 };
  int i;
  vJ = J;
  if (rc < 0) return;
  if ((size_t)rc != strlen(J) || rc > FAROL_CENA_MAX)
    VFALHA("comprimento %d", rc);
  for (i = 0; RUIM[i]; i++)
    if (strstr(J, RUIM[i])) VFALHA("virgula torta '%s'", RUIM[i]);
  if (strstr(J, "\"x\":")) VFALHA("texto livre x");
  if (J[rc - 1] != '}' || J[rc - 2] != ']') VFALHA("fim torto");
  nivel = x->g ? farolrec_nivel(x->g) : 0;
  for (p = J; (p = strstr(p, "{\"t\":\"dica\"")); p++) {
    ndica++;
    if (nivel == 0 && !strstr(p, "\"l1\":{\"m\":\"FT_DICA_PERGUNTAS\"") &&
        !strstr(p, "\"l1\":{\"m\":\"FT_DICA_TRAKT\""))
      VFALHA("frio 0 com dica de gosto");
  }
  for (p = J; (p = strstr(p, "{\"t\":\"fileira\"")); p++) {
    const char *fim = strstr(p + 1, "{\"t\":\""), *q, *it;
    char tm[48], ta[3][64];
    int tna, n = 0, itens[16], cnt[10] = { 0 }, k, dom = -1, nm = 0;
    char marg[16][64];
    int mtipo[16];
    if (!fim) fim = J + rc;
    nf++;
    // r2 endurecida: gosto2 sem gosto1 nao existe mais — nem contador.
    if (strncmp(p + 14, nf == 1 ? ",\"id\":\"gosto1\"" : ",\"id\":\"gosto2\"",
                14))
      VFALHA("id da fileira %d (gosto2 sem gosto1?)", nf);
    q = strstr(p, "\"titulo\":");
    vtexto(q + 9, tm, ta, &tna);
    it = strstr(p, "\"itens\":[") + 9;
    while (*it == '"') {
      int ix = atoi(it + 2);
      if (it[1] != 'c' || ix < 0 || ix >= x->nCand || n >= 12)
        VFALHA("item fora");
      if (usado[ix]) VFALHA("item c%d repetido na cena", ix);
      usado[ix] = 1;
      itens[n++] = ix;
      if (x->cand[ix].flags & FRC_FL_VISTO) VFALHA("item visto c%d", ix);
      if (x->cand[ix].flags & FRC_FL_CONTINUAR)
        VFALHA("item do Continuar c%d na Home", ix);
      it = strchr(it + 1, '"') + 1;
      if (*it == ',') it++;
    }
    if (n < 6) VFALHA("fileira com %d itens", n);
    q = strstr(p, "\"motivo\":{");
    if (q && q < fim) {
      q += 10;
      while (*q == '"') {
        int ix = atoi(q + 2), ok = 0;
        char m[48], a[3][64];
        int na;
        for (k = 0; k < n; k++)
          if (itens[k] == ix) ok = 1;
        if (!ok) VFALHA("motivo de item fora da fileira c%d", ix);
        q = strchr(q + 1, '"') + 2;
        q = vtexto(q, m, a, &na);
        vlinha(x, &x->cand[ix],
               x->salvoDia ? x->salvoDia[ix] : 0, m, a, na, nivel);
        if (nivel == 1 && strcmp(m, "FT_MOT_TERMINOU") &&
            strcmp(m, "FT_MOT_CABE_HOJE") && strcmp(m, "FT_MOT_SALVO") &&
            strncmp(m, "FT_MOT_AMIGO", 12) &&
            strncmp(m, "FT_MOT_RECOM", 12))
          VFALHA("frio 1 com motivo de gosto %s", m);
        mtipo[nm] = vtipo(m);
        snprintf(marg[nm], 64, "%s",
                 !strcmp(m, "FT_MOT_DIRETOR") ? "" : a[0]);
        cnt[mtipo[nm]]++;
        nm++;
        if (*q == ',') q++;
      }
    }
    for (k = 0; k < 10; k++)
      if (dom < 0 || cnt[k] > cnt[dom]) dom = k;
    if (!cnt[dom]) dom = -1;
    if (nf == 2) {
      int32_t id = atoi(ta[0] + 1);
      if (nivel < 2 || strcmp(tm, "FT_FIL_TEMA") || tna != 1 ||
          ta[0][0] != 'k' || !vg_prova(x, FG_DIM_TEMA, id))
        VFALHA("fileira 2 sem tema provado");
      for (k = 0; k < n; k++)
        if (!x->cand[itens[k]].ficha ||
            !vg_tem(x->cand[itens[k]].ficha->tema, FAROL_FICHA_TEMA, id))
          VFALHA("fileira 2: c%d sem o tema", itens[k]);
    } else if (nivel == 1) {
      if (strcmp(tm, "FT_FIL_COMECAR") || tna)
        VFALHA("frio 1 titulo %s", tm);
    } else {
      static const char *const T[10] = {
        "FT_FIL_PORQUE_TERMINOU", "FT_FIL_PESSOA", "FT_FIL_TEMA",
        "FT_FIL_GENERO", "FT_FIL_HORA", "FT_FIL_AMIGOS",
        "FT_FIL_SALVOS", "FT_FIL_RECOMENDADOS", "FT_FIL_REVER",
        "FT_FIL_FORA_CURVA" };
      const char *e = dom < 0 ? "FT_FIL_RECOMENDADOS" : T[dom];
      if (dom == 4 && x->fimSemana) e = "FT_FIL_FIMSEMANA";
      if (strcmp(tm, e))
        VFALHA("titulo %s, dominante pede %s", tm, e);
      if (dom >= 0 && dom <= 3) {
        int best = 0, kk, f2;
        const char *ba = 0;
        if (tna != 1) VFALHA("titulo sem arg");
        for (k = 0; k < nm; k++) {
          if (mtipo[k] != dom) continue;
          f2 = 0;
          for (kk = 0; kk < nm; kk++)
            if (mtipo[kk] == dom && !strcmp(marg[kk], marg[k])) f2++;
          if (f2 > best) { best = f2; ba = marg[k]; }
        }
        if (!ba || strcmp(ba, ta[0])) {
          // o arg do titulo tem de vir de um motivo do tipo dominante
          int ok = 0;
          for (k = 0; k < nm; k++)
            if (mtipo[k] == dom && !strcmp(marg[k], ta[0])) ok = 1;
          if (!ok && !(dom == 1 && ta[0][0] == 'p'))
            VFALHA("arg do titulo %s nao vem do dominante", ta[0]);
        }
      } else if (tna) VFALHA("titulo com arg sobrando");
    }
  }
  if (nf > 2 || ndica > 1) VFALHA("%d fileiras %d dicas", nf, ndica);
  if (nivel == 0 && nf) VFALHA("frio 0 com fileira");
  if (nivel == 1 && nf > 1) VFALHA("frio 1 com 2 fileiras");
}
// Recheca a cena de detalhe: porque so em titulo nao visto, linhas
// verdadeiras e sem afinidade repetida, selos so com prova.
static void vdet(const FarolCenaCtx *x, const FarolCenaDet *d,
                 const char *J, int rc) {
  const char *p;
  int nivel;
  vJ = J;
  if (rc < 0) return;
  nivel = x->g ? farolrec_nivel(x->g) : 0;
  p = strstr(J, "{\"t\":\"porque\"");
  if (p) {
    const char *q = strstr(p, "\"linhas\":[") + 10;
    char afs[2][64];
    int nl = 0;
    if (nivel == 0) VFALHA("frio 0 com porque");
    if (d->alvo.flags & FRC_FL_VISTO) VFALHA("porque em titulo visto");
    while (*q == '{') {
      char m[48], a[3][64];
      int na;
      const char *af;
      q = vtexto(q, m, a, &na);
      vlinha(x, &d->alvo, d->salvoDia, m, a, na, nivel);
      if (nivel == 1 &&
          (!strcmp(m, "FT_MOT_GENERO") || !strcmp(m, "FT_MOT_PESSOA") ||
           !strcmp(m, "FT_MOT_TEMA") ||
           !strcmp(m, "FT_MOT_TERMINOU_TEMA")))
        VFALHA("frio 1 com 'voce gosta'");
      // r2: a mesma afinidade nao se repete em duas linhas do porque.
      af = vargafin(m, a, na);
      if (af) {
        if (nl && !strcmp(afs[0], af))
          VFALHA("afinidade %s repetida no porque", af);
        if (nl < 2) snprintf(afs[nl], 64, "%.63s", af);
      }
      nl++;
      if (*q == ',') q++;
    }
    if (nl < 1 || nl > 2) VFALHA("porque com %d linhas", nl);
    q = strstr(p, "\"selos\":[");
    if (q) {
      q += 9;
      while (*q == '{') {
        int dim = q[6] == 'p' ? FG_DIM_PESSOA :
                  q[6] == 't' ? FG_DIM_TEMA : FG_DIM_GENERO;
        int32_t id = atoi(strstr(q, "\"id\":") + 5);
        const FarolFicha *f = d->alvo.ficha;
        if (!f || !vg_prova(x, dim, id) ||
            !(dim == FG_DIM_PESSOA ? vg_tem(f->pessoa, FAROL_FICHA_PESSOA, id)
              : dim == FG_DIM_TEMA ? vg_tem(f->tema, FAROL_FICHA_TEMA, id)
              : vg_tem(f->genero, FAROL_FICHA_GEN, id)))
          VFALHA("selo falso %ld", (long)id);
        q = strchr(q, '}') + 1;
        if (*q == ',') q++;
      }
    }
  }
}

// 300 perfis sinteticos: toda cena passa no validador sem recusa, a
// mesma entrada devolve a mesma cena byte a byte, e TODA linha de
// motivo e rechecada contra a 3.5 pelo oraculo acima — inclusive nas
// cenas emitidas com cap menor que 8193.
static void aleatorio(void) {
  static FarolFicha sf[40], cf[40];
  static FarolRecCand c[40];
  static FarolRecSemente sm[4];
  static const char *sr[4] = { "tt1", "tt2", "tt3", "tt4" };
  static FarolDicaFato d[4];
  static uint16_t sd[40];
  static char rbuf[4][FAROL_REF_TAM];
  FarolGosto g;
  FarolCenaCtx x;
  int t, i, nS, nC;
  rng = 0x5EED44u;
  for (t = 0; t < 300; t++) {
    int nM = (int)(xr() % 14);         // 0..13: cobre os 3 niveis de frio
    farolgosto_iniciar(&g, 1);
    for (i = 0; i < nM; i++) {
      sf[i] = mkF((int32_t)(30000 + t * 100 + i), FF_FILME,
                  2000 + (int)(xr() % 25), 80 + (int)(xr() % 80),
                  60 + (int)(xr() % 40), 9000,
                  (int32_t)(xr() % 6) + 18, 0,
                  (int32_t)(xr() % 4) * 137427,
                  (int32_t)(xr() % 5) * 4565);
      ve(&g, &sf[i], (int)(xr() % 3) ? FG_D_GOSTOU : FG_D_TERMINOU,
         (uint16_t)(DIA - (int)(xr() % 60)), 1);
    }
    nS = (int)(xr() % 5);
    for (i = 0; i < nS && i < 4; i++) {
      sm[i].ficha = &sf[i < nM ? i : 0];
      sm[i].terminou = (uint8_t)(xr() & 1);
    }
    nC = (int)(xr() % 33);
    for (i = 0; i < nC; i++) {
      cf[i] = mkF((int32_t)(50000 + t * 100 + i), FF_FILME,
                  2000 + (int)(xr() % 25), 80 + (int)(xr() % 80),
                  60 + (int)(xr() % 40), 9000,
                  (int32_t)(xr() % 6) + 18, 0,
                  (int32_t)(xr() % 4) * 137427,
                  (int32_t)(xr() % 5) * 4565);
      c[i] = mkC(&cf[i], 50000 + t * 100 + i,
                 (uint8_t)(1u << (xr() % 7)));
      c[i].flags = (uint8_t)(xr() % 64);
      if (xr() & 1) c[i].semente = (uint16_t)(nS ? (int)(xr() % nS) : 0);
      c[i].quem = (int32_t)(xr() % 100);
      c[i].gostoPct = (uint8_t)(xr() % 101);
      c[i].abandonoDia = (uint16_t)(xr() % (2 * DIA));
      c[i].h = xr();
      c[i].franquia = (int32_t)(xr() % 40);
      sd[i] = (uint16_t)(xr() % (2 * DIA));
    }
    for (i = 0; i < 4; i++) {
      int tipo = (int)(xr() % FC_DICA_N);
      snprintf(rbuf[i], sizeof rbuf[i], "tt%lu",
               (unsigned long)(xr() % 90000));
      dica(&d[i], tipo, (int)(xr() % 6), "d_r", rbuf[i],
           0, 0, 0, 0, 0, 0);
    }
    x = mkCC(&g, c, nC, sm, nS, sr);
    x.salvoDia = sd;
    {
      int r1 = farolcena_home(B1, sizeof B1, &x, d, 4);
      farolcena_home(B2, sizeof B2, &x, d, 4);
      duas_vezes("aleatorio", B1, B2);
      valida("aleatorio", B1, nC);
      vhome(&x, B1, r1);
      // Cap apertado em cada perfil: -1 ou cena valida, nunca fora do
      // buffer e nunca gosto2 sem gosto1 (ressalva 4b da r1).
      {
        size_t cp = 200 + xr() % (sizeof B1 - 200);
        int r2;
        memset(B2, 'Z', sizeof B2);
        r2 = farolcena_home(B2, cp, &x, d, 4);
        if (r2 >= 0) {
          if ((size_t)r2 >= cp || B2[cp] != 'Z') {
            fprintf(stderr, "farol_cena: estouro de cap %d\n", (int)cp);
            exit(1);
          }
          valida("aleatorio-cap", B2, nC);
          vhome(&x, B2, r2);
        }
      }
    }
    // detalhe aleatorio em cima do primeiro candidato
    if (nC) {
      FarolCenaDet dd;
      int r1;
      memset(&dd, 0, sizeof dd);
      dd.alvo = c[0];
      dd.alvoRef = (xr() & 1) ? "tt777" : "c0";
      dd.salvoDia = sd[0];
      dd.onde = (xr() & 1) ? "Netflix" : NULL;
      r1 = farolcena_detalhe(B1, sizeof B1, &x, &dd);
      farolcena_detalhe(B2, sizeof B2, &x, &dd);
      duas_vezes("aleatorio-det", B1, B2);
      valida("aleatorio-det", B1, nC);
      vdet(&x, &dd, B1, r1);
    }
  }
}

int main(void) {
  dump = getenv("FC_DUMP") != NULL;
  m1(); m2(); m3(); m4(); m5(); m6(); m7(); m8(); m9(); m10(); m11();
  m12();
  mn1(); mn2(); mn3(); mn4(); mn5(); mn6(); mn7(); mn8(); mn9();
  md1(); md2(); md3(); md4(); md5(); md6(); md7();
  mc1(); mc2(); mc3();
  contrato();
  if (!dump) aleatorio();
  printf("farol_cena: %d mesas, %d cenas validadas\n", nMesas, nValid);
  return 0;
}
