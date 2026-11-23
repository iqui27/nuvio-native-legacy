// Descoberta local do Farol (PLANO secao 2.9, issue 24).
// Cobre: FA_NAO_VI nunca devolve visto (cacavisto_filme, progresso>=90, item
// iniciado e copia duplicada no catalogo); FA_CURTO respeita duracao (todos
// os formatos de CatItem.meta, rejeitando ano solto, temporadas e canais);
// < 3 elegiveis devolve o que tem (0, 1, 2, sem duplicar a mesma obra);
// determinismo pela semente (mesma semente = mesma saida; sementes distintas
// variam; deslocar indices preserva as obras escolhidas); FA_RETOMAR pela
// memoria do item (progresso/restanteMin/retomadoMs/rewatch); e cat_revisao
// invalidando indices entre quadros e em troca concorrente no meio do laco.
//
// Nenhum URL, titulo real, nome de perfil ou host cru entra neste teste.
#include "farolacha.h"
#include "cacavisto.h"
#include "faroltexto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CAT_CAP 64

static CatItem  C_itens[CAT_CAP];
static int      C_n;
static unsigned C_rev = 1;

static char     V_ids[32][32];
static long     V_tmdbs[32];
static int      V_n;

// Simula troca concorrente do catalogo no meio de farolacha_listar.
static int      R_trocarNaLeitura = -1;
static int      R_leituras;
static int      R_tempestade;

static void zeraCena(void) {
  memset(C_itens, 0, sizeof C_itens);
  C_n = 0;
  C_rev++;
  V_n = 0;
  R_trocarNaLeitura = -1;
  R_leituras = 0;
  R_tempestade = 0;
}

static int addItem(const char *id, const char *tipo, const char *cod,
                   const char *genero, const char *meta, int nota) {
  int i;
  assert(C_n < CAT_CAP);
  i = C_n++;
  memset(&C_itens[i], 0, sizeof C_itens[i]);
  snprintf(C_itens[i].imdb, sizeof C_itens[i].imdb, "%s", id ? id : "");
  snprintf(C_itens[i].tipo, sizeof C_itens[i].tipo, "%s", tipo ? tipo : "movie");
  snprintf(C_itens[i].titulo, sizeof C_itens[i].titulo, "%s", cod ? cod : "");
  snprintf(C_itens[i].genero, sizeof C_itens[i].genero, "%s", genero ? genero : "");
  snprintf(C_itens[i].meta, sizeof C_itens[i].meta, "%s", meta ? meta : "");
  C_itens[i].nota = nota;
  return i;
}

static void marcarVisto(const char *id, long tmdb) {
  assert(V_n < 32);
  snprintf(V_ids[V_n], sizeof V_ids[0], "%s", id ? id : "");
  V_tmdbs[V_n] = tmdb;
  V_n++;
}

int cat_n(void) { return C_n; }

unsigned cat_revisao(void) { return C_rev; }

const CatItem *cat_item(int i) {
  R_leituras++;
  if (R_tempestade) C_rev++;
  else if (R_trocarNaLeitura > 0 && R_leituras == R_trocarNaLeitura) {
    // Na troca simulada o indice 0 vira visto e uma obra nova entra no fim.
    C_itens[0].progresso = 100;
    C_rev++;
    R_trocarNaLeitura = -1;
  }
  return (i >= 0 && i < C_n) ? &C_itens[i] : NULL;
}

int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  int i;
  if (quando) *quando = 0;
  for (i = 0; i < V_n; i++) {
    if (imdb && imdb[0] && V_ids[i][0] && !strcmp(imdb, V_ids[i])) return 1;
    if (tmdb > 0 && V_tmdbs[i] > 0 && tmdb == V_tmdbs[i]) return 1;
  }
  return 0;
}

static void t1_nao_vi_nunca_devolve_visto(void) {
  FarolAchado out[8];
  int i0, i1, i2, i3, i4, i5, i6, i7, i8, n, k;
  unsigned s;

  zeraCena();
  i0 = addItem("tt0000101", "movie",  "obra_101", "Filme · Suspense", "2024 · 95 min", 88);
  marcarVisto("tt0000101", 0); // visto via cacavisto_filme

  i1 = addItem("tt0000102:1:3", "movie", "obra_102", "Filme · Drama", "2023 · 105 min", 85);
  marcarVisto("tt0000102", 0); // visto pelo id base mesmo com sufixo :T:E

  i2 = addItem("tt0000103", "movie",  "obra_103", "Filme · Crime", "2022 · 110 min", 90);
  C_itens[i2].progresso = 92; // visto pela memoria do item (>= 90% em filme)

  i3 = addItem("tt0000104", "movie",  "obra_104", "Filme · Suspense", "2025 · 100 min", 84);
  C_itens[i3].progresso = 35; // ja iniciado (Continuar): nao pode sair em FA_NAO_VI

  i4 = addItem("tt0000105", "series", "obra_105", "Programa de TV · Drama", "2024 · 50 min", 86);
  C_itens[i4].retomadoMs = 1700000000000LL; // retomada remota sem barra

  // Copia duplicada: a de Continuar tem progresso e a da fileira tem 0.
  i5 = addItem("tt0000106", "movie",  "obra_106", "Filme · Mistério", "2021 · 98 min", 89);
  C_itens[i5].progresso = 40;
  i6 = addItem("tt0000106", "movie",  "obra_106", "Filme · Mistério", "2021 · 98 min", 89);
  (void)i0; (void)i1; (void)i6;

  // Apenas estes dois nunca foram vistos nem iniciados.
  i7 = addItem("tt0000107", "movie",  "obra_107", "Filme · Suspense", "2025 · 94 min", 79);
  i8 = addItem("tt0000108", "series", "obra_108", "Programa de TV · Mistério", "2024", 81);

  for (s = 0; s < 40; s++) {
    n = farolacha_listar(FA_NAO_VI, 80, out, 3, s);
    assert(n == 2); // < 3 elegiveis e maxMin ignorado fora de FA_CURTO
    for (k = 0; k < n; k++) {
      assert(out[k].indice == i7 || out[k].indice == i8);
      assert((out[k].motivo & FA_MOT_NAO_VI) != 0);
    }
    assert(out[0].indice != out[1].indice);
  }

  // Se os dois restantes tambem forem vistos, FA_NAO_VI devolve 0 (nunca cai em visto).
  marcarVisto("tt0000107", 0);
  C_itens[i8].progresso = 100;
  assert(farolacha_listar(FA_NAO_VI, 0, out, 3, 7) == 0);
}

static void t2_curto_respeita_duracao(void) {
  FarolAchado out[8];
  int iSem, i84, i100, i101, i142, iCanal, iVisto90, n, k;
  unsigned s;

  // Parser puro de duracao em CatItem.meta (descoberta.c:379 e variantes).
  assert(farolacha_minutos_meta("2024  ·  95 min") == 95);
  assert(farolacha_minutos_meta("2025 · 1h 40min") == 100);
  assert(farolacha_minutos_meta("2025 · 1h40") == 100);
  assert(farolacha_minutos_meta("2010 · 2h") == 120);
  assert(farolacha_minutos_meta("45min") == 45);
  assert(farolacha_minutos_meta("88m") == 88);
  assert(farolacha_minutos_meta("2023 • PT1H35M") == 95);
  assert(farolacha_minutos_meta("2024") == 0);
  assert(farolacha_minutos_meta("1992") == 0);
  assert(farolacha_minutos_meta("2022 · 3 temporadas") == 0);
  assert(farolacha_minutos_meta("14:30") == 0);
  assert(farolacha_minutos_meta("0 min") == 0);
  assert(farolacha_minutos_meta("") == 0);
  assert(farolacha_minutos_meta(NULL) == 0);

  zeraCena();
  iSem   = addItem("tt0000201", "movie",   "obra_201", "Filme · Drama", "2024", 90);
  i84    = addItem("tt0000202", "movie",   "obra_202", "Filme · Suspense", "2026  ·  84 min", 76);
  i100   = addItem("tt0000203", "movie",   "obra_203", "Filme · Suspense", "2025 · 1h40", 80);
  i101   = addItem("tt0000204", "movie",   "obra_204", "Filme · Suspense", "2025 · 1h 41min", 92);
  i142   = addItem("tt0000205", "movie",   "obra_205", "Filme · Aventura", "2015 · 142 min", 95);
  iCanal = addItem("cs:channel:01", "channel", "canal_01", "Canal", "45 min", 99);
  (void)iSem; (void)i101; (void)i142; (void)iCanal;

  for (s = 0; s < 30; s++) {
    n = farolacha_listar(FA_CURTO, 100, out, 3, s);
    assert(n == 2);
    for (k = 0; k < n; k++) {
      assert(out[k].indice == i84 || out[k].indice == i100);
      assert(out[k].minutos > 0 && out[k].minutos <= 100);
      assert((out[k].motivo & FA_MOT_CURTO) != 0);
      assert((out[k].motivo & FA_MOT_NAO_VI) != 0);
    }
  }

  // maxMin <= 0 usa o teto padrao de 100 min (1h40).
  assert(farolacha_listar(FA_CURTO, 0, out, 3, 1) == 2);

  // Teto mais estrito corta o de 100 min.
  n = farolacha_listar(FA_CURTO, 90, out, 3, 1);
  assert(n == 1 && out[0].indice == i84 && out[0].minutos == 84);

  // Nenhum filme/serie cabe em 60 min (o canal de 45 min nao conta).
  assert(farolacha_listar(FA_CURTO, 60, out, 3, 1) == 0);

  // Nao vistos tem prioridade sobre vistos dentro de FA_CURTO.
  iVisto90 = addItem("tt0000206", "movie", "obra_206", "Filme · Suspense", "2020 · 90 min", 99);
  marcarVisto("tt0000206", 0);
  addItem("tt0000207", "movie", "obra_207", "Filme · Suspense", "2024 · 92 min", 70);
  n = farolacha_listar(FA_CURTO, 100, out, 3, 5);
  assert(n == 3);
  for (k = 0; k < n; k++) {
    assert(out[k].indice != iVisto90);
    assert((out[k].motivo & FA_MOT_NAO_VI) != 0);
  }
}

static void t3_menos_de_tres_e_dedup(void) {
  FarolAchado out[4];
  int a, b, n;

  zeraCena();
  assert(farolacha_listar(FA_CURTO, 100, out, 3, 9) == 0);
  assert(farolacha_listar(FA_NAO_VI, 0, out, 3, 9) == 0);
  assert(farolacha_listar(FA_RETOMAR, 0, out, 3, 9) == 0);
  assert(farolacha_listar(FA_SURPRESA, 0, out, 3, 9) == 0);

  a = addItem("tt0000301", "movie", "obra_301", "Filme · Drama", "2024 · 89 min", 78);
  out[1].indice = -77;
  n = farolacha_listar(FA_NAO_VI, 0, out, 3, 9);
  assert(n == 1 && out[0].indice == a && out[0].minutos == 89);
  assert(out[1].indice == -77); // nao escreve alem de n

  // Duas copias do mesmo tt + uma segunda obra = 2 resultados distintos, nunca 3.
  addItem("tt0000301", "movie", "obra_301", "Filme · Drama", "2024 · 89 min", 78);
  b = addItem("tt0000302", "movie", "obra_302", "Filme · Crime", "2023 · 95 min", 82);
  n = farolacha_listar(FA_SURPRESA, 0, out, 3, 9);
  assert(n == 2);
  assert((out[0].indice == a || out[0].indice == 1) || out[0].indice == b);
  assert(strcmp(C_itens[out[0].indice].imdb, C_itens[out[1].indice].imdb) != 0);

  // Entradas invalidas.
  assert(farolacha_listar(FA_NAO_VI, 0, NULL, 3, 1) == 0);
  assert(farolacha_listar(FA_NAO_VI, 0, out, 0, 1) == 0);
  assert(farolacha_listar(FA_NAO_VI, 0, out, -2, 1) == 0);
  assert(farolacha_listar((FarolAchaTipo)99, 0, out, 3, 1) == 0);
}

static void t4_determinismo_semente(void) {
  FarolAchado a[3], b[3];
  char id[32], cod[32], meta[32];
  FarolAchaTipo tipos[4] = { FA_CURTO, FA_NAO_VI, FA_RETOMAR, FA_SURPRESA };
  unsigned seeds[4] = { 0u, 1u, 42u, 0xdeadbeefu };
  int i, t, s;

  zeraCena();
  for (i = 0; i < 14; i++) {
    snprintf(id, sizeof id, "tt00004%02d", i);
    snprintf(cod, sizeof cod, "obra_4%02d", i);
    snprintf(meta, sizeof meta, "2024 · %d min", 75 + (i % 5) * 5);
    addItem(id, "movie", cod, "Filme · Suspense", meta, 72 + (i % 6));
  }
  for (i = 0; i < 6; i++) {
    int idx;
    snprintf(id, sizeof id, "tt00005%02d", i);
    snprintf(cod, sizeof cod, "obra_5%02d", i);
    idx = addItem(id, "movie", cod, "Filme · Drama", "2023 · 110 min", 80);
    C_itens[idx].progresso = 25 + i * 10;
    C_itens[idx].restanteMin = 80 - i * 8;
  }

  for (t = 0; t < 4; t++) {
    int viuDiferente = 0;
    FarolAchado base[3];
    int nBase = farolacha_listar(tipos[t], 100, base, 3, 0u);
    assert(nBase == 3);

    for (s = 0; s < 4; s++) {
      int na = farolacha_listar(tipos[t], 100, a, 3, seeds[s]);
      int nb = farolacha_listar(tipos[t], 100, b, 3, seeds[s]);
      assert(na == 3 && nb == 3);
      assert(memcmp(a, b, sizeof a) == 0);
    }

    for (s = 1; s < 32; s++) {
      int ns = farolacha_listar(tipos[t], 100, a, 3, (unsigned)s);
      assert(ns == 3);
      if (a[0].indice != base[0].indice ||
          a[1].indice != base[1].indice ||
          a[2].indice != base[2].indice)
        viuDiferente = 1;
    }
    assert(viuDiferente); // a semente realmente dirige o sorteio
  }

  // Deslocar todos os indices em +1 (ex.: item visto entrou na frente) com a
  // mesma semente encontra as MESMAS obras nos novos indices (+1).
  {
    int n0 = farolacha_listar(FA_NAO_VI, 0, a, 3, 77u);
    assert(n0 == 3);
    memmove(&C_itens[1], &C_itens[0], sizeof(CatItem) * (size_t)C_n);
    C_n++;
    memset(&C_itens[0], 0, sizeof C_itens[0]);
    snprintf(C_itens[0].imdb, sizeof C_itens[0].imdb, "tt0000999");
    snprintf(C_itens[0].tipo, sizeof C_itens[0].tipo, "movie");
    snprintf(C_itens[0].titulo, sizeof C_itens[0].titulo, "obra_999");
    C_itens[0].progresso = 100;
    C_rev++;
    assert(farolacha_listar(FA_NAO_VI, 0, b, 3, 77u) == 3);
    for (i = 0; i < 3; i++) assert(b[i].indice == a[i].indice + 1);
  }
}

static void t5_retomar_memoria_do_item(void) {
  FarolAchado out[6];
  int iZero, iEpPacote, iFim, iCaca, iAntigo, iRecente, iRewatch, n;

  zeraCena();
  // Nao iniciado: fora.
  iZero = addItem("tt0000601", "movie", "obra_601", "Filme · Drama", "2024 · 100 min", 80);
  // Serie com temporada/episodio do pacote (catalogo.c:719) mas sem progresso: fora.
  iEpPacote = addItem("tt0000602", "series", "obra_602", "Programa de TV", "2024 · 42 min", 80);
  C_itens[iEpPacote].temporada = 1;
  C_itens[iEpPacote].episodio = 1;
  // Filme >= 90% sem rewatch: concluido, fora.
  iFim = addItem("tt0000603", "movie", "obra_603", "Filme · Crime", "2024 · 120 min", 82);
  C_itens[iFim].progresso = 94;
  // Filme marcado visto em cacavisto_filme: fora.
  iCaca = addItem("tt0000604", "movie", "obra_604", "Filme · Crime", "2024 · 120 min", 82);
  C_itens[iCaca].progresso = 40;
  marcarVisto("tt0000604", 0);
  (void)iZero; (void)iFim;

  // Em andamento mais antigo (calcula minutos restantes a partir de meta + %).
  iAntigo = addItem("tt0000605", "movie", "obra_605", "Filme · Drama", "2024 · 100 min", 80);
  C_itens[iAntigo].progresso = 60;
  C_itens[iAntigo].retomadoMs = 1000LL;

  // Em andamento mais recente (usa restanteMin direto da memoria do item).
  iRecente = addItem("tt0000606", "series", "obra_606", "Programa de TV · Suspense", "2025 · 50 min", 85);
  C_itens[iRecente].temporada = 2;
  C_itens[iRecente].episodio = 5;
  C_itens[iRecente].progresso = 50;
  C_itens[iRecente].restanteMin = 23;
  C_itens[iRecente].retomadoMs = 5000LL;

  // Rewatch ativo entra mesmo com visto anterior.
  iRewatch = addItem("tt0000607", "movie", "obra_607", "Filme · Aventura", "2020 · 120 min", 88);
  marcarVisto("tt0000607", 0);
  C_itens[iRewatch].rewatch = 1;
  C_itens[iRewatch].progresso = 50;
  C_itens[iRewatch].restanteMin = 60;
  C_itens[iRewatch].retomadoMs = 3000LL;

  n = farolacha_listar(FA_RETOMAR, 30, out, 5, 11u);
  assert(n == 3);
  assert(out[0].indice == iRecente && out[0].minutos == 23);
  assert(out[1].indice == iRewatch && out[1].minutos == 60);
  assert(out[2].indice == iAntigo  && out[2].minutos == 40);
  assert((out[0].motivo & FA_MOT_RETOMAR) != 0);
}

static void t6_revisao_entre_quadros(void) {
  FarolAchado out[3], novo[3];
  unsigned rev = 0, rev2 = 0;
  int i0, i1, n;

  zeraCena();
  i0 = addItem("tt0000701", "movie", "obra_701", "Filme · Suspense", "2025 · 90 min", 80);
  i1 = addItem("tt0000702", "movie", "obra_702", "Filme · Suspense", "2025 · 92 min", 82);
  (void)i0; (void)i1;

  n = farolacha_listar_rev(FA_NAO_VI, 0, out, 3, 3u, &rev);
  assert(n == 2 && rev == C_rev);
  assert(farolacha_item(&out[0], rev) == &C_itens[out[0].indice]);

  // Quadro seguinte: o catalogo inteiro foi trocado; o indice antigo agora
  // aponta para um item JA VISTO, e uma obra inedita entrou no indice 2.
  C_itens[0].progresso = 100;
  C_itens[1].progresso = 100;
  addItem("tt0000703", "movie", "obra_703", "Filme · Suspense", "2026 · 85 min", 84);
  C_rev++;

  // O indice do quadro anterior e recusado na hora por farolacha_item.
  assert(farolacha_item(&out[0], rev) == NULL);
  assert(farolacha_item(&out[1], rev) == NULL);

  // Nova consulta nao guarda estado: le o catalogo novo e acha so o indice 2.
  n = farolacha_listar_rev(FA_NAO_VI, 0, novo, 3, 3u, &rev2);
  assert(n == 1 && rev2 == C_rev && novo[0].indice == 2);
  assert(farolacha_item(&novo[0], rev2) == &C_itens[2]);

  // Troca concorrente simulada no meio do laco: refaz na revisao nova.
  zeraCena();
  addItem("tt0000710", "movie", "obra_710", "Filme · Drama", "2024 · 90 min", 80);
  addItem("tt0000711", "movie", "obra_711", "Filme · Drama", "2024 · 91 min", 81);
  R_trocarNaLeitura = 4; // apos ler i=0 na varredura principal, i=0 vira visto e C_rev sobe
  n = farolacha_listar_rev(FA_NAO_VI, 0, out, 3, 5u, &rev);
  assert(n == 1 && out[0].indice == 1 && rev == C_rev);

  // Tempestade continua (cat_revisao muda a cada leitura): aborta limpo em 0.
  R_tempestade = 1;
  assert(farolacha_listar_rev(FA_NAO_VI, 0, out, 3, 5u, &rev) == 0);
}

static void t7_genero_e_rotulos(void) {
  FarolAchado out[3];
  unsigned rev = 0;
  char gen[48];
  int iA, iB, iC, n;

  zeraCena();
  iA = addItem("tt0000801", "movie",  "obra_801", "Filme  ·  Suspense  ·  Drama", "2025 · 90 min", 80);
  iB = addItem("tt0000802", "series", "obra_802", "Programa de TV · Suspense", "2024 · 48 min", 83);
  iC = addItem("tt0000803", "movie",  "obra_803", "Filme · Comédia", "2025 · 88 min", 85);
  (void)iC;

  n = farolacha_listar_genero(FA_CURTO, 100, "suspense", out, 3, 12u, &rev);
  assert(n == 2);
  assert((out[0].indice == iA && out[1].indice == iB) ||
         (out[0].indice == iB && out[1].indice == iA));
  assert(farolacha_genero_comum(out, n, rev, gen, sizeof gen) == 1);
  assert(strcmp(gen, "Suspense") == 0);

  // Com o item de Comedia junto, nenhum genero e comum aos tres: devolve 0
  // (e nunca devolve "Filme", que e rotulo de tipo e nao genero).
  n = farolacha_listar_rev(FA_CURTO, 100, out, 3, 12u, &rev);
  assert(n == 3);
  assert(farolacha_genero_comum(out, n, rev, gen, sizeof gen) == 0);
  assert(gen[0] == '\0');

  assert(!strcmp(farolacha_rotulo(FA_CURTO),    FT_INTENCAO_CURTO));
  assert(!strcmp(farolacha_rotulo(FA_NAO_VI),   FT_INTENCAO_NAO_VI));
  assert(!strcmp(farolacha_rotulo(FA_RETOMAR),  FT_INTENCAO_RETOMAR));
  assert(!strcmp(farolacha_rotulo(FA_SURPRESA), FT_INTENCAO_SURPRESA));
  assert(!strcmp(farolacha_rotulo((FarolAchaTipo)-1), ""));
}

int main(void) {
  t1_nao_vi_nunca_devolve_visto();
  t2_curto_respeita_duracao();
  t3_menos_de_tres_e_dedup();
  t4_determinismo_semente();
  t5_retomar_memoria_do_item();
  t6_revisao_entre_quadros();
  t7_genero_e_rotulos();
  puts("farol_acha: regras ok");
  return 0;
}
