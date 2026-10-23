// Cache negativo de rede (src/negcache.c): 404/400 de API de metadados lembrado
// 24 h e em disco; host em recuo apos timeout; fora da lista nada e barrado.
#include "../src/negcache.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *disco;
static char *dados_ler(const char *n) { (void)n; return disco ? strdup(disco) : NULL; }
static int dados_gravar_leve(const char *n, const char *c) { (void)n; free(disco); disco = strdup(c); return 1; }

static int falhas;
static void confere(const char *nome, int ok) {
  printf("  %-62s %s\n", nome, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

int main(void) {
  const long t0 = 1000000;
  const char *intro = "https://api.theintrodb.org/v3/media?imdb_id=tt1&season=1&episode=2";
  const char *intro2 = "https://api.theintrodb.org/v3/media?imdb_id=tt1&season=1&episode=3";
  const char *intro4 = "https://api.theintrodb.org/v4/media?imdb_id=tt1&season=1&episode=2";
  const char *tmdb = "https://api.themoviedb.org/3/find/tt9?api_key=SEGREDO&language=pt";
  const char *tmdb2 = "https://api.themoviedb.org/3/find/tt9?api_key=OUTRA&language=pt";
  const char *addon = "https://addon.exemplo.org/meta/movie/tt1.json";
  char buf[4096];
  printf("-- negcache\n");
  negcache_disco(dados_ler, dados_gravar_leve);

  confere("endereco novo nao e barrado", !negcache_barra(intro, t0));
  negcache_nota(intro, 0, 404, t0);
  confere("404 da theintrodb barra o mesmo endereco", negcache_barra(intro, t0 + 10));
  confere("outro episodio nao e barrado", !negcache_barra(intro2, t0 + 10));
  confere("v3 miss does not block v4", !negcache_barra(intro4, t0 + 10));
  confere("ainda barrado perto de 24 h", negcache_barra(intro, t0 + 24 * 3600 - 5));
  confere("expira depois de 24 h", !negcache_barra(intro, t0 + 24 * 3600 + 5));

  negcache_nota(tmdb, 0, 404, t0);
  confere("tmdb 404 barra", negcache_barra(tmdb, t0 + 1));
  confere("api_key fora da chave (outra chave, mesmo pedido)", negcache_barra(tmdb2, t0 + 1));
  negcache_serializar(buf, sizeof buf);
  confere("nada de segredo ou endereco no que vai ao disco",
          !strstr(buf, "SEGREDO") && !strstr(buf, "themoviedb") && !strstr(buf, "tt9"));

  negcache_nota(addon, 0, 404, t0);
  confere("addon do usuario nunca e barrado", !negcache_barra(addon, t0 + 1));
  negcache_nota("https://api.tiffara.com/titles/tt1/parentsGuide", 0, 400, t0);
  confere("tiffara 400 barra", negcache_barra("https://api.tiffara.com/titles/tt1/parentsGuide", t0 + 1));
  negcache_nota("https://api.tiffara.com/titles/tt2/parentsGuide", 0, 500, t0);
  confere("500 nao e lembrado", !negcache_barra("https://api.tiffara.com/titles/tt2/parentsGuide", t0 + 1));
  negcache_nota("https://v3-cinemeta.strem.io/meta/series/tt3.json", 0, 200, t0);
  confere("200 nao e lembrado", !negcache_barra("https://v3-cinemeta.strem.io/meta/series/tt3.json", t0 + 1));

  // Recuo por timeout: 30 s, depois 60 s...
  {
    const char *u = "https://v3-cinemeta.strem.io/meta/series/tt7.json";
    const char *v = "https://v3-cinemeta.strem.io/catalog/movie/top.json";
    negcache_nota(u, 28, 0, t0);
    confere("1o timeout: host em recuo", negcache_barra(v, t0 + 5));
    confere("recuo acaba em 30 s", !negcache_barra(v, t0 + 31));
    negcache_nota(u, 28, 0, t0 + 40);
    confere("2o timeout seguido: recuo dobra (60 s)", negcache_barra(v, t0 + 40 + 50) && !negcache_barra(v, t0 + 40 + 61));
    negcache_nota(u, 0, 200, t0 + 200);
    negcache_nota(u, 28, 0, t0 + 300);
    confere("sucesso zera a contagem (volta a 30 s)", !negcache_barra(v, t0 + 300 + 31));
    {
      int i; long t = t0 + 1000;
      for (i = 0; i < 20; i++) { negcache_nota(u, 28, 0, t); t += 1000; }
      confere("teto de 10 min", negcache_barra(v, t - 1000 + 599) && !negcache_barra(v, t - 1000 + 601));
    }
  }

  // Disco: outro arranque.
  negcache_gravar();
  confere("gravou em disco", disco && strlen(disco) > 0);
  {
    char *copia = disco ? strdup(disco) : NULL;
    negcache_zerar();
    free(disco); disco = copia;
    confere("arranque novo carrega a recusa do disco", negcache_barra(tmdb, t0 + 100));
    confere("persisted v3 miss does not block v4", !negcache_barra(intro4, t0 + 100));
    negcache_nota(intro4, 0, 404, t0 + 100);
    confere("v4 miss blocks the same v4 URL", negcache_barra(intro4, t0 + 101));
    confere("entrada do disco ainda expira em 24 h", negcache_barra(intro, t0 + 100) && !negcache_barra(intro, t0 + 24 * 3600 + 100));
  }
  negcache_zerar();
  negcache_carregar_texto("zzzz lixo\n0000000000000001 1\n", t0);
  confere("linha corrompida/vencida e ignorada", negcache_n() == 0);

  printf(falhas ? "negcache: FALHOU\n" : "negcache: ok\n");
  return falhas != 0;
}
