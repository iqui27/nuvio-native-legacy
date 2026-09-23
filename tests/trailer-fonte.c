// A ESCOLHA DA FONTE DO TRAILER (trailerfonte.c), sem rede, sem tela e com as
// DUAS plataformas no mesmo binario: `tizen` e argumento, nao #ifdef, entao o
// que a Samsung faria e provado no Mac tambem.
//
// Cobre as tres decisoes do dono de 22/09/2026:
//   - Samsung nunca pede trailer com som (e o OK em tela cheia nao troca de
//     fonte por causa de som);
//   - cada valor de "Fonte do trailer" tenta SO a fonte dele;
//   - Automatico mantem a ordem de hoje, Apple -> IMDb -> YouTube, esperando a
//     Apple responder antes de passar a vez.
#include "../src/trailerfonte.h"
#include <stdio.h>
#include <string.h>

// trailerfonte.c le o ajuste por esta funcao; aqui ela e um numero do teste.
static int ajusteTeste;
int ajustes_trailer_fonte(void) { return ajusteTeste; }

static int falhas;
static void confere(const char *nome, int ok) {
  printf("%s %s\n", ok ? "ok " : "FALHOU", nome);
  if (!ok) falhas++;
}

// Todas as fontes com trailer e ja respondidas.
static TrailerCandidatos todas(void) {
  TrailerCandidatos c;
  memset(&c, 0, sizeof c);
  c.apple = "https://apple/v.m3u8"; c.appleRespondeu = 1;
  c.imdb = "https://imdb/v.mp4";    c.imdbRespondeu = 1;
  c.youtube = "dQw4w9WgXcQ";        c.youtubeRespondeu = 1;
  return c;
}

static int escolhe(int aj, int tz, const TrailerCandidatos *c, const char **u) {
  int q = -1; const char *x = NULL;
  TrailerDecisao d = trailerfonte_escolher(aj, tz, c, &x, &q);
  if (u) *u = x;
  return d == TRF_ABRE ? q : d == TRF_ESPERA ? -1 : 0;
}

int main(void) {
  int o[3], n, tz;
  TrailerCandidatos c;
  const char *u;

  // --- Som
  confere("Samsung nunca pede com som", trailerfonte_com_som(1) == 0);
  confere("LG continua com som na tela cheia", trailerfonte_com_som(0) == 1);

  // --- Automatico: a ordem de hoje, filtrada pelo que a TV toca
  n = trailerfonte_ordem(TRF_AUTO, 0, o);
  confere("LG automatico: Apple -> IMDb", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_IMDB);
  n = trailerfonte_ordem(TRF_AUTO, 1, o);
  confere("Samsung automatico: Apple -> YouTube", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_YOUTUBE);
  n = trailerfonte_ordem(99, 1, o);
  confere("valor desconhecido le como automatico", n == 2 && o[0] == TRF_APPLE);

  for (tz = 0; tz <= 1; tz++) {
    char nome[120];
    c = todas();
    snprintf(nome, sizeof nome, "%s automatico: Apple primeiro com todas", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_AUTO, tz, &c, &u) == TRF_APPLE && !strcmp(u, c.apple));

    // A Apple ainda nao respondeu: a proxima NAO ganha por chegar antes.
    c = todas(); c.apple = NULL; c.appleRespondeu = 0;
    snprintf(nome, sizeof nome, "%s automatico: espera a Apple responder", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_AUTO, tz, &c, NULL) == -1);

    // A Apple respondeu sem trailer: passa a vez a proxima da plataforma.
    c.appleRespondeu = 1;
    snprintf(nome, sizeof nome, "%s automatico: Apple vazia cede a vez", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_AUTO, tz, &c, &u) == (tz ? TRF_YOUTUBE : TRF_IMDB));

    // A Apple deu erro no elemento: mesma coisa, sem reabrir a mesma URL.
    c = todas(); c.appleFalhou = 1;
    snprintf(nome, sizeof nome, "%s automatico: Apple com erro cede a vez", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_AUTO, tz, &c, &u) == (tz ? TRF_YOUTUBE : TRF_IMDB));
    snprintf(nome, sizeof nome, "%s automatico: depois da Apple vem a proxima", tz ? "Samsung" : "LG");
    confere(nome, trailerfonte_depois(TRF_AUTO, tz, TRF_APPLE) == (tz ? TRF_YOUTUBE : TRF_IMDB));

    // Ninguem tem: sem trailer (e nao espera para sempre).
    memset(&c, 0, sizeof c);
    c.appleRespondeu = c.imdbRespondeu = c.youtubeRespondeu = 1;
    snprintf(nome, sizeof nome, "%s automatico: ninguem tem, sem trailer", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_AUTO, tz, &c, NULL) == 0);

    // --- Fonte fixa: SO ela
    c = todas();
    snprintf(nome, sizeof nome, "%s Apple fixa: abre Apple", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_APPLE, tz, &c, &u) == TRF_APPLE);
    c.apple = NULL;
    snprintf(nome, sizeof nome, "%s Apple fixa sem Apple: sem trailer, nada de reserva", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_APPLE, tz, &c, NULL) == 0);
    c = todas(); c.appleFalhou = 1;
    snprintf(nome, sizeof nome, "%s Apple fixa com erro: sem proxima", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_APPLE, tz, &c, NULL) == 0 && trailerfonte_depois(TRF_APPLE, tz, TRF_APPLE) == 0);

    c = todas();
    snprintf(nome, sizeof nome, "%s IMDb fixo: %s", tz ? "Samsung" : "LG", tz ? "nao toca nesta TV" : "abre IMDb, pula a Apple");
    confere(nome, escolhe(TRF_IMDB, tz, &c, &u) == (tz ? 0 : TRF_IMDB));
    c.appleRespondeu = 0; c.apple = NULL;
    snprintf(nome, sizeof nome, "%s IMDb fixo nao espera a Apple", tz ? "Samsung" : "LG");
    confere(nome, escolhe(TRF_IMDB, tz, &c, NULL) == (tz ? 0 : TRF_IMDB));

    c = todas();
    snprintf(nome, sizeof nome, "%s YouTube fixo: %s", tz ? "Samsung" : "LG", tz ? "abre YouTube, pula a Apple" : "nao toca nesta TV");
    confere(nome, escolhe(TRF_YOUTUBE, tz, &c, &u) == (tz ? TRF_YOUTUBE : 0));
    if (tz) confere("Samsung YouTube fixo entrega o id", !strcmp(u, "dQw4w9WgXcQ"));
    c.youtube = NULL; c.youtubeRespondeu = 0;
    snprintf(nome, sizeof nome, "%s YouTube fixo ainda sem lista: %s", tz ? "Samsung" : "LG", tz ? "espera" : "sem trailer");
    confere(nome, escolhe(TRF_YOUTUBE, tz, &c, NULL) == (tz ? -1 : 0));

    for (n = TRF_APPLE; n <= TRF_YOUTUBE; n++) {
      int k = trailerfonte_ordem(n, tz, o);
      snprintf(nome, sizeof nome, "%s ajuste %d: no maximo uma fonte, e a dele", tz ? "Samsung" : "LG", n);
      confere(nome, k <= 1 && (k == 0 || o[0] == n));
    }
  }

  // Atalho do build: le o ajuste gravado.
  ajusteTeste = TRF_YOUTUBE;
  confere("trailerfonte_ajuste le o ajuste", trailerfonte_ajuste() == TRF_YOUTUBE);

  puts(falhas ? "trailer-fonte: FALHOU" : "trailer-fonte: tudo ok");
  return falhas ? 1 : 0;
}
