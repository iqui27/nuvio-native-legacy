// #421: colheita sintetica de texto, pelo mesmo parser das legendas externas.
// Transporte local de Range: nenhum socket, parser/extrator reais.
#include "mkvass.h"
#include "legenda.h"
#include "dados.h"
#include <assert.h>
int xtream_url_limitada(const char *url) { (void)url; return 0; }
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char esperado[4096] = "https://example.test/texto.mkv";
static const char *arquivo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int pedidos, semRange;
char *rede_baixar(const char *url, int s) { (void)url; (void)s; return NULL; }
char *rede_baixar_bin(const char *url, int s, long *n) { (void)url; (void)s; (void)n; return NULL; }
// O mkvass pede por rede_baixar_trecho_st (status, erro e url final); o
// transporte local responde como um servidor sem redirecionamento: a url
// final e a propria. semRange simula HTTP 200 com o arquivo desde o byte 0.
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  (void)url; snprintf(dst, tam, "local"); return dst;
}
long rede_corte_host(const char *url) { (void)url; return 0; }
int rede_resto_recusado(void) { return 0; }
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *status, int *erro, char *final, unsigned tamFinal) {
  FILE *f; char *p; long n;
  (void)s; *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) snprintf(final, tamFinal, "%s", url);
  pthread_mutex_lock(&trava);
  pedidos++;
  assert(!strcmp(url, esperado));
  pthread_mutex_unlock(&trava);
  f = fopen(arquivo, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f);
  if (semRange) { ini = 0; fim = n - 1; }
  if (ini >= n) { fclose(f); return NULL; }
  if (fim >= n) fim = n - 1;
  n = fim - ini + 1;
  p = malloc((size_t)n); assert(p);
  fseek(f, ini, SEEK_SET);
  assert(fread(p, 1, (size_t)n, f) == (size_t)n);
  fclose(f); *tam = n; if (status) *status = semRange ? 200 : 206; return p;
}
static void parar(void) {
  mkvass_parar();
  for (int i = 0; mkvass_ocupado() && i < 2000; i++) usleep(10000);
  assert(!mkvass_ocupado());
}
static int escolher(const char *const *idiomas, int n) {
  assert(n == 1 && !strcmp(idiomas[0], "ara")); return 0;
}
static void conferir(void) {
  const char *srt = "1\n00:00:01,250 --> 00:00:03,000\n<i>مرحبا بالعالم</i>\n<b>Olá &amp; TV</b>\n\n2\n00:00:05,000 --> 00:00:06,250\n<b>نص ثان</b>\n";
  LegendaCue *externa = NULL, cue[4];
  assert(legenda_extrair_srt(srt, &externa) == 2);
  for (int i = 0; i < 2; i++) {
    assert(legenda_cues(externa[i].inicio + .1, 0, cue, 4) == 1);
    assert(cue[0].inicio == externa[i].inicio && cue[0].fim == externa[i].fim);
    assert(!strcmp(cue[0].texto, externa[i].texto));
  }
  free(externa);
}
int main(int argc, char **argv) {
  assert(argc == 3); dados_iniciar(".");
  for (int j = 1; j < argc; j++) {
    arquivo = argv[j]; snprintf(esperado, sizeof esperado, "https://example.test/texto%d.mkv", j);
    legenda_desligar();
    // Prova #421: a pre-busca deve colher texto sem ativacao especifica do .tpk.
    assert(mkvass_prebuscar(esperado, escolher, 0));
    // #9 da revisao: faixa de TEXTO solta o player no cabecalho (fase sai de 1
    // ANTES da colheita); os cues chegam em segundo plano. Esperar fechar 2/2.
    for (int i = 0; mkvass_prebusca_fase() == 1 && i < 2000; i++) usleep(10000);
    assert(mkvass_prebusca_fase() == 2);
    int col = 0, total = 0;
    for (int i = 0; i < 2000; i++) {
      mkvass_estatisticas(NULL, NULL, &col, &total);
      if (total == 2 && col == 2) break;
      usleep(10000);
    }
    assert(col == 2 && total == 2);
    assert(!legenda_ligada_em(legenda_geracao()));
    mkvass_iniciar_ordinal(esperado, 0);
    for (int i = 0; mkvass_estado() != MKVASS_COMPLETO && i < 2000; i++) usleep(10000);
    assert(mkvass_estado() == MKVASS_COMPLETO); conferir(); parar();
    unsigned long long hash = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)esperado; *p; p++) hash = (hash ^ *p) * 1099511628211ULL;
    char nome[80]; snprintf(nome, sizeof nome, "mkvass-%016llx--1.ass", hash);
    char *sc = dados_ler(nome);
    assert(sc && strstr(sc, "<i>مرحبا بالعالم</i>") && strstr(sc, "<b>Olá &amp; TV</b>")); free(sc);
    // Segunda abertura usa o mesmo sidecar SRT, preservando tags e UTF-8.
    legenda_desligar(); int antes = pedidos;
    mkvass_iniciar_ordinal(esperado, 0);
    for (int i = 0; mkvass_estado() != MKVASS_COMPLETO && i < 2000; i++) usleep(10000);
    assert(mkvass_estado() == MKVASS_COMPLETO); conferir(); parar();
    assert(pedidos == antes);
  }
  legenda_desligar(); semRange = 1;
  snprintf(esperado, sizeof esperado, "https://example.test/sem-range.mkv");
  mkvass_iniciar_ordinal(esperado, 0);
  for (int i = 0; !mkvass_nogo() && i < 2000; i++) usleep(10000);
  assert(mkvass_estado() == MKVASS_NOGO_SEM_RANGE);
  assert(!legenda_ligada_em(legenda_geracao())); parar();
  puts("mkvass_texto: UTF8/WEBVTT pre-busca, adocao, tempos, arabe, tags e sidecar ok");
}
