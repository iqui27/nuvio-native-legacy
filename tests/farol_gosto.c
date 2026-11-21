// Modelo de gosto do Farol (issue 37, secoes 2.1-2.2 do PLANO-GOSTO). Sem SDL
// nem rede: linka so src/farolgosto.c. O relogio e injetado por parametro —
// o teste anda dias inteiros para frente e para tras. Cobre: aplicar delta,
// decaimento (meia-vida 120/45), sessoes diarias com gravar/ler entre elas
// (invariante de persistencia, formato FG 2), saturacao por n, despejo do
// mais fraco e mais velho, aneis recusado/mostrado, serializar e ler com ida
// e volta identica, FG 1 antigo ainda valido, versao desconhecida ignorada e
// 10 000 deltas aleatorios.
//
//   bash tests/farol_gosto.sh                  (ASan+UBSan, o padrao)
//   SANITIZE=0 bash tests/farol_gosto.sh       (so os asserts)
#include "../src/farolgosto.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Soma |peso| projetado de todo o vetor (afinidades + hora), em Q8.8.
static int64_t pesoTotal(const FarolGosto *g, uint16_t dia) {
  static const struct { int dim; int cap; } DIMS[] = {
    { FG_DIM_GENERO, FG_GENERO_N }, { FG_DIM_PESSOA, FG_PESSOA_N },
    { FG_DIM_TEMA, FG_TEMA_N },     { FG_DIM_DECADA, FG_DECADA_N },
    { FG_DIM_IDIOMA, FG_IDIOMA_N }, { FG_DIM_DURACAO, FG_DURACAO_N },
  };
  int64_t s = 0;
  size_t d;
  int i, j;
  for (d = 0; d < sizeof DIMS / sizeof DIMS[0]; d++) {
    const FarolAfin *arr =
      DIMS[d].dim == FG_DIM_GENERO  ? g->genero  :
      DIMS[d].dim == FG_DIM_PESSOA  ? g->pessoa  :
      DIMS[d].dim == FG_DIM_TEMA    ? g->tema    :
      DIMS[d].dim == FG_DIM_DECADA  ? g->decada  :
      DIMS[d].dim == FG_DIM_IDIOMA  ? g->idioma  : g->duracao;
    for (i = 0; i < DIMS[d].cap; i++)
      if (arr[i].id) {
        int32_t p = farolgosto_peso(g, DIMS[d].dim, arr[i].id, dia);
        s += p < 0 ? -(int64_t)p : p;
      }
  }
  for (i = 0; i < FG_HORA_F; i++)
    for (j = 0; j < FG_HORA_D; j++) {
      int16_t v = farolgosto_hora(g, i, j, dia);
      s += v < 0 ? -(int64_t)v : v;
    }
  return s;
}

static FarolGostoFicha fichaBase(void) {
  FarolGostoFicha f;
  memset(&f, 0, sizeof f);
  f.tmdb = 1234;
  f.tipo = FG_TIPO_FILME;
  f.ano = 1999;
  f.idioma = ('p' << 8) | 't';
  f.minutos = 110;
  f.genero[0] = 18;
  f.genero[1] = 53;
  f.pessoa[0] = 137427;
  f.tema[0] = 9951;
  return f;
}

static void decaimento(void) {
  FarolGosto g;
  FarolGostoFicha f = fichaBase();
  int32_t p;
  farolgosto_iniciar(&g, 2);
  farolgosto_aplicar(&g, &f, FG_D_TERMINOU, 0, 3, 1);
  p = farolgosto_peso(&g, FG_DIM_GENERO, 18, 0);
  assert(p == FG_D_TERMINOU);
  // 120 dias: metade exata da meia-vida, com folga muito menor que os 2%.
  p = farolgosto_peso(&g, FG_DIM_GENERO, 18, 120);
  assert(p >= 125 && p <= 131);              // 128 +- 3
  // 240 dias: um quarto.
  p = farolgosto_peso(&g, FG_DIM_GENERO, 18, 240);
  assert(p >= 62 && p <= 66);
  // duracao/hora tem meia-vida 45: em 45 dias, metade.
  p = farolgosto_peso(&g, FG_DIM_DURACAO, 4, 45);
  assert(p >= 125 && p <= 131);
  assert(farolgosto_hora(&g, 3, 3, 45) >= 125 &&
         farolgosto_hora(&g, 3, 3, 45) <= 131);
  // O guardado NAO muda na leitura (decaimento preguicoso).
  assert(g.genero[0].peso == FG_D_TERMINOU && g.genero[0].dia == 0);
  // Ao tocar de novo, o decaído vira base do novo incremento.
  farolgosto_aplicar(&g, &f, FG_D_TERMINOU, 120, 3, 0);
  p = g.genero[0].peso;
  // 128 (metade) + 256*8/9 (saturado por n=1) = 128 + 227 = 355.
  assert(p >= 350 && p <= 360);
  assert(g.genero[0].dia == 120 && g.genero[0].n == 2);
  // dia andando para tras nao desdecai nem quebra.
  farolgosto_aplicar(&g, &f, FG_D_TERMINOU, 60, 3, 0);
  p = farolgosto_peso(&g, FG_DIM_GENERO, 18, 120);
  assert(p > 500);
}

static void saturacao(void) {
  FarolGosto g40, g4;
  FarolGostoFicha f = fichaBase(), f2;
  int i;
  int64_t s40, s4;
  // 40 episodios da MESMA serie: +0,1 por episodio.
  f.tipo = FG_TIPO_SERIE;
  farolgosto_iniciar(&g40, 1);
  for (i = 0; i < 40; i++)
    farolgosto_aplicar(&g40, &f, 26, (uint16_t)i, i % 8, i == 0);
  // 4 filmes terminados e gostados: +2,0 cada, fichas diferentes.
  farolgosto_iniciar(&g4, 1);
  for (i = 0; i < 4; i++) {
    f2 = fichaBase();
    f2.tmdb = 2000 + i;
    f2.genero[1] = 878;
    f2.pessoa[0] = 100 + i;
    f2.tema[0] = 7000 + i;
    farolgosto_aplicar(&g4, &f2, FG_D_GOSTOU, (uint16_t)i, i % 8, 1);
  }
  s40 = pesoTotal(&g40, 40);
  s4 = pesoTotal(&g4, 40);
  assert(s40 < s4);                    // o aceite: 40 epis < 4 filmes
  // A saturacao e visivel na mesma afinidade: o 40o incremento e bem
  // menor que o primeiro (delta/(1+n/8): 26 -> ~5).
  assert(g40.genero[0].n == 40);
  assert(farolgosto_peso(&g40, FG_DIM_GENERO, 18, 40) <
         farolgosto_peso(&g4, FG_DIM_GENERO, 18, 40));
}

static void despejo(void) {
  FarolGosto g;
  FarolGostoFicha f;
  int i;
  int32_t novo = 424242;
  farolgosto_iniciar(&g, 1);
  memset(&f, 0, sizeof f);
  f.tmdb = 1;
  // Enche os 32 slots: o slot 7 fica velho e fraco, os demais fortes.
  for (i = 0; i < FG_GENERO_N; i++) {
    f.genero[0] = 1000 + i;
    farolgosto_aplicar(&g, &f, i == 7 ? 16 : 512, i == 7 ? 1 : 10, -1, 0);
  }
  assert(g.genero[7].id == 1007);
  // Entra afinidade nova no dia 20: sai o mais fraco E mais velho.
  f.genero[0] = novo;
  farolgosto_aplicar(&g, &f, 256, 20, -1, 0);
  assert(farolgosto_peso(&g, FG_DIM_GENERO, novo, 20) == 256);
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 1007, 20) == 0);   // despejado
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 1000, 20) > 0);
}

// Bug da revisao FAROL37: sessoes diarias congelavam o decaimento — o toque
// aplicava fator 1 (intervalo < 1/16 de meia-vida) e ainda avancava a
// referencia, jogando o dia fora. Agora o resto se acumula: 45 sessoes
// diarias em outra faixa deixam a celula antiga na metade (meia-vida 45).
static void sessoesDiarias(void) {
  FarolGosto g, g2, g3;
  FarolGostoFicha f;
  int d;
  farolgosto_iniciar(&g, 1);
  memset(&f, 0, sizeof f);
  f.tmdb = 7;
  f.tipo = FG_TIPO_SERIE;
  f.minutos = 45;                       // faixa de duracao 1 -> duracao id 7
  // Celula velha no teto logico, escrita no dia 0: 45 sessoes diarias na
  // faixa 3 nao podem deixa-la congelada — meia-vida de hora e 45 dias.
  g.hora[0][1] = 8192;
  g.diaBase = 0;
  for (d = 1; d <= 45; d++)
    farolgosto_aplicar(&g, &f, 64, (uint16_t)d, 3, 0);
  assert(farolgosto_hora(&g, 0, 1, 45) >= 4014 &&
         farolgosto_hora(&g, 0, 1, 45) <= 4178);  // 4096 +- 2%
  // Mesmo conserto nos caminhos por-obra: afinidade tocada todo dia decai
  // como se o intervalo tivesse passado de uma vez (120 d -> metade).
  farolgosto_iniciar(&g2, 1);
  memset(&f, 0, sizeof f);
  f.genero[0] = 18;
  g2.genero[0].id = 18;
  g2.genero[0].peso = 8192;
  g2.genero[0].n = 1000;                // incremento diario arredonda a 0
  for (d = 1; d <= 120; d++)
    farolgosto_aplicar(&g2, &f, 8, (uint16_t)d, -1, 0);
  assert(farolgosto_peso(&g2, FG_DIM_GENERO, 18, 120) >= 4014 &&
         farolgosto_peso(&g2, FG_DIM_GENERO, 18, 120) <= 4178);
  // E na meia-vida curta de duracao (45 d), idem.
  farolgosto_iniciar(&g3, 1);
  f.tipo = FG_TIPO_SERIE;
  f.minutos = 45;
  g3.duracao[6].id = 7;
  g3.duracao[6].peso = 8192;
  g3.duracao[6].n = 1000;
  for (d = 1; d <= 45; d++)
    farolgosto_aplicar(&g3, &f, 8, (uint16_t)d, -1, 0);
  assert(farolgosto_peso(&g3, FG_DIM_DURACAO, 7, 45) >= 4014 &&
         farolgosto_peso(&g3, FG_DIM_DURACAO, 7, 45) <= 4178);
}

// Grava e rele o modelo no lugar — a saida e a reabertura do app entre
// sessoes — e confere que a ida e a volta saem byte a byte identicas.
static void recarrega(FarolGosto *g) {
  static char a[FAROL_GOSTO_TXT_MAX], b[FAROL_GOSTO_TXT_MAX];
  size_t n = farolgosto_escrever(g, a, sizeof a);
  assert(n > 0);
  assert(farolgosto_ler(g, a, n) == 0);
  assert(farolgosto_escrever(g, b, sizeof b) == n);
  assert(memcmp(a, b, n) == 0);
}

// Bug da revisao FAROL37 r2: o perfil e gravado na saida e relido na
// abertura, e o formato FG 1 nao guardava o resto do decaimento — um
// intervalo de um dia virava resto, a leitura restaurava o dia ja avancado
// e o pedaco sumia a cada recarga, congelando o peso para sempre. Com o
// resto no arquivo (FG 2), gravar/ler entre as sessoes tem de dar o MESMO
// resultado que o cenario todo em memoria (invariante de persistencia).
static void sessoesDiariasRecarga(void) {
  FarolGosto gm, gd;
  FarolGostoFicha f;
  int d;
  farolgosto_iniciar(&gm, 1);
  memset(&f, 0, sizeof f);
  f.tmdb = 7;
  f.tipo = FG_TIPO_SERIE;
  f.minutos = 45;                       // faixa 1 -> duracao id 7
  f.genero[0] = 18;
  // Celula de hora velha e afinidades no teto logico, com n alto para o
  // incremento diario arredondar a zero e isolar o decaimento.
  gm.hora[0][1] = 8192;
  gm.diaBase = 0;
  gm.duracao[0].id = 7;                 // slot 0: a leitura enche em ordem
  gm.duracao[0].peso = 8192;
  gm.duracao[0].n = 1000;
  gm.genero[0].id = 18;
  gm.genero[0].peso = 8192;
  gm.genero[0].n = 1000;
  gd = gm;
  for (d = 1; d <= 120; d++) {
    farolgosto_aplicar(&gm, &f, 8, (uint16_t)d, 3, 0);
    farolgosto_aplicar(&gd, &f, 8, (uint16_t)d, 3, 0);
    recarrega(&gd);
    // A recarga nao pode mudar NADA do modelo.
    assert(memcmp(&gd, &gm, sizeof gm) == 0);
    if (d == 45) {
      // 45 sessoes diarias com recarga: horario e duracao na metade.
      assert(farolgosto_hora(&gd, 0, 1, 45) >= 4014 &&
             farolgosto_hora(&gd, 0, 1, 45) <= 4178);  // 4096 +- 2%
      assert(farolgosto_peso(&gd, FG_DIM_DURACAO, 7, 45) >= 4014 &&
             farolgosto_peso(&gd, FG_DIM_DURACAO, 7, 45) <= 4178);
    }
  }
  // 120 sessoes diarias com recarga: genero na metade.
  assert(farolgosto_peso(&gd, FG_DIM_GENERO, 18, 120) >= 4014 &&
         farolgosto_peso(&gd, FG_DIM_GENERO, 18, 120) <= 4178);
}

// Contador de frio (plano 3.4): obras distintas pela chave da ficha, nao
// eventos. A 38 marca obraNova so no primeiro evento de cada chave.
static void titulosObras(void) {
  FarolGosto g;
  FarolGostoFicha f;
  int i;
  farolgosto_iniciar(&g, 1);
  memset(&f, 0, sizeof f);
  f.tmdb = 42;
  f.tipo = FG_TIPO_SERIE;
  f.minutos = 45;
  // Uma serie com 40 episodios = UMA obra (antes: titulos = 40, maduro).
  for (i = 0; i < 40; i++)
    farolgosto_aplicar(&g, &f, 26, (uint16_t)i, i % 8, i == 0);
  assert(g.titulos == 1);                        // nivel 0 (< 3 com ficha)
  // Duas fichas distintas — uma sem TMDB, so de catalogo/reserva: contam.
  f.tmdb = 500;
  farolgosto_aplicar(&g, &f, FG_D_TERMINOU, 41, 2, 1);
  f.tmdb = 0;                                    // ficha valida sem TMDB
  farolgosto_aplicar(&g, &f, FG_D_SALVOU, 41, 2, 1);
  assert(g.titulos == 3);                        // nivel 1 (3-9)
  // Evento neutro (delta 0) de obra nova tambem conta: a ficha existe.
  farolgosto_aplicar(&g, &f, 0, 42, 2, 1);
  assert(g.titulos == 4);
  // Mais 6 obras -> 10 = nivel maduro. Eventos repetidos nao somam.
  for (i = 0; i < 8; i++)
    farolgosto_aplicar(&g, &f, FG_D_INTERESSE, 43, 2, i < 6);
  assert(g.titulos == 10);
  // O contador volta identico da serializacao.
  {
    char buf[FAROL_GOSTO_TXT_MAX];
    FarolGosto g2;
    size_t n = farolgosto_escrever(&g, buf, sizeof buf);
    assert(n > 0);
    assert(farolgosto_ler(&g2, buf, n) == 0);
    assert(g2.titulos == 10);
  }
}

// Mutante que a revisao viu escapar: tirar a saturacao depois de 8
// evidencias tem de falhar aqui — o 9o incremento de delta 26 e
// 26*8/(8+8) = 13, nao 26.
static void saturacaoDepoisDeOito(void) {
  FarolGosto g;
  FarolGostoFicha f = fichaBase();
  int i;
  int32_t p8, inc;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < 8; i++)
    farolgosto_aplicar(&g, &f, 26, 0, -1, 0);
  p8 = farolgosto_peso(&g, FG_DIM_GENERO, 18, 0);
  farolgosto_aplicar(&g, &f, 26, 0, -1, 0);
  inc = farolgosto_peso(&g, FG_DIM_GENERO, 18, 0) - p8;
  assert(inc >= 12 && inc <= 14);
  // E o 40o: 26*8/48 = 4,33 -> 4.
  for (i = 9; i < 40; i++)
    farolgosto_aplicar(&g, &f, 26, 0, -1, 0);
  p8 = farolgosto_peso(&g, FG_DIM_GENERO, 18, 0);
  farolgosto_aplicar(&g, &f, 26, 0, -1, 0);
  inc = farolgosto_peso(&g, FG_DIM_GENERO, 18, 0) - p8;
  assert(inc >= 3 && inc <= 5);
}

// Segundo mutante da revisao: inverter o desempate por idade no despejo.
// Em empate de |peso projetado| sai o MAIS VELHO — aqui o slot 0 projeta
// 100 como os demais (peso 104 decomposto por 1/16 de meia-vida) e e o
// unico com dia 1.
static void despejoIdade(void) {
  FarolGosto g;
  FarolGostoFicha f;
  int i;
  farolgosto_iniciar(&g, 1);
  for (i = 0; i < FG_GENERO_N; i++) {
    g.genero[i].id = 1000 + i;
    g.genero[i].peso = i == 0 ? 104 : 100;
    g.genero[i].n = 1;
    g.genero[i].dia = i == 0 ? 1 : 6;
  }
  memset(&f, 0, sizeof f);
  f.tmdb = 1;
  f.genero[0] = 4242;
  farolgosto_aplicar(&g, &f, 256, 10, -1, 0);
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 4242, 10) == 256);
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 1000, 10) == 0);   // despejado
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 1001, 10) > 0);    // resta
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 1031, 10) > 0);
}

static void aneis(void) {
  FarolGosto g;
  int i;
  farolgosto_iniciar(&g, 1);
  // recusado: anel de 128 — os 12 mais velhos saem; dedup nao reconta.
  for (i = 1; i <= 140; i++) farolgosto_recusar(&g, (uint32_t)i * 7);
  assert(g.recProx == 140);
  assert(!farolgosto_recusado(&g, 7));              // i=1: saiu do anel
  assert(!farolgosto_recusado(&g, 84));             // i=12: saiu
  assert(farolgosto_recusado(&g, 91));              // i=13: o mais velho que ficou
  assert(farolgosto_recusado(&g, 980));             // i=140
  farolgosto_recusar(&g, 980);                      // duplicado: nao reconta
  assert(g.recProx == 140);
  // mostrado: so conta dia diferente.
  farolgosto_mostrado(&g, 55, 10);
  farolgosto_mostrado(&g, 55, 10);
  farolgosto_mostrado(&g, 55, 11);
  farolgosto_mostrado(&g, 55, 12);
  assert(farolgosto_mostrado_dias(&g, 55) == 3);
  assert(farolgosto_mostrado_dias(&g, 56) == 0);
}

static void idaEVolta(void) {
  FarolGosto g, g2, g3;
  FarolGostoFicha f = fichaBase();
  char buf[FAROL_GOSTO_TXT_MAX], buf2[FAROL_GOSTO_TXT_MAX];
  size_t n, n2;
  int i;
  farolgosto_iniciar(&g, 3);
  for (i = 0; i < 60; i++) {
    f.tmdb = 100 + i;
    f.genero[0] = 18 + (i % 5);
    f.pessoa[0] = 900 + i;
    f.tema[0] = 5000 + (i % 7);
    f.ano = 1950 + i;
    farolgosto_aplicar(&g, &f, (i % 3) * 128 - 64, (uint16_t)(i % 30), i % 8,
                       1);
    farolgosto_recusar(&g, (uint32_t)(i * 31 + 1));
    farolgosto_mostrado(&g, (uint32_t)(i * 17 + 3), (uint8_t)(i % 60));
  }
  n = farolgosto_escrever(&g, buf, sizeof buf);
  assert(n > 0 && n < sizeof buf);
  buf[n] = '\0';
  // Ida e volta identica.
  assert(farolgosto_ler(&g2, buf, n) == 0);
  n2 = farolgosto_escrever(&g2, buf2, sizeof buf2);
  assert(n2 == n);
  assert(memcmp(buf, buf2, n) == 0);
  // Determinismo de parse.
  assert(farolgosto_ler(&g3, buf, n) == 0);
  assert(memcmp(&g2, &g3, sizeof g2) == 0);
  // Conteudo sobrevive: pesos projetados iguais.
  assert(farolgosto_peso(&g, FG_DIM_GENERO, 18, 60) ==
         farolgosto_peso(&g2, FG_DIM_GENERO, 18, 60));
  assert(g2.perfil == 3 && g2.titulos == g.titulos);
  assert(farolgosto_recusado(&g2, 1));
  assert(farolgosto_mostrado_dias(&g2, 3) == 1);
  // Versao desconhecida: arquivo ignorado, *g intacto.
  memcpy(buf2, buf, n);
  buf2[3] = '9';
  {
    FarolGosto antes = g;
    assert(farolgosto_ler(&g, buf2, n) == -1);
    assert(memcmp(&antes, &g, sizeof g) == 0);
  }
  // Sem cabecalho tambem e ignorado.
  assert(farolgosto_ler(&g, "g 18 256 1 10\n", 14) == -1);
  // Linha torta e pulada, o resto carrega. O formato antigo (FG 1) segue
  // valido: sem 5o campo, os restos de decaimento entram em 0.
  {
    const char *txt = "FG 1 5 2 100\ng 18 256 3 90\n"
                      "g torta\np 7 -256 1 90\nh 2 1 2 3 4 5\n";
    assert(farolgosto_ler(&g3, txt, strlen(txt)) == 0);
    // peso 256 no dia 90, projetado no dia 100: 256 x 2^(-10/120) ~= 245.
    assert(farolgosto_peso(&g3, FG_DIM_GENERO, 18, 100) >= 243 &&
           farolgosto_peso(&g3, FG_DIM_GENERO, 18, 100) <= 247);
    assert(farolgosto_peso(&g3, FG_DIM_PESSOA, 7, 100) <= -243 &&
           farolgosto_peso(&g3, FG_DIM_PESSOA, 7, 100) >= -247);
    assert(g3.hora[2][4] == 5 && g3.diaBase == 100);
    assert(g3.horaResto == 0 && g3.restoGenero[0] == 0);
  }
  // FG 2: o 5o campo do cabecalho e das linhas de afinidade e o resto do
  // decaimento (< meia-vida; fora de faixa, prende). Restaurado, ele mantem
  // o decaimento em curso — e por isso a recarga diaria nao congela mais.
  {
    const char *txt = "FG 2 5 2 100 9\ng 18 256 1000 90 17\n"
                      "u 7 8192 4 90 999\n";
    assert(farolgosto_ler(&g3, txt, strlen(txt)) == 0);
    assert(g3.horaResto == 9);
    assert(g3.restoGenero[0] == 17);
    assert(g3.restoDuracao[0] == FG_MEIA_HORA - 1);   // 999 prendeu
    assert(g3.genero[0].peso == 256 && g3.genero[0].dia == 90);
    // O resto restaurado conta no proximo toque: 17 + 16 = 33 avos ainda
    // nao fecham 1/16 de 120 dias; n alto faz o incremento arredondar a 0.
    memset(&f, 0, sizeof f);
    f.genero[0] = 18;
    farolgosto_aplicar(&g3, &f, 8, 91, -1, 0);
    assert(g3.genero[0].peso == 256 && g3.restoGenero[0] == 33);
    farolgosto_aplicar(&g3, &f, 8, 96, -1, 0);        // +80: fecha 113/120
    assert(g3.restoGenero[0] == 113);
    farolgosto_aplicar(&g3, &f, 8, 97, -1, 0);        // +16: 129 -> decai 1/16
    assert(g3.restoGenero[0] == 9);
    assert(g3.genero[0].peso < 256 && g3.genero[0].peso > 240);
  }
  // Nome do arquivo.
  {
    char nome[64];
    assert(farolgosto_nome(2, nome, sizeof nome) == 0);
    assert(strcmp(nome, "farol-gosto-p2.txt") == 0);
    assert(farolgosto_nome(2, nome, 8) == -1);
  }
}

// xorshift deterministico: nada de rand() para o teste ser reproduzivel.
static uint64_t xz;
static uint32_t rnd(void) {
  xz ^= xz << 13; xz ^= xz >> 7; xz ^= xz << 17;
  return (uint32_t)xz;
}

static void aleatorios(void) {
  FarolGosto g;
  int i;
  uint16_t dia = 0;
  farolgosto_iniciar(&g, 4);
  xz = 0x9e3779b97f4a7c15ULL;
  for (i = 0; i < 10000; i++) {
    FarolGostoFicha f;
    int j;
    memset(&f, 0, sizeof f);
    f.tmdb = (int32_t)rnd();
    f.tipo = rnd() & 1;
    f.ano = (int16_t)(rnd() % 250 - 40);           // <1920 e >2030 inclusos
    f.idioma = (int32_t)(rnd() & 0xffffff);
    if (rnd() % 5 == 0) f.idioma = 0;
    f.minutos = (int16_t)(rnd() % 300 - 20);       // <=0 inclusos
    for (j = 0; j < 4; j++) f.genero[j] = (int32_t)(rnd() % 40);
    for (j = 0; j < 6; j++) f.pessoa[j] = (int32_t)(rnd() % 300) - 150;
    for (j = 0; j < 8; j++) f.tema[j] = (int32_t)(rnd() % 60);
    // dia quase sempre anda; as vezes repete ou volta (relogio torto).
    if (rnd() % 11) dia++;
    else if (rnd() % 7 == 0 && dia > 40) dia -= 20;
    {
      int delta = (int)(rnd() % 2049) - 1024;      // ate +-4,0
      int fh = (int)(rnd() % 12) - 2;              // fora de faixa inclusos
      farolgosto_aplicar(&g, &f, delta, dia, fh, rnd() % 5 == 0);
      if (rnd() % 9 == 0) farolgosto_recusar(&g, rnd() % 9000);
      if (rnd() % 9 == 0)
        farolgosto_mostrado(&g, rnd() % 7000, (uint8_t)rnd());
    }
  }
  // Ida e volta no estado caotico.
  {
    static char buf[FAROL_GOSTO_TXT_MAX], buf2[FAROL_GOSTO_TXT_MAX];
    FarolGosto g2;
    size_t n = farolgosto_escrever(&g, buf, sizeof buf);
    size_t n2;
    assert(n > 0);
    assert(farolgosto_ler(&g2, buf, n) == 0);
    n2 = farolgosto_escrever(&g2, buf2, sizeof buf2);
    assert(n2 == n && memcmp(buf, buf2, n) == 0);
  }
  // Projecao num dia distante nao quebra (sh grande -> fator 0).
  assert(pesoTotal(&g, 60000) >= 0);
}

int main(void) {
  _Static_assert(sizeof(FarolGosto) <= 6144, "FarolGosto > 6144");
  printf("sizeof(FarolGosto) = %u\n", (unsigned)sizeof(FarolGosto));
  decaimento();
  sessoesDiarias();
  sessoesDiariasRecarga();
  titulosObras();
  saturacao();
  saturacaoDepoisDeOito();
  despejo();
  despejoIdade();
  aneis();
  idaEVolta();
  aleatorios();
  return 0;
}
