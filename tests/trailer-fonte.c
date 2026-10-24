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
// E a do #178 (29/09/2026): no .tpk o botao Trailer (tela cheia, com som) em
// Automatico tenta o IMDb antes da Apple, que la toca so video; o fundo e o
// .wgt/LG nao mudam.
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
  trailerfonte_definir_imdb_primeiro_destaque(1);
  confere("native Home fallback order follows IMDb -> Apple",
    trailerfonte_depois_destaque(TRF_AUTO,0,TRF_IMDB)==TRF_APPLE &&
    trailerfonte_depois_destaque(TRF_AUTO,0,TRF_APPLE)==0);
  confere("native Home explicit source has no implicit fallback",
    trailerfonte_depois_destaque(TRF_IMDB,0,TRF_IMDB)==0 &&
    trailerfonte_depois_destaque(TRF_APPLE,0,TRF_APPLE)==0);
  trailerfonte_definir_imdb_primeiro_destaque(0);
  confere("LG/WGT Home fallback retains Apple -> IMDb",
    trailerfonte_depois_destaque(TRF_AUTO,0,TRF_APPLE)==TRF_IMDB);
#ifdef NV_TPK
  trailerfonte_definir_imdb_primeiro_destaque(NV_TRAILER_CONTINUA_DETALHE);
#endif

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

  // --- Samsung COM o servico de recomendacoes (#136): o IMDb passa a existir
  // la, entre a Apple e o YouTube — tudo no <video> do app antes do iframe.
  confere("sem NV_REC_URL no teste, IMDb fora da Samsung", trailerfonte_imdb_tizen() == 0);
  trailerfonte_definir_imdb_tizen(1);
  n = trailerfonte_ordem(TRF_AUTO, 1, o);
  confere("Samsung com servico: Apple -> IMDb -> YouTube",
          n == 3 && o[0] == TRF_APPLE && o[1] == TRF_IMDB && o[2] == TRF_YOUTUBE);
  n = trailerfonte_ordem(TRF_AUTO, 0, o);
  confere("LG nao muda: Apple -> IMDb", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_IMDB);
  c = todas(); c.apple = NULL;
  confere("Samsung com servico: sem Apple, IMDb antes do YouTube", escolhe(TRF_AUTO, 1, &c, &u) == TRF_IMDB && !strcmp(u, c.imdb));
  c = todas(); c.appleFalhou = 1;
  confere("Samsung com servico: Apple com erro cede ao IMDb", escolhe(TRF_AUTO, 1, &c, &u) == TRF_IMDB);
  c = todas(); c.apple = NULL; c.imdb = NULL;
  confere("Samsung com servico: sem Apple e sem IMDb, YouTube", escolhe(TRF_AUTO, 1, &c, &u) == TRF_YOUTUBE);
  c = todas(); c.apple = NULL; c.imdb = NULL; c.imdbRespondeu = 0;
  confere("Samsung com servico: IMDb ainda sem resposta segura o YouTube", escolhe(TRF_AUTO, 1, &c, NULL) == -1);
  confere("Samsung com servico: depois da Apple o IMDb, depois do IMDb o YouTube",
          trailerfonte_depois(TRF_AUTO, 1, TRF_APPLE) == TRF_IMDB &&
          trailerfonte_depois(TRF_AUTO, 1, TRF_IMDB) == TRF_YOUTUBE &&
          trailerfonte_depois(TRF_AUTO, 1, TRF_YOUTUBE) == 0);
  c = todas();
  confere("Samsung com servico: IMDb fixo toca", escolhe(TRF_IMDB, 1, &c, &u) == TRF_IMDB);
  trailerfonte_definir_imdb_tizen(0);

  // --- TELA CHEIA COM SOM (#178). Sem NV_TPK no teste o desvio fica
  // desligado: .wgt e LG escolhem na tela cheia exatamente como no fundo.
#ifdef NV_TPK
  confere("build NV_TPK: IMDb-primeiro da tela cheia ligado por padrao", trailerfonte_imdb_primeiro_cheia() == 1);
#else
  confere("sem NV_TPK: IMDb-primeiro da tela cheia desligado por padrao", trailerfonte_imdb_primeiro_cheia() == 0);
#endif
  trailerfonte_definir_imdb_primeiro_cheia(1);
  c = todas(); c.appleIdiomaPreferido = 1;
  confere("localized Apple beats English IMDb on native TPK fullscreen",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_APPLE);
  confere("localized Apple respects a fixed IMDb provider",
          escolhe(TRF_IMDB, 0, &c, &u) == TRF_IMDB);
  c.appleIdiomaPreferido = -1; c.apple = NULL; c.appleRespondeu = 0;
  confere("wait for localized Apple before native TPK English default",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ESPERA);
  c.appleRespondeu = 1;
  confere("localized lookup deadline releases English fallback",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_IMDB);
  c = todas(); c.appleIdiomaPreferido = 1; c.appleFalhou = 1;
  confere("failed localized Apple releases fallback",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_IMDB);
  trailerfonte_definir_imdb_primeiro_cheia(0);   // o que o .wgt e a LG compilam
  for (tz = 0; tz <= 1; tz++) {
    int aj, a[3], b[3], na, nb, k, igual;
    char nome[120];
    for (aj = TRF_AUTO; aj <= TRF_YOUTUBE; aj++) {
      na = trailerfonte_ordem(aj, tz, a);
      nb = trailerfonte_ordem_cheia(aj, tz, trailerfonte_com_som(tz), b);
      igual = na == nb;
      for (k = 0; igual && k < na; k++) igual = a[k] == b[k];
      snprintf(nome, sizeof nome, "%s ajuste %d: tela cheia na mesma ordem do fundo", tz ? "Samsung wgt" : "LG", aj);
      confere(nome, igual);
    }
    c = todas();
    snprintf(nome, sizeof nome, "%s tela cheia automatico: Apple primeiro, como hoje", tz ? "Samsung wgt" : "LG");
    confere(nome, trailerfonte_escolher_cheia(TRF_AUTO, tz, trailerfonte_com_som(tz), &c, &u, &n) == TRF_ABRE && n == TRF_APPLE);
  }

  // .tpk (NV_TPK): trailerfonte_tizen() == 0 la, entao a plataforma e a da
  // LG (Apple -> IMDb, sem YouTube) e a tela cheia tem som.
  trailerfonte_definir_imdb_primeiro_cheia(1);
  confere(".tpk: tela cheia tem som", trailerfonte_com_som(0) == 1);
  n = trailerfonte_ordem_cheia(TRF_AUTO, 0, 1, o);
  confere(".tpk tela cheia automatico: IMDb -> Apple", n == 2 && o[0] == TRF_IMDB && o[1] == TRF_APPLE);
  n = trailerfonte_ordem_cheia(99, 0, 1, o);
  confere(".tpk tela cheia valor desconhecido: le como automatico, IMDb primeiro", n == 2 && o[0] == TRF_IMDB);
  n = trailerfonte_ordem(TRF_AUTO, 0, o);
  confere(".tpk fundo/destaque (mudo) continua Apple -> IMDb", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_IMDB);
  n = trailerfonte_ordem_cheia(TRF_AUTO, 0, 0, o);
  confere(".tpk tela cheia SEM som: Apple primeiro (nada ganha com o IMDb)", n == 2 && o[0] == TRF_APPLE);
  c = todas();
  confere(".tpk tela cheia com as duas: abre o IMDb (tem audio)",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_IMDB && !strcmp(u, c.imdb));
  confere(".tpk fundo com as duas: abre a Apple (melhor imagem, mudo)", escolhe(TRF_AUTO, 0, &c, &u) == TRF_APPLE);
  c = todas(); c.imdb = NULL;
  confere(".tpk tela cheia sem IMDb: cai na Apple (sem som, como hoje)",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_APPLE);
  c = todas(); c.imdb = NULL; c.imdbRespondeu = 0;
  confere(".tpk tela cheia: IMDb ainda sem resposta segura a Apple",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ESPERA);
  c = todas(); c.apple = NULL; c.appleRespondeu = 0;
  confere(".tpk tela cheia: IMDb pronto nao espera a Apple",
          trailerfonte_escolher_cheia(TRF_AUTO, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_IMDB);
  c = todas();
  confere(".tpk tela cheia com Apple fixa: Apple, respeitada",
          trailerfonte_escolher_cheia(TRF_APPLE, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_APPLE);
  n = trailerfonte_ordem_cheia(TRF_APPLE, 0, 1, o);
  confere(".tpk tela cheia com Apple fixa: so a Apple", n == 1 && o[0] == TRF_APPLE);
  c = todas(); c.apple = NULL;
  confere(".tpk tela cheia com IMDb fixo: IMDb",
          trailerfonte_escolher_cheia(TRF_IMDB, 0, 1, &c, &u, &n) == TRF_ABRE && n == TRF_IMDB);
  n = trailerfonte_ordem_cheia(TRF_YOUTUBE, 0, 1, o);
  confere(".tpk tela cheia com YouTube fixo: nada (nao toca nesta TV)", n == 0);
  // Mesmo com o desvio ligado, o .wgt nao muda: la a tela cheia e muda.
  n = trailerfonte_ordem_cheia(TRF_AUTO, 1, trailerfonte_com_som(1), o);
  confere("wgt com o desvio ligado continua Apple -> YouTube", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_YOUTUBE);
  // Onde houvesse som E YouTube (hipotetico): IMDb -> Apple -> YouTube.
  trailerfonte_definir_imdb_tizen(1);
  n = trailerfonte_ordem_cheia(TRF_AUTO, 1, 1, o);
  confere("com som, IMDb e YouTube: IMDb -> Apple -> YouTube",
          n == 3 && o[0] == TRF_IMDB && o[1] == TRF_APPLE && o[2] == TRF_YOUTUBE);
  trailerfonte_definir_imdb_tizen(0);
  trailerfonte_definir_imdb_primeiro_cheia(0);

  // --- DESTAQUE QUE CONTINUA COM SOM NA PAGINA (canario tpk-janela). So com
  // NV_TPK o desvio nasce ligado; .wgt e LG escolhem como sempre.
#ifdef NV_TPK
  confere("build NV_TPK: IMDb-primeiro do destaque ligado por padrao", trailerfonte_imdb_primeiro_destaque() == 1);
#else
  confere("sem NV_TPK: IMDb-primeiro do destaque desligado por padrao", trailerfonte_imdb_primeiro_destaque() == 0);
#endif
  trailerfonte_definir_imdb_primeiro_destaque(0);
  for (tz = 0; tz <= 1; tz++) {
    int aj, a[3], b[3], na, nb, k, igual;
    char nome[120];
    for (aj = TRF_AUTO; aj <= TRF_YOUTUBE; aj++) {
      na = trailerfonte_ordem(aj, tz, a);
      nb = trailerfonte_ordem_destaque(aj, tz, b);
      igual = na == nb;
      for (k = 0; igual && k < na; k++) igual = a[k] == b[k];
      snprintf(nome, sizeof nome, "%s ajuste %d desligado: destaque na ordem de sempre", tz ? "Samsung wgt" : "LG", aj);
      confere(nome, igual);
    }
  }
  trailerfonte_definir_imdb_primeiro_destaque(1);
  n = trailerfonte_ordem_destaque(TRF_AUTO, 0, o);
  confere(".tpk destaque automatico: IMDb -> Apple", n == 2 && o[0] == TRF_IMDB && o[1] == TRF_APPLE);
  n = trailerfonte_ordem(TRF_AUTO, 0, o);
  confere(".tpk pagina do titulo (fundo) continua Apple -> IMDb", n == 2 && o[0] == TRF_APPLE);
  c = todas();
  confere(".tpk destaque com as duas: IMDb (tem audio para continuar com som)",
          trailerfonte_escolher_destaque(TRF_AUTO, 0, &c, &u, &n) == TRF_ABRE && n == TRF_IMDB);
  c = todas(); c.imdb = NULL;
  confere(".tpk destaque sem IMDb: cai na Apple",
          trailerfonte_escolher_destaque(TRF_AUTO, 0, &c, &u, &n) == TRF_ABRE && n == TRF_APPLE);
  c = todas(); c.imdb = NULL; c.imdbRespondeu = 0;
  confere(".tpk destaque: IMDb sem resposta segura a Apple",
          trailerfonte_escolher_destaque(TRF_AUTO, 0, &c, &u, &n) == TRF_ESPERA);
  c = todas();
  confere(".tpk destaque com Apple fixa: Apple, respeitada",
          trailerfonte_escolher_destaque(TRF_APPLE, 0, &c, &u, &n) == TRF_ABRE && n == TRF_APPLE);
  n = trailerfonte_ordem_destaque(TRF_AUTO, 1, o);
  confere("wgt com o desvio ligado: sem IMDb la, Apple -> YouTube", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_YOUTUBE);
  trailerfonte_definir_imdb_primeiro_destaque(0);

  // Atalho do build: le o ajuste gravado.
  ajusteTeste = TRF_YOUTUBE;
  confere("trailerfonte_ajuste le o ajuste", trailerfonte_ajuste() == TRF_YOUTUBE);

  puts(falhas ? "trailer-fonte: FALHOU" : "trailer-fonte: tudo ok");
  return falhas ? 1 : 0;
}
