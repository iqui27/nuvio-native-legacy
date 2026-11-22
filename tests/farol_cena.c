// Compositor deterministico do FarolCena (lote sense, issue 44;
// PLANO-GOSTO 4.2, 4.3 e 5.2). Sem SDL nem rede: linka
// src/farolcena.c + src/farolrec.c + src/farolgosto.c + src/farolinfo.c
// + src/farolpeca.c + src/faroltexto.c + src/js.c — toda cena gerada
// passa pelo validador da 39 (inclusive em perfis aleatorios), com
// nRecusadas == 0. Cobre: frio 0/1/2, ate 2 fileiras + 1 dica, motivo
// so quando e verdadeiro, titulo pelo motivo dominante, detalhe
// (porque + info), veredito da comparacao e o determinismo byte a byte
// (mesma entrada + mesma semente = mesma cena).
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

// MD4: sinal contrario forte — o habito de duracao pesa contra, e a P3
// honesta avisa (4.2).
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
  espera("md4", B1, "{\"v\":1,\"tela\":\"detalhe\",\"perfil\":2,\"feita\":1795000000,\"vale_s\":86400,\"origem\":\"regras\",\"pecas\":[{\"t\":\"porque\",\"alvo\":\"tt558\",\"confianca\":3,\"linhas\":[{\"m\":\"FT_MOT_TERMINOU_TEMA\",\"a\":[\"tt1000\",\"g18\"]},{\"m\":\"FT_MOT_GENERO\",\"a\":[\"g18\"]}],\"selos\":[{\"k\":\"genero\",\"id\":18}]}]}", 0);
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

// 300 perfis sinteticos: toda cena passa no validador sem recusa e a
// mesma entrada devolve a mesma cena, byte a byte.
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
    farolcena_home(B1, sizeof B1, &x, d, 4);
    farolcena_home(B2, sizeof B2, &x, d, 4);
    duas_vezes("aleatorio", B1, B2);
    valida("aleatorio", B1, nC);
    // detalhe aleatorio em cima do primeiro candidato
    if (nC) {
      FarolCenaDet dd;
      memset(&dd, 0, sizeof dd);
      dd.alvo = c[0];
      dd.alvoRef = (xr() & 1) ? "tt777" : "c0";
      dd.salvoDia = sd[0];
      dd.onde = (xr() & 1) ? "Netflix" : NULL;
      farolcena_detalhe(B1, sizeof B1, &x, &dd);
      farolcena_detalhe(B2, sizeof B2, &x, &dd);
      duas_vezes("aleatorio-det", B1, B2);
      valida("aleatorio-det", B1, nC);
    }
  }
}

int main(void) {
  dump = getenv("FC_DUMP") != NULL;
  m1(); m2(); m3(); m4(); m5(); m6(); m7(); m8(); m9(); m10(); m11();
  m12();
  md1(); md2(); md3(); md4();
  mc1(); mc2(); mc3();
  contrato();
  if (!dump) aleatorio();
  printf("farol_cena: %d mesas, %d cenas validadas\n", nMesas, nValid);
  return 0;
}
