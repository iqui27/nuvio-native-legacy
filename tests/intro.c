// LEITURA DOS MARCADORES DO TheIntroDB.
//
// As cargas abaixo sao RESPOSTAS REAIS da API, capturadas com curl antes de o
// leitor existir — nao invencao minha. Sao elas que fixam as tres coisas que
// mudaram junto com a fonte e que dariam erro silencioso se trocadas:
//
//   1. as chaves valem ARRAY e nao objeto (um tipo pode ter varios trechos);
//   2. os tempos vem em MILISSEGUNDOS (segundos daria um numero mil vezes
//      errado sem parecer errado);
//   3. `null` tem significado: start = 0, end = "ate o fim da midia".
//
//   bash tests/intro.sh
#include "../src/intro.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// DUBLE DE REDE. intro.c chama rede_baixar no fio de download; este teste so
// exercita o LEITOR, entao o duble existe para linkar e nada mais. Devolver
// NULL e o comportamento certo caso alguem chame intro_pedir aqui por engano:
// zero marcadores, sem rede.
char *rede_baixar(const char *url, int segundos) {
  (void)url; (void)segundos; return NULL;
}

static const IntroTrecho *achar(const IntroTrecho *v, int n, int tipo) {
  int i;
  for (i = 0; i < n; i++) if (v[i].tipo == tipo) return &v[i];
  return NULL;
}

// Compara em milissegundos para a assercao nao depender de ponto flutuante.
static int mesmoSeg(double a, double esperado) {
  double d = a - esperado;
  return d < 0.001 && d > -0.001;
}

int main(void) {
  IntroTrecho v[8];
  int n;

  // FILME: /v3/media?imdb_id=tt0111161 (Shawshank). So creditos, sem fim.
  n = intro_extrair("{\"tmdb_id\":278,\"type\":\"movie\","
                    "\"credits\":[{\"start_ms\":8300000,\"end_ms\":null}]}", v, 8);
  assert(n == 1);
  { const IntroTrecho *c = achar(v, n, INTRO_CREDITOS);
    assert(c);
    assert(mesmoSeg(c->inicio, 8300.0));   // 8.300.000 ms, e nao 8.300.000 s
    // fim ZERO e o contrato de "ate o fim da midia" (end_ms nulo). Sem isto,
    // um fim 0 seria lido como trecho vazio e o marcador sumiria.
    assert(c->fim == 0.0); }
  puts("ok  filme: creditos em milissegundos, fim nulo vira 'ate o fim'");

  // FILME com abertura e SEM creditos: /v3/media?imdb_id=tt0816692
  // (Interstellar). start_ms nulo = comeca no zero.
  n = intro_extrair("{\"tmdb_id\":157336,\"type\":\"movie\","
                    "\"intro\":[{\"start_ms\":null,\"end_ms\":53000}]}", v, 8);
  assert(n == 1);
  { const IntroTrecho *a = achar(v, n, INTRO_ABERTURA);
    assert(a && mesmoSeg(a->inicio, 0.0) && mesmoSeg(a->fim, 53.0));
    assert(!achar(v, n, INTRO_CREDITOS)); }
  puts("ok  filme: start nulo vira zero, e ausencia de creditos nao inventa um");

  // SERIE: /v3/media?imdb_id=tt14688458&season=1&episode=1 (Silo T1E1).
  n = intro_extrair("{\"tmdb_id\":125988,\"type\":\"tv\",\"season\":1,\"episode\":1,"
                    "\"intro\":[{\"start_ms\":272500,\"end_ms\":366000}],"
                    "\"credits\":[{\"start_ms\":3503000,\"end_ms\":null}]}", v, 8);
  assert(n == 2);
  { const IntroTrecho *a = achar(v, n, INTRO_ABERTURA);
    const IntroTrecho *c = achar(v, n, INTRO_CREDITOS);
    assert(a && mesmoSeg(a->inicio, 272.5) && mesmoSeg(a->fim, 366.0));
    assert(c && mesmoSeg(c->inicio, 3503.0)); }
  puts("ok  serie: abertura e creditos na mesma resposta");

  // MAIS DE UM TRECHO DO MESMO TIPO — e a razao de a chave ser array. Um leitor
  // que so pegasse o primeiro elemento passaria nos casos acima e falharia aqui.
  n = intro_extrair("{\"type\":\"tv\",\"intro\":["
                    "{\"start_ms\":1000,\"end_ms\":2000},"
                    "{\"start_ms\":5000,\"end_ms\":6000}]}", v, 8);
  assert(n == 2);
  assert(mesmoSeg(v[0].inicio, 1.0) && mesmoSeg(v[1].inicio, 5.0));
  puts("ok  duas aberturas no mesmo episodio entram as duas");

  // RESUMO tem tipo proprio: o player pula abertura e resumo, mas o posplay so
  // se importa com creditos. Trocar os tipos faria o painel subir na abertura.
  n = intro_extrair("{\"type\":\"tv\",\"recap\":[{\"start_ms\":0,\"end_ms\":30000}]}", v, 8);
  assert(n == 1 && v[0].tipo == INTRO_RESUMO);
  puts("ok  recap vira INTRO_RESUMO e nao abertura");

  // TRECHO INVERTIDO OU VAZIO NAO ENTRA. Vem de dado errado na base, e um
  // trecho com fim <= inicio deixaria intro_ativo verdadeiro para sempre.
  n = intro_extrair("{\"type\":\"tv\",\"intro\":["
                    "{\"start_ms\":9000,\"end_ms\":9000},"
                    "{\"start_ms\":9000,\"end_ms\":1000}]}", v, 8);
  assert(n == 0);
  puts("ok  trecho vazio ou invertido e descartado");

  // O QUE A API RESPONDE QUANDO NAO CONHECE O TITULO, e o resto do lixo.
  assert(intro_extrair("{\"error\":\"media not found\"}", v, 8) == 0);
  assert(intro_extrair("{}", v, 8) == 0);
  assert(intro_extrair("nao e json", v, 8) == 0);
  assert(intro_extrair("", v, 8) == 0);
  assert(intro_extrair(NULL, v, 8) == 0);
  assert(intro_extrair("{\"credits\":[{\"start_ms\":1000,\"end_ms\":2000}]}", v, 0) == 0);
  puts("ok  404, vazio, lixo e max=0 devolvem zero sem estourar");

  puts("intro: tudo ok");
  return 0;
}
